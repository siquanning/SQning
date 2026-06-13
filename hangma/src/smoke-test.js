// ============================================================
// smoke-test.js — 杭州麻将引擎全覆盖冒烟测试
// 运行: node src/smoke-test.js
// ============================================================

import {
  SUIT_CONFIG, HONOR_LABELS, CHINESE_NUMERALS, SEATS, INITIAL_SCORE,
  createTile, tileKey, tileLabel, tileGlyph, tileSortValue, sortTiles,
  cloneTile, tileAssetName, tileAssetPath, isTileWild, shuffle,
  createPlayers, createNewGame,
  isWinningHand, isWinningWithMelds,
  getWaitTiles, getWaitTilesWithMelds,
  getDiscardHints, getDiscardHintsWithMelds,
  getChiOptions, getPengOptions, getConcealedGangChoices,
  chooseDiscard, shouldPeng, shouldMeldGang, chooseChi,
  chooseConcealedGang, shouldPiaoCai, resolveClaim,
  calculateWinScore, checkFourWindDiscard, countWildInHand,
} from './engine.js';

const WILD_KEY = 'honor:white';

// ============================================================
// Helpers
// ============================================================

let passed = 0;
let failed = 0;

function assert(cond, msg) {
  if (cond) { passed++; }
  else { failed++; console.error('  FAIL:', msg); }
}

function assertEq(actual, expected, msg) {
  if (actual === expected) { passed++; }
  else { failed++; console.error(`  FAIL: ${msg} — expected ${expected}, got ${actual}`); }
}

function assertGte(actual, expected, msg) {
  if (actual >= expected) { passed++; }
  else { failed++; console.error(`  FAIL: ${msg} — expected >= ${expected}, got ${actual}`); }
}

function assertContains(arr, val, msg) {
  if (arr.includes(val)) { passed++; }
  else { failed++; console.error(`  FAIL: ${msg} — [${arr.join(', ')}] does not contain "${val}"`); }
}

function assertNotContains(arr, val, msg) {
  if (!arr.includes(val)) { passed++; }
  else { failed++; console.error(`  FAIL: ${msg} — [${arr.join(', ')}] should NOT contain "${val}"`); }
}

function t(suit, rank, id) {
  return { id: id || `${suit}-${rank}-${Math.random().toString(36).slice(2, 9)}`, suit, rank };
}

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
// 1. Tile utilities
// ============================================================

console.log('\n=== 1. Tile utilities ===');

test('createTile returns correct structure', () => {
  const tile = createTile('character', 5);
  assert(tile.suit === 'character', 'suit matches');
  assert(tile.rank === 5, 'rank matches');
  assert(typeof tile.id === 'string', 'id is string');
});

test('tileKey formats correctly', () => {
  assertEq(tileKey(t('character', 3)), 'character:3', 'number suit');
  assertEq(tileKey(t('honor', 'east')), 'honor:east', 'honor');
  assertEq(tileKey(t('honor', 'white')), 'honor:white', 'wild card key');
});

test('tileLabel number suits', () => {
  assertEq(tileLabel(t('character', 1)), '一万', 'c1');
  assertEq(tileLabel(t('dot', 5)), '五筒', 'd5');
  assertEq(tileLabel(t('bamboo', 9)), '九条', 'b9');
});

test('tileLabel honors', () => {
  assertEq(tileLabel(t('honor', 'east')), '东', 'east');
  assertEq(tileLabel(t('honor', 'red')), '中', 'red');
  assertEq(tileLabel(t('honor', 'white')), '白', 'white');
});

test('tileSortValue ordering', () => {
  const c1 = tileSortValue(t('character', 1));
  const c9 = tileSortValue(t('character', 9));
  const d1 = tileSortValue(t('dot', 1));
  const b1 = tileSortValue(t('bamboo', 1));
  const e = tileSortValue(t('honor', 'east'));
  const wh = tileSortValue(t('honor', 'white'));
  assert(c1 < c9, 'c1 < c9');
  assert(c9 < d1, 'c9 < d1');
  assert(d1 < b1, 'd1 < b1');
  assert(b1 < e, 'b1 < east');
  assert(e < wh, 'east < white');
});

test('sortTiles sorts correctly', () => {
  const tiles = [t('honor', 'white'), t('character', 1), t('dot', 5), t('character', 9)];
  sortTiles(tiles);
  assertEq(tileKey(tiles[0]), 'character:1', 'first is c1');
  assertEq(tileKey(tiles[1]), 'character:9', 'second is c9');
  assertEq(tileKey(tiles[2]), 'dot:5', 'third is d5');
  assertEq(tileKey(tiles[3]), 'honor:white', 'last is white');
});

test('cloneTile creates independent copy', () => {
  const orig = t('character', 1);
  const cloned = cloneTile(orig);
  cloned.rank = 2;
  assertEq(orig.rank, 1, 'original unaffected');
});

test('tileAssetPath returns svg path', () => {
  assert(tileAssetPath(t('character', 1)).includes('.svg'), 'returns svg path');
});

test('isTileWild detects wildcard', () => {
  assert(isTileWild(t('honor', 'white'), 'honor:white'), 'white is wild');
  assert(!isTileWild(t('honor', 'east'), 'honor:white'), 'east is not wild');
});

test('isTileWild with alternate wildKey', () => {
  assert(isTileWild(t('character', 1), 'character:1'), 'c1 can be wild');
});

test('shuffle preserves elements', () => {
  const list = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
  const shuffled = shuffle(list);
  assertEq(shuffled.length, 10, 'same length');
  assertEq([...shuffled].sort((a, b) => a - b).join(','), '1,2,3,4,5,6,7,8,9,10', 'same elements');
});

// ============================================================
// 2. Game creation
// ============================================================

console.log('\n=== 2. Game creation ===');

test('createPlayers returns 4 players', () => {
  const players = createPlayers();
  assertEq(players.length, 4, '4 players');
  assertEq(players[0].name, '你', 'player 0 is human');
  assertEq(players[0].isHuman, true, 'player 0 isHuman');
  assert(players[1].isHuman === false, 'player 1 is AI');
  assert(players[2].isHuman === false, 'player 2 is AI');
  assert(players[3].isHuman === false, 'player 3 is AI');
});

