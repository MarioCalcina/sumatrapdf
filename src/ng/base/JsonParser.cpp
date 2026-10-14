/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/JsonParser.h"

namespace json {

// Escapes s so it can sit inside a double-quoted JSON string; the surrounding
// quotes are not added. Text that gets pasted into a JSON body routinely
// contains quotes and newlines, and either produces invalid JSON if copied in
// raw.
//
// utf-8 continuation bytes pass through untouched: JSON is utf-8, so multi-byte
// characters need no escaping. Everything below 0x20 must be escaped, and the
// ones without a short form get \u00XX.
//
// Also safe for a double-quoted JavaScript string literal:
// U+2028 LINE SEPARATOR and U+2029 PARAGRAPH SEPARATOR are valid
// unescaped in JSON but are line terminators in JS, so they are escaped here.
TempStr EscapeStrTemp(Str s) {
    str::Builder b;
    int n = len(s);
    for (int i = 0; i < n; i++) {
        u8 c = (u8)s.s[i];
        // U+2028 = e2 80 a8, U+2029 = e2 80 a9
        if (c == 0xE2 && i + 2 < n && (u8)s.s[i + 1] == 0x80 && ((u8)s.s[i + 2] == 0xA8 || (u8)s.s[i + 2] == 0xA9)) {
            b.Append(fmt("\\u%04x", (u8)s.s[i + 2] == 0xA8 ? 0x2028 : 0x2029));
            i += 2;
            continue;
        }
        switch (c) {
            case '"':
                b.Append(StrL("\\\""));
                break;
            case '\\':
                b.Append(StrL("\\\\"));
                break;
            case '\n':
                b.Append(StrL("\\n"));
                break;
            case '\r':
                b.Append(StrL("\\r"));
                break;
            case '\t':
                b.Append(StrL("\\t"));
                break;
            case '\b':
                b.Append(StrL("\\b"));
                break;
            case '\f':
                b.Append(StrL("\\f"));
                break;
            default:
                if (c < 0x20) {
                    b.Append(fmt("\\u%04x", (int)c));
                } else {
                    b.AppendChar((char)c);
                }
                break;
        }
    }
    return ToStrTemp(b);
}

} // namespace json
