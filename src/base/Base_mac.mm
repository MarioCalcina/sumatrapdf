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

static TempStr NSStringTemp(NSString* s, Str fallback) {
    const char* u = s ? s.UTF8String : nullptr;
    return u ? str::DupTemp(Str((char*)u)) : str::DupTemp(fallback);
}

// The user's region separators, as orig's LOCALE_STHOUSAND / LOCALE_SDECIMAL:
// "," and "." in en-US, "." and "," in es-ES.
TempStr LocaleThousandSepTemp() {
    @autoreleasepool {
        return NSStringTemp([NSLocale currentLocale].groupingSeparator, StrL(","));
    }
}

TempStr LocaleDecimalSepTemp() {
    @autoreleasepool {
        return NSStringTemp([NSLocale currentLocale].decimalSeparator, StrL("."));
    }
}

// The wall time given, as the user's region writes it ("17/2/26, 2:54:05" in
// es-PE): orig's GetDateFormat / GetTimeFormat.
TempStr FormatLocaleDateTemp(int year, int month, int day, int hour, int minute, int second, DateStyle style) {
    @autoreleasepool {
        NSTimeZone* utc = [NSTimeZone timeZoneForSecondsFromGMT:0];
        NSCalendar* cal = [[NSCalendar alloc] initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
        cal.timeZone = utc;
        NSDateComponents* c = [[NSDateComponents alloc] init];
        c.year = year;
        c.month = month;
        c.day = day;
        c.hour = hour;
        c.minute = minute;
        c.second = second;
        NSDate* date = [cal dateFromComponents:c];
        if (!date) {
            return {};
        }
        NSDateFormatter* f = [[NSDateFormatter alloc] init];
        f.locale = [NSLocale currentLocale];
        f.timeZone = utc;
        f.dateStyle = NSDateFormatterShortStyle;
        f.timeStyle = style == DateStyle::DateTime ? NSDateFormatterMediumStyle : NSDateFormatterNoStyle;
        return NSStringTemp([f stringFromDate:date], {});
    }
}
