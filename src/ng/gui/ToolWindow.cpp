/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "gui/GpuiBridge.h"
#include "base/UITask.h"

#include "gui/UIModels.h"
#include "VirtKeys.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "gui/Dpi.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "SumatraDialogs.h"
#include "gui/ToolWindow.h"
#include "gui/ToolWindowPlat.h"

#include "SumatraLog.h"

struct ToolRootView {
    ToolWindow* tw = nullptr;

    static gp::El* Render(ToolRootView* self, gp::Ctx* cx);
    static void OnKeyDown(ToolRootView* self, gp::Ctx* cx, const gp::KeyEvent* ev);
    static void OnKeyUp(ToolRootView* self, gp::Ctx* cx, const gp::KeyEvent* ev);
    static void OnCaptureKey(ToolRootView* self, gp::Ctx* cx, const gp::KeyEvent* ev);
    static void OnTick(ToolRootView* self, gp::Ctx* cx, const gp::TickEvent* ev);
    static void OnNoKey(ToolRootView*, gp::Ctx*, const gp::KeyEvent*) {}
};

struct ToolWindow {
    ToolWindowDesc desc;
    MainWindow* owner = nullptr;
    gp::Window* gw = nullptr;
    gp::Entity<ToolRootView> view;
    // screen pixels; where it is, or where it will be made
    Rect outer;
    // ToolWindowClose() was called: onClosed is not wanted
    bool closing = false;
    // ToolWindowSetVisible(false)
    bool hidden = false;
    // the user is dragging or sizing it; onExitSizeMove runs when that ends
    bool userSizing = false;
    // a no-activate window gave the key back to its owner after it was shown
    bool focusedBack = false;
    // the native frame has matched the rect we set; later deltas are the user
    bool frameSettled = false;
    int settleFrames = 0;
    Str title; // owned; what the caption shows
};

// open or about to be made; a closed one leaves at once and is deleted from
// the ui task queue
static Vec<ToolWindow*> gToolWindows;
// TestToolWindow off: draw in the frame too, to test the overlay fallback
static bool gToolWindowsOff = false;

bool ToolWindowsAvailable() {
    return !gToolWindowsOff;
}

static bool IsLive(ToolWindow* tw) {
    return tw && VecContains(gToolWindows, tw);
}

ToolWindow* ToolWindowFind(Str name) {
    for (ToolWindow* tw : gToolWindows) {
        if (!tw->closing && str::Eq(Str(tw->desc.name), name)) {
            return tw;
        }
    }
    return nullptr;
}

bool ToolWindowIsLive(ToolWindow* tw) {
    return IsLive(tw) && !tw->closing;
}

ToolWindow* ToolWindowActiveOf(MainWindow* win) {
    for (ToolWindow* tw : gToolWindows) {
        if (tw->owner == win && !tw->closing && ToolWindowIsActive(tw)) {
            return tw;
        }
    }
    return nullptr;
}

ToolWindow* ToolWindowModalOf(MainWindow* win) {
    for (int i = len(gToolWindows) - 1; i >= 0; i--) {
        ToolWindow* tw = gToolWindows[i];
        if (tw->owner == win && !tw->closing && tw->gw && tw->desc.modal == ToolWinModal::Yes &&
            ToolWindowIsVisible(tw)) {
            return tw;
        }
    }
    return nullptr;
}

ToolWindowDesc ToolWindowModalDesc(const char* name, Str (*title)()) {
    ToolWindowDesc desc;
    desc.name = name;
    desc.title = title;
    desc.frame = ToolWinFrame::Caption;
    desc.resize = ToolWinResize::Fixed;
    desc.owner = ToolWinOwner::Owned;
    desc.modal = ToolWinModal::Yes;
    return desc;
}

ToolWindowDesc ToolWindowDockedDesc(const char* name) {
    ToolWindowDesc desc;
    desc.name = name;
    desc.frame = ToolWinFrame::None;
    desc.resize = ToolWinResize::Fixed;
    desc.owner = ToolWinOwner::Owned;
    desc.style = ToolWinStyle::Tool;
    desc.activate = ToolWinActivate::No;
    return desc;
}

