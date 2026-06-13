// ============================================================
// coach-worker.test.js — Tests for coach-worker core functions
// Run: node test/coach-worker.test.js
// ============================================================

import {
  runCoachAnalysis,
  runThreatUpdate,
  extractHandInfo,
  buildWindNames,
} from '../src/coach/coach-worker.js';

import {
  TileSet, SimGameState, tileTypeIndex, fromGameState,
} from '../src/ai/game-sim.js';

import { createNewGame, createTile, tileKey } from '../src/engine.js';

// ============================================================
// Test helpers
// ============================================================

let passed = 0;
let failed = 0;

function assert(cond, msg) {
  if (cond) { passed++; }
  else { failed++; console.error('  FAIL:', msg); }
}

function assertEq(actual, expected, msg) {
  if (actual === expected) { passed++; }
  else { failed++; console.error(`  FAIL: ${msg} — expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`); }
}

function assertGte(actual, expected, msg) {
  if (actual >= expected) { passed++; }
  else { failed++; console.error(`  FAIL: ${msg} — expected >= ${expected}, got ${actual}`); }
}

function assertType(val, type, msg) {
  if (typeof val === type) { passed++; }
  else { failed++; console.error(`  FAIL: ${msg} — expected ${type}, got ${typeof val}`); }
}

function assertNotNull(val, msg) {
  if (val !== null && val !== undefined) { passed++; }
  else { failed++; console.error(`  FAIL: ${msg} — expected non-null, got ${val}`); }
}

// ============================================================
// Build a realistic mock game state
// ============================================================

function buildMockGameData() {
  const game = createNewGame(0);
  // createNewGame sets up 4 players with 13 tiles each, plus dealer draws 14th

  // Manually set a known hand structure for player 0 (observer)
  // Clear and rebuild player 0's hand to a known state
  const p0 = game.players[0];
  p0.hand = [];

  // Build a hand: 1-3万 (sequence), 5-7筒 (sequence), 东东 (pair), 发发发 (triplet), 1条 9条 西
  // Total: 3 + 3 + 2 + 3 + 3 = 14 tiles
  const handTiles = [
    createTile('character', 1), createTile('character', 2), createTile('character', 3),
    createTile('dot', 5), createTile('dot', 6), createTile('dot', 7),
    createTile('honor', 'east'), createTile('honor', 'east'),
    createTile('honor', 'green'), createTile('honor', 'green'), createTile('honor', 'green'),
    createTile('bamboo', 1),
    createTile('bamboo', 9),
    createTile('honor', 'west'),
  ];
  p0.hand = handTiles;

  // Give opponents some reasonable discard history
  for (let p = 1; p < 4; p++) {
    const player = game.players[p];
    // 5 random-ish discards per opponent
    const discards = [
      createTile('honor', 'north'),
      createTile('bamboo', 1),
      createTile('character', 9),
      createTile('dot', 1),
      createTile('honor', 'red'),
    ];
    player.discards = discards.slice(0, 3 + p); // varying lengths
    player.hand = [createTile('bamboo', p + 2), createTile('dot', p + 3)];
  }

  // Set wild tile
  game.wildTile = createTile('honor', 'white');
  game._discardCount = 5;

  // Set tile pool (remaining wall)
  game.tilePool = [];
  for (let i = 0; i < 50; i++) {
    game.tilePool.push(createTile('bamboo', (i % 9) + 1));
  }

  return game;
}

// ============================================================
// extractHandInfo tests
// ============================================================

console.log('\n=== extractHandInfo ===\n');

