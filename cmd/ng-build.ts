// The single build entry point. Compiles and links targets from
// cmd/helper/ng-targets.ts by invoking clang directly (see
// cmd/helper/ng-compile.ts); no generated project files.
//
//   bun cmd/ng-build.ts -dbg                  # SumatraPDF and its libraries
//   bun cmd/ng-build.ts -rel -run -- file.pdf # release, then run with args
//   bun cmd/ng-build.ts -dbg test_util -run   # one target
//   bun cmd/ng-build.ts -rel -universal       # arm64 + x86_64 in one app
//   bun cmd/ng-build.ts -dbg -arch x86_64     # the other architecture
//   bun cmd/ng-build.ts -dbg -clean -v        # wipe output dir first, echo commands
//   bun cmd/ng-build.ts -all -rel             # every target

import { mkdirSync } from "node:fs";
import { cpus } from "node:os";
import { basename, join, relative } from "node:path";
import { findToolchain, hostArch, macArchs, type Arch, type Toolchain } from "./helper/ng-toolchain";
import {
  buildTarget,
  cleanOutDir,
  depsOf,
  outDir,
  outDirName,
  root,
  stageShared,
  universalDir,
  writeDsym,
  type BuildFlags,
} from "./helper/ng-compile";
import { defaultTarget, findTarget, targets as allTargets, type Target } from "./helper/ng-targets";
import { makeMacBundle } from "./helper/ng-mac-bundle";
import { maxRev, ngVersion } from "./helper/ng-version";
import { genDocsForBuild } from "./gen-docs";
import { packEmbedded } from "./helper/embedded";

const usage = `Usage: bun cmd/ng-build.ts <-dbg|-rel> [options] [target...] [-- <run args>]

Configuration (one required):
  -dbg              debug build (out/mac/dbg)
  -rel              release build (out/mac/rel)

Options:
  -arch <a>         arm64 or x86_64 (default: this Mac's, ${hostArch});
                    the other one builds into out/mac/<cfg>-<a>
  -universal        build arm64 and x86_64 and join them with lipo into
                    out/mac/<cfg>-universal
  -asan             address sanitizer
  -clean            delete the output directory first
  -rev <n>          n-th further build of the day: version yy.mm.dd.n (1-${maxRev})
  -no-embed-fonts   leave built-in fonts out of the archive; they download on use
  -all              build every target
  -run              run the (single) built target afterwards; args after --
  -v                echo every compiler and linker command
  -h | -help        this text

Targets (default: ${defaultTarget}):`;

class CliError extends Error {}

type Options = {
  flags: BuildFlags;
  universal: boolean;
  targets: string[];
  all: boolean;
  run: boolean;
  noEmbedFonts: boolean;
  runArgs: string[];
};

function parseArch(a: string | undefined): Arch {
  if (a === "arm64" || a === "x86_64") return a;
  throw new CliError(`-arch needs arm64 or x86_64`);
}

function parseArgs(args: string[]): Options {
  const flags: BuildFlags = { debug: false, asan: false, clean: false, verbose: false, arch: hostArch };
  let config: "dbg" | "rel" | null = null;
  let universal = false;
  let archGiven = false;
  const targets: string[] = [];
  let all = false;
  let run = false;
  let noEmbedFonts = false;
  let runArgs: string[] = [];
  for (let i = 0; i < args.length; i++) {
    const a = args[i]!;
    if (a === "--") {
      runArgs = args.slice(i + 1);
      break;
    }
    if (a === "-dbg" || a === "-rel") {
      if (config && config !== a.slice(1)) throw new CliError("-dbg and -rel cannot be used together");
      config = a.slice(1) as "dbg" | "rel";
    } else if (a === "-arch") {
      flags.arch = parseArch(args[++i]);
      archGiven = true;
    } else if (a === "-universal") universal = true;
    else if (a === "-asan") flags.asan = true;
    else if (a === "-clean") flags.clean = true;
    else if (a === "-rev") {
      flags.rev = Number(args[++i]);
      if (!Number.isInteger(flags.rev) || flags.rev < 1 || flags.rev > maxRev) {
        throw new CliError(`-rev needs a number from 1 to ${maxRev}`);
      }
    } else if (a === "-no-embed-fonts") noEmbedFonts = true;
    else if (a === "-all") all = true;
    else if (a === "-run") run = true;
    else if (a === "-v") flags.verbose = true;
    else if (a === "-h" || a === "-help" || a === "--help") throw new CliError("");
    else if (a.startsWith("-")) throw new CliError(`unknown option: ${a}`);
    else targets.push(a);
  }
  if (!config) throw new CliError("missing -dbg or -rel");
  if (universal && archGiven) throw new CliError("-universal and -arch cannot be used together");
  flags.debug = config === "dbg";
  if (run && (all || targets.length > 1)) throw new CliError("-run needs exactly one target");
  return { flags, universal, targets, all, run, noEmbedFonts, runArgs };
}