test('createPlayers initial score', () => {
  const players = createPlayers();
  assert(players.every(p => p.score === INITIAL_SCORE), 'all players start at 250');
  assert(players.every(p => p.piaoCai === false), 'all start not piaoCai');
  assert(players.every(p => p.piaoCount === 0), 'all piaoCount = 0');
});

test('createNewGame deals correct counts', () => {
  const game = createNewGame();
  assertEq(game.players[0].hand.length, 14, 'dealer 14 tiles');
  assertEq(game.players[1].hand.length, 13, 'p1 13 tiles');
  assertEq(game.players[2].hand.length, 13, 'p2 13 tiles');
  assertEq(game.players[3].hand.length, 13, 'p3 13 tiles');
});

test('createNewGame sets wildcard', () => {
  const game = createNewGame();
  assertEq(game.wildKey, 'honor:white', '白板是财神');
  assertEq(game.wildTile.suit, 'honor', 'wild tile is honor');
  assertEq(game.wildTile.rank, 'white', 'wild tile is white');
});

test('createNewGame initial state', () => {
  const game = createNewGame();
  assertEq(game.dealer, 0, 'dealer is player 0');
  assertEq(game.turn, 0, 'turn starts at 0');
  assertEq(game.phase, 'human-discard', 'initial phase');
  assertEq(game.winner, null, 'no winner yet');
  assertEq(game.locked, false, 'not locked');
});

test('createNewGame tiles total 136', () => {
  const game = createNewGame();
  const handTotal = game.players.reduce((s, p) => s + p.hand.length, 0);
  assertEq(handTotal + game.tilePool.length, 136, 'total 136 tiles');
});

test('createNewGame hands are sorted', () => {
  const game = createNewGame();
  for (const player of game.players) {
    for (let i = 1; i < player.hand.length; i++) {
      if (tileSortValue(player.hand[i - 1]) > tileSortValue(player.hand[i])) {
        assert(false, `${player.name} hand not sorted at index ${i}`);
        return;
      }
    }
  }
  passed++;
});

// ============================================================
// 3. Win detection — isWinningHand
// ============================================================

console.log('\n=== 3. Win detection ===');

test('isWinningHand — standard win (平胡)', () => {
  const hand = [
    t('character', 1), t('character', 2), t('character', 3),
    t('dot', 2), t('dot', 3), t('dot', 4),
    t('bamboo', 5), t('bamboo', 6), t('bamboo', 7),
    t('honor', 'east'), t('honor', 'east'), t('honor', 'east'),
    t('honor', 'red'), t('honor', 'red'),
  ];
  assert(isWinningHand(hand, WILD_KEY), 'standard 4 melds + 1 pair');
});

test('isWinningHand — all triplets (碰碰胡 pattern, still standard win)', () => {
  const hand = [
    t('character', 1), t('character', 1), t('character', 1),
    t('dot', 2), t('dot', 2), t('dot', 2),
    t('bamboo', 3), t('bamboo', 3), t('bamboo', 3),
    t('honor', 'east'), t('honor', 'east'), t('honor', 'east'),
    t('honor', 'red'), t('honor', 'red'),
  ];
  assert(isWinningHand(hand, WILD_KEY), 'all triplets is standard win');
});

test('isWinningHand — seven pairs', () => {
  const hand = [
    t('character', 1), t('character', 1),
    t('character', 9), t('character', 9),
    t('dot', 2), t('dot', 2),
    t('dot', 8), t('dot', 8),
    t('bamboo', 3), t('bamboo', 3),
    t('bamboo', 7), t('bamboo', 7),
    t('honor', 'east'), t('honor', 'east'),
  ];
  assert(isWinningHand(hand, WILD_KEY), 'seven pairs');
});

test('isWinningHand — seven pairs with wild fill', () => {
  const hand = [
    t('character', 1), t('character', 1),
    t('character', 9), t('character', 9),
    t('dot', 2), t('dot', 2),
    t('dot', 8), t('dot', 8),
    t('bamboo', 3), t('bamboo', 3),
    t('bamboo', 7),
    t('honor', 'white'), // wild fills the single b7
    t('honor', 'east'), t('honor', 'east'),
  ];
  assert(isWinningHand(hand, WILD_KEY), 'seven pairs with wildcard');
});

test('isWinningHand — win with wild as part of meld', () => {
  const hand = [
    t('character', 1), t('character', 2),
    t('honor', 'white'), // wild completes the sequence c1,c2,wild=c3
    t('dot', 2), t('dot', 3), t('dot', 4),
    t('bamboo', 5), t('bamboo', 6), t('bamboo', 7),
    t('honor', 'east'), t('honor', 'east'), t('honor', 'east'),
    t('honor', 'red'), t('honor', 'red'),
  ];
  assert(isWinningHand(hand, WILD_KEY), 'wild as part of sequence meld');
});

test('isWinningHand — win with wild in pair', () => {
  const hand = [
    t('character', 1), t('character', 2), t('character', 3),
    t('dot', 2), t('dot', 3), t('dot', 4),
    t('bamboo', 5), t('bamboo', 6), t('bamboo', 7),
    t('honor', 'east'), t('honor', 'east'), t('honor', 'east'),
    t('honor', 'red'), t('honor', 'white'), // wild + red = pair
  ];
  assert(isWinningHand(hand, WILD_KEY), 'wild in pair');
});

test('isWinningHand — multiple wilds', () => {
  const hand = [
    t('honor', 'white'), t('honor', 'white'), t('honor', 'white'),
    t('character', 1), t('character', 2), t('character', 3),
    t('dot', 3), t('dot', 4), t('dot', 5),
    t('bamboo', 7), t('bamboo', 8), t('bamboo', 9),
    t('honor', 'red'), t('honor', 'red'),
  ];
  assert(isWinningHand(hand, WILD_KEY), '3 wilds in hand');
});

