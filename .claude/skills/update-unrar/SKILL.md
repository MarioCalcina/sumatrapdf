---
name: update-unrar
description: Regenerate the amalgamated UnRAR copy in ext/a-unrar from upstream with cmd/amalgam.ts. Use when asked to update, upgrade or re-amalgamate UnRAR, or when the user runs /update-unrar.
---

# Update amalgamated UnRAR

`ext/a-unrar/` is generated — never hand-edit it. `bun cmd/amalgam.ts -unrar` clones upstream into
`.work/deps/unrar`, amalgamates it, and validates the result by compiling it with `cl.exe`.

## Steps

1. Regenerate. With no further arguments it re-runs the repo and revision recorded in
   `ext/versions.txt`:

   ```sh
   bun cmd/amalgam.ts -unrar
   ```

   To move to a different upstream revision, pass it explicitly:

   ```sh
   bun cmd/amalgam.ts -unrar <repo-url> <tag-or-commit>
   ```

   It writes `unrar.cpp`, `dll.hpp` (the C API `src/base/Archive.cpp` uses), `license.txt`, `acknow.txt` and `version.txt` into `ext/a-unrar/`.
   Add `-keep` to reuse the existing checkout while iterating on the amalgamation rules.

2. Check `ext/a-unrar/version.txt` — it records the project homepage, source repo URL, requested
   revision, resolved commit SHA-1 and GitHub commit URL. Confirm it is the revision you meant.

3. Build on a Mac (or push and run the `mac-app` workflow):

   ```sh
   bun cmd/ng-build.ts -dbg -all
   ```

4. Review the diff. Don't commit without a passing build.

## Notes

- rarlab ships tarballs rather than git, so the default repo is a mirror that commits each release tarball verbatim. Its tag numbers do not match the UnRAR version — pick the commit by the tarball it names.
- `global.cpp` is first in `unrarSources` on purpose: it defines `INCLUDEGLOBAL` before pulling in `rar.hpp`, which is what makes `global.hpp` define `ErrHandler` rather than declare it. `rar.hpp` is inlined once, so whichever chunk pulls it in first decides that.
- `isnt.cpp` and `motw.cpp` are guarded by `_WIN_ALL`; ng uses the same amalgamation on Linux, macOS and Wasm.
- `lowercaseWinIncludes` keeps Windows includes usable on case-sensitive filesystems.
