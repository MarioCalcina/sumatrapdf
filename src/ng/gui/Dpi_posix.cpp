/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "gui/Dpi.h"

int dpiX = 96;

int DpiGet() {
    return dpiX > 0 ? dpiX : 96;
}

int DpiScaleByDpi(int dpi, int n) {
    if (dpi <= 0) {
        dpi = 96;
    }
    return (int)(((i64)n * dpi) / 96);
}

int DpiScale(int x) {
    return DpiScaleByDpi(DpiGet(), x);
}

int DpiGetSystemMetrics(int index, int dpi) {
    constexpr int kScrollbarSize = 17;
    if (index == SM_CXVSCROLL || index == SM_CYHSCROLL) {
        return DpiScaleByDpi(dpi, kScrollbarSize);
    }
    return 0;
}

int DpiGetSystemMetrics(int index) {
    return DpiGetSystemMetrics(index, DpiGet());
}

bool DpiPosix_UnitTests() {
    return DpiGetSystemMetrics(SM_CXVSCROLL, 96) == 17 && DpiGetSystemMetrics(SM_CYHSCROLL, 192) == 34 &&
           DpiGetSystemMetrics(-1, 96) == 0;
}