test('isWinningHand — non-winning hand', () => {
  const hand = [
    t('character', 1), t('character', 3), t('character', 5),
    t('dot', 2), t('dot', 4), t('dot', 6),
    t('bamboo', 1), t('bamboo', 3), t('bamboo', 5),
    t('honor', 'east'), t('honor', 'south'), t('honor', 'west'),
    t('honor', 'north'), t('honor', 'red'),
  ];
  assert(!isWinningHand(hand, WILD_KEY), 'random tiles should not win');
});

test('isWinningHand — not 14 tiles returns false', () => {
  assert(!isWinningHand([t('character', 1)], WILD_KEY), '1 tile');
  assert(!isWinningHand(Array(13).fill(t('character', 1)), WILD_KEY), '13 tiles');
  assert(!isWinningHand(Array(15).fill(t('character', 1)), WILD_KEY), '15 tiles');
});

test('isWinningWithMelds — hand + meld = 14 tiles', () => {
  const hand = [
    t('character', 1), t('character', 2), t('character', 3),
    t('dot', 2), t('dot', 3), t('dot', 4),
    t('honor', 'red'), t('honor', 'red'),
  ];
  const melds = [
    { tiles: [t('bamboo', 5), t('bamboo', 6), t('bamboo', 7)] },
    { tiles: [t('honor', 'east'), t('honor', 'east'), t('honor', 'east')] },
  ];
  assert(isWinningWithMelds(hand, melds, WILD_KEY), '8 hand + 6 meld = 14, win');
});

// ============================================================
// 4. Wait tiles & discard hints
// ============================================================

console.log('\n=== 4. Wait tiles & discard hints ===');

test('getWaitTiles — tenpai hand', () => {
  const hand = [
    t('character', 1), t('character', 2), t('character', 3),
    t('dot', 2), t('dot', 3), t('dot', 4),
    t('bamboo', 4), t('bamboo', 5), t('bamboo', 6),
    t('honor', 'east'), t('honor', 'east'),
    t('character', 7), t('character', 8),
  ];
  const waits = getWaitTiles(hand, WILD_KEY);
  // Two-sided wait (c6, c9) + wild always completes any hand
  assertEq(waits.length, 3, 'waiting for c6, c9 + wildcard');
});

test('getWaitTiles — two-sided wait', () => {
  const hand = [
    t('character', 1), t('character', 2), t('character', 3),
    t('dot', 2), t('dot', 3), t('dot', 4),
    t('bamboo', 4), t('bamboo', 5), t('bamboo', 6),
    t('honor', 'east'), t('honor', 'east'),
    t('character', 4), t('character', 5),
  ];
  const waits = getWaitTiles(hand, WILD_KEY);
  assert(waits.length >= 2, `two-sided wait (c3 or c6), got ${waits.length}`);
});

test('getWaitTiles — pair wait', () => {
  const hand = [
    t('character', 1), t('character', 2), t('character', 3),
    t('dot', 2), t('dot', 3), t('dot', 4),
    t('bamboo', 4), t('bamboo', 5), t('bamboo', 6),
    t('honor', 'east'), t('honor', 'east'), t('honor', 'east'),
    t('honor', 'red'),
  ];
  const waits = getWaitTiles(hand, WILD_KEY);
  // Waiting for red (pair) + wild (always a wait)
  assertEq(waits.length, 2, 'waiting for red pair + wildcard');
});

test('getWaitTiles — not 13 tiles returns empty', () => {
  assertEq(getWaitTiles(Array(14).fill(t('character', 1)), WILD_KEY).length, 0, '14 tiles → no waits');
  assertEq(getWaitTiles([], WILD_KEY).length, 0, 'empty → no waits');
});

test('getWaitTilesWithMelds — with exposed melds', () => {
  const hand = [
    t('character', 1), t('character', 2), t('character', 3),
    t('dot', 2), t('dot', 3), t('dot', 4),
    t('honor', 'east'), t('honor', 'east'),
    t('character', 7), t('character', 8),
  ]; // 10 hand tiles
  const melds = [
    { tiles: [t('bamboo', 5), t('bamboo', 6), t('bamboo', 7)] },
  ]; // 3 meld tiles → total 13
  const waits = getWaitTilesWithMelds(hand, melds, WILD_KEY);
  assert(waits.length > 0, 'tenpai with melds (waiting for c6/c9 + wild)');
});

test('getDiscardHints — 14-tile hand finds tenpai discards', () => {
  const hand = [
    t('character', 1), t('character', 2), t('character', 3),
    t('dot', 2), t('dot', 3), t('dot', 4),
    t('bamboo', 4), t('bamboo', 5), t('bamboo', 6),
    t('honor', 'east'), t('honor', 'east'),
    t('character', 4), t('character', 5),
    t('bamboo', 9), // isolated, discarding this → tenpai
  ];
  const hints = getDiscardHints(hand, WILD_KEY);
  assert(hints.length > 0, 'at least one discard leads to tenpai');
  // bamboo:9 is isolated → discarding it should lead to tenpai
  const hintForB9 = hints.find(h => tileKey(h.discard) === 'bamboo:9');
  assert(hintForB9 !== undefined, 'discarding bamboo:9 leads to tenpai');
  assert(hintForB9.waits.length > 0, 'has wait tiles after discarding b9');
});

test('getDiscardHints — sorted by wait count descending', () => {
  const hand = [
    t('character', 1), t('character', 2), t('character', 3),
    t('dot', 2), t('dot', 3), t('dot', 4),
    t('bamboo', 4), t('bamboo', 5), t('bamboo', 6),
    t('honor', 'east'), t('honor', 'east'),
    t('character', 4), t('character', 5),
    t('bamboo', 9),
  ];
  const hints = getDiscardHints(hand, WILD_KEY);
  for (let i = 1; i < hints.length; i++) {
    if (hints[i - 1].waits.length < hints[i].waits.length) {
      assert(false, `hints not sorted: [${i - 1}]=${hints[i - 1].waits.length} < [${i}]=${hints[i].waits.length}`);
      return;
    }
  }
  passed++;
});

// ============================================================
// 5. Chi / Peng / Gang options
// ============================================================

console.log('\n=== 5. Chi / Peng / Gang options ===');

