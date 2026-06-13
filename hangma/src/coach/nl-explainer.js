// ============================================================
// nl-explainer.js — Natural language explainer for coach AI
// ============================================================
// Converts structured SearchInterpretation objects into natural
// language coach messages with template rotation, knowledge tags,
// and wording norms enforcement.
// ============================================================

import { indexToTileType } from '../ai/game-sim.js';
import {
  THREAT_THRESHOLD,
  extractTopK,
  detectThreats,
  analyzeSafeTiles,
  detectDirectionDivergence,
  detectBigVsFast,
  assessPiaoCaiWindow,
  assessConfidence,
} from './search-interpreter.js';

// ----------------------------------------------------------
// Tile label helpers
// ----------------------------------------------------------

const SUIT_NAMES = { character: '万', dot: '筒', bamboo: '条', honor: '' };
const CHINESE = ['一','二','三','四','五','六','七','八','九'];
const HONOR_LABEL = { east: '东', south: '南', west: '西', north: '北', red: '红', green: '发', white: '白' };

function tileIdxToLabel(idx) {
  const t = indexToTileType(idx);
  if (t.suit === 'honor') return HONOR_LABEL[t.rank] || t.rank;
  return CHINESE[t.rank - 1] + SUIT_NAMES[t.suit];
}

function tileToLabel(tile) {
  if (!tile) return '?';
  if (tile.suit === 'honor') return HONOR_LABEL[tile.rank] || tile.rank;
  return CHINESE[tile.rank - 1] + SUIT_NAMES[tile.suit];
}

// ----------------------------------------------------------
// 1. Win Rate → Natural Language Mapping
// ----------------------------------------------------------

/**
 * Map a win rate (0-1) to a natural language description.
 * Uses 5% precision buckets as specified.
 *
 * @param {number} winRate — 0 to 1
 * @returns {{ level: string, phrase: string, bucket: string }}
 */
export function describeWinRate(winRate) {
  const pct = Math.round(winRate * 100 / 5) * 5; // round to nearest 5%

  if (pct >= 85) return { level: '极高', phrase: '胜券在握', bucket: `${pct}%` };
  if (pct >= 75) return { level: '高', phrase: '优势明显', bucket: `${pct}%` };
  if (pct >= 65) return { level: '较高', phrase: '有把握', bucket: `${pct}%` };
  if (pct >= 55) return { level: '中等', phrase: '略有优势', bucket: `${pct}%` };
  if (pct >= 45) return { level: '接近', phrase: '五五开，需要权衡', bucket: `${pct}%` };
  if (pct >= 35) return { level: '偏低', phrase: '处于劣势', bucket: `${pct}%` };
  if (pct >= 25) return { level: '低', phrase: '形势不利', bucket: `${pct}%` };
  return { level: '极低', phrase: '希望渺茫', bucket: `${pct}%` };
}

/**
 * Map search confidence level to reliability description.
 *
 * @param {object} confidence — result from assessConfidence()
 * @returns {{ phrase: string, suffix: string }}
 */
export function describeReliability(confidence) {
  switch (confidence.confidenceLevel) {
    case 'high':
      return { phrase: '结论可靠', suffix: '' };
    case 'medium':
      return { phrase: '有一定参考价值', suffix: '' };
    case 'low':
      return { phrase: '数据不足，仅供参考', suffix: '（仅供参考）' };
    case 'insufficient':
    default:
      return { phrase: '数据严重不足，仅供参考', suffix: '（仅供参考）' };
  }
}

// ----------------------------------------------------------
// 2. Template Libraries (5 categories, 5-10 templates each)
// ----------------------------------------------------------

let _templateCounters = { discard: 0, direction: 0, threat: 0, timing: 0, deep: 0 };

function pickTemplate(category) {
  const lib = TEMPLATES[category];
  if (!lib || lib.length === 0) return null;
  const idx = _templateCounters[category] % lib.length;
  _templateCounters[category]++;
  return lib[idx];
}

// Reset for testing
export function resetTemplateCounters() {
  _templateCounters = { discard: 0, direction: 0, threat: 0, timing: 0, deep: 0 };
}

