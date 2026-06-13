// ============================================================
// search-interpreter.js — ISMCTS search tree interpreter
// ============================================================
// Extracts structured insights from ISMCTS search stats and
// opponent belief models. Produces data objects that nl-explainer
// converts to natural language coach messages.
// ============================================================

import { indexToTileType, computeShanten, shantenAfterDiscard } from '../ai/game-sim.js';

// ----------------------------------------------------------
// Constants
// ----------------------------------------------------------

/** Threat detection thresholds */
export const THREAT_THRESHOLD = {
  POSSIBLE: 0.60,   // 可能听牌
  LIKELY: 0.85,     // 大概率听牌
};

/** Direction divergence: win rate difference below this = fork */
const DIVERGENCE_WIN_RATE_GAP = 0.05;

/** Minimum visits for a search path to be considered reliable */
const MIN_VISITS_RELIABLE = 50;

/** Variance threshold: above this the result is "仅供参考" */
const HIGH_VARIANCE_RATIO = 0.15;

/** Default TOP-K for extraction */
const DEFAULT_TOP_K = 5;

// ----------------------------------------------------------
// Helper: tile label
// ----------------------------------------------------------

const SUIT_NAMES = { character: '万', dot: '筒', bamboo: '条', honor: '' };
const CHINESE = ['一','二','三','四','五','六','七','八','九'];
const HONOR_LABEL = { east: '东', south: '南', west: '西', north: '北', red: '红', green: '发', white: '白' };

function tileIdxToLabel(idx) {
  const t = indexToTileType(idx);
  if (t.suit === 'honor') return HONOR_LABEL[t.rank] || t.rank;
  return CHINESE[t.rank - 1] + SUIT_NAMES[t.suit];
}

// ----------------------------------------------------------
// 1. TOP-K Extraction
// ----------------------------------------------------------

/**
 * Extract top-K discard candidates from ISMCTS search stats.
 *
 * @param {Map|Array|null} stats — ISMCTS stats map or [key, {visits,winRate,value}] entries
 * @param {number} [k=5] — number of top candidates to return
 * @param {number} [playerDiscardIdx] — if provided, ensure this tile is included even outside top-K
 * @returns {{
 *   candidates: Array<{tileIdx:number, label:string, visits:number, winRate:number, value:number, rank:number}>,
 *   totalVisits: number,
 *   topWinRate: number,
 *   topVisits: number,
 * }}
 */
export function extractTopK(stats, k = DEFAULT_TOP_K, playerDiscardIdx = -1) {
  if (!stats) {
    return { candidates: [], totalVisits: 0, topWinRate: 0, topVisits: 0 };
  }

  const entries = stats instanceof Map ? Array.from(stats.entries()) : stats;
  const discards = [];

  for (const [key, data] of entries) {
    if (!key.startsWith('discard:')) continue;
    const tileIdx = parseInt(key.split(':')[1], 10);
    discards.push({
      tileIdx,
      label: tileIdxToLabel(tileIdx),
      visits: data.visits || 0,
      winRate: data.winRate || 0,
      value: data.value || 0,
    });
  }

  // Sort by winRate descending, then visits descending
  discards.sort((a, b) => b.winRate - a.winRate || b.visits - a.visits);

  const totalVisits = discards.reduce((s, d) => s + d.visits, 0);

  // Pick top K
  const candidates = discards.slice(0, k).map((d, i) => ({ ...d, rank: i + 1 }));

  // Ensure player's discard is included if specified and not already in list
  if (playerDiscardIdx >= 0 && !candidates.some(c => c.tileIdx === playerDiscardIdx)) {
    const picked = discards.find(d => d.tileIdx === playerDiscardIdx);
    if (picked) {
      picked.rank = discards.indexOf(picked) + 1;
      candidates.push(picked);
    }
  }

  return {
    candidates,
    totalVisits,
    topWinRate: discards.length > 0 ? discards[0].winRate : 0,
    topVisits: discards.length > 0 ? discards[0].visits : 0,
  };
}

// ----------------------------------------------------------
// 2. Threat Detection
// ----------------------------------------------------------

/**
 * Result of threat detection for a single opponent.
 * @typedef {{
 *   playerId: number,
 *   tenpaiProb: number,
 *   threatLevel: 'none'|'possible'|'likely',
 *   dangerBreakdown: Array<{tileIdx:number, label:string, danger:number}>,
 * }} ThreatResult
 */

/**
 * Detect threats from opponent belief model.
 *
 * @param {import('../ai/opponent-model.js').MultiPlayerModel|null} oppModel
 * @param {object} [options]
 * @param {number} [options.observerId=0] — observer's player ID (excluded from threat detection)
 * @param {number[]} [options.candidateTileIndices] — candidate discard tiles to check danger for
 * @returns {{
 *   threats: ThreatResult[],
 *   highestThreat: ThreatResult|null,
 *   anyThreatLikely: boolean,
 *   anyThreatPossible: boolean,
 * }}
 */
export function detectThreats(oppModel, options = {}) {
  const { observerId = 0, candidateTileIndices = [] } = options;

  if (!oppModel) {
    return { threats: [], highestThreat: null, anyThreatLikely: false, anyThreatPossible: false };
  }

  const threats = [];

  for (let p = 0; p < 4; p++) {
    if (p === observerId) continue;

    const model = oppModel.getModel(p);
    if (!model) continue;

    const tenpaiProb = model.tenpaiProbability;
    let threatLevel = 'none';
    if (tenpaiProb >= THREAT_THRESHOLD.LIKELY) threatLevel = 'likely';
    else if (tenpaiProb >= THREAT_THRESHOLD.POSSIBLE) threatLevel = 'possible';

    // Danger breakdown for candidate tiles
    const dangerBreakdown = [];
    for (const tileIdx of candidateTileIndices) {
      dangerBreakdown.push({
        tileIdx,
        label: tileIdxToLabel(tileIdx),
        danger: model.getTileDanger(tileIdx),
      });
    }
    dangerBreakdown.sort((a, b) => b.danger - a.danger);

    threats.push({
      playerId: p,
      tenpaiProb,
      threatLevel,
      dangerBreakdown,
    });
  }

  // Sort by tenpai probability descending
  threats.sort((a, b) => b.tenpaiProb - a.tenpaiProb);

  const highestThreat = threats.length > 0 ? threats[0] : null;
  const anyThreatLikely = threats.some(t => t.threatLevel === 'likely');
  const anyThreatPossible = threats.some(t => t.threatLevel === 'possible' || t.threatLevel === 'likely');

  return { threats, highestThreat, anyThreatLikely, anyThreatPossible };
}