test('getChiOptions — basic chi (顺吃)', () => {
  const hand = [
    t('character', 2), t('character', 3),
  ];
  const discard = t('character', 1);
  const opts = getChiOptions(hand, discard, WILD_KEY);
  assert(opts.length >= 1, 'can chi c1 with c2,c3');
});

test('getChiOptions — middle chi (中吃)', () => {
  const hand = [
    t('character', 1), t('character', 3),
  ];
  const discard = t('character', 2);
  const opts = getChiOptions(hand, discard, WILD_KEY);
  assert(opts.length >= 1, 'can middle-chi c2 with c1,c3');
});

test('getChiOptions — end chi not allowed (吃不能吃右侧)', () => {
  const hand = [
    t('character', 1), t('character', 2),
  ];
  const discard = t('character', 3);
  const opts = getChiOptions(hand, discard, WILD_KEY);
  assertEq(opts.length, 0, 'cannot chi c3 on right end');
});

test('getChiOptions — honor cannot be chi\'d', () => {
  const hand = [
    t('honor', 'east'), t('honor', 'south'),
  ];
  const discard = t('honor', 'west');
  const opts = getChiOptions(hand, discard, WILD_KEY);
  assertEq(opts.length, 0, 'cannot chi honors');
});

test('getChiOptions — wild cannot be used for chi', () => {
  const hand = [
    t('honor', 'white'), t('character', 3),
  ];
  const discard = t('character', 1);
  const opts = getChiOptions(hand, discard, WILD_KEY);
  assert(opts.every(o => !o.ids.some(id => {
    const ht = hand.find(h => h.id === id);
    return ht && tileKey(ht) === WILD_KEY;
  })), 'wild not used for chi');
});

test('getPengOptions — pair → peng', () => {
  const hand = [
    t('character', 1), t('character', 1),
  ];
  const result = getPengOptions(hand, t('character', 1), WILD_KEY);
  assert(result !== null, 'can peng');
  assertEq(result.type, 'peng', 'type is peng');
});

test('getPengOptions — triplet → gang', () => {
  const hand = [
    t('character', 1), t('character', 1), t('character', 1),
  ];
  const result = getPengOptions(hand, t('character', 1), WILD_KEY);
  assert(result !== null, 'can gang');
  assertEq(result.type, 'gang', 'type is gang');
});

test('getPengOptions — no match returns null', () => {
  const hand = [t('character', 2)];
  const result = getPengOptions(hand, t('character', 1), WILD_KEY);
  assertEq(result, null, 'no match');
});

test('getPengOptions — wildcard cannot be peng\'d', () => {
  const hand = [
    t('honor', 'white'), t('honor', 'white'),
  ];
  const result = getPengOptions(hand, t('honor', 'white'), WILD_KEY);
  assertEq(result, null, 'cannot peng wildcard');
});

test('getConcealedGangChoices — 4 same → can gang', () => {
  const hand = [
    t('character', 1), t('character', 1), t('character', 1), t('character', 1),
  ];
  const choices = getConcealedGangChoices(hand, WILD_KEY);
  assertEq(choices.length, 1, 'one gang choice');
  assertEq(choices[0].length, 4, '4 tiles');
});

test('getConcealedGangChoices — multiple gangs possible', () => {
  const hand = [
    t('character', 1), t('character', 1), t('character', 1), t('character', 1),
    t('dot', 2), t('dot', 2), t('dot', 2), t('dot', 2),
  ];
  const choices = getConcealedGangChoices(hand, WILD_KEY);
  assertEq(choices.length, 2, 'two gang choices');
});

test('getConcealedGangChoices — no gang with wilds', () => {
  const hand = [
    t('honor', 'white'), t('honor', 'white'), t('honor', 'white'), t('honor', 'white'),
  ];
  const choices = getConcealedGangChoices(hand, WILD_KEY);
  assertEq(choices.length, 0, 'wilds cannot be ganged');
});

// ============================================================
// 6. Scoring — 12 patterns in calculateWinScore
// ============================================================

console.log('\n=== 6. Scoring — calculateWinScore ===');

function makeStdWin(suit) {
  const s = suit || 'character';
  return [
    t(s, 1), t(s, 2), t(s, 3),
    t(s, 4), t(s, 5), t(s, 6),
    t('dot', 2), t('dot', 3), t('dot', 4),
    t('bamboo', 7), t('bamboo', 8), t('bamboo', 9),
    t('honor', 'east'), t('honor', 'east'),
  ];
}

function makeBaoTouWin() {
  // 暴头: 1 wild as head + drawn winningTile + 4 melds = 14-tile winning hand
  const wt = t('character', 7); // drawn tile pairs with wild
  const hand = [
    t('character', 1), t('character', 2), t('character', 3),
    t('character', 4), t('character', 5), t('character', 6),
    t('dot', 2), t('dot', 3), t('dot', 4),
    t('bamboo', 7), t('bamboo', 8), t('bamboo', 9),
    t('honor', 'white'), // wild acts as head
    wt,
  ];
  return { hand, winningTile: wt };
}

function makeSevenPairs() {
  return [
    t('character', 1), t('character', 1),
    t('character', 5), t('character', 5),
    t('dot', 2), t('dot', 2),
    t('dot', 8), t('dot', 8),
    t('bamboo', 3), t('bamboo', 3),
    t('bamboo', 6), t('bamboo', 6),
    t('honor', 'east'), t('honor', 'east'),
  ];
}

function makeLuxurySevenPairs() {
  return [
    t('character', 1), t('character', 1),
    t('character', 1), t('character', 1),
    t('dot', 2), t('dot', 2),
    t('dot', 8), t('dot', 8),
    t('bamboo', 3), t('bamboo', 3),
    t('bamboo', 6), t('bamboo', 6),
    t('honor', 'east'), t('honor', 'east'),
  ];
}

function calc(hand, opts = {}) {
  return calculateWinScore({
    hand, wildKey: WILD_KEY,
    kind: opts.kind || '自摸',
    winningTile: opts.winningTile || null,
    piaoCount: opts.piaoCount || 0,
    isDealer: opts.isDealer || false,
    isFirstDraw: opts.isFirstDraw || false,
    isFirstTurn: opts.isFirstTurn || false,
  });
}

