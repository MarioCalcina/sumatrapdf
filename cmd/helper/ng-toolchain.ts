// Finds Apple's clang (the Xcode command line tools) and names the macOS
// architectures and the oldest macOS the build targets.

export type Arch = "arm64" | "x86_64";

/** every architecture a universal build contains */
export const macArchs: Arch[] = ["arm64", "x86_64"];

export const hostArch: Arch = process.arch === "arm64" ? "arm64" : "x86_64";

// macOS 11 is the first release on Apple Silicon, and Intel Macs from 2013 on
// run it
export const macMinVersion = "11.0";

export type Toolchain = {
  /** C++ compiler executable */
  cxx: string;
  /** C compiler executable */
  cc: string;
  /** Apple's libtool: its ar can't read a response file, libtool takes a list file */
  ar: string;
  label: string;
};

export function findToolchain(fail: (msg: string) => never): Toolchain {
  if (process.platform !== "darwin") {
    fail("SumatraPDF builds on macOS only (Xcode command line tools: xcode-select --install)");
  }
  const ar = Bun.which("libtool") ?? "/usr/bin/libtool";
  const fromEnv = process.env["CXX"];
  if (fromEnv) return { cxx: fromEnv, cc: process.env["CC"] ?? fromEnv, ar, label: fromEnv };
  if (!Bun.which("clang++")) fail("clang++ not found. Install the command line tools: xcode-select --install");
  return { cxx: "clang++", cc: Bun.which("clang") ? "clang" : "clang++", ar, label: "clang++" };
}
