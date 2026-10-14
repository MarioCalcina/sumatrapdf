/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Finder's thumbnails and Quick Look's previews of the formats macOS can't
// show (comics, DjVu, XPS, MOBI, FB2), drawn by our engines. This is the
// SumatraQL library in SumatraPDF.app/Contents/Frameworks. The two app
// extensions in Contents/PlugIns load it; their executables are only
// NSExtensionMain, which finds the providers below by name:
//
//   SumatraThumbnails.appex  SumatraThumbnailProvider  page 1 as an image
//   SumatraPreview.appex     SumatraPreviewProvider    the first pages as a PDF

#include "base/Base.h"

// AppKit's MacTypes and the `defer:` parameter collide with ours
#include "base/MacTypesHide.h"
#define BOOL MacObjcBool
#pragma push_macro("defer")
#undef defer
#import <AppKit/AppKit.h>
#import <ImageIO/ImageIO.h>
#import <QuickLookThumbnailing/QuickLookThumbnailing.h>
#import <QuickLookUI/QuickLookUI.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#pragma pop_macro("defer")
#undef BOOL
#include "base/MacTypesShow.h"

#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"
#include "base/Timer.h"
#include "base/UITask.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "AppSettings.h"
#include "DocumentLayout.h"
#include "DocController.h"
#include "DocProperties.h"
#include "gui/UIModels.h"
#include "EngineBase.h"
#include "Annotation.h"
#include "EngineAll.h"

// the engines log; an extension has no log file
void log(Str s) {
    if (len(s) > 0) {
        fwrite(s.s, 1, (size_t)len(s), stderr);
    }
}

// a preview is the first pages, each this many pixels on its longer side,
// shown at half that in points (sharp on a Retina display)
constexpr int kPreviewMaxPages = 20;
constexpr float kPreviewPagePx = 1600;
constexpr CGFloat kPreviewScale = 2;
constexpr CGFloat kPreviewJpegQuality = 0.85;

static EngineBase* OpenDoc(NSURL* url) {
    Str path((char*)url.fileSystemRepresentation);
    return CreateEngineFromFile(path, nullptr, false);
}

static void ReleasePixmapData(void* pixmap, const void*, size_t) {
    FreePixmap((Pixmap*)pixmap);
}

// page `pageNo` with its longer side `px` pixels; nullptr if it can't be drawn
static CGImageRef RenderPage(EngineBase* engine, int pageNo, float px) {
    RectF box = engine->Transform(engine->PageMediabox(pageNo), pageNo, 1.0f, 0);
    float longer = std::max(box.dx, box.dy);
    if (!(longer > 0) || !(px > 0)) {
        return nullptr;
    }
    RenderPageArgs args(pageNo, px / longer, 0);
    Pixmap* pm = PixmapToBgra(engine->RenderPage(args));
    if (!pm) {
        return nullptr;
    }
    CGBitmapInfo alpha = (CGBitmapInfo)kCGImageAlphaNoneSkipFirst;
    if (pm->hasAlpha) {
        alpha = (CGBitmapInfo)(pm->premultiplied ? kCGImageAlphaPremultipliedFirst : kCGImageAlphaFirst);
    }
    size_t size = (size_t)pm->stride * (size_t)pm->height;
    CGDataProviderRef data = CGDataProviderCreateWithData(pm, pm->data, size, ReleasePixmapData);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGImageRef img = CGImageCreate((size_t)pm->width, (size_t)pm->height, 8, 32, (size_t)pm->stride, cs,
                                   alpha | (CGBitmapInfo)kCGBitmapByteOrder32Little, data, nullptr, false,
                                   kCGRenderingIntentDefault);
    CGColorSpaceRelease(cs);
    CGDataProviderRelease(data);
    return img;
}

// as a JPEG: a PDF keeps it as is, where a bitmap would be stored losslessly
static CGImageRef AsJpeg(CGImageRef img) {
    NSMutableData* jpeg = [NSMutableData data];
    CGImageDestinationRef dst =
        CGImageDestinationCreateWithData((__bridge CFMutableDataRef)jpeg, (__bridge CFStringRef)UTTypeJPEG.identifier,
                                         1, nullptr);
    if (!dst) {
        return nullptr;
    }
    NSDictionary* props = @{(__bridge NSString*)kCGImageDestinationLossyCompressionQuality : @(kPreviewJpegQuality)};
    CGImageDestinationAddImage(dst, img, (__bridge CFDictionaryRef)props);
    bool ok = CGImageDestinationFinalize(dst);
    CFRelease(dst);
    if (!ok) {
        return nullptr;
    }
    CGDataProviderRef data = CGDataProviderCreateWithCFData((__bridge CFDataRef)jpeg);
    CGImageRef res = CGImageCreateWithJPEGDataProvider(data, nullptr, false, kCGRenderingIntentDefault);
    CGDataProviderRelease(data);
    return res;
}

