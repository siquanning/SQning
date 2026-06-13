// ============================================================
// coach-interpreter.test.js — Tests for search-interpreter + nl-explainer
// Run: node test/coach-interpreter.test.js
// ============================================================

import {
  extractTopK,
  detectThreats,
  analyzeSafeTiles,
  detectDirectionDivergence,
  detectBigVsFast,
  assessPiaoCaiWindow,
  assessConfidence,
  interpretSearch,
  THREAT_THRESHOLD,
} from '../src/coach/search-interpreter.js';

import {
  describeWinRate,
  describeReliability,
  buildTemplateContext,
  explainDiscard,
  explainDirection,
  explainThreat,
  explainTiming,
  explainDeep,
  attachKnowledgeTags,
  sanitizeWording,
  sanitizeMessage,
  explainTerm,
  annotateTerms,
  generateExplanations,
  KNOWLEDGE_TAGS,
  resetTemplateCounters,
} from '../src/coach/nl-explainer.js';

import {
  TileSet, SimGameState, tileTypeIndex, computeShanten,
} from '../src/ai/game-sim.js';

import {
  MultiPlayerModel, OpponentModel,
} from '../src/ai/opponent-model.js';

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

function assertBetween(actual, lo, hi, msg) {
  if (actual >= lo && actual <= hi) { passed++; }
  else { failed++; console.error(`  FAIL: ${msg} — expected ${lo}-${hi}, got ${actual}`); }
}

function assertDeep(actual, expected, msg) {
  const a = JSON.stringify(actual);
  const e = JSON.stringify(expected);
  if (a === e) { passed++; }
  else { failed++; console.error(`  FAIL: ${msg} — expected ${e}, got ${a}`); }
}

// ============================================================
// Mock data helpers
// ============================================================

function mockStats() {
  // Simulate ISMCTS stats: Map<string, {visits, winRate, value}>
  const map = new Map();
  map.set('discard:0', { visits: 450, winRate: 0.52, value: 234 });  // 一万
  map.set('discard:5', { visits: 380, winRate: 0.48, value: 182 });  // 六万
  map.set('discard:8', { visits: 320, winRate: 0.44, value: 141 });  // 九万
  map.set('discard:27', { visits: 280, winRate: 0.40, value: 112 }); // 东
  map.set('discard:30', { visits: 200, winRate: 0.38, value: 76 });  // 红
  map.set('discard:1', { visits: 150, winRate: 0.35, value: 53 });   // 二万
  return map;
}

function mockLowConfidenceStats() {
  const map = new Map();
  map.set('discard:5', { visits: 12, winRate: 0.55, value: 6.6 });
  map.set('discard:8', { visits: 8, winRate: 0.50, value: 4 });
  map.set('discard:3', { visits: 5, winRate: 0.40, value: 2 });
  return map;
}

function mockOpponentModel(observerId = 0) {
  const mpm = new MultiPlayerModel(observerId);

  // Player 1: likely tenpai (lots of melds, late game discards)
  const p1 = mpm.getModel(1);
  for (let i = 0; i < 15; i++) {
    p1.recordDiscard(i % 34, i, false);
  }
  p1.recordMeld('peng', [tileTypeIndex('dot', 3), tileTypeIndex('dot', 3), tileTypeIndex('dot', 3)], 20);
  // Manually boost tenpai probability for testing
  p1.tenpaiProbability = 0.88;

  // Player 2: possible tenpai
  const p2 = mpm.getModel(2);
  for (let i = 0; i < 10; i++) {
    p2.recordDiscard((i + 5) % 34, i, false);
  }
  p2.tenpaiProbability = 0.65;

  // Player 3: safe
  const p3 = mpm.getModel(3);
  for (let i = 0; i < 5; i++) {
    p3.recordDiscard((i + 10) % 34, i, false);
  }
  p3.tenpaiProbability = 0.15;

  return mpm;
}

function mockHandInfo() {
  return {
    shanten: 1,
    waits: [
      { discard: { suit: 'honor', rank: 'east' }, waits: [{ suit: 'character', rank: 3 }, { suit: 'character', rank: 6 }] },
    ],
    wildCount: 1,
    canPiaoCai: false,
    hasGangOpportunity: false,
    meldCount: 0,
  };
}

function mockInterpretation() {
  const stats = mockStats();
  const oppModel = mockOpponentModel();
  const handInfo = mockHandInfo();

  return interpretSearch({
    stats,
    oppModel,
    handInfo,
    playerDiscardIdx: 5,
    observerId: 0,
    gameContext: { poolRemaining: 40, isDealer: false },
  });
}

