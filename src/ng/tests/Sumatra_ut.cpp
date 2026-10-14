/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's src/tests/Sumatra_ut.cpp is also the unit test driver
// (RunAppUnitTests). src/tools/test_util.cpp is the driver here, so this file
// keeps only the test bodies. ParseTip_UnitTests and SvgTextIcon_UnitTests are
// left out: they need VirtCtrl / SvgIcons (step 12).

#include "base/Base.h"
#include "base/CmdLineArgs.h"
#include "base/File.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "DocProperties.h"
#include "EngineBase.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "Flags.h"
#include "Commands.h"
#include "AIChatCommon.h"
#include "SystemFonts.h"

extern "C" {
#include <mupdf/fitz.h>
void install_load_mac_font_funcs(fz_context* ctx);
}

// must be last to over-write assert()
#include "base/tests/UtAssert.h"

// in src/shared/tests/SumatraCommon_ut.cpp
void BenchRangeTest();
void versioncheck_test();
void hexstrTest();
void colorTest();
void parseCommandsTest();
void DocPropertiesTest();

void CachedObjects_UnitTests();
void CommandPaletteModel_UnitTests();
void ImageReader_UnitTests();
bool RenderCache_UnitTestCookieUnlocked();

static void AIChatProcessTest() {
    u8 rnd1[16];
    u8 rnd2[16];
    memset(rnd1, 0xff, sizeof(rnd1));
    memset(rnd2, 0, sizeof(rnd2));
    TempStr id1 = AIChatFormatSessionIdTemp(rnd1);
    TempStr id2 = AIChatFormatSessionIdTemp(rnd2);
    utassert(len(id1) == 36);
    utassert(id1.s[8] == '-' && id1.s[13] == '-' && id1.s[18] == '-' && id1.s[23] == '-');
    utassert(id1.s[14] == '4');
    utassert(str::Contains(StrL("89ab"), Str(id1.s + 19, 1)));
    utassert(!str::Eq(id1, id2));

    StrVec candidates;
    TempStr shell = AIChatFindExecutableTemp(candidates, StrL("sh"), StrL("sh"));
    utassert(len(shell) > 0);

    TempStr payload = StrL("portable process: \\\"ok\\\"");
    TempStr shellCmd = fmt("printf '%%s' %s", QuoteCmdLineArgTemp(payload));
    TempStr cmdLine = fmt("%s -c %s", QuoteCmdLineArgTemp(shell), QuoteCmdLineArgTemp(shellCmd));
    AIChatProcessLaunchResult launch;
    utassert(AIChatLaunchProcessWithStdoutPipe(cmdLine, {}, &launch));
    utassert(launch.ok);
    utassert(launch.processId > 0);

    str::Builder output;
    AIChatReadPipeToEnd(launch.hReadPipe, output);
    launch.hReadPipe = nullptr;
    utassert(AIChatWaitForProcess(launch.hProcess, 1000));
    AIChatCloseProcess(&launch.hProcess, false);
    utassert(str::Eq(ToStr(output), payload));

    cmdLine = fmt("%s -c %s", QuoteCmdLineArgTemp(shell), QuoteCmdLineArgTemp(StrL("sleep 1")));
    utassert(AIChatLaunchProcessWithStdoutPipe(cmdLine, {}, &launch));
    utassert(!AIChatWaitForProcess(launch.hProcess, 0));
    AIChatTerminateProcess(launch.hProcess);
    AIChatCloseProcess(&launch.hProcess, false);
}

// Body text in a markdown file is CSS sans-serif, which loads a system face.
static void OpenMarkdownWithSystemFonts(fz_context* ctx) {
    TempStr bare = GetTempFilePathTemp(StrL("sumd"));
    utassert(bare.s);
    TempStr mdPath = fmt("%s.md", bare);
    utassert(file::WriteFile(mdPath, StrL("# Hello\n\nplain **bold** and `code`.\n")));

    fz_document* doc = nullptr;
    int nPages = 0;
    int failed = 0;
    fz_var(doc);
    fz_var(nPages);
    fz_var(failed);
    fz_try(ctx) {
        fz_register_document_handlers(ctx);
        doc = fz_open_document(ctx, CStrTemp(mdPath));
        fz_layout_document(ctx, doc, 450, 600, 12);
        nPages = fz_count_pages(ctx, doc);
    }
    fz_always(ctx) {
        fz_drop_document(ctx, doc);
    }
    fz_catch(ctx) {
        failed = 1;
    }
    file::Delete(mdPath);
    file::Delete(bare);
    utassert(!failed);
    utassert(nPages > 0);
}

static void PlatformSystemFontTest() {
    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    utassert(ctx);
    install_load_mac_font_funcs(ctx);
    fz_font* font = fz_load_system_font(ctx, "Helvetica", 0, 0, 0);
    utassert(font);
    utassert(fz_encode_character(ctx, font, 'A') > 0);
    fz_drop_font(ctx, font);
    font = fz_load_system_font(ctx, "sans-serif", 1, 1, 0);
    utassert(font);
    fz_drop_font(ctx, font);
    font = fz_load_system_font(ctx, "serif", 0, 1, 0);
    utassert(font);
    fz_drop_font(ctx, font);
    font = fz_load_system_font(ctx, "monospace", 1, 0, 0);
    utassert(font);
    fz_drop_font(ctx, font);
    OpenMarkdownWithSystemFonts(ctx);
    font = fz_load_system_font(ctx, "SumatraMissingFont-7FA26D", 0, 0, 1);
    utassert(!font);
    fz_drop_context(ctx);
}

static void SystemFontNamesTest() {
    StrVec names;
    GetInstalledFontNames(names);
    utassert(len(names) > 0);
    for (int i = 1; i < len(names); i++) {
        utassert(!StrLessNoCase(names[i], names[i - 1]));
        utassert(!str::EqI(names[i], names[i - 1]));
    }
}

static void PageAspectViewTest() {
    DisplayMode mode = DisplayMode::Automatic;
    float zoom = kInvalidZoom;
    utassert(GetPageAspectView(RectF(0, 0, 800, 600), &mode, &zoom));
    utassert(mode == DisplayMode::SinglePage);
    utassert(zoom == kZoomFitPage);
    utassert(GetPageAspectView(RectF(0, 0, 600, 800), &mode, &zoom));
    utassert(mode == DisplayMode::Continuous);
    utassert(zoom == kZoomFitWidth);
    utassert(!GetPageAspectView(RectF(), &mode, &zoom));
}

void SumatraPDF_UnitTests() {
    CachedObjects_UnitTests();
    CommandPaletteModel_UnitTests();
    ImageReader_UnitTests();
    utassert(RenderCache_UnitTestCookieUnlocked());
    AIChatProcessTest();
    PlatformSystemFontTest();
    SystemFontNamesTest();
    DocPropertiesTest();
    PageAspectViewTest();
    parseCommandsTest();
    colorTest();
    BenchRangeTest();
    versioncheck_test();
    hexstrTest();
}
