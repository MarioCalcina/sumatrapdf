/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include <limits.h>
#include <unistd.h>

#include "base/File.h"

// we pad data read with 3 zeros for convenience. That way returned
// data is a valid null-terminated string or WCHAR*.
// 3 is for absolute worst case of WCHAR* where last char was partially written
constexpr int kZeroPaddingCount = 3;

// GUI-subsystem exes can lack CRT stdout even when launched with a pipe.
void WriteStdout(Str data) {
    if (len(data) == 0) {
        return;
    }
    fwrite(data.s, 1, (size_t)len(data), stdout);
}

void WriteStdoutLn(Str data) {
    WriteStdout(data);
    WriteStdout(StrL("\n"));
}

TempStr MakeUniqueFilePathTemp(Str path) {
    if (!file::Exists(path)) {
        return str::DupTemp(path);
    }
    TempStr noExt = path::GetPathNoExtTemp(path);
    TempStr ext = path::GetExtTemp(path);
    for (int i = 1; i < 10000; i++) {
        TempStr candidate = fmt("%s.%d%s", noExt, i, ext);
        if (!file::Exists(candidate)) {
            return candidate;
        }
    }
    return str::DupTemp(path);
}

bool FileTimeEq(const FILETIME& a, const FILETIME& b) {
    return a.dwLowDateTime == b.dwLowDateTime && a.dwHighDateTime == b.dwHighDateTime;
}

TempStr GetSelfExeDirTemp() {
    TempStr path = GetSelfExePathTemp();
    if (len(path) == 0) {
        return {};
    }
    return path::GetDirTemp(path);
}

TempStr GetPathInExeDirTemp(Str fileName) {
    TempStr dir = GetSelfExeDirTemp();
    if (len(dir) == 0) {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) {
            return fileName;
        }
        dir = str::DupTemp(Str(cwd));
    }
    TempStr path = path::JoinTemp(dir, fileName);
    path = path::NormalizeTemp(path);
    return path;
}