// the document's first pages as a PDF of images; nil if none could be drawn
static NSData* PreviewPdf(NSURL* url) {
    EngineBase* engine = OpenDoc(url);
    if (!engine) {
        return nil;
    }
    NSMutableData* pdf = [NSMutableData data];
    CGDataConsumerRef out = CGDataConsumerCreateWithCFData((__bridge CFMutableDataRef)pdf);
    CGContextRef ctx = CGPDFContextCreate(out, nullptr, nullptr);
    CGDataConsumerRelease(out);
    int nPages = std::min(engine->PageCount(), kPreviewMaxPages);
    int nDrawn = 0;
    for (int pageNo = 1; ctx && pageNo <= nPages; pageNo++) {
        CGImageRef page = RenderPage(engine, pageNo, kPreviewPagePx);
        CGImageRef jpeg = page ? AsJpeg(page) : nullptr;
        CGImageRef img = jpeg ? jpeg : page;
        if (!img) {
            continue;
        }
        CGRect box = CGRectMake(0, 0, CGImageGetWidth(img) / kPreviewScale, CGImageGetHeight(img) / kPreviewScale);
        NSData* boxData = [NSData dataWithBytes:&box length:sizeof(box)];
        NSDictionary* pageInfo = @{(__bridge NSString*)kCGPDFContextMediaBox : boxData};
        CGPDFContextBeginPage(ctx, (__bridge CFDictionaryRef)pageInfo);
        CGContextDrawImage(ctx, box, img);
        CGPDFContextEndPage(ctx);
        nDrawn++;
        CGImageRelease(page);
        if (jpeg) {
            CGImageRelease(jpeg);
        }
    }
    if (ctx) {
        CGPDFContextClose(ctx);
        CGContextRelease(ctx);
    }
    engine->Release();
    return nDrawn > 0 ? pdf : nil;
}

static NSError* CantReadError() {
    return [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadCorruptFileError userInfo:nil];
}

@interface SumatraThumbnailProvider : QLThumbnailProvider
@end

@implementation SumatraThumbnailProvider
- (void)provideThumbnailForFileRequest:(QLFileThumbnailRequest*)request
                     completionHandler:(void (^)(QLThumbnailReply* _Nullable, NSError* _Nullable))handler {
    EngineBase* engine = OpenDoc(request.fileURL);
    if (!engine || engine->PageCount() < 1) {
        if (engine) {
            engine->Release();
        }
        handler(nil, CantReadError());
        return;
    }
    CGSize maxSize = request.maximumSize;
    CGFloat scale = request.scale > 0 ? request.scale : 1;
    CGImageRef page = RenderPage(engine, 1, (float)(std::max(maxSize.width, maxSize.height) * scale));
    engine->Release();
    if (!page) {
        handler(nil, CantReadError());
        return;
    }
    // the page's shape, as large as fits
    CGFloat dx = CGImageGetWidth(page) / scale;
    CGFloat dy = CGImageGetHeight(page) / scale;
    CGFloat fit = std::min((CGFloat)1, std::min(maxSize.width / dx, maxSize.height / dy));
    CGSize size = CGSizeMake(dx * fit, dy * fit);
    id img = CFBridgingRelease(page);
    QLThumbnailReply* reply = [QLThumbnailReply replyWithContextSize:size
                                                        drawingBlock:^MacObjcBool(CGContextRef ctx) {
                                                          CGContextDrawImage(
                                                              ctx, CGRectMake(0, 0, size.width, size.height),
                                                              (__bridge CGImageRef)img);
                                                          return YES;
                                                        }];
    handler(reply, nil);
}
@end

// macOS 12 has data-based previews; on 11 the extension doesn't load
API_AVAILABLE(macos(12.0))
@interface SumatraPreviewProvider : QLPreviewProvider <QLPreviewingController>
@end

@implementation SumatraPreviewProvider
- (void)providePreviewForFileRequest:(QLFilePreviewRequest*)request
                   completionHandler:(void (^)(QLPreviewReply* _Nullable, NSError* _Nullable))handler {
    NSURL* url = request.fileURL;
    // Quick Look sizes its window from this, before the pages exist
    CGSize size = CGSizeMake(kPreviewPagePx / kPreviewScale * 0.75, kPreviewPagePx / kPreviewScale);
    QLPreviewReply* reply =
        [[QLPreviewReply alloc] initWithDataOfContentType:UTTypePDF
                                              contentSize:size
                                        dataCreationBlock:^NSData* _Nullable(QLPreviewReply* replyToUpdate,
                                                                             NSError* __autoreleasing* error) {
                                          NSData* pdf = PreviewPdf(url);
                                          if (!pdf && error) {
                                              *error = CantReadError();
                                          }
                                          return pdf;
                                        }];
    handler(reply, nil);
}
@end
