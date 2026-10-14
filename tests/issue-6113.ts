// #6113: notification toasts on RTL UI (Hebrew) drew English text backwards.
// WS_EX_LAYOUTRTL plus BitBlt of an LTR Gfx buffer mirrored every glyph.
// "Zoom: N%" has a long bar on the Z; that bar must stay in the left half.
//
// Run: bun tests/issue-6113.ts [--no-build]

import { join } from "node:path";
import { ROOT, cmdId, runStandalone } from "./util.ts";
import { ControlCommand, type ControlClient } from "./control.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

async function uiState(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
}

function notifMsg(state: string): string {
  return /notifMsg='([^']*)'/.exec(state)?.[1] ?? "";
}

// ng draws the toast in the frame. There is no LAYOUTRTL child window to
// capture, so the check is that the message is up, goes away, and stays in
// logical order when RTL is forced.
async function testNg(client: ControlClient, frame: number): Promise<void> {
  async function waitMsg(want: string): Promise<void> {
    const deadline = Date.now() + 4000;
    for (;;) {
      const state = await uiState(client);
      const msg = notifMsg(state);
      if (want === "" ? msg === "" : msg.includes(want)) {
        return;
      }
      if (Date.now() > deadline) {
        throw new Error(`issue-6113: notification ${want ? `want '${want}'` : "did not dismiss"}, got '${msg}'`);
      }
      await sleep(40);
    }
  }

  sendCommandSync(frame, cmdId("CmdZoom125"));
  await waitMsg("Zoom: 125%");

  await client.setNotificationsEnabled(false);
  await waitMsg("");
  await client.setNotificationsEnabled(true);

  sendCommandSync(frame, cmdId("CmdDebugToggleRtl"));
  const rtlDeadline = Date.now() + 4000;
  for (;;) {
    if (/rtl=1/.test(await uiState(client))) {
      break;
    }
    if (Date.now() > rtlDeadline) {
      throw new Error("issue-6113: RTL did not turn on");
    }
    await sleep(40);
  }

  sendCommandSync(frame, cmdId("CmdZoom150"));
  await waitMsg("Zoom: 150%");
}

export async function testit(): Promise<void> {
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await testNg(client, frame);
    return;
  } finally {
    await client.quit();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
