/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// @gen-start cmd-enum
// clang-format off
enum {
    // commands are integers sent with WM_COMMAND so start them
    // at some number higher than 0
    CmdFirst = 200,
    CmdSeparator = CmdFirst,

    CmdOpenFile = 201,
    CmdClose = 202,
    CmdCloseCurrentDocument = 203,
    CmdCloseOtherTabs = 204,
    CmdCloseTabsToTheRight = 205,
    CmdCloseTabsToTheLeft = 206,
    CmdCloseAllTabs = 207,
    CmdSaveAs = 208,
    CmdPrint = 209,
    CmdShowInFolder = 210,
    CmdRenameFile = 211,
    CmdDeleteFile = 212,
    CmdExit = 213,
    CmdReloadDocument = 214,
    CmdCreateShortcutToFile = 215,
    CmdSendByEmail = 216,
    CmdProperties = 217,
    CmdSinglePageView = 218,
    CmdFacingView = 219,
    CmdBookView = 220,
    CmdToggleContinuousView = 221,
    CmdToggleMangaMode = 222,
    CmdRotateLeft = 223,
    CmdRotateRight = 224,
    CmdToggleBookmarks = 225,
    CmdToggleTableOfContents = 226,
    CmdToggleFullscreen = 227,
    CmdPresentationWhiteBackground = 228,
    CmdPresentationBlackBackground = 229,
    CmdTogglePresentationMode = 230,
    CmdToggleToolbar = 231,
    CmdChangeScrollbar = 232,
    CmdToggleMenuBar = 233,
    CmdCopySelection = 234,
    CmdTranslateSelectionWithGoogle = 235,
    CmdTranslateSelectionWithDeepL = 236,
    CmdSearchSelectionWithGoogle = 237,
    CmdSearchSelectionWithBing = 238,
    CmdSearchSelectionWithWikipedia = 239,
    CmdSearchSelectionWithGoogleScholar = 240,
    CmdSelectAll = 241,
    CmdNewWindow = 242,
    CmdDuplicateInNewWindow = 243,
    CmdDuplicateInNewTab = 244,
    CmdCopyImage = 245,
    CmdCopyLinkTarget = 246,
    CmdCopyComment = 247,
    CmdCopyFilePath = 248,
    CmdScrollUp = 249,
    CmdScrollDown = 250,
    CmdScrollLeft = 251,
    CmdScrollRight = 252,
    CmdScrollLeftPage = 253,
    CmdScrollRightPage = 254,
    CmdScrollUpPage = 255,
    CmdScrollDownPage = 256,
    CmdScrollDownHalfPage = 257,
    CmdScrollUpHalfPage = 258,
    CmdGoToNextPage = 259,
    CmdGoToPrevPage = 260,
    CmdGoToFirstPage = 261,
    CmdGoToLastPage = 262,
    CmdGoToPage = 263,
    CmdFindFirst = 264,
    CmdFindNext = 265,
    CmdFindPrev = 266,
    CmdFindNextSel = 267,
    CmdFindPrevSel = 268,
    CmdFindToggleMatchCase = 269,
    CmdSaveAnnotations = 270,
    CmdSaveAnnotationsNewFile = 271,
    CmdDiscardChanges = 272,
    CmdDeleteAnnotation = 273,
    CmdZoomFitPage = 274,
    CmdZoomActualSize = 275,
    CmdZoomFitWidth = 276,
    CmdZoomFitByOrientation = 277,
    CmdZoom6400 = 278,
    CmdZoom3200 = 279,
    CmdZoom1600 = 280,
    CmdZoom800 = 281,
    CmdZoom400 = 282,
    CmdZoom200 = 283,
    CmdZoom150 = 284,
    CmdZoom125 = 285,
    CmdZoom100 = 286,
    CmdZoom50 = 287,
    CmdZoom25 = 288,
    CmdZoom12_5 = 289,
    CmdZoom8_33 = 290,
    CmdZoomFitContent = 291,
    CmdZoomShrinkToFit = 292,
    CmdZoomCustom = 293,
    CmdZoomIn = 294,
    CmdZoomOut = 295,
    CmdZoomFitWidthAndContinuous = 296,
    CmdZoomFitPageAndSinglePage = 297,
    CmdContributeTranslation = 298,
    CmdOpenSelectedDocument = 301,
    CmdPinSelectedDocument = 302,
    CmdForgetSelectedDocument = 303,
    CmdExpandAll = 304,
    CmdCollapseAll = 305,
    CmdSaveEmbeddedFile = 306,
    CmdOpenEmbeddedPDF = 307,
    CmdSaveAttachment = 308,
    CmdOpenAttachment = 309,
    CmdOptions = 310,
    CmdAdvancedSettings = 312,
    CmdChangeLanguage = 313,
    CmdCheckUpdate = 314,
    CmdInstallPrereleaseUpdate = 315,
    CmdHelpOpenManual = 316,
    CmdHelpOpenManualOnWebsite = 317,
    CmdHelpOpenKeyboardShortcuts = 318,
    CmdToggleKeyboardHelp = 319,
    CmdHelpVisitWebsite = 320,
    CmdHelpAbout = 321,
    CmdMoveFrameFocus = 322,
    CmdFavoriteAdd = 323,
    CmdFavoriteDel = 324,
    CmdFavoriteToggle = 325,
    CmdToggleLinks = 326,
    CmdToggleShowAnnotations = 327,
    CmdShowAnnotations = 328,
    CmdHideAnnotations = 329,
    CmdCreateAnnotText = 330,
    CmdCreateAnnotLink = 331,
    CmdCreateAnnotFreeText = 332,
    CmdCreateAnnotLine = 333,
    CmdCreateAnnotSquare = 334,
    CmdCreateAnnotCircle = 335,
    CmdCreateAnnotPolygon = 336,
    CmdCreateAnnotPolyLine = 337,
    CmdCreateAnnotHighlight = 338,
    CmdCreateAnnotUnderline = 339,
    CmdCreateAnnotSquiggly = 340,
    CmdCreateAnnotStrikeOut = 341,
    CmdCreateAnnotRedact = 342,
    CmdCreateAnnotStamp = 343,
    CmdCreateAnnotCaret = 344,
    CmdCreateAnnotInk = 345,
    CmdCreateAnnotPopup = 346,
    CmdCreateAnnotFileAttachment = 347,
    CmdInvertColors = 348,
    CmdTogglePageInfo = 349,
    CmdToggleZoom = 350,
    CmdNavigateBack = 351,
    CmdNavigateForward = 352,
    CmdToggleCursorPosition = 353,
    CmdOpenNextFileInFolder = 354,
    CmdOpenPrevFileInFolder = 355,
    CmdCommandPalette = 356,
    CmdShowLog = 357,
    CmdShowErrors = 358,
    CmdClearHistory = 359,
    CmdReopenLastClosedFile = 360,
    CmdNextTab = 361,
    CmdPrevTab = 362,
    CmdNextTabSmart = 363,
    CmdPrevTabSmart = 364,
    CmdMoveTabLeft = 365,
    CmdMoveTabRight = 366,
    CmdInvokeInverseSearch = 367,
    CmdExec = 368,
    CmdViewWithExternalViewer = 369,
    CmdSelectionHandler = 370,
    CmdSetTheme = 371,
    CmdToggleInverseSearch = 372,
    CmdDebugCorruptMemory = 373,
    CmdDebugCrashMe = 374,
    CmdDebugShowNotif = 377,
    CmdDebugStartStressTest = 378,
    CmdDebugTogglePredictiveRender = 379,
    CmdDebugToggleRtl = 380,
    CmdListPrinters = 381,
    CmdScreenshot = 382,
    CmdCropImage = 383,
    CmdResizeImage = 384,
    CmdSaveImage = 385,
    CmdPasteClipboardImage = 386,
    CmdTabGroupSave = 387,
    CmdTabGroupRestore = 388,
    CmdChangeBackgroundColor = 389,
    CmdChangeEbookSettings = 390,
    CmdSetTabColor = 391,
    CmdPdfCompress = 392,
    CmdPdfDecompress = 393,
    CmdPdfDeletePages = 394,
    CmdPdfExtractPages = 395,
    CmdPdfEncrypt = 396,
    CmdPdfDecrypt = 397,
    CmdPdfBake = 398,
    CmdPdfShowInfo = 399,
    CmdDocumentExtractText = 400,
    CmdDocumentShowOutline = 401,
    CmdSetScreenshotHotkey = 402,
    CmdToggleReadAloud = 403,
    CmdPauseReadAloud = 404,
    CmdContinueReadAloud = 405,
    CmdStopReadAloud = 406,
    CmdReadAloudFromTopPage = 407,
    CmdReadAloudSelection = 408,
    CmdToggleToolbarShowReadAloud = 409,
    CmdRemoveDeletedFilesFromHistory = 410,
    CmdNavigateTableOfContents = 411,
    CmdDebugToggleRenderInfo = 412,
    CmdConvertImageToPdf = 413,
    CmdExpandToCurrentPage = 414,
    CmdStartAutoScroll = 415,
    CmdAIChatWithClaudeCode = 416,
    CmdAIChatWithGrokBuild = 417,
    CmdAIChatWithOpenAICodex = 418,
    CmdTranslateSelectionWithGrokBuild = 419,
    CmdTranslateSelectionWithClaudeCode = 420,
    CmdTranslateSelectionWithOpenAICodex = 421,
    CmdFindToggleMatchWholeWord = 422,
    CmdGoToNextFavorite = 423,
    CmdGoToPrevFavorite = 424,
    CmdCreateAnnotImageFromClipboard = 425,
    CmdSetInverseSearch = 426,
    CmdNavigateFavorites = 427,
    CmdNavigateFilesInFolder = 428,
    CmdDebugToggleCacheInfo = 429,
    CmdToggleEngineeringDrawingEnhance = 430,
    CmdSetDocumentColorsFollowTheme = 431,
    CmdTogglePreservePdfImages = 432,
    CmdToggleLightDarkTheme = 433,
    CmdChangeTheme = 434,
    CmdTranslateSelection = 435,
    CmdFavoriteShowInTab = 436,
    CmdTocExpandToLevel1 = 437,
    CmdTocExpandToLevel2 = 438,
    CmdTocExpandToLevel3 = 439,
    CmdTocCollapseSameLevel = 440,
    CmdToggleFavoritesSort = 441,
    CmdZoomFitHeight = 442,
    CmdDeleteFileAndOpenNext = 443,
    CmdShowGeneratedHTML = 444,
    CmdDeleteCachedFiles = 445,
    CmdToggleKeyboardLinkFollowing = 446,
    CmdToggleImages = 447,
    CmdSelectTextViaKeyboard = 448,
    CmdOpenFileWithOSFilePicker = 449,
    CmdToggleFilePicker = 450,
    CmdToggleBoolSetting = 451,
    CmdAIChatWithAntiGravity = 452,
    CmdTranslateSelectionWithAntiGravity = 453,
    CmdConvertToPDF = 454,
    CmdDebugShowFitContentArea = 455,
    CmdExtendSelectionCharLeft = 456,
    CmdExtendSelectionCharRight = 457,
    CmdExtendSelectionWordLeft = 458,
    CmdExtendSelectionWordRight = 459,
    CmdToggleLaserPointer = 460,
    CmdZoomToSelection = 461,
    CmdToggleHoverPreview = 462,
    CmdToggleDisableLinks = 463,
    CmdSignDocument = 464,
    CmdInsertImage = 465,
    CmdToggleHighlightFormFields = 466,
    CmdTogglePageBoxes = 467,
    CmdConvertPdfToImages = 468,
    CmdToggleUniformPageWidth = 469,
    CmdToggleTransparencyGrid = 470,
    CmdTogglePageGrid = 471,
    CmdConfigurePageGrid = 472,
    CmdToggleEditPDF = 473,
    CmdApplyRedactions = 474,
    CmdUndo = 475,
    CmdRedo = 476,
    CmdCutAnnotation = 477,
    CmdCopyAnnotation = 478,
    CmdPasteAnnotation = 479,
    CmdSearchGoogleLens = 480,
    CmdNavigateThumbnail = 481,
    CmdShowAnnotationText = 482,
    CmdAnnotationHighlightBrush = 483,
    CmdFindAnnotation = 484,
    CmdOpenFileNoHistory = 485,
    CmdCopySelectionAsImage = 486,
    CmdSearchGoogleLensPage = 487,
    CmdSearchGoogleLensImage = 488,
    CmdSaveSelectionAsImage = 489,
    CmdToggleTrimEmptyMargins = 490,
    CmdCopyLocationToClipboard = 491,
    CmdToggleAutomaticallyScroll = 492,
    CmdAutomaticallyScrollFaster = 493,
    CmdAutomaticallyScrollSlower = 494,
    CmdToggleReadingBar = 495,
    CmdToggleReadingBarInvert = 496,
    CmdGoToHomePage = 497,
    CmdToggleFreePan = 498,
    CmdNone = 499,
    CmdFileHistory = 500,
    CmdFavorite = 501,
    CmdReadAloudFromCursorPosition = 502,
    CmdToggleGrayscale = 503,
    CmdPrintSelection = 504,
    CmdAutoGenerateTOC = 505,
    CmdOpenSettingsFile = 506,
    CmdOpenFileWithSumatraFilePicker = 507,
    CmdSelectCurrentPage = 508,
    CmdZoomFitVisible = 509,
    CmdSignWithImage = 510,
    CmdInsertTextSnippet = 511,
    CmdToggleThumbnails = 512,
    CmdMergePDF = 513,
    CmdToggleAutoRotateWidePages = 514,
    CmdNavigateTabs = 515,
    CmdNavigateHistory = 516,
    CmdQuickEditSettings = 517,

