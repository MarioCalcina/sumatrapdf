// The main-window layout probe exposes geometry through -dbg-control and
// counts completed relayouts through gAfterLayout. A small direct-DC sample
// also verifies that a fullscreen transition painted its final geometry.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, ROOT, runStandalone, tmpPath } from "./util.ts";
import { launchControlled, killAndWait, sendCommandSync } from "./win-automation.ts";
import { setProcessDpiAware, sleep } from "./winapi.ts";
import type { LayoutInfo, LayoutItem } from "./control.ts";

const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

function requireItem(layout: LayoutInfo, name: string): LayoutItem {
  const item = layout.items[name];
  if (!item) {
    throw new Error(`layout-callback: missing '${name}' in:\n${layout.raw}`);
  }
  return item;
}

export async function testit(): Promise<void> {
  setProcessDpiAware();
  const appdata = tmpPath("layout-callback-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    [
      "ShowToolbar = true",
      "ShowMenubar = false",
      // the sidebar stays open in fullscreen (only presentation hides it), so
      // the canvas-covers-the-frame check needs it off - and a PDF without an
      // outline can still get a generated one (#5724), zlib.3.pdf included
      "ShowToc = false",
      "UseTabs = true",
      "RestoreSession = false",
      "CheckForUpdates = false",
      "HtmlUI [",
      "\tUseFixedPageUI = false",
      "]",
      "",
    ].join("\n"),
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, PDF]);
  try {
    await client.setNotificationsEnabled(false);
    await client.waitForRenderIdle();
    const normal = await client.layout("start");
    const normalFrame = requireItem(normal, "frame");
    const normalCanvas = requireItem(normal, "canvas");
    const normalToolbar = requireItem(normal, "toolbar");
    const normalTabs = requireItem(normal, "tabs");
    if (!normal.watching || normal.count !== 0) {
      throw new Error(`layout-callback: probe did not start cleanly: ${normal.raw}`);
    }
    if (
      !normalCanvas.visible ||
      !normalToolbar.visible ||
      !normalTabs.visible ||
      normalFrame.rect.dx <= 0 ||
      normalFrame.rect.dy <= 0
    ) {
      throw new Error(`layout-callback: initial HWND geometry is invalid:\n${normal.raw}`);
    }

    sendCommandSync(frame, cmdId("CmdToggleFullscreen"));
    let fullscreen = await client.layout();
    let deadline = Date.now() + 3000;
    while (
      (fullscreen.count === 0 ||
        requireItem(fullscreen, "tabs").visible ||
        requireItem(fullscreen, "canvas").rect.y !== 0) &&
      Date.now() < deadline
    ) {
      await sleep(20);
      fullscreen = await client.layout();
    }
    const fullFrame = requireItem(fullscreen, "frame");
    const fullCanvas = requireItem(fullscreen, "canvas");
    const fullTabs = requireItem(fullscreen, "tabs");
    if (fullscreen.count !== 1 || fullTabs.visible) {
      throw new Error(`layout-callback: fullscreen chrome layout is wrong:\n${fullscreen.raw}`);
    }
    if (
      fullCanvas.rect.x !== 0 ||
      fullCanvas.rect.y !== 0 ||
      fullCanvas.rect.dx !== fullFrame.rect.dx ||
      fullCanvas.rect.dy !== fullFrame.rect.dy
    ) {
      throw new Error(`layout-callback: fullscreen canvas does not cover the frame:\n${fullscreen.raw}`);
    }

    sendCommandSync(frame, cmdId("CmdToggleFullscreen"));
    deadline = Date.now() + 3000;
    let restored = await client.layout();
    while (!requireItem(restored, "tabs").visible && Date.now() < deadline) {
      await sleep(20);
      restored = await client.layout();
    }
    if (!requireItem(restored, "tabs").visible) {
      throw new Error(`layout-callback: tabs did not return after leaving fullscreen:\n${restored.raw}`);
    }
    await client.waitForRenderIdle();

    const before = await client.layout("reset");
    const canvasBefore = requireItem(before, "canvas");
    // the toolbar cycles pinned -> overlay -> hidden, one relayout per step,
    // so it takes two commands to get it off the canvas
    let after = before;
    for (const wantHidden of [false, true]) {
      await client.layout("reset");
      sendCommandSync(frame, cmdId("CmdToggleToolbar"));
      deadline = Date.now() + 3000;
      const done = (l: LayoutInfo) => l.count === 1 && (!wantHidden || !requireItem(l, "toolbar").visible);
      after = await client.layout();
      while (!done(after) && Date.now() < deadline) {
        await sleep(20);
        after = await client.layout();
      }
      if (after.count !== 1) {
        throw new Error(
          `layout-callback: toggling one toolbar caused ${after.count} completed relayouts:\n${after.raw}`,
        );
      }
    }

    const canvasAfter = requireItem(after, "canvas");
    const toolbarAfter = requireItem(after, "toolbar");
    if (toolbarAfter.visible) {
      throw new Error(`layout-callback: toolbar stayed visible:\n${after.raw}`);
    }
    if (canvasAfter.rect.dy <= canvasBefore.rect.dy) {
      throw new Error(
        `layout-callback: canvas did not grow after hiding toolbar ` +
          `(${canvasBefore.rect.dy} -> ${canvasAfter.rect.dy})`,
      );
    }

    const stopped = await client.layout("stop");
    if (stopped.watching) {
      throw new Error("layout-callback: probe did not stop");
    }
    console.log(
      `  layout probe exposed ${after.nodes.length} nodes; toolbar toggle completed in ${after.count} relayout ✓`,
    );
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
