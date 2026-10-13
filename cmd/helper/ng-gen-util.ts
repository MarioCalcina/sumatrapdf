// Helpers the code generators (cmd/ng-gen-settings.ts, cmd/ng-gen-commands.ts) need.
// In orig these live in cmd/util.ts next to the premake/ninja drivers, which we
// don't have; clang-format is located the way cmd/format.ts does it.

import { readFileSync } from "node:fs";
import { join } from "node:path";

export async function runLogged(cmd: string, args: string[], cwd?: string): Promise<void> {
  const short = [cmd.split("/").pop(), ...args].join(" ");
  console.log(`> ${short}`);
  const proc = Bun.spawn([cmd, ...args], { cwd, stdout: "inherit", stderr: "inherit", stdin: "inherit" });
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    throw new Error(`command failed with exit code ${exitCode}`);
  }
}

export function findClangFormat(): string | null {
  const onPath = Bun.which("clang-format");
  if (onPath) return onPath;
  const r = Bun.spawnSync(["xcrun", "--find", "clang-format"], { stdout: "pipe", stderr: "ignore" });
  const path = r.stdout.toString().trim();
  return r.exitCode === 0 && path ? path : null;
}

// clang-format every generated C++ file so the output matches cmd/format.ts
export async function clangFormatFiles(rootDir: string, relativePaths: string[]): Promise<void> {
  const exe = findClangFormat();
  if (!exe) {
    throw new Error("couldn't find clang-format");
  }
  for (const rel of [...new Set(relativePaths)]) {
    const path = join(rootDir, rel);
    await runLogged(exe, ["-i", "-style=file", path]);
  }
}

export async function isGitClean(dir: string): Promise<boolean> {
  const proc = Bun.spawn(["git", "status", "--porcelain"], { cwd: dir || ".", stdout: "pipe", stderr: "inherit" });
  const out = (await new Response(proc.stdout).text()).trim();
  await proc.exited;
  if (out.length > 0) {
    console.log(`git status --porcelain returned:\n'${out}'`);
  }
  return out.length === 0;
}

export function extractSumatraVersion(): string {
  const path = join(import.meta.dir, "..", "..", "src", "shared", "Version.h");
  const content = readFileSync(path, "utf-8");
  const prefix = "#define CURR_VERSION ";
  for (const line of content.split("\n")) {
    if (line.startsWith(prefix)) {
      const ver = line.substring(prefix.length).trim();
      const parts = ver.split(".");
      if (parts.length === 0 || parts.length > 3) throw new Error(`invalid version: ${ver}`);
      for (const p of parts) {
        if (isNaN(parseInt(p, 10))) throw new Error(`invalid version: ${ver}`);
      }
      return ver;
    }
  }
  throw new Error(`couldn't extract CURR_VERSION from ${path}`);
}
