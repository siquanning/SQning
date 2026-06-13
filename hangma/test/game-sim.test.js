// ============================================================
// game-sim.test.js — Tests for the fast game simulator
// Run: node --experimental-vm-modules test/game-sim.test.js
// Or:  node test/game-sim.test.js
// ============================================================

import {
  TileSet, SimGameState, tileTypeIndex, indexToTileType, tileToIndex,
  isWinningHandSet, isStandardWinSet, isSevenPairsSet,
  computeShanten, shantenAfterDiscard, chooseBestDiscard,
  drawRandom, rollout, rolloutReward, fromGameState, determinize
} from '../src/ai/game-sim.js';

import {
  isWinningHand, createNewGame, createTile, tileKey
} from '../src/engine.js';

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

function handFromIndices(indices) {
  const ts = new TileSet();
  for (const idx of indices) ts.add(idx);
  return ts;
}

function handFromTileObjs(tiles, wildKey) {
  // Convert tile objects [ {suit, rank} ] → TileSet
  const ts = new TileSet();
  for (const t of tiles) {
    ts.add(tileToIndex(t));
  }
  return ts;
}

// Build a 14-tile winning hand (standard: 4 melds + 1 pair)
// e.g., character 1-2-3, dot 4-5-6, bamboo 7-8-9, character 1-2-3, red-red
function make14TileHand(indices) {
  const ts = new TileSet();
  for (const idx of indices) ts.add(idx);
  return ts;
}

// ============================================================
// Tile type indexing
// ============================================================

function test_indexing() {
  console.log('\n--- Tile type indexing ---');

  assertEq(tileTypeIndex('character', 1), 0, 'character:1 → 0');
  assertEq(tileTypeIndex('character', 9), 8, 'character:9 → 8');
  assertEq(tileTypeIndex('dot', 1), 9, 'dot:1 → 9');
  assertEq(tileTypeIndex('dot', 9), 17, 'dot:9 → 17');
  assertEq(tileTypeIndex('bamboo', 1), 18, 'bamboo:1 → 18');
  assertEq(tileTypeIndex('bamboo', 9), 26, 'bamboo:9 → 26');
  assertEq(tileTypeIndex('honor', 'east'), 27, 'honor:east → 27');
  assertEq(tileTypeIndex('honor', 'white'), 33, 'honor:white → 33');

  // Round-trip
  for (let i = 0; i < 34; i++) {
    const { suit, rank } = indexToTileType(i);
    assertEq(tileTypeIndex(suit, rank), i, `round-trip idx ${i}`);
  }

  // tileToIndex
  const t = createTile('character', 3);
  assertEq(tileToIndex(t), 2, 'tileToIndex character:3 → 2');
}

// ============================================================
// TileSet
// ============================================================

function test_tileset() {
  console.log('\n--- TileSet ---');

  const ts = new TileSet();
  assertEq(ts.total(), 0, 'empty total = 0');

  ts.add(5);
  ts.add(5);
  ts.add(10);
  assertEq(ts.count(5), 2, 'count after 2 adds');
  assertEq(ts.count(10), 1, 'count after 1 add');
  assertEq(ts.total(), 3, 'total = 3');

  ts.remove(5);
  assertEq(ts.count(5), 1, 'count after remove');

  const clone = ts.clone();
  assertEq(clone.count(5), 1, 'clone count matches');
  assertEq(clone.total(), 2, 'clone total matches');
  clone.add(20);
  assertEq(ts.count(20), 0, 'original unaffected by clone mutation');

  const ts2 = new TileSet();
  ts2.add(5);
  ts2.add(5);
  ts2.add(5);
  ts2.add(5);
  assertEq(ts2.count(5), 4, 'count 4 ok');

  // clear
  ts2.clear();
  assertEq(ts2.total(), 0, 'clear → 0');

  // copyFrom
  const ts3 = new TileSet();
  ts3.add(0);
  ts3.add(0);
  const ts4 = new TileSet();
  ts4.copyFrom(ts3);
  assertEq(ts4.count(0), 2, 'copyFrom ok');

  // addAll
  const ts5 = new TileSet();
  ts5.add(1);
  ts5.add(1);
  ts5.addAll(ts3);
  assertEq(ts5.count(1), 2, 'addAll self ok');
  assertEq(ts5.count(0), 2, 'addAll other ok');
}

