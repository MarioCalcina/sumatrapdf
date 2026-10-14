// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6062
//
// Closing the last document with NoHomeTab leaves an empty window. The page
// box still showed the last page number and the canvas scrollbar stayed up.
//
// Run: bun tests/issue-6062.ts [--no-build]

import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommand, sendCommandSync } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const SRC_PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

async function runCloseLastFile(scrollbars: string): Promise<void> {
  const appData = tmpPath(`issue-6062-appdata-${scrollbars}`);
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(appData, { recursive: true });
  const pdf = join(appData, "doc.pdf");
  copyFileSync(SRC_PDF, pdf);
  writeFileSync(
    join(appData, "SumatraPDF-settings.txt"),
    [
      "UiLanguage = en",
      "CheckForUpdates = false",
      "RestoreSession = false",
      "RememberOpenedFiles = false",
      "UseTabs = true",
      "NoHomeTab = true",
      "ShowStartPage = false",
      "ShowToc = false",
      `Scrollbars = ${scrollbars}`,
      "DefaultDisplayMode = continuous",
      "DefaultZoom = 200%",
      "",
    ].join("\n"),
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appData, "-zoom", "200", pdf], {
    saveSettings: true,
  });
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdGoToLastPage"));
    await client.waitForRenderIdle();
    await sleep(150);

    // ng draws the page box and the bars in the frame. There is no Edit child
    // and no WS_VSCROLL; the control pipe reports both.
    const before = String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
    const pageBefore = /pageText='([^']*)'/.exec(before)?.[1]?.trim() ?? "";
    if (!/^\d+$/.test(pageBefore)) {
      throw new Error(`issue-6062 (${scrollbars}): no page number in the toolbar before close (${before})`);
    }
    if (scrollbars === "windows" && !/scrollVis=1\//.test(before)) {
      throw new Error(`issue-6062 (${scrollbars}): expected a vertical scrollbar before close (${before})`);
    }

    sendCommandSync(frame, cmdId("CmdClose"));
    await sleep(250);

    const after = String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
    const pageAfter = /pageText='([^']*)'/.exec(after)?.[1]?.trim() ?? "";
    if (/^\d+$/.test(pageAfter)) {
      throw new Error(
        `issue-6062 (${scrollbars}): page box still shows '${pageAfter}' after closing the last file (${after})`,
      );
    }
    if (!/scrollVis=0\/0/.test(after)) {
      throw new Error(`issue-6062 (${scrollbars}): scrollbar still up after closing the last file (${after})`);
    }
    return;
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  await runCloseLastFile("windows");
  await runCloseLastFile("smart");
}

if (import.meta.main) {
  await runStandalone(testit);
}
