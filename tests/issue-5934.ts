// Issue #5934: close the translate dialog with Esc / Ctrl+W.
//
// Esc is exercised with posted WM_KEYDOWN (no modifiers). Ctrl+W needs
// GetKeyState(VK_CONTROL) which posted messages do not set, so that path is
// covered by code review / manual check rather than this script.

import { writeFileSync } from "node:fs";
import { ControlCommand } from "./control.ts";
import { cmdId, SLOW_BUILD_FACTOR, tmpPath } from "./util";
import { launchControlled, killAndWait } from "./win-automation";
import { sendMessage, sleep, VK_ESCAPE, WM_COMMAND } from "./winapi";

function makeTextPdf(): Buffer {
  const line = "hello translation window";
  const content = `BT /F1 24 Tf 72 720 Td (${line}) Tj ET`;
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
  ];
  let pdf = "%PDF-1.5\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefPos = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    pdf += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xrefPos}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

const ESC_SETTLE_MS = 300;

async function testTranslateEscCloses(): Promise<void> {
  const pdf = tmpPath("issue-5934-translate.pdf");
  writeFileSync(pdf, makeTextPdf());

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();

    // select some text so CmdTranslateSelection has something to show
    sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdSelectTextViaKeyboard")), 0n);
    for (let i = 0; i < 5; i++) {
      sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdExtendSelectionWordRight")), 0n);
    }

    sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdTranslateSelection")), 0n);
    // The translate UI is a tool window. enumWindows does not see it.
    const deadline = Date.now() + 4000;
    let list = "";
    while (Date.now() < deadline) {
      list = String((await client.request(ControlCommand.TestToolWindow, ["list"]))[1] ?? "");
      if (/(^|\n)translate /.test(list)) {
        break;
      }
      await sleep(100);
    }
    if (!/(^|\n)translate /.test(list)) {
      throw new Error(`translate: dialog did not appear (selection empty?)\n${list}`);
    }
    // a key sent before the tool window's first frame is lost: press until it closes
    const closeBy = Date.now() + 3000 * SLOW_BUILD_FACTOR;
    while (Date.now() < closeBy) {
      await client.request(ControlCommand.TestToolWindow, ["input", "translate", "key", VK_ESCAPE, 0, 0, 0]);
      await sleep(ESC_SETTLE_MS);
      list = String((await client.request(ControlCommand.TestToolWindow, ["list"]))[1] ?? "");
      if (!/(^|\n)translate /.test(list)) {
        console.log("  translate: Esc closes ✓");
        return;
      }
    }
    throw new Error(`translate: Esc did not close the translate dialog\n${list}`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  await testTranslateEscCloses();
}

if (import.meta.main) {
  const { runStandalone } = await import("./util");
  await runStandalone(testit);
}
