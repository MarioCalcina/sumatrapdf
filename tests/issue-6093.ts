// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6093
//
// Scrollbars = smart drew nothing: the overlay scrollbar was shown, but
// SetWindowPos put it after (i.e. below) the main frame, so the frame painted
// over it. It has to sit above the frame.
//
// Run: bun tests/issue-6093.ts [--no-build]   (or via tests/run-almost-all.ts)

import { join } from "node:path";
import { cmdId, EXE, pollUntil, ROOT, runStandalone, writeAppdata } from "./util.ts";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { sendCommand, waitForFrame } from "./win-automation.ts";

export async function testit(): Promise<void> {
  const appdata = writeAppdata(
    "issue-6093",
    [
      "UiLanguage = en",
      "CheckForUpdates = false",
      "RestoreSession = false",
      "ShowStartPage = false",
      "Scrollbars = smart",
    ].join("\n"),
  );

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30000);
      await client.setNotificationsEnabled(false);

      // scrolling shows the thin bar for a few seconds
      sendCommand(frame, cmdId("CmdScrollDown"));
      // the bar is drawn in the frame, after the page, so it stays above it
      await pollUntil(
        async () => String((await client.request(ControlCommand.TestUiState, []))[1] ?? ""),
        (s) => /overlayV=1/.test(s) && /overlayAbove=1/.test(s),
        { error: (s) => `issue-6093: no visible overlay scrollbar after scrolling (${s})` },
      );
    },
    ["-appdata", appdata, "-view", "continuous", "-zoom", "200", join(ROOT, "tests", "issue-5871.pdf")],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
