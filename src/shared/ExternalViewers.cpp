/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/CmdLineArgs.h"
#include "base/File.h"
#include "base/Launch.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "SumatraPDF.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "Commands.h"
#include "Translations.h"
#include "SumatraDialogs.h"
#include "gui/MacServices.h"
#include "ExternalViewers.h"

struct ExternalViewerInfo {
    Str name; // shown to the user
    int cmdId;
    Str exts; // valid extensions
    Str exePartialPath;
    Str launchArgs;
    Kind engineKind;
    // set by DetectExternalViewers()
    Str exeFullPath; // if found, full path to the executable (heap-owned)
};

// kindEngineChm

static int gExternalViewersCount = 0;

// clang-format off
static ExternalViewerInfo gExternalViewers[] = {
    {
        StrL("Explorer"),
        CmdOpenWithExplorer,
        StrL("*"),
        StrL("explorer.exe"),
        StrL(R"(/select,"%1")"),
        {},
        Str{},
    },
    {
        StrL("Directory Opus"),
        CmdOpenWithDirectoryOpus,
        StrL("*"),
        StrL(R"(GPSoftware\Directory Opus\dopus.exe)"),
        StrL(R"("%d")"),
        {},
        Str{},
    },
    {
        StrL("Total Commander"),
        CmdOpenWithTotalCommander,
        StrL("*"),
        StrL(R"(totalcmd\TOTALCMD64.EXE)"),
        StrL(R"("%d")"),
        {},
        Str{},
    },
    {
        StrL("Double Commander"),
        CmdOpenWithDoubleCommander,
        StrL("*"),
        StrL(R"(Double Commander\doublecmd.exe)"),
        StrL(R"(--no-splash --client "%d")"),
        {},
        Str{},
    },
    {
        StrL("Acrobat Reader"),
        CmdOpenWithAcrobat,
        StrL(".pdf"),
        StrL(R"(Adobe\Acrobat Reader DC\Reader\AcroRd32.exe)"),
        // Command line format for version 6 and later:
        //   /A "page=%d&zoom=%.1f,%d,%d&..." <filename>
        // see http://www.adobe.com/devnet/acrobat/pdfs/pdf_open_parameters.pdf#page=5
        // zoom=%z,%x,%y : percentage zoom and upper-left view (user-space coords)
        StrL(R"(/A "page=%p&zoom=%z,%x,%y" "%1")"),
        kindEngineMupdf,
        Str{}
    },
    {
        StrL("Acrobat Reader"),
        CmdOpenWithAcrobat,
        StrL(".pdf"),
        StrL(R"(Adobe\Acrobat DC\Acrobat\Acrobat.exe)"),
        // Command line format for version 6 and later:
        //   /A "page=%d&zoom=%.1f,%d,%d&..." <filename>
        // see http://www.adobe.com/devnet/acrobat/pdfs/pdf_open_parameters.pdf#page=5
        StrL(R"(/A "page=%p&zoom=%z,%x,%y" "%1")"),
        kindEngineMupdf,
        Str{}
    },
    {
        StrL("Foxit Reader"),
        CmdOpenWithFoxit,
        StrL(".pdf"),
        StrL(R"(Foxit Software\Foxit Reader\FoxitReader.exe)"),
        // Foxit: filename [-n page] [-z zoom]
        StrL(R"("%1" /A page=%p -z %z)"),
        kindEngineMupdf,
        Str{}
    },
    {
        StrL("Foxit PhantomPDF"),
        CmdOpenWithFoxitPhantom,
        StrL(".pdf"),
        StrL(R"(Foxit Software\Foxit PhantomPDF\FoxitPhantomPDF.exe)"),
        StrL(R"("%1" /A page=%p -z %z)"),
        kindEngineMupdf,
        Str{}
    },
    {
        StrL("PDF-XChange Editor"),
        CmdOpenWithPdfXchange,
        StrL(".pdf"),
        StrL(R"(Tracker Software\PDF Editor\PDFXEdit.exe)"),
        // PDFXChange cmd-line format:
        // [/A "param=value [&param2=value ..."] [PDF filename]
        StrL(R"(/A "page=%p&zoom=%z" "%1")"),
        kindEngineMupdf,
        Str{}
    },
    {
        StrL("Pdf & Djvu Bookmarker"),
        CmdOpenWithPdfDjvuBookmarker,
        StrL(".pdf;.djvu"),
        StrL(R"(Pdf & Djvu Bookmarker\PdfDjvuBookmarker.exe)"),
        Str{},
        {},
        Str{}
    },
    {
        StrL("XPS Viewer"),
        CmdOpenWithXpsViewer,
        StrL(".xps;.oxps"),
        StrL("xpsrchvw.exe"),
        Str{},
        kindEngineMupdf,
        Str{}
    },
    {
        StrL("HTML Help"),
        CmdOpenWithHtmlHelp,
        StrL(".chm"),
        StrL("hh.exe"),
        Str{},
        kindEngineChm,
        Str{}
    }
};
// clang-format on