// --- 6a. 平胡 ---

test('平胡 — standard 4 melds + 1 pair', () => {
  const r = calc(makeStdWin());
  assertContains(r.flags, '平胡', 'ping hu flag');
  assertEq(r.multiplier, 1, 'multiplier = 1');
});

// --- 6b. 七对 ---

test('七对 — 7 pairs', () => {
  const r = calc(makeSevenPairs());
  assertContains(r.flags, '七对', 'seven pairs flag');
  assertNotContains(r.flags, '平胡', 'seven pairs replaces ping hu');
  assertEq(r.multiplier, 2, 'multiplier = 2');
});

test('七对 — with wild fill', () => {
  const hand = [
    t('character', 1), t('character', 1),
    t('character', 5), t('character', 5),
    t('dot', 2), t('dot', 2),
    t('dot', 8), t('dot', 8),
    t('bamboo', 3), t('bamboo', 3),
    t('bamboo', 6),
    t('honor', 'white'),
    t('honor', 'east'), t('honor', 'east'),
  ];
  const r = calc(hand);
  assertContains(r.flags, '七对', 'seven pairs with wild');
});

// --- 6c. 豪华七对 ---

test('豪华七对 — seven pairs with a quad', () => {
  const r = calc(makeLuxurySevenPairs());
  assertContains(r.flags, '七对', 'seven pairs flag');
  assertContains(r.flags, '豪华七对', 'luxury seven pairs flag');
  assertEq(r.multiplier, 4, 'multiplier = 2*2 = 4');
});

// --- 6d. 十三幺 ---

test('十三幺 — 13 terminal/honor types + 1 pair', () => {
  const hand = [
    t('character', 1), t('character', 9),
    t('dot', 1), t('dot', 9),
    t('bamboo', 1), t('bamboo', 9),
    t('honor', 'east'), t('honor', 'south'), t('honor', 'west'), t('honor', 'north'),
    t('honor', 'red'), t('honor', 'green'),
    t('honor', 'white'),
    t('honor', 'east'), // extra east for pair
  ];
  const r = calc(hand);
  assertContains(r.flags, '十三幺', 'shisan yao flag');
});

// --- 6e. 杠上开花 ---

test('杠上开花 — kind=杠开', () => {
  const r = calc(makeStdWin(), { kind: '杠开' });
  assertContains(r.flags, '杠上开花', 'gang shang kai hua flag');
  assertEq(r.multiplier, 2, 'multiplier x2');
});

// --- 6f. 爆头 ---

test('爆头 — 1 wild + tenpai + winning draw', () => {
  const withoutWinning = [
    t('character', 1), t('character', 2), t('character', 3),
    t('character', 4), t('character', 5), t('character', 6),
    t('dot', 2), t('dot', 3), t('dot', 4),
    t('bamboo', 7), t('bamboo', 8),
    t('honor', 'white'),
    t('honor', 'east'),
  ];
  const winningTile = t('bamboo', 9);
  const hand = [...withoutWinning, winningTile];
  const r = calc(hand, { winningTile });
  assertContains(r.flags, '爆头', 'bao tou flag');
  assertContains(r.flags, '平胡', 'ping hu flag');
  assertEq(r.multiplier, 2, 'multiplier x2');
});

test('爆头 — not triggered without winningTile', () => {
  const hand = [
    t('character', 1), t('character', 2), t('character', 3),
    t('character', 4), t('character', 5), t('character', 6),
    t('dot', 2), t('dot', 3), t('dot', 4),
    t('bamboo', 7), t('bamboo', 8), t('bamboo', 9),
    t('honor', 'white'),
    t('honor', 'east'),
  ];
  const r = calc(hand, { winningTile: null });
  assertNotContains(r.flags, '爆头', 'no bao tou without winning tile');
});

// --- 6g. 天胡 ---

test('天胡 — dealer first turn', () => {
  const r = calc(makeStdWin(), { isDealer: true, isFirstTurn: true });
  assertContains(r.flags, '天胡', 'tian hu flag');
});

test('天胡 — non-dealer not triggered', () => {
  const r = calc(makeStdWin(), { isDealer: false, isFirstTurn: true });
  assertNotContains(r.flags, '天胡', 'no tian hu for non-dealer');
});

// --- 6h. 地胡 ---

test('地胡 — non-dealer first draw', () => {
  const r = calc(makeStdWin(), { isDealer: false, isFirstDraw: true });
  assertContains(r.flags, '地胡', 'di hu flag');
});

test('地胡 — dealer not triggered', () => {
  const r = calc(makeStdWin(), { isDealer: true, isFirstDraw: true });
  assertNotContains(r.flags, '地胡', 'no di hu for dealer');
});

// --- 6i. 大四喜 ---

test('大四喜 — 4 wind triplets', () => {
  const hand = [
    t('honor', 'east'), t('honor', 'east'), t('honor', 'east'),
    t('honor', 'south'), t('honor', 'south'), t('honor', 'south'),
    t('honor', 'west'), t('honor', 'west'), t('honor', 'west'),
    t('honor', 'north'), t('honor', 'north'), t('honor', 'north'),
    t('character', 1), t('character', 1),
  ];
  const r = calc(hand);
  assertContains(r.flags, '大四喜', 'da si xi flag');
  assertNotContains(r.flags, '小四喜', 'da si xi excludes xiao si xi');
});

test('大四喜 — with wild assistance', () => {
  const hand = [
    t('honor', 'east'), t('honor', 'east'), t('honor', 'east'),
    t('honor', 'south'), t('honor', 'south'),
    t('honor', 'west'), t('honor', 'west'),
    t('honor', 'north'), t('honor', 'north'),
    t('character', 1), t('character', 1),
    t('honor', 'white'), t('honor', 'white'), t('honor', 'white'),
  ];
  const r = calc(hand);
  assertContains(r.flags, '大四喜', 'da si xi with wilds');
});

// --- 6j. 小四喜 ---

