// pdf2png.mjs — renders PDF pages to PNG using pdfjs-dist + @napi-rs/canvas
import { getDocument } from "pdfjs-dist/legacy/build/pdf.mjs";
import { createCanvas } from "@napi-rs/canvas";
import { writeFileSync, mkdirSync } from "fs";
import { join } from "path";

const pdfPath = process.argv[2];
const outDir = process.argv[3];
const scale = parseFloat(process.argv[4] || "2.0");

if (!pdfPath || !outDir) {
  console.error("Usage: node pdf2png.mjs <pdf-path> <out-dir> [scale]");
  process.exit(1);
}

mkdirSync(outDir, { recursive: true });

// Adapter: @napi-rs/canvas → pdfjs-dist NodeCanvasFactory API
function napiCanvasFactory(width, height) {
  const canvas = createCanvas(width, height);
  const ctx = canvas.getContext("2d");
  return { canvas, context: ctx };
}
napiCanvasFactory.create = napiCanvasFactory;
napiCanvasFactory.reset = function (c, w, h) { c.canvas.width = w; c.canvas.height = h; };
napiCanvasFactory.destroy = function () {};

const doc = await getDocument({
  data: new Uint8Array(await (await import("fs")).promises.readFile(pdfPath)),
  canvasFactory: napiCanvasFactory,
}).promise;

const total = doc.numPages;
console.log(`Total pages: ${total}`);

for (let i = 1; i <= total; i++) {
  const page = await doc.getPage(i);
  const viewport = page.getViewport({ scale });
  const w = Math.floor(viewport.width);
  const h = Math.floor(viewport.height);

  const canvas = createCanvas(w, h);
  const ctx = canvas.getContext("2d");

  await page.render({ canvasContext: ctx, viewport }).promise;

  const buf = canvas.toBuffer("image/png");
  const outPath = join(outDir, `page_${String(i).padStart(3, "0")}.png`);
  writeFileSync(outPath, buf);
  console.log(`Saved: ${outPath}  (${(buf.length / 1024).toFixed(1)} KB)`);
}
console.log("Done.");