bool ToolWindowDialogKey(gp::Ctx* cx, const gp::KeyEvent* ev, void (*close)()) {
    if (ev->vk == VK_ESCAPE) {
        close();
        return true;
    }
    if (ev->vk != VK_RETURN || CtrlOrCmd(ev) || ev->alt) {
        return false;
    }
    bool editFocused = cx->win->input && cx->win->input->focused;
    if (editFocused && gp::InputIsMultiLine(cx->win->input)) {
        return false;
    }
    return DlgDefaultOnEnter(cx, editFocused);
}

MainWindow* ToolWindowOwner(ToolWindow* tw) {
    return IsLive(tw) ? tw->owner : nullptr;
}

gp::Window* ToolWindowGpui(ToolWindow* tw) {
    return IsLive(tw) ? tw->gw : nullptr;
}

void ToolWindowInvalidate(ToolWindow* tw) {
    if (IsLive(tw) && tw->gw) {
        gp::AppInvalidate(tw->gw);
    }
}

void ToolWindowsInvalidateFor(MainWindow* win) {
    for (ToolWindow* tw : gToolWindows) {
        if (tw->owner == win && tw->gw) {
            gp::AppInvalidate(tw->gw);
        }
    }
}

// the record of a window that is gone

static void FinishClosed(ToolWindow* tw) {
    if (!tw->closing && tw->desc.onClosed) {
        tw->desc.onClosed(IsMainWindowValid(tw->owner) ? tw->owner : nullptr);
    }
    if (tw->view.IsValid()) {
        gp::EntityDrop(AppShellGetApp(), tw->view.id);
    }
    str::Free(tw->title);
    delete tw;
}

static void Forget(ToolWindow* tw) {
    VecRemove(gToolWindows, tw);
    tw->gw = nullptr;
    uitask::Post(MkFunc0(FinishClosed, tw), "ToolWindowFinishClosed");
}

static bool TitledFrame(const ToolWindowDesc& desc) {
    return desc.frame != ToolWinFrame::None;
}

static void EndModalNative(ToolWindow* tw) {
    if (!tw || tw->desc.modal != ToolWinModal::Yes) {
        return;
    }
    gp::Window* owner = tw->owner ? tw->owner->gpuiWin : nullptr;
    ToolWinNativeSetModal(tw->gw, owner, false);
}

static bool NearPx(int a, int b) {
    return a - b < 4 && b - a < 4;
}

static bool FrameNear(Rect a, Rect b) {
    return NearPx(a.x, b.x) && NearPx(a.y, b.y) && NearPx(a.dx, b.dx) && NearPx(a.dy, b.dy);
}

static void PlaceFrame(ToolWindow* tw, Rect outer, bool titled) {
    tw->outer = outer;
    tw->frameSettled = false;
    tw->settleFrames = 0;
    ToolWinNativeSetFrame(tw->gw, outer, titled);
}

static void NoteFrame(ToolWindow* tw) {
    if (!tw->gw) {
        return;
    }
    Rect now = ToolWinNativeFrame(tw->gw);
    if (now.IsEmpty()) {
        return;
    }
    bool moved = !FrameNear(now, tw->outer);
    // a window manager can land a few pixels off the rect we asked for; that
    // is not the user dragging, and must not overwrite a saved position
    bool reportMove = false;
    if (!tw->frameSettled) {
        if (!moved) {
            tw->frameSettled = true;
        } else if (ToolWinNativeMouseDown() || ++tw->settleFrames > 8) {
            tw->outer = now;
            tw->frameSettled = true;
        }
    } else if (moved) {
        reportMove = true;
    }
    if (reportMove) {
        tw->outer = now;
        tw->userSizing = true;
        if (tw->desc.onMoved && IsMainWindowValid(tw->owner)) {
            tw->desc.onMoved(tw->owner, now);
        }
    }
    if (tw->userSizing && !ToolWinNativeMouseDown()) {
        tw->userSizing = false;
        if (tw->desc.onExitSizeMove && IsMainWindowValid(tw->owner)) {
            tw->desc.onExitSizeMove(tw->owner, tw->outer);
        }
    }
    // gpui shows a new window as the key window; a bar must not take the keys
    if (tw->desc.activate == ToolWinActivate::No && !tw->focusedBack && ToolWinNativeIsActive(tw->gw) && tw->owner &&
        tw->owner->gpuiWin) {
        tw->focusedBack = true;
        ToolWinNativeActivate(tw->owner->gpuiWin);
    }
}