function printUsage(): void {
  console.log(usage);
  for (const t of allTargets) {
    console.log(`  ${t.name.padEnd(16)} ${t.kind}`);
  }
}

function fail(msg: string): never {
  console.error(msg);
  process.exit(1);
}

async function runExe(exe: string, args: string[]): Promise<number> {
  // A normal launch saves and restores the session. Throwaway runs pass
  // -- -for-testing themselves (ng-dbg and ng-dbg-control already do).
  console.log(`running ${relative(root, exe)} ${args.join(" ")}`);
  const p = Bun.spawn([exe, ...args], { cwd: root, stdout: "inherit", stderr: "inherit" });
  return p.exited;
}

// an app's bundled targets (its Quick Look extensions) are built just before it
function withBundled(targets: Target[]): Target[] {
  const out: Target[] = [];
  for (const t of targets) {
    for (const name of t.bundled ?? []) {
      const b = findTarget(name) ?? fail(`target ${t.name}: unknown bundled target ${name}`);
      if (!out.includes(b)) out.push(b);
    }
    if (!out.includes(t)) out.push(t);
  }
  return out;
}

function usesEmbedded(targets: Target[]): boolean {
  return targets.some((t) => t.embedded || depsOf(t, fail).some((d) => d.embedded));
}

// The LzSA archive the app links: packed by MakeLZSA, built for this Mac.
async function packArchive(tc: Toolchain, f: BuildFlags, noEmbedFonts: boolean): Promise<void> {
  if (noEmbedFonts) console.log("not embedding built-in fonts");
  await genDocsForBuild();
  const packFlags: BuildFlags = { debug: false, asan: false, clean: false, verbose: f.verbose, arch: hostArch };
  stageShared(outDir(packFlags));
  const packer = await buildTarget(tc, findTarget("MakeLZSA")!, packFlags, fail);
  await packEmbedded(packer, outDir(f), root, { manual: true, compress: true, fonts: !noEmbedFonts });
}

async function buildAll(tc: Toolchain, targets: Target[], f: BuildFlags, noEmbedFonts: boolean): Promise<string[]> {
  console.log(`${tc.label} ${f.arch} -> out/${outDirName(f)} (${cpus().length} jobs), ver ${ngVersion(f.rev).ver}`);
  if (f.clean) cleanOutDir(f);
  stageShared(outDir(f));
  if (usesEmbedded(targets)) await packArchive(tc, f, noEmbedFonts);
  const built: string[] = [];
  for (const t of targets) built.push(await buildTarget(tc, t, f, fail));
  return built;
}

function lipo(inputs: string[], out: string): void {
  const r = Bun.spawnSync(["lipo", "-create", ...inputs, "-output", out], { stdout: "pipe", stderr: "pipe" });
  if (r.exitCode !== 0) fail(`lipo failed:\n${r.stderr.toString()}`);
}

// Builds every architecture, then joins each executable into one.
async function buildUniversal(tc: Toolchain, targets: Target[], f: BuildFlags, noEmbedFonts: boolean) {
  const perArch: string[][] = [];
  for (const arch of macArchs) perArch.push(await buildAll(tc, targets, { ...f, arch }, noEmbedFonts));
  const dir = universalDir(f);
  mkdirSync(dir, { recursive: true });
  const built: string[] = [];
  for (let i = 0; i < targets.length; i++) {
    if (targets[i]!.kind === "staticlib") continue;
    const out = join(dir, basename(perArch[0]![i]!));
    lipo(
      perArch.map((exes) => exes[i]!),
      out,
    );
    await writeDsym(out, fail);
    console.log(`universal ${relative(root, out)}`);
    built.push(out);
  }
  return built;
}

async function main(): Promise<void> {
  if (process.argv.length === 2) {
    printUsage();
    return;
  }
  let opts: Options;
  try {
    opts = parseArgs(process.argv.slice(2));
  } catch (e) {
    if (!(e instanceof CliError)) throw e;
    if (e.message) console.error(`${e.message}\n`);
    printUsage();
    process.exit(e.message ? 1 : 0);
  }
  const { flags } = opts;
  const names = opts.all ? allTargets.map((t) => t.name) : opts.targets.length > 0 ? opts.targets : [defaultTarget];
  const targets = withBundled(
    names.map((n) => findTarget(n) ?? fail(`unknown target: ${n} (bun cmd/ng-build.ts for the list)`)),
  );
  const tc = findToolchain(fail);

  const started = performance.now();
  const built = opts.universal
    ? await buildUniversal(tc, targets, flags, opts.noEmbedFonts)
    : await buildAll(tc, targets, flags, opts.noEmbedFonts);
  const app = built.find((p) => basename(p) === defaultTarget);
  if (app) console.log(`bundle ${relative(root, makeMacBundle(root, app, ngVersion(flags.rev).ver))}`);
  console.log(`done in ${((performance.now() - started) / 1000).toFixed(1)} s`);

  const last = built[built.length - 1];
  if (opts.run && last && targets[0]!.kind !== "staticlib") {
    process.exit(await runExe(last, opts.runArgs));
  }
}

await main();
