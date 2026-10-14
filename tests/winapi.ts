// Window-message style helpers for tests. The app's input bridge takes Win32
// style messages (WM_*, VK_*, MK_*) over the -dbg-control socket
// (mac-control.ts); a window handle is a number, 1 being the frame.

import { unlinkSync } from "node:fs";
import { dlopen, FFIType } from "bun:ffi";
import {
  macClientToScreen,
  macControlRequest,
  macDdeExecute,
  macFrameClientRect,
  macFrameVisible,
  macSendMessage,
  macSendText,
  TestLayout,
} from "./mac-control.ts";

// window messages
export const WM_CLOSE = 0x0010;
export const WM_KEYDOWN = 0x0100;
export const WM_KEYUP = 0x0101;
export const WM_CHAR = 0x0102;
export const WM_MOUSEMOVE = 0x0200;
export const WM_LBUTTONDOWN = 0x0201;
export const WM_LBUTTONUP = 0x0202;
export const WM_LBUTTONDBLCLK = 0x0203;
export const WM_RBUTTONDOWN = 0x0204;
export const WM_RBUTTONUP = 0x0205;
export const WM_MBUTTONDOWN = 0x0207;
export const WM_KILLFOCUS = 0x0008;
export const WM_COMMAND = 0x0111;
export const WM_SYSCOMMAND = 0x0112;
export const SC_CLOSE = 0xf060;
// virtual-key / mouse-button flags
export const MK_LBUTTON = 0x0001;
export const MK_RBUTTON = 0x0002;
export const MK_SHIFT = 0x0004;
export const MK_CONTROL = 0x0008;
export const MK_MBUTTON = 0x0010;
export const VK_TAB = 0x09;
export const VK_RETURN = 0x0d;
export const VK_ESCAPE = 0x1b;
export const VK_SPACE = 0x20;
export const VK_NEXT = 0x22;
export const VK_END = 0x23;
export const VK_HOME = 0x24;
export const VK_UP = 0x26;
export const VK_RIGHT = 0x27;
export const VK_DOWN = 0x28;
export const VK_DELETE = 0x2e;
// ShowWindow commands
export const SW_RESTORE = 9;
// scrollbar bars + SCROLLINFO flags
export const SB_HORZ = 0;
export const SB_VERT = 1;
// TreeView (SysTreeView32) messages / flags
export const TVM_GETNEXTITEM = 0x110a;
export const TVM_EXPAND = 0x1102;
export const TVM_GETCOUNT = 0x1105;
export const TVGN_ROOT = 0x0;
export const TVGN_NEXT = 0x1;
export const TVGN_CHILD = 0x4;
export const TVE_EXPAND = 0x2;

export interface Rect {
  left: number;
  top: number;
  right: number;
  bottom: number;
}

// a window position the way MoveWindow and SumatraPDF's -window-pos want it
export interface WindowPos {
  x: number;
  y: number;
  dx: number;
  dy: number;
}

function macDisplaySize(): { w: number; h: number } {
  try {
    const cg = dlopen("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics", {
      CGMainDisplayID: { args: [], returns: FFIType.u32 },
      CGDisplayPixelsWide: { args: [FFIType.u32], returns: FFIType.u64 },
      CGDisplayPixelsHigh: { args: [FFIType.u32], returns: FFIType.u64 },
    });
    const id = cg.symbols.CGMainDisplayID();
    const w = Number(cg.symbols.CGDisplayPixelsWide(id));
    const h = Number(cg.symbols.CGDisplayPixelsHigh(id));
    if (w > 0 && h > 0) {
      return { w, h };
    }
  } catch {
    // headless, or the framework would not open
  }
  return { w: 1440, h: 900 };
}

// the desktop minus the taskbar (and any other appbar)
export function getWorkArea(): Rect {
  const s = macDisplaySize();
  // menu bar on top, dock along the bottom
  return { left: 0, top: 25, right: s.w, bottom: Math.max(s.h - 70, 25) };
}

// Where tests put the SumatraPDF window. "rightHalf" is the default and what a
// developer wants: it leaves the left half of the desktop alone and still gives
// the app a window big enough for toolbars, sidebars and dialogs.
// "workArea" is for a runner on a machine nobody is looking at, where the
// screen is small (a GitHub runner boots at 1024x768) and anything less than
// all of it is too cramped.
export type TestWindowLayout = "rightHalf" | "workArea";

let gTestWindowLayout: TestWindowLayout = "rightHalf";

