// #1198: clicking a link whose JavaScript is app.popUpMenu(...) shows its items
// in a popup at the cursor (orig's TrackPopupMenu); picking "Label: url"
// launches the url. ftp isn't an allowed link protocol, so the pick is read from
// the log and no browser opens.
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

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

async function uiState(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
}

// a left click at a point in window dips
async function click(client: ControlClient, pt: { x: number; y: number }): Promise<void> {
  const res = await client.request(ControlCommand.TestInput, ["click", pt.x, pt.y, 0, 0]);
  if (res[0] !== 0 || !String(res[1] ?? "").startsWith("OK")) {
    throw new Error(`issue-1198-popup: click at ${pt.x},${pt.y}: ${res[1]}`);
  }
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

// the log lines about following the link and the pick
function relevant(log: string): string {
  return log
    .split("\n")
    .filter((l) => /FollowLink|GotoLink|JsMenu|ContextMenu|LaunchBrowser/.test(l))
    .join("\n");
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

    const m = /canvas=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(await uiState(client));
    if (!m) {
      throw new Error("issue-1198-popup: no canvas rect");
    }
    await click(client, { x: +m[1]! + Math.floor(+m[3]! / 2), y: +m[2]! + Math.floor(+m[4]! / 2) });
    await pollUntil(
      () => uiState(client),
      (s) => / popup=1/.test(s),
      { error: (s) => `issue-1198-popup: the link didn't open a popup: ${s}\n${relevant(log())}` },
    );
    const row = await menuRow(client, kPick);
    await click(client, row);

    await pollUntil(log, (s) => s.includes("protocol 'ftp' is not allowed"), {
      error: (s) => `issue-1198-popup: picking '${kPick}' at ${row.x},${row.y} didn't launch its url:\n${relevant(s)}`,
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