// ----------------------------------------------------------
// 2b. Safe Tile Analysis
// ----------------------------------------------------------

/**
 * Analyze which candidate discards are dangerous to which opponents.
 *
 * @param {import('../ai/opponent-model.js').MultiPlayerModel|null} oppModel
 * @param {Array<{tileIdx:number}>} candidates — candidate actions from TOP-K
 * @param {number} [observerId=0]
 * @returns {{
 *   perCandidate: Array<{
 *     tileIdx: number,
 *     label: string,
 *     maxDanger: number,
 *     dangerousTo: number[],
 *     isSafe: boolean,
 *   }>,
 *   safestCandidate: {tileIdx:number, label:string}|null,
 *   riskiestCandidate: {tileIdx:number, label:string}|null,
 * }}
 */
export function analyzeSafeTiles(oppModel, candidates, observerId = 0) {
  if (!oppModel || !candidates || candidates.length === 0) {
    return { perCandidate: [], safestCandidate: null, riskiestCandidate: null };
  }

  const perCandidate = candidates.map(c => {
    const tileIdx = c.tileIdx;
    let maxDanger = 0;
    const dangerousTo = [];

    for (let p = 0; p < 4; p++) {
      if (p === observerId) continue;
      const danger = oppModel.getTileDanger(p, tileIdx);
      if (danger > maxDanger) maxDanger = danger;
      // A tile is "dangerous to" this opponent if danger > 0.5
      if (danger > 0.5) dangerousTo.push(p);
    }

    return {
      tileIdx,
      label: tileIdxToLabel(tileIdx),
      maxDanger,
      dangerousTo,
      isSafe: maxDanger <= 0.4,
    };
  });

  perCandidate.sort((a, b) => a.maxDanger - b.maxDanger);

  return {
    perCandidate,
    safestCandidate: perCandidate.length > 0
      ? { tileIdx: perCandidate[0].tileIdx, label: perCandidate[0].label }
      : null,
    riskiestCandidate: perCandidate.length > 0
      ? { tileIdx: perCandidate[perCandidate.length - 1].tileIdx, label: perCandidate[perCandidate.length - 1].label }
      : null,
  };
}

// ----------------------------------------------------------
// 3. Direction Divergence Detection
// ----------------------------------------------------------

/**
 * Result of direction divergence detection.
 * @typedef {{
 *   isFork: boolean,
 *   pathA: {tileIdx:number, label:string, winRate:number, visits:number, handType:string}|null,
 *   pathB: {tileIdx:number, label:string, winRate:number, visits:number, handType:string}|null,
 *   winRateGap: number,
 *   description: string,
 * }} DivergenceResult
 */

/**
 * Detect if there are two similarly-ranked paths that lead to different
 * hand directions (strategic fork).
 *
 * @param {Map|Array|null} stats — ISMCTS stats
 * @param {object} handInfo — { shanten, waits, wildCount, meldCount }
 * @returns {DivergenceResult}
 */
export function detectDirectionDivergence(stats, handInfo = {}) {
  const { candidates } = extractTopK(stats, 5);

  if (candidates.length < 2) {
    return { isFork: false, pathA: null, pathB: null, winRateGap: 0, description: '' };
  }

  // Check consecutive pairs for close win rates
  for (let i = 0; i < candidates.length - 1; i++) {
    for (let j = i + 1; j < candidates.length; j++) {
      const gap = Math.abs(candidates[i].winRate - candidates[j].winRate);
      if (gap < DIVERGENCE_WIN_RATE_GAP && candidates[i].visits >= MIN_VISITS_RELIABLE) {
        const handTypeA = classifyDiscardDirection(candidates[i].tileIdx, handInfo);
        const handTypeB = classifyDiscardDirection(candidates[j].tileIdx, handInfo);

        // Only report as fork if hand types differ
        if (handTypeA !== handTypeB) {
          return {
            isFork: true,
            pathA: { ...candidates[i], handType: handTypeA },
            pathB: { ...candidates[j], handType: handTypeB },
            winRateGap: gap,
            description: `${candidates[i].label}(${handTypeA}) vs ${candidates[j].label}(${handTypeB})，胜率仅差 ${(gap * 100).toFixed(1)}%`,
          };
        }
      }
    }
  }

  return { isFork: false, pathA: null, pathB: null, winRateGap: 0, description: '' };
}

/**
 * Classify what hand direction a discard suggests.
 * Heuristic based on tile position and hand context.
 */
function classifyDiscardDirection(tileIdx, handInfo) {
  if (handInfo.shanten === 0) return '听牌调整';

  // Honor tiles → likely fast hand (平胡)
  if (tileIdx >= 27) return '弃字牌';

  const rank = tileIdx % 9 + 1;

  // Terminals (1,9) → breaking edge sequences
  if (rank === 1 || rank === 9) return '弃边张';

  // Middle tiles (3-7) → restructuring hand, possibly going for bigger hand
  if (rank >= 3 && rank <= 7) {
    if (handInfo.shanten !== undefined && handInfo.shanten <= 1) {
      return '调整搭子';
    }
    return '弃中张';
  }

  return '弃牌';
}

// ----------------------------------------------------------
// 4. Big Hand vs Fast Hand Detection
// ----------------------------------------------------------

