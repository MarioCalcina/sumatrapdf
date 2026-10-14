// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/2254
//
// FB2 <title-info><annotation>…</annotation> must show in Document Properties
// (Ctrl+D) as Subject, alongside Title and Author.
//
// Fixture: tests/issue-2254.fb2
// Run:  bun tests/issue-2254.ts [--no-build]

import { existsSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { cmdId, EXE, runStandalone } from "./util.ts";
import { sleep } from "./winapi.ts";
import { launchControlled, sendCommandSync, killAndWait } from "./win-automation.ts";

const FB2 = join(import.meta.dir, "issue-2254.fb2");

function assertProps(text: string): void {
  if (!text.includes("Annotation Prop Test")) {
    throw new Error(`issue-2254: missing Title in properties:\n${text}`);
  }
  if (!/Subject:\s*Unique annotation marker for issue 2254/.test(text)) {
    throw new Error(`issue-2254: missing Subject from FB2 annotation:\n${text}`);
  }
  // every <author> in <title-info>, and only the name parts: the fixture's
  // first author also carries <home-page> and <email>, which are not the name
  if (!/Author:\s*Test Author, Second Writer/.test(text)) {
    throw new Error(`issue-2254: want both authors and only their names:\n${text}`);
  }
  if (/example\.org/.test(text)) {
    throw new Error(`issue-2254: the author's home-page/email leaked into a property:\n${text}`);
  }
}

export async function testit(): Promise<void> {
  if (!existsSync(FB2)) {
    throw new Error(`fixture missing: ${FB2}`);
  }
  if (!existsSync(EXE)) {
    throw new Error(`exe missing: ${EXE}`);
  }

  const { proc, client, frame } = await launchControlled([FB2]);
  try {
    await client.waitForRenderIdle();

    sendCommandSync(frame, cmdId("CmdProperties"));
    let text = "";
    const deadline = Date.now() + 8000;
    while (Date.now() < deadline) {
      const res = await client.request(ControlCommand.TestDocumentProperties, ["text"]);
      const raw = String(res[1] ?? "");
      if (res[0] === 0 && (raw.includes("Title:") || raw.includes("Author:"))) {
        text = raw;
        break;
      }
      await sleep(100);
    }
    if (!text) {
      throw new Error("issue-2254: Document Properties text did not appear");
    }
    console.log(`issue-2254 properties text (${text.length} chars):\n${text.slice(0, 800)}`);
    assertProps(text);
    return;
  } finally {
    client.close();
    try {
      await killAndWait(proc);
    } catch {
      // already exited
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
