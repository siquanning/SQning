// ============================================================
// coach-worker.js — Web Worker for coach AI analysis
// ============================================================
// Runs ISMCTS search + search interpreter in a separate thread
// so coach analysis doesn't block the UI.
//
// Inbound messages:
//   { type: "analyze", gameData, depth?, playerDiscard? }
//   { type: "analyzeFromState", simState, handInfo, oppModelData, depth?, playerDiscardIdx? }
//
// Outbound messages:
//   { type: "coachAnalysis", messages, interpretation, meta }
//   { type: "error", message }
//
// Analysis depths:
//   "brief"   — <100ms, ~500 iterations
//   "standard" — <300ms, ~1500 iterations (default)
//   "deep"    — <1s, ~4000 iterations, falls back to standard on timeout
// ============================================================

import {
  SimGameState, fromGameState, computeShanten, chooseBestDiscard,
  indexToTileType, isWinningHandSet,
} from '../ai/game-sim.js';

import { ismctsSearch, chooseDiscardISMCTS } from '../ai/ismcts.js';

import {
  MultiPlayerModel, determinizeBiased,
} from '../ai/opponent-model.js';

import {
  interpretSearch,
  extractTopK,
  assessConfidence,
} from './search-interpreter.js';

import {
  generateExplanations,
  resetTemplateCounters,
} from './nl-explainer.js';

// ----------------------------------------------------------
// Depth presets
// ----------------------------------------------------------

const DEPTH_PRESETS = {
  brief:    { iterations: 200,  timeLimit: 150 },
  standard: { iterations: 600,  timeLimit: 400 },
  deep:     { iterations: 2000, timeLimit: 1200 },
};

/** If deep mode exceeds this (ms), fall back to standard */
const DEEP_FALLBACK_MS = 800;

// ----------------------------------------------------------
// Helpers
// ----------------------------------------------------------

const HONORS = ['east', 'south', 'west', 'north', 'red', 'green', 'white'];

function tileTypeIndexFromGameTile(tile) {
  if (!tile) return -1;
  if (tile.suit === 'character') return tile.rank - 1;
  if (tile.suit === 'dot') return 9 + (tile.rank - 1);
  if (tile.suit === 'bamboo') return 18 + (tile.rank - 1);
  if (tile.suit === 'honor') return 27 + HONORS.indexOf(tile.rank);
  return -1;
}

function indexToSuitRank(idx) {
  const t = indexToTileType(idx);
  return t;
}

// ----------------------------------------------------------
// Hand info extraction (from SimGameState for observer)
// ----------------------------------------------------------

function extractHandInfo(simState, observerId) {
  const hand = simState.hands[observerId];
  const wildIdx = simState.wildIdx;
  const wildCount = hand.data[wildIdx];
  const meldCount = simState.meldInfos[observerId].length;

  // Compute shanten
  const handSize = hand.total();
  let shanten;
  if (handSize === 14) {
    shanten = computeShanten(hand, wildIdx);
  } else if (handSize === 13) {
    shanten = computeShanten(hand, wildIdx);
  } else {
    shanten = 99;
  }

  // Check if can piao cai
  const canPiaoCai = wildCount >= 2 && handSize === 14
    && isWinningHandSet(hand, wildIdx, simState.melds[observerId]);

  // Check for gang opportunity
  let hasGangOpportunity = false;
  const d = hand.data;
  for (let i = 0; i < 34; i++) {
    if (i !== wildIdx && d[i] >= 4) {
      hasGangOpportunity = true;
      break;
    }
  }

  // Build wait info
  const waits = buildWaitInfo(simState, observerId, hand, wildIdx, shanten);

  return {
    shanten: shanten === -1 ? 0 : shanten, // normalize: -1 (can win) → 0 (tenpai ready)
    waits,
    wildCount,
    meldCount,
    canPiaoCai,
    hasGangOpportunity,
  };
}

/**
 * Build wait tile information by trying each possible discard.
 */
