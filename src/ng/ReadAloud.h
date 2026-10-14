/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class EngineBase;
struct DisplayModel;
struct MainWindow;
struct WindowTab;
struct TextSelection;
struct ReadAloudPlaybackBar;
struct MenuModel;
namespace gpui {
struct Ctx;
struct El;
struct PaintCtx;
} // namespace gpui

// --- text-to-speech backend (WinRT speech synthesis, SAPI 5 fallback) ---

struct TtsVoiceInfo {
    Str id;
    Str name;
    Str lang;
};

bool TtsIsAvailable();
bool TtsSpeakUtf8(Str text);
bool TtsQueueUtf8(Str text);
bool TtsDidStartQueued();
void TtsStop();
void TtsRelease();

bool TtsIsSpeaking();

int TtsGetSpokenPosUtf8();

void TtsProcessEvents();

Vec<TtsVoiceInfo> TtsGetVoices();
void TtsFreeVoices(Vec<TtsVoiceInfo>& voices);

bool TtsSetVoiceById(Str voiceId);
Str TtsGetVoiceId();

void TtsSetSpeed(float speed);
float TtsGetSpeed();

void TtsTestPumpOnNextSpeak();
bool ApplyReadAloudVoiceFromSettings();
void ReadAloudFreeVoiceCache();

// --- highlight of the words being spoken ---

constexpr int kReadAloudHighlightDelayInMs = 80;

// pageLoc, not a flat pageNo: flat numbers shift when chapters re-lay out
struct ReadAloudByteLoc {
    Location pageLoc;
    int x = 0;
    int y = 0;
    int dx = 0;
    int dy = 0;
};

struct ReadAloudHighlightMap {
    int len = 0;
    int cap = 0;
    ReadAloudByteLoc* locs = nullptr;
};

void ReadAloudHighlightFree(ReadAloudHighlightMap* map);

bool ReadAloudHighlightBuildFromTextSelection(TextSelection* ts, ReadAloudHighlightMap* map, str::Builder& cleanedOut);

bool ReadAloudGetViewportStart(DisplayModel* dm, int* startPageOut, int* startGlyphOut);

bool ReadAloudCanReadFromCursor(DisplayModel* dm, Point screenPt);

bool ReadAloudGetCursorStart(DisplayModel* dm, Point screenPt, int* startPageOut, int* startGlyphOut);

bool ReadAloudHighlightBuildFromDocument(DisplayModel* dm, int startPage, int startGlyph, ReadAloudHighlightMap* map,
                                         str::Builder& cleanedOut);

void ReadAloudHighlightTimerStart(MainWindow* win);
void ReadAloudHighlightTimerStop(MainWindow* win);
void ReadAloudTick(MainWindow* win, int elapsedMs);

void ReadAloudOnUserViewChanged(MainWindow* win);
void ReadAloudUpdateAutoScroll(MainWindow* win);

bool ReadAloudGetProgressPage(WindowTab* tab, int* pageOut, int* pageCountOut);

void PaintReadAloudHighlight(MainWindow* win, gpui::PaintCtx* ctx);

bool ReadAloudSentenceRange(Str text, int pos, int* startOut, int* endOut);

bool IsReadAloudLowerAscii(char c);
bool IsReadAloudLineBreak(char c);
bool IsReadAloudHorizontalSpace(char c);
bool IsReadAloudCloser(int c);
int ReadAloudWordEndUtf8(Str text, int pos);
bool ReadAloudByteLocHasRect(const ReadAloudByteLoc& loc);
Rect ReadAloudByteLocToRect(const ReadAloudByteLoc& loc);
void ReadAloudClampVisual(ReadAloudHighlightMap* map, int wordStartAbs, int wordEndAbs, int* startAbs, int* endAbs);
void ReadAloudAppendUnderlines(DisplayModel* dm, Rect canvasRc, ReadAloudHighlightMap* map, int startAbs, int endAbs,
                               int minThick, int thickDiv, Vec<Rect>& out);

// --- playback bar shown over the canvas while reading ---

void ReadAloudPlaybackBarUpdateSession(WindowTab* tab);
void ReadAloudPlaybackBarHide(MainWindow* win);
void ReadAloudPlaybackBarForgetTab(MainWindow* win, WindowTab* tab);
void ReadAloudPlaybackBarTick(MainWindow* win);