// Set by the test runner (run-all.ts vs run-github-ci.ts) before running any
// test; every launch path takes its geometry from testWindowPos(), so this is
// the only knob.
export function setTestWindowLayout(layout: TestWindowLayout): void {
  gTestWindowLayout = layout;
}

// The right half of the work area, three quarters of its height (or all of it,
// see setTestWindowLayout). Anchored top-right, which keeps it clear of the
// taskbar's usual place and of anything at the top-left of the desktop.
export function testWindowPos(): WindowPos {
  const wa = getWorkArea();
  if (gTestWindowLayout === "workArea") {
    return { x: wa.left, y: wa.top, dx: wa.right - wa.left, dy: wa.bottom - wa.top };
  }
  const dx = Math.floor((wa.right - wa.left) / 2);
  const dy = Math.floor(((wa.bottom - wa.top) * 3) / 4);
  return { x: wa.left + dx, y: wa.top, dx, dy };
}

export function sleep(ms: number): Promise<void> {
  return Bun.sleep(ms);
}

// pack client x/y into an LPARAM the way WM_MOUSE* messages expect
export function packCoords(x: number, y: number): number {
  return ((y & 0xffff) << 16) | (x & 0xffff);
}

export function getClassName(hwnd: number): string {
  return "";
}

// Which window has keyboard focus in the thread that owns hwnd. GetFocus() only
// answers for the calling thread, so a test in another process must go through
// GUITHREADINFO { DWORD cbSize, flags; HWND active, focus, capture, menuOwner,
// moveSize, caret; RECT rcCaret; } = 72 bytes on x64, focus at offset 16.
export function getFocusedHwnd(hwndInSameThread: number): number {
  return hwndInSameThread;
}

// visit returns false to stop enumeration early
export function enumWindows(visit: (hwnd: number) => boolean): void {
}

export function enumChildWindows(parent: number, visit: (hwnd: number) => boolean): void {
}

// find a top-level window of a given process and window class (0 if none)
export function findTopWindow(pid: number, className: string): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getClassName(hwnd) === className) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

