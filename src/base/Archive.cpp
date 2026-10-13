/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "base/File.h"
#include "base/GuessFileType.h"

#include "libarchive/archive.h"
#include "libarchive/archive_entry.h"

// TODO: set include path to ext/ dir
#include <locale.h>
#include "base/Archive.h"

// we pad data read with 3 zeros for convenience. That way returned
// data is a valid null-terminated string or WCHAR*.
// 3 is for absolute worst case of WCHAR* where last char was partially written
constexpr int kZeroPaddingCount = 3;

thread_local ArchiveExtractProgressCb gArchiveProgressCb{};

Archive::Archive() {
    a = ArenaNew();
}

static Archive::Format FormatFromArchive(struct archive* a) {
    // archive_format returns a bitmask; the high bits identify the family
    // clang-format off
static const struct {
    int mask;
    Archive::Format format;
} kFormats[] = {
    {ARCHIVE_FORMAT_ZIP, Archive::Format::Zip},   {ARCHIVE_FORMAT_RAR, Archive::Format::Rar},
    {ARCHIVE_FORMAT_RAR_V5, Archive::Format::Rar}, {ARCHIVE_FORMAT_7ZIP, Archive::Format::SevenZip},
    {ARCHIVE_FORMAT_TAR, Archive::Format::Tar},
};
    // clang-format on
    int fmt = archive_format(a);
    for (auto& f : kFormats) {
        if ((fmt & f.mask) == f.mask) {
            return f.format;
        }
    }
    return Archive::Format::Unknown;
}

Archive::~Archive() {
    for (auto& fi : fileInfos_) {
        free((void*)fi->data);
    }
    str::Free(archivePath_);
    str::Free(archiveData_);
    str::Free(password);
    ArenaDelete(a);
}

static void EagerLoadEntry(struct archive* a, Archive::FileInfo* fileInfo) {
    int size = fileInfo->fileSizeUncompressed;
    if (size <= 0) {
        return;
    }
    fileInfo->data = AllocArray<char>(size + kZeroPaddingCount);
    if (!fileInfo->data) {
        fileInfo->failed = true; // OOM
        return;
    }
    la_ssize_t n = archive_read_data(a, (void*)fileInfo->data, (size_t)size);
    if (n >= 0 && (int)n == size) {
        return;
    }
    free(fileInfo->data);
    fileInfo->data = nullptr;
    fileInfo->failed = true;
}

// final progress callback, now that the total is known
static void ReportAllDecoded(const ArchiveExtractProgressCb& cb, ArchiveExtractProgress& prog,
                             Vec<Archive::FileInfo*>& infos, int nDecoded) {
    if (nDecoded == 0) {
        return;
    }
    prog.fileInfo = infos[nDecoded - 1];
    prog.nDecoded = nDecoded;
    prog.nTotal = nDecoded;
    cb.Call(&prog);
}

bool Archive::ParseEntries(struct archive* a, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress) {
    constexpr i64 kMaxEagerArchiveSize = 256LL * 1024 * 1024;
    constexpr int kMaxArchiveEntries = 100'000;
    struct archive_entry* entry;
    int fileId = 0;
    i64 eagerSize = 0;
    ArchiveExtractProgress prog{};
    prog.nTotal = -1; // libarchive streams; total is only known at end
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        if (fileId >= kMaxArchiveEntries) {
            return false;
        }
        Str entryName;
        const char* nameZ = archive_entry_pathname_utf8(entry);
        if (nameZ) {
            entryName = Str(nameZ);
        } else {
            nameZ = archive_entry_pathname(entry);
            entryName = nameZ ? Str(nameZ) : Str{};
        }
        i64 entrySize = archive_entry_size(entry);
        if (entrySize < 0 || entrySize > INT_MAX) {
            return false;
        }
        if (eagerLoad) {
            eagerSize += entrySize;
            if (eagerSize > kMaxEagerArchiveSize) {
                return false;
            }
        }
        FileInfo* i = AllocArray<FileInfo>(this->a);
        i->fileId = fileId;
        i->fileSizeUncompressed = (int)entrySize;
        i->filePos = (i64)fileId; // use fileId as position identifier
        i->fileTime = (i64)archive_entry_mtime(entry);
        i->name = str::Dup(this->a, entryName);
        i->isDir = (archive_entry_filetype(entry) == AE_IFDIR);
        i->data = nullptr;
        VecAppend(fileInfos_, i);

        if (!eagerLoad) {
            archive_read_data_skip(a);
        } else {
            EagerLoadEntry(a, i);
        }
        fileId++;
        prog.fileInfo = i;
        prog.nDecoded = fileId;
        cbProgress.Call(&prog);
    }
    ReportAllDecoded(cbProgress, prog, fileInfos_, fileId);
    return fileId > 0;
}