// clang-format off
const int gOpenWithKnownExternalViewerCmds[] = {
    CmdOpenWithExplorer,
    CmdOpenWithDirectoryOpus,
    CmdOpenWithTotalCommander,
    CmdOpenWithDoubleCommander,
    CmdOpenWithAcrobat,
    CmdOpenWithFoxit,
    CmdOpenWithFoxitPhantom,
    CmdOpenWithPdfXchange,
    CmdOpenWithXpsViewer,
    CmdOpenWithHtmlHelp,
    CmdOpenWithPdfDjvuBookmarker,
    0,
};
// clang-format on

bool IsOpenWithKnownExternalViewerCmd(int cmdId) {
    for (int i = 0; gOpenWithKnownExternalViewerCmds[i]; i++) {
        if (gOpenWithKnownExternalViewerCmds[i] == cmdId) {
            return true;
        }
    }
    return false;
}

// a Shortcuts / toolbar entry is a clone with its own id, so it's the command
// it stands for that decides
bool IsOpenWithKnownExternalViewerCmd(CustomCommand* cmd) {
    return cmd && IsOpenWithKnownExternalViewerCmd(cmd->origId);
}

static ExternalViewerInfo* FindKnownExternalViewerInfoByCmdId(int cmdId) {
    for (ExternalViewerInfo& ev : gExternalViewers) {
        if (ev.cmdId == cmdId) {
            return &ev;
        }
    }
    return nullptr;
}

bool HasKnownExternalViewerForCmd(int cmdId) {
    ExternalViewerInfo* info = FindKnownExternalViewerInfoByCmdId(cmdId);
    return info && info->exeFullPath;
}

static bool CanViewExternally(WindowTab* tab) {
    if (!CanAccessDisk()) {
        return false;
    }
    // if tab is nullptr, we're queried for the
    // About window with disabled menu items
    if (!tab) {
        return true;
    }
    return file::Exists(tab->filePath);
}

void FreeExternalViewers() {
    for (ExternalViewerInfo& info : gExternalViewers) {
        str::Free(info.exeFullPath);
        info.exeFullPath = {};
    }
}

void DetectExternalViewers() {
    (void)gExternalViewersCount;
}

static bool filterMatchesEverything(Str ext) {
    return str::IsEmptyOrWhiteSpace(ext) || str::EqIS(ext, StrL("*"));
}

bool CanViewWithKnownExternalViewer(WindowTab* tab, int cmdId) {
    if (!tab || !CanViewExternally(tab)) {
        return false;
    }
    ExternalViewerInfo* ev = FindKnownExternalViewerInfoByCmdId(cmdId);
    if (!ev || len(ev->exeFullPath) == 0) {
        return false;
    }
    // must match file extension

    if (!filterMatchesEverything(ev->exts)) {
        TempStr ext = path::GetExtTemp(tab->filePath);
        if (!str::ContainsI(ev->exts, ext)) {
            return false;
        }
    }
    Kind engineKind = tab->GetEngineType();
    if (engineKind != nullptr) {
        if (ev->engineKind != nullptr) {
            if (ev->engineKind != engineKind) {
                logf("CanViewWithKnownExternalViewer cmd: %d, ev->engineKind '%s' != engineKind '%s'\n", cmdId,
                     Str(ev->engineKind), Str(engineKind));
                return false;
            }
        }
    }
    return true;
}

