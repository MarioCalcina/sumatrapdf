// #5980: Show Links must outline only links, not annotation/comment elements.
// The PDF has one invisible-border link and one large red underline annotation
// with comment text. The old non-image filter drew a second blue box around the
// underline; the fixed link-kind filter draws exactly one blue component.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { launchControlled, killAndWait, sendCommand } from "./win-automation.ts";

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
ReuseInstance = false
ShowLinks = true
`;

function makePdf(): Buffer {
  const content = "";
  const appearance = "1 0 0 RG 3 w 0 2 m 500 2 l S";
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R] /Count 1 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Annots [5 0 R 6 0 R] >>`,
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
    `<< /Type /Annot /Subtype /Link /Rect [60 600 140 640] /Border [0 0 0] ` +
      `/A << /S /URI /URI (https://example.com/) >> >>`,
    `<< /Type /Annot /Subtype /Underline /Rect [50 100 550 230] ` +
      `/QuadPoints [50 230 550 230 50 100 550 100] /Contents (not a link) /T (Tester) ` +
      `/C [1 0 0] /AP << /N 7 0 R >> >>`,
    `<< /Type /XObject /Subtype /Form /BBox [0 0 500 130] /Resources << >> ` +
      `/Length ${appearance.length} >>\nstream\n${appearance}\nendstream`,
  ];

  let pdf = "%PDF-1.7\n%\xe2\xe3\xcf\xd3\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xref = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const offset of offsets) {
    pdf += `${offset.toString().padStart(10, "0")} 00000 n \n`;
  }
  pdf += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5980");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS);
  const pdf = join(dir, "link-and-underline.pdf");
  writeFileSync(pdf, makePdf());

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
    await client.setNotificationsEnabled(false);
    await client.waitForRenderIdle();

    // Each outlined element is one drawn shape (TestOverlayState, ng wire 102).
    // Windows 102 is StopPerfLog. Two toggles force a paint with links back on.
    sendCommand(frame, cmdId("CmdToggleLinks"));
    sendCommand(frame, cmdId("CmdToggleLinks"));
    const kNgTestOverlayState = 102 as ControlCommand;
    let shapes = -1;
    let links = -1;
    let raw = "";
    const deadline = Date.now() + 3000 * SLOW_BUILD_FACTOR;
    while (Date.now() < deadline) {
      const res = await client.request(kNgTestOverlayState, []);
      raw = String(res[1] ?? "");
      shapes = Number(/drawn shapes=(\d+)/.exec(raw)?.[1] ?? "-1");
      links = Number(/links=(\d+)/.exec(raw)?.[1] ?? "-1");
      if (links === 1 && shapes > 0) {
        break;
      }
      await sleep(50);
    }
    if (links !== 1 || shapes !== 1) {
      throw new Error(`issue-5980: Show Links drew ${shapes} shapes, expected only the link (links=${links})\n${raw}`);
    }
    console.log(`issue-5980: one link outline (drawn shapes=${shapes}), underline ignored`);
    return;
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