// ============================================================
// SimGameState
// ============================================================

function test_simstate() {
  console.log('\n--- SimGameState ---');

  const s = new SimGameState();
  assertEq(s.handSize(0), 0, 'initial hand size 0');
  assertEq(s.poolSize(), 0, 'initial pool size 0');

  s.hands[0].add(0);
  s.hands[0].add(0);
  assertEq(s.handSize(0), 2, 'hand size 2');

  const cloned = s.clone();
  cloned.hands[0].add(1);
  assertEq(s.handSize(0), 2, 'original unaffected by clone');
  assertEq(cloned.handSize(0), 3, 'clone has 3');
  assertEq(cloned.winner, -1, 'clone winner = -1');
  assertEq(cloned.drawReason, null, 'clone drawReason = null');

  // verifyInvariant with empty everything = broken (all 0 not 4)
  assertEq(s.verifyInvariant(), false, 'empty state fails invariant');

  // Fill with full deck: pool = 4 of each, others empty
  const s2 = new SimGameState();
  for (let i = 0; i < 34; i++) s2.pool.data[i] = 4;
  assertEq(s2.verifyInvariant(), true, 'full pool passes invariant');
}

// ============================================================
// Win check — cross-validate against engine.js
// ============================================================

function test_winCheck() {
  console.log('\n--- Win check ---');

  const wildKey = 'honor:white';
  const wildIdx = 33;

  // Test 1: Standard winning hand (ping hu)
  // 1-2-3 char, 4-5-6 dot, 7-8-9 bamboo, 1-2-3 char, red-red pair → 14 tiles
  const win1 = [0,1,2, 12,13,14, 21,22,23, 0,1,2, 31,31];
  const hand1 = make14TileHand(win1);
  assertEq(hand1.total(), 14, 'win1 has 14 tiles');
  assert(isWinningHandSet(hand1, wildIdx), 'standard win (4 melds + pair)');

  // Cross-validate with engine.js
  const tiles1 = win1.map(idx => {
    const { suit, rank } = indexToTileType(idx);
    return createTile(suit, rank);
  });
  assert(isWinningHand(tiles1, wildKey), 'engine agrees: standard win');

  // Test 2: Seven pairs
  // 7 different pairs, no wild
  const win2_indices = [0,0, 3,3, 6,6, 9,9, 12,12, 18,18, 21,21];
  const hand2 = make14TileHand(win2_indices);
  assert(isWinningHandSet(hand2, wildIdx), 'seven pairs win');
  // Cross-validate
  const tiles2 = win2_indices.map(idx => {
    const { suit, rank } = indexToTileType(idx);
    return createTile(suit, rank);
  });
  assert(isWinningHand(tiles2, wildKey), 'engine agrees: seven pairs');

  // Test 3: Win with 1 wild tile
  // 1 wild + 13 tiles that form 4 melds + 1 incomplete pair
  const win3 = [0,1,2, 9,10,11, 18,19,20, 3,4,5, 6, 33]; // 33 = wild (honor:white)
  const hand3 = make14TileHand(win3);
  assertEq(hand3.total(), 14, 'win3 has 14 tiles');
  assert(isWinningHandSet(hand3, wildIdx), 'win with 1 wild (wild + 6 = pair)');

  // Cross-validate
  const tiles3 = win3.map(idx => {
    const { suit, rank } = indexToTileType(idx);
    return createTile(suit, rank);
  });
  assert(isWinningHand(tiles3, wildKey), 'engine agrees: 1-wild win');

  // Test 4: Non-winning hand
  const nonWin = [0,1,3, 5,7,9, 11,13,15, 17,19,21, 23,25];
  const hand4 = make14TileHand(nonWin);
  assert(!isWinningHandSet(hand4, wildIdx), 'non-winning hand');

  const tiles4 = nonWin.map(idx => {
    const { suit, rank } = indexToTileType(idx);
    return createTile(suit, rank);
  });
  assert(!isWinningHand(tiles4, wildKey), 'engine agrees: non-win');

  // Test 5: Tenpai with 13 tiles (should return false for win check)
  const tenpai1 = [0,1,2, 9,10,11, 18,19,20, 3,4,5, 6]; // 13 tiles
  const hand5 = make14TileHand(tenpai1);
  assertEq(hand5.total(), 13, '13 tiles');
  assert(!isWinningHandSet(hand5, wildIdx), '13 tiles not winning');
}

