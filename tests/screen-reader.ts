// Screen readers get what orig's UI Automation provider gives them
// (uia/Provider.cpp): the document, its visible pages with their text, and the
// new selection when it changes. ng exposes them as gpui accessibility nodes,
// which AppKit hands to VoiceOver, and reads a selection out once it settles.
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const kText = "Hello screen reader";

function makePdf(): string {
  const content = `BT /F1 24 Tf 72 700 Td (${kText}) Tj ET`;
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
  ]);
}

async function frameLayout(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestToolWindow, ["layout", "frame"]))[1] ?? "");
}

export async function testit(): Promise<void> {
  const dir = tmpPath("screen-reader");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "reader.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  const logPath = join(dir, "log.txt");
  const log = () => (existsSync(logPath) ? readFileSync(logPath, "utf8") : "");

  const { proc, client, frame } = await launchControlled(["-log-to-file", logPath, "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    // the page's text joins its node once extracted: repaint until it is there
    await pollUntil(
      async () => {
        sendCommandSync(frame, cmdId("CmdZoomFitWidth"));
        sendCommandSync(frame, cmdId("CmdZoomFitPage"));
        await client.waitForRenderIdle();
        return frameLayout(client);
      },
      (s) => s.includes("label='reader.pdf'") && /label='Page 1' value='[^']*Hello screen reader/.test(s),
      {
        intervalMs: 200,
        error: (s) => `screen-reader: no document and page nodes:\n${s.split("\n").filter((l) => l.startsWith("node")).join("\n")}`,
      },
    );

    sendCommandSync(frame, cmdId("CmdSelectAll"));
    await pollUntil(log, (s) => s.includes("Accessibility: read out the selection"), {
      error: () => "screen-reader: the selection wasn't read out",
    });
    console.log("screen-reader: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
