/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// the macOS half of Base_posix.cpp: what needs Foundation

#include "base/Base.h"

// Foundation's MacTypes collide with ours, and so does objc's BOOL
#include "base/MacTypesHide.h"
#define BOOL MacObjcBool
#import <Foundation/Foundation.h>
#undef BOOL
#include "base/MacTypesShow.h"

// The languages of System Settings > Language & Region, most preferred first:
// "es-PE", "zh-Hans-CN". An app started from Finder has no $LANG.
void GetUserLanguages(StrVec& out) {
    @autoreleasepool {
        for (NSString* lang in [NSLocale preferredLanguages]) {
            const char* s = lang.UTF8String;
            if (s) {
                out.Append(Str((char*)s));
            }
        }
    }
}
