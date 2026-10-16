/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's UpdateCheck.cpp. The parsing is orig's; the http get goes through
// gpui, and a new version is the dmg of this fork's GitHub release, which the
// app installs over itself (or the user gets from its download page).

#include "gui/GpuiBridge.h"

#include "base/UITask.h"
#include "base/SquareTreeParser.h"
#include "base/Http.h"
#include "base/File.h"
#include "base/Crypto.h"
#include "base/Launch.h"
#include <sys/sysctl.h>
#include <unistd.h>

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppTools.h"
#include "AppSettings.h"
#include "Version.h"
#include "SumatraConfig.h"
#include "Translations.h"
#include "DocController.h"
#include "Commands.h"
#include "TipMarkup.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "HomePage.h"
#include "SumatraDialogs.h"
#include "gui/MacServices.h"
#include "UpdateCheck.h"
#include "UpdateCheckCommon.h"

#include "SumatraLog.h"

static Kind kNotifUpdateCheckInProgress = "notifUpdateCheckInProgress";

// clang-format off
// tried in order; later entries are backups if earlier HTTP gets fail.
// Upstream would offer a build without this fork's changes: the fork's mac-app
// workflow attaches this to each release
static const Str updateInfoURLs[] = {
    StrL("https://github.com/MarioCalcina/sumatrapdf/releases/latest/download/update-check-mac.txt"),
};

#define kWebisteDownloadPageURL "https://github.com/MarioCalcina/sumatrapdf/releases/latest"
// clang-format on

// prevent multiple update tasks from happening simultaneously
// (this might e.g. happen if a user checks manually very quickly after startup)
static bool gUpdateCheckInProgress = false;

// the bottom-left "update available" notification (with the download link)
static Kind kNotifUpdateAvailable = "notifUpdateAvailable";

static void CurrentFileTime(FILETIME* ft) {
    GetSystemTimeAsFileTime(ft);
}

/*
The format of update information downloaded from the server:

[SumatraPDF]
Latest: 26.10.13
BuiltOn: 2026-10-13
DownloadPage: https://github.com/MarioCalcina/sumatrapdf/releases/tag/v26.10.13
*/

static bool ShouldCheckForUpdate(UpdateCheck updateCheckType) {
    if (gUpdateCheckInProgress) {
        logf("CheckForUpdate: skipping because gUpdateCheckInProgress\n");
        return false;
    }

    if (!HasPermission(Perm::InternetAccess)) {
        logf("CheckForUpdate: skipping because no internet access\n");
        return false;
    }

    if (updateCheckType == UpdateCheck::UserInitiated) {
        logf("CheckForUpdate: checking, user initiated\n");
        return true;
    }

    // don't check if the timestamp or version to skip can't be updated
    // (mainly in plugin mode, stress testing and restricted settings)
    if (!HasPermission(Perm::SavePreferences)) {
        logf("CheckForUpdate: skipping auto check because no prefs access\n");
        return false;
    }

    // only applies to automatic update check
    if (!gSettings->checkForUpdates) {
        logf("CheckForUpdate: skipping auto check because CheckForUpdates is false\n");
        return false;
    }

    // don't check for updates at the first start, so that privacy
    // sensitive users can disable the update check in time
    FILETIME never{};
    if (FileTimeEq(gSettings->timeOfLastUpdateCheck, never)) {
        logf("CheckForUpdate: skipping auto check, first start (TimeOfLastUpdateCheck not set)\n");
        return false;
    }

    // pre-release builds check on every startup (testers want the newest build);
    // skip the daily/weekly throttle below
    if (gIsPreReleaseBuild) {
        logf("CheckForUpdate: checking, pre-release build checks on every startup\n");
        return true;
    }

    // only check if at least a day passed since last check
    FILETIME currentTimeFt;
    CurrentFileTime(&currentTimeFt);
    int secsSinceLastUpdate = FileTimeDiffInSecs(currentTimeFt, gSettings->timeOfLastUpdateCheck);

    constexpr int kSecondsInDay = 60 * 60 * 24;
    constexpr int kSecondsInWeek = 7 * 60 * 60 * 24;

    int secsBetweenChecks = gIsPreReleaseBuild ? kSecondsInWeek : kSecondsInDay;
    bool checkUpdate = secsSinceLastUpdate > secsBetweenChecks;
    logf("CheckForUpdate: %s auto check, %d secs since the last one, %d secs between checks\n",
         checkUpdate ? StrL("doing") : StrL("skipping"), secsSinceLastUpdate, secsBetweenChecks);
    return checkUpdate;
}