const TEMPLATES = {

  // --- Discard Suggestion Templates (pre-discard advisory style) ---
  discard: [
    // Pre-discard: explaining which tile to discard and why
    (ctx) => {
      if (ctx.topAssessment === 'bad' || ctx.topAssessment === 'terrible') {
        let out = `手牌暂无孤张，${ctx.aiTileLabel} 是权衡之选。`;
        if (ctx.discardReason) out += ` ${ctx.discardReason}`;
        if (ctx.aiWinRateBucket) out += ` 模拟胜率约 ${ctx.aiWinRateBucket}。`;
        return out;
      }
      let out = `建议打 ${ctx.aiTileLabel}。`;
      if (ctx.discardReason) out += ` ${ctx.discardReason}`;
      if (ctx.discardBenefit) out += `好处是${ctx.discardBenefit}。`;
      if (ctx.discardRisk) out += `代价是${ctx.discardRisk}。`;
      if (ctx.aiWinRateBucket) out += ` 模拟胜率约 ${ctx.aiWinRateBucket}。`;
      return out;
    },
    (ctx) => {
      let out = `这手牌推荐打 ${ctx.aiTileLabel}`;
      if (ctx.discardReason) out += `：${ctx.discardReason}`;
      out += '。';
      if (ctx.discardBenefit) out += `保留${ctx.discardBenefit}。`;
      if (ctx.discardRisk) out += `注意${ctx.discardRisk}。`;
      return out;
    },
    (ctx) => {
      if (ctx.topAssessment === 'bad' || ctx.topAssessment === 'terrible') {
        let out = `手牌暂无理想出牌，${ctx.aiTileLabel} 相对最优`;
        if (ctx.discardReason) out += `—${ctx.discardReason}`;
        out += '。';
        return out;
      }
      let out = `${ctx.aiTileLabel} 是当前最优弃牌`;
      if (ctx.discardReason) out += `——${ctx.discardReason}`;
      out += '。';
      if (ctx.strategyHint) out += ` ${ctx.strategyHint}`;
      return out;
    },
    (ctx) => {
      if (ctx.topAssessment === 'bad' || ctx.topAssessment === 'terrible') {
        return `当前手牌缺乏理想出牌选择，${ctx.aiTileLabel} 是权衡之选。${ctx.discardReason || ''}${ctx.aiWinRateBucket ? ` 模拟胜率约 ${ctx.aiWinRateBucket}。` : ''}`;
      }
      return `综合手牌结构，${ctx.aiTileLabel} 是最佳选择。${ctx.discardReason || ''}${ctx.aiWinRateBucket ? ` 模拟胜率约 ${ctx.aiWinRateBucket}。` : ''}`;
    },
    (ctx) => {
      const parts = [`打 ${ctx.aiTileLabel}。`];
      if (ctx.discardBenefit) parts.push(`✓ ${ctx.discardBenefit}`);
      if (ctx.discardRisk) parts.push(`✗ ${ctx.discardRisk}`);
      if (ctx.principle) parts.push(`[${ctx.principle}]`);
      return parts.join(' ');
    },
    // Post-discard comparison (when player already acted)
    (ctx) => {
      const agree = ctx.playerLabel === ctx.aiTileLabel;
      if (agree) return `打出 ${ctx.playerLabel} 正确，与AI分析一致。${ctx.discardReason || ''}`;
      if (ctx.topAssessment === 'bad' || ctx.topAssessment === 'terrible') {
        return `当前手牌缺乏理想出牌选择，${ctx.aiTileLabel} 是相对较优的选项。${ctx.discardReason || ''}${ctx.diffPct ? ` 胜率差约 ${ctx.diffPct}。` : ''}`;
      }
      return `AI建议打 ${ctx.aiTileLabel} 而非 ${ctx.playerLabel}。${ctx.discardReason || ''}${ctx.diffPct ? ` 胜率差约 ${ctx.diffPct}。` : ''}`;
    },
    (ctx) => {
      const agree = ctx.playerLabel === ctx.aiTileLabel;
      if (agree) return `${ctx.playerLabel} — 正确的选择！${ctx.discardReason || ''}`;
      return `回顾：${ctx.aiTileLabel} 比 ${ctx.playerLabel} 更优。${ctx.candidateCompare || ''}`;
    },
    (ctx) => `${ctx.aiTileLabel} 是最优解。${ctx.discardReason || ''}${ctx.candidateCompare ? ` ${ctx.candidateCompare}` : ''}`,
    (ctx) => {
      if (ctx.candidateBreakdown) return ctx.candidateBreakdown;
      return `打 ${ctx.aiTileLabel}：${ctx.discardReason || '综合牌效最优'}。胜率约 ${ctx.aiWinRateBucket || '?'}。`;
    },
    (ctx) => `推荐 ${ctx.aiTileLabel}。${ctx.discardReason || ''}${ctx.candidateCompare ? ` 对比：${ctx.candidateCompare}` : ''}`,
  ],

  // --- Direction Analysis Templates ---
  direction: [
    (ctx) => {
      const pre = ctx.isTenpai
        ? `已听牌！等待 ${ctx.waitList} 即可胡牌。`
        : `距听牌还差 ${ctx.shanten} 步，建议优先保留中间张（3-7）构建搭子。`;
      return pre + (ctx.wildHint ? ` ${ctx.wildHint}` : '');
    },
    (ctx) => {
      if (ctx.isTenpai) {
        return `手牌已成型，听 ${ctx.waitCount} 门：${ctx.waitList}。保持耐心等待进张。`;
      }
      return `向听数 ${ctx.shanten}，当前阶段应关注牌效：优先处理孤张和边张搭子。`;
    },
    (ctx) => {
      if (ctx.isTenpai) {
        return `听牌确认：${ctx.waitList}。${ctx.wildCount > 0 ? '有财神可做爆头，自摸即可胡。' : '注意观察对手出牌，避免点炮。'}`;
      }
      return `手牌还需 ${ctx.shanten} 步听牌。中间张（3-7）最容易形成顺子，建议保留。`;
    },
    (ctx) => {
      if (ctx.isTenpai) {
        return `当前 ${ctx.waitCount} 门听牌（${ctx.waitList}）。${ctx.wildHint || '专注等待即可。'}`;
      }
      return `进度：向听 ${ctx.shanten}。关注牌河中已出现的牌型，判断哪些搭子仍有希望。`;
    },
    (ctx) => {
      if (ctx.isTenpai) {
        return `听牌状态良好：${ctx.waitList}。建议记录已出的关键牌，避免等死张。`;
      }
      const steps = ctx.shanten <= 1 ? '很快就能听牌' : `还需 ${ctx.shanten} 步`;
      return `手牌发展方向：${steps}。优先打掉无搭子的孤张牌。`;
    },
    (ctx) => {
      if (ctx.isTenpai) return `听牌了！${ctx.waitList}。注意别打危险牌点炮。`;
      return `离听牌差 ${ctx.shanten} 步。可以从弃掉边张幺九开始整理手牌。`;
    },
    (ctx) => {
      if (ctx.isTenpai) return `听牌：${ctx.waitList}，共 ${ctx.waitCount} 门有效进张。`;
      return `向听 ${ctx.shanten}。目前的重点是减少向听数，不必急于做大牌。`;
    },
    (ctx) => {
      if (ctx.isTenpai) return `已听 ${ctx.waitCount} 门（${ctx.waitList}），等待自摸或点炮机会。`;
      return `距离听牌还有 ${ctx.shanten} 步，建议从效率最低的牌开始弃。`;
    },
  ],

  // --- Threat Warning Templates ---
  threat: [
    (ctx) => `⚠ ${ctx.threatPlayer} ${ctx.threatDesc}。建议优先出安全牌，避免打生张。`,
    (ctx) => `注意：${ctx.threatPlayer} ${ctx.threatDesc}，弃牌需谨慎。`,
    (ctx) => `${ctx.threatPlayer} 状态值得关注：${ctx.threatDesc}。你刚出的 ${ctx.discardLabel} 对ta危险度较高。`,
    (ctx) => `防守提醒：${ctx.threatPlayer} ${ctx.threatDesc}。建议跟打ta已出过的牌。`,
    (ctx) => `${ctx.threatPlayer} ${ctx.threatDesc}，建议转向防守策略，优先留安全牌。`,
    (ctx) => `对手警报：${ctx.threatPlayer} ${ctx.threatDesc}。${
      ctx.safestTile ? `当前较安全的弃牌是 ${ctx.safestTile}。` : ''
    }`,
    (ctx) => `防守时刻：${ctx.threatPlayer} ${ctx.threatDesc}，宁可弃和也不要点炮。`,
    (ctx) => `${ctx.threatPlayer} 极可能已听牌！建议立即切换到防守模式。`,
  ],

  // --- Timing Reminder Templates ---
  timing: [
    (ctx) => {
      if (ctx.timingType === 'piaoCai') {
        return `飘财窗口：手中有 ${ctx.wildCount} 张财神，向听 ${ctx.shanten}。${
          ctx.piaoCaiScoreAdv > 0 ? '飘财期望得分更高，值得考虑。' : '直接胡牌可能更稳妥。'
        }`;
      }
      if (ctx.timingType === 'baoTou') {
        return '已听牌且有财神，自摸即可爆头胡牌！注意保持听牌状态。';
      }
      return `距离听牌仅差 1 步，手中有 ${ctx.wildCount} 张财神可灵活使用。`;
    },
    (ctx) => {
      if (ctx.timingType === 'piaoCai') {
        return `手中有 ${ctx.wildCount} 张财神，向听 ${ctx.shanten}，飘财预期得分 ${ctx.piaoCaiScore}。${
          ctx.poolLow ? '牌墙不多，需尽快决定。' : '时机尚可。'
        }`;
      }
      if (ctx.timingType === 'baoTou') {
        return '爆头就绪！自摸任意听牌即可胡，注意听牌变化。';
      }
      return `快听牌了（向听 ${ctx.shanten}），手中有财神可提速。`;
    },
    (ctx) => {
      if (ctx.timingType === 'piaoCai') {
        return `${ctx.wildCount} 张财神 + 向听 ${ctx.shanten} = 飘财候选。风险 ${ctx.piaoCaiRisk}。`;
      }
      if (ctx.timingType === 'baoTou') {
        return '听牌 + 财神 = 爆头条件满足。自摸胡牌即可得分翻倍。';
      }
      return `手牌进展顺利，距听牌 ${ctx.shanten} 步，继续优化牌效。`;
    },
    (ctx) => {
      if (ctx.timingType === 'piaoCai') {
        return `飘财评估：期望分 ${ctx.piaoCaiScore}，直接胡 ${ctx.directScore}。${
          ctx.piaoCaiScoreAdv > 0 ? '飘财占优。' : '建议直接胡。'
        }`;
      }
      if (ctx.timingType === 'baoTou') return '爆头条件已满足，自摸即可。';
      return `向听 ${ctx.shanten}，继续努力！`;
    },
    (ctx) => {
      if (ctx.timingType === 'piaoCai') {
        return `财神充足（${ctx.wildCount}张），向听低（${ctx.shanten}步），飘财成功率高。`;
      }
      if (ctx.timingType === 'baoTou') return '有财神 + 已听牌 = 爆头可胡。';
      return `接近听牌（差${ctx.shanten}步），有财神加持，进度良好。`;
    },
  ],

  // --- Deep Analysis Templates ---
  deep: [
    (ctx) => `深度分析：${ctx.topLabel}（胜率 ${ctx.topWR}）vs ${ctx.secondLabel}（胜率 ${ctx.secondWR}），胜率差 ${ctx.gapPct}%。共 ${ctx.totalVisits} 次模拟。`,
    (ctx) => `ISMCTS 搜索完成（${ctx.totalVisits} 次模拟）。最优 ${ctx.topLabel}（${ctx.topWR}），次优 ${ctx.secondLabel}（${ctx.secondWR}）。`,
    (ctx) => `模拟结果：首选 ${ctx.topLabel}（胜率 ${ctx.topWR}，探索 ${ctx.topVisits} 次），次选 ${ctx.secondLabel}。`,
    (ctx) => `搜索树分析：${ctx.topLabel} 以 ${ctx.topWR} 胜率领先，共探索 ${ctx.topVisits} 次。第二选择 ${ctx.secondLabel} 胜率 ${ctx.secondWR}。`,
    (ctx) => `基于 ${ctx.totalVisits} 次模拟：${ctx.topLabel} 是最优弃牌，胜率 ${ctx.topWR}。${ctx.secondLabel} 为备选。`,
    (ctx) => `深度搜索报告：TOP-2 弃牌为 ${ctx.topLabel} vs ${ctx.secondLabel}，差距 ${ctx.gapPct}%。${ctx.confidenceNote}`,
  ],
};

