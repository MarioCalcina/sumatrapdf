/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "gui/PlatformFont.h"
#include "base/MacTypesHide.h"
#include <CoreText/CoreText.h>
#include "base/MacTypesShow.h"

#include "gui/PlatformText.h"

// TODO: not quite sure why measuring a space directly gives a (much) bigger
// size than the difference below, which looks like better spacing to me
float PlatformTextRender::GetSpaceDx() {
    float l1 = Measure(StrL("wa")).dx;
    float l2 = Measure(StrL("w a")).dx;
    return l2 - l1;
}

int PlatformTextRender::StringLenForWidth(Str s, float dx, float sWidth) {
    float fullWidth = sWidth >= 0 ? sWidth : Measure(s).dx;
    if (fullWidth <= dx) {
        return len(s);
    }
    // make the best guess of the length that fits
    int sLen = len(s);
    int n = (int)((dx / fullWidth) * (float)sLen);
    ReportIf(n > sLen);
    // the guess can land inside a utf-8 sequence; back up to its start
    n = Utf8CodepointStartByte(s, n);
    if (n == 0) {
        // nothing fits in the remaining space; caller flushes the line and
        // re-lays the run at full width. Don't Measure an empty string.
        return 0;
    }
    RectF r = Measure(Str(s.s, n));
    // find the length of s that fits within dx iff the next codepoint makes it
    // exceed dx
    int dir = 1; // increasing length
    if (r.dx > dx) {
        dir = -1; // decreasing length
    }
    while (n > 0) {
        int prevN = n;
        if (dir == 1) {
            Utf8CodepointNext(s, n);
        } else {
            Utf8CodepointPrev(s, n);
        }
        if (n == prevN) {
            break;
        }
        r = Measure(Str(s.s, n));
        if (1 == dir) {
            // if advancing length, we know that previous string did fit, so if
            // the new one doesn't fit, the previous length was the right one
            if (r.dx > dx) {
                return prevN;
            }
        } else {
            // if decreasing length, we know that previous string didn't fit, so if
            // the one one fits, it's of the correct length
            if (r.dx < dx) {
                return n;
            }
        }
    }
    // even a single codepoint is longer than available space
    return 0;
}

// Guesses sizes and draws nothing when there is no platform text engine.
struct StubTextRender : PlatformTextRender {
    PlatformFont* currFont = nullptr;

    float CurrFontSize() const {
        if (currFont && currFont->GetSize() > 0) {
            return currFont->GetSize();
        }
        return 12.5f;
    }

    float AverageCharDx() const { return CurrFontSize() * 0.55f; }

    void SetFont(PlatformFont* font) override { currFont = font; }

    float GetCurrFontLineSpacing() override { return CurrFontSize() * 1.25f; }

    // a rough guess, so measure by codepoint rather than by utf-8 byte
    RectF Measure(Str s) override {
        return {0, 0, (float)Utf8CodepointCount(s) * AverageCharDx(), GetCurrFontLineSpacing()};
    }

    void SetTextColor(Color) override {}
    void Draw(Str, RectF, bool) override {}
};

PlatformTextRender* CreateNativeTextRender();

PlatformTextRender* CreatePlatformTextRender() {
    return CreateNativeTextRender();
}

static CFStringRef NewCoreTextString(Str s) {
    return CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)s.s, len(s), kCFStringEncodingUTF8, false);
}

struct CoreTextRender : PlatformTextRender {
    CGContextRef context = nullptr;
    PlatformFont* currFont = nullptr;
    Color textColor = kColBlack;

    explicit CoreTextRender(CGContextRef context) : context(context) {}

    void SetFont(PlatformFont* font) override { currFont = font; }
    float GetCurrFontLineSpacing() override { return (float)PlatformFontLineHeight(currFont); }

    RectF Measure(Str s) override {
        Size size = PlatformFontMeasureText(currFont, s);
        return {0, 0, (float)size.dx, (float)size.dy};
    }

    void SetTextColor(Color col) override { textColor = col; }

    void Draw(Str s, RectF bb, bool isRtl) override {
        if (!context || !currFont || !currFont->nativeFont || len(s) == 0) {
            return;
        }
        CFStringRef text = NewCoreTextString(s);
        if (!text) {
            return;
        }
        u8 r = 0;
        u8 g = 0;
        u8 b = 0;
        UnpackColor(textColor, r, g, b);
        CGColorRef color = CGColorCreateGenericRGB(r / 255.0, g / 255.0, b / 255.0, 1);
        CTWritingDirection direction = isRtl ? kCTWritingDirectionRightToLeft : kCTWritingDirectionLeftToRight;
        CTParagraphStyleSetting setting = {kCTParagraphStyleSpecifierBaseWritingDirection, sizeof(direction),
                                           &direction};
        CTParagraphStyleRef paragraph = CTParagraphStyleCreate(&setting, 1);
        const void* keys[] = {kCTFontAttributeName, kCTForegroundColorAttributeName, kCTParagraphStyleAttributeName};
        const void* values[] = {(CTFontRef)currFont->nativeFont, color, paragraph};
        CFDictionaryRef attrs = CFDictionaryCreate(kCFAllocatorDefault, keys, values, dimof(keys),
                                                   &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFAttributedStringRef attributed = CFAttributedStringCreate(kCFAllocatorDefault, text, attrs);
        CTLineRef line = CTLineCreateWithAttributedString(attributed);
        CGFloat ascent = 0;
        CTLineGetTypographicBounds(line, &ascent, nullptr, nullptr);
        CGContextSetTextPosition(context, bb.x, bb.y + ascent);
        CTLineDraw(line, context);
        CFRelease(line);
        CFRelease(attributed);
        CFRelease(attrs);
        CFRelease(paragraph);
        CGColorRelease(color);
        CFRelease(text);
    }
};

PlatformTextRender* CreateNativeTextRender() {
    return new CoreTextRender(nullptr);
}

PlatformTextRender* CreateCoreTextRender(CGContextRef context) {
    return new CoreTextRender(context);
}