// ============================================================
// 12a: search-interpreter.js tests
// ============================================================

console.log('\n=== 12a: search-interpreter.js ===\n');

// --- extractTopK ---

console.log('extractTopK:');

{
  const result = extractTopK(mockStats(), 3);
  assertEq(result.candidates.length, 3, 'extracts top 3 candidates');
  assertEq(result.candidates[0].tileIdx, 0, 'top candidate is tile 0 (highest win rate)');
  assertGte(result.candidates[0].winRate, result.candidates[1].winRate, 'sorted by win rate descending');
  assertEq(result.totalVisits, 1780, 'total visits sum correctly');
}

{
  const result = extractTopK(mockStats(), 3, 1);
  // playerDiscardIdx=1 (二万) should be included even if outside top 3
  const hasPlayerTile = result.candidates.some(c => c.tileIdx === 1);
  assert(hasPlayerTile, 'includes player discard even outside top-K');
}

{
  const result = extractTopK(null);
  assertEq(result.candidates.length, 0, 'null stats returns empty');
  assertEq(result.totalVisits, 0, 'null stats has 0 total visits');
}

{
  const result = extractTopK(new Map());
  assertEq(result.candidates.length, 0, 'empty stats returns empty');
}

// --- detectThreats ---

console.log('detectThreats:');

{
  const oppModel = mockOpponentModel();
  const result = detectThreats(oppModel, { observerId: 0, candidateTileIndices: [5, 8, 27] });

  assertEq(result.threats.length, 3, 'detects 3 opponents');
  const p1 = result.threats.find(t => t.playerId === 1);
  assertEq(p1.threatLevel, 'likely', 'player 1 is likely tenpai (88%)');
  const p2 = result.threats.find(t => t.playerId === 2);
  assertEq(p2.threatLevel, 'possible', 'player 2 is possible tenpai (65%)');
  const p3 = result.threats.find(t => t.playerId === 3);
  assertEq(p3.threatLevel, 'none', 'player 3 has no threat (15%)');

  assert(result.anyThreatLikely, 'anyThreatLikely is true');
  assert(result.anyThreatPossible, 'anyThreatPossible is true');
  assertEq(result.highestThreat.playerId, 1, 'player 1 is highest threat');

  // Check danger breakdown
  assertEq(p1.dangerBreakdown.length, 3, 'has danger breakdown for 3 candidate tiles');
  assertGte(p1.dangerBreakdown[0].danger, 0, 'danger value is numeric');
}

{
  const result = detectThreats(null);
  assertEq(result.threats.length, 0, 'null model returns empty threats');
  assertEq(result.highestThreat, null, 'null model has no highest threat');
}

// --- analyzeSafeTiles ---

console.log('analyzeSafeTiles:');

{
  const oppModel = mockOpponentModel();
  const candidates = [
    { tileIdx: 0, label: '一万', winRate: 0.5, visits: 100 },
    { tileIdx: 27, label: '东', winRate: 0.4, visits: 80 },
    { tileIdx: 5, label: '六万', winRate: 0.3, visits: 60 },
  ];
  const result = analyzeSafeTiles(oppModel, candidates, 0);

  assertEq(result.perCandidate.length, 3, 'analyzes 3 candidates');
  assert(result.safestCandidate !== null, 'has safest candidate');
  assert(result.riskiestCandidate !== null, 'has riskiest candidate');

  // All should have per-candidate data
  for (const pc of result.perCandidate) {
    assertGte(pc.maxDanger, 0, `candidate ${pc.label} has danger >= 0`);
    assert(pc.maxDanger <= 1, `candidate ${pc.label} has danger <= 1`);
  }
}

{
  const result = analyzeSafeTiles(null, []);
  assertEq(result.perCandidate.length, 0, 'null model + empty candidates');
  assertEq(result.safestCandidate, null, 'null model has no safest');
}

// --- detectDirectionDivergence ---

console.log('detectDirectionDivergence:');

