// ============================================================
// opponent-model.js — Opponent belief model for ISMCTS
// ============================================================
// Tracks opponent hand distributions based on observed actions.
// Uses discard patterns, meld info, and timing to estimate:
//   1. Suit/direction preferences
//   2. Tenpai probability
//   3. Safe/dangerous tile classification
// Feeds into determinization sampling bias for ISMCTS.
// ============================================================

import { TileSet, drawRandom, determinize } from './game-sim.js';

// ----------------------------------------------------------
// Constants
// ----------------------------------------------------------

const SUIT_START = { character: 0, dot: 9, bamboo: 18, honor: 27 };
const SUIT_END = { character: 8, dot: 17, bamboo: 26, honor: 33 };
const SUITS = ['character', 'dot', 'bamboo', 'honor'];

/** How many recent discards to consider for tenpai detection */
const RECENT_DISCARD_WINDOW = 6;

/** Discard pattern change threshold for tenpai signals */
const TENPAI_SIGNAL_THRESHOLD = 0.3;

// ----------------------------------------------------------
// OpponentModel — per-player belief state
// ----------------------------------------------------------

class OpponentModel {
  constructor(playerId) {
    this.playerId = playerId;

    /** Discard history: [{ tileIdx, turn, fromDraw }] */
    this.discardHistory = [];

    /** Meld history: [{ type, tiles: number[], turn }] */
    this.meldHistory = [];

    /** Per-suit discard counts */
    this.suitDiscardCounts = { character: 0, dot: 0, bamboo: 0, honor: 0 };

    /** Per-suit kept probability (0-1), higher = more likely to hold */
    this.suitProbabilities = { character: 0.25, dot: 0.25, bamboo: 0.25, honor: 0.25 };

    /** Estimated tenpai probability (0-1) */
    this.tenpaiProbability = 0;

    /** Known tiles this player has (from melds) — tile indices */
    this.knownTileIndices = new Set();

    /** Total turn count at last update */
    this.lastUpdateTurn = -1;
  }

  /**
   * Record a discard by this player.
   */
  recordDiscard(tileIdx, turn, fromDraw) {
    this.discardHistory.push({ tileIdx, turn, fromDraw });

    // Update suit counts
    const suit = tileToSuit(tileIdx);
    this.suitDiscardCounts[suit]++;

    // Update beliefs
    this._updateBeliefs(turn);
  }

  /**
   * Record a meld by this player.
   */
  recordMeld(type, tiles, turn) {
    this.meldHistory.push({ type, tiles: tiles.map(t => t.tileIdx || t), turn });

    for (const tile of tiles) {
      const idx = typeof tile === 'number' ? tile : tile.tileIdx;
      if (idx !== undefined) {
        this.knownTileIndices.add(idx);
      }
    }

    this._updateBeliefs(turn);
  }

  /**
   * Update beliefs based on accumulated observations.
   */
  _updateBeliefs(turn) {
    if (turn === this.lastUpdateTurn) return;
    this.lastUpdateTurn = turn;

    this._updateSuitProbabilities();
    this._updateTenpaiProbability(turn);
  }

  /**
   * Infer suit holding probabilities from discard patterns.
   *
   * Heuristic: early discards of a suit suggest the player doesn't need
   * that suit. Late discards of suited tiles when previously not discarding
   * that suit suggest the player is discarding excess tiles of their target suit.
   */
  _updateSuitProbabilities() {
    const totalDiscards = this.discardHistory.length;
    if (totalDiscards === 0) return;

    const total = totalDiscards || 1;
    const probs = { character: 0.25, dot: 0.25, bamboo: 0.25, honor: 0.25 };

    for (const suit of SUITS) {
      const discarded = this.suitDiscardCounts[suit];
      const suitCount = suit === 'honor' ? 28 : 36; // total tiles of this suit in deck

      // Base: equal probability
      // Adjustment: fewer discards of this suit → more likely holding it
      // More discards → less likely holding it
      const discardRatio = discarded / Math.max(total, 1);
      const expectedRatio = suitCount / 136;

      // If discarding MORE than expected → likely doesn't need this suit
      // If discarding LESS than expected → likely collecting this suit
      if (discardRatio > expectedRatio * 1.5) {
        probs[suit] = Math.max(0.05, 0.25 - (discardRatio - expectedRatio) * 2);
      } else if (discardRatio < expectedRatio * 0.5) {
        probs[suit] = Math.min(0.6, 0.25 + (expectedRatio - discardRatio) * 2);
      }
    }

    // Adjust based on melds: if player melded a suit, they're more likely collecting it
    for (const meld of this.meldHistory) {
      if (meld.tiles.length > 0) {
        const meldSuit = tileToSuit(typeof meld.tiles[0] === 'number' ? meld.tiles[0] : 0);
        if (meldSuit !== 'honor') {
          probs[meldSuit] = Math.min(0.8, probs[meldSuit] + 0.15);
        }
      }
    }

    // Normalize to sum to 1
    const sum = probs.character + probs.dot + probs.bamboo + probs.honor;
    for (const suit of SUITS) {
      this.suitProbabilities[suit] = probs[suit] / sum;
    }
  }

