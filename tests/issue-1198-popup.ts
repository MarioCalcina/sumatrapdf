// #1198: clicking a link whose JavaScript is app.popUpMenu(...) shows its items
// in a popup at the cursor (orig's TrackPopupMenu); picking "Label: url"
// launches the url. ftp isn't an allowed link protocol, so the pick is read from
// the log and no browser opens.
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { macCanvasRect } from "./mac-control.ts";
import { assemblePdf, cmdId, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { clickAt, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const kPick = "Datasheet: ftp://example.com/ds";

// one page that is all link
function makePdf(): string {
  const js = `app.popUpMenu("Comment: inline part","-","${kPick}");`.replace(/[()\\]/g, (ch) => `\\${ch}`);
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 300] /Annots [4 0 R] >>",
    `<< /Type /Annot /Subtype /Link /Rect [0 0 300 300] /Border [0 0 0] /A << /S /JavaScript /JS (${js}) >> >>`,
  ]);
}

// the popup's row for `label`, from what the frame drew last
async function menuRow(client: ControlClient, label: string): Promise<{ x: number; y: number }> {
  const want = `'${label}'`;
  const layout = await pollUntil(
    async () => String((await client.request(ControlCommand.TestToolWindow, ["layout", "frame"]))[1] ?? ""),
    (s) => s.split("\n").some((l) => l.includes(want) && /rect=/.test(l)),
    { error: (s) => `issue-1198-popup: no menu row ${want}:\n${s}` },
  );
  const line = layout.split("\n").find((l) => l.includes(want) && /rect=/.test(l))!;
  const [x, y, dx, dy] = /rect=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(line)!.slice(1).map(Number);
  return { x: x! + Math.floor(dx! / 2), y: y! + Math.floor(dy! / 2) };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-1198-popup");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "menu.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  const logPath = join(dir, "log.txt");
  const log = () => (existsSync(logPath) ? readFileSync(logPath, "utf8") : "");

  const { proc, client, frame } = await launchControlled(["-log-to-file", logPath, pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommandSync(frame, cmdId("CmdZoomFitPage"));
    await client.waitForRenderIdle();

    const c = macCanvasRect();
    await clickAt(frame, Math.floor(c.x + c.dx / 2), Math.floor(c.y + c.dy / 2));
    const row = await menuRow(client, kPick);
    await clickAt(frame, row.x, row.y);

    await pollUntil(log, (s) => s.includes("protocol 'ftp' is not allowed"), {
      error: (s) => `issue-1198-popup: picking '${kPick}' didn't launch its url:\n${s.split("\n").slice(-20).join("\n")}`,
    });
    console.log("issue-1198-popup: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
