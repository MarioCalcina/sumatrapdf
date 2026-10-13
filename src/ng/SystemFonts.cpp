/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/MacTypesHide.h"
#include <CoreText/CoreText.h>
#include "base/MacTypesShow.h"

#include "SystemFonts.h"

void GetInstalledFontNames(StrVec& names) {
    CFArrayRef families = CTFontManagerCopyAvailableFontFamilyNames();
    if (!families) {
        return;
    }
    CFIndex count = CFArrayGetCount(families);
    for (CFIndex i = 0; i < count; i++) {
        auto name = (CFStringRef)CFArrayGetValueAtIndex(families, i);
        CFIndex size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(name), kCFStringEncodingUTF8) + 1;
        if (size <= 1 || size > INT_MAX) {
            continue;
        }
        char* buf = AllocArray<char>((int)size);
        if (!buf) {
            continue;
        }
        if (CFStringGetCString(name, buf, size, kCFStringEncodingUTF8)) {
            Str family(buf);
            if (len(family) > 0 && names.FindI(family) < 0) {
                names.Append(family);
            }
        }
        free(buf);
    }
    CFRelease(families);
    SortNoCase(&names);
}
