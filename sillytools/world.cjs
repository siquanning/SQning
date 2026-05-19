/**
 * SillyTavern world book tool - manage lorebooks as plain JSON.
 *
 * Usage:
 *   node world.cjs list                         List all world books
 *   node world.cjs show <name>                  Print world book (or save to .edit.json)
 *   node world.cjs add <name> <key> <content>   Add a new lore entry
 *   node world.cjs remove <name> <id>           Remove a lore entry
 *   node world.cjs pack <name>                  Read .edit.json back into world file
 */

const lib = require('./lib.js');
const fs = require('fs');
const path = require('path');

const cmd = process.argv[2];
const arg = process.argv[3];

function fmtEntry(id, entry) {
  const keys = (entry.keys || []).join(', ');
  const content = (entry.content || '').slice(0, 80).replace(/\n/g, ' ');
  const comment = entry.comment ? ` [${entry.comment}]` : '';
  return `  #${id}  keys: [${keys}]  "${content}..."${comment}`;
}

async function main() {
  if (!cmd || cmd === 'help') {
    console.log(`SillyTavern World Book Tools
────────────────────────────
  node world.cjs list                    List all world books
  node world.cjs show <name>             Show all entries in a world book
  node world.cjs show <name> --raw       Print raw JSON to stdout
  node world.cjs show <name> --edit      Save a copy as .edit.json for editing
  node world.cjs pack <name>             Pack .edit.json → world file
  node world.cjs add <name> <key> <content> [comment]
  node world.cjs remove <name> <id>
`);
    return;
  }

  if (cmd === 'list') {
    const worlds = lib.listWorlds();
    if (worlds.length === 0) {
      console.log('No world books found.');
      return;
    }
    console.log('World books:');
    console.log('-'.repeat(50));
    for (const w of worlds) {
      const data = lib.readWorld(w.name);
      const count = Object.keys(data.entries || {}).length;
      console.log(`  ${w.name}  (${count} entries)  [${w.file}]`);
    }
    return;
  }

  if (cmd === 'show') {
    if (!arg) { console.log('Usage: node world.cjs show <name>'); return; }
    const data = lib.readWorld(arg);
    const worldPath = path.join(lib.WORLDS_DIR, arg.endsWith('.json') ? arg : `${arg}.json`);

    if (process.argv.includes('--raw')) {
      console.log(JSON.stringify(data, null, 2));
      return;
    }

    if (process.argv.includes('--edit')) {
      const editPath = worldPath.replace('.json', '.edit.json');
      fs.writeFileSync(editPath, JSON.stringify(data, null, 2), 'utf8');
      console.log(`Saved: ${editPath}`);
      console.log(`Edit it, then run:`);
      console.log(`  node world.cjs pack ${arg}`);
      return;
    }

    const entries = data.entries || {};
    const ids = Object.keys(entries);
    console.log(`World book: ${arg}  (${ids.length} entries)`);
    console.log('-'.repeat(50));
    for (const id of ids) {
      console.log(fmtEntry(id, entries[id]));
    }
    return;
  }

  if (cmd === 'pack') {
    if (!arg) { console.log('Usage: node world.cjs pack <name>'); return; }
    const worldPath = path.join(lib.WORLDS_DIR, arg.endsWith('.json') ? arg : `${arg}.json`);
    const editPath = worldPath.replace('.json', '.edit.json');
    if (!fs.existsSync(editPath)) {
      console.log(`No .edit.json found. Run "node world.cjs show ${arg} --edit" first.`);
      return;
    }
    const updated = JSON.parse(fs.readFileSync(editPath, 'utf8'));
    const backup = worldPath.replace('.json', '.backup.json');
    fs.copyFileSync(worldPath, backup);
    fs.writeFileSync(worldPath, JSON.stringify(updated, null, 2), 'utf8');
    console.log(`Packed: ${worldPath}`);
    console.log(`Backup: ${backup}`);
    return;
  }

  if (cmd === 'add') {
    if (!arg || !process.argv[4]) {
      console.log('Usage: node world.cjs add <name> <key> <content> [comment]');
      return;
    }
    const key = process.argv[4];
    const content = process.argv[5] || '';
    const comment = process.argv[6] || '';
    const id = lib.addWorldEntry(arg, {
      keys: key.split(',').map(k => k.trim()),
      content,
      comment,
      constant: false,
      selective: false
    });
    console.log(`Added entry #${id} to "${arg}"`);
    console.log(fmtEntry(id, lib.readWorld(arg).entries[id]));
    return;
  }

  if (cmd === 'remove') {
    if (!arg || !process.argv[4]) {
      console.log('Usage: node world.cjs remove <name> <id>');
      return;
    }
    lib.removeWorldEntry(arg, parseInt(process.argv[4]));
    console.log(`Removed entry #${process.argv[4]} from "${arg}"`);
    return;
  }

  console.log(`Unknown command: ${cmd}`);
}

main().catch(e => { console.error('Error:', e.message); process.exit(1); });
