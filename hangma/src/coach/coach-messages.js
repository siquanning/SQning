// ============================================================
// coach-messages.js — Coach message generation system
// ============================================================
// Message data structure, 5 builder functions, priority queue,
// and main orchestrator for coach analysis after each discard.
// ============================================================

import { indexToTileType, tileTypeIndex } from '../ai/game-sim.js';
import { MultiPlayerModel } from '../ai/opponent-model.js';

// ----------------------------------------------------------
// Constants
// ----------------------------------------------------------

const HONORS = ['east', 'south', 'west', 'north', 'red', 'green', 'white'];

/** Priority levels — higher = more urgent */
export const PRIORITY = {
  THREAT: 100,
  TIMING: 80,
  DISCARD: 60,
  DIRECTION: 40,
  DEEP: 20,
  CONFIRM: 10,
};

/** Max messages shown per round */
const MAX_MESSAGES_PER_ROUND = 3;

// ----------------------------------------------------------
// Message factory
// ----------------------------------------------------------

/**
 * @param {string} type — 'discard'|'threat'|'timing'|'direction'|'deep'|'confirm'
 * @param {number} priority — higher = more urgent
 * @param {string} summary — one-line conclusion
 * @param {object} [options]
 * @param {string} [options.reasoning] — expandable reasoning text
 * @param {{optionA: {label,winRate,desc}, optionB: {label,winRate,desc}}|null} [options.fullComparison]
 * @param {string[]} [options.tags]
 * @returns {object} coach message
 */
export function createCoachMessage(type, priority, summary, options = {}) {
  return {
    type,
    priority,
    summary,
    reasoning: options.reasoning || '',
    fullComparison: options.fullComparison || null,
    tags: options.tags || [],
  };
}

// ----------------------------------------------------------
// Tile label helpers (avoid dependency on engine.js)
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
  if (tile.suit === 'honor') return HONOR_LABEL[tile.rank] || tile.rank;
  return CHINESE[tile.rank - 1] + SUIT_NAMES[tile.suit];
}

// ----------------------------------------------------------
// Builder 1: Discard Suggestion
// ----------------------------------------------------------

/**
 * Build discard comparison message.
 * Compares player's discard with AI TOP-1 recommendation.
 *
 * @param {Map|Array|null} stats — ISMCTS stats (Map or serialized entries array)
 * @param {number} playerDiscardIdx — tile index the player discarded
 * @param {object} greedyResult — { tileIdx, shanten } from chooseBestDiscard
 * @param {object} handInfo — { shanten, waits, wildCount }
 * @returns {object} coach message
 */
export function buildDiscardSuggestion(stats, playerDiscardIdx, greedyResult, handInfo) {
  const playerLabel = tileIdxToLabel(playerDiscardIdx);

  // If we have ISMCTS stats, use them for rich comparison
  if (stats && (stats instanceof Map ? stats.size > 0 : stats.length > 0)) {
    return buildDiscardFromStats(stats, playerDiscardIdx, playerLabel, handInfo);
  }

  // Fallback: greedy heuristic comparison
  return buildDiscardFromGreedy(greedyResult, playerDiscardIdx, playerLabel, handInfo);
}

