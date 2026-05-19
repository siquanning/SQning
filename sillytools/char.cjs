/**
 * SillyTavern character card tool - view & edit character cards as plain JSON.
 *
 * Usage:
 *   node char.cjs list                          List all characters
 *   node char.cjs show <name>                   Export character card to .char.json
 *   node char.cjs repack <name>                 Read .char.json back into PNG
 */

const lib = require('./lib.js');
const fs = require('fs');
const path = require('path');

const cmd = process.argv[2];
const arg = process.argv[3];

async function main() {
  if (!cmd || cmd === 'help') {
    console.log(`SillyTavern Character Tools
──────────────────────────
  node char.cjs list                List all characters
  node char.cjs show <name>         Export character card → .char.json
  node char.cjs repack <name>       Pack .char.json back → PNG
  node char.cjs show <name> --raw   Just print the JSON to stdout
`);
    return;
  }

  if (cmd === 'list') {
    const chars = lib.listChars();
    if (chars.length === 0) {
      console.log('No characters found.');
      return;
    }
    console.log('Characters:');
    console.log('-'.repeat(50));
    for (const c of chars) {
      console.log(`  ${c.name}  (${c.file})`);
    }
    return;
  }

  if (cmd === 'show') {
    if (!arg) { console.log('Usage: node char.cjs show <name>'); return; }
    const { card, file, path: pngPath } = lib.extractChar(arg);
    const jsonPath = pngPath.replace(/\.(png|webp)$/i, '.char.json');

    if (process.argv.includes('--raw')) {
      console.log(JSON.stringify(card, null, 2));
      return;
    }

    fs.writeFileSync(jsonPath, JSON.stringify(card, null, 2), 'utf8');
    console.log(`Exported: ${jsonPath}`);
    console.log(`Edit this file, then run:`);
    console.log(`  node char.cjs repack ${arg}`);
    return;
  }

  if (cmd === 'repack') {
    if (!arg) { console.log('Usage: node char.cjs repack <name>'); return; }
    const { card, file, path: pngPath } = lib.extractChar(arg);
    const jsonPath = pngPath.replace(/\.(png|webp)$/i, '.char.json');
    if (!fs.existsSync(jsonPath)) {
      console.log(`No .char.json found. Run "node char.cjs show ${arg}" first.`);
      return;
    }
    const updated = JSON.parse(fs.readFileSync(jsonPath, 'utf8'));
    const backup = lib.writeCharCard(pngPath, updated);
    console.log(`Repacked: ${pngPath}`);
    console.log(`Backup: ${backup}`);
    return;
  }

  console.log(`Unknown command: ${cmd}`);
}

main().catch(e => { console.error('Error:', e.message); process.exit(1); });
