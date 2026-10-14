/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct PageRange {
    int start = 1;
    // end == INT_MAX means to the last page
    int end = INT_MAX;
};

struct FileArgs {
    Str origPath;
    Str cleanPath;

    // page=%d
    int pageNumber = 0;
    // dest=%s
    Str destName;
    // search=%s
    Str search;

    // annotatt=%d
    int annotAttObjNum = 0;

    // attachno=%d
    int attachmentNo = 0;

    ~FileArgs();
};

struct Flags {
    Str printerName;
    Str printSettings;
    Str forwardSearchOrigin;
    Str namedDest;
    Str appdataDir;
    Str inverseSearchCmdLine;
    Str search;
    Str password;
    Str stressTestPath;
    // empty is equivalent to "*" (i.e. all files)
    Str stressTestFilter;
    Str stressTestRanges;
    Str controlPipeName; // -dbg-control <named-pipe>
    Str lang;
    Str logFile;
    StrVec fileNames;
    // pathsToBenchmark contain 2 strings per each file to benchmark:
    // - name of the file to benchmark
    // - optional (nullptr if not available) string that represents which pages
    //   to benchmark. It can also be a string "loadonly" which means we'll
    //   only benchmark loading of the catalog
    StrVec pathsToBenchmark;
    StrVec globalPrefArgs;
    int forwardSearchLine = 0;
    int pageNumber = -1;
    DisplayMode startView = DisplayMode::Automatic;
    float startZoom = kInvalidZoom;
    int stressTestCycles = 1;
    int stressParallelCount = 1;
    int stressTestMax = 0;
    int maxFiles = 0;
    int testPageNo = 0;
    int sleepMs = 0;
    Point startScroll = {-1, -1};
    // -window-pos <width>x<height>@<x>x<y>: open every window exactly there,
    // e.g. 960x540@960x0. Empty unless given. Meant for automated tests, where
    // a window a quarter of the screen renders and captures four times faster
    Rect windowPos;

    bool exitWhenDone = false;
    bool printDialog = false;
    bool restrictedUse = false;
    bool enterPresentation = false;
    bool enterFullScreen = false;
    bool showConsole = false;
    bool exitImmediately = false;
    // -silent: no error UI while printing (-print-to, -print-to-default)
    bool silent = false;
    bool invertColors = false;
    bool regress = false;
    bool tester = false;
    // -new-window: each file in its own new window (not a tab of an existing one)
    bool inNewWindow = false;
    // -new-window-tabs: one new window, all files as tabs in that window
    bool inNewWindowTabs = false;
    // -reuse-instance: the files go to the SumatraPDF already running
    bool reuseInstance = false;
    bool stressRandomizeFiles = false;
    // -for-testing: for ad-hoc testing by humans or agents. Always starts
    // a new instance, doesn't restore session, doesn't save settings
    bool forTesting = false;
    bool testRenderPage = false;
    bool testExtractPage = false;
    bool testApp = false;
    bool dumpExif = false;        // -dump-exif
    bool dumpChm = false;         // -dump-chm
    bool unitTests = false;       // -unit-tests (debug builds only)
    bool regenThumbnails = false; // -regen-thumbnails: re-create the thumbnail cache and exit
    bool forAi = false;           // -for-ai (AI-friendly unit test output)
    bool showPrintersDialog = false;
    bool crashOnOpen = false;
    bool showHelp = false;
    bool log = false;

    Flags() = default;
    ~Flags() = default;
};

void ParseFlagsArgv(Arena* a, int argc, char** argv, Flags&, Str toolNames = {});
void ShowPrintersDialog(bool consoleOnly = false);

bool IsValidPageRange(Str ranges);
bool IsBenchPagesInfo(Str s);
bool ParsePageRanges(Str ranges, Vec<PageRange>& result);