// ----------------------------------------------------------
// 3. Context Assembly Engine
// ----------------------------------------------------------

/**
 * Build context object for template filling.
 * Takes raw interpretation data and produces a flat key-value context.
 *
 * @param {object} interpretation — result from interpretSearch()
 * @param {object} gameContext — { playerName, windNames, poolRemaining, etc. }
 * @returns {object} template context
 */
export function buildTemplateContext(interpretation, gameContext = {}) {
  const { topK, threatAnalysis, safeAnalysis, confidence, piaoCai, bigVsFast, playerComparison,
    handStructure, strategicPrinciples, discardImpacts } = interpretation;
  const topCandidate = topK.candidates.length > 0 ? topK.candidates[0] : null;
  const playerDiscard = playerComparison.playerPicked;

  const ctx = {};

  // --- Discard context ---
  if (playerDiscard) {
    ctx.tileLabel = playerDiscard.label || tileIdxToLabel(playerDiscard.tileIdx) || '?';
    ctx.playerLabel = playerDiscard.label || tileIdxToLabel(playerDiscard.tileIdx) || '?';
    ctx.tileIdx = playerDiscard.tileIdx;
    ctx.winRateBucket = describeWinRate(playerDiscard.winRate).bucket;
    ctx.winRateLevel = describeWinRate(playerDiscard.winRate).level;
  }
  if (topCandidate) {
    ctx.aiTileLabel = topCandidate.label || tileIdxToLabel(topCandidate.tileIdx) || '?';
    ctx.aiTileIdx = topCandidate.tileIdx;
    ctx.aiWinRateBucket = describeWinRate(topCandidate.winRate).bucket;
    if (playerDiscard && topCandidate.tileIdx !== playerDiscard.tileIdx) {
      ctx.diffPct = `${Math.abs((topCandidate.winRate - playerDiscard.winRate) * 100).toFixed(0)}%`;
    }
  }

  // --- Hand structure context (from new analysis) ---
  if (handStructure) {
    ctx.pairCount = handStructure.pairCount;
    ctx.tripletCount = handStructure.tripletCount;
    ctx.sevenPairsDistance = handStructure.sevenPairsDistance;
    ctx.isolatedCount = handStructure.isolatedTiles.length;
    ctx.isolatedLabels = handStructure.isolatedTiles.slice(0, 3).map(i => tileIdxToLabel(i)).join('、');
    ctx.partialMeldCount = handStructure.partialMelds.length;
    ctx.completeSeqCount = handStructure.completeSequences;
  }

  // --- Strategic principles ---
  if (strategicPrinciples && strategicPrinciples.length > 0) {
    const first = strategicPrinciples[0];
    ctx.principle = first.principle;
    ctx.principleAdvice = first.advice;
    ctx.allPrinciples = strategicPrinciples;
  }

  // --- Discard impact analysis (pre-discard) ---
  if (discardImpacts && discardImpacts.length > 0) {
    ctx.candidateBreakdown = buildCandidateBreakdown(discardImpacts, topK, handStructure, strategicPrinciples);
    // For the top candidate, build specific fields
    const topImpact = discardImpacts.find(d => d.tileIdx === (topCandidate ? topCandidate.tileIdx : -1))
      || discardImpacts[0];
    if (topImpact) {
      ctx.topAssessment = topImpact.assessment;
      ctx.discardReason = buildDiscardReasonFromImpact(topImpact);
      ctx.discardBenefit = topImpact.preservedStructures.join('，');
      ctx.discardRisk = topImpact.brokenStructures.join('，');
      ctx.strategyHint = strategicPrinciples && strategicPrinciples.length > 0
        ? strategicPrinciples[0].advice : '';
      // Improvement tiles summary
      if (topImpact.improvementTiles && topImpact.improvementTiles.length > 0) {
        const impLabels = topImpact.improvementTiles.slice(0, 5).map(t => t.label).join('、');
        ctx.improvementHint = `摸进 ${impLabels} 等 ${topImpact.improvementTiles.length} 种牌可改善手牌。`;
      }
    }
    // Per-candidate comparison
    ctx.candidateCompare = buildCandidateCompare(discardImpacts, topK);
  }

  // --- Direction context ---
  const handInfo = interpretation._handInfo || {};
  ctx.isTenpai = handInfo.shanten === 0;
  ctx.shanten = handInfo.shanten !== undefined ? handInfo.shanten : '?';
  ctx.wildCount = handInfo.wildCount || 0;
  ctx.waitCount = handInfo.waits ? handInfo.waits.length : 0;
  if (handInfo.waits && handInfo.waits.length > 0) {
    ctx.waitList = handInfo.waits.slice(0, 4).map(w => {
      const discLabel = typeof w.discard === 'object' ? tileToLabel(w.discard) : tileIdxToLabel(w.discard);
      const waitLabels = w.waits.map(t => typeof t === 'object' ? tileToLabel(t) : tileIdxToLabel(t)).join('/');
      return `${discLabel}→${waitLabels}`;
    }).join('；');
  } else {
    ctx.waitList = '';
  }
  ctx.wildHint = ctx.wildCount > 0 ? `手中有 ${ctx.wildCount} 张财神。` : '';

  // --- Threat context ---
  if (threatAnalysis && threatAnalysis.highestThreat) {
    const ht = threatAnalysis.highestThreat;
    ctx.threatPlayer = gameContext.windNames
      ? gameContext.windNames[ht.playerId]
      : `对手${ht.playerId}`;
    ctx.threatProb = ht.tenpaiProb;
    ctx.threatDesc = ht.threatLevel === 'likely'
      ? '极可能已听牌'
      : ht.threatLevel === 'possible'
        ? '可能已听牌'
        : '暂无威胁';
    if (playerDiscard) {
      ctx.discardLabel = playerDiscard.label;
    }
  }
  if (safeAnalysis && safeAnalysis.safestCandidate) {
    ctx.safestTile = safeAnalysis.safestCandidate.label;
  }

  // --- Timing context ---
  ctx.timingType = '';
  if (ctx.wildCount >= 2 && ctx.shanten <= 2) {
    ctx.timingType = 'piaoCai';
    ctx.piaoCaiScore = piaoCai ? piaoCai.piaoCaiExpectedScore.toFixed(2) : '?';
    ctx.directScore = piaoCai ? piaoCai.directWinExpectedScore.toFixed(2) : '?';
    ctx.piaoCaiScoreAdv = piaoCai ? piaoCai.scoreAdvantage : 0;
    ctx.piaoCaiRisk = piaoCai ? piaoCai.riskLevel : '?';
    ctx.poolLow = gameContext.poolRemaining < 20;
  } else if (ctx.wildCount >= 1 && ctx.shanten === 0) {
    ctx.timingType = 'baoTou';
  }

  // --- Deep analysis context ---
  if (topK.candidates.length >= 2) {
    ctx.topLabel = topK.candidates[0].label;
    ctx.topWR = describeWinRate(topK.candidates[0].winRate).bucket;
    ctx.topVisits = topK.candidates[0].visits;
    ctx.secondLabel = topK.candidates[1].label;
    ctx.secondWR = describeWinRate(topK.candidates[1].winRate).bucket;
    ctx.gapPct = `${Math.abs((topK.candidates[0].winRate - topK.candidates[1].winRate) * 100).toFixed(0)}%`;
    ctx.totalVisits = topK.totalVisits;
    ctx.confidenceNote = confidence ? describeReliability(confidence).suffix : '';
  }

  return ctx;
}

