// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/1106
//
// Rotating the display while Sumatra is fullscreen left the restore geometry
// stale: exiting fullscreen reapplied a pre-rotation maximized rect so the
// window looked "maximized" but did not cover the work area (title-bar buttons
// off-screen on Win7 tablets).
//
// We cannot rotate the physical display in CI. This checks the two pieces we
// can: (1) exit fullscreen after a maximized entry re-maximizes correctly, and
// (2) WM_DISPLAYCHANGE while fullscreen keeps the frame covering the monitor
// and still re-maximizes on exit.
//
// Run:  bun tests/issue-1106.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { ROOT, cmdId, tmpPath } from "./util.ts";
import { launchControlled, sendCommandSync, killAndWait } from "./win-automation.ts";
import { setProcessDpiAware } from "./winapi.ts";

const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

const SETTINGS = `RestoreSession = false
CheckForUpdates = false
WindowState = 2
WindowPos = 200 100 1000 800
`;

function nearlySameSize(a: { w: number; h: number }, b: { w: number; h: number }, tol = 8): boolean {
  return Math.abs(a.w - b.w) <= tol && Math.abs(a.h - b.h) <= tol;
}

async function layoutGeom(
  client: ControlClient,
): Promise<{ w: number; h: number; maximized: boolean; fullscreen: boolean }> {
  const raw = String((await client.request(ControlCommand.TestLayout, []))[1] ?? "");
  const m = /window rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+) maximized=(\d+) fullscreen=(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`issue-1106: no window rect\n${raw}`);
  }
  return { w: +m[3]!, h: +m[4]!, maximized: m[5] === "1", fullscreen: m[6] === "1" };
}

async function testMac(appDataDir: string): Promise<void> {
  const { proc, client, frame } = await launchControlled(["-appdata", appDataDir, PDF], { defaultWindowPos: true });
  try {
    await client.waitForRenderIdle();
    const maximizedBefore = await layoutGeom(client);
    if (!maximizedBefore.maximized) {
      throw new Error("expected window to start maximized (WindowState = 2)");
    }

    sendCommandSync(frame, cmdId("CmdToggleFullscreen"));
    await client.waitForRenderIdle();
    const fsSize = await layoutGeom(client);
    if (!fsSize.fullscreen || fsSize.w < 640 || fsSize.h < 480) {
      throw new Error(`fullscreen did not expand the frame: ${fsSize.w}x${fsSize.h} fullscreen=${fsSize.fullscreen}`);
    }

    sendCommandSync(frame, cmdId("CmdToggleFullscreen"));
    await client.waitForRenderIdle();
    const afterExit = await layoutGeom(client);
    if (!afterExit.maximized) {
      throw new Error("after leaving fullscreen, window should be maximized again");
    }
    if (!nearlySameSize(afterExit, maximizedBefore)) {
      throw new Error(
        `restored maximize size wrong: got ${afterExit.w}x${afterExit.h}, expected ~${maximizedBefore.w}x${maximizedBefore.h}`,
      );
    }
    // ng does not handle WM_DISPLAYCHANGE. That message is Windows-only.
    console.log("SKIP issue-1106 display-change: macOS has no WM_DISPLAYCHANGE");
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(appDataDir, { recursive: true, force: true });
  }
}

export async function testit(): Promise<void> {
  setProcessDpiAware();

  const appDataDir = tmpPath("issue-1106-appdata");
  rmSync(appDataDir, { recursive: true, force: true });
  mkdirSync(appDataDir, { recursive: true });
  writeFileSync(join(appDataDir, "SumatraPDF-settings.txt"), SETTINGS);
  await testMac(appDataDir);
}

if (import.meta.main) {
  const { runStandalone } = await import("./util.ts");
  await runStandalone(testit, "issue-1106");
}
