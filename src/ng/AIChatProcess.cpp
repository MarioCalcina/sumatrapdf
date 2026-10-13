/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/CmdLineArgs.h"
#include "base/File.h"

#include "AIChatCommon.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

TempStr AIChatFindExecutableTemp(const StrVec& fullPathCandidates, Str searchExeName, Str searchNameNoExt) {
    for (int i = 0; i < len(fullPathCandidates); i++) {
        if (file::Exists(fullPathCandidates[i])) {
            return str::DupTemp(fullPathCandidates[i]);
        }
    }
    Str name = len(searchNameNoExt) > 0 ? searchNameNoExt : searchExeName;
    const char* env = getenv("PATH");
    if (len(name) > 0 && env) {
        StrVec dirs;
        Split(&dirs, Str(env), StrL(":"), true);
        for (Str dir : dirs) {
            TempStr candidate = path::JoinTemp(dir, name);
            if (access(CStrTemp(candidate), X_OK) == 0 && !dir::Exists(candidate)) {
                return candidate;
            }
        }
    }
    return {};
}

struct AIChatPosixProcess {
    pid_t pid = -1;
    bool exited = false;
    bool terminated = false;
};

// Parses the quoting produced by QuoteCmdLineArgTemp without involving a
// shell, so prompts cannot trigger shell expansion.
static bool ParsePosixProcessArgs(Str cmdLine, StrVec& args) {
    int i = 0;
    while (i < len(cmdLine)) {
        while (i < len(cmdLine) && str::IsWs(cmdLine.s[i])) {
            i++;
        }
        if (i == len(cmdLine)) {
            break;
        }

        bool quoted = false;
        str::Builder arg;
        while (i < len(cmdLine)) {
            char c = cmdLine.s[i];
            if (!quoted && str::IsWs(c)) {
                break;
            }
            if (c != '\\') {
                if (c == '"') {
                    quoted = !quoted;
                } else {
                    arg.AppendChar(c);
                }
                i++;
                continue;
            }

            int n = 0;
            while (i < len(cmdLine) && cmdLine.s[i] == '\\') {
                n++;
                i++;
            }
            if (i < len(cmdLine) && cmdLine.s[i] == '"') {
                for (int j = 0; j < n / 2; j++) {
                    arg.AppendChar('\\');
                }
                if (n % 2) {
                    arg.AppendChar('"');
                } else {
                    quoted = !quoted;
                }
                i++;
            } else {
                for (int j = 0; j < n; j++) {
                    arg.AppendChar('\\');
                }
            }
        }
        if (quoted) {
            return false;
        }
        args.Append(ToStr(arg));
    }
    return len(args) > 0;
}

static bool ReapPosixProcess(AIChatPosixProcess* process, int options) {
    if (!process || process->exited) {
        return true;
    }
    int status = 0;
    pid_t waited;
    do {
        waited = waitpid(process->pid, &status, options);
    } while (waited < 0 && errno == EINTR);
    if (waited == process->pid || (waited < 0 && errno == ECHILD)) {
        process->exited = true;
    }
    return process->exited;
}

void AIChatCloseProcess(void** processHandle, bool terminateIfRunning) {
    if (!processHandle || !*processHandle) {
        return;
    }
    auto* process = (AIChatPosixProcess*)*processHandle;
    *processHandle = nullptr;
    if (!ReapPosixProcess(process, WNOHANG) && terminateIfRunning) {
        kill(process->pid, SIGKILL);
        process->terminated = true;
    }
    if (process->terminated) {
        ReapPosixProcess(process, 0);
    }
    delete process;
}

