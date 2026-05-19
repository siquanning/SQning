const fs = require('fs');
const path = require('path');

const SIL_BASE = 'D:/SillyTavern-1.17.0/data/default-user';
const CHARS_DIR = path.join(SIL_BASE, 'characters');
const WORLDS_DIR = path.join(SIL_BASE, 'worlds');

// === PNG Character Card v3 ===

function readCharCard(pngPath) {
  const buf = fs.readFileSync(pngPath);
  let offset = 8;
  while (offset < buf.length) {
    const len = buf.readUInt32BE(offset);
    const type = buf.toString('ascii', offset + 4, offset + 8);
    if (type === 'tEXt') {
      const chunkData = buf.slice(offset + 8, offset + 8 + len);
      const nullIdx = chunkData.indexOf(0);
      const keyword = chunkData.toString('ascii', 0, nullIdx);
      if (keyword === 'ccv3') {
        const b64 = chunkData.slice(nullIdx + 1).toString('utf8');
        return JSON.parse(Buffer.from(b64, 'base64').toString('utf8'));
      }
    }
    offset += 12 + len;
  }
  throw new Error('No ccv3 chunk found in PNG');
}

function writeCharCard(pngPath, json) {
  const buf = fs.readFileSync(pngPath);
  const b64 = Buffer.from(JSON.stringify(json), 'utf8').toString('base64');
  const ccv3Content = Buffer.from('ccv3\0' + b64, 'utf8');
  const lenBuf = Buffer.alloc(4);
  lenBuf.writeUInt32BE(ccv3Content.length, 0);
  const typeBuf = Buffer.from('tEXt', 'ascii');
  const crcData = Buffer.concat([typeBuf, ccv3Content]);
  const crc = crc32(crcData);

  let offset = 8;
  const chunks = [];
  let replaced = false;
  while (offset < buf.length) {
    const len = buf.readUInt32BE(offset);
    const type = buf.toString('ascii', offset + 4, offset + 8);
    if (type === 'tEXt') {
      const chunkData = buf.slice(offset + 8, offset + 8 + len);
      const nullIdx = chunkData.indexOf(0);
      const keyword = chunkData.toString('ascii', 0, nullIdx);
      if (keyword === 'ccv3') {
        chunks.push(lenBuf);
        chunks.push(typeBuf);
        chunks.push(ccv3Content);
        chunks.push(crc);
        replaced = true;
        offset += 12 + len;
        continue;
      }
    }
    chunks.push(buf.slice(offset, offset + 4));
    chunks.push(buf.slice(offset + 4, offset + 8));
    chunks.push(buf.slice(offset + 8, offset + 8 + len));
    chunks.push(buf.slice(offset + 8 + len, offset + 12 + len));
    offset += 12 + len;
  }

  if (!replaced) {
    const insertAt = chunks.length - 4;
    chunks.splice(insertAt, 0, lenBuf, typeBuf, ccv3Content, crc);
  }

  const outPath = pngPath.replace(/\.png$/, '.backup.png');
  fs.writeFileSync(outPath, buf);
  fs.writeFileSync(pngPath, Buffer.concat(chunks));
  return outPath;
}

function crc32(buf) {
  let crc = 0xFFFFFFFF;
  for (let i = 0; i < buf.length; i++) {
    crc ^= buf[i];
    for (let j = 0; j < 8; j++) {
      if (crc & 1) crc = (crc >>> 1) ^ 0xEDB88320;
      else crc >>>= 1;
    }
  }
  const result = Buffer.alloc(4);
  result.writeUInt32BE((crc ^ 0xFFFFFFFF) >>> 0, 0);
  return result;
}

// === Character management ===

function listChars() {
  const files = fs.readdirSync(CHARS_DIR);
  const chars = [];
  for (const f of files) {
    const full = path.join(CHARS_DIR, f);
    if (f.endsWith('.png') || f.endsWith('.webp')) {
      try {
        const card = readCharCard(full);
        chars.push({ file: f, name: card.data?.name || card.name, path: full });
      } catch {
        // Not a character card (may be expression sprite)
      }
    }
  }
  return chars;
}

function extractChar(nameOrFile) {
  const chars = listChars();
  const match = chars.find(c => c.name === nameOrFile || c.file === nameOrFile);
  if (!match) throw new Error(`Character not found: ${nameOrFile}`);
  const card = readCharCard(match.path);
  return { card, file: match.file, path: match.path };
}

// === World book management ===

function listWorlds() {
  const files = fs.readdirSync(WORLDS_DIR).filter(f => f.endsWith('.json'));
  return files.map(f => ({
    file: f,
    name: f.replace(/\.json$/, ''),
    path: path.join(WORLDS_DIR, f)
  }));
}

function readWorld(worldName) {
  const worldPath = path.join(WORLDS_DIR, worldName.endsWith('.json') ? worldName : `${worldName}.json`);
  if (!fs.existsSync(worldPath)) throw new Error(`World not found: ${worldName}`);
  return JSON.parse(fs.readFileSync(worldPath, 'utf8'));
}

function writeWorld(worldName, data) {
  const worldPath = path.join(WORLDS_DIR, worldName.endsWith('.json') ? worldName : `${worldName}.json`);
  fs.writeFileSync(worldPath, JSON.stringify(data, null, 2), 'utf8');
}

let _nextId = Date.now();

function addWorldEntry(worldName, entry) {
  const world = readWorld(worldName);
  if (!world.entries) world.entries = {};
  const id = entry.id || _nextId++;
  world.entries[id] = { id, keys: [], secondary_keys: [], comment: '', content: '', constant: false, selective: false, ...entry, id };
  writeWorld(worldName, world);
  return id;
}

function removeWorldEntry(worldName, id) {
  const world = readWorld(worldName);
  delete world.entries[id];
  writeWorld(worldName, world);
}

module.exports = {
  SIL_BASE, CHARS_DIR, WORLDS_DIR,
  readCharCard, writeCharCard,
  listChars, extractChar,
  listWorlds, readWorld, writeWorld,
  addWorldEntry, removeWorldEntry
};
