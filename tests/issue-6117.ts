// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6117
//
// With the focus on the Find window's results list, PageUp / PageDown scrolled
// the document instead of paging the list: they accelerated to CmdScrollUpPage
// / CmdScrollDownPage before the list ever saw them. Home / End were already
// excluded from the accelerator tables a focused control uses, so they worked.
//
// Run: bun tests/issue-6117.ts [--no-build]

import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control";
import { ROOT, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util";
import { sleep } from "./winapi";
import { killAndWait, launchControlled, sendCommand } from "./win-automation";

const VK_PRIOR = 0x21;
const VK_NEXT = 0x22;
const SRC_PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
const TERM = "the";

// "sel" is the results list's current item. NOTREADY until the count scan ends.
async function resultsSel(client: ControlClient): Promise<number> {
  const deadline = Date.now() + 15000 * SLOW_BUILD_FACTOR;
  let raw = "";
  while (Date.now() < deadline) {
    const res = await client.request(ControlCommand.TestFindResultsOrder, [TERM, 1]);
    raw = String(res[1] ?? "");
    const m = /sel=(-?\d+)/.exec(raw);
    if (m) {
      return +m[1]!;
    }
    await sleep(200);
  }
  throw new Error(`issue-6117: the result scan never finished\n${raw}`);
}

async function waitForSel(client: ControlClient, pred: (sel: number) => boolean, what: string): Promise<number> {
  const deadline = Date.now() + 5000 * SLOW_BUILD_FACTOR;
  let sel = await resultsSel(client);
  while (!pred(sel) && Date.now() < deadline) {
    await sleep(25);
    sel = await resultsSel(client);
  }
  if (!pred(sel)) {
    throw new Error(`issue-6117: ${what} (selection ${sel})`);
  }
  return sel;
}

async function toolReq(client: ControlClient, args: (string | number)[]): Promise<string> {
  const res = await client.request(ControlCommand.TestToolWindow, args);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0 || raw.startsWith("ERR") || raw.startsWith("NOTREADY")) {
    throw new Error(`issue-6117: ${args.join(" ")} failed: ${raw}`);
  }
  return raw;
}

// The first results-list row: a wide click target under the two edits.
function resultRow(layout: string): { x: number; y: number } | null {
  for (const m of layout.matchAll(/hit rect=([0-9.-]+),([0-9.-]+),([0-9.-]+),([0-9.-]+) click=1 input=0 focusId=0/g)) {
    const y = Number(m[2]);
    const w = Number(m[3]);
    const h = Number(m[4]);
    if (y < 60 || w < 100 || h < 8) {
      continue;
    }
    return { x: Math.round(Number(m[1]) + 12), y: Math.round(y + h / 2) };
  }
  return null;
}

// The Find window is a gpui tool window. PageUp / PageDown go to it, which is
// what the results list sees once the search edit no longer has the click.
async function macPageResults(client: ControlClient, frame: number): Promise<void> {
  sendCommand(frame, cmdId("CmdFindFirst"));
  const openBy = Date.now() + 5000 * SLOW_BUILD_FACTOR;
  let state = "";
  while (Date.now() < openBy) {
    const res = await client.request(ControlCommand.TestToolWindow, ["state", "find"]);
    state = String(res[1] ?? "");
    if (state.startsWith("OK ")) {
      break;
    }
    await sleep(100);
  }
  if (!state.startsWith("OK ")) {
    throw new Error(`issue-6117: the Find window did not open\n${state}`);
  }
  for (const ch of TERM) {
    await toolReq(client, ["input", "find", "char", ch.charCodeAt(0), 0, 0, 0]);
  }
  await resultsSel(client);

  const laidBy = Date.now() + 5000 * SLOW_BUILD_FACTOR;
  let row: { x: number; y: number } | null = null;
  let layout = "";
  while (Date.now() < laidBy) {
    layout = await toolReq(client, ["layout", "find"]);
    row = resultRow(layout);
    if (row) {
      break;
    }
    await sleep(100);
  }
  if (!row) {
    throw new Error(`issue-6117: no result row\n${layout}`);
  }
  await toolReq(client, ["input", "find", "click", row.x, row.y, 0, 0]);
  const start = await resultsSel(client);
  if (start < 0) {
    throw new Error("issue-6117: clicking a result did not select it");
  }

  await toolReq(client, ["input", "find", "key", VK_NEXT, 0, 0, 0]);
  const afterDown = await waitForSel(
    client,
    (sel) => sel > start + 1,
    `PageDown did not move a whole page from ${start}`,
  );
  await toolReq(client, ["input", "find", "key", VK_PRIOR, 0, 0, 0]);
  await waitForSel(client, (sel) => sel < afterDown, `PageUp did not move back from ${afterDown}`);
}

export async function testit(): Promise<void> {
  const appData = tmpPath("issue-6117");
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(appData, { recursive: true });
  const pdf = join(appData, "doc.pdf");
  copyFileSync(SRC_PDF, pdf);
  writeFileSync(
    join(appData, "SumatraPDF-settings.txt"),
    ["UiLanguage = en", "CheckForUpdates = false", "RestoreSession = false", "SearchUIFloating = true", ""].join("\n"),
  );

  const { proc, client, frame } = await launchControlled(
    ["-appdata", appData, "-view", "continuous", "-zoom", "120", pdf],
    { saveSettings: true },
  );
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    await macPageResults(client, frame);
    return;
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