{
  // Create stats with two close win rates but different tile types
  const stats = new Map();
  stats.set('discard:0', { visits: 300, winRate: 0.52, value: 156 });  // 一万 (terminal)
  stats.set('discard:5', { visits: 280, winRate: 0.50, value: 140 });  // 六万 (middle)

  const handInfo = { shanten: 1 };
  const result = detectDirectionDivergence(stats, handInfo);

  assert(result.isFork, 'detects fork with close win rates');
  assert(result.pathA !== null, 'pathA exists');
  assert(result.pathB !== null, 'pathB exists');
  assert(result.pathA.handType !== result.pathB.handType, 'hand types differ at fork');
  assertGte(result.winRateGap, 0, 'win rate gap >= 0');
  assert(result.winRateGap < 0.05, 'win rate gap < 5%');
}

{
  // Create stats with clear winner (no fork)
  const stats = new Map();
  stats.set('discard:0', { visits: 400, winRate: 0.70, value: 280 });
  stats.set('discard:5', { visits: 100, winRate: 0.30, value: 30 });

  const result = detectDirectionDivergence(stats, {});
  assert(!result.isFork, 'no fork when win rates are far apart');
}

{
  const result = detectDirectionDivergence(null, {});
  assert(!result.isFork, 'null stats = no fork');
}

// --- detectBigVsFast ---

console.log('detectBigVsFast:');

{
  // Create stats with divergence
  const stats = new Map();
  stats.set('discard:27', { visits: 300, winRate: 0.55, value: 165 });  // 东 (fast: honor discard)
  stats.set('discard:3', { visits: 250, winRate: 0.48, value: 120 });  // 四万 (big: middle tile + wild context)

  const handInfo = { shanten: 2, wildCount: 2, canPiaoCai: true, hasGangOpportunity: false };
  const result = detectBigVsFast(stats, handInfo);

  // BigVsFast should have at least one path
  assert(result.bigHandPath !== null || result.fastHandPath !== null, 'has at least one path');
}

{
  const stats = new Map();
  stats.set('discard:0', { visits: 100, winRate: 0.40, value: 40 });

  const result = detectBigVsFast(stats, {});
  assert(!result.hasDivergence, 'single candidate = no divergence');
}

// --- assessPiaoCaiWindow ---

console.log('assessPiaoCaiWindow:');

{
  const result = assessPiaoCaiWindow({
    shanten: 2,
    wildCount: 2,
    poolRemaining: 40,
    isDealer: false,
  });

  assert(result.shouldConsider, 'should consider piao cai with 2 wild + shanten 2');
  assertGte(result.piaoCaiExpectedScore, 0, 'piao cai score >= 0');
  assertGte(result.directWinExpectedScore, 0, 'direct win score >= 0');
  assert(typeof result.riskLevel === 'string', 'risk level is string');
  assert(typeof result.reasoning === 'string', 'has reasoning');
}

{
  const result = assessPiaoCaiWindow({
    shanten: 2,
    wildCount: 0,
    poolRemaining: 40,
  });

  assert(!result.shouldConsider, 'should not consider without 2 wild');
  assertEq(result.riskLevel, 'high', 'risk is high without wild');
}

{
  const result = assessPiaoCaiWindow({
    shanten: -1,
    wildCount: 3,
    poolRemaining: 40,
  });

  assert(!result.shouldConsider, 'should not consider when already winning');
}

// --- assessConfidence ---

console.log('assessConfidence:');

{
  const result = assessConfidence(mockStats());

  assert(result.isReliable, 'mockStats is reliable');
  assert(result.confidenceLevel === 'high' || result.confidenceLevel === 'medium',
    'confidence is high or medium');
  assertGte(result.totalVisits, 500, 'total visits >= 500');
  assert(typeof result.summary === 'string', 'has summary text');
}

{
  const result = assessConfidence(mockLowConfidenceStats());

  assert(!result.isReliable, 'low confidence stats are not reliable');
  assert(result.confidenceLevel === 'low' || result.confidenceLevel === 'insufficient',
    'confidence is low or insufficient');
  assertGte(result.totalVisits, 0, 'has some visits');
}

{
  const result = assessConfidence(null);
  assert(!result.isReliable, 'null stats not reliable');
  assertEq(result.confidenceLevel, 'insufficient', 'null = insufficient');
}

{
  const result = assessConfidence(new Map());
  assertEq(result.confidenceLevel, 'insufficient', 'empty map = insufficient');
}

// --- interpretSearch (full pipeline) ---

console.log('interpretSearch:');

