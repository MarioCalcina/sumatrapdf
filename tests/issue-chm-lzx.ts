// Test for chmdec LZX make_decode_table PRETREE overflow (CC-0010 class advisory).
//
// The control command TestChm runs chmdec's PRETREE check on its own, then reads
// every entry of a minimal malicious CHM (issue-chm-lzx-make.ts). With the fix
// the check rejects the malformed lengths; without it ASan aborts the app.
//
// Run:  bun tests/issue-chm-lzx.ts [--no-build]

import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone } from "./util.ts";

const CHM = join(import.meta.dir, "issue-chm-lzx.chm");

async function runTestChm(chmPath: string): Promise<{ code: number; out: string }> {
  return await withControlledSumatra(EXE, async (client) => {
    const [code, out] = await client.request(ControlCommand.TestChm, [chmPath]);
    return { code: Number(code), out: String(out) };
  });
}

export async function testit(): Promise<void> {
  const res = await runTestChm(CHM);
  console.log(res.out.trim());

  if (res.code !== 0) {
    throw new Error(`control TestChm exited with ${res.code} (expected 0; ASan abort means unfixed lzx.c)`);
  }
  if (!res.out.includes("pretree_isolated=REJECTED")) {
    throw new Error("expected pretree_isolated=REJECTED in output");
  }
  if (!res.out.includes("result=OK")) {
    throw new Error("expected result=OK in output");
  }
  if (!res.out.includes("chm_open=OK")) {
    throw new Error("expected chm_open=OK - evil CHM fixture did not open");
  }

  console.log("✅ CHM LZX pretree overflow rejected safely under ASan");
}

if (import.meta.main) {
  await runStandalone(testit);
}