static bool ToolShouldClose(void* data, gp::Window*) {
    auto* tw = (ToolWindow*)data;
    if (!IsLive(tw)) {
        return true;
    }
    if (!tw->closing) {
        EndModalNative(tw);
        Forget(tw);
    }
    return true;
}

static void ApplyNative(ToolWindow* tw) {
    bool titled = TitledFrame(tw->desc);
    bool utility = tw->desc.style == ToolWinStyle::Tool;
    bool resizable = tw->desc.resize == ToolWinResize::Resizable;
    bool wantsKey = !titled && tw->desc.activate == ToolWinActivate::Yes;
    ToolWinNativeApplyStyle(tw->gw, titled, resizable, utility, !titled, wantsKey);
    PlaceFrame(tw, tw->outer, titled);
    gp::Window* owner = tw->owner ? tw->owner->gpuiWin : nullptr;
    ToolWinNativeSetOwner(tw->gw, owner, tw->desc.owner == ToolWinOwner::Owned);
    if (!tw->desc.minClient.IsEmpty()) {
        ToolWinNativeSetMinClient(tw->gw, tw->desc.minClient.dx, tw->desc.minClient.dy);
    }
    if (tw->desc.modal == ToolWinModal::Yes && !tw->hidden && owner) {
        ToolWinNativeSetModal(tw->gw, owner, true);
    }
    if (tw->hidden) {
        ToolWinNativeShow(tw->gw, false, false);
    }
}

Size ToolWindowOuterSize(const ToolWindowDesc& desc, MainWindow*, Size clientDip) {
    bool titled = TitledFrame(desc);
    Size chrome =
        ToolWinNativeChrome(titled, desc.resize == ToolWinResize::Resizable, desc.style == ToolWinStyle::Tool);
    return Size(clientDip.dx + chrome.dx, clientDip.dy + chrome.dy);
}

Rect ToolWindowCenteredOuter(MainWindow* owner, Size sz) {
    Rect fr = AppShellWindowScreenRect(owner);
    Rect r{fr.x + (fr.dx - sz.dx) / 2, fr.y + (fr.dy - sz.dy) / 2, sz.dx, sz.dy};
    return AppShellShiftToWorkArea(r, owner, true);
}

Rect ToolWindowCenteredRect(const ToolWindowDesc& desc, MainWindow* owner, Size clientDip) {
    return ToolWindowCenteredOuter(owner, ToolWindowOuterSize(desc, owner, clientDip));
}

Rect ToolWindowRect(ToolWindow* tw) {
    if (!IsLive(tw)) {
        return {};
    }
    Rect r = tw->gw ? ToolWinNativeFrame(tw->gw) : Rect{};
    return r.IsEmpty() ? tw->outer : r;
}

void ToolWindowMove(ToolWindow* tw, Rect outer) {
    if (!IsLive(tw) || outer.IsEmpty()) {
        return;
    }
    if (tw->gw) {
        PlaceFrame(tw, outer, TitledFrame(tw->desc));
    } else {
        tw->outer = outer;
    }
}

void ToolWindowActivate(ToolWindow* tw) {
    if (IsLive(tw) && tw->gw) {
        ToolWinNativeActivate(tw->gw);
    }
}

