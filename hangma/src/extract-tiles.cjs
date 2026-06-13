const fs = require('fs');
const path = require('path');
const fontPath = path.join(__dirname, 'assets', 'Mahjong.Colored.otf');
const outDir = path.join(__dirname, 'assets', 'tiles');
const font = fs.readFileSync(fontPath);

const tableCount = font.readUInt16BE(4);
const tables = {};
for (let i = 0; i < tableCount; i += 1) {
  const offset = 12 + i * 16;
  tables[font.toString('latin1', offset, offset + 4)] = {
    offset: font.readUInt32BE(offset + 8),
    length: font.readUInt32BE(offset + 12),
  };
}

const svgTable = tables['SVG '];
if (!svgTable) {
  throw new Error('Mahjong.Colored.otf has no SVG table');
}
const svgBase = svgTable.offset;
const svgIndex = svgBase + font.readUInt32BE(svgBase + 2);
const svgCount = font.readUInt16BE(svgIndex);
const glyphSvgs = new Map();

for (let i = 0; i < svgCount; i += 1) {
  const entry = svgIndex + 2 + i * 12;
  const startGlyph = font.readUInt16BE(entry);
  const endGlyph = font.readUInt16BE(entry + 2);
  const svgOffset = font.readUInt32BE(entry + 4);
  const svgLength = font.readUInt32BE(entry + 8);
  let svg = font.toString('utf8', svgIndex + svgOffset, svgIndex + svgOffset + svgLength);
  const start = svg.indexOf('<svg');
  const end = svg.lastIndexOf('</svg>');
  if (start === -1 || end === -1) {
    continue;
  }
  svg = svg.slice(start, end + 6);
  svg = svg.replace('<svg ', '<svg viewBox="0 0 300 401" width="300" height="401" ');
  svg = svg.replace('<g transform="translate(0,-1638.4) scale(5.12)">', '<g>');
  for (let glyph = startGlyph; glyph <= endGlyph; glyph += 1) {
    glyphSvgs.set(glyph, svg);
  }
}

const tileCodepoints = {
  'honor-east': 0x1f000,
  'honor-south': 0x1f001,
  'honor-west': 0x1f002,
  'honor-north': 0x1f003,
  'honor-red': 0x1f004,
  'honor-green': 0x1f005,
  'honor-white': 0x1f006,
  'character-1': 0x1f007,
  'character-2': 0x1f008,
  'character-3': 0x1f009,
  'character-4': 0x1f00a,
  'character-5': 0x1f00b,
  'character-6': 0x1f00c,
  'character-7': 0x1f00d,
  'character-8': 0x1f00e,
  'character-9': 0x1f00f,
  'bamboo-1': 0x1f010,
  'bamboo-2': 0x1f011,
  'bamboo-3': 0x1f012,
  'bamboo-4': 0x1f013,
  'bamboo-5': 0x1f014,
  'bamboo-6': 0x1f015,
  'bamboo-7': 0x1f016,
  'bamboo-8': 0x1f017,
  'bamboo-9': 0x1f018,
  'dot-1': 0x1f019,
  'dot-2': 0x1f01a,
  'dot-3': 0x1f01b,
  'dot-4': 0x1f01c,
  'dot-5': 0x1f01d,
  'dot-6': 0x1f01e,
  'dot-7': 0x1f01f,
  'dot-8': 0x1f020,
  'dot-9': 0x1f021,
};

fs.rmSync(outDir, { recursive: true, force: true });
fs.mkdirSync(outDir, { recursive: true });

for (const [name, codepoint] of Object.entries(tileCodepoints)) {
  const glyphId = 7 + (codepoint - 0x1f000);
  const svg = glyphSvgs.get(glyphId);
  if (!svg) {
    throw new Error(`missing glyph ${glyphId} for ${name}`);
  }
  fs.writeFileSync(path.join(outDir, `${name}.svg`), svg, 'utf8');
}

console.log(`Wrote ${Object.keys(tileCodepoints).length} Mahjong SVG tiles to ${outDir}`);