/**
 * Detect divergence between "big hand" (high multiplier) and
 * "fast hand" (quick win) strategies.
 *
 * @param {Map|Array|null} stats — ISMCTS stats
 * @param {object} handInfo — { shanten, waits, wildCount, canPiaoCai, hasGangOpportunity }
 * @returns {{
 *   hasDivergence: boolean,
 *   bigHandPath: {tileIdx:number, label:string, winRate:number, estimatedMultiplier:number}|null,
 *   fastHandPath: {tileIdx:number, label:string, winRate:number, estimatedMultiplier:number}|null,
 *   expectedScoreDiff: number,
 *   recommendation: string,
 * }}
 */
export function detectBigVsFast(stats, handInfo = {}) {
  const { candidates } = extractTopK(stats, 5);

  if (candidates.length < 2) {
    return { hasDivergence: false, bigHandPath: null, fastHandPath: null, expectedScoreDiff: 0, recommendation: '' };
  }

  let bigHandPath = null;
  let fastHandPath = null;
  let maxDiff = 0;

  // Heuristic: scan candidates for ones that suggest big hand vs fast hand
  // Big hand indicators: wild tiles in hand (piao cai potential), gang opportunities
  // Fast hand indicators: low shanten, discarding isolated honors

  for (const c of candidates) {
    const isBig = classifyAsBigHandDiscard(c.tileIdx, handInfo);
    const isFast = classifyAsFastDiscard(c.tileIdx, handInfo);

    if (isBig && (!bigHandPath || c.winRate > bigHandPath.winRate)) {
      bigHandPath = {
        tileIdx: c.tileIdx,
        label: c.label,
        winRate: c.winRate,
        estimatedMultiplier: estimateMultiplier(handInfo, true),
      };
    }

    if (isFast && (!fastHandPath || c.winRate > fastHandPath.winRate)) {
      fastHandPath = {
        tileIdx: c.tileIdx,
        label: c.label,
        winRate: c.winRate,
        estimatedMultiplier: estimateMultiplier(handInfo, false),
      };
    }
  }

  // Fallback: if one category is empty, use the highest and lowest win rate candidates
  if (!bigHandPath && candidates.length >= 2) {
    const c = candidates[candidates.length - 1]; // lowest win rate = bigger hand (slower)
    bigHandPath = {
      tileIdx: c.tileIdx,
      label: c.label,
      winRate: c.winRate,
      estimatedMultiplier: estimateMultiplier(handInfo, true),
    };
  }
  if (!fastHandPath && candidates.length >= 2) {
    const c = candidates[0]; // highest win rate = faster hand
    fastHandPath = {
      tileIdx: c.tileIdx,
      label: c.label,
      winRate: c.winRate,
      estimatedMultiplier: estimateMultiplier(handInfo, false),
    };
  }

  if (!bigHandPath || !fastHandPath) {
    return { hasDivergence: false, bigHandPath: null, fastHandPath: null, expectedScoreDiff: 0, recommendation: '' };
  }

  // Compare expected scores: winRate * estimatedMultiplier
  const bigScore = bigHandPath.winRate * bigHandPath.estimatedMultiplier;
  const fastScore = fastHandPath.winRate * fastHandPath.estimatedMultiplier;
  const scoreDiff = bigScore - fastScore;

  let recommendation = '';
  if (Math.abs(scoreDiff) < 0.05) {
    recommendation = '两条路线期望得分相近，可根据牌墙剩余和对手状态决定。';
  } else if (scoreDiff > 0) {
    recommendation = '大牌路线期望得分更高，但胜率略低。若对手无威胁可尝试。';
  } else {
    recommendation = '速听路线期望得分更高，建议优先胡牌。';
  }

  return {
    hasDivergence: Math.abs(bigHandPath.winRate - fastHandPath.winRate) > DIVERGENCE_WIN_RATE_GAP,
    bigHandPath,
    fastHandPath,
    expectedScoreDiff: scoreDiff,
    recommendation,
  };
}

function classifyAsBigHandDiscard(tileIdx, handInfo) {
  // Discarding middle tiles (3-7) when holding wild tiles → restructuring for points
  if (handInfo.wildCount >= 2 && tileIdx >= 0 && tileIdx < 27) {
    const rank = tileIdx % 9 + 1;
    if (rank >= 3 && rank <= 7) return true;
  }
  return false;
}

function classifyAsFastDiscard(tileIdx, handInfo) {
  // Honor tiles (27-33): discarding isolated honors → fast hand
  if (tileIdx >= 27) return true;
  // Terminal tiles (1,9): discarding edge tiles → fast hand
  const rank = tileIdx % 9 + 1;
  if (rank === 1 || rank === 9) return true;
  // Low shanten with no wild: any discard is fast play
  if (handInfo.shanten !== undefined && handInfo.shanten <= 1 && handInfo.wildCount === 0) return true;
  return false;
}

function estimateMultiplier(handInfo, isBigHand) {
  let mult = 1;
  if (isBigHand) {
    if (handInfo.canPiaoCai) mult *= 2;
    if (handInfo.wildCount >= 2) mult *= 1.5;
    if (handInfo.hasGangOpportunity) mult *= 1.3;
  } else {
    if (handInfo.wildCount >= 1) mult *= 1.2;
  }
  return Math.round(mult * 10) / 10;
}

// ----------------------------------------------------------
// 5. PiaoCai Window Assessment
// ----------------------------------------------------------

/**
 * Assess whether now is a good window to declare piao cai.
 * Compares expected score from piao cai vs direct win.
 *
 * @param {object} options
 * @param {number} options.shanten — current shanten number
 * @param {number} options.wildCount — wild tiles in hand
 * @param {number} [options.poolRemaining=0] — tiles remaining in wall
 * @param {boolean} [options.isDealer=false]
 * @param {number} [options.currentRound=0] — round number for context
 * @returns {{
 *   shouldConsider: boolean,
 *   piaoCaiExpectedScore: number,
 *   directWinExpectedScore: number,
 *   scoreAdvantage: number,
 *   riskLevel: 'low'|'medium'|'high',
 *   reasoning: string,
 * }}
 */