bool ToolWindowIsActive(ToolWindow* tw) {
    return IsLive(tw) && tw->gw && ToolWinNativeIsActive(tw->gw);
}

void ToolWindowDragMove(ToolWindow*) {}

bool ToolWindowIsVisible(ToolWindow* tw) {
    return IsLive(tw) && !tw->hidden;
}

void ToolWindowSetVisible(ToolWindow* tw, bool visible) {
    if (!IsLive(tw) || tw->closing || tw->hidden == !visible) {
        return;
    }
    tw->hidden = !visible;
    if (!tw->gw) {
        return;
    }
    if (!visible) {
        EndModalNative(tw);
        ToolWinNativeShow(tw->gw, false, false);
        return;
    }
    gp::Window* owner = tw->owner ? tw->owner->gpuiWin : nullptr;
    if (tw->desc.modal == ToolWinModal::Yes && owner) {
        ToolWinNativeSetModal(tw->gw, owner, true);
    }
    ToolWinNativeShow(tw->gw, true, tw->desc.activate == ToolWinActivate::Yes);
    ToolWindowInvalidate(tw);
}

static void FollowOne(ToolWindow* tw) {
    if (!tw->desc.place || tw->closing || !tw->gw || !IsMainWindowValid(tw->owner)) {
        return;
    }
    Rect r = tw->hidden ? Rect{} : tw->desc.place(tw->owner, tw);
    if (r.IsEmpty()) {
        ToolWinNativeShow(tw->gw, false, false);
        return;
    }
    PlaceFrame(tw, r, TitledFrame(tw->desc));
    ToolWinNativeShow(tw->gw, true, false);
}

void ToolWindowsFollow(MainWindow* win) {
    Vec<ToolWindow*> wins = gToolWindows;
    for (ToolWindow* tw : wins) {
        if (tw->owner == win && IsLive(tw)) {
            FollowOne(tw);
        }
    }
}

Rect ToolWindowDockedBarRect(MainWindow* owner, int marginDip, float barDyDip) {
    Rect canvas = AppShellCanvasScreenRect(owner);
    if (canvas.IsEmpty()) {
        return {};
    }
    int dpi = std::max(AppShellWindowDpi(owner), 96);
    int margin = MulDiv(marginDip, dpi, 96);
    int barDx = std::max(canvas.dx - (2 * margin), 0);
    int barDy = (int)(barDyDip * (float)dpi / 96.f + 0.5f);
    if (barDx <= 0 || barDy <= 0) {
        return {};
    }
    int x = canvas.x + margin;
    int y = canvas.y + canvas.dy - barDy - margin;
    if (y < canvas.y + margin) {
        y = canvas.y + margin;
    }
    return Rect{x, y, barDx, barDy};
}

Point ToolWindowOffsetInOwner(ToolWindow* tw) {
    if (!IsLive(tw) || !tw->gw) {
        return {};
    }
    Rect owner = AppShellWindowScreenRect(tw->owner);
    Rect mine = ToolWindowRect(tw);
    int dpi = std::max(AppShellWindowDpi(tw->owner), 96);
    return Point{MulDiv(mine.x - owner.x, 96, dpi), MulDiv(mine.y - owner.y, 96, dpi)};
}

