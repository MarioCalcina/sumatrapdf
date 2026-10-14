/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class ZipCreator {
    str::Builder centraldir;
    str::Builder* zipOut;
    size_t bytesWritten;
    size_t fileCount;

    bool WriteData(const void* data, size_t size);

  public:
    bool AddFileData(Str name, Str data, u32 dosdate = 0);
    explicit ZipCreator(str::Builder& zipOut);

    ZipCreator(ZipCreator const&) = delete;
    ZipCreator& operator=(ZipCreator const&) = delete;

    bool Finish();
};

Str Ungzip(const Str&, int maxSize);