function buildWaitInfo(simState, observerId, hand, wildIdx, shanten) {
  if (shanten > 0) return [];

  const waits = [];
  const meldSet = simState.melds[observerId];

  for (let i = 0; i < 34; i++) {
    if (hand.data[i] <= 0 || i === wildIdx) continue;

    // Clone hand and try discarding tile i
    const trial = hand.clone();
    trial.remove(i);

    const waitTiles = [];
    for (let j = 0; j < 34; j++) {
      if (j === wildIdx) continue;
      trial.add(j);
      if (isWinningHandSet(trial, wildIdx, meldSet)) {
        waitTiles.push(indexToSuitRank(j));
      }
      trial.remove(j);
    }

    if (waitTiles.length > 0) {
      waits.push({
        discard: indexToSuitRank(i),
        waits: waitTiles,
      });
    }
  }

  return waits;
}

// ----------------------------------------------------------
// Opponent model construction (from game state)
// ----------------------------------------------------------

function buildOpponentModel(gameData, observerId) {
  const mpm = new MultiPlayerModel(observerId);
  let turn = 0;

  const maxDiscards = Math.max(
    ...(gameData.players || []).map(p => (p.discards ? p.discards.length : 0)),
    0
  );

  // Interleave discards by turn
  for (let i = 0; i < maxDiscards; i++) {
    for (let p = 0; p < 4; p++) {
      if (p === observerId) continue;
      const player = gameData.players[p];
      if (!player || !player.discards) continue;
      if (i < player.discards.length) {
        const tile = player.discards[i];
        const idx = tileTypeIndexFromGameTile(tile);
        if (idx >= 0) mpm.recordDiscard(p, idx, turn, false);
      }
    }
    turn++;
  }

  // Record melds
  for (let p = 0; p < 4; p++) {
    if (p === observerId) continue;
    const player = gameData.players[p];
    if (!player || !player.melds) continue;
    for (const meld of player.melds) {
      const tiles = (meld.tiles || []).map(t => tileTypeIndexFromGameTile(t)).filter(i => i >= 0);
      if (tiles.length > 0) {
        mpm.recordMeld(p, meld.type, tiles, turn++);
      }
    }
  }

  return mpm;
}

// ----------------------------------------------------------
// Core analysis function
// ----------------------------------------------------------

/**
 * Run coach analysis: ISMCTS search + interpretation.
 *
 * @param {object} gameData — engine.js GameState (or precomputed simState)
 * @param {string} depth — 'brief' | 'standard' | 'deep'
 * @param {object} [playerDiscard] — { suit, rank } the tile the player just discarded
 * @returns {{ messages: object[], interpretation: object, meta: object }}
 */