static void CreateNow(ToolWindow* tw) {
    if (!IsLive(tw) || tw->gw) {
        return;
    }
    if (!IsMainWindowValidAndNotClosing(tw->owner)) {
        Forget(tw);
        return;
    }
    gp::App* app = AppShellGetApp();
    tw->view = gp::EntityNew<ToolRootView>(app);
    auto* view = (ToolRootView*)gp::EntityGet(app, tw->view.id);
    view->tw = tw;
    str::ReplaceWithCopy(&tw->title, tw->desc.title ? tw->desc.title() : Str{});

    bool titled = TitledFrame(tw->desc);
    Size chrome =
        ToolWinNativeChrome(titled, tw->desc.resize == ToolWinResize::Resizable, tw->desc.style == ToolWinStyle::Tool);
    int dipW = std::max(tw->outer.dx - chrome.dx, 1);
    int dipH = std::max(tw->outer.dy - chrome.dy, 1);
    gp::WinOpts opts;
    opts.borderless = !titled;
    gp::Window* gw = gp::WindowOpenView(app, ToGpui(tw->title), dipW, dipH, tw->view.id, opts);
    if (!gw) {
        logf("ToolWindow: could not create '%s'\n", Str(tw->desc.name));
        if (IsLive(tw)) {
            Forget(tw);
        }
        return;
    }
    tw->gw = gw;
    gp::WindowOnShouldClose(gw, ToolShouldClose, tw);
    ApplyNative(tw);
    if (tw->desc.onTick) {
        gp::WindowSetInterval(gw, tw->desc.tickMs, gp::ListenTo(tw->view, &ToolRootView::OnTick));
    }
    logf("ToolWindow: '%s' at %d,%d %dx%d\n", Str(tw->desc.name), tw->outer.x, tw->outer.y, tw->outer.dx, tw->outer.dy);
    gp::AppInvalidate(gw);
}

static void DestroyNow(ToolWindow* tw) {
    if (!IsLive(tw)) {
        return;
    }
    EndModalNative(tw);
    if (tw->gw) {
        gp::Window* gw = tw->gw;
        gp::AppQuit(gw);
        if (IsLive(tw)) {
            Forget(tw);
        }
        return;
    }
    Forget(tw);
}

static void SyncTitle(ToolWindow* tw) {
    Str title = tw->desc.title ? tw->desc.title() : Str{};
    if (str::Eq(title, tw->title)) {
        return;
    }
    str::ReplaceWithCopy(&tw->title, title);
    if (tw->gw) {
        gp::AppSetTitle(tw->gw, ToGpui(tw->title));
    }
}

// gpui's size for the text of an input or a button
constexpr float kGpuiUiFontPx = 14;

float ToolWindowSetUiFontPx(gp::Ctx* cx, float px) {
    float scale = kGpuiUiFontPx / px;
    gp::WindowSetRemSize(cx->win, 16.f / scale);
    return scale;
}

void ToolWindowSetMinClient(ToolWindow* tw, Size minClientDip) {
    if (IsLive(tw)) {
        tw->desc.minClient = minClientDip;
    }
}

void ToolWindowSetClientSize(ToolWindow* tw, Size clientDip) {
    if (!IsLive(tw) || !IsMainWindowValid(tw->owner)) {
        return;
    }
    Size sz = ToolWindowOuterSize(tw->desc, tw->owner, clientDip);
    Rect r = ToolWindowRect(tw);
    ToolWindowMove(tw, Rect(r.x, r.y, sz.dx, sz.dy));
}

ToolWindow* ToolWindowOpen(const ToolWindowDesc& desc, MainWindow* owner, Rect outer) {
    if (!ToolWindowsAvailable() || !desc.build || !IsMainWindowValidAndNotClosing(owner)) {
        return nullptr;
    }
    auto* tw = new ToolWindow();
    tw->desc = desc;
    tw->owner = owner;
    tw->outer = outer;
    VecAppend(gToolWindows, tw);
    uitask::Post(MkFunc0(CreateNow, tw), "ToolWindowCreate");
    return tw;
}

void ToolWindowClose(ToolWindow* tw) {
    if (!IsLive(tw) || tw->closing) {
        return;
    }
    tw->closing = true;
    uitask::Post(MkFunc0(DestroyNow, tw), "ToolWindowDestroy");
}

void ToolWindowSetOwner(ToolWindow* tw, MainWindow* owner) {
    if (!IsLive(tw) || tw->owner == owner) {
        return;
    }
    tw->owner = owner;
    if (tw->gw) {
        ToolWinNativeSetOwner(tw->gw, owner ? owner->gpuiWin : nullptr, tw->desc.owner == ToolWinOwner::Owned);
    }
    ToolWindowInvalidate(tw);
}

