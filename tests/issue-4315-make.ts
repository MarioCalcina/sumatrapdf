// Generate tests/issue-4315.azw3: a KF8-style MOBI with empty PalmDoc HTML
// and two solid-color JPEG pages. EngineMobi must synthesize recindex pages
// so the book is not blank (issue #4315).
//
// Run: bun tests/issue-4315-make.ts

import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { makePng, tmpPath } from "./util.ts";

function be16(n: number): Buffer {
  const b = Buffer.alloc(2);
  b.writeUInt16BE(n);
  return b;
}
function be32(n: number): Buffer {
  const b = Buffer.alloc(4);
  b.writeUInt32BE(n);
  return b;
}

// a 400x600 solid-color JPEG (GuessFileTypeFromData must recognize it)
function jpegSolid(r: number, g: number, b: number): Buffer {
  const png = tmpPath(`issue-4315-${r}-${g}-${b}.png`);
  const jpg = png.replace(/\.png$/, ".jpg");
  writeFileSync(png, makePng(400, 600, [r, g, b]));
  const out = Bun.spawnSync(["sips", "-s", "format", "jpeg", png, "--out", jpg], { stderr: "pipe" });
  if (out.exitCode !== 0) {
    throw new Error(`jpegSolid failed: ${out.stderr.toString()}`);
  }
  return readFileSync(jpg);
}

export function makeAzw3(): Buffer {
  const html = Buffer.from("<html><body></body></html>", "ascii");
  const imgA = jpegSolid(200, 30, 30);
  const imgB = jpegSolid(30, 30, 200);

  const mobi = Buffer.alloc(116);
  Buffer.from("MOBI").copy(mobi, 0);
  mobi.writeUInt32BE(116, 4);
  mobi.writeUInt32BE(2, 8); // book
  mobi.writeUInt32BE(65001, 12); // utf-8
  mobi.writeUInt32BE(1, 16);
  mobi.writeUInt32BE(8, 20); // KF8
  mobi.writeUInt32BE(2, 64); // firstNonBookRec
  mobi.writeUInt32BE(16 + 116, 68); // fullNameOffset in rec0
  mobi.writeUInt32BE(4, 72);
  mobi.writeUInt32BE(2, 92); // imageFirstRec

  const rec0 = Buffer.concat([
    be16(1),
    be16(0),
    be32(html.length),
    be16(1),
    be16(4096),
    be16(0),
    be16(0),
    mobi,
    Buffer.from("TEST"),
  ]);
  const recs = [rec0, html, imgA, imgB];
  const n = recs.length;
  const hdr = Buffer.alloc(78);
  Buffer.from("Test").copy(hdr, 0);
  Buffer.from("BOOKMOBI").copy(hdr, 60);
  hdr.writeUInt16BE(n, 76);
  let off = 78 + n * 8 + 2;
  const tableParts: Buffer[] = [];
  for (let i = 0; i < n; i++) {
    const row = Buffer.alloc(8);
    row.writeUInt32BE(off, 0);
    row[4] = 0;
    row[7] = i + 1;
    tableParts.push(row);
    off += recs[i]!.length;
  }
  return Buffer.concat([hdr, ...tableParts, Buffer.from([0, 0]), ...recs]);
}

if (import.meta.main) {
  const path = join(import.meta.dir, "issue-4315.azw3");
  writeFileSync(path, makeAzw3());
  console.log("wrote", path);
}
