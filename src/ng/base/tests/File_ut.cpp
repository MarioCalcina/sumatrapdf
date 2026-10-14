/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include <sys/stat.h>
#include <unistd.h>

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

// The app deletes without asking because DeleteFileToTrash() is undoable. A
// second hard link tells moved (the file lives on in the Trash) from deleted.
// CI only: it leaves the file in the Trash of whoever runs it.
static void DeleteFileToTrashTest() {
    if (!getenv("CI")) {
        return;
    }
    TempStr path = GetTempFilePathTemp(StrL("sumatra-trash-test-"));
    utassert(len(path) > 0);
    utassert(file::WriteFile(path, StrL("x")));
    TempStr link2 = str::JoinTemp(path, StrL(".link"));
    utassert(link(CStrTemp(path), CStrTemp(link2)) == 0);
    utassert(file::DeleteFileToTrash(path));
    utassert(!file::Exists(path));
    struct stat st{};
    utassert(stat(CStrTemp(link2), &st) == 0);
    utassert(st.st_nlink == 2);
    file::Delete(link2);
}

void FileUtilTest() {
    FILETIME epoch = FileTimeFromU64(kFileTimeUnixEpoch);
    FILETIME oneSecond = FileTimeFromU64(kFileTimeUnixEpoch + kFileTimeTicksPerSec);
    utassert(FileTimeToU64(epoch) == kFileTimeUnixEpoch);
    utassert(FileTimeDiffInSecs(oneSecond, epoch) == 1);
    utassert(FileTimeDiffInSecs(epoch, oneSecond) == -1);

    utassert(path::IsInDir(StrL("/tmp/sum17E3.tmp.exe"), StrL("/tmp")));
    utassert(path::IsInDir(StrL("/tmp"), StrL("/tmp")));
    utassert(path::IsInDir(StrL("/tmp/a"), StrL("/tmp/")));
    utassert(!path::IsInDir(StrL("/tmp2/a"), StrL("/tmp")));
    utassert(!path::IsInDir(StrL("/temporary/a"), StrL("/temp")));
    utassert(!path::IsInDir(StrL("/tmp"), Str()));
    utassert(!path::IsInDir(StrL("a"), StrL("/tmp/a")));

    Str path1 = StrL("/Applications/SumatraPDF/SumatraPDF");

    TempStr baseName = path::GetBaseNameTemp(path1);
    utassert(str::Eq(baseName, StrL("SumatraPDF")));

    TempStr dirName = path::GetDirTemp(path1);
    utassert(str::Eq(dirName, StrL("/Applications/SumatraPDF")));
    baseName = path::GetBaseNameTemp(dirName);
    utassert(str::Eq(baseName, StrL("SumatraPDF")));

    dirName = path::GetDirTemp(StrL("/etc"));
    utassert(str::Eq(dirName, StrL("/")));
    dirName = path::GetDirTemp(StrL("file"));
    utassert(str::Eq(dirName, StrL(".")));

    Str path2 = path::Join(StrL("/Applications"), StrL("SumatraPDF"));
    utassert(str::Eq(path2, StrL("/Applications/SumatraPDF")));
    str::Free(path2);
    path2 = path::Join(StrL("/Applications/"), StrL("/SumatraPDF"));
    utassert(str::Eq(path2, StrL("/Applications/SumatraPDF")));
    str::Free(path2);

    utassert(path::Match(StrL("/tmp/file.pdf"), StrL("*.pdf")));
    utassert(path::Match(StrL("/tmp/file.pdf"), StrL("file.*")));
    utassert(path::Match(StrL("/tmp/file.pdf"), StrL("*.xps;*.pdf")));
    utassert(!path::Match(StrL("/tmp/file.pdf"), StrL("*.xps;*.djvu")));

    TempStr path = path::JoinTemp(StrL("foo"), StrL("bar"));
    utassert(str::Eq(path, StrL("foo/bar")));
    path = path::JoinTemp(StrL("foo/"), StrL("/bar"));
    utassert(str::Eq(path, StrL("foo/bar")));
    path = path::JoinTemp(StrL("foo/"), StrL("/bar/"), StrL("/z"));
    utassert(str::Eq(path, StrL("foo/bar/z")));

    Str joined = path::Join(StrL("foo"), StrL("bar"));
    utassert(str::Eq(joined, StrL("foo/bar")));
    str::Free(joined);

    TempStr tempDir = path::GetDirTemp(GetTempFilePathTemp(StrL("local-fs")));
    utassert(!path::IsOnNetworkDrive(tempDir));

    {
        // write a temp file, map it and verify the view matches what was written
        TempStr path = GetTempFilePathTemp(StrL("mmap-test"));
        utassert(len(path) > 0);
        Str content = StrL("file::MemoryMap test content 0123456789");
        bool ok = file::WriteFile(path, content);
        utassert(ok);
        file::Mapping m;
        ok = file::MemoryMap(path, &m);
        utassert(ok);
        utassert(m.size == (i64)len(content));
        utassert(m.data && memcmp(m.data, content.s, (size_t)len(content)) == 0);
        file::MemoryUnmap(&m);
        utassert(!m.data && m.size == 0);
        ok = file::Delete(path);
        utassert(ok);
    }
}

// dir::RemoveAll must remove nested and read-only content and must be safe
// to call from several threads at once (the shutdown WebView profile removal
// races the DeleteStaleFilesAsync sweep)
void DirRemoveAllTest() {
    DeleteFileToTrashTest();
}
