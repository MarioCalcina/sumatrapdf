import { join } from "node:path";
import { ROOT } from "./util.ts";
import { outDirName, resolveSources, type BuildFlags } from "../cmd/helper/ng-compile";
import { forArch, targets } from "../cmd/helper/ng-targets";
import { hostArch, macArchs } from "../cmd/helper/ng-toolchain";

function run(args: string[], script = "ng-build.ts"): { code: number; stdout: string; stderr: string } {
  const proc = Bun.spawnSync(["bun", join(ROOT, "cmd", script), ...args], { cwd: ROOT });
  return {
    code: proc.exitCode,
    stdout: proc.stdout.toString(),
    stderr: proc.stderr.toString(),
  };
}

function check(condition: boolean, message: string): void {
  if (!condition) throw new Error(message);
}

export async function testit(): Promise<void> {
  const ngHelp = run([], "ng-build.ts");
  check(ngHelp.code === 0 && ngHelp.stdout.includes("Usage: bun cmd/ng-build.ts"), "ng did not print usage");
  const ngUnknown = run(["-unknown"], "ng-build.ts");
  check(ngUnknown.code !== 0 && ngUnknown.stderr.includes("unknown option"), "ng accepted an unknown option");
  const ngConflict = run(["-dbg", "-rel"], "ng-build.ts");
  check(
    ngConflict.code !== 0 && ngConflict.stderr.includes("cannot be used together"),
    "ng accepted conflicting configs",
  );

  const makeLzsa = targets.find((target) => target.name === "MakeLZSA");
  check(makeLzsa?.systemDeps === "archive", "MakeLZSA pulls desktop system dependencies");

  const flags: BuildFlags = { debug: true, asan: false, clean: false, verbose: false, arch: hostArch };
  check(outDirName(flags) === "mac/dbg", "wrong debug directory");
  check(outDirName({ ...flags, debug: false }) === "mac/rel", "wrong release directory");
  check(outDirName({ ...flags, asan: true }) === "mac/dbg-asan", "wrong ASan directory");
  const other = macArchs.find((a) => a !== hostArch)!;
  check(outDirName({ ...flags, arch: other }) === `mac/dbg-${other}`, "wrong directory for the other architecture");
  const fail = (msg: string): never => {
    throw new Error(msg);
  };
  for (const target of targets) {
    for (const arch of macArchs) {
      for (const src of resolveSources(forArch(target, arch), fail)) {
        check(!src.startsWith("src/") || src.startsWith("src/ng/"), `${target.name} builds ${src} outside src/ng`);
      }
    }
  }
  console.log("PASS: unified build CLI validation");
}

if (import.meta.main) {
  try {
    await testit();
  } catch (error) {
    console.error(`❌ ${error instanceof Error ? error.message : error}`);
    process.exitCode = 1;
  }
}