export function assessPiaoCaiWindow(options = {}) {
  const {
    shanten = 99,
    wildCount = 0,
    poolRemaining = 56,
    isDealer = false,
  } = options;

  // Cannot piao cai without at least 2 wild tiles
  if (wildCount < 2) {
    return {
      shouldConsider: false,
      piaoCaiExpectedScore: 0,
      directWinExpectedScore: 0,
      scoreAdvantage: 0,
      riskLevel: 'high',
      reasoning: '需要至少 2 张财神才能飘财。',
    };
  }

  // Cannot piao cai if already tenpai (can't win after piao cai if already ready)
  if (shanten < 0) {
    return {
      shouldConsider: false,
      piaoCaiExpectedScore: 0,
      directWinExpectedScore: 0,
      scoreAdvantage: 0,
      riskLevel: 'high',
      reasoning: '已能胡牌，建议直接胡牌而非飘财。',
    };
  }

  // Estimate: shanten-based probability of winning before wall exhausts
  const turnsRemaining = Math.floor(poolRemaining / 4);
  const winProb = estimateWinProbability(shanten, turnsRemaining, wildCount);

  // Direct win: base score 1, with wild boost (爆头)
  const directMultiplier = wildCount >= 1 ? 2 : 1; // 爆头 = 2x
  const directWinScore = winProb * directMultiplier * (isDealer ? 1.5 : 1);

  // Piao cai: multiplier doubles but win probability decreases
  // (can't use wild as joker during piao cai)
  const piaoCaiWinProb = estimateWinProbability(shanten + 1, turnsRemaining, wildCount);
  const piaoCaiMultiplier = 2; // piao cai = 2x base
  const piaoCaiScore = piaoCaiWinProb * piaoCaiMultiplier * (isDealer ? 1.5 : 1);

  const scoreAdvantage = piaoCaiScore - directWinScore;

  let riskLevel = 'medium';
  let reasoning = '';

  if (scoreAdvantage > 0.3) {
    riskLevel = 'low';
    reasoning = `飘财期望得分 ${piaoCaiScore.toFixed(2)}，高于直接胡 ${directWinScore.toFixed(2)}。牌墙剩余 ${poolRemaining} 张，机会充足。`;
  } else if (scoreAdvantage > 0) {
    riskLevel = 'medium';
    reasoning = `飘财期望得分 ${piaoCaiScore.toFixed(2)} 略高于直接胡 ${directWinScore.toFixed(2)}。可考虑但需谨慎。`;
  } else if (shanten <= 1 && wildCount >= 2) {
    riskLevel = 'medium';
    reasoning = `飘财期望得分 ${piaoCaiScore.toFixed(2)} 低于直接胡 ${directWinScore.toFixed(2)}，但向听数低，仍有机会。`;
  } else {
    riskLevel = 'high';
    reasoning = `飘财期望得分 ${piaoCaiScore.toFixed(2)} 明显低于直接胡 ${directWinScore.toFixed(2)}。建议优先胡牌。`;
  }

  return {
    shouldConsider: wildCount >= 2 && shanten >= 0,
    piaoCaiExpectedScore: piaoCaiScore,
    directWinExpectedScore: directWinScore,
    scoreAdvantage,
    riskLevel,
    reasoning,
  };
}

/**
 * Estimate probability of winning given shanten and turns remaining.
 * Simple heuristic model — more accurate in actual ISMCTS simulation.
 */
function estimateWinProbability(shanten, turnsRemaining, wildCount) {
  if (shanten < 0) return 1.0;
  if (shanten > 5) return 0.01;
  if (turnsRemaining <= 0) return 0;

  // Each shanten step requires roughly 2-3 turns on average
  // Modify by wild count (each wild = 1 free step)
  const effectiveShanten = Math.max(0, shanten - Math.floor(wildCount / 2));
  const turnsNeeded = effectiveShanten * 2.5;

  if (turnsRemaining < turnsNeeded) {
    return Math.max(0.05, turnsRemaining / (turnsNeeded * 2));
  }

  // Sigmoid-like: high probability if we have enough turns
  const ratio = turnsRemaining / turnsNeeded;
  return Math.min(0.95, 0.3 + 0.65 * (1 - Math.exp(-ratio)));
}

// ----------------------------------------------------------
// 6. Search Confidence Assessment
// ----------------------------------------------------------

/**
 * Assess the reliability of ISMCTS search results.
 *
 * @param {Map|Array|null} stats — ISMCTS stats
 * @returns {{
 *   isReliable: boolean,
 *   confidenceLevel: 'high'|'medium'|'low'|'insufficient',
 *   totalVisits: number,
 *   topWinRate: number,
 *   winRateVariance: number,
 *   visitConcentration: number,
 *   summary: string,
 * }}
 */
