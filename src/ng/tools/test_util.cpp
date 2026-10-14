/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/DbgHelpDyn.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

extern void AppendStoreTest();
extern void ArchiveTest();
extern void BaseUtilTest();
extern void ByteOrderTests();
extern void CryptoUtilTest();
extern void CssParser_UnitTests();
extern void DictTest();
extern void DirRemoveAllTest();
extern void FileUtilTest();
extern void GuessFileTypeTest();
extern void JsonTest();
extern void SettingsUtilTest();
extern void SquareTreeTest();
extern void StrFormatTest();
extern void StrTest();
extern void StrVecTest();
extern void VecTest();
extern bool AnnotSearch_UnitTests();
extern void ChapterTable_UnitTests();
extern void MobiDoc_UnitTests();
extern void LitDoc_UnitTests();
extern void PdfSync_UnitTests();
extern void TextSelection_UnitTests();
extern void PagePosition_UnitTests();
extern void CommandPaletteModel_UnitTests();
extern void SumatraPDF_UnitTests();
extern void DocumentLayout_UnitTests();
extern void RenderScale_UnitTests();
extern void CommandAvailability_UnitTests();
extern void MergePdf_UnitTests();
extern void Annotation_UnitTests();
extern void EngineDjvuDec_UnitTests();
extern void TipMarkup_UnitTests();
extern void ReadAloudHighlight_UnitTests();
extern void RefHoverTest();
extern void AppSettingsTest();
extern void SimpleLogTest();
extern bool ShortcutParse_UnitTestShiftedPunct();
extern bool ShortcutParse_UnitTestGpuiStroke();
extern bool ShortcutParse_UnitTestAccelTable();
extern bool ShortcutParse_UnitTestKeyHint();
extern bool ShortcutParse_UnitTestMacKeyCap();
extern bool MarkdownModel_UnitTestBrowserNavigationUrl();
extern bool MarkdownToc_UnitTestHtmlLinks();
extern bool MarkdownToc_UnitTestHtmlHeadings();
extern bool MarkdownToc_UnitTestMermaid();
#if IS_DEBUG
extern bool PdfDarkModeImageStats_UnitTest();
extern bool EbookDoc_UnitTestLoading();
extern bool EbookDoc_UnitTestNormalizeURL();
extern bool EngineMupdf_UnitTestEbookLineSpacingCss();
extern bool EngineMupdf_UnitTestEbookFontFamilyCss();
extern bool EngineMupdf_UnitTestEbookMarginCss();
extern bool EngineMupdf_UnitTestMergeEBookUI();
extern bool EngineMupdf_UnitTestPageLabels();
bool GumboHtmlParser_UnitTest();
bool DisplayMode_UnitTestZoom();
extern bool Accelerators_UnitTestFolderNavIsSafe();
extern bool NavFiles_UnitTestHidden();
extern bool MeasurementSystem_UnitTests();
extern bool Translations_UnitTestLocales();
extern bool DpiPosix_UnitTests();
extern bool AppToolsPosix_UnitTests();
extern bool Accelerators_UnitTestTreeTakesLetters();
extern bool Accelerators_UnitTestCreateAnnotEdit();
extern bool Accelerators_UnitTestCustomShortcutShown();
#endif
extern void FileWatcher_UnitTests();
extern bool PlatformFontPosix_UnitTests();

// ng: test data lives in docs/test/. Works when run from the repo root and
// when run from out/<cfg>/, where the exe lives
TempStr TestDocPathTemp(Str relPath) {
    TempStr path = str::DupTemp(relPath);
    if (file::Exists(path)) {
        return path;
    }
    TempStr dir = path::JoinTemp(GetSelfExeDirTemp(), StrL("../.."));
    return path::JoinTemp(dir, relPath);
}

int main(int argc, char** argv) {
    bool forAi = false;
    for (int i = 1; i < argc; i++) {
        if (str::Eq(Str(argv[i]), StrL("-for-ai"))) {
            forAi = true;
        }
    }
    if (forAi) {
        setvbuf(stdout, nullptr, _IONBF, 0);
        setvbuf(stderr, nullptr, _IONBF, 0);
        utassert_set_for_ai(true);
    }
    printf("Running unit tests\n");

    AppendStoreTest();
    ArchiveTest();
    BaseUtilTest();
    ByteOrderTests();
    CryptoUtilTest();
    CssParser_UnitTests();
    DictTest();
    DirRemoveAllTest();
    FileUtilTest();
    GuessFileTypeTest();
    JsonTest();
    SettingsUtilTest();
    SquareTreeTest();
    StrFormatTest();
    StrTest();
    StrVecTest();
    VecTest();
    utassert(AnnotSearch_UnitTests());
    ChapterTable_UnitTests();
    MobiDoc_UnitTests();
    LitDoc_UnitTests();
    PdfSync_UnitTests();
    TextSelection_UnitTests();
    PagePosition_UnitTests();
    SumatraPDF_UnitTests();
    DocumentLayout_UnitTests();
    RenderScale_UnitTests();
    CommandAvailability_UnitTests();
    MergePdf_UnitTests();
    Annotation_UnitTests();
    EngineDjvuDec_UnitTests();
    TipMarkup_UnitTests();
    ReadAloudHighlight_UnitTests();
    RefHoverTest();
    AppSettingsTest();
    SimpleLogTest();
    utassert(ShortcutParse_UnitTestShiftedPunct());
    utassert(ShortcutParse_UnitTestGpuiStroke());
    utassert(ShortcutParse_UnitTestAccelTable());
    utassert(ShortcutParse_UnitTestKeyHint());
    utassert(ShortcutParse_UnitTestMacKeyCap());
    utassert(MarkdownModel_UnitTestBrowserNavigationUrl());
    utassert(MarkdownToc_UnitTestHtmlLinks());
    utassert(MarkdownToc_UnitTestHtmlHeadings());
    utassert(MarkdownToc_UnitTestMermaid());
#if IS_DEBUG
    utassert(PdfDarkModeImageStats_UnitTest());
    utassert(Accelerators_UnitTestFolderNavIsSafe());
    utassert(EbookDoc_UnitTestLoading());
    utassert(EbookDoc_UnitTestNormalizeURL());
    utassert(EngineMupdf_UnitTestEbookLineSpacingCss());
    utassert(EngineMupdf_UnitTestEbookFontFamilyCss());
    utassert(EngineMupdf_UnitTestEbookMarginCss());
    utassert(EngineMupdf_UnitTestMergeEBookUI());
    utassert(EngineMupdf_UnitTestPageLabels());
    utassert(GumboHtmlParser_UnitTest());
    utassert(DisplayMode_UnitTestZoom());
    utassert(NavFiles_UnitTestHidden());
    utassert(MeasurementSystem_UnitTests());
    utassert(Translations_UnitTestLocales());
    utassert(DpiPosix_UnitTests());
    utassert(AppToolsPosix_UnitTests());
    utassert(Accelerators_UnitTestTreeTakesLetters());
    utassert(Accelerators_UnitTestCreateAnnotEdit());
    utassert(Accelerators_UnitTestCustomShortcutShown());
#endif
    FileWatcher_UnitTests();
    utassert(PlatformFontPosix_UnitTests());

    int res = utassert_print_results();
    DestroyTempArena();
    return res;
}
