// Reports dependencies needed to run SumatraPDF tests, especially LaTeX/SyncTeX
// integration tests (tests/latex.ts).
//
// Run:  bun tests/check-deps.ts

import { existsSync } from "node:fs";
import { join } from "node:path";
import { findLatexEngine } from "./issue-5633.ts";
import { EXE, ROOT } from "./util.ts";

const MACTEX_INSTALL = "brew install --cask mactex-no-gui   (or BasicTeX: brew install --cask basictex)";

type Dep = {
  name: string;
  ok: boolean;
  detail?: string;
  install?: string;
};

function firstLine(text: string): string {
  return (
    text
      .split(/\r?\n/)
      .find((l) => l.trim())
      ?.trim() ?? ""
  );
}

function depFromPath(name: string, path: string | null, install?: string): Dep {
  if (path) {
    return { name, ok: true, detail: path };
  }
  return { name, ok: false, install };
}

function depFromCmd(name: string, cmd: string[], install?: string): Dep {
  try {
    const p = Bun.spawnSync({ cmd, stdout: "pipe", stderr: "pipe" });
    if (p.exitCode === 0) {
      const detail = firstLine(p.stdout.toString()) || firstLine(p.stderr.toString());
      return { name, ok: true, detail: detail || cmd.join(" ") };
    }
  } catch {
    // not installed
  }
  return { name, ok: false, install };
}

// a CLI on PATH, or in ~/.local/bin (where several installers put it, outside
// the PATH of a GUI-launched shell)
function findCli(names: string[]): string | null {
  const home = process.env.HOME ?? "";
  for (const n of names) {
    const inPath = Bun.which(n);
    if (inPath) {
      return inPath;
    }
    const local = join(home, ".local", "bin", n);
    if (existsSync(local)) {
      return local;
    }
  }
  return null;
}

function checkCore(): Dep[] {
  return [
    existsSync(EXE)
      ? { name: "SumatraPDF (dbg-asan)", ok: true, detail: EXE }
      : { name: "SumatraPDF (dbg-asan)", ok: false, install: "bun cmd/ng-build.ts -dbg -asan" },
    depFromCmd("git", ["git", "--version"], "xcode-select --install"),
    depFromCmd("clang (Xcode command line tools)", ["clang", "--version"], "xcode-select --install"),
  ];
}

function checkLatex(): Dep[] {
  return [
    depFromPath("pdflatex", findLatexEngine("pdflatex"), MACTEX_INSTALL),
    depFromPath("lualatex", findLatexEngine("lualatex"), MACTEX_INSTALL),
  ];
}

function checkAdHocOptional(): Dep[] {
  const exifPy = join(ROOT, "..", "exif-py");
  return [
    existsSync(join(exifPy, ".git"))
      ? { name: "exif-py clone (ad-hoc-exif)", ok: true, detail: exifPy }
      : {
          name: "exif-py clone (ad-hoc-exif)",
          ok: false,
          install: `git clone https://github.com/ianare/exif-py.git ${exifPy}`,
        },
    depFromPath(
      "Grok Build CLI (ad-hoc-selection-translate)",
      findCli(["grok"]),
      "Install Grok CLI from https://x.ai/",
    ),
    depFromPath(
      "Claude Code CLI (ad-hoc-selection-translate)",
      findCli(["claude"]),
      "Install Claude Code from https://claude.ai/",
    ),
    depFromPath(
      "OpenAI Codex CLI (ad-hoc-selection-translate)",
      findCli(["codex"]),
      "Install Codex CLI from https://openai.com/codex",
    ),
    depFromPath(
      "Antigravity CLI (ad-hoc-selection-translate)",
      findCli(["agy", "antigravity"]),
      "Install the Antigravity CLI (agy / antigravity)",
    ),
  ];
}

function printDep(dep: Dep): void {
  const tag = dep.ok ? "OK  " : "MISS";
  const detail = dep.detail ? `  ${dep.detail}` : "";
  console.log(`${tag}  ${dep.name}${detail}`);
  if (!dep.ok && dep.install) {
    console.log(`      ${dep.install}`);
  }
}

function printSection(title: string, deps: Dep[]): number {
  console.log(`\n=== ${title} ===`);
  for (const dep of deps) {
    printDep(dep);
  }
  return deps.filter((d) => !d.ok).length;
}

function main(): void {
  console.log("SumatraPDF test dependencies");

  let missing = 0;
  missing += printSection("Core (tests/run-almost-all.ts)", checkCore());
  const latex = checkLatex();
  missing += printSection("LaTeX / SyncTeX (tests/latex.ts)", latex);
  missing += printSection("Ad-hoc (optional)", checkAdHocOptional());

  console.log("");
  if (!latex.some((d) => d.ok)) {
    console.log("issue-5633 and ad-hoc-synctex-chinese skip without pdflatex / lualatex.");
  }
  console.log(missing === 0 ? "All checked dependencies are installed." : `${missing} dependency check(s) missing.`);
}

main();