export function assessConfidence(stats) {
  if (!stats) {
    return {
      isReliable: false,
      confidenceLevel: 'insufficient',
      totalVisits: 0,
      topWinRate: 0,
      winRateVariance: 0,
      visitConcentration: 0,
      summary: '无搜索统计数据。',
    };
  }

  const entries = stats instanceof Map ? Array.from(stats.entries()) : stats;

  if (entries.length === 0) {
    return {
      isReliable: false,
      confidenceLevel: 'insufficient',
      totalVisits: 0,
      topWinRate: 0,
      winRateVariance: 0,
      visitConcentration: 0,
      summary: '搜索无结果。',
    };
  }

  const discards = [];
  let totalVisits = 0;

  for (const [key, data] of entries) {
    if (!key.startsWith('discard:')) continue;
    const visits = data.visits || 0;
    const winRate = data.winRate || 0;
    discards.push({ visits, winRate });
    totalVisits += visits;
  }

  if (discards.length === 0 || totalVisits === 0) {
    return {
      isReliable: false,
      confidenceLevel: 'insufficient',
      totalVisits: 0,
      topWinRate: 0,
      winRateVariance: 0,
      visitConcentration: 0,
      summary: '无有效的弃牌统计数据。',
    };
  }

  // Sort by win rate
  discards.sort((a, b) => b.winRate - a.winRate);

  const topWinRate = discards[0].winRate;

  // Compute win rate variance (weighted by visits)
  const meanWinRate = discards.reduce((s, d) => s + d.winRate * d.visits, 0) / totalVisits;
  let variance = 0;
  for (const d of discards) {
    variance += d.visits * (d.winRate - meanWinRate) ** 2;
  }
  variance /= totalVisits;

  // Visit concentration: what fraction of visits went to the top candidate?
  const topVisits = discards[0].visits;
  const visitConcentration = topVisits / totalVisits;

  // Determine confidence level
  let confidenceLevel;
  let isReliable;
  let summary;

  const minVisitsPerCandidate = totalVisits / discards.length;

  if (totalVisits < 50 || minVisitsPerCandidate < 5) {
    confidenceLevel = 'insufficient';
    isReliable = false;
    summary = `仅 ${totalVisits} 次模拟，数据严重不足，结果仅供参考。`;
  } else if (totalVisits < 200 || variance > HIGH_VARIANCE_RATIO) {
    confidenceLevel = 'low';
    isReliable = false;
    summary = `共 ${totalVisits} 次模拟，但结果方差较大（${(variance * 100).toFixed(1)}%），结论仅供参考。`;
  } else if (totalVisits < 500 || visitConcentration < 0.25) {
    confidenceLevel = 'medium';
    isReliable = true;
    summary = `共 ${totalVisits} 次模拟，统计量尚可，结论有一定参考价值。`;
  } else {
    confidenceLevel = 'high';
    isReliable = true;
    summary = `共 ${totalVisits} 次模拟，统计量充足，结论可靠。`;
  }

  return {
    isReliable,
    confidenceLevel,
    totalVisits,
    topWinRate,
    winRateVariance: variance,
    visitConcentration,
    summary,
  };
}

// ----------------------------------------------------------
// 7. Hand Structure Analysis
// ----------------------------------------------------------

const SUIT_START = [0, 9, 18]; // character, dot, bamboo

/**
 * Analyze the structure of a hand (TileSet format).
 *
 * @param {import('../ai/game-sim.js').TileSet} handSet
 * @param {number} wildIdx
 * @returns {{
 *   pairCount: number,
 *   tripletCount: number,
 *   partialMelds: Array<{type:string, tiles:number[], quality:string}>,
 *   completeSequences: number,
 *   sevenPairsDistance: number,
 *   suitBreakdown: Array<{suit:string, count:number, pairs:number}>,
 *   isolatedTiles: number[],
 *   edgeWaits: Array<{tiles:number[], desc:string}>,
 * }}
 */
export function analyzeHandStructure(handSet, wildIdx) {
  const d = handSet.data;
  const wildCount = d[wildIdx];

  // Count pairs, triplets, complete sequences
  let pairCount = 0;
  let tripletCount = 0;
  const pairTiles = [];
  const tripletTiles = [];

  for (let i = 0; i < 34; i++) {
    if (i === wildIdx) continue;
    if (d[i] >= 3) { tripletCount++; tripletTiles.push(i); }
    else if (d[i] === 2) { pairCount++; pairTiles.push(i); }
  }

  // Count effective pairs for seven pairs (including wilds)
  let effectivePairs = pairCount + tripletCount; // triplets can be split into pair + single
  let remainingWilds = wildCount;
  // Wild tiles can each pair with a single tile, or two wilds form a pair
  const singles = [];
  for (let i = 0; i < 34; i++) {
    if (i === wildIdx) continue;
    if (d[i] === 1 || d[i] === 4) singles.push(i); // count 4 as having singles after pair
    if (d[i] === 4) singles.push(i); // two extra singles
  }
  const singleCount = singles.length + (tripletCount > 0 ? tripletCount : 0); // triplets contribute a single
  while (remainingWilds > 0 && singleCount > effectivePairs * 0) {
    effectivePairs++;
    remainingWilds--;
  }
  if (remainingWilds >= 2) {
    effectivePairs += Math.floor(remainingWilds / 2);
  }
  const sevenPairsDistance = Math.max(0, 7 - effectivePairs);

  // Find partial melds (搭子)
  const partialMelds = [];
  const usedInMeld = new Set();

  for (const suitBase of SUIT_START) {
    for (let rank = 1; rank <= 9; rank++) {
      const i = suitBase + rank - 1;
      if (i === wildIdx) continue;
      if (d[i] <= 0) continue;

      // Check two-sided wait: N, N+1
      if (rank < 9 && d[i + 1] > 0 && !usedInMeld.has(i) && !usedInMeld.has(i + 1)) {
        const quality = (rank >= 2 && rank <= 7) ? '双面搭' : '边搭';
        partialMelds.push({ type: 'sequence_wait', tiles: [i, i + 1], quality });
        usedInMeld.add(i);
        usedInMeld.add(i + 1);
      }
      // Check gap wait: N, N+2
      if (rank <= 7 && d[i + 2] > 0 && !usedInMeld.has(i) && !usedInMeld.has(i + 2)) {
        partialMelds.push({ type: 'gap_wait', tiles: [i, i + 2], quality: '嵌搭' });
        usedInMeld.add(i);
        usedInMeld.add(i + 2);
      }
    }
  }

  // Find complete sequences
  let completeSequences = 0;
  for (const suitBase of SUIT_START) {
    for (let rank = 1; rank <= 7; rank++) {
      const i = suitBase + rank - 1;
      if (d[i] > 0 && d[i + 1] > 0 && d[i + 2] > 0) {
        completeSequences++;
      }
    }
  }

  // Find isolated tiles
  const isolatedTiles = [];
  for (let i = 0; i < 34; i++) {
    if (i === wildIdx || d[i] <= 0) continue;
    if (usedInMeld.has(i)) continue;
    if (d[i] >= 2) continue; // part of pair or triplet

    // Check if any adjacent tile in same suit
    let hasNeighbor = false;
    const suitBase = Math.floor(i / 9) * 9;
    if (i > suitBase && d[i - 1] > 0) hasNeighbor = true;
    if (i < suitBase + 8 && d[i + 1] > 0) hasNeighbor = true;

    if (!hasNeighbor) {
      isolatedTiles.push(i);
    }
  }

  // Suit breakdown
  const suitNames = ['character', 'dot', 'bamboo', 'honor'];
  const suitBreakdown = [];
  for (let s = 0; s < 4; s++) {
    const start = s === 3 ? 27 : s * 9;
    const end = s === 3 ? 33 : start + 8;
    let count = 0;
    let pairs = 0;
    for (let i = start; i <= end; i++) {
      if (i === wildIdx) continue;
      count += d[i];
      if (d[i] >= 2) pairs++;
    }
    suitBreakdown.push({ suit: suitNames[s], count, pairs });
  }

  return {
    pairCount,
    tripletCount,
    partialMelds,
    completeSequences,
    sevenPairsDistance,
    suitBreakdown,
    isolatedTiles,
    pairTiles,
    tripletTiles,
    wildCount,
  };
}

