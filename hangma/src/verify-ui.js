const path = require("node:path");
const { pathToFileURL } = require("node:url");
const { chromium } = require("playwright");

(async () => {
  let browser;
  try {
    browser = await chromium.launch({ headless: true, channel: "msedge" });
  } catch {
    browser = await chromium.launch({ headless: true });
  }
  const page = await browser.newPage({ viewport: { width: 1366, height: 900 } });
  const errors = [];
  page.on("console", (message) => {
    if (message.type() === "error") {
      errors.push(message.text());
    }
  });
  page.on("pageerror", (error) => errors.push(error.message));

  await page.goto(pathToFileURL(path.join(__dirname, "index.html")).href);
  await page.waitForSelector(".tile.clickable");
  const tileCount = await page.locator("#playerHand .tile").count();
  if (tileCount !== 14) {
    throw new Error(`expected 14 player tiles, got ${tileCount}`);
  }

  const firstTile = page.locator("#playerHand .tile").first();
  const box = await firstTile.boundingBox();
  if (!box) {
    throw new Error("first player tile did not render a box");
  }
  const centerX = box.x + box.width / 2;
  const centerY = box.y + box.height / 2;
  await page.mouse.move(centerX, centerY);
  await page.mouse.down();
  await page.mouse.move(centerX, centerY - 70, { steps: 5 });
  await page.mouse.up();
  await page.waitForFunction(() => document.querySelector("#logList")?.innerText.includes("你打出"));
  await page.waitForTimeout(1500);

  const wallText = await page.locator("#wallCount").innerText();
  if (!wallText.startsWith("牌墙 ")) {
    throw new Error("wall count did not render");
  }
  if (errors.length > 0) {
    throw new Error(errors.join("\\n"));
  }

  await page.setViewportSize({ width: 390, height: 844 });
  await page.screenshot({ path: path.join(__dirname, "..", "assets", "design", "mobile-check.png"), fullPage: true });
  await page.setViewportSize({ width: 1366, height: 900 });
  await page.screenshot({ path: path.join(__dirname, "..", "assets", "design", "desktop-check.png"), fullPage: true });
  await browser.close();
  console.log("UI verification passed");
})().catch(async (error) => {
  console.error(error);
  process.exit(1);
});
