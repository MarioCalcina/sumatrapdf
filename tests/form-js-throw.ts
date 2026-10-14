// An error thrown while form JavaScript runs (here: the console callback) used
// to be rethrown on the JS engine's fz_context. On a render thread that context
// has no fz_try, so the process aborted. Checked with ng's test_mupdf, since
// the app's own console callback never throws.
import { writeFileSync } from "node:fs";
import { join } from "node:path";
import { outDir, root as ROOT } from "../cmd/helper/ng-compile";
import { hostArch } from "../cmd/helper/ng-toolchain";
import { tmpPath } from "./util.ts";

const enc = (s: string) => Buffer.from(s, "latin1");

// a text field with no appearance stream and a format action running js
function makePdf(js: string): Buffer {
  const body: Record<number, Buffer> = {};
  body[1] = enc(
    "<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields [4 0 R] /NeedAppearances true " +
      "/DA (/Helv 12 Tf 0 g) /DR << /Font << /Helv 5 0 R >> >> >> >>",
  );
  body[2] = enc("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
  body[3] = enc("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>");
  body[4] = enc(
    "<< /Type /Annot /Subtype /Widget /FT /Tx /T (total) /V (12) /Rect [72 700 272 730] /P 3 0 R " +
      `/DA (/Helv 12 Tf 0 g) /AA << /F << /S /JavaScript /JS (${js.replace(/[\\()]/g, "\\$&")}) >> >> >>`,
  );
  body[5] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");

  const maxN = 5;
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0]!.length;
  for (let n = 1; n <= maxN; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n]!, enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (let n = 1; n <= maxN; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

export async function testit(): Promise<void> {
  const fail = (message: string): never => {
    throw new Error(message);
  };
  const run = (args: string[]) => {
    const proc = Bun.spawnSync(args, { cwd: ROOT });
    if (proc.exitCode !== 0) fail(`${args.join(" ")} failed (exit ${proc.exitCode}):\n${proc.stdout}\n${proc.stderr}`);
  };
  run([process.execPath, join(ROOT, "cmd/ng-build.ts"), "-rel", "test_mupdf"]);

  const pdf = tmpPath("form-js-throw.pdf");
  writeFileSync(pdf, makePdf('console.println("x");'));
  const probe = join(outDir({ debug: false, asan: false, clean: false, verbose: false, arch: hostArch }), "test_mupdf");
  run([probe, "-js-throw", pdf]);

  // copying a script's event.value out of the JS engine runs out of memory:
  // -js-oom fails the allocation of a 5000 character value
  const oomPdf = tmpPath("form-js-oom.pdf");
  writeFileSync(oomPdf, makePdf('event.value = new Array(5001).join("x");'));
  run([probe, "-js-oom", oomPdf]);
}

if (import.meta.main) {
  try {
    await testit();
    console.log("PASS: form-js-throw");
  } catch (error) {
    console.error(error);
    process.exitCode = 1;
  }
}
