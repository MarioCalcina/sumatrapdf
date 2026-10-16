// The floating Find window reserves the full N / M status width from the total
// hit count, so moving to the last result cannot resize the search combo box.
import { writeFileSync } from "node:fs";
import { ControlCommand, type ControlClient } from "./control.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep, VK_END, VK_HOME, VK_RETURN } from "./winapi.ts";

const query = "needle";
// Keep page and hit counts at different digit widths: the reservation is based
// on hits, not pages.
const pageCount = 9;
const hitsPerPage = 111;
const hitCount = pageCount * hitsPerPage;

function buildPdf(): Buffer {
  const objects: string[] = [];
  const fontObject = 3;
  objects[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  objects[fontObject] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";

  const pages: number[] = [];
  let objectNumber = 4;
  for (let page = 1; page <= pageCount; page++) {
    const pageObject = objectNumber++;
    const contentObject = objectNumber++;
    pages.push(pageObject);
    const hits = Array.from({ length: hitsPerPage }, (_, i) => {
      const x = 20 + (i % 10) * 58;
      const y = 760 - Math.floor(i / 10) * 60;
      return `1 0 0 1 ${x} ${y} Tm (${query}) Tj`;
    }).join("\n");
    const content = `BT /F1 6 Tf\n${hits}\nET`;
    objects[pageObject] =
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
      `/Resources << /Font << /F1 ${fontObject} 0 R >> >> /Contents ${contentObject} 0 R >>`;
    objects[contentObject] = `<< /Length ${content.length} >>\nstream\n${content}\nendstream`;
  }
  objects[2] = `<< /Type /Pages /Kids [${pages.map((page) => `${page} 0 R`).join(" ")}] /Count ${pages.length} >>`;

  let pdf = "%PDF-1.5\n";
  const offsets: number[] = [];
  for (let i = 1; i < objectNumber; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i} 0 obj\n${objects[i]}\nendobj\n`;
  }
  const xrefOffset = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${objectNumber}\n0000000000 65535 f \n`;
  for (const offset of offsets) {
    pdf += `${offset.toString().padStart(10, "0")} 00000 n \n`;
  }
  pdf += `trailer\n<< /Size ${objectNumber} /Root 1 0 R >>\nstartxref\n${xrefOffset}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

async function waitForSelection(client: ControlClient, expected: number, timeoutMs = 30_000): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  let raw = "";
  while (Date.now() < deadline) {
    const response = await client.request(ControlCommand.TestFindResultsOrder, [query, 1]);
    raw = String(response[1] ?? "").trim();
    const match = /sel=(-?\d+)/.exec(raw);
    if (Number(response[0]) === 0 && match && Number(match[1]) === expected) {
      return;
    }
    await sleep(50);
  }
  throw new Error(`find-window-layout: selected result did not reach ${expected}: ${raw}`);
}

async function waitForPage(client: ControlClient, expected: number, timeoutMs = 8000): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  let raw = "";
  while (Date.now() < deadline) {
    const response = await client.request(ControlCommand.TestFavoriteNav, ["page", 0]);
    raw = String(response[1] ?? "").trim();
    // the reply carries the scroll position too ("OK page=N y=N"), so match
    // the page rather than the whole line
    const m = /OK page=(\d+)/.exec(raw);
    if (m && Number(m[1]) === expected) {
      return;
    }
    await sleep(50);
  }
  throw new Error(`find-window-layout: page did not reach ${expected}: ${raw}`);
}

// A changed page range re-runs the search after find-as-you-type's debounce;
// wait for it to finish so Enter steps the results instead of racing it.
async function waitFindIdle(client: ControlClient, timeoutMs = 30_000): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  let raw = "";
  while (Date.now() < deadline) {
    const response = await client.request(ControlCommand.TestFindUiState, []);
    raw = String(response[1] ?? "").trim();
    if (raw.includes("countValid=1") && raw.includes("busy=0")) {
      return;
    }
    await sleep(50);
  }
  throw new Error(`find-window-layout: search did not finish: ${raw}`);
}

async function toolReq(client: ControlClient, args: (string | number)[]): Promise<string> {
  const res = await client.request(ControlCommand.TestToolWindow, args);
  return String(res[1] ?? "");
}

function fieldRect(layout: string, value: string): { x: number; y: number; w: number } | null {
  const re = new RegExp(`node rect=([0-9.]+),([0-9.]+),([0-9.]+),[0-9.]+ role=\\d+ label='[^']*' value='${value}'`);
  const m = re.exec(layout);
  if (!m) {
    return null;
  }
  return { x: Number(m[1]), y: Number(m[2]), w: Number(m[3]) };
}

