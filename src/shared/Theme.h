/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 */

void SetTheme(Str name);
void SetCurrentThemeFromSettings();
void SetThemeByIndex(int themeIdx);
int ThemeGetCount();
Str ThemeGetNameAt(int idx);
int ThemeGetCurrentIndex();
void ToggleLightDarkTheme();
Str ToggleLightDarkThemeTargetName();
void SumatraUpdateTheme();
void UpdateThemeAfterSystemColorChange();
void CreateThemeCommands();

Color ThemeDocumentColors(Color&);
Color ThemePageRenderColors(Color&);
Color ThemeMainWindowBackgroundColor();
Color ThemeControlBackgroundColor();
Color ThemeWindowBackgroundColor();
Color ThemeWindowTextColor();
Color ThemeWindowTextDisabledColor();
// light-mode system colors, for the few places that want them
Color SysControlTextColor();
Color SysDisabledTextColor();
Color SysHighlightBgColor();
Color SysHighlightTextColor();
Color ThemeWindowDarkerTextColor();
Color ThemeWindowControlBackgroundColor();
Color ThemeActiveTabBackgroundColor();
Color ThemeInactiveTabBackgroundColor();
Color ThemeWindowLinkColor();
Color ThemeHotBackgroundColor();
Color ThemeEdgeColor();
Color ThemeHotEdgeColor();
Color ThemeErrorBackgroundColor();
Color ThemeNotificationsBackgroundColor();
Color ThemeNotificationsTextColor();
Color ThemeNotificationsHighlightColor();
Color ThemeNotificationsHighlightTextColor();
Color ThemeNotificationsHighlightLinkColor();
Color ThemeNotificationsProgressColor();
bool ThemeColorizeControls();
bool IsCurrentThemeDefault();
void FreeThemes();
bool MigrateRenamedThemeNames();
bool GetInvertPageColors();
void SetInvertPageColors(bool);

struct VirtButton;
struct PlatformFont;

VirtButton* NewThemedButton(HWND hwndForDpi, Str text, PlatformFont*, bool isDefault);

extern int gFirstSetThemeCmdId;
extern int gLastSetThemeCmdId;
extern int gCurrSetThemeCmdId;