/**
 * Analyze the impact of discarding a specific tile from the hand.
 *
 * @param {import('../ai/game-sim.js').TileSet} handSet — 14-tile hand
 * @param {number} discardIdx — tile type index to discard
 * @param {number} wildIdx
 * @param {object} [meldSet] — optional melds (TileSet)
 * @returns {{
 *   tileIdx: number,
 *   label: string,
 *   shantenAfter: number,
 *   shantenBefore: number,
 *   isIsolated: boolean,
 *   isPartOfPair: boolean,
 *   isPartOfTriplet: boolean,
 *   isPartOfPartialMeld: boolean,
 *   preservedStructures: string[],
 *   brokenStructures: string[],
 *   improvementTiles: Array<{label:string, count:number}>,
 *   tileRole: string,
 *   assessment: 'excellent'|'good'|'neutral'|'bad'|'terrible',
 * }}
 */
export function analyzeDiscardImpact(handSet, discardIdx, wildIdx, meldSet) {
  const d = handSet.data;
  const count = d[discardIdx];
  const shantenBefore = computeShanten(handSet, wildIdx);

  // Classify the tile's role before discard
  let isIsolated = false;
  let isPartOfPair = false;
  let isPartOfTriplet = false;
  let isPartOfPartialMeld = false;
  let tileRole = '孤张';

  if (count >= 3) {
    isPartOfTriplet = true;
    tileRole = '刻子中的一张';
  } else if (count === 2) {
    isPartOfPair = true;
    tileRole = '对子中的一张';
  } else {
    // Check if it's part of a partial meld (搭子)
    const suitBase = Math.floor(discardIdx / 9) * 9;
    const rank = discardIdx - suitBase + 1;
    if (discardIdx < 27) {
      if (rank < 9 && d[discardIdx + 1] > 0) {
        isPartOfPartialMeld = true;
        tileRole = rank <= 7 ? '搭子成员（双面）' : '边搭成员';
      } else if (rank > 1 && d[discardIdx - 1] > 0) {
        isPartOfPartialMeld = true;
        tileRole = rank >= 3 ? '搭子成员（双面）' : '边搭成员';
      } else if (rank <= 7 && d[discardIdx + 2] > 0) {
        isPartOfPartialMeld = true;
        tileRole = '嵌搭成员';
      } else if (rank >= 3 && d[discardIdx - 2] > 0) {
        isPartOfPartialMeld = true;
        tileRole = '嵌搭成员';
      } else {
        isIsolated = true;
      }
    } else {
      isIsolated = (count === 1);
      if (count >= 2) { isPartOfPair = true; tileRole = '对子中的一张'; }
    }
  }

  // What structures are preserved?
  const preservedStructures = [];
  const brokenStructures = [];

  // Try the discard
  const handClone = handSet.clone();
  handClone.remove(discardIdx);
  const shantenAfter = computeShanten(handClone, wildIdx);

  // Preserved: pairs not involving this tile
  for (let i = 0; i < 34; i++) {
    if (i === wildIdx || i === discardIdx) continue;
    if (handClone.data[i] >= 2 && handSet.data[i] >= 2) {
      const label = tileIdxToLabel(i);
      if (handClone.data[i] >= 3 && !preservedStructures.includes(label + '刻子')) {
        preservedStructures.push(label + '刻子');
      } else if (handClone.data[i] === 2 && !preservedStructures.some(s => s.includes(label))) {
        preservedStructures.push(label + '对子');
      }
    }
  }

  // What breaks?
  if (isPartOfPair && count === 2) {
    brokenStructures.push(tileIdxToLabel(discardIdx) + '对子（拆散）');
  } else if (isPartOfTriplet) {
    if (count === 4) {
      preservedStructures.push(tileIdxToLabel(discardIdx) + '刻子（保留）');
    }
  } else if (isPartOfPartialMeld) {
    brokenStructures.push(tileIdxToLabel(discardIdx) + '所在的搭子（拆散）');
  }

  if (shantenAfter < shantenBefore) {
    preservedStructures.push('向听数进步');
  } else if (shantenAfter > shantenBefore) {
    brokenStructures.push('向听数退步');
  }

  if (isIsolated) {
    preservedStructures.push('手牌结构完整（弃孤张不影响搭子）');
  }

  // Find improvement tiles (draws that reduce shanten after this discard)
  const improvementTiles = [];
  for (let i = 0; i < 34; i++) {
    if (i === wildIdx) continue;
    handClone.add(i);
    const sAfterDraw = computeShanten(handClone, wildIdx);
    handClone.remove(i);
    if (sAfterDraw < shantenAfter && sAfterDraw >= 0) {
      improvementTiles.push({ label: tileIdxToLabel(i), count: 4 - handSet.data[i] });
    }
  }
  // If shantenAfter is 0, winning tiles are improvements too
  if (shantenAfter === 0) {
    for (let i = 0; i < 34; i++) {
      if (i === wildIdx) continue;
      handClone.add(i);
      const sAfterDraw = computeShanten(handClone, wildIdx);
      handClone.remove(i);
      if (sAfterDraw < 0) { // winning!
        improvementTiles.push({ label: tileIdxToLabel(i), count: 4 - handSet.data[i] });
      }
    }
  }

  // Deduplicate and sort improvement tiles by count desc
  const uniqueImprovements = [];
  const seen = new Set();
  for (const imp of improvementTiles) {
    if (!seen.has(imp.label)) {
      seen.add(imp.label);
      uniqueImprovements.push(imp);
    }
  }
  uniqueImprovements.sort((a, b) => b.count - a.count);

  // Assessment
  let assessment = 'neutral';
  if (isIsolated && shantenAfter <= shantenBefore) assessment = 'excellent';
  else if (isIsolated) assessment = 'good';
  else if (isPartOfPair && pairCountAfterDiscard(handClone, wildIdx) <= 1) assessment = 'bad';
  else if (isPartOfTriplet) assessment = 'terrible';
  else if (shantenAfter > shantenBefore) assessment = 'bad';
  else if (shantenAfter === shantenBefore && !isIsolated) assessment = 'neutral';

  return {
    tileIdx: discardIdx,
    label: tileIdxToLabel(discardIdx),
    shantenAfter,
    shantenBefore,
    isIsolated,
    isPartOfPair,
    isPartOfTriplet,
    isPartOfPartialMeld,
    preservedStructures: preservedStructures.slice(0, 4),
    brokenStructures: brokenStructures.slice(0, 3),
    improvementTiles: uniqueImprovements.slice(0, 8),
    tileRole,
    assessment,
  };
}

