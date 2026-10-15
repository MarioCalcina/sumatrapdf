// Debug the chat webview rendering WITHOUT calling a provider: opens the grok
// panel on a PDF and injects a canned (user, assistant) turn via the
// -dbg-control TestAIChatReplay command, which takes the same WebView* path a
// real turn does (addUser + appendText + flushBlock). Lets you iterate on the
// webview render fast; watch the app window to see it render. Needs a PDF path.
//   bun tests/ai-chat-replay.ts --no-build ~/doc.pdf
import { ControlCommand, withControlledSumatra } from "./control";
import { EXE, runStandalone } from "./util";

const PDF = process.argv.slice(2).filter((a) => a !== "--no-build")[0] ?? "";

const USER = "summarize page 1";
const RESPONSE = [
  "I'll read page 1 of the guide.",
  "",
  "## Summary of page 1",
  "",
  "Page 1 opens **The Northern Reaches**.",
  "",
  "- **L'Haan**: a frozen land of the blue-skinned **Mirin**, ruled by the Snow Queen.",
  "- **Narandu**: a frozen wasteland stretching across the far north.",
  "",
  "*(printed page number is 23)*",
].join("\n");

function sleep(ms: number): Promise<void> {
  return new Promise((r) => setTimeout(r, ms));
}

export async function testit(): Promise<void> {
  if (!PDF) {
    throw new Error("ai-chat-replay: pass a PDF: bun tests/ai-chat-replay.ts --no-build ~/doc.pdf");
  }
  await withControlledSumatra(
    EXE,
    async (client) => {
      await sleep(1500);
      const res = await client.request(ControlCommand.TestAIChatReplay, [USER, RESPONSE]);
      const exit = Number(res[0] ?? -1);
      console.log(`ai-chat-replay: exit=${exit} msg=${String(res[1] ?? "")}`);
      if (exit !== 0) {
        throw new Error("TestAIChatReplay failed");
      }
      // keep the window up briefly so the render can be observed
      await sleep(3000);
    },
    [PDF],
  );
  console.log("ai-chat-replay: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
