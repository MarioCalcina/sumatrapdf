// Regression test for GHSA-p2ph-2rvm-q37m. CmdExec is reachable through
// WM_COPYDATA while the start page has no current document tab. It must ignore
// the command instead of passing a null WindowTab to RunWithExe and crashing.

import { EXE, runStandalone } from "./util.ts";
import { sendCopyDataW, sleep } from "./winapi.ts";
import { launchControlled, killAndWait } from "./win-automation.ts";

const kCopyDataDdeW = 0x44646557;

export async function testit(): Promise<void> {
  // macOS has no WM_COPYDATA. The same DDE string goes over -dbg-control,
  // which is what reaches CmdExec.
  const missingExe = `${EXE}.ghsa-p2ph-2rvm-q37m-missing`;
  const { proc, client } = await launchControlled([]);
  try {
    sendCopyDataW(1, kCopyDataDdeW, `[CmdExec "${missingExe}"]`);
    const deadline = Date.now() + 250;
    while (Date.now() < deadline) {
      if (proc.exitCode !== null) {
        throw new Error("CmdExec with no document tab terminated SumatraPDF");
      }
      await sleep(20);
    }
  } finally {
    if (proc.exitCode === null) {
      try {
        await client.quit();
      } catch {
        await killAndWait(proc);
      }
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
