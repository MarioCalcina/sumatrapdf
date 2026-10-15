/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// orig's cursors made from an annotation tool's SVG icon (AnnotPlacement.cpp,
// AnnotPlacementCursors_win.cpp): the note and the ink tool, in the window
// text color. gpui's CursorKind is a fixed list, so AppKit makes the cursor
// (MacCursorNew) from the pixels rendered here.

#include "base/Base.h"
#include "base/Pixmap.h"

#include <mupdf/fitz.h>

#include "Settings.h"
#include "DocController.h"
#include "Commands.h"
#include "Notifications.h"
#include "MainWindow.h"
#include "Theme.h"
#include "ImageReader.h"
#include "EmbeddedResources.h"
#include "SvgIcons.h"
#include "gui/MacServices.h"
#include "gui/NativeCursors.h"

#include "SumatraLog.h"

// in points, as orig's max(toolbar icon, system cursor) at 100%
constexpr int kPlacementCursorDx = 32;

// A custom ToolbarSvgIcon comes from the settings file, so it can be malformed;
// mupdf throws on that and an uncaught mupdf exception aborts the process.
// Premultiplied BGRA, transparent where the SVG left the background.
static Pixmap* RenderSvgPixmap(fz_context* ctx, Str svgData, int dx, int dy, Color fgCol) {
    TempStr svg = str::ReplaceTemp(svgData, StrL("currentColor"), SerializeColorTemp(fgCol));
    fz_buffer* buf = nullptr;
    fz_display_list* list = nullptr;
    fz_device* dev = nullptr;
    fz_pixmap* pix = nullptr;
    fz_var(buf);
    fz_var(list);
    fz_var(dev);
    fz_var(pix);
    fz_try(ctx) {
        buf = fz_new_buffer_from_copied_data(ctx, (u8*)svg.s, (size_t)svg.len);
        float svgDx = 0;
        float svgDy = 0;
        list = fz_new_display_list_from_svg(ctx, buf, nullptr, nullptr, &svgDx, &svgDy);
        pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), fz_make_irect(0, 0, dx, dy), nullptr, 1);
        fz_clear_pixmap(ctx, pix);
        dev = fz_new_draw_device(ctx, fz_scale((float)dx / svgDx, (float)dy / svgDy), pix);
        fz_run_display_list(ctx, list, dev, fz_identity, fz_infinite_rect, nullptr);
        fz_close_device(ctx, dev);
    }
    fz_always(ctx) {
        fz_drop_device(ctx, dev);
        fz_drop_display_list(ctx, list);
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx) {
        fz_drop_pixmap(ctx, pix);
        fz_report_error(ctx);
        logf("RenderSvgPixmap: rendering svg icon failed with: '%s'\n", Str(fz_caught_message(ctx)));
        return nullptr;
    }
    Pixmap* pm = AllocPixmap(dx, dy, PixmapFormat::BGRA8, true);
    if (pm) {
        pm->hasAlpha = true;
        for (int y = 0; y < dy; y++) {
            const u8* s = pix->samples + (pix->stride * y);
            u8* d = pm->data + ((size_t)pm->stride * y);
            for (int x = 0; x < dx; x++, s += pix->n, d += 4) {
                d[0] = s[2];
                d[1] = s[1];
                d[2] = s[0];
                d[3] = s[3];
            }
        }
    }
    fz_drop_pixmap(ctx, pix);
    return pm;
}

struct PlacementCursor {
    void* cursor = nullptr;
    float scale = 0;
    Color color = 0;
};

static PlacementCursor gTextCursor;
static PlacementCursor gInkCursor;

// orig's GetTextAnnotationPlacementCursor / GetInkAnnotationPlacementCursor:
// rebuilt when the theme or the display's scale changes
static void* PlacementCursorGet(PlacementCursor& pc, const char* icon, int hotX, int hotY, float scale) {
    Color color = ThemeWindowTextColor();
    if (pc.cursor && pc.scale == scale && pc.color == color) {
        return pc.cursor;
    }
    // an icon with a <text> needs a base14 font, which mupdf only gets from
    // the embedded archive
    static fz_context* ctx = nullptr;
    if (!ctx) {
        InstallEmbeddedFontLoader();
        ctx = fz_new_context_windows();
    }
    if (!ctx) {
        return pc.cursor;
    }
    int px = (int)((float)kPlacementCursorDx * scale + 0.5f);
    Pixmap* pm = RenderSvgPixmap(ctx, Str(icon), px, px, color);
    if (!pm) {
        return pc.cursor;
    }
    void* cursor = MacCursorNew(pm, scale, hotX, hotY);
    FreePixmap(pm);
    if (!cursor) {
        return pc.cursor;
    }
    MacCursorRelease(pc.cursor);
    pc = {cursor, scale, color};
    return cursor;
}

// the AppKit cursor for `cursor` at a display's `scale`, null if there is none
void* NativeCursorGet(NativeCursor cursor, float scale) {
    if (!(scale > 0)) {
        scale = 1;
    }
    switch (cursor) {
        case NativeCursor::TextAnnotationPlacement:
            return PlacementCursorGet(gTextCursor, gIconAnnotText, 0, 0, scale);
        case NativeCursor::InkAnnotationPlacement:
            // the pen's tip
            return PlacementCursorGet(gInkCursor, gIconEditAnnotations, (4 * kPlacementCursorDx) / 24,
                                      (20 * kPlacementCursorDx) / 24, scale);
        default:
            return nullptr;
    }
}

static SeqStrings kNativeCursorNames = "none\0drag\0laser\0text\0ink\0";

// for tests: the native cursor the canvas asked for and whether AppKit shows it
TempStr NativeCursorStateTemp(MainWindow* win) {
    NativeCursor nc = win ? win->nativeCursor : NativeCursor::None;
    void* cursor = nullptr;
    if (nc == NativeCursor::TextAnnotationPlacement) {
        cursor = gTextCursor.cursor;
    } else if (nc == NativeCursor::InkAnnotationPlacement) {
        cursor = gInkCursor.cursor;
    }
    bool shown = cursor && MacCursorIsCurrent(cursor);
    return fmt("nativeCursor=%s shown=%d\n", SeqStrByIndex(kNativeCursorNames, (int)nc), shown ? 1 : 0);
}
