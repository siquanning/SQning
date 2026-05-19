import { chromium } from "playwright";

import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
const __dirname = dirname(fileURLToPath(import.meta.url));
const URL = `file:///${join(__dirname, "index.html").replace(/\\/g, "/")}`;

const CHECKS = [];

function check(name, fn) {
  CHECKS.push({ name, fn });
}

async function main() {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
  page.on("console", (msg) => console.log(`  [console:${msg.type()}]`, msg.text()));
  page.on("pageerror", (err) => console.log("  [PAGE ERROR]", err.message));

  await page.goto(URL, { waitUntil: "domcontentloaded" });
  await page.waitForTimeout(600);

  // ═══ CHECKS ═══
  check("title", async () => {
    const title = await page.title();
    return { pass: title.includes("杭州麻将"), detail: title };
  });

  check("hand tiles visible", async () => {
    const count = await page.locator("#playerHand .tile").count();
    return { pass: count === 14, detail: `Player hand has ${count} tiles` };
  });

  check("score strip shows 4 players", async () => {
    const count = await page.locator(".score-card").count();
    return { pass: count === 4, detail: `${count} score cards` };
  });

  check("wall count badge visible", async () => {
    const text = await page.locator("#wallCount").textContent();
    return { pass: /牌墙/.test(text), detail: text };
  });

  check("turn indicator visible", async () => {
    const text = await page.locator("#turnText").textContent();
    return { pass: /当前/.test(text), detail: text };
  });

  check("wild tile badge visible", async () => {
    const text = await page.locator("#wildTileBadge").textContent();
    return { pass: /财神/.test(text), detail: text.trim() };
  });

  check("status line visible", async () => {
    const text = await page.locator("#statusLine").textContent();
    return { pass: text.length > 0, detail: text.trim() };
  });

  check("opponent seats rendered", async () => {
    const top = await page.locator("#seatTop .opponent").count();
    const left = await page.locator("#seatLeft .opponent").count();
    const right = await page.locator("#seatRight .opponent").count();
    const pass = top > 0 && left > 0 && right > 0;
    return { pass, detail: `top:${top} left:${left} right:${right}` };
  });

  check("opponent hand backs visible", async () => {
    const seats = ["#seatTop", "#seatLeft", "#seatRight"];
    const counts = await Promise.all(seats.map((s) => page.locator(`${s} .tile-back`).count()));
    const pass = counts.every((c) => c === 13);
    return { pass, detail: `backs: ${counts.join("/")}` };
  });

  check("river board exists", async () => {
    const rivers = await page.locator(".river").count();
    return { pass: rivers === 4, detail: `${rivers} river areas` };
  });

  check("action bar exists", async () => {
    const visible = await page.locator("#actionBar").isVisible();
    return { pass: visible, detail: "action bar visible" };
  });

  check("hint panel exists", async () => {
    const text = await page.locator("#hintList").textContent();
    return { pass: text.length > 0, detail: text.trim().slice(0, 50) };
  });

  check("log panel exists", async () => {
    const items = await page.locator(".log-item").count();
    return { pass: items > 0, detail: `${items} log entries` };
  });

  check("new game button works", async () => {
    await page.locator("#newGameBtn").click();
    await page.waitForTimeout(300);
    const count = await page.locator("#playerHand .tile").count();
    return { pass: count === 14, detail: `${count} tiles after new game` };
  });

  check("sound button exists", async () => {
    const visible = await page.locator("#soundBtn").isVisible();
    return { pass: visible, detail: "sound toggle visible" };
  });

  check("no JS errors on page", async () => {
    const errors = [];
    page.on("pageerror", (err) => errors.push(err));
    await page.locator("#newGameBtn").click();
    await page.waitForTimeout(500);
    return { pass: errors.length === 0, detail: errors.length ? errors.map(String).join("; ") : "clean" };
  });

  // Discard indicator checks — need to play through a discard cycle
  check("last-discard shows draw indicator after AI discards", async () => {
    // Click the rightmost tile to discard (human's first turn)
    const tiles = page.locator("#playerHand .tile");
    const count = await tiles.count();
    if (count === 0) return { pass: false, detail: "no tiles to discard" };
    await tiles.nth(count - 1).click();
    // Wait for AI draw + discard cycle (550ms turn delay + 550ms discard delay + buffer)
    await page.waitForTimeout(2000);
    // The lastDiscard should now show the AI's discard with indicator
    const card = page.locator(".last-discard-card");
    const visible = await card.isVisible().catch(() => false);
    if (!visible) return { pass: false, detail: "no last-discard-card visible" };
    const text = await card.locator("span").first().textContent();
    const hasText = /进牌打出|摸到即打/.test(text);
    return { pass: hasText, detail: `text: "${text.trim()}"` };
  });

  check("log contains draw-discard distinction for AI", async () => {
    // After human discards, AI should have drawn and discarded.
    // Log should contain either "进牌" or "刚摸的" for AI players.
    const logText = await page.locator("#logList").textContent();
    const hasDrawInfo = /进牌|刚摸的/.test(logText);
    return { pass: hasDrawInfo, detail: hasDrawInfo ? "log mentions draw vs held" : `log: ${logText.slice(0, 120)}` };
  });

  // ═══ RUN ═══
  console.log("\n╔══════════════════════════════════╗");
  console.log("║   Hangma UI Check               ║");
  console.log("╚══════════════════════════════════╝\n");

  let passed = 0;
  let failed = 0;
  for (const { name, fn } of CHECKS) {
    try {
      const result = await fn();
      if (result.pass) {
        console.log(`  ✅ ${name} — ${result.detail}`);
        passed++;
      } else {
        console.log(`  ❌ ${name} — ${result.detail}`);
        failed++;
      }
    } catch (e) {
      console.log(`  ❌ ${name} — ERROR: ${e.message}`);
      failed++;
    }
  }

  // Screenshot
  await page.screenshot({ path: join(__dirname, "..", "assets", "design", "ui-check.png"), fullPage: false });
  console.log(`\n  📸 Screenshot saved to ui-check.png`);

  // Mobile viewport check
  await page.setViewportSize({ width: 390, height: 844 });
  await page.waitForTimeout(400);
  await page.screenshot({ path: join(__dirname, "..", "assets", "design", "mobile-check.png"), fullPage: false });
  console.log(`  📸 Mobile screenshot saved to mobile-check.png`);

  console.log(`\n  Results: ${passed} passed, ${failed} failed out of ${CHECKS.length} checks\n`);

  await browser.close();
}

main().catch((err) => {
  console.error("Fatal:", err.message);
  process.exit(1);
});
