/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ByteReaderWriter.h"

#include "base/Pixmap.h"

void CopyPixmapRows(Pixmap* dst, const u8* src, int srcStride) {
    size_t rowBytes = (size_t)dst->width * PixmapBytesPerPixel(dst->format);
    for (int y = 0; y < dst->height; y++) {
        memcpy(dst->data + (size_t)y * dst->stride, src + (size_t)y * srcStride, rowBytes);
    }
}

static void AppendPixmapPixelBGR(str::Builder& data, const Pixmap* pixmap, int x, int y) {
    const u8* src = pixmap->data + ((size_t)y * pixmap->stride) + ((size_t)x * PixmapBytesPerPixel(pixmap->format));
    if (pixmap->format == PixmapFormat::RGBA8) {
        data.AppendChar((char)src[2]);
        data.AppendChar((char)src[1]);
        data.AppendChar((char)src[0]);
        return;
    }
    data.AppendChar((char)src[0]);
    data.AppendChar((char)src[1]);
    data.AppendChar((char)src[2]);
}

// create data for a .bmp file from this pixmap. The output matches the classic BMP
// format accepted by Windows LoadImage(..., LD_LOADFROMFILE): BITMAPFILEHEADER +
// BITMAPINFOHEADER + bottom-up 24bpp BGR rows.
Str PixmapToBmpFormat(const Pixmap* pixmap) {
    if (!pixmap || !pixmap->data || pixmap->width <= 0 || pixmap->height <= 0) {
        return {};
    }
    if (pixmap->format != PixmapFormat::BGRA8 && pixmap->format != PixmapFormat::BGR8 &&
        pixmap->format != PixmapFormat::RGBA8) {
        return {};
    }

    size_t w = (size_t)pixmap->width;
    size_t h = (size_t)pixmap->height;
    size_t rowStride = ((w * 3) + 3) & ~(size_t)3;
    size_t headerLen = 14 + 40;
    size_t bmpBytes = (rowStride * h) + headerLen;
    if (rowStride > UINT32_MAX || bmpBytes > INT_MAX || bmpBytes > UINT32_MAX) {
        return {};
    }

    ByteWriterLE w2((int)bmpBytes);
    str::Builder& bmpData = w2.d;
    w2.Write16(0x4d42); // "BM"
    w2.Write32((u32)bmpBytes);
    w2.Write16(0);
    w2.Write16(0);
    w2.Write32((u32)headerLen);

    w2.Write32(40); // BITMAPINFOHEADER size
    w2.Write32((u32)w);
    w2.Write32((u32)h);
    w2.Write16(1);  // planes
    w2.Write16(24); // bit count
    w2.Write32(0);  // BI_RGB
    w2.Write32((u32)(rowStride * h));
    w2.Write32(0);
    w2.Write32(0);
    w2.Write32(0);
    w2.Write32(0);

    int padding = (int)rowStride - (pixmap->width * 3);
    for (int y = pixmap->height - 1; y >= 0; y--) {
        for (int x = 0; x < pixmap->width; x++) {
            AppendPixmapPixelBGR(bmpData, pixmap, x, y);
        }
        for (int i = 0; i < padding; i++) {
            bmpData.AppendChar(0);
        }
    }

    return bmpData.TakeStr();
}

static void UnpremultiplyBgra(u8* d) {
    u32 a = d[3];
    if (a == 0 || a == 255) {
        return;
    }
    d[0] = (u8)std::min<u32>(255, (((u32)d[0] * 255) + (a / 2)) / a);
    d[1] = (u8)std::min<u32>(255, (((u32)d[1] * 255) + (a / 2)) / a);
    d[2] = (u8)std::min<u32>(255, (((u32)d[2] * 255) + (a / 2)) / a);
}

// a 32bpp straight-alpha DIB copy of a heap-backed BGR8 / RGBA8 / BGRA8 pixmap
static Pixmap* CopyHeapPixmapAsBgraDib(const Pixmap* px) {
    if (!px->data || PixmapBytesPerPixel(px->format) == 0) {
        return nullptr;
    }
    Pixmap* dib = AllocPixmapDIB(px->width, px->height);
    if (!dib) {
        return nullptr;
    }
    for (int y = 0; y < px->height; y++) {
        const u8* src = px->data + ((size_t)y * px->stride);
        u8* dst = dib->data + ((size_t)y * dib->stride);
        for (int x = 0; x < px->width; x++) {
            if (px->format == PixmapFormat::BGR8) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = 0xff;
                src += 3;
            } else if (px->format == PixmapFormat::RGBA8) {
                dst[0] = src[2];
                dst[1] = src[1];
                dst[2] = src[0];
                dst[3] = src[3];
                src += 4;
            } else {
                memcpy(dst, src, 4);
                src += 4;
            }
            if (px->premultiplied) {
                UnpremultiplyBgra(dst);
            }
            dst += 4;
        }
    }
    dib->xres = px->xres;
    dib->yres = px->yres;
    return dib;
}

