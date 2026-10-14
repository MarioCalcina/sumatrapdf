/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/JsonParser.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

void JsonTest() {
    // U+2028 / U+2029 must be escaped so the result is safe in a JS string literal
    {
        const char lineSep[] = {'\xE2', '\x80', '\xA8'};
        const char paraSep[] = {'\xE2', '\x80', '\xA9'};
        utassert(str::Eq(json::EscapeStrTemp(Str(lineSep, 3)), StrL("\\u2028")));
        utassert(str::Eq(json::EscapeStrTemp(Str(paraSep, 3)), StrL("\\u2029")));
        utassert(str::Eq(json::EscapeStrTemp(StrL("a\"b\\c\n")), StrL("a\\\"b\\\\c\\n")));
        utassert(str::Eq(json::EscapeStrTemp(StrL("\b\f\r\t")), StrL("\\b\\f\\r\\t")));
    }
}