function runCoachAnalysis(gameData, depth = 'standard', playerDiscard = null) {
  const observerId = 0; // coach always analyzes from human player's perspective
  const preset = DEPTH_PRESETS[depth] || DEPTH_PRESETS.standard;

  // 1. Build simulation state
  let simState;
  try {
    simState = fromGameState(gameData);
  } catch (e) {
    return { error: `构建模拟状态失败: ${e.message}` };
  }

  // 2. Extract hand info before search
  const handInfo = extractHandInfo(simState, observerId);

  // 3. Build opponent model
  const oppModel = buildOpponentModel(gameData, observerId);

  // 4. Determine player's discard index
  let playerDiscardIdx = -1;
  if (playerDiscard && playerDiscard.suit && playerDiscard.rank !== undefined) {
    playerDiscardIdx = tileTypeIndexFromGameTile(playerDiscard);
    // If this is a post-discard analysis (hand has 13 tiles), add the tile back
    if (simState.hands[observerId].total() < 14 && playerDiscardIdx >= 0) {
      simState.hands[observerId].add(playerDiscardIdx);
    }
  }

  // 5. Ensure it's observer's turn with 14 tiles
  const handSize = simState.hands[observerId].total();
  if (handSize < 14) {
    // Not at decision point — provide basic hand analysis only
    return buildBasicAnalysis(handInfo, oppModel, playerDiscardIdx, gameData);
  }

  // 6. Run ISMCTS search with determinization bias
  const startTime = performance.now();

  let result;
  try {
    result = ismctsSearch(simState, observerId, preset.iterations, {
      timeLimit: preset.timeLimit,
      determinizeFn: (state, obsId) => determinizeBiased(state, obsId, oppModel),
    });
  } catch (e) {
    // ISMCTS failed — fall back to basic analysis
    return buildBasicAnalysis(handInfo, oppModel, playerDiscardIdx, gameData);
  }

  const elapsed = performance.now() - startTime;

  // 7. Deep mode fallback: if took too long, note it in meta
  let actualDepth = depth;
  let fallbackNote = null;
  if (depth === 'deep' && elapsed > DEEP_FALLBACK_MS) {
    fallbackNote = `深度分析超时（${elapsed.toFixed(0)}ms），结果基于标准深度统计。`;
    actualDepth = 'standard';
  }

  // 8. Get greedy baseline for comparison
  const greedyResult = chooseBestDiscard(simState, observerId);

  // 9. Interpret search results
  const poolRemaining = gameData.tilePool ? gameData.tilePool.length : simState.pool.total();
  const isDealer = gameData.dealer === observerId;
  const observerHand = simState.hands[observerId];

  const interpretation = interpretSearch({
    stats: result.stats,
    oppModel,
    handInfo,
    playerDiscardIdx,
    observerId,
    handSet: observerHand,
    gameContext: {
      poolRemaining,
      isDealer,
      currentRound: simState.roundCount || 0,
      wildIdx: simState.wildIdx,
    },
  });

  // 10. Attach extra data for nl-explainer
  interpretation._handInfo = handInfo;
  interpretation._greedyResult = greedyResult;
  interpretation._elapsed = elapsed;

  // 11. Generate natural language messages
  resetTemplateCounters();
  const windNames = buildWindNames(gameData, observerId);
  const messages = generateExplanations(interpretation, {
    windNames,
    poolRemaining,
    isDealer,
  });

  // 12. Build meta
  const meta = {
    depth: actualDepth,
    requestedDepth: depth,
    elapsed,
    iterations: result.stats ? (() => {
      let total = 0;
      for (const [, v] of result.stats) total += (v.visits || 0);
      return total;
    })() : 0,
    fallbackNote,
    confidence: interpretation.confidence,
    shanten: handInfo.shanten,
    wildCount: handInfo.wildCount,
    handStructure: interpretation.handStructure,
    strategicPrinciples: interpretation.strategicPrinciples,
  };

  return { messages, interpretation, meta };
}

/**
 * Basic analysis when ISMCTS can't run (no search data).
 */
function buildBasicAnalysis(handInfo, oppModel, playerDiscardIdx, gameData) {
  const observerId = 0;
  let observerHand = null;
  let wildIdx = 33;
  try {
    const simState = fromGameState(gameData);
    observerHand = simState.hands[observerId];
    wildIdx = simState.wildIdx;
  } catch (_) { /* ignore */ }

  const interpretation = interpretSearch({
    stats: null,
    oppModel,
    handInfo,
    playerDiscardIdx,
    observerId,
    handSet: observerHand,
    gameContext: {
      poolRemaining: gameData.tilePool ? gameData.tilePool.length : 0,
      isDealer: gameData.dealer === observerId,
      wildIdx,
    },
  });
  interpretation._handInfo = handInfo;

  resetTemplateCounters();
  const windNames = buildWindNames(gameData, observerId);
  const messages = generateExplanations(interpretation, { windNames });

  const meta = {
    depth: 'basic',
    elapsed: 0,
    iterations: 0,
    fallbackNote: '无搜索数据，仅基于手牌分析。',
    confidence: interpretation.confidence,
    shanten: handInfo.shanten,
    wildCount: handInfo.wildCount,
    handStructure: interpretation.handStructure,
    strategicPrinciples: interpretation.strategicPrinciples,
  };

  return { messages, interpretation, meta };
}