// unfortunately libarchive's rar support is weak, so rar goes to unrar.dll first
static bool TryOpenUnrarFallback(Archive* archive, Str path, bool eagerLoad,
                                 const ArchiveExtractProgressCb& cbProgress) {
    bool ok = archive->OpenUnrarFallback(path, eagerLoad, cbProgress);
    if (ok) {
        archive->format = Archive::Format::Rar;
    }
    return ok;
}

// hintType is the result of a prior GuessFileTypeFromData() done
// by the caller. When not Unknown we skip the internal 2 KiB sniff and
// use it to drive rar-first vs. libarchive routing.
// eagerLoad = true: decompress every entry at open time and close
//   the archive so no re-open will ever happen.
// cbProgress fires after each entry is processed (see
// ArchiveExtractProgress). Pass a default-constructed Func1 to skip
// notifications.
bool Archive::Open(Str path, bool eagerLoad, FileType hintType, const ArchiveExtractProgressCb& cbProgress) {
    if (len(path) == 0) {
        return false;
    }
    FileType ft = hintType;
    if (ft == FileType::Unknown) {
        // No pre-sniffed hint: peek at the first 2 KiB ourselves so we can
        // route RAR files through unrar.dll instead of libarchive.
        char buf[2048 + 1]{};
        int n = file::ReadN(path, (u8*)buf, dimof(buf) - 1);
        if (n > 0) {
            Str d = Str((char*)buf, n);
            ft = GuessFileTypeFromData(d);
        }
    }

    // tar archives can't seek, so we have to eager-load even when the
    // caller didn't ask for it.
    if (ft == FileType::Tar) {
        eagerLoad = true;
    }

    bool ok = false;
    if (ft == FileType::Rar) {
        ok = TryOpenUnrarFallback(this, path, eagerLoad, cbProgress);
    }
    if (!ok) {
        ok = OpenArchive(path, eagerLoad, cbProgress);
    }
    if (!ok) {
        return false;
    }
    if (eagerLoad) {
        // Discard the paths so ReadEntry
        // can't re-open the archive to fetch a missing entry. Entries whose
        // decompression failed above have failed=true and data=nullptr, and
        // later GetFileDataById will see archivePath_==nullptr and mark
        // the entry as failed.
        str::Free(archivePath_);
        archivePath_ = {};
        rarFilePath_ = {}; // arena-allocated; don't free
    }
    return true;
}

static void SetArchivePassword(struct archive* a, Str password) {
    if (password) {
        archive_read_add_passphrase(a, CStrTemp(password));
    }
}

static int ArchiveReadOpenFilename(struct archive* a, Str path) {
    return archive_read_open_filename(a, CStrTemp(path), 10240);
}

static struct archive* NewLibarchiveReader(Str password) {
    static const char* locale = setlocale(LC_CTYPE, "");
    (void)locale;
    struct archive* a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    SetArchivePassword(a, password);
    return a;
}

// Open path for on-demand extraction. Retry once: after sleep or a brief
// SMB disconnect the first open often fails and the second reconnects.
static struct archive* OpenLibarchiveFile(Str path, Str password) {
    struct archive* a = NewLibarchiveReader(password);
    if (ArchiveReadOpenFilename(a, path) == ARCHIVE_OK) {
        return a;
    }
    archive_read_free(a);
    a = NewLibarchiveReader(password);
    if (ArchiveReadOpenFilename(a, path) == ARCHIVE_OK) {
        return a;
    }
    archive_read_free(a);
    return nullptr;
}

static struct archive* OpenLibarchiveMemory(Str data, Str password) {
    if (len(data) == 0) {
        return nullptr;
    }
    struct archive* a = NewLibarchiveReader(password);
    if (archive_read_open_memory(a, data.s, (size_t)data.len) == ARCHIVE_OK) {
        return a;
    }
    archive_read_free(a);
    return nullptr;
}