// ============================================================
// Shanten
// ============================================================

function test_shanten() {
  console.log('\n--- Shanten ---');

  const wildIdx = 33;

  // Winning hand → shanten = -1
  const win1 = [0,1,2, 12,13,14, 21,22,23, 0,1,2, 31,31];
  const handWin = make14TileHand(win1);
  assertEq(computeShanten(handWin, wildIdx), -1, 'winning hand → shanten -1');

  // Tenpai hand (13 tiles, 1 away from win) → shanten = 0
  // 1-2-3 char, 4-5-6 dot, 7-8-9 bamb, 1-2-3 char, single 6 — wait, that's 13 tiles
  const tenpai = [0,1,2, 9,10,11, 18,19,20, 3,4,5, 6];
  const handTenpai = make14TileHand(tenpai);
  assertEq(computeShanten(handTenpai, wildIdx), 0, 'tenpai hand → shanten 0');

  // 1-shanten hand: 3 complete melds + 2 isolated → needs 1 more tile
  // 1-2-3 char, 4-5-6 dot, 7-8-9 bamb, two singles
  const shanten1 = [0,1,2, 9,10,11, 18,19,20, 6, 25];
  const handS1 = make14TileHand(shanten1);
  const s1 = computeShanten(handS1, wildIdx);
  assert(s1 >= 1 && s1 <= 3, `1-shanten hand: shanten=${s1} (expected 1-3)`);

  // Completely random hand → high shanten
  const random13 = [0,4,8,12,16,20,24,27,28,29,30,31,32];
  const handRandom = make14TileHand(random13);
  const sRandom = computeShanten(handRandom, wildIdx);
  assert(sRandom >= 3, `random hand shanten=${sRandom} (expected >= 3)`);

  // shantenAfterDiscard: 14-tile hand, test after removing a tile
  const hand14 = [0,1,2, 9,10,11, 18,19,20, 3,4,5, 6, 27];
  const hand14Set = make14TileHand(hand14);
  const sa = shantenAfterDiscard(hand14Set, 27, wildIdx); // discard honor:east
  assert(sa >= 0, `shantenAfterDiscard returns valid: ${sa}`);
  // Should be better than keeping it
  const sb = computeShanten(hand14Set, wildIdx);
  assert(sa <= sb + 1, 'shanten after discard ≤ original shanten + 1');
}

// ============================================================
// chooseBestDiscard
// ============================================================

function test_bestDiscard() {
  console.log('\n--- chooseBestDiscard ---');

  const state = new SimGameState();
  state.wildIdx = 33;

  // Hand: 3 complete melds + 1 pair + 3 loose tiles = 14 tiles
  // melds: 0,1,2 (char1-3), 9,10,11 (dot1-3), 18,19,20 (bam1-3)
  // pair: 31,31 (honor:red pair)
  // loose: 6 (char7), 27 (honor:east), 28 (honor:south)
  // Best discard: any loose tile (6, 27, or 28), first one is 6
  const indices = [0,1,2, 9,10,11, 18,19,20, 31,31, 6, 27, 28];
  for (const idx of indices) state.hands[0].add(idx);

  const result = chooseBestDiscard(state, 0);
  assert(result.tileIdx >= 0 && result.tileIdx < 34, 'valid tile idx returned');
  assert(result.shanten >= 0, 'valid shanten returned');

  // The best discard should be a loose tile (6, 27, or 28)
  // Not one of the meld tiles or pair tiles
  assert([6, 27, 28].includes(result.tileIdx),
    `best discard is loose tile (got ${result.tileIdx}, expected 6, 27, or 28)`);
}