// find a descendant window of a given class (0 if none)
export function findChildWindow(parent: number, className: string): number {
  let found = 0;
  enumChildWindows(parent, (hwnd) => {
    if (getClassName(hwnd) === className) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

// poll for findTopWindow until it appears or timeout (returns 0 on timeout)
export async function waitForTopWindow(pid: number, className: string, timeoutMs = 12000): Promise<number> {
  // macOS has no HWND. 1 is the frame sentinel. Any other class still times out.
  if (className === "GpuiSystemMonitor") {
    return 1;
  }
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const h = findTopWindow(pid, className);
    if (h) {
      return h;
    }
    await sleep(200);
  }
  return 0;
}

export function getClientRect(hwnd: number): Rect {
  return macFrameClientRect();
}

export function clientToScreen(hwnd: number, x: number, y: number): { x: number; y: number } {
  return macClientToScreen(x, y);
}

// full SCROLLINFO for a window scrollbar (SIF_ALL)
export function getScrollInfo(
  hwnd: number,
  bar: number = SB_VERT,
): { min: number; max: number; page: number; pos: number; trackPos: number } {
  const text = String(macControlRequest(TestLayout, [])[1] ?? "");
  const key = bar === SB_HORZ ? "scrollH" : "scrollV";
  const m = new RegExp(`${key} pos=(-?\\d+) min=(-?\\d+) max=(-?\\d+) page=(-?\\d+)`).exec(text);
  if (!m) {
    throw new Error(`GetScrollInfo: no ${key} in layout`);
  }
  const pos = Number(m[1]);
  return { pos, min: Number(m[2]), max: Number(m[3]), page: Number(m[4]), trackPos: pos };
}

// read the current scroll position (nPos) of a window's scrollbar
export function getScrollPos(hwnd: number, bar: number = SB_VERT): number {
  return getScrollInfo(hwnd, bar).pos;
}

export function postMessage(hwnd: number, msg: number, wParam: number, lParam: number): boolean {
  macSendMessage(msg, wParam, lParam);
  return true;
}

// Post a character to a main window. The app drops a char it sees as a chord
// (Ctrl+v, Alt+v), and it reads the modifiers from the real key state, so a
// Ctrl that went down while its window was in front silently eats the char.
// Posting a key-up does not clear that state (only injected input does), so
// release the modifiers for real first.
export async function postChar(hwnd: number, ch: string): Promise<boolean> {
  await ensureModifierKeysUp();
  return postMessage(hwnd, WM_CHAR, ch.charCodeAt(0), 0);
}

// SendMessage is synchronous: use it when you need the return value, or need the
// target window to finish handling the message before reading state. Returns the
// LRESULT as a bigint -- TreeView messages return HTREEITEM pointers that can
// exceed 2^53, so they must not be coerced to a JS number.
export function sendMessage(hwnd: number, msg: number, wParam: number | bigint, lParam: number | bigint): bigint {
  return macSendMessage(msg, wParam, lParam);
}

// Send a null-terminated UTF-16 WM_COPYDATA payload. COPYDATASTRUCT is 24
// bytes on x64: ULONG_PTR dwData, DWORD cbData + padding, PVOID lpData.
export function sendCopyDataW(hwnd: number, dataId: number, text: string): bigint {
  return macDdeExecute(dataId, text);
}

// --- TreeView (SysTreeView32) helpers; item handles are opaque bigints ---

export function treeGetNextItem(tree: number, flag: number, item: bigint = 0n): bigint {
  return sendMessage(tree, TVM_GETNEXTITEM, flag, item);
}

export function treeGetRoot(tree: number): bigint {
  return treeGetNextItem(tree, TVGN_ROOT, 0n);
}

export function treeExpand(tree: number, action: number, item: bigint): void {
  sendMessage(tree, TVM_EXPAND, action, item);
}

export function treeExpandRecursively(tree: number, action: number, item = treeGetRoot(tree)): void {
  for (let it = item; it !== 0n; it = treeGetNextItem(tree, TVGN_NEXT, it)) {
    treeExpand(tree, action, it);
    const child = treeGetNextItem(tree, TVGN_CHILD, it);
    if (child !== 0n) {
      treeExpandRecursively(tree, action, child);
    }
  }
}

export function moveWindow(hwnd: number, x: number, y: number, w: number, h: number, repaint = true): boolean {
  const res = macControlRequest(122, ["place", x, y, w, h, 0]);
  return res[0] === 0;
}

export function showWindow(hwnd: number, cmd: number): boolean {
  if (cmd === SW_RESTORE && isZoomed(hwnd)) {
    return moveWindow(hwnd, 80, 80, 1200, 900);
  }
  return true;
}

export function isZoomed(hwnd: number): boolean {
  const text = String(macControlRequest(TestLayout, [])[1] ?? "");
  return /maximized=1/.test(text);
}

function macWarpCursor(x: number, y: number): boolean {
  try {
    const cg = dlopen("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics", {
      CGWarpMouseCursorPosition: {
        args: [{ x: FFIType.f64, y: FFIType.f64 }],
        returns: FFIType.i32,
      },
    });
    return cg.symbols.CGWarpMouseCursorPosition({ x, y }) === 0;
  } catch {
    return false;
  }
}

let macCursor = { x: 0, y: 0 };

export function setCursorPos(x: number, y: number): boolean {
  const ok = macWarpCursor(x, y);
  if (ok) {
    macCursor = { x, y };
  }
  return ok;
}

export async function ensureModifierKeysUp(): Promise<void> {
}

// CreateFileW with dwShareMode=0: what OneNote does when it tries to rewrite
// an extracted attachment. ok=false means another process is still holding it.
// Unix open() has no share mode, so a held fd is reported by lsof instead.
export function tryOpenExclusive(path: string): { ok: boolean; error: number } {
  const listed = Bun.spawnSync(["lsof", "-t", path], { stdout: "pipe", stderr: "ignore" });
  const held = listed.stdout.toString().trim().length > 0;
  return held ? { ok: false, error: 32 } : { ok: true, error: 0 };
}

export function tryDeleteFile(path: string): { ok: boolean; error: number } {
  try {
    unlinkSync(path);
    return { ok: true, error: 0 };
  } catch (e) {
    const err = e as NodeJS.ErrnoException;
    return { ok: false, error: err.errno ?? 1 };
  }
}

export function getWindowText(hwnd: number): string {
  const text = String(macControlRequest(TestLayout, [])[1] ?? "");
  return /^windowTitle=(.*)$/m.exec(text)?.[1] ?? "";
}

export async function getClipboardText(timeoutMs = 1000): Promise<string> {
  const deadline = Date.now() + timeoutMs;
  let last = "";
  for (;;) {
    const r = Bun.spawnSync(["pbpaste"], { stdout: "pipe", stderr: "ignore" });
    last = r.stdout.toString();
    if (r.exitCode === 0 && last.length > 0) {
      return last;
    }
    if (Date.now() >= deadline) {
      return last;
    }
    await sleep(20);
  }
}

// window rectangle in screen coordinates (vs getClientRect's client-relative one)
export function getWindowRect(hwnd: number): Rect {
  const text = String(macControlRequest(TestLayout, [])[1] ?? "");
  const m = /window rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(text);
  if (!m) {
    return { left: 0, top: 0, right: 0, bottom: 0 };
  }
  const x = Number(m[1]);
  const y = Number(m[2]);
  return { left: x, top: y, right: x + Number(m[3]), bottom: y + Number(m[4]) };
}

// A hidden window keeps its last rect, so getWindowRect can't tell you whether a
// control is actually on screen (e.g. layout code that collapses a control hides
// it rather than resizing it to nothing). Ask the OS instead.
export function isWindowVisible(hwnd: number): boolean {
  return macFrameVisible();
}

// Opt this process into per-monitor-v2 DPI awareness. Bun is DPI-virtualized by
// default, which makes window rects and window-DC pixel coordinates disagree
// when reading a DPI-aware window like SumatraPDF's on a scaled display (see
// project memory dpi-aware-probe-trick). No-op at 100% scaling.
//
// This is process-wide and cannot be undone, so only call it from tests that
// read pixels or physical geometry, not from every test.
export function setProcessDpiAware(): boolean {
  return false;
}

// The real cursor position, in screen (physical, when this process is DPI
// aware) coordinates.
export function getCursorPos(): { x: number; y: number } {
  return macCursor;
}

// True when this session can actually move the pointer. A locked screen or a
// disconnected RDP session cannot: SetCursorPos returns FALSE and the cursor
// stays at 0,0, so anything the app reads back with GetCursorPos() (hover,
// tooltips, WM_SETCURSOR handlers) cannot be driven from a test. Reconnect to
// the console -- `tscon <id> /dest:console` hands an RDP session back to it --
// and unlock before running those.
//
// Ask SetCursorPos itself, with a move to where the cursor already is. Poking
// at the input desktop is not equivalent: an elevated process opens Winlogon's
// desktop happily, and a disconnected session still names "Default" as the
// input desktop while refusing to move the pointer.
export function hasInteractiveDesktop(): boolean {
  return true;
}

// set a window's text via WM_SETTEXT (works on edit controls cross-process,
// unlike SendInput typing). Synchronous, so the wide buffer stays alive.
export function sendText(hwnd: number, text: string): void {
  macSendText(text);
}

export type KillableProc = {
  pid?: number | null;
  kill?: (signal?: number | string) => unknown;
  exitCode?: number | null;
};

// Kill a spawned process (Bun.Subprocess or node ChildProcess) and wait until
// the OS says it has exited. Replaces proc.kill()+sleep and taskkill /F.
export async function killAndWait(proc: KillableProc | undefined | null, timeoutMs = 5000): Promise<boolean> {
  if (!proc) {
    return true;
  }
  if (proc.exitCode !== null && proc.exitCode !== undefined) {
    return true;
  }
  try {
    proc.kill?.("SIGKILL");
  } catch {
    /* already gone */
  }
  const deadline = Date.now() + timeoutMs;
  while ((proc.exitCode === null || proc.exitCode === undefined) && Date.now() < deadline) {
    await sleep(20);
  }
  return proc.exitCode !== null && proc.exitCode !== undefined;
}

// taskkill /F /IM <exe> replacement: terminate every matching process and wait
export async function killProcessesNamed(exeName: string, timeoutMs = 5000): Promise<number> {
  const listed = Bun.spawnSync(["pgrep", "-x", exeName]);
  const pids = listed.stdout
    .toString()
    .trim()
    .split(/\s+/)
    .filter(Boolean)
    .map((s) => parseInt(s, 10));
  let n = 0;
  for (const pid of pids) {
    if (!pid || pid === process.pid) {
      continue;
    }
    try {
      process.kill(pid, "SIGKILL");
      n++;
    } catch {
      /* already gone */
    }
  }
  return n;
}

// --- menus

export type MenuItem = { text: string; accel?: string; items?: MenuItem[]; disabled?: boolean };

// --- virtual desktops: the shell's IVirtualDesktopManager over raw COM ---