// ----------------------------------------------------------
// 3a. Candidate Breakdown Builders
// ----------------------------------------------------------

/**
 * Build a detailed per-candidate breakdown string for the discard message.
 */
function buildCandidateBreakdown(impacts, topK, structure, principles) {
  if (!impacts || impacts.length < 2) return '';

  const lines = [];

  for (const imp of impacts.slice(0, 3)) {
    const entry = topK.candidates.find(c => c.tileIdx === imp.tileIdx);
    const wr = entry ? ` 胜率约 ${(entry.winRate * 100).toFixed(0)}%` : '';
    const role = imp.tileRole ? `（${imp.tileRole}）` : '';
    const preserved = imp.preservedStructures.length > 0
      ? ` ✓保留：${imp.preservedStructures.slice(0, 2).join('、')}` : '';
    const broken = imp.brokenStructures.length > 0
      ? ` ✗损失：${imp.brokenStructures.slice(0, 2).join('、')}` : '';
    const improvement = imp.improvementTiles && imp.improvementTiles.length > 0
      ? ` → 改善牌：${imp.improvementTiles.slice(0, 4).map(t => t.label).join(' ')}` : '';

    const marker = imp.assessment === 'excellent' ? '★' : imp.assessment === 'good' ? '✓' : '';

    lines.push(`${marker}打 ${imp.label}${role}${wr}${preserved}${broken}${improvement}`);
  }

  // Add principle reference
  if (principles && principles.length > 0) {
    lines.push(`[${principles[0].principle}] ${principles[0].advice}`);
  }

  return lines.join('\n');
}