function pairCountAfterDiscard(handSet, wildIdx) {
  let pairs = 0;
  const d = handSet.data;
  for (let i = 0; i < 34; i++) {
    if (i === wildIdx) continue;
    if (d[i] >= 2) pairs++;
  }
  return pairs;
}

/**
 * Match known mahjong strategic principles to the current hand.
 *
 * @param {object} structure — result from analyzeHandStructure()
 * @param {object} handInfo — { shanten, wildCount, meldCount }
 * @returns {Array<{principle:string, relevance:string, advice:string}>}
 */
export function matchStrategicPrinciples(structure, handInfo = {}) {
  const principles = [];
  const { pairCount, tripletCount, sevenPairsDistance, isolatedTiles, partialMelds } = structure;
  const wildCount = handInfo.wildCount || 0;

  // 1. 一对定将
  if (pairCount === 1 && tripletCount === 0 && sevenPairsDistance >= 3) {
    principles.push({
      principle: '一对定将',
      relevance: '直接适用',
      advice: '手牌仅有一对，这对是雀头候选，不建议拆对。应优先处理孤张和效率低的搭子。',
    });
  }

  // 2. 两对拆搭
  if (pairCount === 2 && tripletCount === 0 && sevenPairsDistance >= 3) {
    principles.push({
      principle: '两对拆搭',
      relevance: '直接适用',
      advice: '手牌有两对，应拆掉一个搭子而非拆对。保留两对可进张形成刻子或等待更好的雀头。',
    });
  }

  // 3. 三对拆对
  if (pairCount >= 3 && sevenPairsDistance >= 3) {
    principles.push({
      principle: '三对拆对',
      relevance: '直接适用',
      advice: `手牌有 ${pairCount} 对，应拆掉一对。多对会限制搭子空间，且降低听牌效率。`,
    });
  }

  // 4. 七对潜力
  if (sevenPairsDistance <= 2 && pairCount >= 4) {
    principles.push({
      principle: '七对考量',
      relevance: sevenPairsDistance <= 1 ? '强烈推荐' : '可考虑',
      advice: sevenPairsDistance <= 1
        ? `仅差 ${sevenPairsDistance} 对即可七对听牌，建议保留所有对子，往七对方向发展。`
        : `手牌已有 ${pairCount} 对，七对是可行的副线。拆牌时优先保留对子。`,
    });
  }

  // 5. 孤张优先
  if (isolatedTiles.length >= 2) {
    const labels = isolatedTiles.slice(0, 3).map(i => tileIdxToLabel(i)).join('、');
    principles.push({
      principle: '牌效基础',
      relevance: '通用原则',
      advice: `手中有 ${isolatedTiles.length} 张孤张（${labels}），应优先处理。孤张无助于搭子和听牌，是最低效的手牌。`,
    });
  }

  // 6. 双面搭优于嵌搭/边搭
  const twoSided = partialMelds.filter(m => m.quality === '双面搭').length;
  const gapped = partialMelds.filter(m => m.quality !== '双面搭').length;
  if (twoSided > 0 && gapped > 0) {
    principles.push({
      principle: '搭子选择',
      relevance: '通用原则',
      advice: `手中有 ${twoSided} 个双面搭和 ${gapped} 个嵌/边搭。双面搭进张概率是嵌搭的2倍，优先保留双面搭。`,
    });
  }

  // 7. 幺九处理
  const terminals = isolatedTiles.filter(i => i < 27 && (i % 9 === 0 || i % 9 === 8));
  if (terminals.length > 0 && isolatedTiles.length > terminals.length) {
    principles.push({
      principle: '幺九优先弃',
      relevance: '通用原则',
      advice: '幺九牌的搭子价值最低（只能形成边搭），在手中有多个孤张时优先弃幺九。',
    });
  }

  // 8. 财神使用
  if (wildCount >= 2 && handInfo.shanten <= 2) {
    principles.push({
      principle: '飘财窗口',
      relevance: '时机提醒',
      advice: `手中有 ${wildCount} 张财神且向听数 ${handInfo.shanten}，具备飘财条件。飘财可翻倍得分但增加风险，需权衡。`,
    });
  }

  return principles;
}