  /**
   * Estimate tenpai probability based on recent discard pattern changes.
   *
   * Signals:
   * 1. Discarding tiles from suits they previously kept → dangerous
   * 2. Discarding tiles that complete common sequences → tenpai
   * 3. Later in the game → higher base probability
   */
  _updateTenpaiProbability(currentTurn) {
    const history = this.discardHistory;
    if (history.length < 3) {
      this.tenpaiProbability = 0;
      return;
    }

    let signals = 0;
    let totalWeight = 0;

    // Signal 1: Tile danger level of recent discards
    // A tile has high "danger" if it's in a suit the player kept (low discard ratio)
    const recentDiscards = history.slice(-RECENT_DISCARD_WINDOW);
    const earlyDiscards = history.slice(0, Math.floor(history.length / 2));

    if (earlyDiscards.length > 0) {
      for (const recent of recentDiscards) {
        const recentSuit = tileToSuit(recent.tileIdx);
        // Check if this suit was previously kept (low discard count early on)
        const earlySuitDiscards = earlyDiscards.filter(d => tileToSuit(d.tileIdx) === recentSuit).length;
        const earlyRatio = earlySuitDiscards / earlyDiscards.length;
        const expectedRatio = (recentSuit === 'honor' ? 28 : 36) / 136;

        if (earlyRatio < expectedRatio * 0.5) {
          // Player previously kept this suit — now discarding it is a tenpai signal
          signals += 0.15;
        }
        totalWeight += 0.15;
      }
    }

    // Signal 2: Discarding middle tiles (rank 3-7) late in game
    // These are more valuable for sequences, discarding them suggests tenpai
    for (const recent of recentDiscards) {
      const rank = tileToRank(recent.tileIdx);
      if (rank >= 3 && rank <= 7) {
        signals += 0.1;
      }
      totalWeight += 0.1;
    }

    // Signal 3: Game progress — later turns = higher base probability
    const turnFactor = Math.min(1.0, currentTurn / 50);
    signals += turnFactor * 0.2;
    totalWeight += 0.2;

    // Signal 4: Number of melds — more melds = closer to winning
    const meldFactor = Math.min(1.0, this.meldHistory.length / 4);
    signals += meldFactor * 0.2;
    totalWeight += 0.2;

    this.tenpaiProbability = Math.min(0.95, totalWeight > 0 ? signals / Math.max(totalWeight, 0.01) : 0);
  }

  /**
   * Get the "danger level" of a tile for this opponent.
   * 0 = safe (opponent likely doesn't need it), 1 = very dangerous.
   */
  getTileDanger(tileIdx) {
    const suit = tileToSuit(tileIdx);

    // Tiles of suits the opponent is collecting are more dangerous
    const suitProb = this.suitProbabilities[suit];

    // If opponent is likely tenpai, all tiles are more dangerous
    const tenpaiBoost = this.tenpaiProbability * 0.3;

    // Honors are generally less dangerous (fewer wait patterns need specific honors)
    const honorPenalty = suit === 'honor' ? 0.3 : 0;

    return Math.min(1.0, Math.max(0.0, suitProb + tenpaiBoost - honorPenalty));
  }

