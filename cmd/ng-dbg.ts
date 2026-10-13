import { existsSync } from "node:fs";
import { join } from "node:path";

// Build the debug ASan executable and run it under lldb.
// Arguments after -- go to SumatraPDF. The launch saves and restores
// the session; pass -- -for-testing to skip that.

const usage = `Usage: bun cmd/ng-dbg.ts [-clean] [-- <SumatraPDF args>]

  -clean    delete the debug ASan output directory first`;

class CliError extends Error {}

type Options = {
  clean: boolean;
  appArgs: string[];
};

function parseArgs(args: string[]): Options | null {
  if (args.length === 1 && ["-h", "-help", "--help"].includes(args[0]!)) return null;

  const sep = args.indexOf("--");
  const ours = sep < 0 ? args : args.slice(0, sep);
  const appArgs = sep < 0 ? [] : args.slice(sep + 1);
  let clean = false;
  for (const arg of ours) {
    if (arg !== "-clean") throw new CliError(`unknown option: ${arg}`);
    if (clean) throw new CliError("-clean can only be specified once");
    clean = true;
  }
  return { clean, appArgs };
}

// ASan prints the report and then calls _exit, so the debugger never stops.
// abort_on_error turns that into SIGABRT. The nano allocator holds the address
// range ASan needs (malloc: nano zone abandoned). Leave values the user
// already set.
function asanAbortEnv(): NodeJS.ProcessEnv {
  const env = { ...process.env };
  const cur = env.ASAN_OPTIONS ?? "";
  if (!/(^|:)abort_on_error(=|:|$)/.test(cur)) {
    env.ASAN_OPTIONS = cur ? `${cur}:abort_on_error=1` : "abort_on_error=1";
  }
  if (env.MallocNanoZone === undefined) env.MallocNanoZone = "0";
  return env;
}

async function run(command: string[], description: string, env?: NodeJS.ProcessEnv): Promise<void> {
  console.log(`> ${command.join(" ")}`);
  const proc = Bun.spawn(command, { stdin: "inherit", stdout: "inherit", stderr: "inherit", env });
  const code = await proc.exited;
  if (code !== 0) throw new Error(`${description} failed with exit code ${code}`);
}

async function main(): Promise<void> {
  let opts: Options | null;
  try {
    opts = parseArgs(process.argv.slice(2));
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    console.error(`error: ${message}\n`);
    console.error(usage);
    process.exitCode = 1;
    return;
  }
  if (!opts) {
    console.log(usage);
    return;
  }
  const lldb = Bun.which("lldb");
  if (!lldb) throw new Error("lldb not found (xcode-select --install)");

  const buildArgs = ["bun", join(import.meta.dir, "ng-build.ts"), "-dbg", "-asan"];
  if (opts.clean) buildArgs.push("-clean");
  await run(buildArgs, "build");

  const exe = join(import.meta.dir, "..", "out", "mac", "dbg-asan", "SumatraPDF");
  if (!existsSync(exe)) throw new Error(`debug ASan executable not found: ${exe}`);
  // A normal launch: it saves settings and restores the session.
  // A throwaway run passes -- -for-testing itself.
  // -o run starts the program; -- keeps the app's arguments from being read
  // as lldb's
  await run([lldb, "-o", "run", "--", exe, ...opts.appArgs], "lldb", asanAbortEnv());
}

try {
  await main();
} catch (error) {
  const message = error instanceof Error ? error.message : String(error);
  console.error(`\nDebug failed: ${message}`);
  process.exitCode = 1;
}
