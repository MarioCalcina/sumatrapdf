/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum class PaperFormat {
    Other,
    A2,
    A3,
    A4,
    A5,
    A6,
    Letter,
    Legal,
    Tabloid,
    Statement
};
PaperFormat GetPaperFormatFromSizeApprox(SizeF size);

void GetPrintersInfo(str::Builder& out);

class EngineBase;
struct MainWindow;
struct Print_Advanced_Data;

struct PrintPageLayout {
    float zoom = 1.f;
    int rotation = 0;
    Point offset;
    Rect stretch;
    bool isStretch = false;
};

// result of command-line printing; the numeric values double as the process
// exit code so an automated caller can tell why printing failed (issue #3478)
enum class PrintResult {
    Ok = 0,
    Failed = 1,             // generic / unspecified failure (reserved)
    CannotLoadFile = 2,     // couldn't open the file or unsupported format
    PrintingNotAllowed = 3, // the document doesn't allow printing
    PrinterNotFound = 4,    // the named (or default) printer doesn't exist
    PrintFailed = 5,        // the printer driver / device failed
    NoPermission = 6,       // printing is disabled by restriction policy
};

PrintResult PrintFile(Str fileName, Str printerName = {}, bool displayErrors = true, Str settings = {});
PrintResult PrintFile2(EngineBase* engine, Str printerName = {}, bool displayErrors = true, Str settings = {});
// selectionByDefault: open the dialog with "Selection" chosen (needs a rectangular selection)
void PrintCurrentFile(MainWindow* win, bool waitForCompletion = false, bool selectionByDefault = false);
void AbortPrinting(MainWindow* win);
