// Environment of the ng debug ASan app (out/mac/dbg-asan/SumatraPDF) the
// tests drive.

// Inherited by every child. ASan reads these at process start, so setting
// them here does not change this process. The nano allocator occupies the
// shadow memory ASan needs.
const cur = process.env.ASAN_OPTIONS ?? "";
if (!/(^|:)abort_on_error(=|:|$)/.test(cur)) {
  process.env.ASAN_OPTIONS = cur ? `${cur}:abort_on_error=1` : "abort_on_error=1";
}
if (process.env.MallocNanoZone === undefined) {
  process.env.MallocNanoZone = "0";
}
