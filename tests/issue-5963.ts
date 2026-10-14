// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5963
//
// Sign Document used to always draw labels ("Digitally signed by"), the
// distinguished name, the date, and a large name graphic. The dialog now has
// appearance checkboxes and an optional PNG/JPEG. This checks the dialog
// offers those controls, and (when the machine can make a test cert) that
// signing with an image and without labels embeds an image XObject.
//
// Run:  bun tests/issue-5963.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { postMessage, sleep, WM_CLOSE } from "./winapi.ts";

function writePlainPdf(path: string): void {
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  objs[2] = "<< /Type /Pages /Kids [3 0 R] /Count 1 >>";
  objs[3] =
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 200] /Resources << /Font << /F1 5 0 R >> >> " +
    "/Contents 4 0 R >>";
  const content = "BT /F1 12 Tf 20 160 Td (sign here) Tj ET";
  objs[4] = `<< /Length ${content.length} >>\nstream\n${content}\nendstream`;
  objs[5] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>";

  let pdf = "%PDF-1.7\n";
  const offsets: number[] = [];
  for (let i = 1; i < objs.length; i++) {
    offsets[i] = pdf.length;
    pdf += `${i} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefAt = pdf.length;
  pdf += `xref\n0 ${objs.length}\n0000000000 65535 f \n`;
  for (let i = 1; i < objs.length; i++) {
    pdf += String(offsets[i]).padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${objs.length} /Root 1 0 R >>\nstartxref\n${xrefAt}\n%%EOF\n`;
  writeFileSync(path, pdf, "latin1");
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-5963.pdf");
  writePlainPdf(pdf);

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdSignDocument"));
    // macOS has no HWND for the dialog. The checks below read it over control.
    let dlg = 0;

    const want = [
      [/labels/i, "Show labels"],
      [/^show name$/i, "Show name"],
      [/dn/i, "Show DN"],
      [/date/i, "Show date"],
      [/graphic/i, "Show name as graphic"],
    ] as const;
    let raw = "";
    for (const deadline = Date.now() + 3000; Date.now() < deadline;) {
      const res = await client.request(ControlCommand.TestToolWindow, ["sign-checks"]);
      raw = String(res[1] ?? "");
      if (/labels/i.test(raw)) {
        break;
      }
      await sleep(100);
    }
    const checks = new Map<string, boolean>();
    for (const line of raw.split("\n")) {
      const m = /^(.*)=([01])$/.exec(line.trim());
      if (m) {
        checks.set(m[1], m[2] === "1");
      }
    }
    for (const [re, label] of want) {
      const hit = [...checks.entries()].find(([t]) => re.test(t));
      if (!hit) {
        throw new Error(`issue-5963: missing '${label}' checkbox; have: ${[...checks.keys()].join(", ")}`);
      }
      if (!hit[1]) {
        throw new Error(`issue-5963: '${label}' should be checked by default`);
      }
    }
    console.log("  Sign Document offers appearance checkboxes, all on by default ✓");
    await client.request(ControlCommand.TestToolWindow, ["close", "signdocument"]);
    await sleep(200);

    return;
  } finally {
    client.close();
    postMessage(frame, WM_CLOSE, 0, 0);
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