namespace path {

bool IsSep(char c) {
    return c == kPathSepChar;
}

template <typename S>
static S BaseNameT(S path) {
    int start = path.len;
    while (start > 0 && !IsSep(path.s[start - 1])) {
        start--;
    }
    return S(path.s + start, path.len - start);
}

template <typename S>
static S JoinTempT(S dir, S name, S name2, S sep) {
    if (name && IsSep(name.s[0])) {
        name = S(name.s + 1, name.len - 1);
    }
    if (len(dir) == 0 || IsSep(dir.s[dir.len - 1])) {
        sep = {};
    }
    S res = str::JoinTemp(dir, sep, name);
    if (name2) {
        res = JoinTempT(res, name2, S{}, sep);
    }
    return res;
}

// "." for a bare name, the root ("\", "C:\" or a UNC "\\server") when the
// name sits right under it, else everything before the separator
template <typename S>
static S DirTempT(S path) {
    S baseName = BaseNameT(path);
    int n = (int)(baseName.s - path.s);
    if (n == 0) {
        return S{};
    }
    if (n == 2 && IsSep(path.s[0]) && IsSep(path.s[1])) {
        return str::DupTemp(path);
    }
    if (n == 1 || (n == 3 && path.s[1] == ':')) {
        return str::DupTemp(S(path.s, n));
    }
    return str::DupTemp(S(path.s, n - 1));
}

TempStr GetBaseNameTemp(Str path) {
    return BaseNameT(path);
}

TempStr JoinTemp(Str dir, Str name, Str name2) {
    return JoinTempT(dir, name, name2, StrL(kPathSep));
}

Str Join(Arena* a, Str dir, Str name) {
    return str::Dup(a, JoinTemp(dir, name));
}

Str Join(Str dir, Str name) {
    return Join(nullptr, dir, name);
}

TempStr GetDirTemp(Str path) {
    TempStr res = DirTempT(path);
    return res.s ? res : str::DupTemp(StrL("."));
}

static int GetExtPos(Str path) {
    int ext = -1;
    for (int i = 0; i < path.len; i++) {
        char c = path.s[i];
        if (c == '.') {
            ext = i;
        } else if (IsSep(c)) {
            ext = -1;
        }
    }
    return ext;
}

TempStr GetExtTemp(Str path) {
    int ext = GetExtPos(path);
    if (ext < 0) {
        return StrL("");
    }
    return str::DupTemp(Str(path.s + ext, path.len - ext));
}

TempStr GetPathNoExtTemp(Str path) {
    int ext = GetExtPos(path);
    if (ext < 0) {
        return str::DupTemp(path);
    }
    return str::DupTemp(Str(path.s, ext));
}

TempStr ToOSTemp(Str path) {
    return str::ReplaceTemp(path, StrL("/"), StrL("\\"));
}

static Str AdvanceUntilWildcardMatch(Str fileName, Str filter);

static bool MatchWildcardsRec(Str fileName, Str filter) {
    if (len(filter) == 0) {
        return len(fileName) == 0;
    }
    switch (filter.s[0]) {
        case '\0':
        case ';':
            return len(fileName) == 0;
        case '*': {
            Str filterRest(filter.s + 1, filter.len - 1);
            fileName = AdvanceUntilWildcardMatch(fileName, filterRest);
            return len(fileName) > 0 || len(filterRest) == 0 || filterRest.s[0] == ';';
        }
        case '?':
            return len(fileName) > 0 &&
                   MatchWildcardsRec(Str(fileName.s + 1, fileName.len - 1), Str(filter.s + 1, filter.len - 1));
        default:
            return tolower(fileName.s[0]) == tolower(filter.s[0]) &&
                   MatchWildcardsRec(Str(fileName.s + 1, fileName.len - 1), Str(filter.s + 1, filter.len - 1));
    }
}

static Str AdvanceUntilWildcardMatch(Str fileName, Str filter) {
    while (len(fileName) > 0 && !MatchWildcardsRec(fileName, filter)) {
        fileName.s++;
        fileName.len--;
    }
    return fileName;
}

bool Match(Str path, Str filter) {
    Str baseName = GetBaseNameTemp(path);
    if (len(baseName) == 0) {
        return false;
    }
    while (str::ContainsChar(filter, ';')) {
        if (MatchWildcardsRec(baseName, filter)) {
            return true;
        }
        int semiIdx = str::IndexOfChar(filter, ';');
        filter = Str(filter.s + semiIdx + 1, filter.len - semiIdx - 1);
    }
    return MatchWildcardsRec(baseName, filter);
}

bool IsWslUnc(Str path) {
    return str::StartsWithI(path, StrL("\\\\wsl.localhost\\")) || str::StartsWithI(path, StrL("\\\\wsl$\\"));
}

bool IsWslMount(Str path) {
    if (len(path) == 0 || !str::StartsWithI(path, StrL("/mnt/"))) {
        return false;
    }
    if (path.len < 6) {
        return false;
    }
    char drive = (char)tolower((unsigned char)path.s[5]);
    return drive >= 'a' && drive <= 'z' && (IsSep(path.s[6]) || path.s[6] == '\0');
}

TempStr WslUncToUnixTemp(Str path) {
    if (len(path) == 0) {
        return {};
    }

    int off = 0;
    if (str::StartsWithI(path, StrL("\\\\wsl.localhost\\"))) {
        off = LenL("\\\\wsl.localhost\\");
    } else if (str::StartsWithI(path, StrL("\\\\wsl$\\"))) {
        off = LenL("\\\\wsl$\\");
    } else {
        return {};
    }

    for (; off < path.len && path.s[off] && !IsSep(path.s[off]); off++) {
        ;
    }
    if (off >= path.len || !IsSep(path.s[off]) || off + 1 >= path.len) {
        return {};
    }

    TempStr unixPath = str::JoinTemp(StrL("/"), Str(path.s + off + 1, path.len - off - 1));
    str::TransCharsInPlace(unixPath, StrL("\\"), StrL("/"));
    return unixPath;
}

TempStr WindowsToWslMountTemp(Str path) {
    if (len(path) == 0 || path.len < 3) {
        return {};
    }
    char drive = (char)tolower((unsigned char)path.s[0]);
    if (!(drive >= 'a' && drive <= 'z' && path.s[1] == ':' && IsSep(path.s[2]))) {
        return {};
    }

    TempStr rest = str::DupTemp(Str(path.s + 3, path.len - 3));
    str::TransCharsInPlace(rest, StrL("\\"), StrL("/"));
    return fmt("/mnt/%c/%s", drive, rest);
}

// path is dir, or something inside it. Only at a separator, so "C:\foo"
// does not contain "C:\foobar".
bool IsInDir(Str path, Str dir) {
    int n = len(dir);
    if (n == 0 || len(path) < n) {
        return false;
    }
    if (!str::StartsWithI(path, dir)) {
        return false;
    }
    if (len(path) == n) {
        return true;
    }
    if (IsSep(dir.s[n - 1])) {
        return true;
    }
    return IsSep(path.s[n]);
}

} // namespace path

