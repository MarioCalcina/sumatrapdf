// Crash 2026-09-09-15-43-0ee0: the home page layout was cached in one global,
// but every window has its own home chrome laid out and painted from it.
// Opening a second window rebuilt the cache for that window, and the first one
// then painted with the second window's geometry (and pointers into the
// rebuilt, freed thumbs vector).
import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { cmdId, ROOT, runStandalone, tmpPath } from "./util";
import { setCursorPos, sleep } from "./winapi";
import { killAndWait, launchControlled, sendCommand } from "./win-automation";
import type { ControlClient } from "./control.ts";

const nFiles = 6;

function makeAppDir(): string {
  const dir = tmpPath("home-two-windows");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(join(dir, "sub"), { recursive: true });
  const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const states: string[] = [];
  for (let i = 0; i < nFiles; i++) {
    const p = join(dir, "sub", `doc-${String(i).padStart(2, "0")}.pdf`);
    copyFileSync(src, p);
    states.push(`\t[\n\t\tFilePath = ${p}\n\t\tOpenCount = ${nFiles - i}\n\t]`);
  }
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nRememberOpenedFiles = true\n` +
      `HomePageViewMode = thumbnails\nFileStates [\n${states.join("\n")}\n]\n`,
  );
  return dir;
}

async function searchRect(client: ControlClient, winIdx = 0, timeoutMs = 8000): Promise<number[]> {
  const deadline = Date.now() + timeoutMs;
  let raw = "";
  for (;;) {
    const h = await client.homeSelection(undefined, winIdx);
    raw = h.raw;
    // ng has no search Edit. The box width is the per-window layout, so wait
    // until that width is real before treating the page as laid out.
    if (h.ready && h.search[2] > 0) {
      return h.search;
    }
    if (Date.now() > deadline) {
      throw new Error(`home-two-windows: home page never laid out (${raw})`);
    }
    await sleep(50);
  }
}

const same = (a: number[], b: number[]) => a.length === b.length && a.every((v, i) => v === b[i]);

export async function testit(): Promise<void> {
  // a cursor over the thumbnails keeps changing the hover selection
  setCursorPos(0, 0);
  const dir = makeAppDir();
  const { proc, client, frame } = await launchControlled(["-appdata", dir]);
  try {
    // the control commands report the first window, so this is its layout
    const first = await searchRect(client, 0);
    const firstDx = first[2];

    // second window: its home page lays out at a clearly different width
    sendCommand(frame, cmdId("CmdNewWindow"));
    const deadline = Date.now() + 8000;
    let second = 0;
    while (!second && Date.now() < deadline) {
      const raw = String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
      const n = / windows=(\d+)/.exec(raw);
      second = n && +n[1] >= 2 ? 1 : 0;
      await sleep(100);
    }
    if (!second) {
      throw new Error("home-two-windows: second window never appeared");
    }
    const placed = await client.request(ControlCommand.TestUiState, ["place", 40, 40, 700, 620, 1]);
    if (placed[0] !== 0) {
      throw new Error(`home-two-windows: could not resize the second window (${placed[1]})`);
    }
    let secondDx = 0;
    while (Date.now() < deadline) {
      const h = await client.homeSelection(undefined, 1);
      secondDx = h.ready ? h.search[2] : 0;
      if (secondDx > 0 && secondDx !== firstDx) {
        break;
      }
      await sleep(100);
    }
    if (!secondDx || secondDx === firstDx) {
      throw new Error(`home-two-windows: second window did not lay out its home page (search dx ${secondDx})`);
    }

    // the first window keeps its own layout: repainting it must not adopt the
    // second window's
    // mac has no canvas HWND to invalidate. A few frames is the repaint.
    const checks = 8;
    for (let i = 0; i < checks; i++) {
      if (i > 0) {
        await sleep(50);
      }
      const back = await searchRect(client, 0);
      if (!same(back, first)) {
        throw new Error(
          `home-two-windows: first window painted with the second window's layout: search=${back}, expected ${first}`,
        );
      }
      const firstDxNow = back[2];
      if (firstDxNow !== firstDx) {
        throw new Error(`home-two-windows: first window's search box was moved by the second window's layout`);
      }
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
