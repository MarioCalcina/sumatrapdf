/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// A font described by (name, size, style), with the platform's font object
// attached. Fonts are interned: the same description always maps to the same
// PlatformFont, so callers can compare them by pointer. They are needed for as
// long as the program runs, so they come out of the perm arena and are never
// freed individually.

enum class PlatformFontStyle {
    Regular = 0,
    Bold = 1,
    Italic = 2,
    Underline = 4,
    Strikeout = 8,
};

inline PlatformFontStyle operator|(PlatformFontStyle a, PlatformFontStyle b) {
    return (PlatformFontStyle)((int)a | (int)b);
}

struct PlatformFont {
    // intrusive list of interned fonts, rooted at gPlatformFonts
    PlatformFont* next = nullptr;
    Str name;
    float sizePt = 0;
    PlatformFontStyle style = PlatformFontStyle::Regular;
    uintptr_t nativeId = 0;
    int averageCharWidth = 0;
    void* nativeFont = nullptr;
    // memoized by GetBoldPlatformFont()
    PlatformFont* boldVariant = nullptr;

    Str GetName() const { return name; }
    float GetSize() const { return sizePt; }
    PlatformFontStyle GetStyle() const { return style; }
    bool SameAs(Str name, float sizePt, PlatformFontStyle style) const;
};

PlatformFont* GetPlatformFont(Str name, float sizePt, PlatformFontStyle style);
// the bold variant of a font (the font itself if it is already bold). Like all
// PlatformFonts it is interned and lives for the whole run
PlatformFont* GetBoldPlatformFont(PlatformFont*);
// fills in the platform font object; implemented per platform. Returns false if
// no font could be created at all
bool PlatformFontCreateNative(PlatformFont*);

// maxDx < 0 means "as wide as it needs to be" (no wrapping)
Size PlatformFontMeasureText(PlatformFont*, Str s, int maxDx = -1);
int PlatformFontLineHeight(PlatformFont*);

PlatformFont* GetDefaultGuiFont(bool bold = false, bool italic = false);
PlatformFont* GetUserGuiFont(Str fontName, int size);
PlatformFont* GetUserGuiFontEx(Str fontName, int size, bool bold, bool italic);
PlatformFont* GetScaledPlatformFont(PlatformFont*, int percent);
