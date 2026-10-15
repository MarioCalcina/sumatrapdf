// High-level GUI automation actions for driving SumatraPDF in tests, built on
// winapi.ts. winapi.ts is the raw-message layer; this file is the "do a thing"
// layer (launch the app, find its windows, click a spot, open a context menu).
// Input goes over the -dbg-control socket, so it works with the window in the
// background and without real mouse or keyboard events.
//
// Put reusable helpers here, not in the individual test scripts.

import { macClick } from "./mac-control.ts";
import { cmdId, drainProcStderr, EXE, setFailureContext } from "./util.ts";
import {
  testWindowPos,
  findChildWindow,
  waitForTopWindow,
  sleep,
  sendMessage,
  postMessage,
  WM_KEYDOWN,
  WM_KEYUP,
  WM_COMMAND,
  VK_ESCAPE,
  ensureModifierKeysUp,
  getCursorPos,
  setCursorPos,
  sendCopyDataW,
  getWindowText,
  killAndWait as killAndWaitProcess,
  killProcessesNamed,
} from "./winapi.ts";

export { ensureModifierKeysUp };
import { ControlClient, ControlCommand, uniquePipeName } from "./control.ts";

export { killProcessesNamed };

type SharedControlledSession = {
  proc: Bun.Subprocess;
  pipe: string;
  frame: number;
};

let sharedSessionRequested = false;
let sharedSession: SharedControlledSession | null = null;

// keep drain promises alive: `void new Response().text()` can be GC'd, the
// pipe fills, ASan's next stderr write kills the process, tests see EPIPE
const gStderrDrains = new Set<Promise<string>>();
const gStderrByProc = new WeakMap<Bun.Subprocess, Promise<string>>();

function drainStderr(proc: Bun.Subprocess): Promise<string> {
  const p = drainProcStderr(proc.stderr);
  gStderrDrains.add(p);
  gStderrByProc.set(proc, p);
  void p.finally(() => gStderrDrains.delete(p));
  return p;
}

export async function takeStderr(proc: Bun.Subprocess): Promise<string> {
  const p = gStderrByProc.get(proc);
  return p ? (await p).trim() : "";
}

// the process the current test launched last; its stderr is attached to a
// test failure. stderr only closes when the process exits, so a still-running
// app yields nothing rather than hanging the report.
let gLastProc: Bun.Subprocess | null = null;
const STDERR_GRACE_MS = 1500;
const STDERR_TAIL_CHARS = 4000;

async function lastProcStderrTail(): Promise<string> {
  if (!gLastProc) {
    return "";
  }
  const timeout = new Promise<string>((resolve) => setTimeout(() => resolve(""), STDERR_GRACE_MS));
  const s = await Promise.race([takeStderr(gLastProc), timeout]);
  return s.length > STDERR_TAIL_CHARS ? "..." + s.slice(-STDERR_TAIL_CHARS) : s;
}

setFailureContext(lastProcStderrTail);

export function beginSharedControlledSession(): void {
  // macOS has no WM_COPYDATA open, so each test launches its own process.
}

export async function endSharedControlledSession(): Promise<void> {
  sharedSessionRequested = false;
  if (sharedSession) {
    const session = sharedSession;
    sharedSession = null;
    await killAndWaitProcess(session.proc);
  }
}

export async function killAndWait(proc: Bun.Subprocess): Promise<void> {
  if (sharedSession?.proc === proc) {
    return;
  }
  await killAndWaitProcess(proc);
}

// ng's frame is gpui's window. Tests import this binding, so the assignment
// is visible to them.
export let FRAME_CLASS = "GpuiSystemMonitor";

// -window-pos for the right half of the screen (see testWindowPos)
export function windowPosArgs(): string[] {
  const p = testWindowPos();
  return ["-window-pos", `${p.dx}x${p.dy}@${p.x}x${p.y}`];
}

