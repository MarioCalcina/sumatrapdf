/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/StrQueue.h"

#include "base/DirScan.h"

void AdvanceDirIter(DirIter::iterator* it, int n);
void CloseDirIter(DirIter::iterator* it);

DirIter::DirIter(Str dir) : dir(dir) {}

DirIter::iterator::iterator(const DirIter* di, bool didFinish) {
    this->di = di;
    this->dirsToVisit.Append(di->dir);
    this->didFinish = didFinish;
    AdvanceDirIter(this, 1);
}

DirIter::iterator::~iterator() {
    CloseDirIter(this);
}

DirIter::iterator DirIter::begin() const {
    return {this, false};
}

DirIter::iterator DirIter::end() const {
    return {this, true};
}

DirIterEntry* DirIter::iterator::operator*() {
    if (didFinish) {
        return nullptr;
    }
    return &data;
}

DirIter::iterator& DirIter::iterator::operator++() {
    AdvanceDirIter(this, 1);
    return *this;
}

bool operator!=(const DirIter::iterator& a, const DirIter::iterator& b) {
    return (a.di != b.di) || (a.didFinish != b.didFinish);
};

i64 GetFileSize(DirIterEntry* de) {
    return de ? de->size : 0;
}

bool IsDirectory(DirIterEntry* de) {
    return de && de->isDir;
}

bool IsRegularFile(DirIterEntry* de) {
    return de && de->isFile;
}

struct DirTraverseThreadData {
    StrQueue* queue = nullptr; // we don't own it
    Str dir;
    bool recurse = false;
    ~DirTraverseThreadData() { str::Free(dir); }
};

static void DirTraverseThread(DirTraverseThreadData* td) {
    DirIter di(td->dir);
    di.includeFiles = true;
    di.includeDirs = false;
    di.recurse = td->recurse;
    for (DirIterEntry* de : di) {
        td->queue->append(de->filePath);
    }
    td->queue->MarkFinished();
    delete td;
}

void StartDirTraverseAsync(StrQueue* queue, Str dir, bool recurse) {
    auto* td = new DirTraverseThreadData{queue, str::Dup(dir), recurse};
    auto fn = MkFunc0(DirTraverseThread, td);
    RunAsync(fn, StrL("DirTraverseThread"));
}