static struct archive* OpenLibarchiveSource(Archive* ar) {
    if (ar->archiveData_) {
        return OpenLibarchiveMemory(ar->archiveData_, ar->password);
    }
    if (ar->archivePath_) {
        return OpenLibarchiveFile(ar->archivePath_, ar->password);
    }
    return nullptr;
}

// eagerLoad true (the default, used by ebooks): decompress every entry now,
// then drop the source bytes. False: keep archiveData_ and extract pages
// later, same as opening a file with lazy load.
bool Archive::OpenFromData(Str data, bool eagerLoad) {
    if (len(data) == 0) {
        return false;
    }

    str::Free(archiveData_);
    archiveData_ = str::Dup(data);
    if (len(archiveData_) == 0) {
        return false;
    }

    // tar archives can't seek, so we have to eager-load even when the
    // caller didn't ask for it.
    FileType ft = GuessFileTypeFromData(archiveData_);
    if (ft == FileType::Tar || ft == FileType::Cbt) {
        eagerLoad = true;
    }

    struct archive* a = OpenLibarchiveMemory(archiveData_, password);
    if (!a) {
        str::Free(archiveData_);
        archiveData_ = {};
        return false;
    }
    ArchiveExtractProgressCb emptyCb;
    bool ok = ParseEntries(a, eagerLoad, emptyCb);
    if (ok) {
        format = FormatFromArchive(a);
    }
    if (archive_read_has_encrypted_entries(a) > 0) {
        isEncrypted = true;
    }
    archive_read_free(a);
    if (!ok || eagerLoad) {
        str::Free(archiveData_);
        archiveData_ = {};
    }
    return ok;
}

bool Archive::OpenArchive(Str path, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress) {
    struct archive* a = NewLibarchiveReader(password);
    int r = ArchiveReadOpenFilename(a, path);
    if (r != ARCHIVE_OK) {
        archive_read_free(a);
        return false;
    }
    archivePath_ = str::Dup(path);
    bool ok = ParseEntries(a, eagerLoad, cbProgress);
    if (ok) {
        format = FormatFromArchive(a);
    }
    if (archive_read_has_encrypted_entries(a) > 0) {
        isEncrypted = true;
    }
    archive_read_free(a);
    return ok;
}

Vec<Archive::FileInfo*> const& Archive::GetFileInfos() {
    return fileInfos_;
}

static int getFileIdByName(Vec<Archive::FileInfo*>& fileInfos, Str name) {
    for (auto* fileInfo : fileInfos) {
        if (str::EqI(fileInfo->name, name)) {
            return fileInfo->fileId;
        }
    }
    return -1;
}

int Archive::GetFileId(Str fileName) {
    return getFileIdByName(fileInfos_, fileName);
}

// Return the FileInfo record for a given entry, loading its data into
// fileInfo->data on demand (on a miss, re-opens the archive unless
// that was disabled by eager-load mode).
//
// Ownership: the returned FileInfo* is owned by this archive. By
// default fileInfo->data is *not* transferred to the caller — a later
// call for the same entry returns the same cached buffer, and the
// archive destructor frees it. If the caller wants the buffer to
// outlive the archive, they should set fileInfo->data = nullptr after
// saving the pointer; they then become responsible for free()ing it.
//
// Returns nullptr for an unknown name / out-of-range fileId. For an
// entry whose decompression failed check fileInfo->failed — data will
// be nullptr in that case.
Archive::FileInfo* Archive::GetFileDataByName(Str fileName) {
    int fileId = getFileIdByName(fileInfos_, fileName);
    return GetFileDataById(fileId);
}

