// #6012: Advanced Settings tracks how many values were edited this session
// and shows a "changed settings: n" banner. The dialog is at least 480px wide.
//
// Run: bun tests/issue-6012.ts [--no-build]

import { ControlCommand, type ControlClient } from "./control.ts";
import { cmdId, runStandalone } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

async function macAdvRaw(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestToolWindow, ["state", "advsettings"]))[1] ?? "");
}

function macAdvWidth(raw: string): number {
  const m = /^OK size=(\d+),/.exec(raw);
  return m ? Number(m[1]) : 0;
}

async function waitForMacDialog(client: ControlClient, timeoutMs = 8000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  let raw = "";
  while (Date.now() < deadline) {
    raw = await macAdvRaw(client);
    const dx = macAdvWidth(raw);
    if (dx > 0) {
      return dx;
    }
    await sleep(30);
  }
  throw new Error(`issue-6012: Advanced Settings did not open\n${raw}`);
}

async function macDialogOpen(client: ControlClient): Promise<boolean> {
  return macAdvWidth(await macAdvRaw(client)) > 0;
}

async function adv(client: ControlClient, action: string, arg = 0): Promise<string> {
  const deadline = Date.now() + 8000;
  while (Date.now() < deadline) {
    const res = await client.request(ControlCommand.TestAdvSettingsRows, [action, arg]);
    const exitCode = res[0] as number;
    const out = String(res[1] ?? "");
    if (exitCode === 0) {
      return out;
    }
    if (exitCode !== 2) {
      throw new Error(`issue-6012: TestAdvSettingsRows(${action}) failed: ${out.trim()}`);
    }
    await sleep(50);
  }
  throw new Error(`issue-6012: Advanced Settings never ready for ${action}`);
}

function requireMatch(out: string, re: RegExp, what: string) {
  if (!re.test(out)) {
    throw new Error(`issue-6012: ${what}: ${out.trim()}`);
  }
}

export async function testit(): Promise<void> {
  const { proc, client, frame } = await launchControlled([]);
  try {
    sendCommandSync(frame, cmdId("CmdAdvancedSettings"));
    const dx = await waitForMacDialog(client);
    if (dx < 480) {
      throw new Error(`issue-6012: client width ${dx} is below the 480px minimum`);
    }

    requireMatch(await adv(client, "changed"), /changed=0\b.*banner=0/, "fresh dialog should have no edits");
    requireMatch(await adv(client, "esc"), /closed=1/, "Esc should close when nothing is unsaved");
    const goneDeadline = Date.now() + 5000;
    const dialogGone = async () => (!(await macDialogOpen(client)));
    while (!(await dialogGone()) && Date.now() < goneDeadline) {
      await sleep(30);
    }
    if (!(await dialogGone())) {
      throw new Error("issue-6012: Esc did not close an unmodified dialog");
    }

    sendCommandSync(frame, cmdId("CmdAdvancedSettings"));
    let dlg2 = 0;
    await waitForMacDialog(client);
    requireMatch(await adv(client, "toggle", 0), /changed=1 banner=1/, "one edit must show the banner");
    requireMatch(await adv(client, "esc"), /closed=0/, "Esc must not close with unsaved edits");
    const stillOpen = await macDialogOpen(client);
    if (!stillOpen) {
      throw new Error("issue-6012: Esc closed Advanced Settings with unsaved edits");
    }
    requireMatch(await adv(client, "toggle", 1), /changed=2 banner=1/, "two edits must keep the banner");
    requireMatch(await adv(client, "esc"), /closed=0/, "Esc must not close with two unsaved edits");
    requireMatch(await adv(client, "toggle", 0), /changed=1 banner=1/, "reverting to one edit must keep the banner");
    requireMatch(await adv(client, "toggle", 1), /changed=0 banner=0/, "reverting all edits must hide the banner");
    requireMatch(await adv(client, "esc"), /closed=1/, "Esc should close after reverting all edits");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
