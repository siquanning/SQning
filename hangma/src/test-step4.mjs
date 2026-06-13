// ============================================================
// test-step4.mjs — 吃碰杠选项 smoke test
// 运行: node src/test-step4.mjs
// ============================================================

import { getChiOptions, getPengOptions, getConcealedGangChoices, tileKey } from './engine.js';

const WILD_KEY = 'honor:white';

function t(suit, rank, id) {
  return { id: id || `${suit}-${rank}-${Math.random()}`, suit, rank };
}

let passed = 0;
let failed = 0;

function test(name, fn) {
  try {
    fn();
    passed += 1;
  } catch (e) {
    failed += 1;
    console.error(`  ✗ ${name}: ${e.message}`);
  }
}

function assert(cond, msg) { if (!cond) throw new Error(msg); }

// ============================================================
// getChiOptions
// ============================================================

test('empty for honor discard', () => {
  const hand = [t('character', 1, 'a'), t('character', 2, 'b'), t('character', 3, 'c')];
  const opts = getChiOptions(hand, t('honor', 'east', 'e'), WILD_KEY);
  assert(opts.length === 0, 'should return empty for honor discard');
});

test('discard is lowest: chi with rank+1 and rank+2', () => {
  const hand = [t('character', 3, 'a'), t('character', 4, 'b')];
  const opts = getChiOptions(hand, t('character', 2, 'd'), WILD_KEY);
  assert(opts.length === 1, 'should have 1 chi option');
  assert(opts[0].length === 2, 'should return 2 tile ids');
  assert(opts[0].includes('a') && opts[0].includes('b'), 'should include both tiles');
});

test('discard is middle: chi with rank-1 and rank+1', () => {
  const hand = [t('character', 4, 'a'), t('character', 6, 'b')];
  const opts = getChiOptions(hand, t('character', 5, 'd'), WILD_KEY);
  assert(opts.length === 1, 'should have 1 chi option');
  assert(opts[0].includes('a') && opts[0].includes('b'), '4 and 6 with discard 5');
});

test('discard is highest: chi with rank-2 and rank-1', () => {
  const hand = [t('character', 7, 'a'), t('character', 8, 'b')];
  const opts = getChiOptions(hand, t('character', 9, 'd'), WILD_KEY);
  assert(opts.length === 1, 'should have 1 chi option');
  assert(opts[0].includes('a') && opts[0].includes('b'), '7 and 8 with discard 9');
});

test('multiple chi options for same discard', () => {
  // discard=5, hand has 3,4 and 6,7 → three chi ways: 3-4-5, 4-5-6, 5-6-7
  const hand = [
    t('character', 3, 'a'), t('character', 4, 'b'),
    t('character', 6, 'c'), t('character', 7, 'd'),
  ];
  const opts = getChiOptions(hand, t('character', 5, 'd'), WILD_KEY);
  assert(opts.length === 3, `should have 3 options, got ${opts.length}`);
});

test('chi with one wild tile', () => {
  const hand = [t('character', 3, 'a'), t('honor', 'white', 'w')];
  const opts = getChiOptions(hand, t('character', 4, 'd'), WILD_KEY);
  // discard=4: middle→needs 3,5 OR lowest→needs 5,6 OR highest→needs 2,3
  // Has char-3 + wild → chi type "highest" (discard=4 needs 2,3) uses [char-3, wild]
  // OR chi type "middle" needs 3,5 → [char-3, wild]
  assert(opts.length === 2, `should have 2 options (middle+highest), got ${opts.length}`);
});

test('chi with two wild tiles', () => {
  const hand = [t('honor', 'white', 'w1'), t('honor', 'white', 'w2')];
  const opts = getChiOptions(hand, t('character', 5, 'd'), WILD_KEY);
  assert(opts.length === 3, `should have 3 options (two wilds fill all 3 positions), got ${opts.length}`);
});

test('chi at edge: discard=1 only lowest type', () => {
  const hand = [t('character', 2, 'a'), t('character', 3, 'b')];
  const opts = getChiOptions(hand, t('character', 1, 'd'), WILD_KEY);
  assert(opts.length === 1, 'discard 1 should only have 1 chi type');
});

test('chi at edge: discard=9 only highest type', () => {
  const hand = [t('character', 7, 'a'), t('character', 8, 'b')];
  const opts = getChiOptions(hand, t('character', 9, 'd'), WILD_KEY);
  assert(opts.length === 1, 'discard 9 should only have 1 chi type');
});

test('no chi when tiles dont match', () => {
  const hand = [t('character', 1, 'a'), t('dot', 5, 'b')];
  const opts = getChiOptions(hand, t('character', 5, 'd'), WILD_KEY);
  assert(opts.length === 0, 'mismatched suits should give no options');
});