{
  const result = interpretSearch({
    stats: mockStats(),
    oppModel: mockOpponentModel(),
    handInfo: mockHandInfo(),
    playerDiscardIdx: 5,
    observerId: 0,
    gameContext: { poolRemaining: 40, isDealer: false },
  });

  // Check all sections present
  assert(result.topK.candidates.length > 0, 'has topK candidates');
  assert(result.threatAnalysis.threats.length === 3, 'has threat analysis');
  assert(result.safeAnalysis.perCandidate.length > 0, 'has safe analysis');
  assert(typeof result.divergence.isFork === 'boolean', 'has divergence result');
  assert(typeof result.bigVsFast.hasDivergence === 'boolean', 'has bigVsFast result');
  assert(typeof result.piaoCai.shouldConsider === 'boolean', 'has piaoCai result');
  assert(typeof result.confidence.isReliable === 'boolean', 'has confidence result');
  assert(result.playerComparison.playerPicked !== null, 'has player comparison');
  assert(typeof result.timestamp === 'number', 'has timestamp');
}

// ============================================================
// 12b: nl-explainer.js tests
// ============================================================

console.log('\n=== 12b: nl-explainer.js ===\n');

// --- describeWinRate ---

console.log('describeWinRate:');

{
  const r = describeWinRate(0.65);
  assertEq(r.level, '较高', '65% → 较高');
  assertEq(r.phrase, '有把握', '65% → 有把握');
  assertEq(r.bucket, '65%', '65% → bucket 65%');
}

{
  const r = describeWinRate(0.62);
  assertEq(r.bucket, '60%', '62% rounds to 60% bucket');
}

{
  const r = describeWinRate(0.48);
  assertEq(r.level, '接近', '48% → 接近');
  assertEq(r.phrase, '五五开，需要权衡', '48% → 五五开');
}

{
  const r = describeWinRate(0.90);
  assertEq(r.level, '极高', '90% → 极高');
}

{
  const r = describeWinRate(0.15);
  assertEq(r.level, '极低', '15% → 极低');
}

// --- describeReliability ---

console.log('describeReliability:');

{
  const r = describeReliability({ confidenceLevel: 'high' });
  assertEq(r.phrase, '结论可靠', 'high → 结论可靠');
}

{
  const r = describeReliability({ confidenceLevel: 'low' });
  assert(r.phrase.includes('仅供参考'), 'low → includes 仅供参考');
}

// --- buildTemplateContext ---

console.log('buildTemplateContext:');

{
  const interp = mockInterpretation();
  const ctx = buildTemplateContext(interp, {
    windNames: ['自己', '对家', '上家', '下家'],
  });

  assert(typeof ctx.tileLabel === 'string', 'has tileLabel');
  assert(typeof ctx.aiTileLabel === 'string', 'has aiTileLabel');
  assert(typeof ctx.isTenpai === 'boolean', 'has isTenpai');
  assert(typeof ctx.shanten === 'number' || ctx.shanten === '?', 'has shanten');
  assert(typeof ctx.wildCount === 'number', 'has wildCount');
  // Threat context
  assert(typeof ctx.threatPlayer === 'string', 'has threatPlayer');
  assert(typeof ctx.threatDesc === 'string', 'has threatDesc');
}

// --- explainDiscard ---

console.log('explainDiscard:');

{
  resetTemplateCounters();
  const interp = mockInterpretation();
  const ctx = buildTemplateContext(interp, { windNames: ['自己', '对家', '上家', '下家'] });

  // Player discarded tile 5, AI top is tile 0 → mismatch
  const result = explainDiscard(ctx, false);
  assert(typeof result.summary === 'string', 'mismatch produces summary');
  assert(result.summary.includes('建议') || result.summary.includes('推荐') || result.summary.includes('考虑'),
    'uses suggestive language');
  assert(result.tags.includes('弃牌对比'), 'mismatch has 弃牌对比 tag');
}

{
  resetTemplateCounters();
  const interp = mockInterpretation();
  // Override player discarding tile 0 (top pick)
  interp.playerComparison.matched = true;
  interp.playerComparison.playerPicked = interp.topK.candidates[0];
  const ctx = buildTemplateContext(interp, { windNames: ['自己', '对家', '上家', '下家'] });

  const result = explainDiscard(ctx, true);
  assert(typeof result.summary === 'string', 'match produces summary');
  assert(result.tags.includes('弃牌一致'), 'match has 弃牌一致 tag');
}

// --- explainDirection ---

console.log('explainDirection:');

{
  resetTemplateCounters();
  const interp = mockInterpretation();
  // Add hand info
  interp._handInfo = { shanten: 1, waits: [], wildCount: 0 };
  const ctx = buildTemplateContext(interp, { windNames: ['自己', '对家', '上家', '下家'] });
  ctx.isTenpai = false;
  ctx.shanten = 1;

  const result = explainDirection(ctx);
  assert(typeof result.summary === 'string', 'produces direction summary');
  assert(result.summary.includes('1') || result.summary.includes('一'),
    'mentions shanten number');
}