function buildDiscardFromStats(stats, playerDiscardIdx, playerLabel, handInfo) {
  // Parse stats into ranked discard candidates
  const entries = stats instanceof Map ? Array.from(stats.entries()) : stats;
  const discards = [];

  for (const [key, data] of entries) {
    if (key.startsWith('discard:')) {
      const tileIdx = parseInt(key.split(':')[1], 10);
      discards.push({ tileIdx, visits: data.visits || 0, winRate: data.winRate || 0 });
    }
  }
  discards.sort((a, b) => b.winRate - a.winRate);

  if (discards.length === 0) {
    return createCoachMessage('confirm', PRIORITY.CONFIRM, `已打出 ${playerLabel}。`, { tags: ['弃牌分析'] });
  }

  const top1 = discards[0];
  const top1Label = tileIdxToLabel(top1.tileIdx);
  const top1WinRate = (top1.winRate * 100).toFixed(1);
  const isMatch = playerDiscardIdx === top1.tileIdx;

  if (isMatch) {
    const tags = ['弃牌一致'];
    if (handInfo.waits && handInfo.waits.length > 0) {
      tags.push(`听${handInfo.waits.length}门`);
    }
    return createCoachMessage('confirm', PRIORITY.CONFIRM,
      `打出 ${playerLabel} 是最优选择（胜率 ${top1WinRate}%），与 ISMCTS 分析一致。`,
      {
        reasoning: buildDiscardReasoning(discards, playerDiscardIdx, handInfo),
        tags,
      });
  }

  // Player chose differently — full comparison
  const picked = discards.find(d => d.tileIdx === playerDiscardIdx);
  const pickedWinRate = picked ? (picked.winRate * 100).toFixed(1) : '?';
  const diff = (top1.winRate - (picked ? picked.winRate : 0)) * 100;

  const tags = ['弃牌对比'];
  if (diff > 5) tags.push(`胜率差${diff.toFixed(0)}%`);

  const waitInfo = handInfo.waits && handInfo.waits.length > 0
    ? `当前可听${handInfo.waits.length}门。`
    : '';

  return createCoachMessage('discard', PRIORITY.DISCARD,
    `ISMCTS 建议打 ${top1Label}（胜率 ${top1WinRate}%），你打了 ${playerLabel}（胜率 ${pickedWinRate}%）。${waitInfo}`,
    {
      reasoning: buildDiscardReasoning(discards, playerDiscardIdx, handInfo),
      fullComparison: {
        optionA: {
          label: `AI 推荐: ${top1Label}`,
          winRate: `${top1WinRate}%`,
          desc: buildOptionDesc(top1, handInfo),
        },
        optionB: {
          label: `你的选择: ${playerLabel}`,
          winRate: `${pickedWinRate}%`,
          desc: picked ? buildOptionDesc(picked, handInfo) : '无统计数据',
        },
      },
      tags,
    });
}

function buildDiscardFromGreedy(greedyResult, playerDiscardIdx, playerLabel, handInfo) {
  if (!greedyResult || greedyResult.tileIdx < 0) {
    return createCoachMessage('confirm', PRIORITY.CONFIRM, `已打出 ${playerLabel}。`, { tags: ['弃牌分析'] });
  }

  const aiLabel = tileIdxToLabel(greedyResult.tileIdx);
  const isMatch = playerDiscardIdx === greedyResult.tileIdx;

  if (isMatch) {
    const tags = ['弃牌一致'];
    if (handInfo.waits && handInfo.waits.length > 0) tags.push(`听${handInfo.waits.length}门`);
    return createCoachMessage('confirm', PRIORITY.CONFIRM,
      `打出 ${playerLabel} 是正确选择，与 AI 分析一致。`,
      { tags });
  }

  return createCoachMessage('discard', PRIORITY.DISCARD,
    `AI 建议打 ${aiLabel}，而非 ${playerLabel}。`,
    {
      reasoning: `AI 向听数 ${greedyResult.shanten}，建议回顾手牌结构，优先弃掉孤张或无搭子牌。`,
      tags: ['弃牌对比'],
    });
}

function buildDiscardReasoning(discards, playerDiscardIdx, handInfo) {
  const lines = [];
  const top3 = discards.slice(0, 3);

  lines.push('ISMCTS 搜索分析 TOP-3 弃牌选项：');
  for (const d of top3) {
    const marker = d.tileIdx === playerDiscardIdx ? ' ← 你的选择' : '';
    lines.push(`  ${tileIdxToLabel(d.tileIdx)}: 胜率 ${(d.winRate * 100).toFixed(1)}%, 探索 ${d.visits} 次${marker}`);
  }

  if (handInfo.shanten !== undefined) {
    lines.push(`当前向听数: ${handInfo.shanten}`);
  }

  return lines.join('\n');
}

function buildOptionDesc(stat, handInfo) {
  const parts = [`胜率 ${(stat.winRate * 100).toFixed(1)}%`, `探索 ${stat.visits} 次`];
  if (handInfo.shanten !== undefined) parts.push(`向听 ${handInfo.shanten}`);
  return parts.join('，');
}

// ----------------------------------------------------------
// Builder 2: Direction Analysis
// ----------------------------------------------------------

/**
 * Build hand direction / development analysis.
 *
 * @param {Map|Array|null} stats — ISMCTS stats
 * @param {object} handInfo — { shanten, waits, wildCount }
 * @returns {object|null} coach message or null if nothing useful
 */