// ============================================================
// drawRandom
// ============================================================

function test_drawRandom() {
  console.log('\n--- drawRandom ---');

  const pool = new TileSet();
  assertEq(drawRandom(pool), -1, 'empty pool → -1');

  pool.add(5);
  pool.add(5);
  pool.add(10);
  const drawn = drawRandom(pool);
  assert(drawn === 5 || drawn === 10, 'draws from populated pool');
  // Probability: 2/3 for 5, 1/3 for 10
}

// ============================================================
// Rollout
// ============================================================

function test_rollout() {
  console.log('\n--- Rollout ---');

  // Create a game state with dealt hands and pool
  const state = new SimGameState();
  state.wildIdx = 33;
  state.dealer = 0;
  state.turn = 0;

  // Deal 14 to player 0, 13 to others
  // For simplicity, just give random tiles
  // Player 0 gets a decent hand (near tenpai)
  const p0Hand = [0,1,2, 9,10,11, 18,19,20, 3,4,5, 6, 27]; // 14 tiles
  for (const idx of p0Hand) state.hands[0].add(idx);

  // Other players get random 13-tile hands
  const otherTiles = [30,30, 28,28, 7,7, 8,8, 16,16,17, 25,25];
  for (const idx of otherTiles) state.hands[1].add(idx);
  const other2 = [31,31, 29,29, 5,5, 13,13,14, 22,22,23, 26];
  for (const idx of other2) state.hands[2].add(idx);
  const other3 = [15,15, 4,4, 10,10,11, 19,19,20, 0,0, 12];
  for (const idx of other3) state.hands[3].add(idx);

  // Fill the pool with remaining tiles
  state.computeRemainingPool();
  assert(state.pool.total() > 0, 'pool has tiles');
  assert(state.verifyInvariant(), 'invariant holds');

  // Run rollout
  const result = rollout(state);
  // Should have a winner or draw reason
  assert(result.winner >= 0 || result.drawReason !== null, 'rollout reached terminal state');

  // rolloutReward
  if (result.winner >= 0) {
    assertEq(rolloutReward(result, result.winner), 1.0, 'winner gets reward 1');
    if (result.winner !== 0) {
      assertEq(rolloutReward(result, 0), 0.0, 'loser gets reward 0');
    }
  } else {
    assertEq(rolloutReward(result, 0), 0.5, 'draw → reward 0.5');
  }
}

// ============================================================
// fromGameState
// ============================================================

function test_fromGameState() {
  console.log('\n--- fromGameState ---');

  const uiGame = createNewGame();

  // Manually set known hand for player 0 for testing
  const sim = fromGameState(uiGame);

  assertEq(sim.turn, 0, 'turn matches');
  assertEq(sim.dealer, 0, 'dealer matches');
  assertEq(sim.wildIdx, 33, 'wildIdx = honor:white');
  assertEq(sim.handSize(0), 14, 'dealer has 14 tiles');
  assertEq(sim.handSize(1), 13, 'non-dealer has 13 tiles');
  assertEq(sim.handSize(2), 13, 'non-dealer has 13 tiles');
  assertEq(sim.handSize(3), 13, 'non-dealer has 13 tiles');
  assert(sim.verifyInvariant(), 'fromGameState invariant holds');
}

// ============================================================
// Determinize
// ============================================================