static void OpenWebPage(Str url) {
    logf("UpdateCheck: opening '%s'\n", url);
    SumatraLaunchBrowser(url);
}

// The build's own download when the update info names a trusted one.
static void OpenDownloadPage(UpdateInfo* updateInfo) {
    Str page = updateInfo->downloadPage;
    if (len(page) == 0 || !IsTrustedUpdateDlUrl(page)) {
        page = StrL(kWebisteDownloadPageURL);
    }
    OpenWebPage(page);
}

// --- installing an update ---------------------------------------------------
//
// The release's dmg replaces the running app: downloaded and checked against
// its SHA-256, its app is copied next to ours. Once we have quit, a script
// swaps the two and starts the new one.
//
//   download -> sha256 -> hdiutil attach -> ditto -> .SumatraPDF.app.update
//   quit -> script: mv App App.old, mv update App, rm App.old, open App

static Kind kNotifUpdateInstall = "notifUpdateInstall";
constexpr int kSha256HexLen = 64;

struct UpdateInstall {
    MainWindow* win = nullptr;
    Str dmgUrl;     // empty: dmgPath is already here (TestInstallUpdate)
    Str dmgSha256;  // hex
    Str dmgPath;    // the downloaded dmg
    Str app;        // the bundle we replace
    Str staged;     // its replacement
    Str updatePage; // the fallback when installing fails
    bool ok = false;
    ~UpdateInstall() {
        str::Free(dmgUrl);
        str::Free(dmgSha256);
        str::Free(dmgPath);
        str::Free(app);
        str::Free(staged);
        str::Free(updatePage);
    }
};

// the bundle can be replaced: not run from the dmg, not translocated by
// Gatekeeper, and in a folder we may write to
static bool CanReplaceApp(Str app) {
    if (!str::EndsWithI(app, StrL(".app")) || str::Contains(app, StrL("/AppTranslocation/"))) {
        return false;
    }
    TempStr dir = path::GetDirTemp(app);
    return access(CStrTemp(dir), W_OK) == 0 && access(CStrTemp(app), W_OK) == 0;
}

static bool CanInstallUpdate(UpdateInfo* info) {
    if (len(info->dmgSha256) != kSha256HexLen || !IsTrustedUpdateDlUrl(info->dmgUrl)) {
        return false;
    }
    return CanReplaceApp(MacAppBundlePathTemp());
}

static bool OnDmgData(void* ctx, const uint8_t* data, int n) {
    return file::WriteAll(*(file::FileHandle*)ctx, Str((char*)data, n));
}

static bool DownloadDmg(UpdateInstall* d) {
    file::FileHandle fh = file::OpenReadWrite(d->dmgPath, true);
    if (fh == file::kInvalidFileHandle) {
        return false;
    }
    gp::HttpReq req;
    req.url = ToGpui(d->dmgUrl);
    req.onBody = OnDmgData;
    req.onBodyCtx = &fh;
    gp::HttpRsp rsp;
    bool ok = gp::HttpSend(req, &rsp) && rsp.status == 200;
    gp::HttpRspFree(&rsp);
    file::Close(fh);
    if (!ok) {
        logf("DownloadDmg: '%s' failed, status %d\n", d->dmgUrl, rsp.status);
        return false;
    }
    Str data = file::ReadFile(d->dmgPath);
    u8 digest[32];
    CalcSHA2Digest(data, digest);
    str::Free(data);
    str::Builder hex;
    for (u8 b : digest) {
        hex.Append(fmt("%02x", (int)b));
    }
    ok = str::EqI(ToStr(hex), d->dmgSha256);
    logf("DownloadDmg: sha256 %s, expected %s\n", ToStr(hex), d->dmgSha256);
    return ok;
}