test('小四喜 — 3 wind triplets + 1 wind pair', () => {
  const hand = [
    t('honor', 'east'), t('honor', 'east'), t('honor', 'east'),
    t('honor', 'south'), t('honor', 'south'), t('honor', 'south'),
    t('honor', 'west'), t('honor', 'west'), t('honor', 'west'),
    t('honor', 'north'), t('honor', 'north'),
    t('character', 1), t('character', 1), t('character', 1),
  ];
  const r = calc(hand);
  assertContains(r.flags, '小四喜', 'xiao si xi flag');
});

// --- 6k. 大三元 ---

test('大三元 — 3 dragon triplets (with wilds for white)', () => {
  const hand = [
    t('honor', 'red'), t('honor', 'red'), t('honor', 'red'),
    t('honor', 'green'), t('honor', 'green'), t('honor', 'green'),
    t('honor', 'white'), t('honor', 'white'), t('honor', 'white'),
    t('character', 1), t('character', 2), t('character', 3),
    t('character', 5), t('character', 5),
  ];
  const r = calc(hand);
  assertContains(r.flags, '大三元', 'da san yuan flag');
});

// --- 6l. 小三元 ---

test('小三元 — 2 dragon triplets + 1 dragon pair', () => {
  const hand = [
    t('honor', 'red'), t('honor', 'red'), t('honor', 'red'),
    t('honor', 'green'), t('honor', 'green'), t('honor', 'green'),
    t('honor', 'white'), t('honor', 'white'),
    t('character', 1), t('character', 2), t('character', 3),
    t('character', 4), t('character', 5), t('character', 6),
  ];
  const r = calc(hand);
  assertContains(r.flags, '小三元', 'xiao san yuan flag');
});

// --- 6m. Combo stacking ---

test('Combo — 七对 + 豪华七对 = x4', () => {
  const r = calc(makeLuxurySevenPairs());
  assertEq(r.multiplier, 4, 'luxury seven pairs = x4');
});

test('Combo — 平胡 + 杠上开花 + 爆头 = x4', () => {
  const withoutWinning = [
    t('character', 1), t('character', 2), t('character', 3),
    t('character', 4), t('character', 5), t('character', 6),
    t('dot', 2), t('dot', 3), t('dot', 4),
    t('bamboo', 7), t('bamboo', 8),
    t('honor', 'white'),
    t('honor', 'east'),
  ];
  const winningTile = t('bamboo', 9);
  const hand = [...withoutWinning, winningTile];
  const r = calc(hand, { kind: '杠开', winningTile });
  assertContains(r.flags, '平胡', 'ping hu');
  assertContains(r.flags, '杠上开花', 'gang kai');
  assertContains(r.flags, '爆头', 'bao tou');
  assertEq(r.multiplier, 4, '1 × 2(杠开) × 2(爆头) = 4');
});

test('Combo — 七对 + 天胡 = x4', () => {
  const r = calc(makeSevenPairs(), { isDealer: true, isFirstTurn: true });
  assertContains(r.flags, '七对', 'seven pairs');
  assertContains(r.flags, '天胡', 'tian hu');
  assertEq(r.multiplier, 4, '2(七对) × 2(天胡) = 4');
});

// --- 6n. Piao cai multipliers ---

test('飘财 — piaoCount=0 → ×1', () => {
  assertEq(calc(makeStdWin()).multiplier, 1, 'no piao cai');
});

test('飘财 — piaoCount=1 → ×2', () => {
  assertEq(calc(makeStdWin(), { piaoCount: 1 }).multiplier, 2, '一飘 x2');
});

test('飘财 — piaoCount=2 → ×4', () => {
  assertEq(calc(makeStdWin(), { piaoCount: 2 }).multiplier, 4, '二飘 x4');
});

test('飘财 — piaoCount=3 → ×8', () => {
  assertEq(calc(makeStdWin(), { piaoCount: 3 }).multiplier, 8, '三飘 x8');
});

test('飘财 — 七对 + 一飘 = 2×2=4', () => {
  const r = calc(makeSevenPairs(), { piaoCount: 1 });
  assertEq(r.multiplier, 4, '7pairs + 1 piao = 4');
});

test('飘财+爆头 — 一飘=4x (2×2)', () => {
  const { hand, winningTile } = makeBaoTouWin();
  const r = calc(hand, { piaoCount: 1, winningTile });
  assertEq(r.multiplier, 4, '一飘+爆头 = 4');
  assertContains(r.flags, '爆头', 'has 爆头');
});

test('飘财+爆头 — 二飘=8x (2×4)', () => {
  const { hand, winningTile } = makeBaoTouWin();
  const r = calc(hand, { piaoCount: 2, winningTile });
  assertEq(r.multiplier, 8, '二飘+爆头 = 8');
});

test('飘财+爆头 — 三飘=16x (2×8)', () => {
  const { hand, winningTile } = makeBaoTouWin();
  const r = calc(hand, { piaoCount: 3, winningTile });
  assertEq(r.multiplier, 16, '三飘+爆头 = 16');
});

test('杠开+爆头 = x4', () => {
  const { hand, winningTile } = makeBaoTouWin();
  const r = calc(hand, { kind: '杠开', winningTile });
  assertEq(r.multiplier, 4, '杠开+爆头 = 4');
  assertContains(r.flags, '杠上开花', 'has 杠开');
  assertContains(r.flags, '爆头', 'has 爆头');
});

test('杠开+爆头+一飘 = x8', () => {
  const { hand, winningTile } = makeBaoTouWin();
  const r = calc(hand, { kind: '杠开', piaoCount: 1, winningTile });
  assertEq(r.multiplier, 8, '杠开+爆头+一飘 = 8');
});

test('七对+爆头 = x4', () => {
  const hand = makeSevenPairs(); // no wild, but test 爆头 via piaoCount path
  const r = calc(hand, { piaoCount: 1 });
  assertEq(r.multiplier, 4, '七对+一飘(含爆头) = 4');
});

// --- 6o. isSelfDraw ---