bool CouldBePDFDoc(WindowTab* tab) {
    // consider any error state a potential PDF document
    return !tab || !tab->ctrl || tab->GetEngineType() == kindEngineMupdf;
}

bool IsPdfDoc(WindowTab* tab) {
    if (!tab || !tab->ctrl) {
        return true;
    }
    if (tab->GetEngineType() != kindEngineMupdf) {
        return false;
    }
    DisplayModel* dm = tab->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    return !engine || EngineMupdfIsPdf(engine);
}

// substitutions in cmdLine:
//  %1 : file path (else the file path is appended)
//  %d : directory in which file is
//  %p : current page number
//  %z : zoom as percentage (100 = 100%; fit modes → 100)
//  %x : horizontal scroll position on the page (user-space; 0 if not scrolled)
//  %y : vertical scroll position on the page (user-space; 0 if not scrolled)
//  %% : a literal '%' (so e.g. "%%d" reaches the external program as "%d",
//       useful for tools like `mutool draw -o page-%d.png` -- see #5583)
// any other "%x" sequence is passed through unchanged.
// Note: substituted values (path, dir) are inserted literally and not
// re-scanned, so a '%' inside a file path can't trigger another substitution.
static TempStr FormatParamTemp(Str arg, WindowTab* tab) {
    Str path = tab->filePath ? tab->filePath : StrL("");

    // Zoom/scroll for Adobe-style open parameters (page=%p&zoom=%z,%x,%y).
    float zoomPct = 100.f;
    int scrollX = 0;
    int scrollY = 0;
    if (tab->ctrl) {
        float zv = tab->ctrl->GetZoomVirtual(true);
        if (zv > 0) {
            zoomPct = zv;
        }
        DisplayModel* dm = tab->AsFixed();
        if (dm) {
            ScrollState ss = dm->GetScrollState();
            if (ss.x >= 0) {
                scrollX = (int)ss.x;
            }
            if (ss.y >= 0) {
                scrollY = (int)ss.y;
            }
        }
    }

    str::Builder out;
    for (int i = 0; i < arg.len; i++) {
        if (arg.s[i] != '%') {
            out.AppendChar(arg.s[i]);
            continue;
        }
        if (i + 1 >= arg.len) {
            out.AppendChar('%');
            break;
        }
        switch (arg.s[i + 1]) {
            case '%':
                out.AppendChar('%'); // %% -> literal %
                i++;
                break;
            case '1':
                // TODO: if %1 is un-quoted, we should quote it but it's complicated because
                // it could be part of a pattern like %1.Page%p.txt
                // (as in https://github.com/sumatrapdfreader/sumatrapdf/issues/3868)
                out.Append(path);
                i++;
                break;
            case 'd':
                out.Append(path::GetDirTemp(path));
                i++;
                break;
            case 'p':
                out.Append(fmt("%d", tab->ctrl ? tab->ctrl->CurrentPageNo() : 0));
                i++;
                break;
            case 'z':
                out.Append(fmt("%.1f", zoomPct));
                i++;
                break;
            case 'x':
                out.Append(fmt("%d", scrollX));
                i++;
                break;
            case 'y':
                out.Append(fmt("%d", scrollY));
                i++;
                break;
            default:
                // unknown (or trailing) '%': leave it literal and keep scanning
                out.AppendChar('%');
                break;
        }
    }
    return ToStrTemp(out);
}

static TempStr GetDocumentPathQuoted(WindowTab* tab) {
    auto path = tab->filePath;
    return str::JoinTemp(StrL("\""), path, StrL("\""));
}

bool ViewWithKnownExternalViewer(WindowTab* tab, int cmdId) {
    bool canView = CanViewWithKnownExternalViewer(tab, cmdId);
    if (!canView) {
        logf("ViewWithKnownExternalViewer cmd: %d\n", cmdId);
        // with command palette can send un-enforcable command so not ReportIf
        ReportDebugIf(!canView);
        return false;
    }
    ExternalViewerInfo* ev = FindKnownExternalViewerInfoByCmdId(cmdId);
    if (len(ev->exeFullPath) == 0) {
        return false;
    }
    TempStr args;
    if (ev->launchArgs) {
        args = FormatParamTemp(ev->launchArgs, tab);
    } else {
        args = GetDocumentPathQuoted(tab);
    }
    return LaunchFileShell(ev->exeFullPath, args);
}

