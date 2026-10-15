// Compiles, archives and links targets from ng-targets.ts with Apple's clang.
// No generator in between: every object is one compiler spawn, run N at a
// time, skipped when the object is newer than its source and every header the
// last compile recorded (-MMD .d files).
//
// Output: out/mac/<cfg>/<target> (apps, tools), lib/ (static libs) and
// obj/<target>/<source path>.o beneath the same directory. A build is for one
// architecture; a universal app is two builds joined by lipo (ng-build.ts).

import { copyFileSync, existsSync, mkdirSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { cpus } from "node:os";
import { dirname, isAbsolute, join, relative } from "node:path";
import { Glob } from "bun";
import { hostArch, macMinVersion, type Arch, type Toolchain } from "./ng-toolchain";
import { coreHookSources, findTarget, forArch, type Target } from "./ng-targets";
import { embedLzsa } from "./embedded";
import { ngVersion, versionDefines } from "./ng-version";

export type BuildFlags = {
  debug: boolean;
  asan: boolean;
  clean: boolean;
  verbose: boolean;
  arch: Arch;
  /** n-th further build of the day: the .n of version yy.mm.dd.n */
  rev?: number;
};

export type Fail = (msg: string) => never;

export const root = join(import.meta.dir, "..", "..");

// ─── output layout ────────────────────────────────────────────────────────

function cfgName(f: BuildFlags): string {
  return (f.debug ? "dbg" : "rel") + (f.asan ? "-asan" : "");
}

// the host's architecture builds into out/mac/<cfg>, the other into
// out/mac/<cfg>-<arch>
export function outDirName(f: BuildFlags): string {
  const suffix = f.arch === hostArch ? "" : `-${f.arch}`;
  return `mac/${cfgName(f)}${suffix}`;
}

export function outDir(f: BuildFlags): string {
  return join(root, "out", outDirName(f));
}

// where a universal build joins the per-architecture apps
export function universalDir(f: BuildFlags): string {
  return join(root, "out", "mac", `${cfgName(f)}-universal`);
}

function objPath(dir: string, target: Target, src: string): string {
  return join(dir, "obj", target.name, `${src}.o`);
}

// ─── sources ──────────────────────────────────────────────────────────────

export function resolveSources(t: Target, fail: Fail): string[] {
  const excluded = (t.exclude ?? []).map((p) => new Glob(p));
  const out: string[] = [];
  for (const pattern of t.sources) {
    const g = new Glob(pattern);
    const matches = Array.from(g.scanSync({ cwd: root, dot: false }));
    if (matches.length === 0) fail(`target ${t.name}: no files match ${pattern}`);
    for (const m of matches.sort()) {
      if (excluded.some((e) => e.match(m))) continue;
      if (!out.includes(m)) out.push(m);
    }
  }
  return out;
}

// Writes the target's config headers into out/<cfg>/generated/<target>/ and
// returns the target with that directory first on the include path. Rewrites
// only on a change, so an unchanged header does not invalidate the objects.
function withGenerated(t: Target, arch: Arch, dir: string): Target {
  const wanted = (t.generated ?? []).filter((g) => !g.arch || g.arch.includes(arch));
  if (wanted.length === 0) return t;
  const genDir = join(dir, "generated", t.name);
  mkdirSync(genDir, { recursive: true });
  for (const g of wanted) {
    const dst = join(genDir, g.name);
    if (g.content === undefined) {
      const src = join(root, g.from!);
      if (mtime(src) > mtime(dst)) copyFileSync(src, dst);
      continue;
    }
    if (!existsSync(dst) || readFileSync(dst, "utf8") !== g.content) writeFileSync(dst, g.content);
  }
  return { ...t, includes: [relative(root, genDir), ...(t.includes ?? [])] };
}

function isCSource(p: string): boolean {
  return /\.c$/.test(p);
}

function isObjC(p: string): boolean {
  return /\.(m|mm)$/.test(p);
}

// ─── flags ────────────────────────────────────────────────────────────────

// gcc 14+ and clang 16+ make these legacy-C diagnostics errors, which -w does
// not undo. Third-party C (zlib's gzlib) still writes that way and we do not
// edit ext/.
const legacyCWarnings = [
  "-Wno-implicit-function-declaration",
  "-Wno-implicit-int",
  "-Wno-incompatible-pointer-types",
  "-Wno-int-conversion",
];

// warnings off in our code: ported code from orig was only ever held to its
// original compiler's rules
const ourWarningsOff = [
  // a `//` comment whose last character is a backslash
  "-Wno-comment",
  "-Wno-implicit-fallthrough",
  "-Wno-invalid-offsetof",
  "-Wno-parentheses",
  "-Wno-reorder",
  "-Wno-sign-compare",
  "-Wno-switch",
  "-Wno-unknown-pragmas",
  "-Wno-unused-but-set-variable",
  "-Wno-unused-const-variable",
  "-Wno-unused-function",
  "-Wno-unused-variable",
  // `if (!this)` in Arena
  "-Wno-undefined-bool-conversion",
];

/** what every compile and link of `arch` passes */
export function archFlags(arch: Arch): string[] {
  return ["-arch", arch, `-mmacosx-version-min=${macMinVersion}`];
}

function definesOf(t: Target, f: BuildFlags): string[] {
  const extra = (f.debug ? t.debugDefines : t.releaseDefines) ?? [];
  const version = t.versionDefines ? versionDefines(ngVersion(f.rev)) : [];
  return [...(t.defines ?? []), ...extra, ...version];
}

function cflags(t: Target, f: BuildFlags, cpp: boolean): string[] {
  const flags = [...archFlags(f.arch), "-DIS_TRACY=0", "-DSUMATRA_NG"];
  if (cpp) {
    flags.push("-std=c++20", "-fno-rtti");
    if (!t.exceptions) flags.push("-fno-exceptions");
  } else {
    flags.push("-std=gnu11");
  }
  if (t.strict) {
    flags.push("-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter", "-Wno-missing-field-initializers");
    flags.push(...ourWarningsOff);
  } else {
    flags.push("-w");
    if (!cpp) flags.push(...legacyCWarnings);
  }
  // release builds keep debug info too: it goes into the .dSYM
  if (f.debug) {
    flags.push(t.alwaysOptimize ? "-O1" : "-O0", t.alwaysOptimize ? "-DNDEBUG" : "-DDEBUG");
  } else {
    flags.push("-O2", "-DNDEBUG");
  }
  flags.push("-g", "-Wno-deprecated-declarations");
  if (f.asan) flags.push("-fsanitize=address", "-fno-omit-frame-pointer");
  for (const d of definesOf(t, f)) flags.push(`-D${d}`);
  for (const i of t.includes ?? []) flags.push("-I", join(root, i));
  return flags;
}

function perSourceFlags(t: Target, src: string, arch: Arch): string[] {
  const out: string[] = [];
  for (const ps of t.perSource ?? []) {
    if (ps.arch && !ps.arch.includes(arch)) continue;
    if (!new Glob(ps.glob).match(src)) continue;
    out.push(...ps.flags);
  }
  return out;
}

// the per-source overrides go into the key too: they are not in the common
// flags, so a change to one would otherwise leave every object up to date
function flagsKey(t: Target, f: BuildFlags): string[] {
  const perSrc = (t.perSource ?? [])
    .filter((ps) => !ps.arch || ps.arch.includes(f.arch))
    .map((ps) => `${ps.glob}:${ps.flags.join(" ")}`);
  return [...cflags(t, f, true), ...perSrc];
}

// ─── dependency tracking ──────────────────────────────────────────────────

function mtime(p: string): number {
  try {
    return statSync(p).mtimeMs;
  } catch {
    return -1;
  }
}

// Headers the last compile of `obj` read, from the compiler's own record:
// make syntax, "obj: src \\\n hdr hdr ..." with backslash continuations.
function recordedDeps(obj: string): string[] | null {
  const depFile = `${obj}.d`;
  if (!existsSync(depFile)) return null;
  const text = readFileSync(depFile, "utf8");
  const colon = text.indexOf(":");
  if (colon < 0) return null;
  return text
    .slice(colon + 1)
    .replace(/\\\r?\n/g, " ")
    .split(/\s+/)
    .filter(Boolean)
    .map((p) => p.replace(/\\ /g, " "));
}

function needsCompile(src: string, obj: string): boolean {
  const objTime = mtime(obj);
  if (objTime < 0) return true;
  if (mtime(join(root, src)) > objTime) return true;
  const deps = recordedDeps(obj);
  if (!deps) return true;
  for (const d of deps) {
    // clang echoes the source path as we passed it: relative to the root
    const t = mtime(isAbsolute(d) ? d : join(root, d));
    if (t < 0 || t > objTime) return true;
  }
  return false;
}

// ─── spawning ─────────────────────────────────────────────────────────────

export function formatCmd(cmd: string[]): string {
  return cmd.map((a) => (/[\s"]/.test(a) ? `"${a.replaceAll('"', '\\"')}"` : a)).join(" ");
}

type SpawnResult = { code: number; out: string };

async function spawn(cmd: string[]): Promise<SpawnResult> {
  const p = Bun.spawn(cmd, { cwd: root, stdout: "pipe", stderr: "pipe" });
  const [out, err, code] = await Promise.all([new Response(p.stdout).text(), new Response(p.stderr).text(), p.exited]);
  return { code, out: out + err };
}

// ─── compile one target ───────────────────────────────────────────────────

type CompileJob = { src: string; obj: string; cmd: string[] };

function compileCmd(tc: Toolchain, t: Target, f: BuildFlags, src: string, obj: string): string[] {
  const cpp = !isCSource(src);
  const cmd = [cpp ? tc.cxx : tc.cc, ...cflags(t, f, cpp), ...perSourceFlags(t, src, f.arch)];
  if (isObjC(src) || /ext\/gpui\/gpui\.cpp$/.test(src)) {
    cmd.push("-x", "objective-c++", "-fobjc-arc");
  }
  cmd.push("-MMD", "-MF", `${obj}.d`, "-c", src, "-o", obj);
  return cmd;
}

async function runPool<T>(jobs: T[], n: number, fn: (job: T) => Promise<boolean>): Promise<boolean> {
  let next = 0;
  let ok = true;
  const worker = async () => {
    while (ok && next < jobs.length) {
      const job = jobs[next++]!;
      if (!(await fn(job))) ok = false;
    }
  };
  await Promise.all(Array.from({ length: Math.min(n, jobs.length) }, worker));
  return ok;
}

async function compileTarget(tc: Toolchain, t0: Target, f: BuildFlags, dir: string, fail: Fail): Promise<string[]> {
  const t = withGenerated(forArch(t0, f.arch), f.arch, dir);
  const sources = resolveSources(t, fail);
  const objs: string[] = [];
  const jobs: CompileJob[] = [];

  // a flags change invalidates every object of the target
  const stamp = join(dir, "obj", t.name, "flags.txt");
  const key = [tc.cxx, ...flagsKey(t, f)].join(" ");
  const flagsChanged = !existsSync(stamp) || readFileSync(stamp, "utf8") !== key;

  for (const src of sources) {
    const obj = objPath(dir, t, src);
    objs.push(obj);
    if (!flagsChanged && !needsCompile(src, obj)) continue;
    mkdirSync(dirname(obj), { recursive: true });
    const cmd = compileCmd(tc, t, f, src, obj);
    jobs.push({ src, obj, cmd });
  }
  if (jobs.length === 0) {
    console.log(`  ${t.name}: up to date`);
    return objs;
  }
  console.log(`  ${t.name}: compiling ${jobs.length} of ${sources.length} files`);
  const started = performance.now();
  const ok = await runPool(jobs, cpus().length, async (job) => {
    if (f.verbose) console.log(`> ${formatCmd(job.cmd)}`);
    const r = await spawn(job.cmd);
    const text = r.out.trim();
    if (r.code !== 0) {
      rmSync(job.obj, { force: true });
      console.error(`\nerror compiling ${job.src}:\n${text}\n> ${formatCmd(job.cmd)}`);
      return false;
    }
    if (text) console.log(text);
    return true;
  });
  if (!ok) fail(`build of ${t.name} failed`);
  mkdirSync(dirname(stamp), { recursive: true });
  writeFileSync(stamp, key);
  console.log(`  ${t.name}: compiled in ${((performance.now() - started) / 1000).toFixed(1)} s`);
  return objs;
}

// ─── archive and link ─────────────────────────────────────────────────────

async function archive(tc: Toolchain, t: Target, dir: string, objs: string[], fail: Fail): Promise<string> {
  const libDir = join(dir, "lib");
  mkdirSync(libDir, { recursive: true });
  const out = join(libDir, `lib${t.name}.a`);
  // the member list, so dropping a source rearchives instead of leaving its
  // object in the lib (mtimes alone never notice a removal)
  const listPath = join(libDir, `${t.name}.objs`);
  const list = objs.join("\n");
  const listSame = existsSync(listPath) && readFileSync(listPath, "utf8") === list;
  if (listSame && existsSync(out) && objs.every((o) => mtime(o) <= mtime(out))) return out;
  writeFileSync(listPath, list);
  rmSync(out, { force: true });
  // Apple's ar has no @response-file support; its libtool takes a list file
  // (one path per line, unquoted) instead
  const fileList = join(libDir, `${t.name}.rsp`);
  writeFileSync(fileList, objs.join("\n") + "\n");
  const cmd = [tc.ar, "-static", "-no_warning_for_no_symbols", "-o", out, "-filelist", fileList];
  const r = await spawn(cmd);
  if (r.code !== 0) {
    console.error(r.out);
    fail(`archiving ${t.name} failed: ${formatCmd(cmd)}`);
  }
  return out;
}

// ld64 stores a debug map that points at the .o files and does not copy DWARF
// into the executable. The clang driver only runs dsymutil when it compiled
// and linked in one invocation, so a .dSYM has to be written here.
export async function writeDsym(exe: string, fail: Fail): Promise<void> {
  const dsymutil = existsSync("/usr/bin/dsymutil") ? "/usr/bin/dsymutil" : "dsymutil";
  const cmd = [dsymutil, exe, "-o", `${exe}.dSYM`];
  console.log(`  dsymutil ${relative(root, exe)}`);
  const r = await spawn(cmd);
  if (r.code !== 0) {
    console.error(r.out);
    fail(`dsymutil failed: ${formatCmd(cmd)}`);
  }
}

// Frameworks required by the app and GPUI's speech and webview backends.
const macFrameworks = [
  "AudioToolbox",
  "AVFoundation",
  "Cocoa",
  "CoreServices",
  "CoreText",
  "CoreGraphics",
  "ImageIO",
  "IOKit",
  "PDFKit",
  "Security",
  "Speech",
  "UniformTypeIdentifiers",
  "WebKit",
];

// mac's iconv is GNU libiconv in its own dylib; a-libarchive's charset
// conversion needs it
const macLibs = ["-lcurl", "-liconv"];

// where an app extension's executable finds the app's Contents/Frameworks:
// SumatraPDF.app/Contents/PlugIns/<name>.appex/Contents/MacOS/<name>
const appexFrameworksRpath = "@executable_path/../../../../Frameworks";

// SumatraCore's shell functions: what the stubs in coreHookSources define.
// nm on objects compiled like SumatraQL's, which links those stubs.
async function coreHookSymbols(tc: Toolchain, f: BuildFlags, dir: string, fail: Fail): Promise<string[]> {
  const ql = findTarget("SumatraQL") ?? fail("no SumatraQL target");
  const hooks: Target = { ...ql, name: "corehooks", sources: coreHookSources, coreHooks: undefined };
  const objs = await compileTarget(tc, hooks, f, dir, fail);
  const r = await spawn(["nm", "-gUm", ...objs]);
  if (r.code !== 0) fail(`nm failed: ${r.out}`);
  // "(__TEXT,__text) external _Name"; weak ones are inlines from headers
  const syms = r.out
    .split("\n")
    .filter((l) => / external _/.test(l) && !/weak|private external/.test(l))
    .map((l) => l.trim().split(/\s+/).pop()!);
  return [...new Set(syms)].sort();
}

function linkCmd(tc: Toolchain, t: Target, f: BuildFlags, out: string, inputs: string[], hooks: string[]): string[] {
  const ld = [...archFlags(f.arch)];
  if (t.kind === "dylib") ld.push("-dynamiclib", "-install_name", `@rpath/${t.name}`, "-rpath", "@loader_path");
  if (t.allLoad) ld.push("-Wl,-all_load");
  if (t.exports) {
    ld.push("-Wl,-dead_strip");
    for (const sym of t.exports) ld.push(`-Wl,-exported_symbol,${sym}`);
  }
  // resolved at load time, from the image that loads SumatraCore
  const hookFlag = { leave: "-Wl,-U,", define: "-Wl,-u,", export: "-Wl,-exported_symbol," };
  for (const sym of t.coreHooks ? hooks : []) ld.push(hookFlag[t.coreHooks!] + sym);
  if (t.kind === "app" && t.dylibs) {
    // the bundle's Frameworks, the build dir, and for a debug build the build
    // dir again by absolute path: tests run a copy of the exe from elsewhere
    ld.push("-rpath", "@executable_path/../Frameworks", "-rpath", "@executable_path");
    if (f.debug) ld.push("-rpath", outDir(f));
  }
  if (t.kind === "appex") {
    // Foundation's NSExtensionMain runs the extension; its code is in a dylib
    ld.push("-e", "_NSExtensionMain", "-rpath", appexFrameworksRpath, "-framework", "Foundation");
  } else if (t.systemDeps === "archive") {
    // base's number formatting asks Foundation for the region's separators
    ld.push("-liconv", "-framework", "Foundation");
  } else {
    for (const fw of macFrameworks) ld.push("-framework", fw);
    ld.push(...macLibs);
  }
  for (const fw of t.frameworks ?? []) ld.push("-framework", fw);
  if (f.asan) ld.push("-fsanitize=address");
  return [tc.cxx, ...inputs, "-o", out, ...ld];
}

async function link(
  tc: Toolchain,
  t: Target,
  f: BuildFlags,
  dir: string,
  objs: string[],
  libs: string[],
  fail: Fail,
): Promise<string> {
  const out = join(dir, t.name);
  const inputs = [...objs, ...libs];
  const hooks = t.coreHooks ? await coreHookSymbols(tc, f, dir, fail) : [];
  const cmd = linkCmd(tc, t, f, out, inputs, hooks);
  const key = cmd.join("\n");
  // the command line itself is an input: a changed flag relinks
  const cmdStamp = join(dir, `${t.name}-link.txt`);
  const cmdChanged = !existsSync(cmdStamp) || readFileSync(cmdStamp, "utf8") !== key;
  const needLink = cmdChanged || !existsSync(out) || inputs.some((o) => mtime(o) > mtime(out));
  const dsym = `${out}.dSYM`;
  const needDsym = needLink || !existsSync(dsym) || mtime(dsym) < mtime(out);
  if (!needLink && !needDsym) {
    console.log(`  ${t.name}: link up to date`);
    return out;
  }
  if (needLink) {
    console.log(`  ${t.name}: linking ${relative(root, out)}`);
    if (f.verbose) console.log(`> ${formatCmd(cmd)}`);
    const r = await spawn(cmd);
    if (r.code !== 0) {
      console.error(r.out);
      fail(`linking ${t.name} failed: ${formatCmd(cmd)}`);
    }
    const text = r.out.trim();
    if (text) console.log(text);
    writeFileSync(cmdStamp, key);
  }
  await writeDsym(out, fail);
  return out;
}

// ─── build a target and its dependencies ──────────────────────────────────

/** Static-library dependencies of `t` in link order (dependencies last). */
export function depsOf(t: Target, fail: Fail): Target[] {
  const out: Target[] = [];
  const visit = (name: string) => {
    const d = findTarget(name);
    if (!d) fail(`target ${t.name}: unknown dependency ${name}`);
    if (d.kind !== "staticlib") fail(`target ${t.name}: dependency ${name} is not a static library`);
    for (const n of d.deps ?? []) visit(n);
    if (!out.includes(d)) out.push(d);
  };
  for (const n of t.deps ?? []) visit(n);
  return out.reverse();
}

export async function buildTarget(tc: Toolchain, t: Target, f: BuildFlags, fail: Fail): Promise<string> {
  const dir = outDir(f);
  mkdirSync(dir, { recursive: true });

  const libs: string[] = [];
  if (t.kind !== "staticlib") {
    for (const d of depsOf(t, fail)) {
      const dep = t.systemDeps ? { ...d, systemDeps: t.systemDeps } : d;
      const objs = await compileTarget(tc, dep, f, dir, fail);
      libs.push(await archive(tc, dep, dir, objs, fail));
    }
  }
  for (const name of t.dylibs ?? []) {
    const lib = join(dir, name);
    if (!existsSync(lib)) fail(`target ${t.name}: build ${name} first`);
    libs.push(lib);
  }
  const objs = await compileTarget(tc, t, f, dir, fail);
  if (t.kind === "staticlib") return archive(tc, t, dir, objs, fail);
  if (t.embedded || depsOf(t, fail).some((d) => d.embedded)) {
    objs.push(await embedLzsa(tc, join(dir, "embedded.lzsa"), dir, archFlags(f.arch)));
  }
  return link(tc, t, f, dir, objs, libs, fail);
}

// Compiles `file`, standing in for the target's source `asSource`, alone with
// target `name`'s flags for `arch`, `firstIncludes` ahead of the target's
// include dirs: cmd/amalgam.ts checks a regenerated amalgamation with it
// before it replaces the one in ext/.
export async function checkCompile(
  tc: Toolchain,
  name: string,
  file: string,
  asSource: string,
  firstIncludes: string[],
  arch: Arch,
  fail: Fail,
): Promise<void> {
  const t0 = findTarget(name) ?? fail(`unknown target: ${name}`);
  const f: BuildFlags = { debug: false, asan: false, clean: false, verbose: false, arch };
  const dir = join(outDir(f), "amalgam-check");
  const t = withGenerated(forArch(t0, arch), arch, dir);
  const withFirst = { ...t, includes: [...firstIncludes.map((p) => relative(root, p)), ...(t.includes ?? [])] };
  const cpp = !isCSource(file);
  const cmd = [cpp ? tc.cxx : tc.cc, ...cflags(withFirst, f, cpp), ...perSourceFlags(t, asSource, arch)];
  cmd.push("-c", file, "-o", "/dev/null");
  console.log(`> ${formatCmd(cmd)}`);
  const r = await spawn(cmd);
  if (r.out.trim()) console.log(r.out.trim());
  if (r.code !== 0) fail(`${file} does not compile for ${arch}`);
}

export function cleanOutDir(f: BuildFlags): void {
  const dir = outDir(f);
  if (!existsSync(dir)) return;
  console.log(`cleaning ${relative(root, dir)}`);
  rmSync(dir, { recursive: true, force: true });
}
