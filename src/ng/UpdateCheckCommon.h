/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by UpdateCheckCommon.cpp and each app's UpdateCheck.cpp ---

struct UpdateInfo;
extern UpdateInfo* gPendingUpdate;
// this fork's releases
static const Str kExpectedDlHost = StrL("https://github.com/MarioCalcina/sumatrapdf/");
bool IsTrustedUpdateDlUrl(Str dlURL);
void BuildUpdateURL(str::Builder& url, Str baseURL, UpdateCheck updateCheckType);

struct UpdateInfo {
    // the window that asked
    MainWindow* win = nullptr;
    Str latestVer;
    Str builtOn; // optional "yyyy-mm-dd" from the update-check file
    // optional, where the browser gets this build when we can't install it (ng)
    Str downloadPage;
    // optional, the dmg the app installs itself and its SHA-256 in hex (ng)
    Str dmgUrl;
    Str dmgSha256;

    UpdateInfo() = default;
    ~UpdateInfo() {
        str::Free(latestVer);
        str::Free(builtOn);
        str::Free(downloadPage);
        str::Free(dmgUrl);
        str::Free(dmgSha256);
    }
};

UpdateInfo* ParseUpdateInfo(Str d);