function buildDiscardReasonFromImpact(impact) {
  if (!impact) return '';

  const parts = [];
  if (impact.isIsolated) {
    parts.push(`${impact.label} 是孤张，打出不影响手牌结构`);
  } else if (impact.isPartOfPair && impact.assessment === 'bad') {
    parts.push(`${impact.label} 是对子中的一张，拆对会损失雀头备选`);
  } else if (impact.isPartOfPartialMeld) {
    parts.push(`${impact.label} 是搭子的一部分，打出会拆散搭子`);
  } else if (impact.isPartOfTriplet) {
    parts.push(`${impact.label} 是刻子的一部分，打出会破坏刻子`);
  } else {
    parts.push(`打出 ${impact.label} 后向听数 ${impact.shantenAfter}`);
  }

  if (impact.shantenAfter < impact.shantenBefore) {
    parts.push('向听数进步');
  } else if (impact.shantenAfter > impact.shantenBefore) {
    parts.push('向听数退步');
  }

  if (impact.assessment === 'bad' || impact.assessment === 'terrible') {
    parts.push('手牌暂无孤张可打，此为权衡之选');
  }

  return parts.join('，');
}

function buildCandidateCompare(impacts, topK) {
  if (!impacts || impacts.length < 2) return '';

  const top = impacts[0];
  const second = impacts[1];

  let cmp = `${top.label}优于${second.label}：`;
  if (top.assessment === 'excellent' && second.assessment !== 'excellent') {
    cmp += `${top.label}是孤张可安全弃出，而${second.label}${second.tileRole}打出会损伤手牌。`;
  } else if (top.shantenAfter < second.shantenAfter) {
    cmp += `打${top.label}后向听数更低（${top.shantenAfter} vs ${second.shantenAfter}）。`;
  } else if (top.shantenAfter === second.shantenAfter) {
    const topImprove = top.improvementTiles ? top.improvementTiles.length : 0;
    const secImprove = second.improvementTiles ? second.improvementTiles.length : 0;
    if (topImprove > secImprove) {
      cmp += `两者向听数相同，但打${top.label}的改善进张更多（${topImprove}种 vs ${secImprove}种）。`;
    } else {
      cmp += `打${top.label}保留的结构更好——${top.preservedStructures.slice(0, 2).join('、') || '手牌更完整'}。`;
    }
  } else {
    cmp += `综合牌效和手牌结构，${top.label}更优。`;
  }

  return cmp;
}

