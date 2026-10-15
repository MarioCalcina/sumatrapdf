/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include <chm.h>

#include "base/Crypto.h"
#include "base/File.h"
#include "base/GuessFileType.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "Flags.h"
#include "EbookBase.h"
#include "ChmFile.h"
#include "ChmDump.h"

static Str ChmEntryKind(const chm_entry* e) {
    return e->is_dir ? StrL("dir") : e->is_file ? StrL("file") : StrL("entry");
}

static Str ChmEntryClass(const chm_entry* e) {
    return e->is_special  ? StrL("special")
           : e->is_meta   ? StrL("meta")
           : e->is_normal ? StrL("normal")
                          : StrL("unknown");
}

struct ChmObjectReadResult {
    uint64_t bytesRead = 0;
    u8 sha1[20]{};
};

static bool ReadChmObject(chm_ctx* ctx, chm_entry* e, ChmObjectReadResult* result) {
    if (e->length == 0) {
        CalcSHA1Digest({}, result->sha1);
        result->bytesRead = 0;
        return true;
    }
    if (e->length > 512ULL * 1024 * 1024) {
        return false; // sanity cap for the dump tool
    }

    size_t n = (size_t)e->length;
    AutoFree<uint8_t> buf((uint8_t*)malloc(n));
    if (!buf) {
        return false;
    }
    int64_t got = chm_read_entry(ctx, e, buf);
    if (got != (int64_t)e->length) {
        result->bytesRead = got > 0 ? (uint64_t)got : 0;
        return false;
    }
    CalcSHA1Digest(Str((char*)buf.Get(), (int)n), result->sha1);
    result->bytesRead = (uint64_t)got;
    return true;
}

struct ChmDumpCtx {
    int entries = 0;
    int files = 0;
    int dirs = 0;
    int unpackFailures = 0;
    uint64_t totalSize = 0;
};

static void ChmDumpEntry(chm_ctx* h, chm_entry* e, ChmDumpCtx* ctx) {
    if (!e->path || e->path[0] == 0) {
        return;
    }

    ctx->entries++;
    if (e->is_file) {
        ctx->files++;
        ctx->totalSize += e->length;
    }
    if (e->is_dir) {
        ctx->dirs++;
    }

    ChmObjectReadResult readResult;
    bool unpacked = true;
    if (e->is_file) {
        unpacked = ReadChmObject(h, e, &readResult);
        if (!unpacked) {
            ctx->unpackFailures++;
        }
    }

    Str sha1Str = StrL("-");
    if (e->is_file && unpacked) {
        sha1Str = str::MemToHexTemp(Str((char*)readResult.sha1, sizeofi(readResult.sha1)));
    }

    Str compression = e->is_compressed ? StrL("compressed") : StrL("uncompressed");
    WriteStdoutLn(fmt("%s class=%s space=%s size=%llu read=%llu sha1=%s status=%s path=%s", ChmEntryKind(e),
                      ChmEntryClass(e), compression, (unsigned long long)e->length,
                      (unsigned long long)readResult.bytesRead, sha1Str, Str(unpacked ? "ok" : "failed"),
                      Str(e->path)));
}

static bool DumpChmFileRaw(Str path) {
    Str data = file::ReadFile(path);
    AutoFree<char> freeData(data.s);
    if (len(data) == 0) {
        WriteStdoutLn(StrL("error: couldn't read file"));
        return false;
    }
    chm_ctx* h = chm_ctx_new(nullptr, nullptr, nullptr, nullptr);
    if (!h || !chm_open(h, (const uint8_t*)data.s, (size_t)data.len)) {
        chm_ctx_free(h);
        WriteStdoutLn(StrL("error: couldn't open CHM"));
        return false;
    }

    chm_entry** entries = nullptr;
    int nEntries = chm_get_entries(h, &entries);

    ChmDumpCtx ctx;
    for (int i = 0; i < nEntries; i++) {
        ChmDumpEntry(h, entries[i], &ctx);
    }
    bool ok = nEntries > 0;
    WriteStdoutLn(fmt("summary entries=%d files=%d dirs=%d total-size=%llu unpack-failures=%d enumerate=%s",
                      ctx.entries, ctx.files, ctx.dirs, (unsigned long long)ctx.totalSize, ctx.unpackFailures,
                      Str(ok ? "ok" : "failed")));

    chm_ctx_free(h);
    return ok && ctx.unpackFailures == 0;
}