export function buildDirectionAnalysis(stats, handInfo) {
  if (!handInfo.waits || handInfo.waits.length === 0) {
    // Not tenpai — give shanten improvement direction
    if (handInfo.shanten !== undefined && handInfo.shanten <= 2) {
      return createCoachMessage('direction', PRIORITY.DIRECTION,
        `当前向听数 ${handInfo.shanten}，距离听牌还差 ${handInfo.shanten} 步。优先保留中间张，构建顺子搭子。`,
        {
          reasoning: '中间张（3-7）更容易形成顺子，边张和幺九牌搭子价值较低。尽量保持手牌中有多组两面搭子。',
          tags: ['方向建议', `向听${handInfo.shanten}`],
        });
    }
    return null;
  }

  // Tenpai — summarize wait patterns
  const waitList = handInfo.waits.slice(0, 4).map(w => {
    const discardLabel = tileToLabel(w.discard);
    const waitLabels = w.waits.map(tileToLabel).join('/');
    return `${discardLabel}→${waitLabels}`;
  }).join('；');

  const wildHint = handInfo.wildCount > 0
    ? `手中有 ${handInfo.wildCount} 张财神，已具备爆头条件。`
    : '';

  return createCoachMessage('direction', PRIORITY.DIRECTION,
    `已听牌！听 ${handInfo.waits.length} 门：${waitList}。${wildHint}`,
    {
      reasoning: buildDirectionReasoning(handInfo),
      tags: ['听牌分析', `听${handInfo.waits.length}门`],
    });
}

function buildDirectionReasoning(handInfo) {
  const lines = ['听牌方向分析：'];
  if (handInfo.waits) {
    for (const w of handInfo.waits.slice(0, 5)) {
      lines.push(`  打 ${tileToLabel(w.discard)} → 等 ${w.waits.map(tileToLabel).join('/')}`);
    }
  }
  if (handInfo.wildCount > 0) {
    lines.push(`手中有 ${handInfo.wildCount} 张财神，可做爆头。自摸任意听牌+财神即可胡。`);
  }
  return lines.join('\n');
}

// ----------------------------------------------------------
// Builder 3: Threat Warning
// ----------------------------------------------------------

/**
 * Build threat warning from opponent belief model.
 *
 * @param {MultiPlayerModel|null} oppModel
 * @param {number} playerDiscardIdx — tile index the player just discarded
 * @param {object} gameState — engine GameState (for player names/winds)
 * @returns {object|null} coach message or null if no threat
 */
export function buildThreatWarning(oppModel, playerDiscardIdx, gameState) {
  if (!oppModel) return null;

  // Check each opponent's tenpai probability
  const threats = [];
  for (let p = 1; p < 4; p++) {
    const tenpaiProb = oppModel.getTenpaiProbability(p);
    if (tenpaiProb > 0.4) {
      const danger = oppModel.getTileDanger(p, playerDiscardIdx);
      threats.push({ playerId: p, tenpaiProb, danger });
    }
  }

  if (threats.length === 0) return null;

  // Sort by danger level
  threats.sort((a, b) => b.danger - a.danger);
  const topThreat = threats[0];

  let windName;
  try {
    windName = gameState.players[topThreat.playerId].name;
  } catch (_) {
    windName = `对手${topThreat.playerId}`;
  }

  if (topThreat.tenpaiProb > 0.7 && topThreat.danger > 0.5) {
    return createCoachMessage('threat', PRIORITY.THREAT,
      `⚠ ${windName} 极可能已听牌（置信度 ${(topThreat.tenpaiProb * 100).toFixed(0)}%），你刚打的 ${tileIdxToLabel(playerDiscardIdx)} 对其危险度 ${(topThreat.danger * 100).toFixed(0)}%。`,
      {
        reasoning: buildThreatReasoning(threats, gameState),
        tags: ['威胁预警', '高危'],
      });
  }

  if (topThreat.tenpaiProb > 0.4) {
    return createCoachMessage('threat', PRIORITY.DISCARD,
      `注意 ${windName} 可能已听牌（置信度 ${(topThreat.tenpaiProb * 100).toFixed(0)}%），出牌需谨慎。`,
      {
        reasoning: buildThreatReasoning(threats, gameState),
        tags: ['威胁预警'],
      });
  }

  return null;
}

function buildThreatReasoning(threats, gameState) {
  const lines = ['对手威胁评估：'];
  for (const t of threats) {
    let name;
    try { name = gameState.players[t.playerId].name; } catch (_) { name = `对手${t.playerId}`; }
    const level = t.tenpaiProb > 0.7 ? '高' : t.tenpaiProb > 0.4 ? '中' : '低';
    lines.push(`  ${name}: 听牌概率 ${(t.tenpaiProb * 100).toFixed(0)}%（${level}），弃牌危险度 ${(t.danger * 100).toFixed(0)}%`);
  }
  lines.push('建议：尽量跟打对手已出过的安全牌，避免点炮。');
  return lines.join('\n');
}

// ----------------------------------------------------------
// Builder 4: Timing Reminder
// ----------------------------------------------------------