// Launch with -dbg-control so the test can wait for render-idle (and other
// control commands) instead of sleeping. saveSettings: skip -for-testing when
// the test has to read back the settings file. env: extra environment variables.
export async function launchControlled(
  args: string[],
  opts?: { defaultWindowPos?: boolean; saveSettings?: boolean; env?: Record<string, string> },
): Promise<{ proc: Bun.Subprocess; client: ControlClient; frame: number }> {
  // many tests post keys and clicks directly; a held modifier would chord them
  await ensureModifierKeysUp();
  if (sharedSession) {
    if (opts?.env) {
      throw new Error("a shared controlled session can't take env; register the test after the shared group");
    }
    const path = args[args.length - 1];
    if (!path || path.startsWith("-")) {
      throw new Error("shared controlled session needs a document path as its last argument");
    }
    // Cancel a gesture left by the prior test, or the next click only resets it.
    sendMessage(sharedSession.frame, WM_KEYDOWN, VK_ESCAPE, 0);
    sendMessage(sharedSession.frame, WM_COMMAND, cmdId("CmdDiscardChanges"), 0);
    sendMessage(sharedSession.frame, WM_COMMAND, cmdId("CmdToggleEditPDF"), 0);
    const command = `[Open("${path}", 0, 1, 0)]`;
    if (!sendCopyDataW(sharedSession.frame, 0x44646557, command)) {
      throw new Error("could not open the next document in the shared SumatraPDF process");
    }
    const client = await ControlClient.connect(sharedSession.pipe);
    await client.waitForRenderIdle();
    return { proc: sharedSession.proc, client, frame: sharedSession.frame };
  }
  const pipe = uniquePipeName();
  const posArgs = opts?.defaultWindowPos || args.includes("-window-pos") ? [] : windowPosArgs();
  const testing = opts?.saveSettings ? [] : ["-for-testing"];
  // drain stderr: ASan writes reports there, and with stderr:"ignore" those
  // writes hit a closed pipe and kill the process (EPIPE in the test client)
  const proc = Bun.spawn([EXE, ...testing, ...posArgs, "-dbg-control", pipe, ...args], {
    stdout: "ignore",
    stderr: "pipe",
    env: opts?.env ? { ...process.env, ...opts.env } : undefined,
  });
  drainStderr(proc);
  gLastProc = proc;
  try {
    const client = await ControlClient.connect(pipe);
    // macOS has no HWND. The control socket is the ready signal; 1 stands in
    // for the frame so callers that only check it is non-zero keep going.
    const frame = 1;
    if (!frame) {
      client.close();
      throw new Error("SumatraPDF main window did not appear");
    }
    if (sharedSessionRequested) {
      sharedSession = { proc, pipe, frame };
    }
    return { proc, client, frame };
  } catch (e) {
    await killAndWaitProcess(proc);
    throw e;
  }
}

export function sendCommandSync(hwnd: number, id: number): void {
  sendMessage(hwnd, WM_COMMAND, id, 0);
}

export async function waitForTitle(frame: number, pred: (title: string) => boolean, timeoutMs = 8000): Promise<string> {
  const deadline = Date.now() + timeoutMs;
  let last = "";
  while (Date.now() < deadline) {
    last = getWindowText(frame);
    if (pred(last)) {
      return last;
    }
    await sleep(40);
  }
  throw new Error(`window title did not match in time (last: '${last}')`);
}

// Wait for the app to exit by itself, which is also when it has finished
// writing its settings file. Exact, unlike sleeping "long enough" after
// CmdExit / WM_CLOSE. Returns false on timeout so the caller can report it
export async function waitForExit(proc: Bun.Subprocess, timeoutMs = 10000): Promise<boolean> {
  let exited = false;
  void proc.exited.then(() => {
    exited = true;
  });
  const deadline = Date.now() + timeoutMs;
  while (!exited && Date.now() < deadline) {
    await sleep(25);
  }
  return exited;
}

// the main SUMATRA_PDF_FRAME window of a process (0 on timeout)
export function waitForFrame(pid: number, timeoutMs = 12000): Promise<number> {
  return waitForTopWindow(pid, FRAME_CLASS, timeoutMs);
}

// the document canvas (child of the frame)
export function findCanvas(frame: number): number {
  return frame;
}

export function findChildByClass(parent: number, className: string): number {
  const child = findChildWindow(parent, className);
  // ng draws the toolbar in the frame; there is no SUMATRA_VIRT_TOOLBAR child
  if (!child && className === "SUMATRA_VIRT_TOOLBAR") {
    return parent;
  }
  return child;
}