    CmdLast = 517,
    CmdFirstCustom = CmdLast + 100,

    // aliases, at the end to not mess ordering
    CmdViewLayoutFirst = CmdSinglePageView,
    CmdViewLayoutLast = CmdToggleMangaMode,

    CmdZoomFirst = CmdZoomFitPage,
    CmdZoomLast = CmdZoomCustom,

    CmdCreateAnnotFirst = CmdCreateAnnotText,
    CmdCreateAnnotLast = CmdCreateAnnotFileAttachment,
    CmdTrimEmptyMargins = CmdToggleTrimEmptyMargins,
};
// clang-format on
// @gen-end cmd-enum

struct CommandArg {
    enum class Type : u16 {
        None,
        Bool,
        Int,
        Float,
        String,
        Color,
    };

    // arguments are a linked list for simplicity
    struct CommandArg* next = nullptr;

    Type type = Type::None;

    // TODO: we have a fixed number of argument names
    // we could use SeqStrings and use u16 for arg name id
    Str name;

    // TODO: could be a union
    Str strVal;
    bool boolVal = false;
    int intVal = 0;
    float floatVal = 0.0;
    ParsedColor colorVal;
};

CommandArg* AllocCommandArg(Str name, Str strVal);
void FreeCommandArgs(CommandArg* first);