bool PathMatchFilter(Str path, Str filter) {
    if (filterMatchesEverything(filter)) {
        return true;
    }
    return path::Match(path, filter);
}

// TODO: find a better file for this?
// extract the executable (first token) from cmdLine, honoring a leading quote,
// and set *restOut to the remaining command line (after the exe and any spaces)
static TempStr ExtractExePathTemp(Str cmdLine, Str* restOut) {
    Str s = cmdLine;
    str::TrimChar(s, ' ');
    str::Builder exe;
    if (len(s) > 0 && s.s[0] == '"') {
        s = Str(s.s + 1, s.len - 1);
        int i = 0;
        for (; i < s.len && s.s[i] != '"'; i++) {
            exe.AppendChar(s.s[i]);
        }
        s = Str(s.s + (i < s.len ? i + 1 : i), s.len - (i < s.len ? i + 1 : i));
    } else {
        int i = 0;
        for (; i < s.len && s.s[i] != ' '; i++) {
            exe.AppendChar(s.s[i]);
        }
        s = Str(s.s + i, s.len - i);
    }
    str::TrimChar(s, ' ');
    *restOut = s;
    return ToStrTemp(exe);
}

bool RunWithExe(WindowTab* tab, Str cmdLine, Str filter) {
    if (!tab) {
        return false;
    }
    if (!PathMatchFilter(tab->filePath, filter)) {
        return false;
    }
    if (str::IsEmptyOrWhiteSpace(cmdLine)) {
        return false;
    }
    // Split into the exe (first token) and the rest of the command line, then do
    // the %1/%p/%d/%% substitution on the rest WITHOUT re-tokenizing and
    // re-quoting it, so the user's own quoting is preserved (issue #5695).
    // Re-quoting moved/mangled quotes and broke e.g. cmd.exe command lines.
    // This matches how the known external viewers are launched (FormatParamTemp
    // on the raw args). The user is responsible for quoting arguments that
    // contain spaces, e.g. "%1".
    Str rest;
    TempStr exePath = ExtractExePathTemp(cmdLine, &rest);
    if (len(exePath) == 0) {
        return false;
    }
    // TODO: this should be in ViewWithCustomExternalViewer()
    if (!file::Exists(exePath)) {
        TempStr msg =
            fmt("External viewer executable not found: %s. Fix ExternalViewers in advanced settings.", exePath);
        auto caption = Tr("Error");
        MsgBox(tab ? tab->win : nullptr, msg, caption, MbOk | MbIconError);
        return false;
    }
    if (str::IsEmptyOrWhiteSpace(rest)) {
        // no arguments given: pass the document path as the only argument
        return LaunchFileShell(exePath, tab->filePath);
    }
    TempStr params = FormatParamTemp(rest, tab);
    return LaunchFileShell(exePath, params);
}

// --- send as e-mail attachment (step 14a) -----------------------------------

// ng: the mail app gets the file as an attachment; without one, a mailto
// names the file so the user can attach it.
static bool IsEmailAttachmentSendAvailable() {
    return true;
}

static bool SendAsEmailAttachmentWithMail(Str filePath) {
    return MacComposeEmail(filePath);
}

static bool SendAsEmailAttachmentWithMailto(Str filePath) {
    TempStr name = path::GetBaseNameTemp(filePath);
    TempStr url = fmt("mailto:?subject=%s&body=%s", name, filePath);
    return LaunchBrowser(url);
}

bool CanSendAsEmailAttachment(WindowTab* tab) {
    // Requirements: a valid filename and a working way to send it
    if (!CanViewExternally(tab)) {
        return false;
    }
    return IsEmailAttachmentSendAvailable();
}

bool SendAsEmailAttachment(WindowTab* tab) {
    if (!tab || !CanSendAsEmailAttachment(tab)) {
        return false;
    }
    if (SendAsEmailAttachmentWithMail(tab->filePath)) {
        return true;
    }
    return SendAsEmailAttachmentWithMailto(tab->filePath);
}
