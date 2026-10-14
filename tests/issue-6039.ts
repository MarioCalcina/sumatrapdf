// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6039
//
// Page Grid was painted only for fixed-page documents even though EPUBs use
// the same paginated canvas and page-coordinate transforms.

import { mkdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import { sendCommand, waitForFrame } from "./win-automation.ts";

const EPUB = join(ROOT, "tests", "issue-5846.epub");

export async function testit(): Promise<void> {
  const appdata = tmpPath("issue-6039-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30_000);
      await client.setNotificationsEnabled(false);
      const readMarks = async () => {
        const res = String((await client.request(ControlCommand.TestCanvasFlags, ["grid-marks"]))[1] ?? "");
        const m = /marks=(\d+)/.exec(res);
        if (!m) {
          throw new Error(`issue-6039: grid-marks: ${res}`);
        }
        return Number(m[1]);
      };
      const nBefore = await readMarks();
      sendCommand(frame, cmdId("CmdTogglePageGrid"));
      await client.waitForRenderIdle(30_000);
      const nAfter = await readMarks();
      if (nAfter < nBefore + 40) {
        throw new Error(`issue-6039: EPUB page grid did not show (grid pixels before=${nBefore} after=${nAfter})`);
      }
      console.log(`  EPUB grid pixels ${nBefore} -> ${nAfter} ✓`);
    },
    ["-appdata", appdata, EPUB],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