// Takes p. Returns it when its pixels already read as BGRA8; otherwise a
// 32bpp DIB copy (the engine renders a page with few colors to an 8-bit
// palette DIB, image engines decode to 24bpp) and p is freed.
Pixmap* PixmapToBgra(Pixmap* p) {
    if (!p) {
        return nullptr;
    }
    if (p->format == PixmapFormat::BGRA8 && p->data) {
        return p;
    }
    Pixmap* dib = CopyHeapPixmapAsBgraDib(p);
    FreePixmap(p);
    return dib;
}

static inline u8 BlendOver(u8 src, u8 dst, u32 srcAlpha, bool premultiplied) {
    u32 inv = 255 - srcAlpha;
    u32 s = premultiplied ? src : (((u32)src * srcAlpha) + 127) / 255;
    return (u8)std::min<u32>(255, s + ((((u32)dst * inv) + 127) / 255));
}

Pixmap* PixmapToBgr(Pixmap* p) {
    if (!p || p->format == PixmapFormat::BGR8) {
        return p;
    }
    if (p->format == PixmapFormat::Native || !p->data) {
        return p;
    }

    Pixmap* res = AllocPixmap(p->width, p->height, PixmapFormat::BGR8);
    if (!res) {
        return p;
    }
    bool isRgba = p->format == PixmapFormat::RGBA8;
    for (int y = 0; y < p->height; y++) {
        const u8* s = p->data + ((size_t)y * p->stride);
        u8* d = res->data + ((size_t)y * res->stride);
        for (int x = 0; x < p->width; x++, s += 4, d += 3) {
            u8 b = isRgba ? s[2] : s[0];
            u8 g = s[1];
            u8 r = isRgba ? s[0] : s[2];
            if (p->hasAlpha) {
                b = BlendOver(b, 255, s[3], p->premultiplied);
                g = BlendOver(g, 255, s[3], p->premultiplied);
                r = BlendOver(r, 255, s[3], p->premultiplied);
            }
            d[0] = b;
            d[1] = g;
            d[2] = r;
        }
    }
    res->xres = p->xres;
    res->yres = p->yres;
    FreePixmap(p);
    return res;
}

// Like BlitPixmap(), but honours the source alpha. BlitPixmap() is a straight
// SRCCOPY, which paints the transparent parts of an icon black; anything drawn
// over an arbitrary background (toolbar icons on the home page) needs this.
//
// The compositing is done by hand rather than with AlphaBlend() or GDI+:
// msimg32 and GdiPlusUtil are linked into SumatraPDF.exe but not into the other
// consumers of base (PdfFilter, PdfPreview, ...). Reading the destination back
// costs a BitBlt, which is nothing at icon sizes.
//
// Only 1:1 blits are composited; a scaling blit falls back to the opaque path.

static bool SkipRecolorPixel(int x, int y, Vec<Rect>* skipRects) {
    if (skipRects) {
        for (Rect& r : *skipRects) {
            if (r.Contains(x, y)) {
                return true;
            }
        }
    }
    return false;
}

static int Mul255(int a, int b) {
    int n = (a * b) + 128;
    n += n >> 8;
    return n >> 8;
}

// 0 up to `from`, 255 from `to` on, linear in between
static int Ramp255(int v, int from, int to) {
    if (v <= from) {
        return 0;
    }
    if (v >= to) {
        return 255;
    }
    return ((v - from) * 255) / (to - from);
}

// link ink is blue (a pale blue fill is not), not too dark, not near paper
constexpr int kLinkBlueFrom = 32;
constexpr int kLinkBlueTo = 64;
constexpr int kLinkDarkFrom = 64;
constexpr int kLinkDarkTo = 96;
constexpr int kLinkLightFrom = 224;
constexpr int kLinkLightTo = 240;

// How much of a pixel (B,G,R) is link ink, 0..255. Ramps, not cutoffs: JPEG
// noise in a pale blue fill straddled a cutoff pixel by pixel, which turned
// the fill into a checkerboard of link and page colors (#6313)
static int LinkWeight(const u8* pixel) {
    int maxRG = pixel[2] > pixel[1] ? pixel[2] : pixel[1];
    int lum = (pixel[0] + pixel[1] + pixel[2]) / 3;
    int w = Ramp255(pixel[0] - maxRG, kLinkBlueFrom, kLinkBlueTo);
    w = Mul255(w, Ramp255(pixel[0], kLinkDarkFrom, kLinkDarkTo));
    w = Mul255(w, 255 - Ramp255(lum, kLinkLightFrom, kLinkLightTo));
    return w;
}