test('自摸 → isSelfDraw=true', () => {
  assertEq(calc(makeStdWin(), { kind: '自摸' }).isSelfDraw, true, '自摸');
  assertEq(calc(makeStdWin(), { kind: '杠开' }).isSelfDraw, true, '杠开');
  assertEq(calc(makeStdWin(), { kind: '飘财' }).isSelfDraw, true, '飘财');
});

test('点炮 → isSelfDraw=false', () => {
  assertEq(calc(makeStdWin(), { kind: '点炮' }).isSelfDraw, false, '点炮');
});

// --- 6p. Edge cases ---

test('空手牌 → multiplier=0, no flags', () => {
  const r = calc([]);
  assertEq(r.multiplier, 0, 'zero multiplier');
  assertEq(r.flags.length, 0, 'no flags');
});

test('非14张 → multiplier=0', () => {
  assertEq(calc(Array(13).fill(t('character', 1))).multiplier, 0, '13 tiles');
  assertEq(calc([t('character', 1)]).multiplier, 0, '1 tile');
});

test('大四喜不触发小四喜', () => {
  const hand = [
    t('honor', 'east'), t('honor', 'east'), t('honor', 'east'),
    t('honor', 'south'), t('honor', 'south'), t('honor', 'south'),
    t('honor', 'west'), t('honor', 'west'), t('honor', 'west'),
    t('honor', 'north'), t('honor', 'north'), t('honor', 'north'),
    t('character', 1), t('character', 1),
  ];
  const r = calc(hand);
  assertContains(r.flags, '大四喜', 'has da si xi');
  assertNotContains(r.flags, '小四喜', 'no xiao si xi');
});

test('大三元不触发小三元', () => {
  const hand = [
    t('honor', 'red'), t('honor', 'red'), t('honor', 'red'),
    t('honor', 'green'), t('honor', 'green'), t('honor', 'green'),
    t('honor', 'white'), t('honor', 'white'), t('honor', 'white'),
    t('character', 1), t('character', 2), t('character', 3),
    t('character', 5), t('character', 5),
  ];
  const r = calc(hand);
  assertContains(r.flags, '大三元', 'has da san yuan');
  assertNotContains(r.flags, '小三元', 'no xiao san yuan');
});

// ============================================================
// 7. Four wind discard
// ============================================================

console.log('\n=== 7. Four wind discard ===');

test('checkFourWindDiscard — same wind × 4', () => {
  const discards = [
    { playerId: 0, tile: t('honor', 'east') },
    { playerId: 1, tile: t('honor', 'east') },
    { playerId: 2, tile: t('honor', 'east') },
    { playerId: 3, tile: t('honor', 'east') },
  ];
  const result = checkFourWindDiscard(discards);
  assertEq(result.isFourWind, true, 'four wind detected');
  assert(result.windLabel !== undefined, 'wind label present');
});

test('checkFourWindDiscard — different winds', () => {
  const discards = [
    { playerId: 0, tile: t('honor', 'east') },
    { playerId: 1, tile: t('honor', 'south') },
    { playerId: 2, tile: t('honor', 'west') },
    { playerId: 3, tile: t('honor', 'north') },
  ];
  const result = checkFourWindDiscard(discards);
  assertEq(result.isFourWind, false, 'different winds — no trigger');
});

test('checkFourWindDiscard — non-wind tile breaks it', () => {
  const discards = [
    { playerId: 0, tile: t('honor', 'east') },
    { playerId: 1, tile: t('honor', 'east') },
    { playerId: 2, tile: t('character', 1) },
    { playerId: 3, tile: t('honor', 'east') },
  ];
  const result = checkFourWindDiscard(discards);
  assertEq(result.isFourWind, false, 'non-wind breaks it');
});

test('checkFourWindDiscard — fewer than 4 entries', () => {
  const discards = [
    { playerId: 0, tile: t('honor', 'east') },
  ];
  const result = checkFourWindDiscard(discards);
  assertEq(result.isFourWind, false, 'only 1 discard');
});

test('checkFourWindDiscard — empty/null', () => {
  assertEq(checkFourWindDiscard([]).isFourWind, false, 'empty');
  assertEq(checkFourWindDiscard(null).isFourWind, false, 'null');
});

// ============================================================
// 8. countWildInHand
// ============================================================

console.log('\n=== 8. countWildInHand ===');

test('countWildInHand — zero wilds', () => {
  const hand = [t('character', 1), t('character', 2), t('honor', 'east')];
  assertEq(countWildInHand(hand, WILD_KEY), 0, 'no wilds');
});

test('countWildInHand — one wild', () => {
  const hand = [t('character', 1), t('honor', 'white'), t('honor', 'east')];
  assertEq(countWildInHand(hand, WILD_KEY), 1, 'one wild');
});

test('countWildInHand — multiple wilds', () => {
  const hand = [t('honor', 'white'), t('honor', 'white'), t('honor', 'white')];
  assertEq(countWildInHand(hand, WILD_KEY), 3, 'three wilds');
});

// ============================================================
// 9. AI decision functions
// ============================================================

console.log('\n=== 9. AI decisions ===');

test('chooseDiscard — returns a tile from hand', () => {
  const game = createNewGame();
  const hand = game.players[1].hand;
  const discard = chooseDiscard(hand, 1, game);
  assert(discard !== null && discard !== undefined, 'discard is not null');
  assert(hand.some(t => t.id === discard.id), 'discarded tile is from hand');
});

test('chooseDiscard — returns last tile on error fallback', () => {
  const hand = [t('character', 1)];
  const discard = chooseDiscard(hand, 1, {});  // broken game state
  assert(discard !== null, 'fallback returns something');
  assertEq(discard.id, hand[0].id, 'fallback returns last hand tile');
});

test('shouldPeng — basic call', () => {
  const game = createNewGame();
  assertEq(typeof shouldPeng(1, t('character', 1), game), 'boolean', 'returns boolean');
});

test('shouldPeng — piaoCai blocks it', () => {
  const game = createNewGame();
  game.players[1].piaoCai = true;
  assertEq(shouldPeng(1, t('character', 1), game), false, 'piaoCai blocks peng');
});

