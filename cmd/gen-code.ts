// gen-code.ts - runs the C++ codegen (gen-flags, gen-commands, the DocProp
// name maps). ng-gen-settings.ts writes Settings.h
import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { main as genFlags } from "./gen-flags";
import { main as genCommands } from "./gen-commands";
import { clangFormatFiles } from "./util";

// C++ outputs touched by gen-code; formatted at the end so a follow-up
// cmd/ng-format.ts run does not dirty the tree (e.g. indented @gen markers).
const generatedCppFiles = [
  "src/ng/Flags.cpp",
  "src/ng/Commands.h",
  "src/ng/Commands.cpp",
  "src/ng/Accelerators.cpp",
  "src/ng/EbookDoc.cpp",
  "src/ng/DocProperties.cpp",
  "src/ng/EngineMupdf.cpp",
];

function varIntEncode(val: number): number[] {
  const bytes: number[] = [];
  let n = BigInt(val);
  let zigzag = (n << 1n) ^ (n >> 63n);
  while (true) {
    let b = Number(zigzag & 0x7fn);
    zigzag >>= 7n;
    if (zigzag !== 0n) {
      b |= 0x80;
    }
    bytes.push(b);
    if (zigzag === 0n) {
      break;
    }
  }
  return bytes;
}

function escapeCString(s: string): string {
  return s.replace(/\\/g, "\\\\").replace(/"/g, '\\"');
}

function varIntCString(val: number): string {
  const bytes = varIntEncode(val);
  return '"' + bytes.map((b) => `\\x${b.toString(16).padStart(2, "0")}`).join("") + '"';
}

function replaceBetweenMarkers(content: string, startMarker: string, endMarker: string, generated: string): string {
  const startIdx = content.indexOf(startMarker);
  const endIdx = content.indexOf(endMarker);
  if (startIdx < 0 || endIdx < 0) {
    console.error(`Could not find markers '${startMarker}' and '${endMarker}'`);
    process.exit(1);
  }
  const before = content.substring(0, startIdx + startMarker.length);
  const after = content.substring(endIdx);
  return before + "\n" + generated + "\n" + after;
}

// name<->DocProp maps, previously hand-written as SeqStrNum literals with the
// per-entry number encoded by hand. That is error-prone: the number must be
// zigzag-LEB128 (varIntCString), and several maps used raw bytes instead, so
// every lookup was wrong. Generate them here so the encoding is always correct.
// Each entry is [format-specific name, DocProp enum member].
type PropEntry = [string, string];

// DocProp -> PDF /Info key.
const pdfInfoNames: PropEntry[] = [
  ["Title", "Title"],
  ["Author", "Author"],
  ["Subject", "Subject"],
  ["Copyright", "Copyright"],
  ["CreationDate", "CreationDate"],
  ["ModDate", "ModificationDate"],
  ["Creator", "CreatorApp"],
  ["Producer", "PdfProducer"],
];

type PropMapTarget = { file: string; tag: string; varName: string; entries: PropEntry[] };

const docPropMaps: PropMapTarget[] = [
  {
    file: "src/ng/EbookDoc.cpp",
    tag: "docprop-epub",
    varName: "epubPropsMap",
    entries: [
      ["dc:title", "Title"],
      ["dc:creator", "Author"],
      ["dc:date", "CreationDate"],
      ["dcterms:modified", "ModificationDate"],
      ["dc:description", "Subject"],
      ["dc:rights", "Copyright"],
    ],
  },
  { file: "src/ng/DocProperties.cpp", tag: "docprop-pdf-info", varName: "gPdfInfoPropsMap", entries: pdfInfoNames },
  {
    file: "src/ng/EngineMupdf.cpp",
    tag: "docprop-mupdf",
    varName: "mupdfPropsMap",
    // keys mirror mupdf's FZ_META_INFO_* ("info:Title" etc.)
    entries: [
      ["info:Title", "Title"],
      ["info:Author", "Author"],
      ["info:Subject", "Subject"],
      ["info:Producer", "PdfProducer"],
      ["info:Creator", "CreatorApp"],
      ["info:CreationDate", "CreationDate"],
      ["info:ModDate", "ModificationDate"],
    ],
  },
];

// parse `enum class DocProp : u8 { None = 0, Title = 1, ... }` -> { Title: 1, ... }
function parseDocPropValues(rootDir: string): Record<string, number> {
  const src = readFileSync(join(rootDir, "src", "ng", "DocProperties.h"), "utf-8");
  const m = src.match(/enum class DocProp[^{]*\{([^}]*)\}/);
  if (!m) {
    throw new Error("DocProp enum not found in src/ng/DocProperties.h");
  }
  const vals: Record<string, number> = {};
  for (const line of m[1].split("\n")) {
    const mm = line.match(/\b(\w+)\s*=\s*(\d+)/);
    if (mm) {
      vals[mm[1]] = parseInt(mm[2], 10);
    }
  }
  return vals;
}

function generateSeqStrNumPropMap(varName: string, entries: PropEntry[], vals: Record<string, number>): string {
  const lines: string[] = ["// clang-format off", `static SeqStrNum ${varName} =`];
  for (const [name, prop] of entries) {
    const val = vals[prop];
    if (val === undefined) {
      throw new Error(`unknown DocProp '${prop}' for ${varName}`);
    }
    lines.push(`    "${escapeCString(name)}\\0" ${varIntCString(val)}`);
  }
  lines.push('    "\\0";', "// clang-format on");
  return lines.join("\n");
}

// decode a generated map back (matching Str.cpp VarIntDecode) and assert the
// first, middle and last entries round-trip - catches any encoding mistake at
// generation time.
function verifyPropMap(t: PropMapTarget, vals: Record<string, number>): void {
  const gen = generateSeqStrNumPropMap(t.varName, t.entries, vals);
  // pull the "\xNN" byte groups back out, in entry order
  const byteGroups = [...gen.matchAll(/\\0" "((?:\\x[0-9a-f]{2})+)"/g)].map((m) =>
    [...m[1].matchAll(/\\x([0-9a-f]{2})/g)].map((b) => parseInt(b[1], 16)),
  );
  const decode = (bytes: number[]): number => {
    let n = 0n;
    let shift = 0n;
    for (const b of bytes) {
      n |= BigInt(b & 0x7f) << shift;
      if (!(b & 0x80)) break;
      shift += 7n;
    }
    return Number((n >> 1n) ^ -(n & 1n));
  };
  const idxs = [0, Math.floor(t.entries.length / 2), t.entries.length - 1];
  for (const i of idxs) {
    const want = vals[t.entries[i][1]];
    const got = decode(byteGroups[i]);
    if (got !== want) {
      throw new Error(`${t.varName}[${i}] (${t.entries[i][0]}): encoded ${got}, expected ${want}`);
    }
  }
}

export function genDocPropMaps(rootDir: string) {
  const vals = parseDocPropValues(rootDir);
  // group targets by file
  const byFile = new Map<string, PropMapTarget[]>();
  for (const t of docPropMaps) {
    verifyPropMap(t, vals);
    const list = byFile.get(t.file) ?? [];
    list.push(t);
    byFile.set(t.file, list);
  }
  for (const [file, targets] of byFile) {
    const p = join(rootDir, file);
    let content = readFileSync(p, "utf-8");
    for (const t of targets) {
      content = replaceBetweenMarkers(
        content,
        `// @gen-start ${t.tag}`,
        `// @gen-end ${t.tag}`,
        generateSeqStrNumPropMap(t.varName, t.entries, vals),
      );
    }
    writeFileSync(p, content, "utf-8");
    console.log(`Generated DocProp map(s) in ${file}: ${targets.map((t) => t.varName).join(", ")}`);
  }
}

export async function main() {
  const timeStart = performance.now();
  const rootDir = join(import.meta.dir, "..");

  genFlags();
  genCommands();
  genDocPropMaps(rootDir);

  await clangFormatFiles(rootDir, generatedCppFiles);

  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`gen-code finished in ${elapsed}s`);
}

if (import.meta.main) {
  await main();
}
