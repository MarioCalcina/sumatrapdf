// The note and the ink tool show their SVG icon as the cursor over the page,
// as orig does (AnnotPlacement.cpp's SetPlacementCursor); the other tools show
// the cross. AppKit has to show the cursor the canvas made, not gpui's cross.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, makeBlankPdf, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, pressEscape, sendCommand } from "./win-automation.ts";

async function markupState(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
}

async function uiState(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
}

// the mouse moves over the page's center: that sets the canvas cursor
async function hoverPage(client: ControlClient): Promise<void> {
  const m = /canvas=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(await uiState(client));
  if (!m) {
    throw new Error("annot-placement-cursor: no canvas rect");
  }
  const x = +m[1]! + Math.floor(+m[3]! / 2);
  const y = +m[2]! + Math.floor(+m[4]! / 2);
  await client.request(ControlCommand.TestInput, ["move", x, y, 0, 0]);
  await client.request(ControlCommand.TestInput, ["move", x + 1, y + 1, 0, 0]);
}

async function wantCursor(client: ControlClient, cmd: string, want: string): Promise<void> {
  sendCommand(1, cmdId(cmd));
  await pollUntil(
    () => markupState(client),
    (s) => /Placement active=1/.test(s),
    { error: (s) => `annot-placement-cursor: ${cmd} didn't start placing:\n${s}` },
  );
  await hoverPage(client);
  await pollUntil(
    () => markupState(client),
    (s) => s.includes(want),
    { error: (s) => `annot-placement-cursor: ${cmd}: want '${want}':\n${s}` },
  );
  await pressEscape(1);
  await pollUntil(
    () => markupState(client),
    (s) => !/Placement active=1/.test(s),
    { error: (s) => `annot-placement-cursor: Esc didn't end ${cmd}:\n${s}` },
  );
}

export async function testit(): Promise<void> {
  const dir = tmpPath("annot-placement-cursor");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "blank.pdf");
  writeFileSync(pdf, makeBlankPdf(1), "latin1");

  const { proc, client } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    await wantCursor(client, "CmdCreateAnnotText", "nativeCursor=text shown=1");
    await wantCursor(client, "CmdCreateAnnotInk", "nativeCursor=ink shown=1");
    // a tool without an icon cursor keeps the cross
    await wantCursor(client, "CmdCreateAnnotSquare", "nativeCursor=none");
    console.log("annot-placement-cursor: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