// ----------------------------------------------------------
// 4. Message Generation with Templates
// ----------------------------------------------------------

/**
 * Generate a discard suggestion message.
 *
 * @param {object} ctx — template context from buildTemplateContext()
 * @param {boolean} isMatch — whether player's discard matches AI top pick
 * @returns {{ summary: string, tags: string[] }}
 */
export function explainDiscard(ctx, isMatch) {
  const subLib = isMatch ? TEMPLATES.discard.slice(0, 5) : TEMPLATES.discard.slice(5);
  const idx = _templateCounters.discard % subLib.length;
  _templateCounters.discard++;
  const summary = subLib[idx](ctx);
  const tags = isMatch ? ['弃牌一致'] : ['弃牌对比'];

  return { summary, tags };
}

/**
 * Generate a direction analysis message.
 *
 * @param {object} ctx — template context
 * @returns {{ summary: string, tags: string[] }}
 */
export function explainDirection(ctx) {
  const tpl = pickTemplate('direction');
  const summary = tpl(ctx);
  const tags = ctx.isTenpai
    ? ['听牌分析', `听${ctx.waitCount}门`]
    : ['方向建议', `向听${ctx.shanten}`];

  return { summary, tags };
}

/**
 * Generate a threat warning message.
 *
 * @param {object} ctx — template context
 * @returns {{ summary: string, tags: string[] }}|null
 */