// ----------------------------------------------------------
// 8. Full interpretation — orchestrates all analyses
// ----------------------------------------------------------

/**
 * Run all search interpretation analyses and return a structured result.
 *
 * @param {object} options
 * @param {Map|Array|null} options.stats — ISMCTS search stats
 * @param {import('../ai/opponent-model.js').MultiPlayerModel|null} options.oppModel
 * @param {object} options.handInfo — { shanten, waits, wildCount, canPiaoCai, hasGangOpportunity, meldCount }
 * @param {number} [options.playerDiscardIdx] — the tile the player actually discarded
 * @param {number} [options.observerId=0]
 * @param {object} [options.gameContext] — { poolRemaining, isDealer, currentRound }
 * @returns {object} SearchInterpretation
 */
export function interpretSearch(options = {}) {
  const {
    stats = null,
    oppModel = null,
    handInfo = {},
    playerDiscardIdx = -1,
    observerId = 0,
    gameContext = {},
    handSet = null,        // optional: TileSet for structure analysis
  } = options;

  // 1. TOP-K extraction
  const topK = extractTopK(stats, DEFAULT_TOP_K, playerDiscardIdx);

  // 2. Threat detection
  const candidateIndices = topK.candidates.map(c => c.tileIdx);
  const threatAnalysis = detectThreats(oppModel, { observerId, candidateTileIndices: candidateIndices });

  // 3. Safe tile analysis
  const safeAnalysis = analyzeSafeTiles(oppModel, topK.candidates, observerId);

  // 4. Direction divergence
  const divergence = detectDirectionDivergence(stats, handInfo);

  // 5. Big hand vs fast hand
  const bigVsFast = detectBigVsFast(stats, handInfo);

  // 6. PiaoCai window
  const piaoCai = assessPiaoCaiWindow({
    shanten: handInfo.shanten,
    wildCount: handInfo.wildCount || 0,
    poolRemaining: gameContext.poolRemaining || 56,
    isDealer: gameContext.isDealer || false,
  });

  // 7. Search confidence
  const confidence = assessConfidence(stats);

  // 8. Hand structure analysis (if handSet provided)
  let handStructure = null;
  let strategicPrinciples = [];
  let discardImpacts = [];
  if (handSet) {
    const wildIdx = gameContext.wildIdx !== undefined ? gameContext.wildIdx : 33;
    handStructure = analyzeHandStructure(handSet, wildIdx);
    strategicPrinciples = matchStrategicPrinciples(handStructure, handInfo);

    // Analyze impact for each TOP-K candidate
    for (const c of topK.candidates.slice(0, 5)) {
      discardImpacts.push(analyzeDiscardImpact(handSet, c.tileIdx, wildIdx));
    }
    // Also analyze player's discard if not in TOP-K
    if (playerDiscardIdx >= 0 && !discardImpacts.some(d => d.tileIdx === playerDiscardIdx)) {
      discardImpacts.push(analyzeDiscardImpact(handSet, playerDiscardIdx, wildIdx));
    }

    // Re-rank topK candidates by hand structure assessment (priority over raw ISMCTS win rate)
    const impactByTile = new Map();
    for (const imp of discardImpacts) impactByTile.set(imp.tileIdx, imp);
    const ASSESSMENT_RANK = { excellent: 4, good: 3, neutral: 2, bad: 1, terrible: 0 };
    topK.candidates.sort((a, b) => {
      const impA = impactByTile.get(a.tileIdx);
      const impB = impactByTile.get(b.tileIdx);
      const rankA = impA ? (ASSESSMENT_RANK[impA.assessment] ?? 2) : 2;
      const rankB = impB ? (ASSESSMENT_RANK[impB.assessment] ?? 2) : 2;
      if (rankA !== rankB) return rankB - rankA; // higher tier first
      return b.winRate - a.winRate; // same tier → higher win rate first
    });
    // Update rank numbers after re-sort
    topK.candidates.forEach((c, i) => { c.rank = i + 1; });
  }

  // Player vs AI comparison
  let playerPicked = playerDiscardIdx >= 0
    ? topK.candidates.find(c => c.tileIdx === playerDiscardIdx) || null
    : null;

  // If player's discard was not explored by ISMCTS, synthesize from impact analysis
  if (!playerPicked && playerDiscardIdx >= 0) {
    const playerImpact = discardImpacts.find(d => d.tileIdx === playerDiscardIdx);
    if (playerImpact) {
      playerPicked = {
        tileIdx: playerDiscardIdx,
        label: playerImpact.label,
        visits: 0,
        winRate: 0,
        value: 0,
        rank: -1,
        _synthetic: true,
      };
    } else {
      // Last resort: create entry from tile index alone
      playerPicked = {
        tileIdx: playerDiscardIdx,
        label: tileIdxToLabel(playerDiscardIdx),
        visits: 0,
        winRate: 0,
        value: 0,
        rank: -1,
        _synthetic: true,
      };
    }
  }

  const topCandidate = topK.candidates.length > 0 ? topK.candidates[0] : null;
  const playerMatchedTop = topCandidate && playerPicked && playerPicked.tileIdx === topCandidate.tileIdx;
  const winRateDiff = (playerPicked && topCandidate)
    ? topCandidate.winRate - playerPicked.winRate
    : 0;

  return {
    topK,
    threatAnalysis,
    safeAnalysis,
    divergence,
    bigVsFast,
    piaoCai,
    confidence,
    handStructure,
    strategicPrinciples,
    discardImpacts,
    playerComparison: {
      playerPicked: playerPicked || null,
      topCandidate,
      matched: playerMatchedTop,
      winRateDiff,
    },
    timestamp: Date.now(),
  };
}
