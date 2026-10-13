/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "base/Http.h"

HttpRsp::~HttpRsp() {
    str::Free(url);
}

bool IsHttpRspOk(const HttpRsp* rsp) {
    if (rsp->error != 0) {
        logf("HttpRspOk: rsp->error %d, should be 0\n", (int)rsp->error);
        return false;
    }
    if (rsp->httpStatusCode >= 300) {
        logf("HttpRspOk: rsp->httpStatusCode: %d\n", (int)rsp->httpStatusCode);
        return false;
    }
    return true;
}

//--- URL encoding

// URL-encode s so it can be substituted into a URL, shortening it if the
// encoded form would not fit in maxEncodedLen characters.
TempStr URLEncodeMayTruncateTemp(Str s, int maxEncodedLen, bool* didTruncateOut) {
    if (maxEncodedLen <= 0) {
        maxEncodedLen = kMaxUrlEncodedLen;
    }
    return url::EncodeMayTruncateTemp(s, maxEncodedLen, didTruncateOut);
}

//--- POST with custom headers (WinHTTP)

// Splits "Name: value\nName2: value2" into the CRLF-separated form http wants.
// Also accepts a literal backslash-n: a value that came from a settings file is
// a single line, so that is how a user writes a separator there.
TempStr HttpNormalizeHeadersTemp(Str headers) {
    if (str::IsEmptyOrWhiteSpace(headers)) {
        return {};
    }
    TempStr normalized = str::ReplaceTemp(headers, StrL("\\n"), StrL("\n"));
    str::Builder b;
    StrVec lines;
    Split(&lines, normalized, StrL("\n"), true);
    for (Str line : lines) {
        Str t = str::DupTemp(line);
        str::TrimWSInPlace(t, str::TrimOpt::Both);
        if (str::IsEmptyOrWhiteSpace(t) || !str::Contains(t, StrL(":"))) {
            continue;
        }
        if (len(b) > 0) {
            b.Append(StrL("\r\n"));
        }
        b.Append(t);
    }
    return ToStrTemp(b);
}
