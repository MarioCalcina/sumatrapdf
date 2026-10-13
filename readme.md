## SumatraPDF for macOS

A macOS-only fork of SumatraPDF, a multi-format (PDF, EPUB, MOBI, CBZ, CBR, FB2,
CHM, XPS, DjVu) reader under (A)GPLv3 license, with some code under BSD license
(see AUTHORS).

### Install

Download `SumatraPDF-mac.dmg` from the
[latest release](https://github.com/MarioCalcina/sumatrapdf/releases/latest),
open it and drag SumatraPDF to Applications.

Apple has not notarized the app, so the first time macOS won't open it. Try to
open it once, then in System Settings > Privacy & Security click
**Open Anyway**. Later versions install themselves from Help > Check for
Updates.

### Build

The app is SumatraPDF ng (`src/ng/`). It runs on macOS 11 or later, on Apple
Silicon and Intel Macs; the original Windows app and the Linux and WebAssembly
builds were removed. Build on a Mac with bun and the Xcode command line tools:

    bun cmd/ng-build.ts -rel              # this Mac's architecture
    bun cmd/ng-build.ts -rel -universal   # Apple Silicon + Intel

or run the `mac-app` GitHub workflow, which builds a universal
`SumatraPDF-mac.dmg`.

Upstream project:
* [Website](https://www.sumatrapdfreader.org/free-pdf-reader)
* [Manual](https://www.sumatrapdfreader.org/manual)
* [Source](https://github.com/sumatrapdfreader/sumatrapdf)