// Returns the FileInfo whose ->data field holds decompressed bytes (or
// nullptr / ->failed if extraction failed). The buffer stays owned by
// this archive; callers that want to keep the data past the archive's
// lifetime should set ->data = nullptr to transfer ownership.
// Walks the libarchive entries to fi and decompresses its first toRead bytes.
// Returns {} on failure; *permanent is set when retrying can't help
// (transient I/O failures leave it false so a later call retries).
Str Archive::ReadLibarchiveEntry(FileInfo* fi, int toRead, bool* permanent) {
    *permanent = false;
    struct archive* a = OpenLibarchiveSource(this);
    if (!a) {
        *permanent = len(archivePath_) == 0 && len(archiveData_) == 0;
        return {};
    }
    AutoCall freeArchive(archive_read_free, a);
    if (addOverflows<int>(toRead, kZeroPaddingCount)) {
        *permanent = true;
        return {};
    }

    struct archive_entry* entry;
    for (int idx = 0; archive_read_next_header(a, &entry) == ARCHIVE_OK; idx++) {
        if (idx != fi->fileId) {
            archive_read_data_skip(a);
            continue;
        }
        u8* data = AllocArray<u8>(toRead + kZeroPaddingCount);
        if (!data) {
            return {}; // OOM: retry later
        }
        la_ssize_t n = archive_read_data(a, data, (size_t)toRead);
        if (n < 0) {
            free(data);
            return {}; // I/O error: retry later
        }
        if (toRead == fi->fileSizeUncompressed && (int)n != toRead) {
            free(data);
            *permanent = true; // truncated / corrupt entry
            return {};
        }
        return Str((char*)data, (int)n);
    }
    *permanent = true; // no such entry
    return {};
}

// first toRead bytes of an entry, through whichever library opened the archive
Str Archive::ReadEntry(FileInfo* fi, int toRead, bool* permanent) {
    if (LoadedUsingUnrarDll()) {
        return ReadUnrarEntry(fi, toRead, permanent);
    }
    return ReadLibarchiveEntry(fi, toRead, permanent);
}

Archive::FileInfo* Archive::GetFileDataById(int fileId) {
    if (fileId < 0) {
        return nullptr;
    }
    ReportIf(fileId >= len(fileInfos_));
    auto* fi = fileInfos_[fileId];
    ReportIf(fi->fileId != fileId);
    if (fi->data || fi->failed) {
        return fi; // cached, or already tried
    }
    bool permanent;
    Str d = ReadEntry(fi, fi->fileSizeUncompressed, &permanent);
    fi->data = d.s;
    if (!d.s && permanent) {
        fi->failed = true;
    }
    return fi;
}

Str Archive::GetFileDataPartById(int fileId, int sizeHint) {
    if (fileId < 0) {
        return {};
    }
    ReportIf(fileId >= len(fileInfos_));
    auto* fi = fileInfos_[fileId];
    int n = std::min(fi->fileSizeUncompressed, sizeHint);
    // if full data is cached, return a copy of the prefix
    if (fi->data) {
        u8* data = AllocArray<u8>(n + kZeroPaddingCount);
        if (!data) {
            return {};
        }
        memcpy(data, fi->data, (size_t)n);
        return Str((char*)data, n);
    }
    bool permanent;
    return ReadEntry(fi, n, &permanent);
}

///// format specific handling /////

// Open a file on disk. Archive::Open(path) detects RAR via a
// content sniff and routes it through unrar.dll; everything else goes
// through libarchive.
//
// eagerLoad: if true, every file is decompressed during Open() and the
// archive is then closed. GetFileDataById for a file that failed to
// decompress returns FileInfo with data=nullptr and never re-opens the
// file; use FileInfo::failed to tell "not yet loaded" from "failed".
// cbProgress fires once per entry (see ArchiveExtractProgress); pass a
// default-constructed Func1 to skip notifications.
Archive* OpenArchiveFromFile(Str path, bool eagerLoad, const ArchiveExtractProgressCb& cbProgress) {
    auto* archive = new Archive();
    if (!archive->Open(path, eagerLoad, FileType::Unknown, cbProgress)) {
        delete archive;
        return nullptr;
    }
    return archive;
}

// Open from in-memory data. libarchive auto-detects the container (zip/rar/
// 7z/tar/etc.). Eager-loads by default (ebooks want every member now).
Archive* OpenArchiveFromData(Str data) {
    auto* archive = new Archive();
    if (!archive->OpenFromData(data)) {
        delete archive;
        return nullptr;
    }
    return archive;
}

Str Archive::ReadUnrarEntry(FileInfo*, int, bool* permanent) {
    *permanent = true;
    return {};
}

bool Archive::OpenUnrarFallback(Str, bool, const ArchiveExtractProgressCb&) {
    return false;
}

FILETIME Archive::FileInfo::GetWinFileTime() const {
    if (fileTime < 0) {
        return {(DWORD)-1, (DWORD)-1};
    }
    u64 ticks = kFileTimeUnixEpoch + (u64)fileTime * kFileTimeTicksPerSec;
    return FileTimeFromU64(ticks);
}
