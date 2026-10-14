# Upgrading djvudec

How to update SumatraPDF's vendored plain-C DjVu decoder (`ext/djvudec/djvu.c` +
`ext/djvudec/djvu.h`) from the upstream dist.

## Upstream source

- **Repo:** https://github.com/kjk/djvudec (`main` branch)
- **Files to copy:** `dist/djvu.c` and `dist/djvu.h` only
- **Do NOT use** https://codeberg.org/ccxvii/djvudec — that is an unrelated,
  older 2011 decoder with a different API (`djvudec.h`).

The amalgamated files are generated upstream by `bun cmd/build-dist.ts`. Never
edit `ext/djvudec/djvu.c` or `ext/djvudec/djvu.h` by hand in SumatraPDF.

## Steps

### 1. Fetch latest upstream dist

Clone or refresh a shallow copy, then record the commit hash:

```bash
git clone --depth 1 https://github.com/kjk/djvudec.git /tmp/djvudec-update
git -C /tmp/djvudec-update rev-parse HEAD
```

If `/tmp/djvudec-update` already exists:

```bash
git -C /tmp/djvudec-update pull --ff-only
```

### 2. Copy dist files into ext/djvudec

```bash
cp /tmp/djvudec-update/dist/djvu.c ext/djvudec/djvu.c
cp /tmp/djvudec-update/dist/djvu.h ext/djvudec/djvu.h
```

**Do not clang-format** files under `ext/djvudec/` (vendored code).

### 3. Update ext/versions.txt

Record the import in `ext/versions.txt`:

```
djvudec         main       YYYY-MM-DD
  https://github.com/kjk/djvudec
  commit <full-upstream-hash>
  amalgamated dist/djvu.c + dist/djvu.h only
```

Use today's date for `YYYY-MM-DD` and the hash from step 1.

### 4. Check for API changes

Diff the new `ext/djvudec/djvu.h` against the previous version. The public API
is also documented in upstream `src/djvu.h`.

Pay special attention to:

- **New functions** — may need `EngineDjvuDec.cpp` updates
- **Removed/renamed functions** — update `src/ng/EngineDjvuDec.cpp`
- **`djvu_init()`** — must be called once before concurrent decode (bilinear
  scaler table). `djvu_doc_open` also calls it, but that is not enough when
  render threads race on first open. `EngineDjvuDec.cpp` calls it once from a
  function-local static initializer.

Current integration points:

| File | Role |
|------|------|
| `src/ng/EngineDjvuDec.cpp` | Engine using the djvu_* API |
| `src/ng/EngineCreate.cpp` | Creates the djvudec engine for DjVu files |
| `cmd/helper/ng-targets.ts` | `djvudec` static lib target |
| `ext/versions.txt` | Vendored dependency version log |

### 5. Update EngineDjvuDec (if needed)

Review `src/ng/EngineDjvuDec.cpp` for any API mismatches. Common cases:

- New init function → ensure a once-guarded call before concurrent use
- New metadata/render helpers → wire up if SumatraPDF should expose them
- Changed struct fields → fix accessors

After editing it, run `bun cmd/ng-format.ts`.

### 6. Build and smoke-test

```bash
bun cmd/ng-build.ts -dbg
out/mac/dbg/SumatraPDF -for-testing <path-to>.djvu
```

### 7. Commit message

Include the upstream commit hash:

```
Update djvudec to kjk/djvudec <short-hash>

Imported dist/djvu.c and dist/djvu.h from https://github.com/kjk/djvudec @ <full-hash>.
```

Append `prompt: ...` if the change was AI-assisted (per Agents.md).

## Architecture notes

- `ext/djvudec` is a **single-file amalgamation** (like sqlite): compile
  `djvu.c` directly; include `djvu.h`.
- `EngineDjvuDec` keeps a single shared `djvu_doc` (read-only after open);
  `djvu_page_render_into` / metadata calls are re-entrant on the same doc.
- Thread-safety analysis lives in `docs/djvudec-threading.md` and upstream
  `thread-safety.md`.
- djvudec is the **only** DjVu engine (the libdjvu-based `EngineDjVu` has been
  removed; `EngineCreate.cpp` creates `EngineDjvuDec` for all DjVu files).

## Checklist

- [ ] Copied `dist/djvu.c` and `dist/djvu.h` from kjk/djvudec
- [ ] Updated `ext/versions.txt` with import date and upstream commit hash
- [ ] Checked `djvu.h` for API changes
- [ ] Updated `src/libsumatrapdf.def` if exports changed
- [ ] Updated `EngineDjvuDec.cpp` if integration changed
- [ ] Did **not** clang-format `ext/djvudec/`
- [ ] `bun cmd/build.ts -dbg` succeeds
- [ ] Smoke-tested a `.djvu` file
