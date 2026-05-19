// pdf2png-mupdf.mjs — render PDF to PNG using mupdf.js (WASM)
import mupdf from "mupdf";
import { writeFileSync, mkdirSync } from "fs";
import { join } from "path";

const pdfPath = process.argv[2];
const outDir = process.argv[3];
const scale = parseFloat(process.argv[4] || "2.0");

if (!pdfPath || !outDir) {
  console.error("Usage: node pdf2png-mupdf.mjs <pdf-path> <out-dir> [scale]");
  process.exit(1);
}

mkdirSync(outDir, { recursive: true });

const doc = mupdf.Document.openDocument(pdfPath, "application/pdf");
const total = doc.countPages();
console.log(`Total pages: ${total}`);

for (let i = 0; i < total; i++) {
  const page = doc.loadPage(i);
  const bounds = page.getBounds();
  const matrix = mupdf.Matrix.scale(scale, scale);

  const pixmap = page.toPixmap(matrix, mupdf.ColorSpace.DeviceRGB);
  const pngData = pixmap.asPNG();
  const outPath = join(outDir, `page_${String(i + 1).padStart(3, "0")}.png`);
  writeFileSync(outPath, pngData);
  console.log(`Saved: ${outPath}  (${(pngData.length / 1024).toFixed(1)} KB)`);
}
console.log("Done.");
