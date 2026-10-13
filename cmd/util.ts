import { copyFileSync, existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join, dirname, extname, resolve } from "node:path";

// user docs live in the sumatra-website repo, checked out next to this one
export const websiteDir = resolve(import.meta.dir, "..", "..", "hack", "webapps", "sumatra-website");
export const websiteDocsDir = join(websiteDir, "www", "docs");

export async function runLogged(cmd: string, args: string[], cwd?: string): Promise<void> {
  const short = [cmd.split("/").pop(), ...args].join(" ");
  console.log(`> ${short}`);
  const proc = Bun.spawn([cmd, ...args], {
    cwd,
    stdout: "inherit",
    stderr: "inherit",
    stdin: "inherit",
  });
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    throw new Error(`command failed with exit code ${exitCode}`);
  }
}

// clang-format on PATH or inside the Xcode toolchain
export function findClangFormat(): string {
  const onPath = Bun.which("clang-format");
  if (onPath) return onPath;
  const r = Bun.spawnSync(["xcrun", "--find", "clang-format"], { stdout: "pipe", stderr: "ignore" });
  const path = r.stdout.toString().trim();
  if (r.exitCode === 0 && path) return path;
  throw new Error("couldn't find clang-format (brew install clang-format)");
}

// clang-format every generated C++ file so gen-code output matches cmd/format.ts
export async function clangFormatFiles(rootDir: string, relativePaths: string[]): Promise<void> {
  const clangFormatPath = findClangFormat();
  for (const rel of [...new Set(relativePaths)]) {
    const path = join(rootDir, rel);
    await runLogged(clangFormatPath, ["-i", "-style=file", path]);
  }
}

export async function isGitClean(dir: string): Promise<boolean> {
  const proc = Bun.spawn(["git", "status", "--porcelain"], {
    cwd: dir || ".",
    stdout: "pipe",
    stderr: "inherit",
  });
  const out = (await new Response(proc.stdout).text()).trim();
  await proc.exited;
  if (out.length > 0) {
    console.log(`git status --porcelain returned:\n'${out}'`);
  }
  return out.length === 0;
}

export function extractSumatraVersion(): string {
  const path = join("src", "shared", "Version.h");
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

function normalizeNewlines(data: string): string {
  return data.replace(/\r\n/g, "\n").replace(/\r/g, "\n");
}
const extsToNormalizeNL = [".md", ".css"];
export function copyFileNormalized(dst: string, src: string): void {
  const dstDir = join(dst, "..");
  mkdirSync(dstDir, { recursive: true });
  const ext = extname(dst).toLowerCase();
  if (extsToNormalizeNL.includes(ext)) {
    const data = readFileSync(src, "utf-8");
    writeFileSync(dst, normalizeNewlines(data));
  } else {
    copyFileSync(src, dst);
  }
}
