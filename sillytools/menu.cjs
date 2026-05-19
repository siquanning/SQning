/**
 * SillyTavern 管理工具 - 交互菜单
 * 在终端运行: node menu.cjs
 */
const lib = require('./lib.js');
const fs = require('fs');
const path = require('path');
const readline = require('readline');

const rl = readline.createInterface({ input: process.stdin, output: process.stdout });

function ask(q) { return new Promise(resolve => rl.question(q, resolve)); }

const MENUS = {};

async function mainMenu() {
  console.log('\n' + '='.repeat(50));
  console.log('  SillyTavern 管理工具');
  console.log('='.repeat(50));
  console.log('  1. 角色卡管理');
  console.log('  2. 世界书管理');
  console.log('  0. 退出');
  console.log('');

  const c = await ask('选择 > ');
  if (c === '1') await charMenu();
  else if (c === '2') await worldMenu();
  else if (c === '0') { rl.close(); return; }
  else console.log('无效选择');
  await mainMenu();
}

async function charMenu() {
  console.log('\n--- 角色卡管理 ---');
  const chars = lib.listChars();
  if (chars.length === 0) { console.log('没有角色'); return; }

  chars.forEach((c, i) => console.log(`  ${i + 1}. ${c.name} (${c.file})`));
  console.log('  0. 返回');

  const c = await ask('\n选择角色 > ');
  const idx = parseInt(c) - 1;
  if (idx >= 0 && idx < chars.length) {
    await charActions(chars[idx]);
  }
}

async function charActions(ch) {
  console.log(`\n当前角色: ${ch.name}`);
  console.log('  1. 导出为 .char.json');
  console.log('  2. 查看 JSON (控制台)');
  console.log('  3. 从 .char.json 写回 PNG');
  console.log('  0. 返回');

  const c = await ask('操作 > ');
  if (c === '1') {
    const { card, path: pngPath } = lib.extractChar(ch.name);
    const jsonPath = pngPath.replace(/\.(png|webp)$/i, '.char.json');
    fs.writeFileSync(jsonPath, JSON.stringify(card, null, 2), 'utf8');
    console.log(`已导出: ${jsonPath}`);
    console.log('在 VS Code 中可以直接打开编辑，编辑完后选择操作 3 写回');
  } else if (c === '2') {
    const { card } = lib.extractChar(ch.name);
    console.log(JSON.stringify(card, null, 2));
  } else if (c === '3') {
    const { card, path: pngPath } = lib.extractChar(ch.name);
    const jsonPath = pngPath.replace(/\.(png|webp)$/i, '.char.json');
    if (!fs.existsSync(jsonPath)) {
      console.log('请先导出 (操作 1)');
      return;
    }
    const updated = JSON.parse(fs.readFileSync(jsonPath, 'utf8'));
    const backup = lib.writeCharCard(pngPath, updated);
    console.log(`已写回: ${pngPath}  (备份: ${backup})`);
  }
  if (c !== '0') await charActions(ch);
}

async function worldMenu() {
  console.log('\n--- 世界书管理 ---');
  const worlds = lib.listWorlds();
  if (worlds.length === 0) { console.log('没有世界书'); return; }

  worlds.forEach((w, i) => {
    const data = lib.readWorld(w.name);
    const count = Object.keys(data.entries || {}).length;
    console.log(`  ${i + 1}. ${w.name} (${count} 条词条)`);
  });
  console.log('  0. 返回');

  const c = await ask('\n选择世界书 > ');
  const idx = parseInt(c) - 1;
  if (idx >= 0 && idx < worlds.length) {
    await worldActions(worlds[idx].name);
  }
}

async function worldActions(name) {
  console.log(`\n当前世界书: ${name}`);
  console.log('  1. 查看所有词条');
  console.log('  2. 添加词条');
  console.log('  3. 删除词条');
  console.log('  4. 导出编辑 (.edit.json)');
  console.log('  5. 写回 (.edit.json → 世界书)');
  console.log('  0. 返回');

  const c = await ask('操作 > ');
  if (c === '1') {
    const data = lib.readWorld(name);
    const entries = data.entries || {};
    const ids = Object.keys(entries);
    if (ids.length === 0) { console.log('(空)'); return await worldActions(name); }
    for (const id of ids) {
      const e = entries[id];
      console.log(`\n  #${id}  [${(e.keys || []).join(', ')}]`);
      if (e.comment) console.log(`  备注: ${e.comment}`);
      console.log(`  ${e.content}`);
    }
  } else if (c === '2') {
    const key = await ask('关键词 (逗号分隔) > ');
    const content = await ask('内容 > ');
    const comment = await ask('备注 (可选) > ');
    const id = lib.addWorldEntry(name, {
      keys: key.split(',').map(k => k.trim()).filter(Boolean),
      content,
      comment
    });
    console.log(`已添加 #${id}`);
  } else if (c === '3') {
    const data = lib.readWorld(name);
    const entries = data.entries || {};
    const ids = Object.keys(entries);
    if (ids.length === 0) { console.log('没有可删除的词条'); return await worldActions(name); }
    for (const id of ids) {
      console.log(`  #${id}  [${(entries[id].keys || []).join(', ')}]  ${entries[id].content.slice(0, 40)}...`);
    }
    const id = await ask('要删除的 ID > ');
    lib.removeWorldEntry(name, parseInt(id));
    console.log(`已删除 #${id}`);
  } else if (c === '4') {
    const worldPath = path.join(lib.WORLDS_DIR, name.endsWith('.json') ? name : `${name}.json`);
    const editPath = worldPath.replace('.json', '.edit.json');
    const data = lib.readWorld(name);
    fs.writeFileSync(editPath, JSON.stringify(data, null, 2), 'utf8');
    console.log(`已导出: ${editPath}`);
    console.log('在 VS Code 中编辑完后，选择操作 5 写回');
  } else if (c === '5') {
    const worldPath = path.join(lib.WORLDS_DIR, name.endsWith('.json') ? name : `${name}.json`);
    const editPath = worldPath.replace('.json', '.edit.json');
    if (!fs.existsSync(editPath)) { console.log('请先导出 (操作 4)'); return await worldActions(name); }
    const updated = JSON.parse(fs.readFileSync(editPath, 'utf8'));
    const backup = worldPath.replace('.json', '.backup.json');
    fs.copyFileSync(worldPath, backup);
    fs.writeFileSync(worldPath, JSON.stringify(updated, null, 2), 'utf8');
    console.log(`已写回: ${worldPath}  (备份: ${backup})`);
  }
  if (c !== '0') await worldActions(name);
}

mainMenu().catch(e => { console.error(e); rl.close(); });