function test_determinize() {
  console.log('\n--- Determinize ---');

  const state = new SimGameState();
  state.wildIdx = 33;

  // Player 0 (observer) has known hand
  const p0Hand = [0,1,2, 9,10,11, 18,19,20, 3,4,5, 6, 27];
  for (const idx of p0Hand) state.hands[0].add(idx);

  // All unseen tiles in pool
  for (let i = 0; i < 34; i++) {
    const used = state.hands[0].data[i] + state.hands[1].data[i]
      + state.hands[2].data[i] + state.hands[3].data[i];
    state.pool.data[i] = 4 - used;
  }

  const det = determinize(state, 0);

  // Observer's hand unchanged
  for (let i = 0; i < 34; i++) {
    assertEq(det.hands[0].data[i], state.hands[0].data[i],
      `observer hand[${i}] unchanged`);
  }

  // Other players' hands are filled
  assertEq(det.handSize(1), 0, 'no tiles assigned when hand was empty');
  // Actually, determinize only assigns tiles to opponents if they already
  // have hand sizes > 0. With empty opponent hands, it does nothing.
  assert(det.pool.total() > 0, 'pool still has tiles');

  // Test with opponent hand sizes
  const state2 = new SimGameState();
  state2.wildIdx = 33;
  for (const idx of p0Hand) state2.hands[0].add(idx);
  state2.hands[1].add(0); state2.hands[1].add(0); // placeholder counts
  state2.hands[2].add(1); state2.hands[2].add(1);
  state2.hands[3].add(2); state2.hands[3].add(2);
  for (let i = 0; i < 34; i++) {
    const used = state2.hands[0].data[i] + state2.hands[1].data[i]
      + state2.hands[2].data[i] + state2.hands[3].data[i];
    state2.pool.data[i] = 4 - used;
  }

  const det2 = determinize(state2, 0);
  assertEq(det2.handSize(0), 14, 'observer hand preserved');
  // Opponents should have their tiles filled from pool
  assertEq(det2.handSize(1) + det2.handSize(2) + det2.handSize(3),
    state2.handSize(1) + state2.handSize(2) + state2.handSize(3),
    'opponent hand sizes preserved after determinization');
}

// ============================================================
// Performance: clone speed
// ============================================================

function test_perf() {
  console.log('\n--- Performance ---');

  const state = new SimGameState();
  state.wildIdx = 33;
  // Fill with a full game
  for (let i = 0; i < 34; i++) state.pool.data[i] = 4;

  const ITERS = 10000;
  const start = performance.now();
  for (let i = 0; i < ITERS; i++) {
    state.clone();
  }
  const elapsed = performance.now() - start;
  const usPerClone = (elapsed / ITERS) * 1000;
  console.log(`  Clone: ${usPerClone.toFixed(2)} μs/op (${ITERS} iterations, ${elapsed.toFixed(0)} ms total)`);

  // Rollout speed
  const hand = [0,1,2, 9,10,11, 18,19,20, 3,4,5, 6, 27]; // 14 tiles
  for (const idx of hand) state.hands[0].add(idx);
  state.hands[1].add(30); state.hands[1].add(30);
  state.hands[2].add(31); state.hands[2].add(31);
  state.hands[3].add(28); state.hands[3].add(28);
  state.computeRemainingPool();

  const ROLLOUTS = 500;
  const rstart = performance.now();
  for (let i = 0; i < ROLLOUTS; i++) {
    const clone = state.clone();
    rollout(clone);
  }
  const relapsed = performance.now() - rstart;
  const rolloutsPerSec = ROLLOUTS / (relapsed / 1000);
  console.log(`  Rollout: ${rolloutsPerSec.toFixed(0)}/sec (${ROLLOUTS} rollouts, ${relapsed.toFixed(0)} ms total)`);

  assert(rolloutsPerSec > 300, `>300 rollouts/sec (got ${rolloutsPerSec.toFixed(0)})`);
}

// ============================================================
// Run all tests
// ============================================================

console.log('=== game-sim Tests ===\n');

test_indexing();
test_tileset();
test_simstate();
test_winCheck();
test_shanten();
test_bestDiscard();
test_drawRandom();
test_rollout();
test_fromGameState();
test_determinize();
test_perf();

console.log(`\n=== Results: ${passed} passed, ${failed} failed ===`);
if (failed > 0) process.exit(1);