static MainWindow* OtherMainWindow(MainWindow* win) {
    for (MainWindow* w : gWindows) {
        if (w != win && IsMainWindowValidAndNotClosing(w)) {
            return w;
        }
    }
    return nullptr;
}

// the main window is going: its tool windows go first, and say so. One that
// outlives its owner (onOwnerClosed) moves to another main window instead
void ToolWindowsCloseFor(MainWindow* win) {
    Vec<ToolWindow*> wins = gToolWindows;
    for (ToolWindow* tw : wins) {
        if (tw->owner != win) {
            continue;
        }
        MainWindow* other = tw->desc.onOwnerClosed && !tw->closing ? OtherMainWindow(win) : nullptr;
        if (!other) {
            DestroyNow(tw);
            continue;
        }
        ToolWindowSetOwner(tw, other);
        tw->desc.onOwnerClosed(other);
    }
}

gp::El* ToolRootView::Render(ToolRootView* self, gp::Ctx* cx) {
    // as the shell does: a tool window is one more frame of the same thread
    ResetTempArena();
    DlgAccelBeginFrame(cx->win);
    ToolWindow* tw = self->tw;
    gp::El* root = gp::Div(cx->a)->FlexCol()->SizeFull()->Bg(ToGpui(ThemeWindowControlBackgroundColor()));
    if (!IsLive(tw) || tw->closing || !IsMainWindowValid(tw->owner)) {
        return root;
    }
    SyncTitle(tw);
    NoteFrame(tw);
    root->OnKeyDown(gp::Listen(cx, &ToolRootView::OnKeyDown));
    root->CaptureKeyDown(gp::Listen(cx, &ToolRootView::OnCaptureKey));
    if (tw->desc.onKeyUp) {
        root->OnKeyUp(gp::Listen(cx, &ToolRootView::OnKeyUp));
    }
    if (gp::El* el = tw->desc.build(tw->owner, cx)) {
        root->Child(el);
    }
    // ng: with nothing focused gpui calls a key listener only when a listener
    // is registered below it (DispatchKeyChain wants the anchor inside the
    // listener's subtree), so content without key listeners of its own (the
    // manual: a button row and a webview) would leave the root deaf
    root->Child(gp::Div(cx->a)->Absolute()->W(0)->H(0)->OnKeyDown(gp::Listen(cx, &ToolRootView::OnNoKey)));
    // the dropped list of an editable combo box in this window, over the rest
    if (gp::El* comboList = DialogComboListBuild(cx)) {
        root->Child(comboList);
    }
    // a message box raised while this window was the active one
    if (gp::El* msgBox = MsgBoxBuildInToolWindow(tw, cx)) {
        root->Child(msgBox);
    }
    return root;
}

void ToolRootView::OnKeyDown(ToolRootView* self, gp::Ctx* cx, const gp::KeyEvent* ev) {
    ToolWindow* tw = self->tw;
    if (!IsLive(tw) || tw->closing || !IsMainWindowValidAndNotClosing(tw->owner)) {
        return;
    }
    // as in the frame: Esc closes an open popup or dropped list, nothing else
    bool handled = ev->vk == VK_ESCAPE && DismissTrackedPopup(cx);
    if (!handled && tw->desc.onKey) {
        handled = tw->desc.onKey(tw->owner, cx, ev);
    }
    if (!handled) {
        // orig's IsDialogMessage: Alt + letter, and the bare letter while no
        // text field has the focus
        bool editFocused = cx->win->input && cx->win->input->focused;
        handled = DlgAccelOnKey(cx, ev->vk, ev->alt, CtrlOrCmd(ev), editFocused);
    }
    if (!handled) {
        return;
    }
    const_cast<gp::KeyEvent*>(ev)->propagate = false;
    gp::Notify(cx);
}