void ReadAloudPlaybackPauseOrResume();
void ReadAloudPlaybackStop();
void ReadAloudPlaybackBarDestroy(MainWindow* win);
TempStr ReadAloudPlaybackBarStateTemp(int* exitCodeOut);
TempStr ReadAloudPlaybackBarTestTemp(Str action, int* exitCodeOut);
gpui::El* ReadAloudPlaybackBarBuild(MainWindow* win, gpui::Ctx* cx);

// --- read-aloud session: what to read, chunking, menus ---

constexpr int CmdTtsVoiceDefault = 0x7100;
constexpr int CmdTtsVoiceFirst = 0x7101;
constexpr int CmdTtsVoiceLast = 0x71ff;
constexpr int CmdTtsMenuReadCurrentPage = 0x7200;
constexpr int CmdTtsMenuContinueReading = 0x7201;
constexpr int CmdTtsMenuReadSelection = 0x7202;
constexpr int CmdTtsMenuPauseReading = 0x7203;
constexpr int CmdTtsMenuReadFromCursor = 0x7204;
constexpr int CmdTtsMenuStopReading = 0x7205;
constexpr int CmdTtsSpeedFirst = 0x7300;
constexpr int CmdTtsSpeedLast = 0x730f;

WindowTab* GetReadAloudSourceTab();
void ReadAloudForgetTab(WindowTab*);
void ReadAloudAfterTtsEvents();
bool CanContinueReadAloud(WindowTab* tab);

void ReadAloudInTab(WindowTab* tab);
void ReadAloudContinueInTab(WindowTab* tab);
void ReadAloudSelectionInTab(WindowTab* tab);
void ReadAloudFromViewportTopInTab(WindowTab* tab);
void ReadAloudFromCursorInTab(WindowTab* tab, Point screenPt);
void ReadAloudStopRememberPos();
void ResetReadAloudStateForTab(WindowTab* tab);
void StopReadAloudIfSourceWindow(MainWindow* win);

// handles kWmTtsEvent: advances the session, refreshes highlight and playback bar
void ReadAloudOnTtsEvent(MainWindow* win);

TempStr ReadAloudSpeedLabelTemp(float speed);
int ReadAloudSpeedCount();
float ReadAloudSpeedAt(int idx);
int ReadAloudClosestSpeedIdx();
void ReadAloudSetSpeedIdx(int idx);

void RebuildReadAloudMenu(MainWindow* win, MenuModel* menu, bool includeCursorItem = false,
                          bool canReadFromCursor = false);
bool HandleReadAloudMenuCommand(MainWindow* win, int cmdId);

// --- shared by ReadAloudSession.cpp and each app's ReadAloud.cpp ---

enum class SpeakChunkResult {
    Ok,
    Failed,
    TabGone,
};

extern int gReadAloudPaintLogState;
extern WindowTab* gReadAloudSourceTab;

void ReadAloudPaintLogOnce(int code, Str fmt);
bool ReadAloudGetCurrentWordAbsRange(WindowTab* tab, int* startAbsOut, int* endAbsOut);
bool ReadAloudGetSentenceAbsRange(WindowTab* tab, int wordStartAbs, int wordEndAbs, int* startAbsOut, int* endAbsOut);
bool ReadAloudGetCurrentWordScreenRect(MainWindow* win, Rect* rectOut);
bool ReadAloudIsWordRectFullyVisibleInViewport(MainWindow* win, const Rect& wordRect, int margin);
TempStr ReadAloudPlaybackBarTextTemp(WindowTab* tab);
void ReadAloudSaveVoicePref(Str voiceId);
bool ReadAloudHasMoreChunks(WindowTab* tab);
void ReadAloudFinishSession(WindowTab* tab, MainWindow* win);
SpeakChunkResult ReadAloudSpeakChunk(WindowTab* tab, Str errMsg);
void ReadAloudClearSourceTab();

void ReadAloudShowNotif(WindowTab* tab, Str msg);

// implemented by each app
void ReadAloudSetSpeed(float speed);

TempStr TtsLangIdToLocaleNameTemp(Str lang);
void HandleReadAloudMenuSelection(MainWindow* win, int selected);
ReadAloudPlaybackBar* ReadAloudPlaybackBarEnsure(MainWindow* win);
void ReadAloudBarSetSession(ReadAloudPlaybackBar* bar, WindowTab* tab);
bool ReadAloudBarIsShown(MainWindow* win);
TempStr ReadAloudVoiceIdTemp(int voiceIndex);