// Left-click at client (x,y) of hwnd. Sent synchronously, so the click is fully
// handled before returning, then we wait settleMs for any re-render.
//
// The cursor is moved to the click first: SetCapture (from a drag start) injects
// WM_MOUSEMOVE at the real cursor, and if that is far from (x,y) the app treats
// the click as a drag (ClickEdgeToTurnPage and similar then no-op).
export async function clickAt(hwnd: number, x: number, y: number, settleMs = 350, extraMk = 0): Promise<void> {
  macClick(x, y, extraMk);
  await sleep(settleMs);
}

type ScreenRect = { left: number; top: number; right: number; bottom: number };

function rectHasPoint(r: ScreenRect, x: number, y: number): boolean {
  return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

// Moves the real cursor off `avoid` (default: where test windows open), so real
// mouse moves can't hover the app, e.g. open a toolbar drop-down. SetCursorPos
// clamps to the screen, so try each side and keep the one that landed outside.
export function parkCursorAway(avoid?: ScreenRect[]): boolean {
  if (!avoid) {
    const p = testWindowPos();
    avoid = [{ left: p.x, top: p.y, right: p.x + p.dx, bottom: p.y + p.dy }];
  }
  const fr = avoid[0]!;
  const candidates = [
    { x: fr.left - 40, y: fr.top + 100 },
    { x: fr.right + 40, y: fr.top + 100 },
    { x: fr.left + 100, y: fr.bottom + 40 },
    { x: fr.left + 100, y: fr.top - 40 },
  ];
  for (const p of candidates) {
    if (!setCursorPos(p.x, p.y)) {
      continue;
    }
    const c = getCursorPos();
    if (!avoid.some((r) => rectHasPoint(r, c.x, c.y))) {
      return true;
    }
  }
  return false;
}

// Press a key (WM_KEYDOWN). Posted (not sent) so it flows through the app's
// PreTranslateMessage like real key input would (needed for canvas shortcuts /
// arrow keys; also fine for the form editor's Enter/Tab/Esc handling).
// a held Ctrl/Shift/Alt on the machine would turn this into a chord
export async function pressKey(hwnd: number, vk: number, settleMs = 250): Promise<void> {
  await ensureModifierKeysUp();
  postMessage(hwnd, WM_KEYDOWN, vk, 0);
  await sleep(settleMs);
}

export const pressEscape = (hwnd: number) => pressKey(hwnd, VK_ESCAPE, 250);

// Send a WM_COMMAND (menu/command id) to a window (usually the frame).
export function sendCommand(hwnd: number, cmdId: number): void {
  postMessage(hwnd, WM_COMMAND, cmdId, 0);
}

// A posted, unmodified key press that doesn't wait for the app.
export function pressVKey(hwnd: number, vk: number): void {
  postMessage(hwnd, WM_KEYDOWN, vk, 0);
  postMessage(hwnd, WM_KEYUP, vk, 0);
}

async function ngInput(client: ControlClient, args: (string | number)[]): Promise<void> {
  const res = await client.request(ControlCommand.TestInput, args);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0 || !raw.startsWith("OK")) {
    throw new Error(`TestInput ${args.join(" ")} failed: ${raw}`);
  }
}

// ng has no window to post keys to: they go through TestInput.
export function ngKey(client: ControlClient, vk: number): Promise<void> {
  return ngInput(client, ["key", vk, 0]);
}

export async function ngType(client: ControlClient, text: string): Promise<void> {
  for (const ch of text) {
    await ngInput(client, ["char", ch.codePointAt(0)!]);
  }
}

type Pt = { x: number; y: number };

// Pixels in the placement-preview blue within 8px of the start-end box.
export function countPreviewBlue(shot: { w: number; h: number; data: Uint8Array } | null, start: Pt, end: Pt): number {
  if (!shot) {
    return 0;
  }
  const left = Math.max(0, Math.min(start.x, end.x) - 8);
  const right = Math.min(shot.w - 1, Math.max(start.x, end.x) + 8);
  const top = Math.max(0, Math.min(start.y, end.y) - 8);
  const bottom = Math.min(shot.h - 1, Math.max(start.y, end.y) + 8);
  let count = 0;
  for (let y = top; y <= bottom; y++) {
    for (let x = left; x <= right; x++) {
      const off = (y * shot.w + x) * 4;
      const b = shot.data[off]!;
      const g = shot.data[off + 1]!;
      const r = shot.data[off + 2]!;
      if (b > 150 && b > g + 50 && g > r + 30 && r < 80) {
        count++;
      }
    }
  }
  return count;
}