namespace file {

thread_local CopyProgressCb gFileCopyProgressCb;

Str ReadFile(Str path) {
    return ReadFileWithArena(path, nullptr);
}

bool StartsWith(Str path, Str s) {
    u8* buf = AllocArrayTemp<u8>(s.len);
    if (!buf) {
        return false;
    }
    if (ReadN(path, buf, s.len) != s.len) {
        return false;
    }
    return MemEq(buf, s.s, s.len);
}

int ReadN(Str path, u8* buf, size_t toRead) {
    FILE* fp = OpenFILE(path);
    if (!fp) {
        return -1;
    }
    AutoCall closeFile(fclose, fp);
    memset(buf, 0, toRead);
    size_t nRead = fread((void*)buf, 1, toRead, fp);
    if (nRead == 0 && ferror(fp)) {
        return -1;
    }
    return (int)nRead;
}

Str ReadFileWithArena(Str filePath, Arena* a) {
    char* d = nullptr;
    int res;
    int size = 0;
    FILE* fp = OpenFILE(filePath);
    if (!fp) {
        return {};
    }
    AutoCall closeFile(fclose, fp);
    res = fseek(fp, 0, SEEK_END);
    if (res != 0) {
        return {};
    }
    long fileSize = ftell(fp);
    size_t nRead = 0;
    if (fileSize < 0 || fileSize > INT_MAX - kZeroPaddingCount) {
        goto Error;
    }
    size = (int)fileSize;
    d = AllocArray<char>(a, size + kZeroPaddingCount);
    if (!d) {
        goto Error;
    }
    res = fseek(fp, 0, SEEK_SET);
    if (res != 0) {
        goto Error;
    }

    nRead = fread((void*)d, 1, size, fp);
    if (nRead != (size_t)size) {
        int err = ferror(fp);
        int isEof = feof(fp);
        logf("ReadFileWithArena: fread() failed, path: '%s', size: %d, nRead: %d, err: %d, isEof: %d\n", filePath,
             (int)size, (int)nRead, err, isEof);
        ReportIf(!(isEof || (err != 0)));
        goto Error;
    }

    return Str(d, size);
Error:
    Free(a, (void*)d);
    return {};
}

} // namespace file

namespace dir {

// errOut (optional) gets the OS error when creation fails, so callers can log why.
// 0 there means the create itself reported success but the directory wasn't there
// afterwards.
bool CreateForFile(Str path, int* errOut) {
    TempStr dir = path::GetDirTemp(path);
    return CreateAll(dir, errOut);
}

} // namespace dir

