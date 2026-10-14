/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// the macOS half of File_posix.cpp: what needs Foundation

#include "base/Base.h"

// Foundation's MacTypes collide with ours
#include "base/MacTypesHide.h"
#import <Foundation/Foundation.h>
#include "base/MacTypesShow.h"

#include "base/File.h"

namespace file {

// Moves the file to the Trash of its volume, where Finder's Put Back can
// restore it. A file that is already gone counts as deleted, as in Delete().
bool DeleteFileToTrash(Str path) {
    if (len(path) == 0) {
        return false;
    }
    @autoreleasepool {
        NSString* s = [[NSString alloc] initWithBytes:path.s length:(NSUInteger)len(path) encoding:NSUTF8StringEncoding];
        if (!s) {
            return false;
        }
        NSError* err = nil;
        NSURL* url = [NSURL fileURLWithPath:s];
        if ([[NSFileManager defaultManager] trashItemAtURL:url resultingItemURL:nil error:&err]) {
            return true;
        }
        return [err.domain isEqualToString:NSCocoaErrorDomain] && err.code == NSFileNoSuchFileError;
    }
}

} // namespace file
