---
name: update-zopfli
description: Regenerate the amalgamated zopfli copy in ext/a-zopfli from upstream with cmd/amalgam.ts. Use when asked to update, upgrade or re-amalgamate zopfli, or when the user runs /update-zopfli.
---

# Update amalgamated zopfli

`ext/a-zopfli/` is generated — never hand-edit it. `bun cmd/amalgam.ts -zopfli` clones upstream into
`.work/deps/zopfli`, amalgamates it, and validates the result by compiling it with `cl.exe`.

## Steps

1. Regenerate. With no further arguments it re-runs the repo and revision recorded in
   `ext/versions.txt`:

   ```sh
   bun cmd/amalgam.ts -zopfli
   ```

   To move to a different upstream revision, pass it explicitly:

   ```sh
   bun cmd/amalgam.ts -zopfli <repo-url> <tag-or-commit>
   ```

   It writes `zopflipng/zopflipng_lib.h`, `zopflipng/lodepng/lodepng.h`, `zopfli.cpp`, `COPYING` and `version.txt` into `ext/a-zopfli/`.
   Add `-keep` to reuse the existing checkout while iterating on the amalgamation rules.

2. Check `ext/a-zopfli/version.txt` — it records the project homepage, source repo URL, requested
   revision, resolved commit SHA-1 and GitHub commit URL. Confirm it is the revision you meant.

3. Build on a Mac (or push and run the `mac-app` workflow):

   ```sh
   bun cmd/ng-build.ts -dbg -all
   ```

4. Review the diff. Don't commit without a passing build.

## Notes

- The SumatraPDF build uses only the amalgamated `ext/a-zopfli` source.
