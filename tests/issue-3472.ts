// #3472: with Fit Width, a reflowable ebook page was always taller than the
// window, so reading meant scrolling inside a page and then turning it -- two
// levels of scrolling. A reflow document is laid out once into a fixed-size
// page (A5 by default) and Fit Width only scales that page, so on any window
// whose shape isn't A5 the height overflows. The layout height is now derived
// from the window, so one page is one screen.
//
// GUI automation rather than -dbg-control: the whole point is what the layout
// does with the real window size.

import { ControlCommand } from "./control.ts";
import { launchControlled, killAndWait } from "./win-automation.ts";
import { runStandalone } from "./util.ts";

const EPUB = "tests/issue-5846.epub";

export async function testit(): Promise<void> {
  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit width", EPUB]);
  try {
    await client.waitForRenderIdle();
    // macOS has no scrollbar HWND. TestLayout reports the same scroll range
    // the canvas computed from this window.
    const raw = String((await client.request(ControlCommand.TestLayout, []))[1] ?? "");
    const canvas = /item name=canvas visible=\d+ rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
    const si = /scrollV pos=(-?\d+) min=(-?\d+) max=(-?\d+) page=(-?\d+)/.exec(raw);
    if (!canvas || !si) {
      throw new Error(`issue-3472: layout missing the scroll range: ${raw}`);
    }
    const page = Number(si[4]);
    const max = Number(si[3]);
    const overflow = max + 1 - page;
    const dx = Number(canvas[3]);
    const dy = Number(canvas[4]);
    if (page < 100) {
      throw new Error(`issue-3472: document didn't load (scroll page ${page}, max ${max})`);
    }
    if (overflow > 8) {
      throw new Error(`issue-3472: Fit Width page overflows the window by ${overflow}px (canvas ${dx}x${dy})`);
    }
    console.log(`issue-3472: canvas ${dx}x${dy}, Fit Width page overflow ${overflow}px`);
    return;
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
