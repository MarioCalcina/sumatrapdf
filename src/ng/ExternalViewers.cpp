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

static bool filterMatchesEverything(Str ext) {
    return str::IsEmptyOrWhiteSpace(ext) || str::EqIS(ext, StrL("*"));
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
    // The user is responsible for quoting arguments that contain spaces, e.g. "%1".
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

// the tab's file when other apps may open it (File > Open With, Share);
// empty otherwise
Str FilePathForOtherApps(WindowTab* tab) {
    if (!tab || !CanViewExternally(tab)) {
        return {};
    }
    return tab->filePath;
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