bool AIChatLaunchProcessWithStdoutPipe(Str cmdLine, Str cwd, AIChatProcessLaunchResult* out) {
    if (!out || len(cmdLine) == 0) {
        return false;
    }
    *out = {};

    StrVec args;
    if (!ParsePosixProcessArgs(cmdLine, args)) {
        return false;
    }
    Vec<char*> argv;
    for (Str arg : args) {
        VecAppend(argv, arg.s);
    }

    int fds[2];
    if (pipe(fds) != 0) {
        return false;
    }
    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return false;
    }
    if (pid == 0) {
        close(fds[0]);
        if (dup2(fds[1], STDOUT_FILENO) < 0 || dup2(fds[1], STDERR_FILENO) < 0) {
            _exit(126);
        }
        close(fds[1]);
        if (len(cwd) > 0 && chdir(CStrTemp(cwd)) != 0) {
            _exit(126);
        }
        execvp(argv[0], argv.els);
        _exit(127);
    }

    close(fds[1]);
    auto* process = new AIChatPosixProcess{pid};
    out->ok = true;
    out->hProcess = process;
    out->hReadPipe = new int(fds[0]);
    out->processId = (u32)pid;
    return true;
}

int AIChatReadPipeChunk(void* hReadPipe, char* buf, int size) {
    if (!hReadPipe) {
        return -1;
    }
    int fd = *(int*)hReadPipe;
    for (;;) {
        ssize_t n = read(fd, buf, (size_t)size);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        return (int)n;
    }
}

void AIChatCloseReadPipe(void* hReadPipe) {
    if (!hReadPipe) {
        return;
    }
    int fd = *(int*)hReadPipe;
    delete (int*)hReadPipe;
    close(fd);
}

void AIChatReadPipeToEnd(void* hReadPipe, str::Builder& out) {
    char buf[4096];
    int n;
    while ((n = AIChatReadPipeChunk(hReadPipe, buf, sizeof(buf))) > 0) {
        out.Append(Str(buf, n));
    }
    AIChatCloseReadPipe(hReadPipe);
}

bool AIChatWaitForProcess(void* hProcess, int timeoutMs) {
    auto* process = (AIChatPosixProcess*)hProcess;
    if (!process || process->exited) {
        return true;
    }
    u64 started = GetTickCount64();
    for (;;) {
        if (ReapPosixProcess(process, WNOHANG)) {
            return true;
        }
        if (timeoutMs >= 0 && GetTickCount64() - started >= (u64)timeoutMs) {
            return false;
        }
        SleepInMs(5);
    }
}

void AIChatTerminateProcess(void* hProcess) {
    auto* process = (AIChatPosixProcess*)hProcess;
    if (!process || process->exited) {
        return;
    }
    if (kill(process->pid, SIGKILL) == 0 || errno == ESRCH) {
        process->terminated = true;
    }
}

constexpr int kAIChatMaxCaptureBytes = 1024 * 1024;

// ng: orig's loop with poll() in place of PeekNamedPipe
bool AIChatRunCapture(Str cmdLine, int timeoutMs, str::Builder& out) {
    AIChatProcessLaunchResult launch;
    if (!AIChatLaunchProcessWithStdoutPipe(cmdLine, {}, &launch)) {
        return false;
    }

    int fd = *(int*)launch.hReadPipe;
    u64 deadline = GetTickCount64() + (u64)timeoutMs;
    while (GetTickCount64() < deadline && out.len < kAIChatMaxCaptureBytes) {
        struct pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLIN;
        int res = poll(&pfd, 1, 10);
        if (res < 0 && errno != EINTR) {
            break;
        }
        if (res <= 0) {
            continue;
        }
        char buf[4096];
        int n = AIChatReadPipeChunk(launch.hReadPipe, buf, sizeof(buf));
        if (n <= 0) {
            // end of file: the process closed its side
            break;
        }
        out.Append(Str(buf, n));
    }
    AIChatCloseReadPipe(launch.hReadPipe);
    AIChatCloseProcess(&launch.hProcess, true);
    return true;
}

TempStr AIChatFormatSessionIdTemp(const u8* bytes) {
    u8 b[16];
    memcpy(b, bytes, sizeof(b));
    // RFC 4122: version 4, variant 1
    b[6] = (u8)((b[6] & 0x0f) | 0x40);
    b[8] = (u8)((b[8] & 0x3f) | 0x80);
    return fmt("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", b[0], b[1], b[2], b[3], b[4],
               b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
}
