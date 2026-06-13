// ============================================================
// ai-worker.js — Web Worker for ISMCTS search + AI decisions
// ============================================================
// Runs in a separate thread so ISMCTS computation doesn't block
// the UI. Main thread communicates via postMessage.
//
// Inbound messages:
//   { type: "chooseDiscard", gameData, playerId, difficulty }
//   { type: "shouldClaim",   gameData, playerId, claimType, tileSuit, tileRank }
//   { type: "chooseChi",     gameData, playerId, tileSuit, tileRank, choices }
//   { type: "chooseConcealedGang", gameData, playerId, choices }
//   { type: "shouldPiaoCai", gameData, playerId }
//
// Outbound messages:
//   { action: "discard", tileIdx, suit, rank, stats, source, error? }
//   { action: "shouldClaim", playerId, claim, error? }
//   { action: "chooseChi", playerId, choice, error? }
//   { action: "chooseConcealedGang", playerId, tileIdx, error? }
//   { action: "shouldPiaoCai", playerId, should, error? }
// ============================================================

import { fromGameState, computeShanten, chooseBestDiscard, runRollouts, indexToTileType, isWinningHandSet } from './game-sim.js';
import { chooseDiscardISMCTS, shouldClaimISMCTS } from './ismcts.js';
import { getDifficultyConfig } from './difficulty.js';

// Tile index → suit/rank conversion
const HONORS = ['east', 'south', 'west', 'north', 'red', 'green', 'white'];

function suitRankToIndex(suit, rank) {
  if (suit === 'character') return rank - 1;
  if (suit === 'dot') return 9 + (rank - 1);
  if (suit === 'bamboo') return 18 + (rank - 1);
  if (suit === 'honor') return 27 + HONORS.indexOf(rank);
  return -1;
}

function indexToSuitRank(idx) {
  if (idx < 9) return { suit: 'character', rank: idx + 1 };
  if (idx < 18) return { suit: 'dot', rank: idx - 9 + 1 };
  if (idx < 27) return { suit: 'bamboo', rank: idx - 18 + 1 };
  return { suit: 'honor', rank: HONORS[idx - 27] };
}

/** Pick a random non-wild tile from the player's hand. */
function randomDiscard(simState, playerId) {
  const hand = simState.hands[playerId];
  const wildIdx = simState.wildIdx;
  const candidates = [];
  const d = hand.data;
  for (let i = 0; i < 34; i++) {
    if (d[i] > 0 && i !== wildIdx) candidates.push(i);
  }
  if (candidates.length === 0) return -1;
  return candidates[Math.floor(Math.random() * candidates.length)];
}

// ============================================================
// Message dispatch
// ============================================================

self.onmessage = (e) => {
  const { type } = e.data;

  switch (type) {
    case 'chooseDiscard':
      handleChooseDiscard(e.data);
      break;
    case 'shouldClaim':
      handleShouldClaim(e.data);
      break;
    case 'chooseChi':
      handleChooseChi(e.data);
      break;
    case 'chooseConcealedGang':
      handleChooseConcealedGang(e.data);
      break;
    case 'shouldPiaoCai':
      handleShouldPiaoCai(e.data);
      break;
    default:
      self.postMessage({ error: true, message: `Unknown message type: ${type}` });
  }
};

// ============================================================
// chooseDiscard (existing ISMCTS path)
// ============================================================

function handleChooseDiscard({ gameData, playerId, difficulty }) {
  try {
    const simState = fromGameState(gameData);
    const config = getDifficultyConfig(difficulty);

    if (Math.random() < config.randomRate) {
      const tileIdx = randomDiscard(simState, playerId);
      if (tileIdx < 0) {
        self.postMessage({ error: true, message: 'No discardable tile in hand' });
        return;
      }
      const sr = indexToSuitRank(tileIdx);
      self.postMessage({ action: 'discard', tileIdx, suit: sr.suit, rank: sr.rank, stats: null, source: 'random' });
      return;
    }

    const result = chooseDiscardISMCTS(simState, playerId, config.iterations);

    if (result.tileIdx < 0) {
      self.postMessage({ error: true, message: 'ISMCTS returned no valid discard' });
      return;
    }

    const sr = indexToSuitRank(result.tileIdx);

    let statsArr = null;
    if (result.stats && result.stats.size > 0) {
      statsArr = Array.from(result.stats.entries());
    }

    self.postMessage({
      action: 'discard',
      tileIdx: result.tileIdx,
      suit: sr.suit,
      rank: sr.rank,
      stats: statsArr,
      source: 'ismcts',
    });
  } catch (err) {
    self.postMessage({ error: true, message: err.message || 'Unknown worker error' });
  }
}

// ============================================================
// shouldClaim — peng / gang decision
// ============================================================