// the dmg's app, copied next to ours
static bool StageUpdate(UpdateInstall* d) {
    TempStr mnt = fmt("%s.mnt", d->dmgPath);
    dir::RemoveAll(d->staged);
    if (!dir::CreateAll(mnt)) {
        return false;
    }
    StrVec attach;
    attach.Append(StrL("/usr/bin/hdiutil"));
    attach.Append(StrL("attach"));
    attach.Append(StrL("-nobrowse"));
    attach.Append(StrL("-readonly"));
    attach.Append(StrL("-noautoopen"));
    attach.Append(StrL("-mountpoint"));
    attach.Append(mnt);
    attach.Append(d->dmgPath);
    if (RunAndWait(attach) != 0) {
        rmdir(CStrTemp(mnt));
        return false;
    }
    StrVec ditto;
    ditto.Append(StrL("/usr/bin/ditto"));
    ditto.Append(path::JoinTemp(mnt, StrL("SumatraPDF.app")));
    ditto.Append(d->staged);
    bool ok = RunAndWait(ditto) == 0;
    StrVec detach;
    detach.Append(StrL("/usr/bin/hdiutil"));
    detach.Append(StrL("detach"));
    detach.Append(mnt);
    detach.Append(StrL("-force"));
    RunAndWait(detach);
    // only once unmounted, when it is empty
    rmdir(CStrTemp(mnt));
    return ok && file::Exists(path::JoinTemp(d->staged, StrL("Contents/MacOS/SumatraPDF")));
}

static void UpdateInstallFinish(UpdateInstall* d) {
    AutoDelete delD(d);
    MainWindow* win = IsMainWindowValidAndNotClosing(d->win) ? d->win : nullptr;
    if (win) {
        RemoveNotificationsForGroup(win, kNotifUpdateInstall);
    }
    if (!d->ok) {
        logf("UpdateInstallFinish: failed, opening '%s'\n", d->updatePage);
        if (win) {
            NotificationCreateArgs args;
            args.win = win;
            args.msg = Tr("Installation could not be completed.");
            args.warning = true;
            args.timeoutMs = kNotif5SecsTimeOut;
            ShowNotification(args);
        }
        OpenWebPage(d->updatePage);
        return;
    }
    // waits for us to quit; puts the old app back if the new one can't move in
    constexpr const char* kSwapScript =
        "while kill -0 \"$1\" 2>/dev/null; do sleep 0.2; done\n"
        "if mv \"$3\" \"$3.old\"; then\n"
        "  if mv \"$2\" \"$3\"; then rm -rf \"$3.old\"; else mv \"$3.old\" \"$3\"; fi\n"
        "fi\n"
        "open \"$3\"\n";
    StrVec args;
    args.Append(StrL("/bin/sh"));
    args.Append(StrL("-c"));
    args.Append(Str(kSwapScript));
    args.Append(StrL("sh"));
    args.Append(fmt("%d", (int)getpid()));
    args.Append(d->staged);
    args.Append(d->app);
    if (!LaunchFileShellArgs(args)) {
        OpenWebPage(d->updatePage);
        return;
    }
    logf("UpdateInstallFinish: '%s' replaces '%s' once we quit\n", d->staged, d->app);
    OnMenuExit();
}

static void UpdateInstallAsync(UpdateInstall* d) {
    d->ok = (len(d->dmgUrl) == 0 || DownloadDmg(d)) && StageUpdate(d);
    if (len(d->dmgUrl) > 0) {
        file::Delete(d->dmgPath);
    }
    uitask::Post(MkFunc0<UpdateInstall>(UpdateInstallFinish, d), "UpdateInstallFinish");
}

