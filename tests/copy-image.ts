// The page context menu's Copy Image puts the image under the click on the
// clipboard (orig's CopyImageToClipboard). ng had no bitmap type for it on
// macOS and copied nothing.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { VK_ESCAPE } from "./winapi.ts";

// a 2x2 image over the page's center
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

async function clipboard(client: ControlClient): Promise<{ changes: number; raw: string }> {
  const raw = String((await client.request(ControlCommand.TestPasteboard, ["general"]))[1] ?? "");
  return { changes: +(/changes=(\d+)/.exec(raw)?.[1] ?? "-1"), raw };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("copy-image");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "image.pdf");
  writeFileSync(pdf, imagePdf(), "latin1");

  const { proc, client } = await launchControlled(["-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const ui = String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
    const m = /canvas=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(ui);
    if (!m) {
      throw new Error("copy-image: no canvas rect");
    }
    const x = +m[1]! + Math.floor(+m[3]! / 2);
    const y = +m[2]! + Math.floor(+m[4]! / 2);

    const before = await clipboard(client);
    // the context menu on the image, then its Copy Image
    await client.request(ControlCommand.TestInput, ["click", x, y, 1, 0]);
    await pollUntil(
      async () => String((await client.request(ControlCommand.TestUiState, []))[1] ?? ""),
      (s) => / popup=1/.test(s),
      { error: () => "copy-image: the context menu didn't open" },
    );
    await client.request(ControlCommand.TestUiState, ["ctxcmd", cmdId("CmdCopyImage")]);
    await client.request(ControlCommand.TestInput, ["key", VK_ESCAPE, 0, 0, 0]);
    const after = await pollUntil(
      () => clipboard(client),
      (c) => c.changes > before.changes && c.raw.includes("public.png"),
      { error: (c) => `copy-image: no image on the clipboard: before ${before.raw}, after ${c.raw}` },
    );
    console.log(`copy-image: OK ${after.raw}`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
