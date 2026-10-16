/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by KeyboardHelpCommon.cpp and each app's KeyboardHelp.cpp ---

// A section is an ordered list of command ids (terminated by 0). The keyboard
// shortcut for each command is looked up from its actual binding, not hard-coded
// here, so re-binding or clearing a shortcut is reflected automatically.
// clang-format off
static const int kSecNav[] = {
    CmdScrollUp, CmdScrollDown, CmdScrollLeft, CmdScrollRight,
    CmdScrollUpPage, CmdScrollDownPage,
    CmdGoToNextPage, CmdGoToPrevPage,
    CmdGoToFirstPage, CmdGoToLastPage, CmdGoToPage,
    CmdNavigateBack, CmdNavigateForward, 0,
};

static const int kSecView[] = {
    CmdZoomIn, CmdZoomOut,
    CmdZoomFitPage, CmdZoomFitWidth, CmdZoomActualSize,
    CmdToggleZoom, CmdSinglePageView, CmdFacingView,
    CmdBookView, CmdToggleContinuousView,
    CmdRotateLeft, CmdRotateRight, CmdToggleFullscreen, 0,
};

static const int kSecDoc[] = {
    CmdOpenFile, CmdSaveAs, CmdPrint, CmdReloadDocument,
    CmdClose, CmdNewWindow, CmdOpenNextFileInFolder,
    CmdOpenPrevFileInFolder, CmdRenameFile, CmdProperties, 0,
};

static const int kSecFind[] = {
    CmdFindFirst, CmdFindNext, CmdFindPrev,
    CmdSelectAll, CmdCopySelection, CmdSelectTextViaKeyboard,
    CmdToggleKeyboardLinkFollowing, 0,
};

static const int kSecTabs[] = {
    CmdNextTabSmart, CmdNextTab, CmdPrevTab,
    CmdMoveTabLeft, CmdMoveTabRight, CmdReopenLastClosedFile, 0,
};

static const int kSecAnnot[] = {
    CmdCreateAnnotHighlight, CmdCreateAnnotUnderline, CmdSaveAnnotations,
    CmdDeleteAnnotation, 0,
};

static const int kSecIface[] = {
    CmdCommandPalette, CmdToggleBookmarks, CmdToggleToolbar, CmdToggleMenuBar,
    CmdToggleCursorPosition, CmdTogglePageInfo,
    CmdFavoriteAdd, CmdFavoriteToggle, CmdHelpOpenManual,0,
};
// clang-format on

struct KbSectionDef {
    Str title;
    const int* commands;
    int column; // which of the two columns this section is laid out in
};
// column 0 (left): Navigation, Interface, Find & Select
// column 1 (right): View & Zoom, Document, Tabs, Annotations
static const KbSectionDef kSections[] = {
    {TrN("Navigation"), kSecNav, 0},    {TrN("View & Zoom"), kSecView, 1},   {TrN("Interface"), kSecIface, 0},
    {TrN("Document"), kSecDoc, 1},      {TrN("Find & Select"), kSecFind, 0}, {TrN("Tabs"), kSecTabs, 1},
    {TrN("Annotations"), kSecAnnot, 1},
};
bool IsHelpListedCmd(int cmdId);

struct SumatraKeyboardHelpDataSource : KeyboardHelpDataSource {
    Str Translate(Str s) override { return trans::GetTranslation(s); }

    TempStr CommandDescriptionTemp(int cmdId) override {
        Str description = GetCommandDescription(cmdId);
        if (len(description) == 0) {
            return {};
        }
        return str::DupTemp(trans::GetTranslation(description));
    }

    TempStr CommandShortcutTemp(int cmdId, int maxCount) override { return ShortcutsForCmdTemp(cmdId, maxCount); }
};
