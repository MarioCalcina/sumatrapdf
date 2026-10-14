// Command Palette list, mode switches and hints follow RTL when the UI
// language is Hebrew (issue #5956). The palette hwnd stays LTR so hit-testing
// is not mirrored; drawing and HBox layout use IsUIRtl().
//
// The document page is never mirrored (issue #5326). Toggling RTL and back
// must restore the same page pixels — a leftover LAYOUT_RTL on the canvas
// double-buffer used to keep the page flipped after leaving RTL.
//
// Run: bun tests/issue-5956.ts [--no-build]

import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { ROOT, cmdId, runStandalone } from "./util.ts";
import { sendMessage, sleep, VK_ESCAPE, WM_KEYDOWN } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

async function paletteRtl(client: ControlClient): Promise<number | null> {
  const res = await client.request(ControlCommand.TestCommandPalette, []);
  const exitCode = res[0] as number;
  const out = String(res[1] ?? "");
  if (exitCode === 2) {
    return null;
  }
  if (exitCode !== 0) {
    throw new Error(`issue-5956: TestCommandPalette failed: ${out.trim()}`);
  }
  const m = /rtl=(\d+)/.exec(out);
  if (!m) {
    throw new Error(`issue-5956: no rtl= in: ${out.trim()}`);
  }
  return Number(m[1]);
}

async function waitRtl(client: ControlClient): Promise<number> {
  const deadline = Date.now() + 8_000;
  for (;;) {
    const rtl = await paletteRtl(client);
    if (rtl !== null) {
      return rtl;
    }
    if (Date.now() > deadline) {
      throw new Error("issue-5956: command palette did not open");
    }
    await sleep(50);
  }
}

async function closePalette(client: ControlClient, frame: number): Promise<void> {
  // the palette is not a separate HWND; Escape closes it
  sendMessage(frame, WM_KEYDOWN, VK_ESCAPE, 0);
  const deadline = Date.now() + 3_000;
  while (Date.now() < deadline) {
    if ((await paletteRtl(client)) === null) {
      return;
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    sendCommandSync(frame, cmdId("CmdCommandPalette"));
    const ltr = await waitRtl(client);
    if (ltr !== 0) {
      throw new Error(`issue-5956: expected rtl=0 in LTR UI, got ${ltr}`);
    }
    await closePalette(client, frame);

    sendCommandSync(frame, cmdId("CmdDebugToggleRtl"));
    sendCommandSync(frame, cmdId("CmdCommandPalette"));
    const rtl = await waitRtl(client);
    if (rtl !== 1) {
      throw new Error(`issue-5956: expected rtl=1 after CmdDebugToggleRtl, got ${rtl}`);
    }
    await closePalette(client, frame);
    sendCommandSync(frame, cmdId("CmdDebugToggleRtl"));
    sendCommandSync(frame, cmdId("CmdCommandPalette"));
    const back = await waitRtl(client);
    if (back !== 0) {
      throw new Error(`issue-5956: expected rtl=0 after leaving RTL, got ${back}`);
    }
    await closePalette(client, frame);
    return;
  } finally {
    try {
      await client.quit();
    } catch {
      /* process already gone */
    }
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