// dmgPath set: install that dmg, no download (TestInstallUpdate)
static void StartUpdateInstall(MainWindow* win, UpdateInfo* info, Str dmgPath) {
    auto* d = new UpdateInstall();
    d->win = win;
    d->app = str::Dup(MacAppBundlePathTemp());
    d->staged = str::Dup(path::JoinTemp(path::GetDirTemp(d->app), StrL(".SumatraPDF.app.update")));
    d->updatePage =
        str::Dup(info && IsTrustedUpdateDlUrl(info->downloadPage) ? info->downloadPage : StrL(kWebisteDownloadPageURL));
    if (len(dmgPath) > 0) {
        d->dmgPath = str::Dup(dmgPath);
    } else {
        d->dmgUrl = str::Dup(info->dmgUrl);
        d->dmgSha256 = str::Dup(info->dmgSha256);
        d->dmgPath = str::Dup(GetTempFilePathTemp(StrL("SumatraPDF-update-")));
    }
    logf("StartUpdateInstall: '%s' -> '%s'\n", len(d->dmgUrl) > 0 ? d->dmgUrl : d->dmgPath, d->app);
    if (win) {
        NotificationCreateArgs args;
        args.win = win;
        args.msg = Tr("Downloading update...");
        args.timeoutMs = kNotifNoTimeout;
        args.groupId = kNotifUpdateInstall;
        ShowNotification(args);
    }
    RunAsync(MkFunc0<UpdateInstall>(UpdateInstallAsync, d), StrL("UpdateInstallAsync"));
}

// TestInstallUpdate: the given dmg as if it were downloaded. False when this
// copy of the app can't be replaced
bool InstallUpdateFromDmg(MainWindow* win, Str dmgPath) {
    if (!CanReplaceApp(MacAppBundlePathTemp()) || !file::Exists(dmgPath)) {
        return false;
    }
    StartUpdateInstall(win, nullptr, dmgPath);
    return true;
}

// installs it when we can, else the browser gets its download page
static void InstallUpdate(UpdateInfo* updateInfo) {
    if (CanInstallUpdate(updateInfo)) {
        StartUpdateInstall(updateInfo->win, updateInfo, {});
        return;
    }
    OpenDownloadPage(updateInfo);
}

static void OnInstallAnswer(UpdateInfo* updateInfo, int res) {
    AutoDelete delInfo(updateInfo);
    ScheduleSaveSettings();
    if (res == MbRetYes) {
        InstallUpdate(updateInfo);
    }
}

// ng: orig's TaskDialog with "Don't install" / "Install and relaunch" buttons;
// the port's message box has Yes / No
static void NotifyUserOfUpdate(UpdateInfo* updateInfo) {
    Str mainInstr = Tr("New version available");
    Str ver = updateInfo->latestVer;
    Str fmtStr = Tr("You have version '%s' and version '%s' is available.\nDo you want to install the new version?");
    TempStr content = fmt("%s\n\n%s", mainInstr, fmt(fmtStr.s, currentVersion, ver));
    logf("NotifyUserOfUpdate: %s -> %s\n", currentVersion, ver);

    uint flags = MbYesNo | MbIconInformation;
    MsgBox(updateInfo->win, content, Tr("SumatraPDF Update"), flags,
           MkFunc1<UpdateInfo, int>(OnInstallAnswer, updateInfo));
}

// pre-release builds surface an available update with a bottom-left notification
// whose "Update" link opens the download page
static void ShowUpdateAvailableNotification(MainWindow* win, UpdateInfo* updateInfo) {
    if (!win || !updateInfo) {
        return;
    }
    TempStr link = fmt("[%s](CmdInstallPrereleaseUpdate)", Tr("Update"));
    Str displayVer = updateInfo->latestVer;
    TempStr msg;
    if (updateInfo->builtOn) {
        msg = fmt(Tr("Version %s from %s available. %s").s, displayVer, updateInfo->builtOn, link);
    } else {
        msg = fmt(Tr("Version %s available. %s").s, displayVer, link);
    }
    NotificationCreateArgs args;
    args.win = win;
    args.msg = msg;
    args.warning = true; // yellowish background so it stands out
    args.groupId = kNotifUpdateAvailable;
    args.timeoutMs = kNotifNoTimeout; // persist until the user clicks the link or closes it
    args.corner = NotifCorner::BottomLeft;
    args.xMargin = 2;
    args.yMargin = 2;
    ShowNotification(args);
}

