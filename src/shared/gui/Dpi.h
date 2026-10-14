/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Current layout DPI; DpiScale() and DpiGet() read it.
extern int dpiX;

// win32's MulDiv: a * b / c, rounded to nearest
inline int MulDiv(int a, int b, int c) {
    if (c == 0) {
        return -1;
    }
    long long n = (long long)a * (long long)b;
    long long half = (long long)(c < 0 ? -c : c) / 2;
    n = n >= 0 ? n + half : n - half;
    return (int)(n / c);
}

int DpiGetForHwnd(HWND);
int DpiGet();
int DpiScaleByDpi(int dpi, int n);
int DpiScale(int x);

// ng: the win32 metric ids the portable model code asks for. Dpi_posix.cpp
// supplies the portable scrollbar dimensions for them.
#define SM_CXVSCROLL 2
#define SM_CYHSCROLL 3

int DpiGetSystemMetrics(int index);
int DpiGetSystemMetrics(int index, int dpi);
