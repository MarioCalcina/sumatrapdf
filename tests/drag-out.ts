// Dragging selected text or an image on the page out of the window, as orig
// does with OLE (Canvas.cpp): AppKit's dragging session gets the text, or the
// image as PNG / TIFF / a PNG file. A test's mouse is gpui's, so the session
// ends at once; what it carried stays on the drag pasteboard.
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const kText = "Drag me out please";

// text across the page's center
function textPdf(): string {
  const content = `BT /F1 40 Tf 130 382 Td (${kText}) Tj ET`;
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
  ]);
}

// a 2x2 image over the page's center, a third of the page: not a scanned page
function imagePdf(): string {
  const pixels = "\xff\x00\x00\x00\xff\x00\x00\x00\xff\xff\xff\x00";
  const content = "q 400 0 0 400 106 196 cm /Im1 Do Q";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /XObject << /Im1 4 0 R >> >> /Contents 5 0 R >>",
    `<< /Type /XObject /Subtype /Image /Width 2 /Height 2 /ColorSpace /DeviceRGB /BitsPerComponent 8 /Length ${pixels.length} >>\nstream\n${pixels}\nendstream`,
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
  ]);
}

async function uiState(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
}

async function input(client: ControlClient, kind: string, x: number, y: number, button: number): Promise<void> {
  const res = await client.request(ControlCommand.TestInput, [kind, x, y, button, 0]);
  if (res[0] !== 0 || !String(res[1] ?? "").startsWith("OK")) {
    throw new Error(`drag-out: ${kind} at ${x},${y}: ${res[1]}`);
  }
}

// a press at the canvas' center, then a move well past the drag threshold
async function dragFromCenter(client: ControlClient): Promise<void> {
  const m = /canvas=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(await uiState(client));
  if (!m) {
    throw new Error("drag-out: no canvas rect");
  }
  const x = +m[1]! + Math.floor(+m[3]! / 2);
  const y = +m[2]! + Math.floor(+m[4]! / 2);
  await input(client, "down", x, y, 0);
  await input(client, "move", x + 15, y, 1);
  await input(client, "move", x + 40, y + 5, 1);
}

function logLines(logPath: string, re: RegExp): string {
  const log = existsSync(logPath) ? readFileSync(logPath, "utf8") : "";
  return log
    .split("\n")
    .filter((l) => re.test(l))
    .join("\n");
}

async function dragOut(pdf: string, logPath: string, select: boolean, want: (pb: string) => boolean): Promise<string> {
  // the selection's floating toolbar would sit over the text the press is on
  const appdata = `${logPath}-appdata`;
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), "SelectionToolbar = false\nCheckForUpdates = false\n");
  const args = ["-appdata", appdata, "-log-to-file", logPath, "-zoom", "fit page", pdf];
  const { proc, client, frame } = await launchControlled(args);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    if (select) {
      sendCommandSync(frame, cmdId("CmdSelectAll"));
      await client.waitForRenderIdle();
    }
    const state = (s: string) => /selection=\d+ .*mouseAction=\d+/.exec(s)?.[0] ?? s;
    const before = state(await uiState(client));
    await dragFromCenter(client);
    await pollUntil(
      () => logLines(logPath, /DragOut:/),
      (s) => /started=1/.test(s),
      {
        error: (s) =>
          `drag-out: ${pdf}: no drag started:\n${s}\nbefore the press: ${before}\n` +
          logLines(logPath, /SelectAll|MouseLeft|mouseAction|Selection/),
      },
    );
    const pb = await pollUntil(
      async () => String((await client.request(ControlCommand.TestPasteboard, []))[1] ?? ""),
      want,
      { error: (s) => `drag-out: ${pdf}: drag pasteboard: ${s}` },
    );
    return pb;
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("drag-out");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const text = join(dir, "text.pdf");
  writeFileSync(text, textPdf(), "latin1");
  const image = join(dir, "image.pdf");
  writeFileSync(image, imagePdf(), "latin1");

  const pbText = await dragOut(text, join(dir, "text.log"), true, (pb) => pb.includes(kText));
  console.log(`drag-out: text: ${pbText}`);
  const pbImage = await dragOut(image, join(dir, "image.log"), false, (pb) => /public\.png/.test(pb));
  for (const t of ["public.tiff", "public.file-url"]) {
    if (!pbImage.includes(t)) {
      throw new Error(`drag-out: the image drag has no ${t}: ${pbImage}`);
    }
  }
  console.log(`drag-out: image: ${pbImage}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
