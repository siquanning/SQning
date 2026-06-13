// ============================================================
// test-step5.mjs — 计分系统测试（16种牌型 + calculateWinScore）
// 运行: node src/test-step5.mjs
// ============================================================

import { calculateWinScore, tileKey } from './engine.js';

const WILD_KEY = 'honor:white';

function t(suit, rank) {
  return { id: `${suit}-${rank}-${Math.random().toString(36).slice(2, 9)}`, suit, rank };
}

function assert(condition, msg) {
  if (!condition) throw new Error(`FAIL: ${msg}`);
  return true;
}

function assertFlags(hand, kind, expectedFlags, msg, opts = {}) {
  const result = calculateWinScore({
    hand, wildKey: WILD_KEY, kind: kind || '自摸',
    winningTile: opts.winningTile || null,
    piaoCount: opts.piaoCount || 0,
    isDealer: opts.isDealer || false,
    isFirstDraw: opts.isFirstDraw || false,
    isFirstTurn: opts.isFirstTurn || false,
  });
  const got = new Set(result.flags);
  for (const f of expectedFlags) {
    assert(got.has(f), `${msg}: expected flag "${f}", got [${result.flags.join(', ')}]`);
  }
  assert(result.multiplier > 0, `${msg}: multiplier should be > 0`);
  return result;
}