/**
 * Build timing reminder for special opportunities.
 *
 * @param {object} gameState — engine GameState
 * @param {number} simHandShanten — shanten number from simulation
 * @param {number} wildCount — wild tiles in hand
 * @param {boolean} hasPiaoCai — whether player already declared piao cai
 * @returns {object|null} coach message or null
 */
export function buildTimingReminder(gameState, simHandShanten, wildCount, hasPiaoCai) {
  // 飘财窗口检测
  if (!hasPiaoCai && wildCount >= 2 && simHandShanten <= 2) {
    const tilePoolRemaining = gameState.tilePool ? gameState.tilePool.length : 0;
    const urgency = tilePoolRemaining < 20 ? '牌墙所剩不多' : '时机良好';

    return createCoachMessage('timing', PRIORITY.TIMING,
      `手中有 ${wildCount} 张财神且向听数 ${simHandShanten}，可考虑飘财！${urgency}。`,
      {
        reasoning: `飘财后不能用财神做万能牌，但胡牌计分翻倍。手中有 ${wildCount} 张财神意味着飘财后仍有充足灵活性。向听数越低，飘财成功率越高。`,
        tags: ['时机提醒', '飘财窗口'],
      });
  }

  // 爆头检测
  if (wildCount >= 1 && simHandShanten === 0) {
    return createCoachMessage('timing', PRIORITY.TIMING,
      '已听牌且有财神！自摸即可爆头胡牌，注意听牌变化。',
      {
        reasoning: '爆头=自摸+手中有财神，是最常见的胡牌方式。保持听牌状态，等待自摸机会。',
        tags: ['时机提醒', '爆头就绪'],
      });
  }

  // 听牌接近中
  if (wildCount >= 1 && simHandShanten === 1) {
    return createCoachMessage('timing', PRIORITY.DIRECTION,
      `距离听牌仅差 1 步，手中有 ${wildCount} 张财神可灵活使用。`,
      {
        tags: ['时机提醒', '接近听牌'],
      });
  }

  return null;
}

// ----------------------------------------------------------
// Builder 5: Deep Analysis
// ----------------------------------------------------------

/**
 * Build deep analysis from ISMCTS search stats.
 * Only generated when ISMCTS stats have sufficient data.
 *
 * @param {Map|Array|null} stats — ISMCTS stats
 * @param {number} playerDiscardIdx
 * @param {object} handInfo
 * @returns {object|null} coach message or null
 */
export function buildDeepAnalysis(stats, playerDiscardIdx, handInfo) {
  if (!stats) return null;

  const entries = stats instanceof Map ? Array.from(stats.entries()) : stats;
  const discards = [];

  for (const [key, data] of entries) {
    if (key.startsWith('discard:')) {
      const tileIdx = parseInt(key.split(':')[1], 10);
      discards.push({ tileIdx, visits: data.visits || 0, winRate: data.winRate || 0, value: data.value || 0 });
    }
  }
  discards.sort((a, b) => b.visits - a.visits);

  // Only generate deep analysis if we have enough data (total visits > 500)
  const totalVisits = discards.reduce((s, d) => s + d.visits, 0);
  if (totalVisits < 500 || discards.length < 2) return null;

  const top2 = discards.slice(0, 2);
  const top1Label = tileIdxToLabel(top2[0].tileIdx);
  const top2Label = tileIdxToLabel(top2[1].tileIdx);

  const reasoning = [
    `ISMCTS 深度搜索分析（共 ${totalVisits} 次模拟）：`,
    '',
    '搜索树根节点统计：',
    ...discards.slice(0, 5).map((d, i) =>
      `  ${i + 1}. ${tileIdxToLabel(d.tileIdx)}: 胜率 ${(d.winRate * 100).toFixed(1)}%, 探索 ${d.visits} 次 (${(d.visits / totalVisits * 100).toFixed(0)}%)`),
    '',
    '分析说明：ISMCTS 通过大量随机模拟评估每种弃牌选择的长期胜率。',
    `探索次数最多的选项是 ${top1Label}，说明搜索认为该分支最有希望。`,
    '胜率差异反映了手牌结构和对手模型对最终结果的影响。',
  ].join('\n');

  return createCoachMessage('deep', PRIORITY.DEEP,
    `深度分析：${top1Label} vs ${top2Label} —— 胜率差 ${((top2[0].winRate - top2[1].winRate) * 100).toFixed(1)}%`,
    {
      reasoning,
      fullComparison: {
        optionA: {
          label: `首选: ${top1Label}`,
          winRate: `${(top2[0].winRate * 100).toFixed(1)}%`,
          desc: buildOptionDesc(top2[0], handInfo),
        },
        optionB: {
          label: `次选: ${top2Label}`,
          winRate: `${(top2[1].winRate * 100).toFixed(1)}%`,
          desc: buildOptionDesc(top2[1], handInfo),
        },
      },
      tags: ['深度解析', `${totalVisits}次模拟`],
    });
}

