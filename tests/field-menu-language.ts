// A text field's context menu (Cut, Copy, Paste, Select All) is gpui's, with
// its own English strings; ng builds it from the same rows in the UI language.
// The menu is AppKit's and modal, so its rows are read through -dbg-control.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

async function fieldMenu(client: ControlClient): Promise<{ ok: boolean; rows: string }> {
  const res = await client.request(ControlCommand.TestInputMenu, []);
  return { ok: res[0] === 0, rows: String(res[1] ?? "") };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("field-menu-language");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "UiLanguage = es\nCheckForUpdates = false\nRestoreSession = false\nShowStartPage = true\n",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", dir]);
  try {
    // on the start page Find focuses its search box
    const menu = await pollUntil(
      () => {
        sendCommand(frame, cmdId("CmdFindFirst"));
        return fieldMenu(client);
      },
      (m) => m.ok,
      { intervalMs: 200, error: (m) => `field-menu-language: no text field got the focus: ${m.rows}` },
    );
    const rows = menu.rows.split("|");
    for (const want of ["Cortar", "Pegar"]) {
      if (!rows.includes(want)) {
        throw new Error(`field-menu-language: want '${want}' in the menu: ${menu.rows}`);
      }
    }
    if (rows.includes("Cut") || rows.includes("Paste")) {
      throw new Error(`field-menu-language: menu still in English: ${menu.rows}`);
    }
    console.log(`field-menu-language: OK ${menu.rows}`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
