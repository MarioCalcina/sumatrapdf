# Upgrading chmdec

How to update SumatraPDF's vendored plain-C CHM reader (`ext/chmdec/chm.c` +
`ext/chmdec/chm.h`) from the upstream dist.

## Upstream source

- **Repo:** https://github.com/kjk/chmdec (`main` branch)
- **Files to copy:** `dist/chm.c` and `dist/chm.h` only
- Replaces the older `ext/libchm` vendor (same API family; was
  https://github.com/kjk/libchm).

The amalgamated files are generated upstream by `bun cmd/build-dist.ts`. Never
edit `ext/chmdec/chm.c` or `ext/chmdec/chm.h` by hand in SumatraPDF.

## Steps

### 1. Fetch latest upstream dist

Clone or refresh a shallow copy, then record the commit hash:

```bash
git clone --depth 1 https://github.com/kjk/chmdec.git /tmp/chmdec-update
git -C /tmp/chmdec-update rev-parse HEAD
```

If `/tmp/chmdec-update` already exists:

```bash
git -C /tmp/chmdec-update pull --ff-only
```

### 2. Copy dist files into ext/chmdec

```bash
cp /tmp/chmdec-update/dist/chm.c ext/chmdec/chm.c
cp /tmp/chmdec-update/dist/chm.h ext/chmdec/chm.h
```

Optionally refresh the license if it changed:

```bash
cp /tmp/chmdec-update/LICENSE.md ext/chmdec/LICENSE.md
```

**Do not clang-format** files under `ext/chmdec/` (vendored code).

### 3. Update ext/versions.txt

Record the import in `ext/versions.txt`:

```
chmdec          dist       YYYY-MM-DD
  https://github.com/kjk/chmdec
  commit <full-upstream-hash>
  amalgamated dist/chm.c + dist/chm.h only
```

Use today's date for `YYYY-MM-DD` and the hash from step 1.

### 4. Check for API changes

Diff the new `ext/chmdec/chm.h` against the previous version.

Pay special attention to:

- **New functions** — may need `ChmFile.cpp` updates
- **Removed/renamed functions** — update callers
- **`chm_open` buffer lifetime** — the in-memory buffer is not copied; it must
  outlive the `chm_ctx` (SumatraPDF keeps file bytes in `ChmFile` until
  `chm_ctx_free`)

Current integration points:

| File | Role |
|------|------|
| `src/ng/ChmFile.cpp` / `ChmFile.h` | CHM document open / entry read |
| `src/ng/ChmDump.cpp` | `-dump-chm` diagnostics |
| `src/ng/SumatraTestCommon.cpp` | `-test-chm` LZX / open regression |
| `cmd/helper/ng-targets.ts` | `chmdec` static lib target |
| `ext/versions.txt` | Vendored dependency version log |

### 5. Build and smoke-test

```bash
bun cmd/ng-build.ts -dbg
# optional CHM regression (needs a sample .chm):
# out/mac/dbg/SumatraPDF -for-testing -test-chm path/to/file.chm
# out/mac/dbg/SumatraPDF -for-testing -dump-chm path/to/file.chm
```

Also run `bun tests/issue-chm-lzx.ts` if LZX decode tables changed.

## Commit message template

```
Update chmdec to kjk/chmdec <short-hash>

Imported dist/chm.c and dist/chm.h from https://github.com/kjk/chmdec @ <full-hash>.
```

## Notes

- `ext/chmdec` is a **single-file amalgamation** (like sqlite / heicdec): compile
  `chm.c` only; include `chm.h` from callers.
- The build target is named `chmdec`; sources keep the historical `chm_*` C
  API names.