// Maps black-on-white pixels to textColor-on-bgColor (interpolating the rest),
// turns blue-ish "link" pixels into linkColor, and leaves skipRects alone.
// Pixel bytes are B,G,R[,A]; rows are stride bytes apart.
static void RecolorPixels(u8* data, int w, int h, size_t stride, int bpp, Color textColor, Color bgColor,
                          Color linkColor, Vec<Rect>* skipRects) {
    u8 linkR = 0, linkG = 0, linkB = 0;
    UnpackColor(linkColor, linkR, linkG, linkB);
    u8 textR, textG, textB, bgR, bgG, bgB;
    UnpackColor(textColor, textR, textG, textB);
    UnpackColor(bgColor, bgR, bgG, bgB);
    const int base[4] = {textB, textG, textR, 0};
    const int diff[4] = {(int)bgB - textB, (int)bgG - textG, (int)bgR - textR, 255};
    const int linkBase[3] = {linkB, linkG, linkR};
    const int linkDiff[3] = {(int)bgB - linkB, (int)bgG - linkG, (int)bgR - linkR};
    int nChannels = std::min(bpp, 4);
    for (int y = 0; y < h; y++) {
        u8* pixel = data + ((size_t)y * stride);
        for (int x = 0; x < w; x++, pixel += bpp) {
            if (SkipRecolorPixel(x, y, skipRects)) {
                continue;
            }
            int linkW = linkColor ? LinkWeight(pixel) : 0;
            // link ink fades to bg by its R/G coverage, like text does
            int rg = ((int)pixel[1] + pixel[2]) / 2;
            for (int i = 0; i < nChannels; i++) {
                int page = base[i] + Mul255(pixel[i], diff[i]);
                if (linkW > 0 && i < 3) {
                    int link = linkBase[i] + Mul255(rg, linkDiff[i]);
                    page += Mul255(linkW, link - page);
                }
                pixel[i] = (u8)page;
            }
        }
    }
}

// same, for an HBITMAP: in place for mapped 24/32-bit DIBs, via the palette
// for 8-bit ones, else through GetDIBits/SetDIBits

void RecolorPixmap(Pixmap* px, Color textColor, Color bgColor, Color linkColor, Vec<Rect>* skipRects) {
    if (!px) {
        return;
    }
    if ((textColor & 0xffffff) == kColBlack && (bgColor & 0xffffff) == kColWhite && !linkColor && !skipRects) {
        return;
    }
    if (!px->data || px->width <= 0 || px->height <= 0 || px->format == PixmapFormat::RGBA8) {
        return;
    }
    RecolorPixels(px->data, px->width, px->height, px->stride, PixmapBytesPerPixel(px->format), textColor, bgColor,
                  linkColor, skipRects);
}

// Copy an HBITMAP into a top-down 32bpp BGRA pixmap. Reading the clipboard as
// 24bpp GetDIBits sheared paste-as-stamp (issue #6059): area-copy puts a
// 32-bpp CF_BITMAP on the clipboard, and converting that to DWORD-padded 24bpp
// rows does not land on the stride the stamp path uses. 32bpp BI_RGB rows are
// always width*4, the same as AllocPixmap(BGRA8).

Pixmap* PixmapApplyExifOrientation(Pixmap* px, int orientation) {
    if (!px || !px->data || orientation < 2 || orientation > 8) {
        return px;
    }
    int w = px->width;
    int h = px->height;
    bool transposed = orientation >= 5;
    Pixmap* out = AllocPixmap(transposed ? h : w, transposed ? w : h, px->format, px->premultiplied);
    if (!out) {
        return px;
    }
    out->xres = px->xres;
    out->yres = px->yres;
    out->hasAlpha = px->hasAlpha;

    int bpp = PixmapBytesPerPixel(px->format);
    for (int sy = 0; sy < h; sy++) {
        const u8* src = px->data + ((size_t)sy * (size_t)px->stride);
        for (int sx = 0; sx < w; sx++, src += bpp) {
            int dx = sx;
            int dy = sy;
            switch (orientation) {
                case 2:
                    dx = w - 1 - sx;
                    break;
                case 3:
                    dx = w - 1 - sx;
                    dy = h - 1 - sy;
                    break;
                case 4:
                    dy = h - 1 - sy;
                    break;
                case 5:
                    dx = sy;
                    dy = sx;
                    break;
                case 6:
                    dx = h - 1 - sy;
                    dy = sx;
                    break;
                case 7:
                    dx = h - 1 - sy;
                    dy = w - 1 - sx;
                    break;
                case 8:
                    dx = sy;
                    dy = w - 1 - sx;
                    break;
            }
            u8* dst = out->data + ((size_t)dy * (size_t)out->stride) + ((size_t)dx * (size_t)bpp);
            memcpy(dst, src, (size_t)bpp);
        }
    }
    FreePixmap(px);
    return out;
}
