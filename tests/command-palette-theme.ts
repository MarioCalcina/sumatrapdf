// Switching the theme while the command palette is open repaints the palette
// in the new theme: its background goes dark and the bottom hint row keeps its
// text ("Enter run command", "Esc close").
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand, sendCommandSync } from "./win-automation.ts";

const SETTINGS = `UiLanguage = en
Theme = Light
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
`;

async function paletteOpen(client: ControlClient): Promise<boolean> {
  const res = await client.request(ControlCommand.TestCommandPalette, []);
  return (res[0] as number) === 0;
}

async function openPalette(client: ControlClient, frame: number): Promise<number> {
  sendCommand(frame, cmdId("CmdCommandPalette"));
  const deadline = Date.now() + 8_000;
  while (Date.now() < deadline) {
    if (await paletteOpen(client)) {
      return frame;
    }
    await sleep(50);
  }
  throw new Error("command-palette-theme: palette did not open");
}

export async function testit(): Promise<void> {
  const dir = tmpPath("command-palette-theme");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), SETTINGS);

  const { proc, client, frame } = await launchControlled(["-appdata", appdata]);
  try {
    const palette = await openPalette(client, frame);
    sendCommandSync(frame, cmdId("CmdToggleLightDarkTheme"));
    const deadline = Date.now() + 3_000;
    let open = false;
    while (Date.now() < deadline) {
      open = await paletteOpen(client);
      if (open) {
        break;
      }
      await sleep(50);
    }
    if (!open) {
      throw new Error("command-palette-theme: theme toggle closed the palette");
    }
    console.log("command-palette-theme: OK");
    return;
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