function buildWindNames(gameData, observerId) {
  const defaultNames = ['自己', '下家', '对家', '上家'];
  if (!gameData || !gameData.players) return defaultNames;

  const names = [];
  for (let p = 0; p < 4; p++) {
    if (p === observerId) {
      names.push('自己');
    } else {
      const player = gameData.players[p];
      if (player && player.name) {
        // Map position-relative names
        const pos = (p - observerId + 4) % 4;
        const posNames = { 1: '下家', 2: '对家', 3: '上家' };
        names.push(player.name || posNames[pos] || `对手${p}`);
      } else {
        names.push(`对手${p}`);
      }
    }
  }
  return names;
}

// ----------------------------------------------------------
// Incremental update helpers
// ----------------------------------------------------------

/**
 * Lightweight incremental threat update (no full ISMCTS re-run).
 * Used when another player discards — just updates threat assessment.
 *
 * @param {object} gameData — engine.js GameState
 * @param {number} actingPlayerId — the player who just acted (discarded/melded)
 * @param {object} action — { type: 'discard'|'meld', tile?: {suit,rank} }
 * @returns {{ threats: object, messages: object[] }}
 */
function runThreatUpdate(gameData, actingPlayerId, action) {
  const observerId = 0;
  const oppModel = buildOpponentModel(gameData, observerId);

  const handInfo = {
    shanten: 99,
    waits: [],
    wildCount: 0,
    meldCount: 0,
    canPiaoCai: false,
    hasGangOpportunity: false,
  };

  // Extract minimal hand info for observer
  if (gameData.players && gameData.players[0] && gameData.players[0].hand) {
    const simState = fromGameState(gameData);
    Object.assign(handInfo, extractHandInfo(simState, observerId));
  }

  const interpretation = interpretSearch({
    stats: null,
    oppModel,
    handInfo,
    playerDiscardIdx: -1,
    observerId,
    gameContext: {
      poolRemaining: gameData.tilePool ? gameData.tilePool.length : 56,
      isDealer: gameData.dealer === observerId,
    },
  });
  interpretation._handInfo = handInfo;

  resetTemplateCounters();
  const windNames = buildWindNames(gameData, observerId);
  const messages = generateExplanations(interpretation, { windNames });

  return { interpretation, messages };
}

// Export core function for testing / direct use
export { runCoachAnalysis, runThreatUpdate, buildWindNames, extractHandInfo };

// ----------------------------------------------------------
// Worker message dispatch (only in Web Worker context)
// ----------------------------------------------------------

if (typeof self !== 'undefined' && typeof self.onmessage !== 'undefined') {
self.onmessage = (e) => {
  const { type } = e.data;

  switch (type) {
    case 'analyze': {
      const { gameData, depth = 'standard', playerDiscard = null } = e.data;
      try {
        const result = runCoachAnalysis(gameData, depth, playerDiscard);
        if (result.error) {
          self.postMessage({ type: 'coachAnalysis', error: result.error });
        } else {
          self.postMessage({
            type: 'coachAnalysis',
            messages: result.messages,
            interpretation: result.interpretation,
            meta: result.meta,
          });
        }
      } catch (err) {
        self.postMessage({
          type: 'coachAnalysis',
          error: err.message || 'Coach worker error',
        });
      }
      break;
    }

    case 'threatUpdate': {
      const { gameData, actingPlayerId, action } = e.data;
      try {
        const result = runThreatUpdate(gameData, actingPlayerId, action);
        self.postMessage({
          type: 'threatUpdate',
          messages: result.messages,
          threats: result.interpretation.threatAnalysis,
        });
      } catch (err) {
        self.postMessage({
          type: 'threatUpdate',
          error: err.message || 'Threat update error',
        });
      }
      break;
    }

    default:
      self.postMessage({
        type: 'error',
        message: `Unknown message type: ${type}`,
      });
  }
};
} // end typeof self check