struct CustomCommand {
    // all commands are stored as linked list
    struct CustomCommand* next = nullptr;

    // the command id like CmdOpenFile
    int origId = 0;

    // for debugging, the full definition of the command
    // as given by the user
    Str definition;

    // optional name, if given this shows up in command palette
    Str name;

    // optional keyboard shortcut
    Str key;

    // a unique command id generated by us, starting with CmdFirstCustom
    // it identifies a command with their fixed set of arguments
    int id = 0;

    CommandArg* firstArg = nullptr;
};

CustomCommand* AllocCustomCommand(Str definition, Str name, Str key);
void FreeCustomCommand(CustomCommand* cmd);

extern CustomCommand* gFirstCustomCommand;
struct CommandInfo {
    int id;
    SeqStrings name; // One-entry sequence for case/whitespace-insensitive lookup.
    Str description;
};

extern const CommandInfo gCommands[];
extern const int gCommandsCount;
extern const CommandInfo gCommandAltDescs[];
extern const int gCommandAltDescsCount;

int GetCommandIdByName(Str);
Str GetCommandName(int commandId);
Str GetCommandDescription(int commandId);

CustomCommand* CreateCustomCommand(Str definition, int origCmdId, CommandArg* args, Str name = {}, Str key = {});
CustomCommand* CloneCustomCommand(CustomCommand* cmd, Str name = {}, Str key = {});
CustomCommand* FindCustomCommand(int cmdId);
void FreeCustomCommands();
CommandArg* NewStringArg(Str name, Str val);
CommandArg* NewFloatArg(Str name, float val);
void InsertArg(CommandArg** firstPtr, CommandArg* arg);