{
  resetTemplateCounters();
  const interp = mockInterpretation();
  interp._handInfo = {
    shanten: 0,
    waits: [{ discard: 27, waits: [3, 6] }],
    wildCount: 1,
  };
  const ctx = buildTemplateContext(interp, { windNames: ['自己', '对家', '上家', '下家'] });
  ctx.isTenpai = true;

  const result = explainDirection(ctx);
  assert(result.summary.includes('听牌'), 'tenpai message mentions 听牌');
  assert(result.tags.includes('听牌分析'), 'tenpai has 听牌分析 tag');
}

// --- explainThreat ---

console.log('explainThreat:');

{
  resetTemplateCounters();
  const interp = mockInterpretation();
  const ctx = buildTemplateContext(interp, { windNames: ['自己', '对家', '上家', '下家'] });

  const result = explainThreat(ctx);
  assert(result !== null, 'produces threat when threat exists');
  assert(result.summary.includes('已听牌') || result.summary.includes('可能'),
    'threat mentions 听牌');
  assert(result.tags.includes('威胁预警'), 'has 威胁预警 tag');
}

{
  resetTemplateCounters();
  const result = explainThreat({ threatDesc: '暂无威胁' });
  assertEq(result, null, 'no threat when desc is 暂无威胁');
}

// --- explainTiming ---

console.log('explainTiming:');

{
  resetTemplateCounters();
  const interp = mockInterpretation();
  interp._handInfo = { shanten: 2, wildCount: 2 };
  const ctx = buildTemplateContext(interp, { windNames: ['自己', '对家', '上家', '下家'] });
  ctx.timingType = 'piaoCai';

  const result = explainTiming(ctx);
  assert(result !== null, 'produces piaoCai timing');
  assert(result.summary.includes('财神'), 'mentions 财神');
  assert(result.tags.includes('时机提醒'), 'has 时机提醒 tag');
  assert(result.tags.includes('飘财窗口'), 'has 飘财窗口 tag');
}

{
  resetTemplateCounters();
  const interp = mockInterpretation();
  interp._handInfo = { shanten: 0, wildCount: 1 };
  const ctx = buildTemplateContext(interp, { windNames: ['自己', '对家', '上家', '下家'] });
  ctx.timingType = 'baoTou';

  const result = explainTiming(ctx);
  assert(result !== null, 'produces baoTou timing');
  assert(result.tags.includes('爆头就绪'), 'has 爆头就绪 tag');
}

{
  resetTemplateCounters();
  const result = explainTiming({ timingType: '' });
  assertEq(result, null, 'no timing message when type is empty');
}

// --- explainDeep ---

console.log('explainDeep:');

{
  resetTemplateCounters();
  const interp = mockInterpretation();
  const ctx = buildTemplateContext(interp, { windNames: ['自己', '对家', '上家', '下家'] });

  const result = explainDeep(ctx);
  assert(result !== null, 'produces deep analysis with sufficient visits');
  assert(result.summary.includes('模拟'), 'mentions 模拟 count');
  assert(result.tags.includes('深度解析'), 'has 深度解析 tag');
}

{
  resetTemplateCounters();
  const result = explainDeep({ totalVisits: 50 });
  assertEq(result, null, 'no deep analysis when insufficient visits');
}

// --- attachKnowledgeTags ---

console.log('attachKnowledgeTags:');

{
  const msg = { summary: 'test', tags: ['弃牌对比'] };
  const ctx = { tileLabel: '一万', totalVisits: 500 };
  const result = attachKnowledgeTags(msg, ctx);

  assert(result.knowledgeTags.includes(KNOWLEDGE_TAGS.EFFICIENCY), 'discard has efficiency tag');
  assert(result.knowledgeTags.includes(KNOWLEDGE_TAGS.PROBABILITY), 'has probability tag');
  assert(result.tags.includes('弃牌对比'), 'original tags preserved');
}

{
  const msg = { summary: 'test', tags: ['威胁预警'] };
  const ctx = { threatProb: 0.9 };
  const result = attachKnowledgeTags(msg, ctx);

  assert(result.knowledgeTags.includes(KNOWLEDGE_TAGS.DEFENSE), 'threat has defense tag');
}