// ----------------------------------------------------------
// Opponent model factory
// ----------------------------------------------------------

/**
 * Build a MultiPlayerModel from the engine game state.
 * Replays all observed discards and melds to populate opponent beliefs.
 *
 * @param {object} game — engine GameState
 * @param {number} observerId — human player (0)
 * @returns {MultiPlayerModel}
 */
export function buildOpponentModelFromGame(game, observerId) {
  const model = new MultiPlayerModel(observerId);
  let turn = 0;

  // Max discards across all players to determine interleaving
  const maxDiscards = Math.max(...game.players.map(p => p.discards.length), 0);

  // Interleave discards by turn order approximation
  for (let i = 0; i < maxDiscards; i++) {
    for (let p = 0; p < 4; p++) {
      if (p === observerId) continue;
      const player = game.players[p];
      if (i < player.discards.length) {
        const tile = player.discards[i];
        try {
          const idx = tileTypeIndex(tile.suit, tile.rank);
          model.recordDiscard(p, idx, turn, false);
        } catch (_) { /* ignore malformed tile */ }
      }
    }
    turn++;
  }

  // Feed melds (these happen at specific points, order is approximate)
  for (let p = 0; p < 4; p++) {
    if (p === observerId) continue;
    const player = game.players[p];
    for (const meld of player.melds) {
      try {
        const tileIndices = meld.tiles.map(t => tileTypeIndex(t.suit, t.rank));
        model.recordMeld(p, meld.type, tileIndices, turn++);
      } catch (_) { /* ignore malformed meld */ }
    }
  }

  return model;
}

// ----------------------------------------------------------
// Main orchestrator
// ----------------------------------------------------------

/**
 * Generate all coach messages for a player discard action.
 *
 * @param {object} options
 * @param {object} options.gameState — engine GameState
 * @param {object} options.simState — SimGameState (pre-discard, 14-tile hand)
 * @param {object} options.playerDiscard — {suit, rank} the discarded tile
 * @param {number} options.playerDiscardIdx — tile type index
 * @param {object} options.greedyResult — {tileIdx, shanten} from chooseBestDiscard
 * @param {MultiPlayerModel|null} options.opponentModel
 * @param {object} options.handInfo — {shanten, waits, wildCount}
 * @param {Map|Array|null} options.ismctsStats — ISMCTS search stats (null if not available)
 * @returns {object[]} sorted & filtered coach messages
 */
export function generateCoachMessages(options) {
  const {
    gameState, simState, playerDiscard, playerDiscardIdx,
    greedyResult, opponentModel, handInfo, ismctsStats,
  } = options;

  const messages = [];

  // 1. Threat warning (highest priority — safety first)
  const threat = buildThreatWarning(opponentModel, playerDiscardIdx, gameState);
  if (threat) messages.push(threat);

  // 2. Timing reminder
  const wildCount = handInfo.wildCount || 0;
  const hasPiaoCai = gameState.players[0].piaoCai;
  const timing = buildTimingReminder(gameState, handInfo.shanten, wildCount, hasPiaoCai);
  if (timing) messages.push(timing);

  // 3. Discard suggestion (always generated)
  const discard = buildDiscardSuggestion(ismctsStats, playerDiscardIdx, greedyResult, handInfo);
  if (discard) messages.push(discard);

  // 4. Direction analysis
  const direction = buildDirectionAnalysis(ismctsStats, handInfo);
  if (direction) messages.push(direction);

  // 5. Deep analysis (only with ISMCTS stats)
  const deep = buildDeepAnalysis(ismctsStats, playerDiscardIdx, handInfo);
  if (deep) messages.push(deep);

  // Sort by priority descending, then limit to max per round
  messages.sort((a, b) => b.priority - a.priority);

  // Always include discard comparison + max 2 others
  const discardMsg = messages.find(m => m.type === 'discard' || m.type === 'confirm');
  const others = messages.filter(m => m !== discardMsg);
  const selected = [];

  if (discardMsg) selected.push(discardMsg);
  for (const m of others) {
    if (selected.length >= MAX_MESSAGES_PER_ROUND) break;
    selected.push(m);
  }

  return selected;
}
