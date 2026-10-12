## SumatraPDF for macOS

A macOS-only fork of SumatraPDF, a multi-format (PDF, EPUB, MOBI, CBZ, CBR, FB2,
CHM, XPS, DjVu) reader under (A)GPLv3 license, with some code under BSD license
(see AUTHORS).

The app is SumatraPDF ng (`src/ng/`). The original Windows app and the Linux and
WebAssembly builds were removed. Build on a Mac with bun and the Xcode command
line tools:

    bun cmd/ng-build.ts -rel

or run the `mac-app` GitHub workflow, which builds `SumatraPDF-mac.dmg`.

Upstream project:
* [Website](https://www.sumatrapdfreader.org/free-pdf-reader)
* [Manual](https://www.sumatrapdfreader.org/manual)
* [Source](https://github.com/sumatrapdfreader/sumatrapdf)
