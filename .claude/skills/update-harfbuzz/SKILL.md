---
name: update-harfbuzz
description: Regenerate the amalgamated HarfBuzz copy in ext/a-harfbuzz from upstream with cmd/amalgam.ts. Use when asked to update, upgrade or re-amalgamate HarfBuzz, or when the user runs /update-harfbuzz.
---

# Update amalgamated HarfBuzz

`ext/a-harfbuzz/` is generated — never hand-edit it. `bun cmd/amalgam.ts -harfbuzz` clones upstream into
`.work/deps/harfbuzz`, amalgamates it, and validates the result by compiling it for arm64 and x86_64 with its build target's flags.

## Steps

1. Regenerate. With no further arguments it re-runs the repo and revision recorded in
   `ext/versions.txt`:

   ```sh
   bun cmd/amalgam.ts -harfbuzz
   ```

   To move to a different upstream revision, pass it explicitly:

   ```sh
   bun cmd/amalgam.ts -harfbuzz <repo-url> <tag-or-commit>
   ```

   Keep the revision in sync with the harfbuzz submodule of the vendored mupdf (`ext/versions.txt`).

   It writes `harfbuzz.cc`, the public `hb*.h` headers, three `.hh` X-macro fragments the inliner can't expand, `COPYING`. The output directory is wiped first, so headers dropped upstream don't linger and `version.txt` into `ext/a-harfbuzz/`.
   Add `-keep` to reuse the existing checkout while iterating on the amalgamation rules.

2. Check `ext/a-harfbuzz/version.txt` — it records the project homepage, source repo URL, requested
   revision, resolved commit SHA-1 and GitHub commit URL. Confirm it is the revision you meant.

3. Build on a Mac (or push and run the `mac-daily` workflow):

   ```sh
   bun cmd/ng-build.ts -dbg -all
   ```

4. Review the diff. Don't commit without a passing build.

## Notes

- Only the sources mupdf needs are amalgamated: `hb_base_sources`, `hb_subset_sources` and `hb-ft.cc` from upstream's `src/meson.build`. The cairo, CoreText, DirectWrite, graphite2, ICU, raster, Uniscribe and wasm backends are left out. That list lives in `cmd/amalgam.ts`.
- The single translation unit needs `/bigobj`; both build systems pass it.