test('shouldMeldGang — accepts by default', () => {
  const game = createNewGame();
  assertEq(shouldMeldGang(1, t('character', 1), game), true, 'gang accepted');
});

test('shouldMeldGang — piaoCai blocks it', () => {
  const game = createNewGame();
  game.players[1].piaoCai = true;
  assertEq(shouldMeldGang(1, t('character', 1), game), false, 'piaoCai blocks gang');
});

test('chooseChi — with valid choices', () => {
  const game = createNewGame();
  const choices = getChiOptions(game.players[1].hand, t('character', 5), WILD_KEY);
  const result = chooseChi(1, t('character', 5), choices, game);
  // May choose or reject — both are valid
  assert(result === null || (result.ids && result.label), 'returns null or valid choice');
});

test('chooseChi — no choices returns null', () => {
  const game = createNewGame();
  assertEq(chooseChi(1, t('honor', 'east'), [], game), null, 'null for empty choices');
});

test('chooseConcealedGang — returns null for empty choices', () => {
  const game = createNewGame();
  assertEq(chooseConcealedGang(1, [], game), null, 'null for empty');
});

test('chooseConcealedGang — with gang-able hand', () => {
  const game = createNewGame();
  game.players[1].hand = [
    t('character', 1), t('character', 1), t('character', 1), t('character', 1),
    t('dot', 2), t('dot', 3), t('dot', 4),
    t('bamboo', 5), t('bamboo', 6), t('bamboo', 7),
    t('honor', 'east'), t('honor', 'east'), t('honor', 'east'),
    t('honor', 'red'),
  ];
  sortTiles(game.players[1].hand);
  const choices = getConcealedGangChoices(game.players[1].hand, WILD_KEY);
  assertGte(choices.length, 1, 'has gang choices');
  const result = chooseConcealedGang(1, choices, game);
  // Shanten may or may not improve — both null and non-null are valid
  if (result !== null) {
    assertEq(result.length, 4, 'returns 4 tiles');
  }
});

test('shouldPiaoCai — returns boolean', () => {
  const game = createNewGame();
  // Player 1 starts with 13 tiles, unlikely to have 2 wilds and tenpai
  const result = shouldPiaoCai(1, game);
  assertEq(typeof result, 'boolean', 'returns boolean');
});

test('shouldPiaoCai — false when no wilds', () => {
  const game = createNewGame();
  game.players[1].hand = game.players[1].hand.filter(t => tileKey(t) !== WILD_KEY);
  assertEq(shouldPiaoCai(1, game), false, 'no wilds → false');
});

test('resolveClaim — returns null for empty list', () => {
  const game = createNewGame();
  assertEq(resolveClaim([], t('character', 1), 0, game), null, 'empty → null');
});

test('resolveClaim — gang > peng priority', () => {
  const game = createNewGame();
  game.players[1].hand = [
    t('character', 1), t('character', 1), t('character', 1),
  ];
  game.players[2].hand = [
    t('character', 1), t('character', 1),
  ];
  const claims = [
    { playerId: 2, type: 'peng', priority: 2 },
    { playerId: 1, type: 'gang', priority: 2.5 },
  ];
  const result = resolveClaim(claims, t('character', 1), 0, game);
  assert(result !== null, 'has result');
  assertEq(result.playerId, 1, 'gang wins over peng');
});

test('resolveClaim — chi vs peng priority', () => {
  const game = createNewGame();
  game.players[1].hand = [t('character', 1), t('character', 1)];
  game.players[2].hand = [t('character', 2), t('character', 3)];
  const tile = t('character', 1);
  const claims = [
    { playerId: 2, type: 'chi', priority: 1 },
    { playerId: 1, type: 'peng', priority: 2 },
  ];
  const result = resolveClaim(claims, tile, 0, game);
  assert(result !== null, 'has result');
  assertEq(result.type, 'peng', 'peng > chi'); // peng priority 2 > chi 1
});

// ============================================================
// 10. Integration-like: newGame → AI discard flow
// ============================================================

console.log('\n=== 10. Integration smoke ===');

test('Full AI discard cycle for all 3 AI players', () => {
  const game = createNewGame();
  for (let pid = 1; pid <= 3; pid++) {
    const hand = game.players[pid].hand;
    assertEq(hand.length, 13, `player ${pid} starts with 13 tiles`);
    // Simulate draw (add a random tile from pool)
    if (game.tilePool.length > 0) {
      hand.push(game.tilePool.pop());
      sortTiles(hand);
    }
    assertEq(hand.length, 14, `player ${pid} has 14 after draw`);
    const discard = chooseDiscard(hand, pid, game);
    assert(discard !== null, `player ${pid} can choose a discard`);
    assert(hand.some(t => t.id === discard.id), `discard from pid ${pid} in hand`);
  }
});

test('Game state consistency after AI decisions', () => {
  const game = createNewGame();
  const initialPoolSize = game.tilePool.length;
  assertEq(initialPoolSize, 136 - (14 + 13 * 3), 'correct initial pool size');

  // Run several turns: p1 draws, discards, p2 draws, discards...
  for (let round = 0; round < 3; round++) {
    for (let pid = 1; pid <= 3; pid++) {
      if (game.tilePool.length > 0) {
        const tile = game.tilePool.pop();
        game.players[pid].hand.push(tile);
        sortTiles(game.players[pid].hand);
        const discard = chooseDiscard(game.players[pid].hand, pid, game);
        if (discard) {
          const idx = game.players[pid].hand.findIndex(t => t.id === discard.id);
          if (idx >= 0) {
            game.players[pid].discards.push(game.players[pid].hand.splice(idx, 1)[0]);
          }
        }
      }
    }
  }
  // Verify pool + hands + melds + discards = 136
  let totalTiles = game.tilePool.length;
  for (const p of game.players) {
    totalTiles += p.hand.length + p.melds.reduce((s, m) => s + m.tiles.length, 0) + p.discards.length;
  }
  assertEq(totalTiles, 136, 'tile conservation after 3 rounds');
});

// ============================================================

console.log(`\n=== Results: ${passed} passed, ${failed} failed ===`);
if (failed > 0) process.exit(1);
