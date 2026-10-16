/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Measuring and drawing text with the platform's text engine, for code that
// lays out its own text (the ebook formatter). Everything here works in utf-8;
// converting to whatever the platform wants is the implementation's job.

// PlatformFont lives in gui/PlatformFont.h; include it before this header

struct PlatformTextRender {
    virtual void SetFont(PlatformFont* font) = 0;
    virtual float GetCurrFontLineSpacing() = 0;
    virtual RectF Measure(Str s) = 0;

    virtual void SetTextColor(Color col) = 0;

    virtual void Draw(Str s, RectF bb, bool isRtl) = 0;

    virtual ~PlatformTextRender() = default;

    // these only need Measure(), so they are the same for every implementation
    float GetSpaceDx();
    // how much of s fits in dx, in utf-8 bytes and never cutting a sequence in
    // half. The caller usually already measured the whole string (that's how it
    // knows it needs to wrap); pass that width as sWidth to skip re-measuring
    int StringLenForWidth(Str s, float dx, float sWidth = -1);
};

// measures (and draws) with resources of its own, created on demand, so there
// is nothing to initialize first
PlatformTextRender* CreatePlatformTextRender();

struct CGContext;
PlatformTextRender* CreateCoreTextRender(struct CGContext* context);
