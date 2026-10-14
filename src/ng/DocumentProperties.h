/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;
struct DocController;
struct PropertiesWnd;

void ShowProperties(MainWindow* win, DocController* ctrl);
void DeletePropertiesWindow(MainWindow* win);
bool IsPropertiesDialogVisible();
gpui::El* PropertiesDialogBuild(MainWindow* win, gpui::Ctx* cx);
TempStr PropertiesDialogButtonsTemp(int* exitCodeOut);
TempStr PropertiesDialogTextTemp(int* exitCodeOut);
