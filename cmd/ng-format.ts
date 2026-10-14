// Formats sources: clang-format for src/ng/**/*.{cpp,c,h}, prettier for cmd/**/*.ts and tests/**/*.ts.
//
//   bun cmd/ng-format.ts          # both
//   bun cmd/ng-format.ts -ts      # prettier only
//   bun cmd/ng-format.ts a.cpp b.h   # clang-format just these files

import { Glob } from "bun";
import { basename, join } from "node:path";
import { findClangFormat } from "./util";

const root = join(import.meta.dir, "..");

// generated with a layout of their own (cmd/trans-gen.ts)
const keepLayout = ["translationlangs.cpp"];

async function formatCpp(files: string[]): Promise<void> {
  const exe = findClangFormat();
  if (files.length === 0) {
    const g = new Glob("src/ng/**/*.{cpp,c,h}");
    files = Array.from(g.scanSync({ cwd: root })).filter((f) => !keepLayout.includes(basename(f).toLowerCase()));
  }
  console.log(`clang-format: ${files.length} files`);
  const batch = 50;
  for (let i = 0; i < files.length; i += batch) {
    const r = Bun.spawnSync([exe, "-i", "-style=file", ...files.slice(i, i + batch)], { cwd: root, stderr: "pipe" });
    if (r.exitCode !== 0) {
      console.error(r.stderr.toString());
      process.exit(1);
    }
  }
}

// .prettierrc.json / .prettierignore have the settings
function formatTs(): void {
  console.log("prettier: cmd/**/*.ts tests/**/*.ts");
  const r = Bun.spawnSync(["bunx", "prettier", "--write", "--log-level", "warn", "cmd/**/*.ts", "tests/**/*.ts"], {
    cwd: root,
    stdout: "inherit",
    stderr: "inherit",
  });
  if (r.exitCode !== 0) process.exit(1);
}

const args = process.argv.slice(2);
const tsOnly = args.includes("-ts");
const files = args.filter((a) => !a.startsWith("-"));
if (tsOnly) {
  formatTs();
} else if (files.length > 0) {
  await formatCpp(files);
} else {
  await formatCpp([]);
  formatTs();
}
