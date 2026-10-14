/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include <curl/curl.h>

#include "base/File.h"
#include "base/Http.h"

// libcurl; the portable half of the API is in Http.cpp.

static constexpr u32 kHttpErrorSuccess = 0;
static constexpr u32 kHttpErrorFailure = 1;

static constexpr long kHttpTimeoutSecs = 30;

static size_t AppendResponse(char* data, size_t size, size_t count, void* userData) {
    size_t n = size * count;
    if (n > INT_MAX) {
        return 0;
    }
    auto* out = (str::Builder*)userData;
    return out->Append(Str(data, (int)n)) ? n : 0;
}

struct DownloadState {
    FILE* file = nullptr;
    const Func1<HttpProgress*>* cbProgress = nullptr;
    i64 maxSize = -1;
    HttpProgress progress{};
};

static size_t WriteDownload(char* data, size_t size, size_t count, void* userData) {
    size_t n = size * count;
    auto* state = (DownloadState*)userData;
    if (n > INT64_MAX || (state->maxSize >= 0 && state->progress.nDownloaded > state->maxSize - (i64)n)) {
        return 0;
    }
    size_t written = fwrite(data, 1, n, state->file);
    if (written != n) {
        return written;
    }
    state->progress.nDownloaded += (i64)n;
    state->cbProgress->Call(&state->progress);
    return written;
}

static void SetCurlDefaults(CURL* curl, Str url) {
    curl_easy_setopt(curl, CURLOPT_URL, CStrTemp(url));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, kHttpTimeoutSecs);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "SumatraPdfHTTP");
}

static curl_slist* AppendHeaders(curl_slist* list, Str headers) {
    int start = 0;
    for (int i = 0; i <= len(headers); i++) {
        if (i < len(headers) && headers.s[i] != '\n' && headers.s[i] != '\r') {
            continue;
        }
        if (i > start) {
            list = curl_slist_append(list, CStrTemp(Str(headers.s + start, i - start)));
        }
        if (i < len(headers) && headers.s[i] == '\r' && i + 1 < len(headers) && headers.s[i + 1] == '\n') {
            i++;
        }
        start = i + 1;
    }
    return list;
}

static bool PerformRequest(CURL* curl, HttpRsp* rspOut) {
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, AppendResponse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rspOut->data);
    CURLcode code = curl_easy_perform(curl);
    rspOut->error = (u32)code;
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    rspOut->httpStatusCode = (u32)status;
    return code == CURLE_OK;
}

static bool PostUrl(Str url, Str headers, Str body, HttpRsp* rspOut) {
    rspOut->data.Reset();
    rspOut->error = kHttpErrorSuccess;
    rspOut->httpStatusCode = 0;
    CURL* curl = curl_easy_init();
    if (!curl) {
        rspOut->error = kHttpErrorFailure;
        return false;
    }

    SetCurlDefaults(curl, url);
    curl_slist* headerList = AppendHeaders(nullptr, headers);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.s ? body.s : "");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)len(body));
    bool ok = PerformRequest(curl, rspOut);
    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);
    return ok && IsHttpRspOk(rspOut);
}

bool HttpGetToFile(Str urlA, Str destFilePath, const Func1<HttpProgress*>& cbProgress, i64 maxSize) {
    logf("HttpGetToFile: url: '%s', file: '%s'\n", urlA, destFilePath);
    FILE* file = fopen(CStrTemp(destFilePath), "wb");
    if (!file) {
        return false;
    }
    CURL* curl = curl_easy_init();
    if (!curl) {
        fclose(file);
        file::Delete(destFilePath);
        return false;
    }

    DownloadState state{file, &cbProgress, maxSize};
    SetCurlDefaults(curl, urlA);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteDownload);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &state);
    CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    bool ok = fclose(file) == 0 && code == CURLE_OK && status == 200;
    if (!ok) {
        file::Delete(destFilePath);
    }
    return ok;
}

bool HttpPostUrl(Str url, Str contentType, Str extraHeaders, Str body, HttpRsp* rspOut) {
    str::Builder headers;
    if (len(contentType) > 0) {
        headers.Append(fmt("Content-Type: %s", contentType));
    }
    TempStr extra = HttpNormalizeHeadersTemp(extraHeaders);
    if (len(extra) > 0) {
        if (len(headers) > 0) {
            headers.Append(StrL("\r\n"));
        }
        headers.Append(extra);
    }
    return PostUrl(url, ToStr(headers), body ? body : Str(), rspOut);
}