CustomCommand* CreateCommandFromDefinition(Str definition);
CommandArg* GetCommandArg(CustomCommand*, Str argName);
int GetCommandIntArg(CustomCommand* cmd, Str name, int defValue);
bool GetCommandBoolArg(CustomCommand* cmd, Str name, bool defValue);
Str GetCommandStringArg(CustomCommand* cmd, Str name, Str defValue);
void GetCommandsWithOrigId(Vec<CustomCommand*>& commands, int origId);

#define kCmdArgColor StrL("color")
#define kCmdArgBgColor StrL("bgcolor")
#define kCmdArgOpacity StrL("opacity")
#define kCmdArgOpenEdit StrL("openedit")
#define kCmdArgTextSize StrL("textsize")
// TextSnippets: the text a CmdInsertTextSnippet inserts
#define kCmdArgText StrL("text")
#define kCmdArgBorderWidth StrL("borderwidth")
#define kCmdArgAlignment StrL("alignment")
#define kCmdArgInteriorColor StrL("interiorcolor")

#define kCmdArgCopyToClipboard StrL("copytoclipboard")
#define kCmdArgSetContent StrL("setcontent")
#define kCmdArgExe StrL("exe")
#define kCmdArgURL StrL("url")
// SelectionHandlers: how and what to send (see Customize-search-translation-services.md)
#define kCmdArgMethod StrL("method")
#define kCmdArgBody StrL("body")
#define kCmdArgContentType StrL("contenttype")
#define kCmdArgHeaders StrL("headers")
#define kCmdArgLevel StrL("level")
#define kCmdArgFilter StrL("filter")
#define kCmdArgN StrL("n")
#define kCmdArgMode StrL("mode")
// [CmdSignDocument certify]: open Sign Document set to certify (no changes allowed)
#define kCmdArgCertify StrL("certify")
#define kCmdArgTheme StrL("theme")
#define kCmdArgCommandLine StrL("cmdline")
#define kCmdArgToolbarText StrL("toolbartext")
#define kCmdArgToolbarSvgIcon StrL("toolbarsvgicon")
// text (or, when it starts with "<svg", an icon) for a button on the toolbar
// that pops up over a text selection
#define kCmdArgSelectToolbar StrL("selecttoolbar")
#define kCmdArgFocusEdit StrL("focusedit")
#define kCmdArgFocusList StrL("focuslist")
// optional bool to force a state on a toggle command instead of flipping it (#5067)
#define kCmdArgState StrL("state")
#define kCmdArgName StrL("name")
// path of the file to open, e.g. [CmdFileHistory C:\dir\file.pdf]
#define kCmdArgFilePath StrL("filepath")

// page of the favorite to go to, as stored in settings (a page number or `bm:<bookmark>`)
#define kCmdArgPage StrL("page")

constexpr const char* kPalettePrefixCommands = ">";
constexpr const char* kPalettePrefixFileHistory = "#";
constexpr const char* kPalettePrefixTabs = "@";
constexpr const char* kPalettePrefixEverything = ":";
constexpr const char* kPalettePrefixTOC = "%";
constexpr const char* kPalettePrefixFavorites = "$";
constexpr const char* kPalettePrefixAnnotations = "*";
constexpr const char* kPalettePrefixBoolSettings = "=";
constexpr const char* kPalettePrefixThumbnails = "&";

enum class PaletteMode {
    Commands,
    Tabs,
    FileHistory,
    Everything,
    Toc,
    Favorites,
    Annotations,
    Settings,
    Thumbnails,
};

PaletteMode ParsePaletteMode(Str& query);
