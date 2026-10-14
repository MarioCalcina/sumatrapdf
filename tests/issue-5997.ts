// #5997: Copy Comment copied the annotation's hover text. FreeText hover text
// is only its author (the contents are already visible on the page), so the
// clipboard got the author instead of the annotation's /Contents string.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control";
import { cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util";
import { sleep } from "./winapi";
import { killAndWait, launchControlled } from "./win-automation";

const AUTHOR = "Annotation author";
const CONTENTS = "Copied annotation content";

// A blank A4 page with one FreeText annotation and a visible appearance.
// Keep the annotation centered horizontally and in the upper quarter so its
// context menu can be opened at a DPI/window-size-independent canvas point.
function makePdf(): Buffer {
  const appearance = `BT /Helv 24 Tf 0 0 1 rg 230 625 Td (${CONTENTS}) Tj ET`;
  const objects = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R] /Count 1 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Resources << /Font << /Helv 5 0 R >> >> /Annots [4 0 R] >>`,
    `<< /Type /Annot /Subtype /FreeText /Rect [197.5 560 397.5 700] /DA (/Helv 24 Tf 0 0 1 rg) /T (${AUTHOR}) /Contents (${CONTENTS}) /AP << /N 6 0 R >> >>`,
    `<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>`,
    `<< /Type /XObject /Subtype /Form /BBox [0 0 595 842] /Resources << /Font << /Helv 5 0 R >> >> /Length ${appearance.length} >>\nstream\n${appearance}\nendstream`,
  ];
  let pdf = "%PDF-1.7\n%\xe2\xe3\xcf\xd3\n";
  const offsets: number[] = [];
  for (let i = 0; i < objects.length; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i + 1} 0 obj\n${objects[i]}\nendobj\n`;
  }
  const xref = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${objects.length + 1}\n0000000000 65535 f \n`;
  for (const offset of offsets) {
    pdf += `${offset.toString().padStart(10, "0")} 00000 n \n`;
  }
  pdf += `trailer\n<< /Size ${objects.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

function setClipboard(value: string): void {
  const res = Bun.spawnSync(["pbcopy"], { stdin: Buffer.from(value), stdout: "pipe", stderr: "pipe" });
  if (res.exitCode !== 0) {
    throw new Error(`issue-5997: pbcopy failed: ${res.stderr.toString()}`);
  }
}

function clipboardText(): string {
  const res = Bun.spawnSync(["pbpaste"], { stdout: "pipe", stderr: "pipe" });
  if (res.exitCode !== 0) {
    throw new Error(`issue-5997: pbpaste failed: ${res.stderr.toString()}`);
  }
  return res.stdout.toString().replace(/\r?\n$/, "");
}

// screen= is the frame point TestContextMenuAt and ctxcmd use.
async function freeTextCenter(client: ControlClient): Promise<{ x: number; y: number }> {
  const deadline = Date.now() + 5000 * SLOW_BUILD_FACTOR;
  let raw = "";
  while (Date.now() < deadline) {
    raw = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
    const m = /type=FreeText[^\n]*screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
    if (m && +m[3]! > 0 && +m[4]! > 0) {
      return { x: +m[1]! + Math.floor(+m[3]! / 2), y: +m[2]! + Math.floor(+m[4]! / 2) };
    }
    await sleep(40);
  }
  throw new Error(`issue-5997: FreeText screen rect missing\n${raw}`);
}

async function copyCommentAt(client: ControlClient, x: number, y: number): Promise<void> {
  const menu = await client.request(ControlCommand.TestContextMenuAt, [x, y]);
  const raw = String(menu[1] ?? "");
  if (!raw.includes("Copy Comment")) {
    throw new Error(`issue-5997: annotation context menu did not offer Copy Comment\n${raw}`);
  }
  const ran = await client.request(ControlCommand.TestUiState, ["ctxcmd", cmdId("CmdCopyComment")]);
  if (String(ran[1] ?? "") !== "ok") {
    throw new Error(`issue-5997: Copy Comment failed: ${String(ran[1] ?? "")}`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5997");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\n",
  );
  const pdf = join(dir, "free-text.pdf");
  writeFileSync(pdf, makePdf());

  const sentinel = "issue-5997 clipboard sentinel";
  setClipboard(sentinel);

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    dir,
    "-view",
    "single page",
    "-zoom",
    "fit page",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    const at = await freeTextCenter(client);
    await copyCommentAt(client, at.x, at.y);
    const deadline = Date.now() + 5000 * SLOW_BUILD_FACTOR;
    let copied = sentinel;
    while (copied === sentinel && Date.now() < deadline) {
      await sleep(50);
      copied = clipboardText();
    }
    if (copied !== CONTENTS) {
      throw new Error(
        `issue-5997: copied ${JSON.stringify(copied)}, expected ${JSON.stringify(CONTENTS)} (author was ${AUTHOR})`,
      );
    }
    console.log("issue-5997: copied the FreeText contents");
    return;
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
