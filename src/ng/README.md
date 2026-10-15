# SumatraPDF ng

This fork builds ng for macOS only. Dependencies are under `ext/`; every
source of ours is under `src/ng/`, the base library in `src/ng/base/`.

Build on a Mac with `bun cmd/ng-build.ts -dbg` or `-rel`. Outputs live in
`out/mac/dbg` and `out/mac/rel`. The build also wraps the executable in
`SumatraPDF.app`, which declares the document types Finder offers it for
(`cmd/helper/ng-mac-bundle.ts`).

Compiler and sanitizer variants add suffixes, such as `dbg-clang` and
`dbg-asan`. `-all` builds every target; `test_util -run -- -for-ai` runs the
base unit tests. `-run` launches SumatraPDF as a normal session, so it
saves and restores settings. Pass `-- -for-testing` for a throwaway run.

## Keyboard shortcuts

`src/ng/Accelerators.cpp` holds the Windows defaults and a per-platform
layer over them. Shortcut strings in the settings take `Cmd` (also `Command`,
`Super`, `Meta`) for the Command key.

macOS: `Ctrl` in a default becomes `Cmd`, except for switching tabs
(`Ctrl + Tab`, `Ctrl + Page Up / Down`). `Alt + Left / Right`, `Ctrl + F4` and
`Ctrl + Insert` are dropped. These differ or are added:

| Shortcut              | Command             | Windows default         |
| --------------------- | ------------------- | ----------------------- |
| `Cmd + G`             | Find next           | `F3`                    |
| `Cmd + Shift + G`     | Find previous       | `Shift + F3`            |
| `Cmd + Alt + G`       | Go to page          | `Ctrl + G`              |
| `Cmd + ,`             | Settings            | none                    |
| `Cmd + I`             | Properties          | `Ctrl + D`              |
| `Cmd + D`             | Add favorite        | `Ctrl + B`              |
| `Cmd + ?`             | Manual              | `F1`                    |
| `Cmd + 0`             | Actual size         | `Ctrl + 1`              |
| `Cmd + 9`             | Fit page            | `Ctrl + 0`              |
| `Ctrl + Cmd + F`      | Fullscreen          | `F11`                   |
| `Cmd + Shift + F`     | Presentation        | `F5`                    |
| `Ctrl + Cmd + S`      | Bookmarks sidebar   | `F12`                   |
| `Cmd + Alt + T`       | Toolbar             | `F8`                    |
| `Cmd + {` / `Cmd + }` | Previous / next tab | `Ctrl + Page Up / Down` |
| `Cmd + [` / `Cmd + ]` | Back / forward      | `Alt + Left / Right`    |
| `Cmd + Up` / `Down`   | First / last page   | `Home` / `End`          |
| `Alt + Up` / `Down`   | Page up / down      | `Ctrl + Up / Down`      |
| `Cmd + Backspace`     | Delete annotation   | `Ctrl + Delete`         |

The Windows default keeps working where its key is still free (`F3`, `F11`,
`Cmd + B`, `Cmd + 1`). Shortcuts are shown as the menu bar writes them (`⇧⌘G`).
Keyboards that type `[` and `]` with Option (Spanish) can't reach the bracket
shortcuts, so, as in Safari and Chrome, `Cmd + Left / Right` also go back /
forward and `Cmd + Alt + Left / Right` switch tabs.

Keys the app handles itself (dialogs, the command palette, lists) take `Cmd`
where Windows takes `Ctrl` (`CtrlOrCmd()` in `gui/GpuiBridge.h`), and so do
Ctrl-clicks (open in new tab, multi-select). The folder list adds Finder's
`Cmd + Up`, `Cmd + [` and `Cmd + ]`. Key hints use the macOS glyphs
(`MacKeyCapTemp()`), taken from the English hint since a translation may
rename the key (Spanish `Supr` for `Del`): `⌘↩`, `⌃Tab`, `⌦`. Menus, keyboard
help and palette show `←` `→` `⇞` `⇟` `↖` `↘` in every language. The keyboard
selection caret (`F7`)
moves as in a text view: `Option` by word, `Cmd + Left / Right` to the line's
edge, `Cmd + Up / Down` to the document's. `Shift + Option + Left / Right`
extend a mouse selection by word.

Ng scripts use the `cmd/ng-` prefix, with build helpers under `cmd/helper/ng-`.
`ng-gen-commands.ts` and `ng-gen-settings.ts` write the generated command and
settings code. Format C/C++ with `bun cmd/ng-format.ts`. Embedded byte arrays
are generated and ignored. Translation staging lives in `.work/ng`; `bun cmd/trans-dl.ts` (run by CI)
writes `.work/translations.txt` from apptranslator's public history, plus
`translations/fork.txt` for strings the history lacks, so all of orig's
languages are complete. Numbers and dates follow the macOS region. The manual comes from a
sumatra-website checkout, staged in `.work/docs`.