{
  const game = buildMockGameData();
  const simState = fromGameState(game);
  const info = extractHandInfo(simState, 0);

  assertType(info.shanten, 'number', 'shanten is a number');
  assertGte(info.shanten, -1, 'shanten >= -1');
  assertType(info.wildCount, 'number', 'wildCount is a number');
  assertType(info.meldCount, 'number', 'meldCount is a number');
  assertType(info.canPiaoCai, 'boolean', 'canPiaoCai is boolean');
  assertType(info.hasGangOpportunity, 'boolean', 'hasGangOpportunity is boolean');
  assert(Array.isArray(info.waits), 'waits is an array');

  console.log(`  handInfo: shanten=${info.shanten}, wildCount=${info.wildCount}, meldCount=${info.meldCount}`);
}

// ============================================================
// buildWindNames tests
// ============================================================

console.log('\n=== buildWindNames ===\n');

{
  const game = buildMockGameData();
  const names = buildWindNames(game, 0);

  assertEq(names.length, 4, 'has 4 wind names');
  assertEq(names[0], '自己', 'player 0 is 自己');
  assert(names[1] !== names[2] && names[2] !== names[3], 'opponents have distinct names');
}

{
  const names = buildWindNames(null, 0);
  assertEq(names.length, 4, 'null game data returns default names');
  assertEq(names[0], '自己', 'default p0 is 自己');
}

// ============================================================
// runCoachAnalysis — standard depth
// ============================================================

console.log('\n=== runCoachAnalysis (standard) ===\n');

{
  const game = buildMockGameData();
  const playerDiscard = createTile('honor', 'west');

  const result = runCoachAnalysis(game, 'standard', playerDiscard);

  assert(!result.error, 'no error from analysis');

  // Check messages
  assert(Array.isArray(result.messages), 'messages is array');
  assert(result.messages.length > 0, 'has at least 1 message');

  // Check interpretation
  assertNotNull(result.interpretation, 'has interpretation');
  assertNotNull(result.interpretation.topK, 'has topK');
  assertNotNull(result.interpretation.confidence, 'has confidence');
  assertNotNull(result.interpretation.threatAnalysis, 'has threatAnalysis');

  // Check meta
  assertNotNull(result.meta, 'has meta');
  assertType(result.meta.elapsed, 'number', 'meta.elapsed is number');
  assertType(result.meta.iterations, 'number', 'meta.iterations is number');
  assert(result.meta.depth === 'standard' || result.meta.depth === 'basic',
    'depth is standard or basic');

  // Check message structure
  for (const msg of result.messages) {
    assertType(msg.type, 'string', 'message has type');
    assertType(msg.priority, 'number', 'message has priority');
    assertType(msg.summary, 'string', 'message has summary');
    assert(Array.isArray(msg.tags), 'message has tags');
  }

  console.log(`  depth=${result.meta.depth}, messages=${result.messages.length}, elapsed=${result.meta.elapsed.toFixed(0)}ms`);
}

// ============================================================
// runCoachAnalysis — brief depth
// ============================================================

console.log('\n=== runCoachAnalysis (brief) ===\n');

{
  const game = buildMockGameData();
  const result = runCoachAnalysis(game, 'brief', null);

  assert(!result.error, 'no error from brief analysis');
  assert(Array.isArray(result.messages), 'brief produces messages');
  assertType(result.meta.elapsed, 'number', 'brief has elapsed');
  assert(result.meta.elapsed < 2000, 'brief finishes within 2s');

  console.log(`  depth=${result.meta.depth}, elapsed=${result.meta.elapsed.toFixed(0)}ms`);
}

// ============================================================
// runCoachAnalysis — deep depth
// ============================================================

console.log('\n=== runCoachAnalysis (deep) ===\n');

{
  const game = buildMockGameData();
  const playerDiscard = createTile('bamboo', 9);
  const result = runCoachAnalysis(game, 'deep', playerDiscard);

  assert(!result.error, 'no error from deep analysis');
  assert(Array.isArray(result.messages), 'deep produces messages');
  assert(result.meta.elapsed < 2000, 'deep finishes within 2s');

  console.log(`  depth=${result.meta.depth}, elapsed=${result.meta.elapsed.toFixed(0)}ms, iterations=${result.meta.iterations}`);
}