function handleShouldClaim({ gameData, playerId, claimType, tileSuit, tileRank }) {
  try {
    const simState = fromGameState(gameData);
    const tileIdx = suitRankToIndex(tileSuit, tileRank);

    if (tileIdx < 0) {
      self.postMessage({ action: 'shouldClaim', playerId, claim: false });
      return;
    }

    // Gang is always beneficial (adds meld + extra draw + potential 杠开)
    if (claimType === 'gang') {
      self.postMessage({ action: 'shouldClaim', playerId, claim: true });
      return;
    }

    const result = shouldClaimISMCTS(simState, playerId, claimType, tileIdx, 0);
    self.postMessage({ action: 'shouldClaim', playerId, claim: result });
  } catch (err) {
    self.postMessage({ error: true, message: err.message || 'shouldClaim error' });
  }
}

// ============================================================
// chooseChi — pick best chi option
// ============================================================

function handleChooseChi({ gameData, playerId, tileSuit, tileRank, chiPatterns }) {
  try {
    if (!chiPatterns || chiPatterns.length === 0) {
      self.postMessage({ action: 'chooseChi', playerId, choice: null });
      return;
    }

    const simState = fromGameState(gameData);
    const hand = simState.hands[playerId];
    const wildIdx = simState.wildIdx;
    const beforeShanten = computeShanten(hand, wildIdx);

    let bestIdx = -1;
    let bestShanten = 99;

    // chiPatterns is an array of { indices: number[] } pairs representing which tiles from hand to use
    for (let i = 0; i < chiPatterns.length; i++) {
      const pattern = chiPatterns[i];
      const indices = pattern.indices || pattern;

      const handClone = hand.clone();
      let usedWild = 0;
      for (const idx of indices) {
        if (handClone.data[idx] > 0) {
          handClone.remove(idx);
        } else {
          handClone.remove(wildIdx);
          usedWild++;
        }
      }

      if (usedWild > hand.data[wildIdx]) continue;

      const afterShanten = computeShanten(handClone, wildIdx);
      if (afterShanten < bestShanten) {
        bestShanten = afterShanten;
        bestIdx = i;
      }
    }

    // Only chi if shanten doesn't worsen
    if (bestIdx >= 0 && bestShanten <= beforeShanten + 1) {
      self.postMessage({ action: 'chooseChi', playerId, choice: bestIdx, choiceIndex: bestIdx });
    } else {
      self.postMessage({ action: 'chooseChi', playerId, choice: null });
    }
  } catch (err) {
    self.postMessage({ error: true, message: err.message || 'chooseChi error' });
  }
}

// ============================================================
// chooseConcealedGang — which (if any) concealed gang to make
// ============================================================

function handleChooseConcealedGang({ gameData, playerId, choices }) {
  try {
    if (!choices || choices.length === 0) {
      self.postMessage({ action: 'chooseConcealedGang', playerId, tileIdx: -1 });
      return;
    }

    const simState = fromGameState(gameData);
    const hand = simState.hands[playerId];
    const wildIdx = simState.wildIdx;
    const beforeShanten = computeShanten(hand, wildIdx);

    // choices are tile type indices that the player has 4 of
    for (const tileIdx of choices) {
      if (tileIdx === wildIdx) continue;

      const handClone = hand.clone();
      handClone.remove(tileIdx);
      handClone.remove(tileIdx);
      handClone.remove(tileIdx);
      handClone.remove(tileIdx);

      const afterShanten = computeShanten(handClone, wildIdx);

      // Gang if shanten doesn't worsen (gang adds an extra draw)
      if (afterShanten <= beforeShanten) {
        self.postMessage({ action: 'chooseConcealedGang', playerId, tileIdx });
        return;
      }
    }

    self.postMessage({ action: 'chooseConcealedGang', playerId, tileIdx: -1 });
  } catch (err) {
    self.postMessage({ error: true, message: err.message || 'chooseConcealedGang error' });
  }
}

// ============================================================
// shouldPiaoCai — decide whether to declare piao cai
// ============================================================

function handleShouldPiaoCai({ gameData, playerId }) {
  try {
    const simState = fromGameState(gameData);
    const hand = simState.hands[playerId];
    const wildIdx = simState.wildIdx;

    // Must have at least two wild tiles
    if (hand.data[wildIdx] < 2) {
      self.postMessage({ action: 'shouldPiaoCai', playerId, should: false });
      return;
    }

    // Multi-piao: already in piaoCai mode, can continue if piaoCount < 2
    if (simState.piaoCai && simState.piaoCai[playerId]) {
      self.postMessage({ action: 'shouldPiaoCai', playerId, should: true });
      return;
    }

    // First piao: must be in 暴头 (can win right now)
    const canWin = isWinningHandSet(hand, wildIdx, simState.melds[playerId]);
    self.postMessage({ action: 'shouldPiaoCai', playerId, should: canWin });
  } catch (err) {
    self.postMessage({ error: true, message: err.message || 'shouldPiaoCai error' });
  }
}
