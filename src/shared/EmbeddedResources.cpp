/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Http.h"
#include "base/LzmaSimpleArchive.h"

#include "Version.h"
#include "mupdf/noto_sumatra.h"

#include "EmbeddedResources.h"

extern "C" const u8 gEmbeddedLzsa[];
extern "C" const u8 gEmbeddedLzsaEnd[];

static lzma::SimpleArchive gEmbeddedArchive{};
static bool gEmbeddedTried = false;

// Returns a read-only view valid for the lifetime of the executable.
Str GetEmbeddedLzsa() {
    return Str((char*)gEmbeddedLzsa, (int)((uintptr_t)gEmbeddedLzsaEnd - (uintptr_t)gEmbeddedLzsa));
}

bool EnsureEmbeddedArchiveLoaded() {
    if (gEmbeddedTried) {
        return gEmbeddedArchive.filesCount > 0;
    }
    gEmbeddedTried = true;
    Str data = GetEmbeddedLzsa();
    if (!lzma::ParseSimpleArchive((const u8*)data.s, len(data), &gEmbeddedArchive)) {
        logf("EnsureEmbeddedArchiveLoaded: ParseSimpleArchive failed (size=%d)\n", len(data));
        gEmbeddedArchive.filesCount = 0;
        return false;
    }
    logf("EnsureEmbeddedArchiveLoaded: %d files in IDR_EMBEDDED_PAK (%d bytes)\n", gEmbeddedArchive.filesCount,
         len(data));
    return gEmbeddedArchive.filesCount > 0;
}

lzma::SimpleArchive* GetEmbeddedArchive() {
    if (!EnsureEmbeddedArchiveLoaded()) {
        return nullptr;
    }
    return &gEmbeddedArchive;
}

// Returns malloc'd, NUL-terminated data; the caller frees it.
u8* GetEmbeddedFileData(Str name, int* outSize) {
    if (outSize) {
        *outSize = 0;
    }
    if (len(name) == 0 || !EnsureEmbeddedArchiveLoaded()) {
        return nullptr;
    }
    if (str::Contains(name, StrL("/"))) {
        name = str::DupTemp(name);
        str::TransCharsInPlace(name, StrL("/"), StrL("\\"));
    }
    int idx = lzma::GetIdxFromName(&gEmbeddedArchive, name);
    u8* data = lzma::GetFileDataByIdx(&gEmbeddedArchive, idx, nullptr);
    if (data && outSize) {
        *outSize = (int)gEmbeddedArchive.files[idx].uncompressedSize;
    }
    return data;
}

struct EmbeddedFont {
    EmbeddedFont* next;
    Str name;
    u8* data;
    int size;
};

static Mutex gFontsMutex;
static EmbeddedFont* gFonts = nullptr;

// ~/Library/Application Support/SumatraPDF/fonts. Not the settings
// directory, which -appdata can move.
static TempStr FontCacheDirTemp() {
    const char* home = getenv("HOME");
    if (home && *home) {
        TempStr dir = path::JoinTemp(Str((char*)home), StrL("Library/Application Support"));
        dir = path::JoinTemp(dir, StrL(kAppName));
        return path::JoinTemp(dir, StrL("fonts"));
    }
    const char* tmp = getenv("TMPDIR");
    TempStr dir = path::JoinTemp(Str((char*)(tmp && *tmp ? tmp : "/tmp")), StrL(kAppName));
    return path::JoinTemp(dir, StrL("fonts"));
}

static Str ReadKeptFont(Str path) {
    Str data = file::ReadFile(path);
    if (len(data) <= 0) {
        str::Free(data);
        return {};
    }
    return data;
}

static bool DownloadFont(Str url, Str dest) {
    if (!dir::CreateForFile(dest)) {
        logf("font cache: cannot create '%s'\n", dest);
        return false;
    }
    TempStr part = str::JoinTemp(dest, StrL(".part"));
    Func1<HttpProgress*> progress;
    if (!HttpGetToFile(url, part, progress)) {
        file::Delete(part);
        return false;
    }
    if (!file::RenameReplace(dest, part)) {
        file::Delete(part);
        return false;
    }
    return true;
}

// Embedded archive first, then the cache file named by the URL, then a download
// into that cache. The bytes are kept for the process: mupdf holds the pointer.
// A miss is remembered too, so a font we cannot get is not retried.
static void LoadFontFromCacheOrNet(Str name, EmbeddedFont* f) {
    const char* url = sumatra_lookup_font_url(CStrTemp(name));
    if (!url || !url[0]) {
        return;
    }
    const char* slash = strrchr(url, '/');
    const char* file = slash ? slash + 1 : url;
    if (!file[0]) {
        return;
    }
    TempStr path = path::JoinTemp(FontCacheDirTemp(), Str(file));
    Str data = ReadKeptFont(path);
    if (len(data) == 0) {
        logf("font '%s': downloading\n", name);
        if (!DownloadFont(Str(url), path)) {
            logf("font '%s': download failed\n", name);
            return;
        }
        data = ReadKeptFont(path);
    }
    f->data = (u8*)data.s;
    f->size = len(data);
}

// mupdf's built-in fonts (src/mupdf/noto_sumatra.c). Each is loaded once.
static const u8* LoadEmbeddedFont(const char* fileName, int* size) {
    Str name(fileName);
    AutoUnlockMutex lock(&gFontsMutex);
    for (EmbeddedFont* f = gFonts; f; f = f->next) {
        if (str::Eq(f->name, name)) {
            *size = f->size;
            return f->data;
        }
    }
    EmbeddedFont* f = AllocStruct<EmbeddedFont>();
    f->name = str::Dup(name);
    f->data = GetEmbeddedFileData(fmt("fonts\\%s", name), &f->size);
    if (!f->data) {
        LoadFontFromCacheOrNet(name, f);
    }
    ListInsertFront(&gFonts, f);
    *size = f->size;
    return f->data;
}

void InstallEmbeddedFontLoader() {
    fz_set_builtin_font_loader(LoadEmbeddedFont);
}
