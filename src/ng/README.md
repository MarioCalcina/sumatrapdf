# SumatraPDF ng

This fork builds ng for macOS only. Dependencies are under `ext/`. Files ng
shares with the original tree live in `src/shared/` and `src/base/`;
`cmd/helper/ng-shared.ts` lists them.

Build on a Mac with `bun cmd/ng-build.ts -dbg` or `-rel`. Outputs live in
`out/mac/dbg` and `out/mac/rel`. The build also wraps the executable in
`SumatraPDF.app`, which declares the document types Finder offers it for
(`cmd/helper/ng-mac-bundle.ts`).

Compiler and sanitizer variants add suffixes, such as `dbg-clang` and
`dbg-asan`. `-all` builds every target; `test_util -run -- -for-ai` runs the
base unit tests. `-run` launches SumatraPDF as a normal session, so it
saves and restores settings. Pass `-- -for-testing` for a throwaway run.

## Keyboard shortcuts

`src/shared/Accelerators.cpp` holds the Windows defaults and a per-platform
layer over them. Shortcut strings in the settings take `Cmd` (also `Command`,
`Super`, `Meta`) off Windows: Command on macOS, Super on Linux.

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

Keys the app handles itself (dialogs, the command palette, lists) take `Cmd`
where Windows takes `Ctrl` (`CtrlOrCmd()` in `gui/GpuiBridge.h`), and so do
Ctrl-clicks (open in new tab, multi-select). The folder list adds Finder's
`Cmd + Up`, `Cmd + [` and `Cmd + ]`. Key hints use the macOS glyphs
(`MacKeyCapTemp()`): `⌘↩`, `⌃Tab`, `⌦`.

Ng scripts use the `cmd/ng-` prefix, with build helpers under `cmd/helper/ng-`.
Use `ng-gen-commands.ts` and `ng-gen-settings.ts` for ng generated headers;
the original generators still write the original app's files. Format ng
C/C++ with `bun cmd/ng-format.ts`. Embedded byte arrays are generated and
ignored; shared sources and headers are staged under each build's
`generated/shared/` so their includes use ng headers. To diverge a shared file,
copy it into `src/ng` and remove it from `ng-shared.ts`, then update its target.
Translation staging lives in `.work/ng`; `bun cmd/trans-dl.ts` (run by CI)
writes `.work/translations.txt` from apptranslator's public history. The
manual uses the same website checkout and `.work/docs` staging as the
original app.

`mac-daily.yml` builds debug and release and runs the base unit tests.
`mac-app.yml` (manual) builds `SumatraPDF-mac.dmg` and screenshots of the app.

## macOS integration

AppKit code lives in `*_mac.mm` files, mostly `gui/NativeWindow_mac.mm`:

- trackpad pinch zooms around the cursor (gpui has no gesture events); a
  two-finger double tap toggles the zoom
- opened documents go to the Dock menu's recent documents
- the System theme ("Follow macOS") follows the system appearance
- the UI language on first start is the first of the system's languages we have
- Print opens the macOS print panel on a rendered PDF (PDFKit)
- tabs sit in the title bar beside the traffic lights
