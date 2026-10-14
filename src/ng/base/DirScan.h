/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Directory iteration (implemented in DirScan*.cpp).
// Not self-sufficient: include after base/Base.h.

struct StrQueue;

struct DirIterEntry {
    Str name;
    Str filePath;
    i64 size = 0;
    FILETIME accessTime{};
    FILETIME modificationTime{};
    bool isDir = false;
    bool isFile = false;
    bool stopTraversal = false;
    bool fileMatches = false;
};

struct DirIter {
    Str dir;
    bool includeFiles = true;
    bool includeDirs = false;
    bool recurse = false;

    DirIter() = default;
    explicit DirIter(Str dir);

    struct iterator {
        const DirIter* di;
        bool didFinish = false;

        StrVec dirsToVisit;
        TempStr currDir;
        void* dirHandle = nullptr;
        DirIterEntry data;

        iterator(const DirIter*, bool);
        iterator(const iterator&) = delete;
        iterator& operator=(const iterator&) = delete;
        ~iterator();

        DirIterEntry* operator*();
        iterator& operator++();   // ++it
        friend bool operator!=(const iterator& a, const iterator& b);
    };
    iterator begin() const;
    iterator end() const;
};

void StartDirTraverseAsync(StrQueue* queue, Str dir, bool recurse);

i64 GetFileSize(DirIterEntry*);
bool IsDirectory(DirIterEntry*);
bool IsRegularFile(DirIterEntry*);
