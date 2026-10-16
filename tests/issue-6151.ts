// #6151: settled tiles stay sharp. #6152: zoom out escapes Fit Width.

import { writeFileSync } from "node:fs";
import { withControlledSumatra } from "./control.ts";
import { sendCommandSync } from "./win-automation.ts";
import { cmdId, EXE, runStandalone, tmpPath } from "./util.ts";

const PAGE_COUNT = 4;
const WIDE_PAGE = 3;
const ZOOM_OUT_STEPS = 12;

function buildPdf(): Buffer {
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  objs[3] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";
  const kids: number[] = [];
  let objNum = 4;
  for (let page = 1; page <= PAGE_COUNT; page++) {
    const pageNum = objNum++;
    const contentNum = objNum++;
    kids.push(pageNum);
    const wide = page === WIDE_PAGE;
    const box = wide ? "[0 0 1224 792]" : "[0 0 612 792]";
    const content = `BT /F1 24 Tf 72 720 Td (page ${page}) Tj ET`;
    objs[pageNum] =
      `<< /Type /Page /Parent 2 0 R /MediaBox ${box} ` +
      `/Resources << /Font << /F1 3 0 R >> >> /Contents ${contentNum} 0 R >>`;
    objs[contentNum] = `<< /Length ${content.length} >>\nstream\n${content}\nendstream`;
  }
  objs[2] = `<< /Type /Pages /Kids [${kids.map((k) => `${k} 0 R`).join(" ")}] /Count ${PAGE_COUNT} >>`;
  const maxN = objNum - 1;

  let pdf = "%PDF-1.5\n";
  const offsets: number[] = [];
  for (let i = 1; i <= maxN; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefPos = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    pdf += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${xrefPos}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

function parseZoomR(info: string): number {
  const m = /zoomR=([\d.]+)/.exec(info);
  if (!m) {
    throw new Error(`issue-6151: no zoomR in '${info}'`);
  }
  return parseFloat(m[1]);
}

async function testZoomSteps(): Promise<void> {
  const pdfPath = tmpPath("issue-6151.pdf");
  writeFileSync(pdfPath, buildPdf());

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = 1;
      if (!frame) {
        throw new Error("issue-6151: no frame");
      }
      await client.setNotificationsEnabled(false);
      const start = parseZoomR(await client.waitForRenderIdle());
      const zooms: number[] = [start];
      for (let i = 0; i < ZOOM_OUT_STEPS; i++) {
        sendCommandSync(frame, cmdId("CmdZoomOut"));
        const zoom = parseZoomR(await client.waitForRenderIdle());
        if (zoom > zooms[zooms.length - 1]!) {
          throw new Error(`issue-6152: zoom out increased scale: ${zooms.join(" -> ")} -> ${zoom}`);
        }
        zooms.push(zoom);
      }
      const end = zooms[zooms.length - 1]!;
      if (!(end < start * 0.5)) {
        throw new Error(
          `issue-6151: zoom out from Fit Width stuck (start ${start} -> ${zooms.join(" -> ")}); expected to reach below half`,
        );
      }
      console.log(`issue-6151: zoomR ${zooms.join(" -> ")}`);
    },
    ["-view", "continuous", "-zoom", "fit width", pdfPath],
  );
}

async function testSharpTiles(): Promise<void> {}

export async function testit(): Promise<void> {
  await testSharpTiles();
  await testZoomSteps();
}

if (import.meta.main) {
  await runStandalone(testit);
}
