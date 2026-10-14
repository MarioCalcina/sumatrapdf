/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

void WriteStdout(Str data);
void WriteStdoutLn(Str data);

#define kPathSep "/"
constexpr char kPathSepChar = '/';

namespace path {

bool IsSep(char c);

TempStr GetExtTemp(Str path);
TempStr GetBaseNameTemp(Str path);
TempStr GetPathNoExtTemp(Str path);

TempStr GetDirTemp(Str path);

Str Join(Arena* a, Str dir, Str name);
Str Join(Str dir, Str name);
TempStr JoinTemp(Str dir, Str name, Str name2 = Str());

bool IsDirectory(Str path);

TempStr NormalizeTemp(Str path);
TempStr ToOSTemp(Str path);

bool IsSame(Str path1, Str path2);
bool HasVariableDriveLetter(Str path);
bool IsOnNetworkDrive(Str path);
bool IsOnAvailableDrive(Str path);
bool IsCloudPlaceholder(Str path);
TempStr StorageInfoTemp(Str path);
bool IsEphemeralHostFile(Str path);
bool IsAbsolute(Str path);
bool IsInDir(Str path, Str dir);

bool IsWslUnc(Str path);
bool IsWslMount(Str path);
TempStr WslUncToUnixTemp(Str path);
TempStr WindowsToWslMountTemp(Str path);

bool Match(Str path, Str filter);

enum Type {
    None, // path doesn't exist
    File,
    Dir,
};
Type GetType(Str path);

} // namespace path

TempStr GetTempFilePathTemp(Str filePrefix = Str());
TempStr GetSelfExePathTemp();
TempStr GetSelfExeDirTemp();
TempStr GetPathInExeDirTemp(Str fileName = Str());
TempStr MakeUniqueFilePathTemp(Str path);

namespace file {

using FileHandle = int;
constexpr FileHandle kInvalidFileHandle = -1;

bool Exists(Str path);

FILE* OpenFILE(Str path);

// handle-based i/o, for files kept open across many reads / appends
FileHandle OpenReadWrite(Str path, bool createIfMissing);
void Close(FileHandle);
i64 SeekEnd(FileHandle);
bool WriteAll(FileHandle, Str data);

Str ReadFileWithArena(Str path, Arena*);
Str ReadFile(Str path);
int ReadN(Str path, u8* buf, size_t toRead);
bool WriteFile(Str path, Str);

i64 GetSize(Str path);

// read-only memory-mapped view of an entire file
struct Mapping {
    u8* data = nullptr;
    i64 size = 0;
    FileHandle hFile = kInvalidFileHandle;
    void* hMapping = nullptr;
};
bool MemoryMap(Str path, Mapping*);
void MemoryUnmap(Mapping*);
bool Delete(Str path);
bool DeleteFileToTrash(Str path);

FILETIME GetModificationTime(Str path);

bool SetModificationTime(Str path, FILETIME lastMod);

bool StartsWith(Str path, Str s);

bool DeleteZoneIdentifier(Str path);

// Progress reported by Copy(). bytesTotal == 0 means "total not known".
struct CopyProgress {
    i64 bytesCopied;
    i64 bytesTotal;
};
using CopyProgressCb = Func1<CopyProgress*>;

// Thread-local progress callback honored by long-running copies (e.g.
// caching a network-drive cbx locally). Caller sets it before triggering
// an operation that may copy large files; clears it afterwards.
extern thread_local CopyProgressCb gFileCopyProgressCb;

bool Copy(Str dst, Str src, bool dontOverwrite);
bool Copy(Str dst, Str src, bool dontOverwrite, const CopyProgressCb& cbProgress);
bool Rename(Str newPath, Str oldPath);
bool RenameReplace(Str newPath, Str oldPath);

bool SetAccessTime(Str path, FILETIME accessTime);

} // namespace file

namespace dir {

bool Exists(Str dir);

bool Create(Str dir);
bool CreateForFile(Str path, int* errOut = nullptr);
bool CreateAll(Str dir, int* errOut = nullptr);
bool RemoveAll(Str dir);
bool Empty(Str dir);
bool HasWriteAccess(Str dir);

} // namespace dir

bool FileTimeEq(const FILETIME& a, const FILETIME& b);
int FileTimeDiffInSecs(const FILETIME& ft1, const FILETIME& ft2);