  /**
   * Get a weighted random sample for determinization.
   * Returns an array of tile indices to assign to this opponent's hand.
   *
   * @param {TileSet} availablePool — tiles available for assignment
   * @param {number} count — number of tiles to assign
   * @param {number} wildIdx — wild tile index
   * @returns {number[]} assigned tile indices
   */
  sampleHand(availablePool, count, wildIdx) {
    const result = [];
    const pool = availablePool.clone();

    for (let i = 0; i < count; i++) {
      if (pool.total() === 0) break;

      // Compute weights for each tile type
      const weights = [];
      const candidates = [];

      const d = pool.data;
      for (let idx = 0; idx < 34; idx++) {
        if (d[idx] <= 0) continue;
        candidates.push(idx);
        weights.push(this._tileWeight(idx, wildIdx));
      }

      if (candidates.length === 0) break;

      // Weighted random selection
      const totalWeight = weights.reduce((a, b) => a + b, 0);
      let r = Math.random() * totalWeight;
      let chosen = candidates[0];

      for (let j = 0; j < candidates.length; j++) {
        r -= weights[j];
        if (r <= 0) {
          chosen = candidates[j];
          break;
        }
      }

      result.push(chosen);
      pool.remove(chosen);
    }

    return result;
  }

  /**
   * Weight for a tile type in determinization sampling.
   * Higher weight = more likely to be in this opponent's hand.
   */
  _tileWeight(tileIdx, wildIdx) {
    if (tileIdx === wildIdx) return 1.2; // wild tiles are slightly more likely (random)

    const suit = tileToSuit(tileIdx);
    const baseWeight = this.suitProbabilities[suit];

    // Known tiles from melds should not be re-assigned
    if (this.knownTileIndices.has(tileIdx)) return 0;

    // Bias: tiles that this opponent hasn't discarded are more likely
    const hasDiscarded = this.discardHistory.some(d => d.tileIdx === tileIdx);
    if (hasDiscarded) {
      // If they discarded this exact tile type, they're less likely to hold more
      return baseWeight * 0.3;
    }

    return baseWeight;
  }

  /**
   * Reset all beliefs.
   */
  reset() {
    this.discardHistory = [];
    this.meldHistory = [];
    this.suitDiscardCounts = { character: 0, dot: 0, bamboo: 0, honor: 0 };
    this.suitProbabilities = { character: 0.25, dot: 0.25, bamboo: 0.25, honor: 0.25 };
    this.tenpaiProbability = 0;
    this.knownTileIndices = new Set();
    this.lastUpdateTurn = -1;
  }
}

// ----------------------------------------------------------
// Tile suit/rank helpers
// ----------------------------------------------------------

function tileToSuit(idx) {
  if (idx < 9) return 'character';
  if (idx < 18) return 'dot';
  if (idx < 27) return 'bamboo';
  return 'honor';
}

function tileToRank(idx) {
  if (idx < 9) return idx + 1;
  if (idx < 18) return idx - 9 + 1;
  if (idx < 27) return idx - 18 + 1;
  return idx - 27 + 1;
}

// ----------------------------------------------------------
// MultiPlayerModel — tracks all opponents
// ----------------------------------------------------------

export class MultiPlayerModel {
  /**
   * @param {number} observerId — the AI player (don't need model for self)
   */
  constructor(observerId) {
    this.observerId = observerId;
    /** @type {Map<number, OpponentModel>} */
    this.models = new Map();
    for (let p = 0; p < 4; p++) {
      if (p !== observerId) {
        this.models.set(p, new OpponentModel(p));
      }
    }
  }

  /**
   * Record a discard by any player.
   */
  recordDiscard(playerId, tileIdx, turn, fromDraw = false) {
    const model = this.models.get(playerId);
    if (model) model.recordDiscard(tileIdx, turn, fromDraw);
  }

  /**
   * Record a meld by any player.
   */
  recordMeld(playerId, type, tiles, turn) {
    const model = this.models.get(playerId);
    if (model) model.recordMeld(type, tiles, turn);
  }

  /**
   * Get the opponent model for a specific player.
   */
  getModel(playerId) {
    return this.models.get(playerId);
  }

  /**
   * Get tenpai probability for a player.
   */
  getTenpaiProbability(playerId) {
    const model = this.models.get(playerId);
    return model ? model.tenpaiProbability : 0;
  }

  /**
   * Get danger level of a tile for a specific opponent.
   */
  getTileDanger(playerId, tileIdx) {
    const model = this.models.get(playerId);
    return model ? model.getTileDanger(tileIdx) : 0.5;
  }

