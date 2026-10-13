/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"

#include "gui/PlatformFont.h"
#include "gui/PlatformText.h"
#include "gui/UIModels.h"
#include "EngineBase.h"
#include "EngineAll.h"

#include "base/tests/UtAssert.h"

#include "base/MacTypesHide.h"
#include <CoreGraphics/CoreGraphics.h>
#include "base/MacTypesShow.h"

extern TempStr TestDocPathTemp(Str relPath);

static bool HasInk(Pixmap* pixmap) {
    for (int y = 0; y < pixmap->height; y++) {
        const u8* row = pixmap->data + (size_t)y * pixmap->stride;
        for (int x = 0; x < pixmap->width; x++) {
            const u8* p = row + x * 4;
            if (p[0] != 0xff || p[1] != 0xff || p[2] != 0xff) {
                return true;
            }
        }
    }
    return false;
}

static void TestTextRender(PlatformFont* font) {
    Pixmap* pixmap = AllocPixmap(160, 50, PixmapFormat::BGRA8, true);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGBitmapInfo info = (CGBitmapInfo)((u32)kCGBitmapByteOrder32Little | (u32)kCGImageAlphaPremultipliedFirst);
    CGContextRef context =
        CGBitmapContextCreate(pixmap->data, pixmap->width, pixmap->height, 8, pixmap->stride, colorSpace, info);
    CGColorSpaceRelease(colorSpace);
    CGContextSetRGBFillColor(context, 1, 1, 1, 1);
    CGContextFillRect(context, CGRectMake(0, 0, pixmap->width, pixmap->height));
    CGContextTranslateCTM(context, 0, pixmap->height);
    CGContextScaleCTM(context, 1, -1);
    CGContextSetTextMatrix(context, CGAffineTransformIdentity);
    PlatformTextRender* text = CreateCoreTextRender(context);
    text->SetFont(font);
    text->SetTextColor(kColBlack);
    text->Draw(StrL("CoreText"), RectF(4, 4, 150, 40), false);
    delete text;
    CGContextFlush(context);
    CGContextRelease(context);
    utassert(HasInk(pixmap));
    FreePixmap(pixmap);
}

static void TestEbookRender() {
    Str data = file::ReadFile(TestDocPathTemp(StrL("docs/test/test.epub")));
    utassert(len(data) > 0);
    EngineBase* engine = CreateEngineEpubFromData(data);
    str::Free(data);
    utassert(engine && engine->PageCount() > 0);

    RenderPageArgs args(1, 1.0f, 0);
    Pixmap* pixmap = engine->RenderPage(args);
    utassert(pixmap && pixmap->data);
    utassert(HasInk(pixmap));
    FreePixmap(pixmap);
    engine->Release();
}

bool PlatformFontPosix_UnitTests() {
    PlatformFont* font = GetPlatformFont(StrL("Sans"), 12.0f, PlatformFontStyle::Regular);
    utassert(font && font->nativeFont);

    Size narrow = PlatformFontMeasureText(font, StrL("iiii"));
    Size wide = PlatformFontMeasureText(font, StrL("WWWW"));
    utassert(narrow.dx > 0 && narrow.dy > 0);
    utassert(wide.dx > narrow.dx);

    Size wrapped = PlatformFontMeasureText(font, StrL("one two three four"), wide.dx);
    utassert(wrapped.dx <= wide.dx);
    utassert(wrapped.dy > wide.dy);
    TestTextRender(font);
    TestEbookRender();
    return true;
}