// TestLayout for a tool window: what its last frame drew, in dips of its
// client area. gpui keeps the selectable texts, the accessibility nodes (a
// button with its label, a field with its value), the click targets and the
// scrolled boxes of a frame; those are what a test needs to find a row or a
// button
static TempStr GpuiLayoutTemp(gp::Window* gw, Str name, Rect r) {
    gp::WinSize ws = gp::WindowSize(gw);
    str::Builder out;
    out.Append(fmt("OK name=%s rect=%d,%d,%d,%d client=%d,%d rem=%.2f\n", name, r.x, r.y, r.dx, r.dy, (int)ws.dipW,
                   (int)ws.dipH, gp::WindowRemSize(gw)));
    const gp::PaintCtx& paint = gw->paint;
    for (const gp::TextHit& t : paint.texts) {
        const gp::Bounds& b = t.bounds;
        out.Append(fmt("text rect=%.0f,%.0f,%.0f,%.0f font=%.1f '%s'\n", b.x, b.y, b.w, b.h, t.font,
                       Str{(char*)t.text.s, (int)t.text.len}));
    }
    for (const gp::AccessibilityNode& n : gw->accessibility) {
        const gp::AccessibilityInfo& info = n.info;
        if (info.role == gp::AccessibilityRole::None && info.label.len == 0 && info.value.len == 0) {
            continue;
        }
        const gp::Bounds& b = n.bounds;
        out.Append(fmt("node rect=%.0f,%.0f,%.0f,%.0f role=%d label='%s' value='%s'", b.x, b.y, b.w, b.h,
                       (int)info.role, Str{(char*)info.label.s, (int)info.label.len},
                       Str{(char*)info.value.s, (int)info.value.len}));
        if (info.disabled) {
            out.Append(StrL(" disabled=1"));
        }
        if (info.toggled == gp::AccessibilityToggled::True) {
            out.Append(StrL(" checked=1"));
        }
        out.Append(StrL("\n"));
    }
    for (const gp::HitRect& h : paint.hits) {
        bool click = h.onClick.IsValid() || h.listener.IsValid() || h.clickAction != 0;
        if (!click && !h.input && !h.onMouseDown.IsValid()) {
            continue;
        }
        const gp::Bounds& b = h.bounds;
        out.Append(fmt("hit rect=%.0f,%.0f,%.0f,%.0f click=%d input=%d focusId=%d\n", b.x, b.y, b.w, b.h, click ? 1 : 0,
                       h.input ? 1 : 0, h.focusId));
    }
    for (const gp::ScrollRect& s : paint.scrolls) {
        const gp::Bounds& b = s.bounds;
        out.Append(fmt("scroll rect=%.0f,%.0f,%.0f,%.0f content=%.0f,%.0f offset=%.0f,%.0f\n", b.x, b.y, b.w, b.h,
                       s.contentW, s.contentH, s.scrollX, s.scrollY));
    }
    return ToStrTemp(out);
}

static TempStr ToolWindowLayoutTemp(ToolWindow* tw) {
    return GpuiLayoutTemp(tw->gw, Str(tw->desc.name), ToolWindowRect(tw));
}

void ToolRootView::OnTick(ToolRootView* self, gp::Ctx* cx, const gp::TickEvent* ev) {
    ToolWindow* tw = self->tw;
    if (!IsLive(tw) || tw->closing || !tw->desc.onTick || !IsMainWindowValidAndNotClosing(tw->owner)) {
        return;
    }
    tw->desc.onTick(tw->owner, cx, ev->ms);
}

void ToolRootView::OnKeyUp(ToolRootView* self, gp::Ctx* cx, const gp::KeyEvent* ev) {
    ToolWindow* tw = self->tw;
    if (!IsLive(tw) || tw->closing || !tw->desc.onKeyUp || !IsMainWindowValidAndNotClosing(tw->owner)) {
        return;
    }
    tw->desc.onKeyUp(tw->owner, cx, ev);
}