static void DumpChmFileMetadata(Str path) {
    AutoDelete<ChmFile> doc(ChmFile::CreateFromFile(path));
    if (!doc) {
        WriteStdoutLn(StrL("metadata: unavailable"));
        return;
    }
    WriteStdoutLn(fmt("metadata title=%s", doc->title));
    WriteStdoutLn(fmt("metadata creator=%s", doc->creator));
    WriteStdoutLn(fmt("metadata home=%s", doc->homePath));
    WriteStdoutLn(fmt("metadata toc=%s", doc->tocPath));
    WriteStdoutLn(fmt("metadata index=%s", doc->indexPath));
    WriteStdoutLn(fmt("metadata codepage=%u", doc->codepage));

    EbookToc toc;
    if (!doc->ParseToc(toc) || len(toc.items) == 0) {
        WriteStdoutLn(StrL("toc: none"));
    }
    for (EbookTocItem& it : toc.items) {
        WriteStdoutLn(fmt("toc level=%d name=%s url=%s", it.level, it.name, it.url));
    }
    EbookToc index;
    if (!doc->ParseIndex(index) || len(index.items) == 0) {
        WriteStdoutLn(StrL("index: none"));
    }
    for (EbookTocItem& it : index.items) {
        WriteStdoutLn(fmt("index level=%d name=%s url=%s", it.level, it.name, it.url));
    }
}

// Dump CHM metadata, file table, and TOC/index information to stdout.
// Returns 0 if every requested CHM opened, enumerated, and unpacked successfully.
int DumpChm(const Flags& flags) {
    if (len(flags.fileNames) == 0) {
        WriteStdoutLn(StrL("No file specified for -dump-chm"));
        return 1;
    }

    bool ok = true;
    for (Str path : flags.fileNames) {
        WriteStdoutLn(fmt("chm path=%s", path));
        ok &= DumpChmFileRaw(path);
        DumpChmFileMetadata(path);
        WriteStdoutLn(StrL("end"));
    }
    return ok ? 0 : 1;
}

extern "C" int LZX_test_pretree_make_decode_table(void);

// entries larger than this aren't read by ChmLzxCheck
constexpr u64 kMaxCheckedEntry = 128ULL * 1024 * 1024;

// Decompresses every entry, so ASan sees an LZX overflow in any of them.
static void ReadAllChmEntries(chm_ctx* h, str::Builder& out) {
    chm_entry** entries = nullptr;
    int nEntries = chm_get_entries(h, &entries);
    chm_entry* payload = nullptr;
    int readOk = 0;
    int readFail = 0;
    for (int i = 0; i < nEntries; i++) {
        chm_entry* e = entries[i];
        if (e->path && str::Eq(Str(e->path), StrL("/payload"))) {
            payload = e;
        }
        if (e->length == 0 || e->length > kMaxCheckedEntry) {
            continue;
        }
        AutoFree<uint8_t> buf((uint8_t*)malloc((size_t)e->length));
        if (buf && chm_read_entry(h, e, buf) == (int64_t)e->length) {
            readOk++;
        } else {
            readFail++;
        }
    }

    if (!payload) {
        out.Append(StrL("payload_retrieve=NOTFOUND\n"));
    } else if (payload->length == 0 || payload->length > kMaxCheckedEntry) {
        out.Append(StrL("payload_retrieve=FAILED\n"));
    } else {
        // chm_read_entry fills the whole entry: the buffer must hold length bytes
        AutoFree<uint8_t> buf((uint8_t*)malloc((size_t)payload->length));
        int64_t got = buf ? chm_read_entry(h, payload, buf) : 0;
        out.Append(got > 0 ? StrL("payload_retrieve=ATTEMPTED\n") : StrL("payload_retrieve=FAILED\n"));
    }
    out.Append(fmt("paths=%d retrieve_ok=%d retrieve_fail=%d\n", nEntries, readOk, readFail));
}

// What tests/issue-chm-lzx.ts checks: chmdec's PRETREE make_decode_table on its
// own (a heap buffer, so ASan catches the lzx overflow), then every entry of the
// file and ChmFile. One line per step; false when a step failed.
bool ChmLzxCheck(Str chmPath, str::Builder& out) {
    bool ok = true;
    int pretree = LZX_test_pretree_make_decode_table();
    if (pretree == 1) {
        out.Append(StrL("pretree_isolated=REJECTED\n"));
    } else {
        out.Append(fmt("pretree_isolated=UNEXPECTED_%d\n", pretree));
        ok = false;
    }

    Str data = file::ReadFile(chmPath);
    AutoFree<char> freeData(data.s);
    chm_ctx* h = len(data) > 0 ? chm_ctx_new(nullptr, nullptr, nullptr, nullptr) : nullptr;
    if (!h || !chm_open(h, (const uint8_t*)data.s, (size_t)data.len)) {
        out.Append(fmt("chm_open=FAILED path=%s\n", chmPath));
        ok = false;
    } else {
        out.Append(StrL("chm_open=OK\n"));
        ReadAllChmEntries(h, out);
    }
    chm_ctx_free(h);

    ChmFile* doc = ChmFile::CreateFromFile(chmPath);
    if (!doc) {
        out.Append(StrL("chmfile=FAILED\n"));
        return ok;
    }
    StrVec allPaths;
    doc->GetAllPaths(&allPaths);
    out.Append(fmt("chmfile=OK\nchmfile_paths=%d\n", len(allPaths)));
    if (len(doc->tocPath) > 0) {
        out.Append(StrL("chmfile_toc=YES\n"));
    }
    delete doc;
    return ok;
}