// the "Update" link of the pre-release update notification
void DownloadAndInstallPendingUpdate(MainWindow* win) {
    if (!win || !gPendingUpdate) {
        return;
    }
    UpdateInfo* updateInfo = gPendingUpdate;
    gPendingUpdate = nullptr;
    AutoDelete delInfo(updateInfo);
    RemoveNotificationsForGroup(win, kNotifUpdateAvailable);
    updateInfo->win = win;
    InstallUpdate(updateInfo);
}

static bool ShouldDownloadUpdate(UpdateInfo* updateInfo) {
    Str latestVer = updateInfo->latestVer;
    return CompareProgramVersion(latestVer, currentVersion) > 0;
}

static void OnVisitWebsiteAnswer(int res) {
    if (res == MbRetYes) {
        OpenWebPage(StrL(kWebisteDownloadPageURL));
    }
}

// Shown only for a user-initiated update check that couldn't download/parse the
// update info. Tells the user and points them at the download page so they can
// update manually (e.g. if TLS validation or the network failed).
static void NotifyUpdateCheckFailed(MainWindow* win, u32 err) {
    logf("NotifyUpdateCheckFailed: err=%#x\n", (unsigned)err);
    TempStr msg = fmt(Tr("Couldn't download update information (error %#x).").s, err);
    TempStr content = fmt("%s\n\n%s\n\n%s", Tr("Couldn't check for updates"), msg,
                          StrL("Visit the website to download the latest version?"));
    MsgBox(win, content, Tr("SumatraPDF Update"), MbYesNo | MbIconWarning, MkFunc1Void(OnVisitWebsiteAnswer));
}

static u32 MaybeStartUpdateDownload(MainWindow* win, HttpRsp* rsp, UpdateCheck updateCheckType) {
    Str url = rsp->url;

    if (rsp->error != 0) {
        logf("MaybeStartUpdateDownload: http get of '%s' failed with %d\n", url, (int)rsp->error);
        return rsp->error;
    }
    if (rsp->httpStatusCode != 200) {
        logf("MaybeStartUpdateDownload: http get of '%s' failed with code %d\n", url, (int)rsp->httpStatusCode);
        return (u32)rsp->httpStatusCode;
    }

    bool isValidURL = false;
    for (auto updateInfoURL : updateInfoURLs) {
        if (str::StartsWith(url, updateInfoURL)) {
            isValidURL = true;
            break;
        }
    }
    if (!isValidURL) {
        logf("MaybeStartUpdateDownload: '%s' is not a valid url\n", url);
        return 1;
    }
    str::Builder* data = &rsp->data;
    if (0 == len(*data)) {
        logf("MaybeStartUpdateDownload: empty response from url '%s'\n", url);
        return 2;
    }

    UpdateInfo* updateInfo = ParseUpdateInfo(ToStr(*data));
    if (!updateInfo) {
        logf("MaybeStartUpdateDownload: ParseUpdateInfo() failed. URL: '%s'\n", url);
        return 3;
    }
    updateInfo->win = win;

    if (!ShouldDownloadUpdate(updateInfo)) {
        Str myVer = currentVersion;
        logf("MaybeStartUpdateDownload: myVer >= latestVer ('%s' >= '%s')\n", myVer, updateInfo->latestVer);
        /* if automated => don't notify that there is no new version */
        if (updateCheckType == UpdateCheck::UserInitiated) {
            auto* wnd = GetNotificationForGroup(win, kNotifUpdateCheckInProgress);
            if (wnd) {
                NotificationUpdateMessage(wnd, Tr("You have the latest version."), kNotif5SecsTimeOut);
            }
        }
        delete updateInfo;
        return 0;
    }

    // pre-release automatic check: show a bottom-left notification whose
    // "Update" link opens the download page
    if (updateCheckType == UpdateCheck::Automatic && gIsPreReleaseBuild) {
        RemoveNotificationsForGroup(win, kNotifUpdateCheckInProgress);
        delete gPendingUpdate;       // drop any update from a previous check
        gPendingUpdate = updateInfo; // take ownership (freed when installed/replaced)
        ShowUpdateAvailableNotification(win, updateInfo);
        return 0;
    }

    RemoveNotificationsForGroup(win, kNotifUpdateCheckInProgress);
    NotifyUserOfUpdate(updateInfo);
    return 0;
}