void ToolRootView::OnCaptureKey(ToolRootView* self, gp::Ctx* cx, const gp::KeyEvent* ev) {
    ToolWindow* tw = self->tw;
    if (!IsLive(tw) || tw->closing || !IsMainWindowValidAndNotClosing(tw->owner)) {
        return;
    }
    // a message box in this window is modal to it
    bool handled = MsgBoxOnKeyInToolWindow(tw, ev->vk, CtrlOrCmd(ev));
    if (!handled && tw->desc.onCaptureKey) {
        handled = tw->desc.onCaptureKey(tw->owner, cx, ev);
    }
    if (!handled) {
        return;
    }
    const_cast<gp::KeyEvent*>(ev)->propagate = false;
    gp::Notify(cx);
}

TempStr ToolWindowTestTemp(Str action, Str name, Str kind, int a, int b, int c, int d) {
    if (str::Eq(action, StrL("on")) || str::Eq(action, StrL("off"))) {
        gToolWindowsOff = str::Eq(action, StrL("off"));
        return fmt("OK available=%d", ToolWindowsAvailable() ? 1 : 0);
    }
    if (str::Eq(action, StrL("list"))) {
        str::Builder out;
        out.Append(fmt("OK available=%d count=%d", ToolWindowsAvailable() ? 1 : 0, len(gToolWindows)));
        for (ToolWindow* tw : gToolWindows) {
            Rect r = ToolWindowRect(tw);
            out.Append(fmt("\n%s made=%d rect=%d,%d,%d,%d", Str(tw->desc.name), tw->gw ? 1 : 0, r.x, r.y, r.dx, r.dy));
            out.Append(fmt(" title='%s'", tw->title));
        }
        return ToStrTemp(out);
    }
    // the same dump of the first main window, for what is still in the frame
    if (str::Eq(action, StrL("layout")) && str::Eq(name, StrL("frame"))) {
        MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
        if (!win || !win->gpuiWin) {
            return StrL("NOTREADY");
        }
        return GpuiLayoutTemp(win->gpuiWin, name, AppShellWindowScreenRect(win));
    }
    ToolWindow* tw = ToolWindowFind(name);
    if (!tw) {
        return StrL("ERR no-such-window");
    }
    if (str::Eq(action, StrL("close"))) {
        // as the close box does: the owner of the content hears about it
        DestroyNow(tw);
        return StrL("OK");
    }
    if (!tw->gw) {
        return StrL("NOTREADY");
    }
    if (str::Eq(action, StrL("input"))) {
        if (str::Eq(kind, StrL("mackey"))) {
            ToolWinNativeInjectKey(tw->gw, a);
            return StrL("OK");
        }
        TempStr res = AppShellTestInputGpui(tw->gw, kind, a, b, c, d);
        ToolWindowInvalidate(tw);
        return res;
    }
    if (str::Eq(action, StrL("layout"))) {
        return ToolWindowLayoutTemp(tw);
    }
    if (str::Eq(action, StrL("state"))) {
        gp::Window* gw = tw->gw;
        bool edit = gw->input && gw->input->focused;
        str::Builder out;
        gp::WinSize ws = gp::WindowSize(gw);
        out.Append(
            fmt("OK size=%d,%d focusId=%d edit=%d", (int)ws.dipW, (int)ws.dipH, gp::WindowFocusedId(gw), edit ? 1 : 0));
        if (edit) {
            gp::InputState* in = gw->input;
            gp::Str v = gp::InputValue(in);
            out.Append(fmt(" editText='%s' caret=%d sel=%d,%d", Str{(char*)v.s, (int)v.len}, gp::InputCursor(in),
                           (int)in->selectedRange.start, (int)in->selectedRange.end));
        }
        out.Append(fmt(" popup=%d active=%d", IsTrackedPopupOpenInApp(gw->app) ? 1 : 0, gw->active ? 1 : 0));
        return ToStrTemp(out);
    }
    return StrL("ERR action");
}