{
  const msg = { summary: 'test', tags: [] };
  const ctx = { timingType: 'piaoCai' };
  const result = attachKnowledgeTags(msg, ctx);

  assert(result.knowledgeTags.includes(KNOWLEDGE_TAGS.SCORING), 'piaoCai has scoring tag');
  assert(result.knowledgeTags.includes(KNOWLEDGE_TAGS.TIMING), 'piaoCai has timing tag');
}

// --- sanitizeWording ---

console.log('sanitizeWording:');

{
  assertEq(sanitizeWording('你必须打这张牌'), '你建议打这张牌', '必须 → 建议');
  assertEq(sanitizeWording('一定不要打'), '建议不要打', '一定 → 建议');
  assertEq(sanitizeWording('打错了'), '可以优化', '打错了 → 可以优化');
  assertEq(sanitizeWording('这个选择不好'), '这个选择不是最优', '不好 → 不是最优');
  assertEq(sanitizeWording('这是个错误'), '这是个值得商榷', '错误 → 值得商榷');
  assertEq(sanitizeWording('绝对不能打'), '尽量避免打', '绝对不能 → 尽量避免');
}

// --- sanitizeMessage ---

console.log('sanitizeMessage:');

{
  const msg = { summary: '你必须打东', reasoning: '这个选择不好' };
  const result = sanitizeMessage(msg);
  assertEq(result.summary, '你建议打东', 'sanitizes summary');
  assertEq(result.reasoning, '这个选择不是最优', 'sanitizes reasoning');
}

{
  assertEq(sanitizeMessage(null), null, 'null message returns null');
}

// --- explainTerm ---

console.log('explainTerm:');

{
  const expl = explainTerm('听牌');
  assert(expl !== null, '听牌 has explanation');
  assert(expl.includes('胡牌'), '听牌 explanation mentions 胡牌');
}

{
  const expl = explainTerm('nonexistent');
  assertEq(expl, null, 'unknown term returns null');
}

// --- annotateTerms ---

console.log('annotateTerms:');

{
  const text = '已听牌，注意避免点炮。';
  const annotations = annotateTerms(text);
  assertGte(annotations.length, 2, 'finds 听牌 and 点炮');
}

// --- generateExplanations ---

console.log('generateExplanations:');

{
  resetTemplateCounters();
  const interp = mockInterpretation();
  interp._handInfo = mockHandInfo();

  const messages = generateExplanations(interp, {
    windNames: ['自己', '对家', '上家', '下家'],
  });

  assertGte(messages.length, 1, 'produces at least 1 message');
  assert(messages.length <= 3, 'produces at most 3 messages');

  // Check priority ordering
  for (let i = 1; i < messages.length; i++) {
    assert(messages[i - 1].priority >= messages[i].priority,
      'messages sorted by priority descending');
  }

  // Check each message has required fields
  for (const msg of messages) {
    assert(typeof msg.type === 'string', 'message has type');
    assert(typeof msg.priority === 'number', 'message has priority');
    assert(typeof msg.summary === 'string', 'message has summary');
    assert(Array.isArray(msg.tags), 'message has tags array');
    assert(Array.isArray(msg.termAnnotations), 'message has termAnnotations array');
  }
}

// --- Template rotation ---

console.log('template rotation:');

{
  resetTemplateCounters();
  const interp = mockInterpretation();
  interp._handInfo = mockHandInfo();
  const ctx = buildTemplateContext(interp, { windNames: ['自己', '对家', '上家', '下家'] });
  ctx.isTenpai = false;

  const msg1 = explainDirection(ctx);
  const msg2 = explainDirection(ctx);
  // Should use different templates
  assert(msg1.summary !== msg2.summary, 'templates rotate (different summaries)');
}

// --- Wording norms in generated messages ---

console.log('wording norms:');

{
  resetTemplateCounters();
  const interp = mockInterpretation();
  interp._handInfo = mockHandInfo();
  const messages = generateExplanations(interp, {
    windNames: ['自己', '对家', '上家', '下家'],
  });

  for (const msg of messages) {
    // No message should use forbidden words
    assert(!msg.summary.includes('必须'), 'no 必须 in messages');
    assert(!msg.summary.includes('一定'), 'no 一定 in messages');
    assert(!msg.summary.includes('错误'), 'no 错误 in messages');
    assert(!msg.summary.includes('糟糕'), 'no 糟糕 in messages');
    assert(!msg.summary.includes('打错了'), 'no 打错了 in messages');
  }
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