function assertMultiplier(hand, kind, expectedMult, msg, opts = {}) {
  const result = calculateWinScore({
    hand, wildKey: WILD_KEY, kind: kind || '自摸',
    winningTile: opts.winningTile || null,
    piaoCount: opts.piaoCount || 0,
    isDealer: opts.isDealer || false,
    isFirstDraw: opts.isFirstDraw || false,
    isFirstTurn: opts.isFirstTurn || false,
  });
  assert(result.multiplier === expectedMult,
    `${msg}: expected multiplier ${expectedMult}, got ${result.multiplier}, flags=[${result.flags.join(', ')}]`);
  return result;
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
// helpers: build various win hands
// ============================================================

function makeStdWin(suit) {
  const s = suit || 'character';
  return [
    t(s,1),t(s,2),t(s,3),
    t(s,4),t(s,5),t(s,6),
    t('dot',2),t('dot',3),t('dot',4),
    t('bamboo',7),t('bamboo',8),t('bamboo',9),
    t('honor','east'),t('honor','east'),
  ];
}

function makeSevenPairs() {
  return [
    t('character',1),t('character',1),
    t('character',5),t('character',5),
    t('dot',2),t('dot',2),
    t('dot',8),t('dot',8),
    t('bamboo',3),t('bamboo',3),
    t('bamboo',6),t('bamboo',6),
    t('honor','east'),t('honor','east'),
  ];
}

function makeLuxurySevenPairs() {
  return [
    t('character',1),t('character',1),
    t('character',1),t('character',1),
    t('dot',2),t('dot',2),
    t('dot',8),t('dot',8),
    t('bamboo',3),t('bamboo',3),
    t('bamboo',6),t('bamboo',6),
    t('honor','east'),t('honor','east'),
  ];
}

// ============================================================
// 1. 平胡
// ============================================================

test('平胡 — 标准4顺子+1对', () => {
  assertFlags(makeStdWin(), '自摸', ['平胡'], 'basic ping hu');
});

test('平胡 — multiplier=1', () => {
  assertMultiplier(makeStdWin(), '自摸', 1, 'ping hu base');
});

// ============================================================
// 2. 七对
// ============================================================

test('七对 — 7个对子', () => {
  assertFlags(makeSevenPairs(), '自摸', ['七对'], 'seven pairs');
});

test('七对 — multiplier=2', () => {
  assertMultiplier(makeSevenPairs(), '自摸', 2, 'seven pairs x2');
});

test('七对 — 有财神填单', () => {
  const hand = makeSevenPairs();
  hand[0] = t('honor','white'); // replace one with wild — need another to pair
  // Actually 七对 with wild: 7 pairs, wild fills a single
  const hand2 = [
    t('character',1),t('character',1),
    t('character',5),t('character',5),
    t('dot',2),t('dot',2),
    t('dot',8),t('dot',8),
    t('bamboo',3),t('bamboo',3),
    t('bamboo',6),
    t('honor','white'),
    t('honor','east'),t('honor','east'),
  ];
  assertFlags(hand2, '自摸', ['七对'], 'seven pairs with wild fill');
});

// ============================================================
// 3. 豪华七对
// ============================================================

test('豪华七对 — 七对中有4张相同', () => {
  assertFlags(makeLuxurySevenPairs(), '自摸', ['七对', '豪华七对'], 'luxury seven pairs');
});

test('豪华七对 — multiplier=4 (七对x2 * 豪华x2)', () => {
  assertMultiplier(makeLuxurySevenPairs(), '自摸', 4, 'luxury seven pairs x4');
});

// ============================================================
// 8. 十三幺
// ============================================================

test('十三幺 — 13种幺九各一+1对', () => {
  const hand = [
    t('character',1),t('character',9),
    t('dot',1),t('dot',9),
    t('bamboo',1),t('bamboo',9),
    t('honor','east'),t('honor','south'),t('honor','west'),t('honor','north'),
    t('honor','red'),t('honor','green'),
    t('honor','white'), // 白板是幺九之一也是财神
    t('honor','east'),  // 东对子
  ];
  // TODO: 十三幺需要特殊判定——不仅是牌型对，还要能胡。15张不行，需要恰好14张。
  // 十三幺需要 isShiSanYao 返回 true，但 isWinningHand 也需要返回 true。
  // 这里仅测试 flag 检测。十三幺不在当前 isStandardWin/isSevenPairs 范畴。
  // 实际：十三幺是独立胡牌型，当前引擎 isWinningHand 只检测平胡/七对。
  const result = calculateWinScore({
    hand, wildKey: WILD_KEY, kind: '自摸',
    winningTile: null, piaoCount: 0, isDealer: false,
    isFirstDraw: false, isFirstTurn: false,
  });
  assert(result.flags.includes('十三幺'), `十三幺 detection, got [${result.flags.join(', ')}]`);
});

// ============================================================
// 9. 杠上开花
// ============================================================

test('杠上开花 — kind="杠开"', () => {
  const result = calculateWinScore({
    hand: makeStdWin(), wildKey: WILD_KEY, kind: '杠开',
    winningTile: null, piaoCount: 0, isDealer: false,
    isFirstDraw: false, isFirstTurn: false,
  });
  assert(result.flags.includes('杠上开花'), `杠上开花 flag, got [${result.flags.join(', ')}]`);
  assert(result.multiplier === 2, `杠上开花 multiplier should be 2, got ${result.multiplier}`);
});

// ============================================================
// 10. 爆头
// ============================================================

test('爆头 — 1财神+听牌后摸到胡牌', () => {
  // 13张听牌(含1财神) + 摸到9条 = 14张胡牌
  const withoutWinning = [
    t('character',1),t('character',2),t('character',3),
    t('character',4),t('character',5),t('character',6),
    t('dot',2),t('dot',3),t('dot',4),
    t('bamboo',7),t('bamboo',8),
    t('honor','white'),
    t('honor','east'), // pairs with wild
  ];
  const winningTile = t('bamboo',9);
  const hand = [...withoutWinning, winningTile];
  assertFlags(hand, '自摸', ['平胡', '爆头'], 'bao tou', { winningTile });
});

test('爆头 — multiplier=2', () => {
  const withoutWinning = [
    t('character',1),t('character',2),t('character',3),
    t('character',4),t('character',5),t('character',6),
    t('dot',2),t('dot',3),t('dot',4),
    t('bamboo',7),t('bamboo',8),
    t('honor','white'),
    t('honor','east'),
  ];
  const winningTile = t('bamboo',9);
  const hand = [...withoutWinning, winningTile];
  assertMultiplier(hand, '自摸', 2, 'bao tou x2', { winningTile });
});

// ============================================================
// 11. 天胡
// ============================================================

test('天胡 — 庄家起手胡', () => {
  const result = calculateWinScore({
    hand: makeStdWin(), wildKey: WILD_KEY, kind: '自摸',
    winningTile: null, piaoCount: 0,
    isDealer: true, isFirstTurn: true, isFirstDraw: false,
  });
  assert(result.flags.includes('天胡'), `天胡 flag, got [${result.flags.join(', ')}]`);
});

test('天胡 — 非庄家不触发', () => {
  const result = calculateWinScore({
    hand: makeStdWin(), wildKey: WILD_KEY, kind: '自摸',
    winningTile: null, piaoCount: 0,
    isDealer: false, isFirstTurn: true, isFirstDraw: false,
  });
  assert(!result.flags.includes('天胡'), `天胡 should not fire for non-dealer`);
});

// ============================================================
// 12. 地胡
// ============================================================

test('地胡 — 闲家第一次摸牌胡', () => {
  const result = calculateWinScore({
    hand: makeStdWin(), wildKey: WILD_KEY, kind: '自摸',
    winningTile: null, piaoCount: 0,
    isDealer: false, isFirstDraw: true, isFirstTurn: false,
  });
  assert(result.flags.includes('地胡'), `地胡 flag, got [${result.flags.join(', ')}]`);
});

test('地胡 — 庄家不触发', () => {
  const result = calculateWinScore({
    hand: makeStdWin(), wildKey: WILD_KEY, kind: '自摸',
    winningTile: null, piaoCount: 0,
    isDealer: true, isFirstDraw: true, isFirstTurn: false,
  });
  assert(!result.flags.includes('地胡'), `地胡 should not fire for dealer`);
});

// ============================================================
// 13. 大四喜
// ============================================================

test('大四喜 — 四风各成刻子', () => {
  const hand = [
    t('honor','east'),t('honor','east'),t('honor','east'),
    t('honor','south'),t('honor','south'),t('honor','south'),
    t('honor','west'),t('honor','west'),t('honor','west'),
    t('honor','north'),t('honor','north'),t('honor','north'),
    t('character',1),t('character',1),
  ];
  assertFlags(hand, '自摸', ['平胡', '大四喜'], 'da si xi');
});

test('大四喜 — 有财神辅助完成', () => {
  // 14 tiles: 3east,2south,2west,2north, 2pair, 3white(wild)
  // 3 wilds assist: south+1w=triplet, west+1w=triplet, north+1w=triplet, east=nat triplet
  const hand = [
    t('honor','east'),t('honor','east'),t('honor','east'),
    t('honor','south'),t('honor','south'),
    t('honor','west'),t('honor','west'),
    t('honor','north'),t('honor','north'),
    t('character',1),t('character',1),
    t('honor','white'),t('honor','white'),t('honor','white'),
  ];
  const result = calculateWinScore({
    hand, wildKey: WILD_KEY, kind: '自摸',
    winningTile: null, piaoCount: 0, isDealer: false,
    isFirstDraw: false, isFirstTurn: false,
  });
  assert(result.flags.includes('大四喜'), `大四喜 with wilds, got [${result.flags.join(', ')}]`);
});

// ============================================================
// 14. 小四喜
// ============================================================

test('小四喜 — 三门风刻+一对风', () => {
  const hand = [
    t('honor','east'),t('honor','east'),t('honor','east'),
    t('honor','south'),t('honor','south'),t('honor','south'),
    t('honor','west'),t('honor','west'),t('honor','west'),
    t('honor','north'),t('honor','north'),
    t('character',1),t('character',1),t('character',1),
  ];
  assertFlags(hand, '自摸', ['平胡', '小四喜'], 'xiao si xi');
});

// ============================================================
// 15. 大三元
// ============================================================

test('大三元 — 中发白各成刻子', () => {
  // 由于白板是财神，需要4张白板(1张当wild填充，3张自然组成刻子)
  // 或者用非白板wild。但wildKey总=honor:white。
  // 构造：3红+3绿+3白(但白=wild) → 需要考虑wild从counts中排除
  // 实际：白板在counts中不计入（全部当wild），所以大三元需要3红+3绿+足够wild填3白
  const hand = [
    t('honor','red'),t('honor','red'),t('honor','red'),
    t('honor','green'),t('honor','green'),t('honor','green'),
    t('honor','white'),t('honor','white'),t('honor','white'),
    t('character',1),t('character',2),t('character',3),
    t('character',5),t('character',5),
  ];
  // wildKey=honor:white, so 3 whites are all wilds (wildCount=3).
  // White counts as 0 in counts map. countDragonSets needs to form triplets for all 3 dragons.
  // white triplet: 0 natural + 3 wild = triplet. red: 3 natural. green: 3 natural.
  // Pair: 5万5万.
  const result = calculateWinScore({
    hand, wildKey: WILD_KEY, kind: '自摸',
    winningTile: null, piaoCount: 0, isDealer: false,
    isFirstDraw: false, isFirstTurn: false,
  });
  assert(result.flags.includes('大三元'), `大三元 with wilds as white dragon, got [${result.flags.join(', ')}]`);
});

// ============================================================
// 16. 小三元
// ============================================================

test('小三元 — 二门箭刻+一对箭', () => {
  const hand = [
    t('honor','red'),t('honor','red'),t('honor','red'),
    t('honor','green'),t('honor','green'),t('honor','green'),
    t('honor','white'),t('honor','white'), // 2 wilds = white pair
    t('character',1),t('character',2),t('character',3),
    t('character',4),t('character',5),t('character',6),
  ];
  const result = calculateWinScore({
    hand, wildKey: WILD_KEY, kind: '自摸',
    winningTile: null, piaoCount: 0, isDealer: false,
    isFirstDraw: false, isFirstTurn: false,
  });
  assert(result.flags.includes('小三元'), `小三元, got [${result.flags.join(', ')}]`);
});

// ============================================================
// 牌型叠加测试
// ============================================================


// ============================================================
// 飘财倍数测试
// ============================================================

test('飘财 — 一飘 x2', () => {
  const result = calculateWinScore({
    hand: makeStdWin(), wildKey: WILD_KEY, kind: '飘财',
    winningTile: null, piaoCount: 1, isDealer: false,
    isFirstDraw: false, isFirstTurn: false,
  });
  assert(result.multiplier === 2, `一飘 should be x2, got ${result.multiplier}`);
});

test('飘财 — 二飘 x4', () => {
  const result = calculateWinScore({
    hand: makeStdWin(), wildKey: WILD_KEY, kind: '飘财',
    winningTile: null, piaoCount: 2, isDealer: false,
    isFirstDraw: false, isFirstTurn: false,
  });
  assert(result.multiplier === 4, `二飘 should be x4, got ${result.multiplier}`);
});

test('飘财 — 三飘 x8', () => {
  const result = calculateWinScore({
    hand: makeStdWin(), wildKey: WILD_KEY, kind: '飘财',
    winningTile: null, piaoCount: 3, isDealer: false,
    isFirstDraw: false, isFirstTurn: false,
  });
  assert(result.multiplier === 8, `三飘 should be x8, got ${result.multiplier}`);
});

test('飘财叠加 — 一飘+爆头 = x4', () => {
  const withoutWinning = [
    t('character',1),t('character',2),t('character',3),
    t('character',4),t('character',5),t('character',6),
    t('dot',2),t('dot',3),t('dot',4),
    t('bamboo',7),t('bamboo',8),
    t('honor','white'),
    t('honor','east'), // pairs with wild
  ];
  const winningTile = t('bamboo',9);
  const hand = [...withoutWinning, winningTile];
  const result = calculateWinScore({
    hand, wildKey: WILD_KEY, kind: '飘财',
    winningTile, piaoCount: 1, isDealer: false,
    isFirstDraw: false, isFirstTurn: false,
  });
  assert(result.multiplier === 4, `一飘+爆头 should be 1*2*2=4, got ${result.multiplier}`);
  assert(result.flags.includes('爆头'), `should include 爆头`);
});

// ============================================================
// 边缘/异常测试
// ============================================================

test('异常 — 空手牌返回0倍率', () => {
  const result = calculateWinScore({
    hand: [], wildKey: WILD_KEY, kind: '自摸',
    winningTile: null, piaoCount: 0, isDealer: false,
    isFirstDraw: false, isFirstTurn: false,
  });
  assert(result.multiplier === 0, 'empty hand should give 0');
  assert(result.flags.length === 0, 'empty hand should have no flags');
});

test('异常 — 非14张返回0倍率', () => {
  const result = calculateWinScore({
    hand: [t('character',1)], wildKey: WILD_KEY, kind: '自摸',
    winningTile: null, piaoCount: 0, isDealer: false,
    isFirstDraw: false, isFirstTurn: false,
  });
  assert(result.multiplier === 0, 'non-14 hand should give 0');
});

test('自摸 — isSelfDraw=true', () => {
  const result = calculateWinScore({
    hand: makeStdWin(), wildKey: WILD_KEY, kind: '自摸',
    winningTile: null, piaoCount: 0, isDealer: false,
    isFirstDraw: false, isFirstTurn: false,
  });
  assert(result.isSelfDraw === true, '自摸 should be self-draw');
});

test('点炮 — isSelfDraw=false', () => {
  const result = calculateWinScore({
    hand: makeStdWin(), wildKey: WILD_KEY, kind: '点炮',
    winningTile: null, piaoCount: 0, isDealer: false,
    isFirstDraw: false, isFirstTurn: false,
  });
  assert(result.isSelfDraw === false, '点炮 should not be self-draw');
});

test('大四喜不触发小四喜', () => {
  const hand = [
    t('honor','east'),t('honor','east'),t('honor','east'),
    t('honor','south'),t('honor','south'),t('honor','south'),
    t('honor','west'),t('honor','west'),t('honor','west'),
    t('honor','north'),t('honor','north'),t('honor','north'),
    t('character',1),t('character',1),
  ];
  const result = calculateWinScore({
    hand, wildKey: WILD_KEY, kind: '自摸',
    winningTile: null, piaoCount: 0, isDealer: false,
    isFirstDraw: false, isFirstTurn: false,
  });
  assert(result.flags.includes('大四喜'), 'should have 大四喜');
  assert(!result.flags.includes('小四喜'), 'should NOT have 小四喜');
});

// ============================================================

console.log(`\n${passed} passed, ${failed} failed`);
if (failed > 0) process.exitCode = 1;