  /**
   * Get the most dangerous opponent for a given tile.
   * @returns {{ playerId: number, danger: number }|null}
   */
  getMostDangerous(tileIdx) {
    let maxDanger = -1;
    let maxPlayer = -1;
    for (const [p, model] of this.models) {
      const danger = model.getTileDanger(tileIdx);
      if (danger > maxDanger) {
        maxDanger = danger;
        maxPlayer = p;
      }
    }
    return maxPlayer >= 0 ? { playerId: maxPlayer, danger: maxDanger } : null;
  }

  /**
   * Score all tile types by overall danger across all opponents.
   * @returns {Map<number, number>} tileIdx → max danger
   */
  scoreAllTiles() {
    const scores = new Map();
    for (let i = 0; i < 34; i++) {
      let maxDanger = 0;
      for (const [, model] of this.models) {
        const d = model.getTileDanger(i);
        if (d > maxDanger) maxDanger = d;
      }
      scores.set(i, maxDanger);
    }
    return scores;
  }

  /**
   * Determinize the game state with opponent-model-informed sampling.
   *
   * Rather than random assignment, this uses the opponent models to bias
   * tile assignments toward more likely distributions.
   *
   * @param {import('./game-sim.js').SimGameState} state
   * @returns {import('./game-sim.js').SimGameState}
   */
  determinizeBiased(state) {
    const det = state.clone();
    const observerId = this.observerId;
    const wildIdx = state.wildIdx;

    for (let p = 0; p < 4; p++) {
      if (p === observerId) continue;

      const model = this.models.get(p);
      const handSize = det.hands[p].total();
      if (handSize === 0) continue;

      // Clear opponent hand
      det.hands[p].clear();

      // Sample biased hand
      const assigned = model
        ? model.sampleHand(det.pool, handSize, wildIdx)
        : this._uniformSample(det.pool, handSize);

      for (const idx of assigned) {
        det.pool.remove(idx);
        det.hands[p].add(idx);
      }
    }

    return det;
  }

  _uniformSample(pool, count) {
    const result = [];
    for (let i = 0; i < count; i++) {
      const idx = drawRandom(pool);
      if (idx < 0) break;
      result.push(idx);
      pool.remove(idx);
    }
    return result;
  }

  /**
   * Reset all models (new game).
   */
  reset() {
    for (const [, model] of this.models) {
      model.reset();
    }
  }

  /**
   * Build from a UI game state — observe all visible information.
   * @param {object} game — engine.js GameState
   * @param {number} observerId
   */
  static fromGameState(game, observerId) {
    const mpm = new MultiPlayerModel(observerId);

    for (let p = 0; p < 4; p++) {
      if (p === observerId) continue;
      const player = game.players[p];
      const model = mpm.models.get(p);

      // Record discards
      for (let i = 0; i < player.discards.length; i++) {
        const tile = player.discards[i];
        // Import tileToIndex to convert
        const idx = tileTypeIndexFromGame(tile);
        if (idx >= 0) model.recordDiscard(idx, i, false);
      }

      // Record melds
      for (const meld of player.melds) {
        const tiles = meld.tiles.map(t => tileTypeIndexFromGame(t)).filter(i => i >= 0);
        model.recordMeld(meld.type, tiles, 999);
      }
    }

    return mpm;
  }
}

// ----------------------------------------------------------
// Determinization with opponent model bias
// ----------------------------------------------------------

/**
 * Enhanced determinization that uses opponent models to bias sampling.
 * Falls back to uniform determinization if no model is provided.
 *
 * @param {import('./game-sim.js').SimGameState} state
 * @param {number} observerId
 * @param {MultiPlayerModel} [oppModel]
 * @returns {import('./game-sim.js').SimGameState}
 */
export function determinizeBiased(state, observerId, oppModel) {
  if (oppModel) {
    return oppModel.determinizeBiased(state);
  }
  return determinize(state, observerId);
}

// ----------------------------------------------------------
// Tile index conversion from engine.js tile objects
// ----------------------------------------------------------

const HONORS = ['east', 'south', 'west', 'north', 'red', 'green', 'white'];

export function tileTypeIndexFromGame(tile) {
  if (!tile) return -1;
  if (tile.suit === 'character') return tile.rank - 1;
  if (tile.suit === 'dot') return 9 + (tile.rank - 1);
  if (tile.suit === 'bamboo') return 18 + (tile.rank - 1);
  if (tile.suit === 'honor') return 27 + HONORS.indexOf(tile.rank);
  return -1;
}

// ----------------------------------------------------------
// Export OpponentModel for testing
// ----------------------------------------------------------

export { OpponentModel };