test('chi includes regular-only option alongside wild options', () => {
  // discard=3, hand has 1,2 AND a wild
  // chi types: highest(1-2-3)→[1,2], middle(2-3-4)→[2,wild]
  // Both are valid and should be returned
  const hand = [t('character', 1, 'a'), t('character', 2, 'b'), t('honor', 'white', 'w')];
  const opts = getChiOptions(hand, t('character', 3, 'd'), WILD_KEY);
  assert(opts.length === 2, `should have 2 options (1-2-3 and 2-wild-4), got ${opts.length}`);
  const regularOpt = opts.find(o => o.includes('a') && o.includes('b'));
  assert(regularOpt !== undefined, 'should include regular-tile option [1,2]');
});

// ============================================================
// getPengOptions
// ============================================================

test('peng: 2 matching tiles → peng', () => {
  const hand = [t('character', 1, 'a'), t('character', 1, 'b'), t('dot', 5, 'c')];
  const result = getPengOptions(hand, t('character', 1, 'd'), WILD_KEY);
  assert(result !== null, 'should return peng');
  assert(result.type === 'peng', 'should be peng type');
  assert(result.tiles.length === 2, 'should return 2 tiles');
});

test('peng: 3 matching tiles → ming gang (priority)', () => {
  const hand = [t('character', 1, 'a'), t('character', 1, 'b'), t('character', 1, 'c')];
  const result = getPengOptions(hand, t('character', 1, 'd'), WILD_KEY);
  assert(result !== null, '3 matches should return claim');
  assert(result.type === 'gang', '3+ matching should be gang');
  assert(result.tiles.length === 3, 'should return 3 tiles');
});

test('peng: wild tile cannot be claimed', () => {
  const hand = [t('honor', 'white', 'w1'), t('honor', 'white', 'w2'), t('dot', 5, 'c')];
  const result = getPengOptions(hand, t('honor', 'white', 'd'), WILD_KEY);
  assert(result === null, 'wild tile discard should not be claimable');
});

test('peng: only 1 matching → null', () => {
  const hand = [t('character', 1, 'a'), t('dot', 5, 'b')];
  const result = getPengOptions(hand, t('character', 1, 'd'), WILD_KEY);
  assert(result === null, '1 matching tile should not be enough');
});

test('peng: no match → null', () => {
  const hand = [t('dot', 5, 'a'), t('bamboo', 3, 'b')];
  const result = getPengOptions(hand, t('character', 1, 'd'), WILD_KEY);
  assert(result === null, 'no match should return null');
});

// ============================================================
// getConcealedGangChoices
// ============================================================

test('four identical tiles → concealed gang', () => {
  const hand = [
    t('character', 1, 'a'), t('character', 1, 'b'),
    t('character', 1, 'c'), t('character', 1, 'd'),
  ];
  const opts = getConcealedGangChoices(hand, WILD_KEY);
  assert(opts.length === 1, 'should find 1 gang');
  assert(opts[0].length === 4, 'should return 4 tiles');
});

test('five identical tiles → still one gang choice', () => {
  const hand = [
    t('character', 1, 'a'), t('character', 1, 'b'),
    t('character', 1, 'c'), t('character', 1, 'd'),
    t('character', 1, 'e'),
  ];
  const opts = getConcealedGangChoices(hand, WILD_KEY);
  assert(opts.length === 1, 'should find 1 gang (first 4)');
  assert(opts[0].length === 4, 'should return exactly 4 tiles');
});

test('wild tiles cannot form concealed gang', () => {
  const hand = [
    t('honor', 'white', 'w1'), t('honor', 'white', 'w2'),
    t('honor', 'white', 'w3'), t('honor', 'white', 'w4'),
  ];
  const opts = getConcealedGangChoices(hand, WILD_KEY);
  assert(opts.length === 0, 'wild tiles should not form concealed gang');
});

test('less than 4 identical → no gang', () => {
  const hand = [
    t('character', 1, 'a'), t('character', 1, 'b'),
    t('character', 1, 'c'),
  ];
  const opts = getConcealedGangChoices(hand, WILD_KEY);
  assert(opts.length === 0, '3 identical should not form gang');
});

test('multiple concealed gang choices', () => {
  const hand = [
    t('character', 1, 'a1'), t('character', 1, 'a2'),
    t('character', 1, 'a3'), t('character', 1, 'a4'),
    t('dot', 5, 'b1'), t('dot', 5, 'b2'),
    t('dot', 5, 'b3'), t('dot', 5, 'b4'),
  ];
  const opts = getConcealedGangChoices(hand, WILD_KEY);
  assert(opts.length === 2, 'should find 2 gang choices');
});

test('mixed hand with one gang', () => {
  const hand = [
    t('character', 1, 'a1'), t('character', 1, 'a2'),
    t('character', 1, 'a3'), t('character', 1, 'a4'),
    t('dot', 2, 'b1'), t('dot', 3, 'b2'),
    t('honor', 'east', 'c1'), t('honor', 'white', 'w1'),
  ];
  const opts = getConcealedGangChoices(hand, WILD_KEY);
  assert(opts.length === 1, 'should find 1 gang among mixed tiles');
  assert(tileKey(opts[0][0]) === 'character:1', 'gang should be character:1');
});

// ============================================================
console.log(`\n${passed} passed, ${failed} failed\n`);
if (failed > 0) process.exit(1);
