/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: the POSIX half of gui/PlatformFont.cpp. Linux uses Pango and macOS uses
// CoreText; wasm retains estimated metrics until browser font measurement is
// exposed to native code.

#include "base/Base.h"

#include "gui/PlatformFont.h"

#include "base/MacTypesHide.h"
#include <CoreText/CoreText.h>
#include "base/MacTypesShow.h"

static CTFontRef NativeFont(PlatformFont* font) {
    return font ? (CTFontRef)font->nativeFont : nullptr;
}

static CFStringRef NewCfString(Str s) {
    return CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)s.s, len(s), kCFStringEncodingUTF8, false);
}

bool PlatformFontCreateNative(PlatformFont* font) {
    CTFontRef base = nullptr;
    if (len(font->name) > 0) {
        CFStringRef name = NewCfString(font->name);
        if (name) {
            base = CTFontCreateWithName(name, font->sizePt, nullptr);
            CFRelease(name);
        }
    } else {
        base = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, font->sizePt, nullptr);
    }
    if (!base) {
        return false;
    }
    CTFontSymbolicTraits traits = 0;
    int style = (int)font->style;
    if (style & (int)PlatformFontStyle::Bold) {
        traits |= kCTFontBoldTrait;
    }
    if (style & (int)PlatformFontStyle::Italic) {
        traits |= kCTFontItalicTrait;
    }
    if (traits) {
        CTFontRef styled = CTFontCreateCopyWithSymbolicTraits(base, font->sizePt, nullptr, traits,
                                                              kCTFontBoldTrait | kCTFontItalicTrait);
        if (styled) {
            CFRelease(base);
            base = styled;
        }
    }
    font->nativeFont = (void*)base;
    return true;
}

static CFAttributedStringRef NewAttributedString(PlatformFont* font, Str s) {
    CFStringRef text = NewCfString(s);
    if (!text) {
        return nullptr;
    }
    const void* keys[] = {kCTFontAttributeName};
    const void* values[] = {NativeFont(font)};
    CFDictionaryRef attrs = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1, &kCFTypeDictionaryKeyCallBacks,
                                               &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef result = CFAttributedStringCreate(kCFAllocatorDefault, text, attrs);
    CFRelease(attrs);
    CFRelease(text);
    return result;
}

Size PlatformFontMeasureText(PlatformFont* font, Str s, int maxDx) {
    if (!font || len(s) == 0) {
        return {};
    }
    CFAttributedStringRef text = NewAttributedString(font, s);
    if (!text) {
        return {};
    }
    CGSize size{};
    if (maxDx < 0) {
        CTLineRef line = CTLineCreateWithAttributedString(text);
        CGFloat ascent = 0;
        CGFloat descent = 0;
        CGFloat leading = 0;
        size.width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
        size.height = ascent + descent + leading;
        CFRelease(line);
    } else {
        CTFramesetterRef framesetter = CTFramesetterCreateWithAttributedString(text);
        CFIndex n = CFAttributedStringGetLength(text);
        size = CTFramesetterSuggestFrameSizeWithConstraints(framesetter, CFRangeMake(0, n), nullptr,
                                                            CGSizeMake(maxDx, CGFLOAT_MAX), nullptr);
        CFRelease(framesetter);
    }
    CFRelease(text);
    return {(int)ceil(size.width), (int)ceil(size.height)};
}

int PlatformFontLineHeight(PlatformFont* font) {
    CTFontRef native = NativeFont(font);
    if (!native) {
        return 0;
    }
    return (int)ceil(CTFontGetAscent(native) + CTFontGetDescent(native) + CTFontGetLeading(native));
}

static PlatformFontStyle FontStyle(bool bold, bool italic) {
    PlatformFontStyle style = PlatformFontStyle::Regular;
    if (bold) {
        style = style | PlatformFontStyle::Bold;
    }
    if (italic) {
        style = style | PlatformFontStyle::Italic;
    }
    return style;
}

PlatformFont* GetDefaultGuiFont(bool bold, bool italic) {
    return GetPlatformFont(Str(), 12.0f, FontStyle(bold, italic));
}

PlatformFont* GetUserGuiFont(Str fontName, int size) {
    return GetUserGuiFontEx(fontName, size, false, false);
}

PlatformFont* GetUserGuiFontEx(Str fontName, int size, bool bold, bool italic) {
    if (str::EqI(fontName, StrL("automatic")) || str::EqI(fontName, StrL("auto"))) {
        fontName = {};
    }
    return GetPlatformFont(fontName, (float)size * 72.0f / 96.0f, FontStyle(bold, italic));
}

PlatformFont* GetScaledPlatformFont(PlatformFont* font, int percent) {
    if (!font || percent <= 0) {
        return nullptr;
    }
    return GetPlatformFont(font->name, font->sizePt * (float)percent / 100.0f, font->style);
}

PlatformFont* GetBoldPlatformFont(PlatformFont* font) {
    if (!font) {
        return nullptr;
    }
    if (font->boldVariant) {
        return font->boldVariant;
    }
    if ((int)font->style & (int)PlatformFontStyle::Bold) {
        font->boldVariant = font;
        return font;
    }
    font->boldVariant = GetPlatformFont(font->name, font->sizePt, font->style | PlatformFontStyle::Bold);
    return font->boldVariant ? font->boldVariant : font;
}