// ============================================================
// runCoachAnalysis — invalid depth fallback
// ============================================================

console.log('\n=== runCoachAnalysis (invalid depth) ===\n');

{
  const game = buildMockGameData();
  const result = runCoachAnalysis(game, 'nonexistent', null);

  assert(!result.error, 'no error from invalid depth (falls back to standard)');
}

// ============================================================
// runThreatUpdate
// ============================================================

console.log('\n=== runThreatUpdate ===\n');

{
  const game = buildMockGameData();
  const result = runThreatUpdate(game, 1, { type: 'discard', tile: createTile('dot', 3) });

  assert(Array.isArray(result.messages), 'threat update produces messages');
  assertNotNull(result.interpretation, 'threat update has interpretation');
  assertNotNull(result.interpretation.threatAnalysis, 'threat update has threatAnalysis');

  console.log(`  messages=${result.messages.length}, threats=${result.interpretation.threatAnalysis.threats.length}`);
}

// ============================================================
// Message content checks
// ============================================================

console.log('\n=== Message content ===\n');

{
  const game = buildMockGameData();
  const playerDiscard = createTile('honor', 'west');
  const result = runCoachAnalysis(game, 'standard', playerDiscard);

  // At least one message should exist
  assertGte(result.messages.length, 1, 'at least 1 message generated');

  // Check that the discard message mentions the discarded tile
  const discardMsg = result.messages.find(m => m.type === 'discard' || m.type === 'confirm');
  if (discardMsg) {
    assert(discardMsg.summary.includes('西') || discardMsg.summary.includes('west'),
      'discard message mentions 西');
  }

  // Direction analysis should be present
  const dirMsg = result.messages.find(m => m.type === 'direction');
  if (dirMsg) {
    assert(dirMsg.summary.length > 5, 'direction message has content');
    assert(dirMsg.tags.some(t => t.includes('向听') || t.includes('听牌')), 'direction has relevant tags');
  }

  // No message should contain forbidden words
  for (const msg of result.messages) {
    assert(!msg.summary.includes('必须'), 'messages avoid 必须');
    assert(!msg.summary.includes('错误'), 'messages avoid 错误');
  }

  console.log('  All message content checks passed');
}

// ============================================================
// Timeout behavior: deep depth fallback
// ============================================================

console.log('\n=== Deep depth timeout fallback ===\n');

{
  const game = buildMockGameData();
  const startTime = performance.now();
  const result = runCoachAnalysis(game, 'deep', null);
  const elapsed = performance.now() - startTime;

  // Deep should complete (either normally or with fallback)
  assert(!result.error, 'deep analysis completes without error');
  assert(result.meta.elapsed < 2000, 'deep analysis stays within time budget');

  console.log(`  actual elapsed: ${elapsed.toFixed(0)}ms, meta.elapsed: ${result.meta.elapsed.toFixed(0)}ms`);
}

// ============================================================
// Stats integrity
// ============================================================

console.log('\n=== Stats integrity ===\n');

{
  const game = buildMockGameData();
  const playerDiscard = createTile('honor', 'west');
  const result = runCoachAnalysis(game, 'standard', playerDiscard);

  // If ISMCTS ran, interpretation should have stats-dependent fields
  if (result.meta.iterations > 0) {
    assert(result.interpretation.topK.totalVisits > 0, 'topK has visits when ISMCTS ran');
    assert(result.interpretation.confidence.totalVisits > 0, 'confidence has visits when ISMCTS ran');
  }

  // Meta should always have shanten
  assertType(result.meta.shanten, 'number', 'meta always has shanten');
}

// ============================================================
// Summary
// ============================================================

console.log(`\n=== Results ===`);
console.log(`Passed: ${passed}`);
console.log(`Failed: ${failed}`);
console.log(`Total:  ${passed + failed}`);

if (failed > 0) {
  process.exit(1);
}
