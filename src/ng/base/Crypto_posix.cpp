/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Crypto.h"

#include "base/MacTypesHide.h"
#include <Security/CMSDecoder.h>
#include "base/MacTypesShow.h"

#include <CommonCrypto/CommonDigest.h>

void CalcMD5Digest(Str data, u8 digest[16]) {
    CC_MD5(data.s, (CC_LONG)data.len, digest);
}

void CalcSHA1Digest(Str data, u8 digest[20]) {
    CC_SHA1(data.s, (CC_LONG)data.len, digest);
}

void CalcSHA2Digest(Str data, u8 digest[32]) {
    CC_SHA256(data.s, (CC_LONG)data.len, digest);
}

// Extracts the embedded content from a signed PKCS#7 / .p7m wrapper.
Str ExtractP7m(Str d) {
    if (len(d) == 0) {
        return {};
    }
    CMSDecoderRef decoder = nullptr;
    OSStatus status = CMSDecoderCreate(&decoder);
    if (status == errSecSuccess) {
        status = CMSDecoderUpdateMessage(decoder, d.s, (size_t)len(d));
    }
    if (status == errSecSuccess) {
        status = CMSDecoderFinalizeMessage(decoder);
    }
    CFDataRef content = nullptr;
    if (status == errSecSuccess) {
        status = CMSDecoderCopyContent(decoder, &content);
    }
    Str out;
    if (status == errSecSuccess && content) {
        CFIndex n = CFDataGetLength(content);
        if (n > 0 && n <= INT_MAX) {
            out = str::Dup(Str((const char*)CFDataGetBytePtr(content), (int)n));
        }
    }
    if (content) {
        CFRelease(content);
    }
    if (decoder) {
        CFRelease(decoder);
    }
    return out;
}

TempStr GetExecutableSignerTemp(Str /*exePath*/) {
    return {};
}
