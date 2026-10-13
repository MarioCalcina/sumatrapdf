---
name: update-extract
description: Regenerate the amalgamated extract copy in ext/a-extract from upstream with cmd/amalgam.ts. Use when asked to update, upgrade or re-amalgamate extract, or when the user runs /update-extract.
---

# Update amalgamated extract

`ext/a-extract/` is generated — never hand-edit it. `bun cmd/amalgam.ts -extract` clones upstream into
`.work/deps/extract`, amalgamates it, and validates the result by compiling it for arm64 and x86_64 with its build target's flags.

## Steps

1. Regenerate. With no further arguments it re-runs the repo and revision recorded in
   `ext/versions.txt`:

   ```sh
   bun cmd/amalgam.ts -extract
   ```

   To move to a different upstream revision, pass it explicitly:

   ```sh
   bun cmd/amalgam.ts -extract <repo-url> <tag-or-commit>
   ```

   It writes `extract/*.h`, `memento.h`, `extract.c` and `version.txt` into `ext/a-extract/`.
   Add `-keep` to reuse the existing checkout while iterating on the amalgamation rules.

2. Check `ext/a-extract/version.txt` — it records the project homepage, source repo URL, requested
   revision, resolved commit SHA-1 and GitHub commit URL. Confirm it is the revision you meant.

3. Build on a Mac (or push and run the `mac-daily` workflow):

   ```sh
   bun cmd/ng-build.ts -dbg -all
   ```

4. Review the diff. Don't commit without a passing build.

## Notes

- The MuPDF build uses only the amalgamated `ext/a-extract` source.
