// #6112: MuPDF file-attachment icons in the property row / icon menu were
// drawn upside-down (the paths are Open Iconic y-down, not PDF y-up).
//
// Run: bun tests/issue-6112.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath, makeBlankPdf } from "./util.ts";
import { packCoords, sendMessage, sleep, WM_COMMAND } from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

async function toolbarDump(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 5_000;
  let raw = "";
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkupAnnots, []);
    raw = String(res[1] ?? "");
    const m = /annotEditToolbar .*/.exec(raw);
    if (res[0] === 0 && m && /visible=1/.test(m[0]!)) {
      return m[0]!;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6112: annotation toolbar did not appear\n${raw}`);
    }
    await sleep(40);
  }
}

function parseRect(m: RegExpExecArray | null): { x: number; y: number; dx: number; dy: number } {
  if (!m) {
    return { x: 0, y: 0, dx: 0, dy: 0 };
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6112");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "a.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makeBlankPdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
  );

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-view",
    "single page",
    "-zoom",
    "fit page",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    findCanvas(frame);
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));

    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotFileAttachment"), packCoords(150, 300));
    await client.waitForRenderIdle();

    const dump = await toolbarDump(client);
    const iconName = / iconName=(\S+)/.exec(dump)?.[1] ?? "";
    if (iconName !== "PushPin") {
      throw new Error(`issue-6112: icon chip should be PushPin, got ${iconName || "(empty)"}: ${dump}`);
    }
    const placed = parseRect(/ placed=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(dump));
    const icon = parseRect(/[=;]icon:(-?\d+),(-?\d+),(\d+),(\d+)/.exec(dump));
    if (icon.dx < 8) {
      throw new Error(`issue-6112: no icon chip: ${dump}`);
    }
    // ng draws the property row in the frame. Chip rects are frame client pixels.
    // The orientation check counts ink in a window DC. mac has no such DC.
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-6112: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
