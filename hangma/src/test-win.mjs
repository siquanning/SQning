// ============================================================
// test-win.mjs — 胡牌判定 smoke test
// 运行: node src/test-win.mjs
// ============================================================

import { isWinningHand, tileKey } from './engine.js';

const WILD_KEY = 'honor:white';

function t(suit, rank) {
  return { suit, rank };
}

function assert(condition, msg) {
  if (!condition) throw new Error(`FAIL: ${msg}`);
  return true;
}

function assertWin(tiles, msg) {
  assert(isWinningHand(tiles, WILD_KEY), msg);
}

function assertNotWin(tiles, msg) {
  assert(!isWinningHand(tiles, WILD_KEY), msg);
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

// ============================================================
// 平胡 — 无财神
// ============================================================

test('4 sequences + 1 pair (all sequences)', () => {
  const hand = [
    t('character',1),t('character',2),t('character',3),
    t('dot',2),t('dot',3),t('dot',4),
    t('bamboo',4),t('bamboo',5),t('bamboo',6),
    t('bamboo',7),t('bamboo',8),t('bamboo',9),
    t('honor','east'),t('honor','east'),
  ];
  assertWin(hand, '4 sequences + 1 pair');
});

test('3 sequences + 1 triplet + 1 pair', () => {
  const hand = [
    t('character',1),t('character',2),t('character',3),
    t('dot',3),t('dot',3),t('dot',3),
    t('bamboo',5),t('bamboo',6),t('bamboo',7),
    t('honor','east'),t('honor','east'),
    t('honor','red'),t('honor','red'),t('honor','red'),
  ];
  assertWin(hand, '3 sequences + 1 triplet + 1 pair');
});

test('all triplets + 1 pair', () => {
  const hand = [
    t('character',1),t('character',1),t('character',1),
    t('dot',3),t('dot',3),t('dot',3),
    t('bamboo',6),t('bamboo',6),t('bamboo',6),
    t('honor','east'),t('honor','east'),t('honor','east'),
    t('honor','red'),t('honor','red'),
  ];
  assertWin(hand, 'all triplets + 1 pair');
});

test('mixed triplets + sequences + pair', () => {
  const hand = [
    t('character',1),t('character',1),t('character',1),
    t('dot',2),t('dot',3),t('dot',4),
    t('bamboo',5),t('bamboo',5),t('bamboo',5),
    t('bamboo',7),t('bamboo',8),t('bamboo',9),
    t('honor','green'),t('honor','green'),
  ];
  assertWin(hand, '2 triplets + 2 sequences + 1 pair');
});

test('pair is part of a suit sequence hand', () => {
  const hand = [
    t('character',1),t('character',2),t('character',3),
    t('character',4),t('character',5),t('character',6),
    t('character',7),t('character',8),t('character',9),
    t('dot',1),t('dot',1),
    t('bamboo',2),t('bamboo',3),t('bamboo',4),
  ];
  assertWin(hand, '1-9 run + pair + sequence');
});

// ============================================================
// 平胡 — 有财神
// ============================================================

test('1 wild as pair, rest form 4 melds', () => {
  const hand = [
    t('character',1),t('character',2),t('character',3),
    t('dot',2),t('dot',3),t('dot',4),
    t('bamboo',3),t('bamboo',4),t('bamboo',5),
    t('bamboo',6),t('bamboo',7),t('bamboo',8),
    t('honor','east'), // single — no pair needed, wild is the pair
    t('honor','white'), // 1 wild = pair
  ];
  assertWin(hand, '1 wild as pair');
});

test('2 wilds: 1 for pair, 1 to complete triplet', () => {
  const hand = [
    t('character',1),t('character',2),t('character',3),
    t('dot',2),t('dot',3),t('dot',4),
    t('bamboo',3),t('bamboo',4),t('bamboo',5),
    t('honor','east'),t('honor','east'), // 2 easts + 1 wild = triplet
    t('bamboo',9), // pairs with 1 wild = 将牌
    t('honor','white'),t('honor','white'), // 2 wilds: 1 in meld, 1 in pair
  ];
  assertWin(hand, '2 wilds: 1 in triplet + 1 in pair');
});

test('2 wilds: wild fills char-2 in sequence', () => {
  const hand = [
    t('character',1),t('character',3), // missing char-2, wild fills it
    t('dot',2),t('dot',3),t('dot',4),
    t('bamboo',3),t('bamboo',4),t('bamboo',5),
    t('bamboo',6),t('bamboo',7),t('bamboo',8),
    t('bamboo',9), // pairs with 1 wild
    t('honor','white'),t('honor','white'),
  ];
  assertWin(hand, '2 wilds: wild fills char-2 in sequence');
});

test('3 wilds: pair + 2 wilds in melds', () => {
  const hand = [
    t('character',1),t('character',2),t('character',3),
    t('dot',2),t('dot',3),t('dot',4),
    t('bamboo',3),t('bamboo',4),t('bamboo',5),
    t('honor','east'), // 1 east + 2 wilds = triplet
    t('bamboo',9), // pairs with 1 wild
    t('honor','white'),t('honor','white'),t('honor','white'),
  ];
  assertWin(hand, '3 wilds: 2 in triplet + 1 in pair');
});

test('4 wilds: extreme case', () => {
  const hand = [
    t('character',1),t('character',2),t('character',3),
    t('dot',2),t('dot',3),t('dot',4),
    t('bamboo',3),t('bamboo',4),t('bamboo',5),
    t('bamboo',6), // 1 bam-6 + 2 wilds = 1 triplet (but we only have 1 meld left with 3 wilds total)
    // Wait: 3 sequences from 9 tiles above, 1 bam-6 leftover, need 1 more meld.
    // 1 bam-6 + 3 wilds... hmm that's 4 tiles for 1 meld, too many.
    // Actually: 9 tiles form 3 sequences. Remaining: 1 bam-6 + 4 wilds = 5 tiles.
    // Need: 1 more meld (3 tiles) + 1 pair (1 wild).
    // 1 wild = pair, 3 wilds + 1 bam-6 = can form 1 meld with 2 wilds...
    // Actually: 1 wild = pair, 3 wilds left for melds. 1 bam-6 + 2 wilds = triplet. 1 wild left over.
    // 5 tiles total: consumed 3 (triplet) + 1 (pair) = 4, leaving 1 wild... Hmm.
    // Let me reconsider the test case. I need exactly 14 tiles that work with 4 wilds.
    t('honor','white'),t('honor','white'),t('honor','white'),t('honor','white'),
  ];
  // 3 seqs (9) + 1 bam6 + 4 wilds = 14
  // 4 wilds: 1 for pair, 3 for melds
  // 3 melds already formed. Need 1 more meld from: bam6 + 3 wilds
  // bam6 + 2 wilds = triplet → 1 wild leftover... but that's fine, we already have 4 melds.
  // Wait, we need exactly 4 melds. We have 3 seqs = 3 melds. bam6 + 2 wilds = 1 meld (triplet).
  // 1 wild = pair. Total = 3+1+1 = 5... no.
  // 3 seqs (melds) + 1 bam6+2wild (meld) = 4 melds.
  // 1 wild pair = 将牌. Total consumed: 9 (seqs) + 3 (meld: bam6 + 2 wilds) + 1 (pair wild) = 13.
  // But we have 14 tiles! 9 + 1 + 4 = 14. OK: 9 consumed in seqs, bam6 + 2 wilds consumed in last meld,
  // 1 wild consumed as pair. Total = 9 + 1 + 2 + 1 = 13. But we had 4 wilds and used 3...
  // Hmm, 1 wild remains unused. canFormSets with 3 wilds and 1 tile and 1 needed set:
  // bam6(1) + 3 wilds ≥ 3 → use 1 tile + 2 wilds = triplet. 1 wild left. neededSets becomes 0. Returns true.
  // So 3 wilds consumed in melds (2 used, 1 not). 1 wild consumed as pair. Total = 14. ✓
  assertWin(hand, '4 wilds: extreme case');
});

// ============================================================
// 七对
// ============================================================

test('7 pairs, no wilds', () => {
  const hand = [
    t('character',1),t('character',1),
    t('character',3),t('character',3),
    t('dot',2),t('dot',2),
    t('dot',4),t('dot',4),
    t('bamboo',5),t('bamboo',5),
    t('bamboo',7),t('bamboo',7),
    t('honor','east'),t('honor','east'),
  ];
  assertWin(hand, '7 pairs no wilds');
});

test('豪华七对: 4 same tiles as 2 pairs', () => {
  const hand = [
    t('character',1),t('character',1),t('character',1),t('character',1),
    t('character',3),t('character',3),
    t('dot',2),t('dot',2),
    t('dot',4),t('dot',4),
    t('bamboo',5),t('bamboo',5),
    t('bamboo',7),t('bamboo',7),
  ];
  assertWin(hand, '豪华七对: 4 char-1 as 2 pairs');
});

test('6 pairs + 2 wilds as a pair', () => {
  const hand = [
    t('character',1),t('character',1),
    t('character',3),t('character',3),
    t('dot',2),t('dot',2),
    t('dot',4),t('dot',4),
    t('bamboo',5),t('bamboo',5),
    t('bamboo',7),t('bamboo',7),
    t('honor','white'),t('honor','white'),
  ];
  assertWin(hand, '6 pairs + 2 wilds as pair');
});

test('6 pairs + 1 single + 1 wild (wild completes pair)', () => {
  const hand = [
    t('character',1),t('character',1),
    t('character',3),t('character',3),
    t('dot',2),t('dot',2),
    t('dot',4),t('dot',4),
    t('bamboo',5),t('bamboo',5),
    t('bamboo',7),t('bamboo',7),
    t('honor','east'), // single
    t('honor','white'), // wild
  ];
  assertWin(hand, '6 pairs + 1 single + 1 wild');
});

test('5 pairs + 2 singles + 2 wilds', () => {
  const hand = [
    t('character',1),t('character',1),
    t('character',3),t('character',3),
    t('dot',2),t('dot',2),
    t('dot',4),t('dot',4),
    t('bamboo',5),t('bamboo',5),
    t('honor','east'),t('honor','north'), // 2 singles
    t('honor','white'),t('honor','white'), // 2 wilds
  ];
  assertWin(hand, '5 pairs + 2 singles + 2 wilds');
});

// ============================================================
// 非胡牌
// ============================================================

test('wrong tile count: 13', () => {
  const hand = Array(13).fill(t('character',1));
  assertNotWin(hand, '13 tiles should not win');
});

test('wrong tile count: 15', () => {
  const hand = Array(15).fill(t('character',1));
  assertNotWin(hand, '15 tiles should not win');
});

test('4 melds but no pair (0 wilds)', () => {
  const hand = [
    t('character',1),t('character',2),t('character',3),
    t('dot',2),t('dot',3),t('dot',4),
    t('bamboo',3),t('bamboo',4),t('bamboo',5),
    t('bamboo',6),t('bamboo',7),t('bamboo',8),
    t('honor','east'),t('honor','red'), // 2 different singles, not a pair
  ];
  assertNotWin(hand, '4 melds but no pair');
});

test('3 melds + 1 pair + 3 singles', () => {
  const hand = [
    t('character',1),t('character',2),t('character',3),
    t('dot',2),t('dot',3),t('dot',4),
    t('bamboo',3),t('bamboo',4),t('bamboo',5),
    t('honor','east'),t('honor','east'),
    t('honor','west'),t('honor','north'),t('honor','red'),
  ];
  assertNotWin(hand, 'only 3 melds + 1 pair');
});

test('missing tile in sequence (no wilds)', () => {
  const hand = [
    t('character',1),t('character',2), // missing char-3 for sequence
    t('dot',2),t('dot',3),t('dot',4),
    t('bamboo',3),t('bamboo',4),t('bamboo',5),
    t('bamboo',6),t('bamboo',7),t('bamboo',8),
    t('honor','east'),t('honor','east'),t('honor','red'),t('honor','red'),
  ];
  assertNotWin(hand, 'broken sequence, no wilds to fill');
});

test('6 pairs + 2 singles (14 tiles, not seven pairs)', () => {
  const hand = [
    t('character',1),t('character',1),
    t('character',3),t('character',3),
    t('dot',2),t('dot',2),
    t('dot',4),t('dot',4),
    t('bamboo',5),t('bamboo',5),
    t('bamboo',7),t('bamboo',7),
    t('honor','east'),t('honor','north'), // 2 singles, not a pair
  ];
  assertNotWin(hand, '6 pairs + 2 singles, not seven pairs');
});

test('1 wild with broken meld structure', () => {
  // 1 wild as pair, but only 3.5 melds worth of tiles
  const hand = [
    t('character',1),t('character',2),t('character',3),
    t('dot',2),t('dot',3),t('dot',4),
    t('bamboo',3),t('bamboo',4),t('bamboo',5),
    // only 3 melds (9 tiles) + 1 wild + 3 singles
    t('honor','east'),t('honor','north'),t('honor','west'),t('honor','red'),
    t('honor','white'),
  ];
  assertNotWin(hand, '1 wild as pair but only 3 melds');
});

// ============================================================
// 边界条件
// ============================================================

test('all honor tiles standard win', () => {
  const hand = [
    t('honor','east'),t('honor','east'),t('honor','east'),
    t('honor','south'),t('honor','south'),t('honor','south'),
    t('honor','west'),t('honor','west'),t('honor','west'),
    t('honor','north'),t('honor','north'),t('honor','north'),
    t('honor','red'),t('honor','red'),
  ];
  assertWin(hand, 'all honors: 4 triplets + 1 pair');
});

test('all characters hand', () => {
  const hand = [
    t('character',1),t('character',1),
    t('character',2),t('character',2),t('character',2),
    t('character',4),t('character',5),t('character',6),
    t('character',7),t('character',8),t('character',9),
    t('character',3),t('character',3),t('character',3),
  ];
  assertWin(hand, 'all characters hand');
});

test('hand with 4-of-a-kind + unmatched singles', () => {
  const hand = [
    t('honor','east'),t('honor','east'),t('honor','east'),t('honor','east'),
    t('character',1),t('character',2),t('character',3),
    t('dot',2),t('dot',3),t('dot',4),
    t('bamboo',3),t('bamboo',4),t('bamboo',5),
    t('honor','red'),t('honor','north'),
  ];
  // 4 easts: 3 for triplet, 1 left. red and north are different - no pair. Not winning.
  assertNotWin(hand, '4-of-a-kind leaves orphan, no pair');
});

test('2 triplets + 2 sequences + 1 pair (alt)', () => {
  const hand = [
    t('honor','east'),t('honor','east'),t('honor','east'),
    t('character',1),t('character',2),t('character',3),
    t('dot',2),t('dot',3),t('dot',4),
    t('bamboo',3),t('bamboo',4),t('bamboo',5),
    t('honor','red'),t('honor','red'),
  ];
  assertWin(hand, '2 triplets + 2 sequences + 1 pair (alt)');
});

test('4-of-a-kind with wild: 4 easts → triplet + wild-pair', () => {
  const hand = [
    t('honor','east'),t('honor','east'),t('honor','east'),t('honor','east'),
    t('character',1),t('character',2),t('character',3),
    t('dot',2),t('dot',3),t('dot',4),
    t('bamboo',3),t('bamboo',4),t('bamboo',5),
    t('honor','white'),
  ];
  // 3 easts = triplet, 1 east + wild = pair. Char/dot/bam seqs = 3 melds.
  // 4 melds + wild+east pair = win. 13 non-wild + 1 wild = 14 ✓
  assertWin(hand, '4 easts: triplet + east+wild pair');
});

// ============================================================
// Run
// ============================================================

console.log(`\n${passed} passed, ${failed} failed\n`);
if (failed > 0) process.exit(1);