export function explainThreat(ctx) {
  if (!ctx.threatPlayer || ctx.threatDesc === '暂无威胁') return null;

  const tpl = pickTemplate('threat');
  const summary = tpl(ctx);
  const tags = ctx.threatProb >= THREAT_THRESHOLD.LIKELY
    ? ['威胁预警', '高危']
    : ['威胁预警'];

  return { summary, tags };
}

/**
 * Generate a timing reminder message.
 *
 * @param {object} ctx — template context
 * @returns {{ summary: string, tags: string[] }}|null
 */
export function explainTiming(ctx) {
  if (!ctx.timingType) return null;

  const tpl = pickTemplate('timing');
  const summary = tpl(ctx);
  const tags = ctx.timingType === 'piaoCai'
    ? ['时机提醒', '飘财窗口']
    : ctx.timingType === 'baoTou'
      ? ['时机提醒', '爆头就绪']
      : ['时机提醒', '接近听牌'];

  return { summary, tags };
}

/**
 * Generate a deep analysis message.
 *
 * @param {object} ctx — template context
 * @returns {{ summary: string, tags: string[] }}|null
 */
export function explainDeep(ctx) {
  if (!ctx.topLabel || !ctx.secondLabel || ctx.totalVisits < 100) return null;

  const tpl = pickTemplate('deep');
  const summary = tpl(ctx);
  const tags = ['深度解析', `${ctx.totalVisits}次模拟`];

  return { summary, tags };
}

// ----------------------------------------------------------
// 5. Knowledge Tag Attachment
// ----------------------------------------------------------

/**
 * Knowledge tag categories.
 */
export const KNOWLEDGE_TAGS = {
  EFFICIENCY: '牌效基础',      // basic tile efficiency
  DIRECTION: '方向判断',       // hand direction decisions
  DEFENSE: '防守博弈',         // defensive play
  PROBABILITY: '概率推演',     // probability-based reasoning
  SCORING: '计分策略',         // scoring strategy
  TIMING: '时机把握',          // timing/special opportunities
};

/**
 * Attach knowledge tags to a message based on its content and context.
 *
 * @param {object} message — { summary, tags[] }
 * @param {object} context — template context + interpretation data
 * @returns {object} message with knowledge tags attached
 */
export function attachKnowledgeTags(message, context = {}) {
  const knowledgeTags = [];

  // Always include base efficiency for discard messages
  if (context.tileLabel) {
    knowledgeTags.push(KNOWLEDGE_TAGS.EFFICIENCY);
  }

  // Direction analysis → direction tag
  if (context.isTenpai !== undefined) {
    knowledgeTags.push(KNOWLEDGE_TAGS.DIRECTION);
  }

  // Threat → defense tag
  if (context.threatProb && context.threatProb >= THREAT_THRESHOLD.POSSIBLE) {
    knowledgeTags.push(KNOWLEDGE_TAGS.DEFENSE);
  }

  // ISMCTS search → probability tag
  if (context.totalVisits && context.totalVisits > 0) {
    knowledgeTags.push(KNOWLEDGE_TAGS.PROBABILITY);
  }

  // Piao cai → scoring + timing
  if (context.timingType === 'piaoCai') {
    knowledgeTags.push(KNOWLEDGE_TAGS.SCORING);
    knowledgeTags.push(KNOWLEDGE_TAGS.TIMING);
  }

  if (context.timingType === 'baoTou') {
    knowledgeTags.push(KNOWLEDGE_TAGS.TIMING);
  }

  // Deduplicate
  const uniqueKT = [...new Set(knowledgeTags)];

  return {
    ...message,
    knowledgeTags: uniqueKT,
  };
}

// ----------------------------------------------------------
// 6. Wording Norm Check
// ----------------------------------------------------------

/**
 * Forbidden/inappropriate phrases and their replacements.
 */
const WORDING_RULES = [
  // Imperative → suggestive
  { pattern: /必须/g, replacement: '建议' },
  { pattern: /一定/g, replacement: '建议' },
  { pattern: /千万不要/g, replacement: '不建议' },
  { pattern: /绝对不能/g, replacement: '尽量避免' },
  { pattern: /肯定/g, replacement: '很可能' },

  // Evaluative → descriptive
  { pattern: /打错了/g, replacement: '可以优化' },
  { pattern: /不好/g, replacement: '不是最优' },
  { pattern: /错误/g, replacement: '值得商榷' },
  { pattern: /糟糕/g, replacement: '不太理想' },
  { pattern: /太差/g, replacement: '不够理想' },

  // Absolute → probabilistic
  { pattern: /绝对/g, replacement: '基本' },
  { pattern: /100%/g, replacement: '极高概率' },
  { pattern: /0%/g, replacement: '极低概率' },
];

/**
 * Sanitize text to enforce wording norms.
 *
 * Rules:
 * - Use "建议"/"推荐", never "必须"/"一定"
 * - Probabilities use high/medium/low tiers
 * - Terms include brief explanations
 * - Avoid evaluative language
 *
 * @param {string} text
 * @returns {string} sanitized text
 */