`mac-daily.yml` builds debug and release and runs the base unit tests;
`mac-tests.yml` (manual) runs the integration tests in `tests/`.
`mac-app.yml` (manual) builds `SumatraPDF-mac.dmg`, publishes it as a GitHub
release with `update-check-mac.txt` (what Check for Updates reads: the version,
the dmg and its SHA-256) and takes screenshots of the app.

## macOS integration

AppKit code lives in `*_mac.mm` files, mostly `gui/NativeWindow_mac.mm`:

- trackpad pinch zooms around the cursor (gpui has no gesture events); a
  two-finger double tap toggles the zoom
- a sideways swipe turns the page while the page fits the width (Trackpad
  settings' "Swipe between pages", with two or three fingers)
- opened documents go to the Dock menu's recent documents
- the System theme ("Follow macOS") follows the system appearance
- the UI language on first start is the first of the system's languages we have;
  `CFBundleLocalizations` lists ours, so AppKit's own panels (print, open)
  and menu items follow it
- the app menu gets Services and Hide, the Window menu Minimize, Zoom and
  Bring All to Front, which gpui's menu bar lacks (`AppShellMacFinishMenus`),
  and an Edit menu through which AppKit's text fields (the save, open and print
  panels) cut, copy and paste; Help gets macOS's search field.
  gpui finds the Window menu by its English title, so it is translated after
- Print opens the macOS print panel (PDFKit) on the PDF itself, or on a
  rendered one for a selection, a rotation or unsaved annotations
- Save As (and every other save) is the macOS save panel; a `-dbg-control`
  script gets the port's dialog, which it can drive
- File > Open is the macOS open panel (`gui/FilePanels_mac.mm`): several files
  at once and orig's file-type filters in a popup; the other open prompts
  (images, certificates, attachments, Merge PDF) get their filters too. A
  `-dbg-control` script gets gpui's one-file prompt
- copied images (screenshots, Copy Selection as image, the image editor) go on
  the pasteboard as PNG and TIFF (`gui/MacServices_mac.mm`); gpui writes text
  only
- Fullscreen.PreventSleep holds an IOKit assertion against display sleep
- dragging the page shows macOS's closed hand; the note and ink tools show
  their icon as the cursor (`gui/NativeCursors.cpp`)
- the window opens where it was closed, on the display it was on
  (`AppShellKeepOnScreen`)
- a tab dragged out of its strip and dropped on another window joins that
  window; the window server says which one is under the pointer
- the Home page lists recent files with Finder's icon for their type
- File > Open Recent and Show in Finder; About, Settings and Quit only in the
  app menu
- File > Open With lists the apps Finder offers for the document, Share the
  sharing services (AirDrop, Mail, Messages...); Send by Email goes through the
  mail sharing service; ExternalViewers from the settings come before it
- Settings can make SumatraPDF the default PDF reader
- `InfoPlist.strings` in every language (`translations/infoplist.txt`) tell
  why macOS asks for the local network: the print panel looks for printers
- a command line naming files hands them to the SumatraPDF already running
  (`-reuse-instance`, the ReuseInstance setting, `-forward-search`), so a LaTeX
  editor's forward search lands in the open window
- Finder's thumbnails and Quick Look's previews of comics, DjVu, XPS, MOBI
  and FB2 (`QuickLook_mac.mm`): two app extensions in `Contents/PlugIns` load
  `Contents/Frameworks/SumatraQL`, and the app declares those types
  (`ng-mac-bundle.ts`). The engines, the model and the embedded archive are in
  `Contents/Frameworks/SumatraCore`, which the app links too; the shell
  functions they call come from the app, or from SumatraQL's stubs. A preview is the first 20 pages; it needs macOS 12
- Check for Updates downloads the release's dmg, checks its SHA-256 and
  installs it over the app, which restarts. A copy it can't replace (run from
  the dmg, translocated by Gatekeeper, a folder it can't write) gets the
  download page
- tabs sit in the title bar beside the traffic lights
- one universal app for Apple Silicon and Intel, macOS 11 or later

gpui gaps, not fixable from this repo (the framework is vendored from
gpui-kit-cpp):

- a text field's context menu (Cut, Copy, Paste, Select All) is gpui's and
  stays English
- a text selection can't be dragged out of the window: gpui has no drag
  source
