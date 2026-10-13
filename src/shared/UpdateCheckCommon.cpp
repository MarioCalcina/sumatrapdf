/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/UITask.h"
#include "base/SquareTreeParser.h"
#include "base/Http.h"
#include "base/File.h"
#include "base/Crypto.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "AppTools.h"
#include "AppSettings.h"
#include "Version.h"
#include "SumatraConfig.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "Notifications.h"
#include "MainWindow.h"
#include "HomePage.h"
#include "UpdateCheck.h"
#include "UpdateCheckCommon.h"

// an available update surfaced by the pre-release startup notification; its
// "Update" link opens the download page (owned here until then)
UpdateInfo* gPendingUpdate = nullptr;

bool HasPendingPreReleaseUpdate() {
    return gPendingUpdate != nullptr;
}

bool IsTrustedUpdateDlUrl(Str dlURL) {
    return str::StartsWith(dlURL, kExpectedDlHost);
}

// the fork's update info is a release asset: no query, nothing counts checks
void BuildUpdateURL(str::Builder& url, Str baseURL, UpdateCheck) {
    url.Reset(baseURL);
}

UpdateInfo* ParseUpdateInfo(Str d) {
    // if a user configures os-wide proxy that is not a regular ie proxy
    // (which we pick up) we might get garbage http response
    // check if response looks valid
    if (len(d) == 0) {
        return nullptr;
    }
    Str prefix = (d.s[0] == '[') ? StrL("[SumatraPDF]") : StrL("SumatraPDF");
    if (!str::StartsWith(d, prefix)) {
        return nullptr;
    }

    SquareTreeNode* root = ParseSquareTree(d);
    if (!root) {
        return nullptr;
    }
    AutoDelete delRoot(root);

    SetPromoString(SerializeSquareTreeNodeTemp(root->GetChild(StrL("Promo"))));

    SquareTreeNode* node = root->GetChild(StrL("SumatraPDF"));
    if (!node) {
        return nullptr;
    }

    Str latestVer = node->GetValue(StrL("Latest"));
    if (!IsValidProgramVersion(latestVer)) {
        return nullptr;
    }
    auto* res = new UpdateInfo();
    res->latestVer = str::Dup(latestVer);
    Str onDate = node->GetValue(StrL("BuiltOn"));
    if (onDate) {
        res->builtOn = str::Dup(onDate);
    }
    // optional: without it the user is sent to the releases page
    res->downloadPage = str::Dup(node->GetValue(StrL("DownloadPage")));
    return res;
}
