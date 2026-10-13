---
name: update-zlib
description: Regenerate the amalgamated zlib copy in ext/a-zlib from upstream with cmd/amalgam.ts. Use when asked to update, upgrade or re-amalgamate zlib, or when the user runs /update-zlib.
---

# Update amalgamated zlib

`ext/a-zlib/` is generated — never hand-edit it. `bun cmd/amalgam.ts -zlib` clones upstream into
`.work/deps/zlib`, amalgamates it, and validates the result by compiling it for arm64 and x86_64 with its build target's flags.

## Steps

1. Regenerate. With no further arguments it re-runs the repo and revision recorded in
   `ext/versions.txt`:

   ```sh
   bun cmd/amalgam.ts -zlib
   ```

   To move to a different upstream revision, pass it explicitly:

   ```sh
   bun cmd/amalgam.ts -zlib <repo-url> <tag-or-commit>
   ```

   It writes `zlib.h`, `zlib.c`, `LICENSE` and `version.txt` into `ext/a-zlib/`.
   Add `-keep` to reuse the existing checkout while iterating on the amalgamation rules.

2. Check `ext/a-zlib/version.txt` — it records the project homepage, source repo URL, requested
   revision, resolved commit SHA-1 and GitHub commit URL. Confirm it is the revision you meant.

3. Build on a Mac (or push and run the `mac-daily` workflow):

   ```sh
   bun cmd/ng-build.ts -dbg -all
   ```

4. Review the diff. Don't commit without a passing build.

## Notes

- The zlib build uses only the amalgamated `ext/a-zlib` source. A small sample document is kept at `ext/a-zlib/zlib.3.pdf`.