function inputHits(layout: string): { x: number; y: number; w: number; h: number }[] {
  const hits: { x: number; y: number; w: number; h: number }[] = [];
  for (const m of layout.matchAll(/hit rect=([0-9.]+),([0-9.]+),([0-9.]+),([0-9.]+) click=\d+ input=1/g)) {
    hits.push({ x: Number(m[1]), y: Number(m[2]), w: Number(m[3]), h: Number(m[4]) });
  }
  return hits;
}

async function findLayout(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 4000;
  let raw = "";
  while (Date.now() < deadline) {
    raw = await toolReq(client, ["layout", "find"]);
    // the "n / 999" status reserves its slot; measuring before it shows is wider
    if (raw.startsWith("OK ") && raw.includes("value='needle'") && raw.includes(` / ${hitCount}' value=`)) {
      return raw;
    }
    await sleep(50);
  }
  throw new Error(`find-window-layout: find window layout not ready:\n${raw}`);
}

async function clickHit(client: ControlClient, hit: { x: number; y: number; w: number; h: number }): Promise<void> {
  await toolReq(client, ["input", "find", "click", Math.round(hit.x + hit.w / 2), Math.round(hit.y + hit.h / 2), 0, 0]);
}

async function keyFind(client: ControlClient, vk: number): Promise<void> {
  await toolReq(client, ["input", "find", "key", vk, 0, 0, 0]);
}

// ng draws the find window in GPUI, so there is no ComboBox hwnd. The search
// field's laid-out width is the same measurement: it must not change when the
// current match gains digits.
async function testNg(client: ControlClient): Promise<void> {
  const initialLayout = await findLayout(client);
  const search = fieldRect(initialLayout, "needle");
  const hits = inputHits(initialLayout);
  const pages = hits.filter((h) => !search || Math.abs(h.w - search.w) > 8).sort((a, b) => a.w - b.w)[0];
  if (!search || !pages) {
    throw new Error(`find-window-layout: find controls not found\n${initialLayout}`);
  }
  const initialWidth = Math.round(search.w);

  await clickHit(client, pages);
  for (const ch of "1-9") {
    await toolReq(client, ["input", "find", "char", ch.charCodeAt(0), 0, 0, 0]);
  }
  await keyFind(client, VK_HOME);
  const afterHome = await toolReq(client, ["state", "find"]);
  const caret = /caret=(\d+)/.exec(afterHome);
  if (!caret || Number(caret[1]) !== 0) {
    throw new Error(`find-window-layout: page-range caret stayed at ${caret?.[1] ?? "none"}, expected 0\n${afterHome}`);
  }
  await waitFindIdle(client);
  await waitForSelection(client, 0);

  const searchHit = hits.find((h) => search && Math.abs(h.w - search.w) <= 8);
  if (!searchHit) {
    throw new Error(`find-window-layout: search field hit not found\n${initialLayout}`);
  }
  await clickHit(client, searchHit);
  await keyFind(client, VK_RETURN);
  await waitForSelection(client, 1);
  await keyFind(client, VK_END);
  await keyFind(client, VK_END);
  await waitForSelection(client, hitCount - 1);
  await waitForPage(client, pageCount);

  const after = fieldRect(await findLayout(client), "needle");
  if (!after) {
    throw new Error("find-window-layout: search field missing after navigation");
  }
  const afterWidth = Math.round(after.w);
  if (afterWidth !== initialWidth) {
    throw new Error(`find-window-layout: combo width changed from ${initialWidth} to ${afterWidth}`);
  }
  console.log(
    `find-window-layout: combo width stayed ${initialWidth}px for 1 / ${hitCount} and ${hitCount} / ${hitCount}`,
  );
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("find-window-layout.pdf");
  writeFileSync(pdf, buildPdf());

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await waitForSelection(client, 0);
    await testNg(client);
    return;
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
