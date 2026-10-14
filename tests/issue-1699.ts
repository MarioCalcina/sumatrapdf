// Test for issue #1699: "zoom to selection".
//
// With something selected, `Zoom / To Selection` (Ctrl + 4, also in the canvas
// context menu) zooms so the selection fills the window and centres it. Back
// (Alt + Left) returns to the view it was zoomed from, and the selection is
// left alone so it can still be copied.
//
// The test document is four solid colour quadrants with a line of text inside
// the top-left (red) one, so both the input and the result can be read off the
// pixels: after zooming into the selected text the window must be all red,
// with the other three quadrants scrolled out of view.
//
// It selects text by dragging (a rectangular Ctrl + drag selection can't be
// driven from a test: the app reads the real Ctrl key state, and injected
// keyboard state doesn't stick on the test machine).

import { writeFileSync } from "node:fs";
import { ControlCommand, type ControlClient } from "./control.ts";
import { cmdId, tmpPath, assemblePdf } from "./util";
import { launchControlled, killAndWait } from "./win-automation";
import { sendMessage, WM_COMMAND } from "./winapi";

// page coordinates of the text line, so the test can find it on screen
const TEXT_X = 40;
const TEXT_Y = 690;
const TEXT = "ZoomHereZoomHere";
const PAGE_DX = 612;
const PAGE_DY = 792;

// one page: four solid quadrants (red, green, blue, yellow) and a line of text
// in the red one
function makeQuadrantPdf(): string {
  const stream =
    [
      "1 0 0 rg 0 396 306 396 re f", // top-left red
      "0 0.6 0 rg 306 396 306 396 re f", // top-right green
      "0 0 1 rg 0 0 306 396 re f", // bottom-left blue
      "1 1 0 rg 306 0 306 396 re f", // bottom-right yellow
    ].join("\n") + `\nBT /F1 16 Tf 0 0 0 rg ${TEXT_X} ${TEXT_Y} Td (${TEXT}) Tj ET`;
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 1 /Kids [3 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ${PAGE_DX} ${PAGE_DY}] /Contents 4 0 R ` +
      `/Resources << /Font << /F1 5 0 R >> >> >>`,
    `<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`,
    `<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>`,
  ];
  return assemblePdf(objs);
}

type PageBox = { x: number; y: number; dx: number; dy: number; canvasDx: number; canvasDy: number; scrollY: number };

async function zoomReal(client: ControlClient): Promise<number> {
  const raw = String((await client.request(ControlCommand.TestDisplayMode, ["zoom-real"]))[1] ?? "");
  const m = /zoomReal=([0-9.eE+-]+)/.exec(raw);
  if (!m) {
    throw new Error(`issue-1699: no zoom: ${raw}`);
  }
  return Number(m[1]);
}

async function pageBox(client: ControlClient): Promise<PageBox> {
  const raw = String((await client.request(ControlCommand.TestLayout, []))[1] ?? "");
  const canvas = /item name=canvas visible=\d+ rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
  const page = /page n=1 shown=1 pos=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
  const scroll = /scrollV pos=(-?\d+)/.exec(raw);
  if (!canvas || !page || !scroll) {
    throw new Error(`issue-1699: layout missing the page: ${raw}`);
  }
  return {
    x: Number(page[1]),
    y: Number(page[2]),
    dx: Number(page[3]),
    dy: Number(page[4]),
    canvasDx: Number(canvas[3]),
    canvasDy: Number(canvas[4]),
    scrollY: Number(scroll[1]),
  };
}

// No window DC on macOS. The page box is in zoomed canvas pixels, so a zoom
// into the top-left text grows it and keeps the scroll near the top.
async function checkZoomToSelection(client: ControlClient, frame: number): Promise<void> {
  sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdZoomFitPage")), 0n);
  await client.waitForRenderIdle();
  const before = await pageBox(client);
  const z0 = await zoomReal(client);
  if (before.dx < before.canvasDx * 0.4 || before.dy < before.canvasDy * 0.4) {
    throw new Error(`the whole page should be visible at fit page, got ${JSON.stringify(before)} zoom ${z0}`);
  }

  sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdSelectTextViaKeyboard")), 0n);
  for (let i = 0; i < 3; i++) {
    sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdExtendSelectionWordRight")), 0n);
  }
  sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdZoomToSelection")), 0n);
  await client.waitForRenderIdle();

  const after = await pageBox(client);
  const z1 = await zoomReal(client);
  if (z1 < z0 * 2 || after.dx < before.dx * 2 || after.dy < before.dy * 2) {
    throw new Error(
      `Zoom To Selection did not zoom into the selection: ` +
        `${JSON.stringify(after)} zoom ${z1} (before ${JSON.stringify(before)} zoom ${z0})`,
    );
  }
  // the line is in the top-left quadrant, so the scroll stays in the top half
  if (after.scrollY > after.dy * 0.45) {
    throw new Error(`Zoom To Selection scrolled away from the top-left text: ${JSON.stringify(after)}`);
  }

  sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdNavigateBack")), 0n);
  await client.waitForRenderIdle();
  const back = await pageBox(client);
  const z2 = await zoomReal(client);
  if (z2 > z0 * 1.25 || back.dx > before.dx * 1.5) {
    throw new Error(
      `Back did not return to the view before Zoom To Selection: ` +
        `${JSON.stringify(back)} zoom ${z2} (before ${JSON.stringify(before)} zoom ${z0})`,
    );
  }
  console.log(`  zoom to selection: zoom ${z0.toFixed(2)} -> ${z1.toFixed(2)}, Back restored ${z2.toFixed(2)}`);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-1699.pdf");
  writeFileSync(pdf, makeQuadrantPdf(), "latin1");

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await checkZoomToSelection(client, frame);
    return;
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util");
  await runStandalone(testit);
}