struct UpdateCheckAsyncData {
    MainWindow* win = nullptr;
    UpdateCheck updateCheckType = UpdateCheck::Automatic;
    HttpRsp* rsp = nullptr;
    int nextUrl = 0;
    UpdateCheckAsyncData() = default;
    ~UpdateCheckAsyncData() { delete rsp; }
};

// ng: gpui's http client, where orig uses base's winhttp
static bool UpdateHttpGet(Str uri, HttpRsp* rsp) {
    gp::HttpRsp gr;
    bool ok = gp::HttpGet(ToGpui(uri), &gr);
    rsp->httpStatusCode = (u32)gr.status;
    rsp->error = ok ? 0 : (u32)-1;
    if (ok && gr.body.len > 0) {
        rsp->data.Append(Str((char*)gr.body.els, gr.body.len));
    }
    gp::HttpRspFree(&gr);
    return ok && gr.status == 200;
}

static void UpdateCheckFinish(UpdateCheckAsyncData* data) {
    gUpdateCheckInProgress = false;
    AutoDelete delData(data);

    auto updateCheckType = data->updateCheckType;
    auto* rsp = data->rsp;
    MainWindow* win = nullptr;
    if (IsMainWindowValidAndNotClosing(data->win)) {
        win = data->win;
    } else if (len(gWindows) > 0) {
        win = gWindows[0];
    }
    if (!win || !rsp) {
        return;
    }
    u32 err = MaybeStartUpdateDownload(win, rsp, updateCheckType);
    if ((err != 0) && (updateCheckType == UpdateCheck::UserInitiated)) {
        RemoveNotificationsForGroup(win, kNotifUpdateCheckInProgress);
        // a manual check that couldn't fetch update info: tell the user and point
        // them at the website so they can update manually
        NotifyUpdateCheckFailed(win, err);
    }
}

static void UpdateCheckAsync(UpdateCheckAsyncData* data) {
    auto updateCheckType = data->updateCheckType;
    HttpRsp* rsp = nullptr;
    for (auto updateInfoURL : updateInfoURLs) {
        delete rsp;
        str::Builder url;
        BuildUpdateURL(url, updateInfoURL, updateCheckType);
        Str uri = ToStr(url);
        rsp = new HttpRsp;
        str::ReplaceWithCopy(&rsp->url, uri);
        bool ok = UpdateHttpGet(uri, rsp);
        logf("UpdateCheckAsync: response from '%s': error=%d, status=%d, %d bytes\n", rsp->url, (int)rsp->error,
             (int)rsp->httpStatusCode, (int)len(rsp->data));
        if (ok) {
            break;
        }
    }
    data->rsp = rsp;
    uitask::Post(MkFunc0<UpdateCheckAsyncData>(UpdateCheckFinish, data), "TaskUpdateCheckFinish");
}

// start auto-update check by downloading auto-update information from url
// on a background thread and processing the retrieved data on ui thread
void StartAsyncUpdateCheck(MainWindow* win, UpdateCheck updateCheckType) {
    if (!ShouldCheckForUpdate(updateCheckType)) {
        return;
    }

    logf("StartAsyncUpdateCheck: updateCheckType=%d\n", (int)updateCheckType);
    if (UpdateCheck::UserInitiated == updateCheckType) {
        NotificationCreateArgs args;
        args.win = win;
        args.msg = Tr("Checking for update...");
        args.warning = true;
        args.timeoutMs = kNotifNoTimeout;
        args.groupId = kNotifUpdateCheckInProgress;
        ShowNotification(args);
    }
    CurrentFileTime(&gSettings->timeOfLastUpdateCheck);
    gUpdateCheckInProgress = true;

    // data freed in UpdateCheckFinish()
    auto* data = new UpdateCheckAsyncData();
    data->win = win;
    data->updateCheckType = updateCheckType;
    RunAsync(MkFunc0<UpdateCheckAsyncData>(UpdateCheckAsync, data), StrL("UpdateCheckAsync"));
}