export function sanitizeWording(text) {
  let result = text;

  for (const rule of WORDING_RULES) {
    result = result.replace(rule.pattern, rule.replacement);
  }

  return result;
}

/**
 * Sanitize all text fields in a message.
 */
export function sanitizeMessage(message) {
  if (!message) return message;

  const sanitized = { ...message };

  if (sanitized.summary) {
    sanitized.summary = sanitizeWording(sanitized.summary);
  }
  if (sanitized.reasoning) {
    sanitized.reasoning = sanitizeWording(sanitized.reasoning);
  }

  return sanitized;
}

// ----------------------------------------------------------
// 7. Term Explanations
// ----------------------------------------------------------

/**
 * Brief explanations for common mahjong terms.
 * Attached to messages when the term first appears.
 */
export const TERM_GLOSSARY = {
  '听牌': '手牌已准备就绪，只需再进一张特定牌即可胡牌。',
  '向听': '距离听牌还差的步数。向听数越低越接近胡牌。',
  '爆头': '自摸胡牌时手中有财神，得分翻倍。',
  '飘财': '胡牌前公开宣布，胡牌后得分翻倍，但过程中不能用财神当万能牌。',
  '财神': '万能牌，可替代任何牌使用。',
  '搭子': '两张牌组成的一组，再进一张即可成顺子或刻子。',
  '顺子': '同花色连续三张牌（如 123万）。',
  '刻子': '三张完全相同的牌。',
  '幺九': '数字为 1 或 9 的牌，或字牌。',
  '中张': '数字为 2-8 的牌，更容易形成顺子。',
  '安全牌': '对手大概率不需要的牌，打出不会点炮。',
  '生张': '牌河中未出现或极少出现的牌，较危险。',
  '点炮': '打出的牌被对手胡走。',
};

/**
 * Look up term explanation.
 */
export function explainTerm(term) {
  return TERM_GLOSSARY[term] || null;
}

/**
 * Find terms in text that have glossary entries, and annotate them.
 * @returns {Array<{term:string, explanation:string}>}
 */
export function annotateTerms(text) {
  const found = [];
  for (const [term, explanation] of Object.entries(TERM_GLOSSARY)) {
    if (text.includes(term)) {
      found.push({ term, explanation });
    }
  }
  return found;
}

// ----------------------------------------------------------
// 8. Full Explanation Pipeline
// ----------------------------------------------------------

/**
 * Run the full NL explanation pipeline:
 * interpretation → context → templates → sanitize → annotate
 *
 * @param {object} interpretation — result from interpretSearch()
 * @param {object} [gameContext] — additional game context
 * @returns {object[]} array of coach messages
 */
export function generateExplanations(interpretation, gameContext = {}) {
  const ctx = buildTemplateContext(interpretation, gameContext);
  const messages = [];

  // Threat (highest priority)
  const threat = explainThreat(ctx);
  if (threat) {
    const tagged = attachKnowledgeTags(threat, ctx);
    messages.push({
      type: 'threat',
      priority: 100,
      summary: sanitizeWording(tagged.summary),
      tags: [...tagged.tags, ...tagged.knowledgeTags],
      termAnnotations: annotateTerms(tagged.summary),
    });
  }

  // Timing
  const timing = explainTiming(ctx);
  if (timing) {
    const tagged = attachKnowledgeTags(timing, ctx);
    messages.push({
      type: 'timing',
      priority: 80,
      summary: sanitizeWording(tagged.summary),
      tags: [...tagged.tags, ...tagged.knowledgeTags],
      termAnnotations: annotateTerms(tagged.summary),
    });
  }

  // Discard suggestion
  const isMatch = interpretation.playerComparison.matched;
  const discard = explainDiscard(ctx, isMatch);
  if (discard) {
    const tagged = attachKnowledgeTags(discard, ctx);
    messages.push({
      type: isMatch ? 'confirm' : 'discard',
      priority: 60,
      summary: sanitizeWording(tagged.summary),
      tags: [...tagged.tags, ...tagged.knowledgeTags],
      termAnnotations: annotateTerms(tagged.summary),
    });
  }

  // Direction
  const direction = explainDirection(ctx);
  if (direction) {
    const tagged = attachKnowledgeTags(direction, ctx);
    messages.push({
      type: 'direction',
      priority: 40,
      summary: sanitizeWording(tagged.summary),
      tags: [...tagged.tags, ...tagged.knowledgeTags],
      termAnnotations: annotateTerms(tagged.summary),
    });
  }

  // Deep analysis
  const deep = explainDeep(ctx);
  if (deep) {
    const tagged = attachKnowledgeTags(deep, ctx);
    messages.push({
      type: 'deep',
      priority: 20,
      summary: sanitizeWording(tagged.summary),
      tags: [...tagged.tags, ...tagged.knowledgeTags],
      termAnnotations: annotateTerms(tagged.summary),
    });
  }

  // Sort by priority descending, limit to 3
  messages.sort((a, b) => b.priority - a.priority);
  return messages.slice(0, 3);
}
