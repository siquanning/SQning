// ============================================================
// game-sim.js — Fast game simulator for ISMCTS rollouts
// ============================================================
// Compact Uint8Array(34) representation for all tile sets.
// All win/shanten checks operate directly on integer indices 0-33.
// Target: >2000 rollouts/sec for ISMCTS search.
// ============================================================

// ----------------------------------------------------------
// Tile type indexing (0-33, matches engine.js TILE_KEYS_IN_ORDER)
// ----------------------------------------------------------
// 0-8:   character 1-9
// 9-17:  dot 1-9
// 18-26: bamboo 1-9
// 27-33: honor (east, south, west, north, red, green, white)

const HONORS = ['east', 'south', 'west', 'north', 'red', 'green', 'white'];

/** suit+rank → tile type index 0-33 */
export function tileTypeIndex(suit, rank) {
  if (suit === 'character') return rank - 1;
  if (suit === 'dot') return 9 + (rank - 1);
  if (suit === 'bamboo') return 18 + (rank - 1);
  if (suit === 'honor') return 27 + HONORS.indexOf(rank);
  return -1;
}

/** tile type index 0-33 → { suit, rank } */
export function indexToTileType(idx) {
  if (idx < 9) return { suit: 'character', rank: idx + 1 };
  if (idx < 18) return { suit: 'dot', rank: idx - 9 + 1 };
  if (idx < 27) return { suit: 'bamboo', rank: idx - 18 + 1 };
  return { suit: 'honor', rank: HONORS[idx - 27] };
}

/** Convert a tile object {suit, rank} to its type index */
export function tileToIndex(tile) {
  return tileTypeIndex(tile.suit, tile.rank);
}

// ----------------------------------------------------------
// Full deck constant
// ----------------------------------------------------------

/** Full mahjong deck: 4 copies of each of the 34 tile types */
const FULL_DECK = new Uint8Array(34);
for (let i = 0; i < 34; i++) FULL_DECK[i] = 4;

/** Iteration order for deterministic traversal of tile types */
const TILE_ORDER = Array.from({ length: 34 }, (_, i) => i);

// Suit start indices for sequence checking
const SUIT_START = [0, 9, 18]; // character, dot, bamboo
const SUIT_END = [8, 17, 26];

// ----------------------------------------------------------
// TileSet — compact tile count representation
// ----------------------------------------------------------

export class TileSet {
  constructor() {
    /** @type {Uint8Array} length 34, each entry 0-4 */
    this.data = new Uint8Array(34);
  }

  clone() {
    const ts = new TileSet();
    ts.data = new Uint8Array(this.data);
    return ts;
  }

  add(idx) {
    this.data[idx]++;
  }

  remove(idx) {
    this.data[idx]--;
  }

  count(idx) {
    return this.data[idx];
  }

  total() {
    let s = 0;
    const d = this.data;
    for (let i = 0; i < 34; i++) s += d[i];
    return s;
  }

  /** Set all counts to 0 */
  clear() {
    this.data.fill(0);
  }

  /** Copy counts from another TileSet */
  copyFrom(other) {
    this.data.set(other.data);
  }

  /** Add all tiles from another TileSet */
  addAll(other) {
    const d = this.data;
    const od = other.data;
    for (let i = 0; i < 34; i++) d[i] += od[i];
  }
}

// ----------------------------------------------------------
// SimGameState — minimal game state for simulation
// ----------------------------------------------------------

export class SimGameState {
  constructor() {
    /** Wall/pool: tiles not yet drawn */
    this.pool = new TileSet();
    /** Hands: one TileSet per player (0-3) */
    this.hands = [new TileSet(), new TileSet(), new TileSet(), new TileSet()];
    /** Melds: one TileSet per player (tiles consumed in melds) */
    this.melds = [new TileSet(), new TileSet(), new TileSet(), new TileSet()];
    /** Discards: one TileSet per player */
    this.discards = [new TileSet(), new TileSet(), new TileSet(), new TileSet()];
    /** Meld structure info for scoring context */
    this.meldInfos = [[], [], [], []];
    /** Current turn player (0-3) */
    this.turn = 0;
    /** Dealer (0-3) */
    this.dealer = 0;
    /** Tile type index of the wild/财神 tile */
    this.wildIdx = 33; // default: honor:white
    /** Number of wild tiles each player has */
    this.handWildCounts = [0, 0, 0, 0];
    /** Piao cai status */
    this.piaoCai = [false, false, false, false];
    /** Number of discards made so far (for tianhu/dihu) */
    this.discardCount = 0;
    /** Round number (increments on each draw) */
    this.roundCount = 0;
    /** Winner (set at end of simulation) */
    this.winner = -1;
    /** How the winner won: "自摸" | "点炮" | "杠开" | null */
    this.winKind = null;
    /** Who dealt into the winning tile (for 点炮) */
    this.fromPlayer = -1;
    /** The winning tile index */
    this.winningTileIdx = -1;
    /** Draw reason (流局) */
    this.drawReason = null;
  }

  /** Deep clone — critical for MCTS tree node expansion */
  clone() {
    const gs = new SimGameState();
    gs.pool.copyFrom(this.pool);
    for (let i = 0; i < 4; i++) {
      gs.hands[i].copyFrom(this.hands[i]);
      gs.melds[i].copyFrom(this.melds[i]);
      gs.discards[i].copyFrom(this.discards[i]);
      gs.meldInfos[i] = this.meldInfos[i].map(m => ({ ...m }));
    }
    gs.turn = this.turn;
    gs.dealer = this.dealer;
    gs.wildIdx = this.wildIdx;
    gs.handWildCounts = [...this.handWildCounts];
    gs.piaoCai = [...this.piaoCai];
    gs.discardCount = this.discardCount;
    gs.roundCount = this.roundCount;
    gs.winner = this.winner;
    gs.winKind = this.winKind;
    gs.fromPlayer = this.fromPlayer;
    gs.winningTileIdx = this.winningTileIdx;
    gs.drawReason = this.drawReason;
    return gs;
  }

  /** Total tiles in a player's hand */
  handSize(playerId) {
    return this.hands[playerId].total();
  }

  /** Total tiles remaining in wall */
  poolSize() {
    return this.pool.total();
  }

  /** Build the remaining pool: full deck minus all visible/consumed tiles */
  computeRemainingPool() {
    const pdata = this.pool.data;
    for (let i = 0; i < 34; i++) {
      let used = 0;
      for (let p = 0; p < 4; p++) {
        used += this.hands[p].data[i] + this.melds[p].data[i] + this.discards[p].data[i];
      }
      pdata[i] = FULL_DECK[i] - used;
    }
  }

  /** Verify invariant: pool + all hands + all melds + all discards = FULL_DECK */
  verifyInvariant() {
    const total = new Uint8Array(34);
    total.set(this.pool.data);
    for (let p = 0; p < 4; p++) {
      for (let i = 0; i < 34; i++) {
        total[i] += this.hands[p].data[i] + this.melds[p].data[i] + this.discards[p].data[i];
      }
    }
    for (let i = 0; i < 34; i++) {
      if (total[i] !== 4) return false;
    }
    return true;
  }
}

// ----------------------------------------------------------
// Initialization helpers
// ----------------------------------------------------------

/**
 * Create SimGameState from the UI game state (engine.js format).
 * @param {object} game - The full game state from engine.js
 * @returns {SimGameState}
 */
export function fromGameState(game) {
  const sim = new SimGameState();

  sim.turn = game.turn;
  sim.dealer = game.dealer;
  sim.wildIdx = tileTypeIndex(game.wildTile.suit, game.wildTile.rank);
  sim.discardCount = game._discardCount || 0;

  for (let p = 0; p < 4; p++) {
    const player = game.players[p];
    for (const tile of player.hand) {
      const idx = tileToIndex(tile);
      sim.hands[p].add(idx);
      if (idx === sim.wildIdx) sim.handWildCounts[p]++;
    }
    for (const meld of player.melds) {
      sim.meldInfos[p].push({ type: meld.type, concealed: meld.concealed });
      for (const tile of meld.tiles) {
        sim.melds[p].add(tileToIndex(tile));
      }
    }
    for (const tile of player.discards) {
      sim.discards[p].add(tileToIndex(tile));
    }
    sim.piaoCai[p] = player.piaoCai;
  }

  // Enumerate remaining pool tiles from the UI game's tilePool
  for (const tile of game.tilePool) {
    sim.pool.add(tileToIndex(tile));
  }

  return sim;
}

// ----------------------------------------------------------
// Fast win check on Uint8Array count data
// ----------------------------------------------------------

/**
 * Memoized check: can remaining tiles form `neededSets` melds using `wildCount` wilds?
 * Operates on raw Uint8Array counts (non-wild tiles only).
 * Mutates counts temporarily, restores before returning.
 */
function canFormSets(counts, wildCount, neededSets, memo) {
  if (neededSets === 0) return true;
  if (wildCount < 0) return false;

  let firstIdx = -1;
  for (const idx of TILE_ORDER) {
    if (counts[idx] > 0) { firstIdx = idx; break; }
  }

  if (firstIdx === -1) {
    return wildCount >= neededSets * 3;
  }

  if (!memo) memo = new Map();
  const memoKey = _memoKey(counts, wildCount, neededSets);
  const cached = memo.get(memoKey);
  if (cached !== undefined) return cached;

  const cnt = counts[firstIdx];

  // Try triplet
  if (cnt + wildCount >= 3) {
    const use = Math.min(cnt, 3);
    const needWild = 3 - use;
    counts[firstIdx] = cnt - use;
    if (canFormSets(counts, wildCount - needWild, neededSets - 1, memo)) {
      counts[firstIdx] = cnt;
      memo.set(memoKey, true);
      return true;
    }
    counts[firstIdx] = cnt;
  }

  // Try sequence (only suited tiles, idx 0-26)
  if (firstIdx < 27) {
    const suitBase = Math.floor(firstIdx / 9) * 9;
    const rank = firstIdx - suitBase + 1;
    if (rank <= 7) {
      const i2 = firstIdx + 1;
      const i3 = firstIdx + 2;
      const c2 = counts[i2] || 0;
      const c3 = counts[i3] || 0;
      const needWild = (c2 === 0 ? 1 : 0) + (c3 === 0 ? 1 : 0);

      if (wildCount >= needWild) {
        counts[firstIdx] = cnt - 1;
        if (c2 > 0) counts[i2] = c2 - 1;
        if (c3 > 0) counts[i3] = c3 - 1;

        if (canFormSets(counts, wildCount - needWild, neededSets - 1, memo)) {
          counts[firstIdx] = cnt;
          if (c2 > 0) counts[i2] = c2;
          if (c3 > 0) counts[i3] = c3;
          memo.set(memoKey, true);
          return true;
        }

        counts[firstIdx] = cnt;
        if (c2 > 0) counts[i2] = c2;
        if (c3 > 0) counts[i3] = c3;
      }
    }
  }

  memo.set(memoKey, false);
  return false;
}

function _memoKey(counts, wildCount, neededSets) {
  // Encode full count vector preserving positional info
  // String.fromCharCode is ~3x faster than hex building
  let s = '';
  for (let i = 0; i < 34; i++) {
    s += String.fromCharCode(48 + counts[i]); // '0'-'4'
  }
  return s + 'W' + wildCount + 'N' + neededSets;
}

/**
 * Standard win: 4 melds + 1 pair. Works on TileSet hand data.
 * @param {TileSet} handSet
 * @param {number} wildIdx
 * @returns {boolean}
 */
export function isStandardWinSet(handSet, wildIdx) {
  const total = handSet.total();
  if (total !== 14) return false;

  const counts = new Uint8Array(handSet.data);
  const wildCount = handSet.data[wildIdx];
  counts[wildIdx] = 0;

  const memo = new Map();

  if (wildCount === 0) {
    for (const idx of TILE_ORDER) {
      const c = counts[idx];
      if (c >= 2) {
        counts[idx] = c - 2;
        if (canFormSets(counts, 0, 4, memo)) {
          return true;
        }
        counts[idx] = c;
      }
    }
    return false;
  }

  // wildCount >= 1: wild can be used in melds or in pair
  // Option A: natural pair, all wilds used in melds
  for (const idx of TILE_ORDER) {
    const c = counts[idx];
    if (c >= 2) {
      counts[idx] = c - 2;
      if (canFormSets(counts, wildCount, 4, memo)) {
        counts[idx] = c;
        return true;
      }
      counts[idx] = c;
    }
  }

  // Option B: 1 wild + 1 natural = pair
  for (const idx of TILE_ORDER) {
    const c = counts[idx];
    if (c >= 1) {
      counts[idx] = c - 1;
      if (canFormSets(counts, wildCount - 1, 4, memo)) {
        counts[idx] = c;
        return true;
      }
      counts[idx] = c;
    }
  }

  // Option C: 2 wilds = pair (only if wildCount >= 2)
  if (wildCount >= 2 && canFormSets(counts, wildCount - 2, 4, memo)) {
    return true;
  }

  return false;
}

/**
 * Seven pairs check on TileSet.
 */
export function isSevenPairsSet(handSet, wildIdx) {
  if (handSet.total() !== 14) return false;

  const d = handSet.data;
  const wildCount = d[wildIdx];
  let singles = 0;

  for (let i = 0; i < 34; i++) {
    if (i === wildIdx) continue;
    if (d[i] % 2 !== 0) singles++;
  }

  if (wildCount < singles) return false;
  if ((wildCount - singles) % 2 !== 0) return false;
  return true;
}

/**
 * Full win check (standard + seven pairs).
 * When meldSet provided, combines hand + meld tiles. Handles:
 *  - 14 total: normal win check
 *  - 15 total (post-gang): try removing each tile, check remaining 14
 * @param {TileSet} handSet
 * @param {number} wildIdx
 * @param {TileSet} [meldSet]
 * @returns {boolean}
 */
export function isWinningHandSet(handSet, wildIdx, meldSet) {
  const handTotal = handSet.total();
  if (!meldSet || meldSet.total() === 0) {
    if (handTotal !== 14) return false;
    return isStandardWinSet(handSet, wildIdx) || isSevenPairsSet(handSet, wildIdx);
  }

  const total = handTotal + meldSet.total();
  if (total !== 14 && total !== 15) return false;

  // Combine hand + melds
  const combined = handSet.clone();
  for (let i = 0; i < 34; i++) {
    for (let j = 0; j < meldSet.data[i]; j++) combined.add(i);
  }

  if (total === 14) {
    return isStandardWinSet(combined, wildIdx) || isSevenPairsSet(combined, wildIdx);
  }

  // Post-gang: 15 total, try removing one tile to reach winning 14
  for (let i = 0; i < 34; i++) {
    if (combined.data[i] > 0) {
      combined.data[i]--;
      if (isStandardWinSet(combined, wildIdx) || isSevenPairsSet(combined, wildIdx)) {
        return true;
      }
      combined.data[i]++;
    }
  }
  return false;
}

// ----------------------------------------------------------
// Shanten calculation
// ----------------------------------------------------------

/**
 * Compute the maximum (melds, partials) achievable from count data.
 * A partial is: a pair (2 tiles), or 2-of-a-triplet, or 2 consecutive suited tiles.
 * Returns [melds, partials].
 */
function evaluateMelds(counts, wildCount, memo) {
  if (wildCount < 0) return [0, 0];

  let firstIdx = -1;
  for (const idx of TILE_ORDER) {
    if (counts[idx] > 0) { firstIdx = idx; break; }
  }

  if (firstIdx === -1) {
    const meldsFromWild = Math.floor(wildCount / 3);
    const partialsFromWild = wildCount % 3 >= 2 ? 1 : 0;
    return [meldsFromWild, partialsFromWild];
  }

  if (!memo) memo = new Map();
  const memoKey = _memoKey(counts, wildCount, 4) + '_eval';
  const cached = memo.get(memoKey);
  if (cached) return cached;

  const cnt = counts[firstIdx];
  let bestMelds = 0, bestPartials = 0;

  // Option A: Form a complete triplet
  if (cnt + wildCount >= 3) {
    const use = Math.min(cnt, 3);
    const needWild = 3 - use;
    counts[firstIdx] = cnt - use;
    const [m, p] = evaluateMelds(counts, wildCount - needWild, memo);
    counts[firstIdx] = cnt;
    const score = (m + 1) * 10 + p;
    const bestScore = bestMelds * 10 + bestPartials;
    if (score > bestScore) {
      bestMelds = m + 1;
      bestPartials = p;
    }
  }

  // Option B: Form a partial pair (2 of this tile)
  if (cnt + wildCount >= 2) {
    const use = Math.min(cnt, 2);
    const needWild = 2 - use;
    counts[firstIdx] = cnt - use;
    const [m, p] = evaluateMelds(counts, wildCount - needWild, memo);
    counts[firstIdx] = cnt;
    const score = m * 10 + (p + 1);
    const bestScore = bestMelds * 10 + bestPartials;
    if (score > bestScore) {
      bestMelds = m;
      bestPartials = p + 1;
    }
  }

  // Option C: Form a complete sequence (suited tiles only)
  if (firstIdx < 27) {
    const suitBase = Math.floor(firstIdx / 9) * 9;
    const rank = firstIdx - suitBase + 1;
    if (rank <= 7) {
      const i2 = firstIdx + 1;
      const i3 = firstIdx + 2;
      const c2 = counts[i2] || 0;
      const c3 = counts[i3] || 0;
      const needWild = (c2 === 0 ? 1 : 0) + (c3 === 0 ? 1 : 0);

      if (wildCount >= needWild) {
        counts[firstIdx] = cnt - 1;
        if (c2 > 0) counts[i2] = c2 - 1;
        if (c3 > 0) counts[i3] = c3 - 1;

        const [m, p] = evaluateMelds(counts, wildCount - needWild, memo);
        counts[firstIdx] = cnt;
        if (c2 > 0) counts[i2] = c2;
        if (c3 > 0) counts[i3] = c3;

        const score = (m + 1) * 10 + p;
        const bestScore = bestMelds * 10 + bestPartials;
        if (score > bestScore) {
          bestMelds = m + 1;
          bestPartials = p;
        }
      }
    }

    // Option D: Form a partial sequence (2 consecutive)
    if (rank <= 8) {
      const i2 = firstIdx + 1;
      const c2 = counts[i2] || 0;
      if (c2 + wildCount >= 1) {
        const needWild = c2 === 0 ? 1 : 0;
        counts[firstIdx] = cnt - 1;
        if (c2 > 0) counts[i2] = c2 - 1;

        const [m, p] = evaluateMelds(counts, wildCount - needWild, memo);
        counts[firstIdx] = cnt;
        if (c2 > 0) counts[i2] = c2;

        const score = m * 10 + (p + 1);
        const bestScore = bestMelds * 10 + bestPartials;
        if (score > bestScore) {
          bestMelds = m;
          bestPartials = p + 1;
        }
      }
    }
  }

  // Option E: Skip this tile (ignore it)
  {
    const saved = counts[firstIdx];
    counts[firstIdx] = 0;
    const [m, p] = evaluateMelds(counts, wildCount, memo);
    counts[firstIdx] = saved;
    const score = m * 10 + p;
    const bestScore = bestMelds * 10 + bestPartials;
    if (score > bestScore) {
      bestMelds = m;
      bestPartials = p;
    }
  }

  const result = [bestMelds, bestPartials];
  memo.set(memoKey, result);
  return result;
}

/** Memoization cache for shanten values, shared across calls */
const shantenMemo = new Map();

/**
 * Compute shanten number for a hand represented by a TileSet.
 * @param {TileSet} handSet
 * @param {number} wildIdx
 * @returns {number} -1 = winning, 0 = tenpai, 1+ = shanten number
 */
export function computeShanten(handSet, wildIdx) {
  const total = handSet.total();
  if (total === 14 && isWinningHandSet(handSet, wildIdx)) return -1;

  const wildCount = handSet.data[wildIdx];
  const counts = new Uint8Array(handSet.data);
  counts[wildIdx] = 0;

  // Build memo key for caching
  const ck = _memoKey(counts, wildCount, 4) + '_s';
  const cached = shantenMemo.get(ck);
  if (cached !== undefined) return cached;

  // Seven pairs shanten
  let pairs = 0;
  let w = wildCount;
  for (let i = 0; i < 34; i++) {
    if (i === wildIdx) continue;
    pairs += Math.floor(counts[i] / 2);
  }
  let remainingSingles = 0;
  for (let i = 0; i < 34; i++) {
    if (i === wildIdx) continue;
    if (counts[i] % 2 !== 0) remainingSingles++;
  }
  if (w >= remainingSingles) {
    w -= remainingSingles;
    pairs += remainingSingles;
    pairs += Math.floor(w / 2);
  } else {
    pairs += w;
  }
  const shanten7 = Math.max(0, 6 - pairs);

  // Standard shanten: try each possible pair source
  let minShanten = 8;

  // No pair case
  {
    const [m, p] = evaluateMelds(counts, wildCount, null);
    const s = 8 - 2 * m - Math.min(p, 4 - m);
    if (s < minShanten) minShanten = Math.max(-1, s);
  }

  // Try each tile type as pair (including wild-involved pairs)
  for (let pairIdx = -1; pairIdx < 34; pairIdx++) {
    let w = wildCount;
    const saved = pairIdx >= 0 ? counts[pairIdx] : 0;

    if (pairIdx === -1) {
      if (w < 2) continue;
      w -= 2;
    } else if (pairIdx === wildIdx) {
      continue; // already handled by wildCount
    } else {
      const c = counts[pairIdx];
      if (c + w < 2) continue;
      if (c >= 2) {
        counts[pairIdx] = c - 2;
      } else {
        counts[pairIdx] = 0;
        w -= (2 - c);
      }
    }

    const [m, p] = evaluateMelds(counts, w, null);
    const s = 8 - 2 * m - Math.min(p, 4 - m);
    if (s < minShanten) minShanten = Math.max(-1, s);

    if (pairIdx >= 0) counts[pairIdx] = saved;
  }

  const result = Math.min(minShanten, shanten7);
  shantenMemo.set(ck, result);
  return result;
}

/**
 * Compute shanten after discarding a specific tile from a hand.
 * @param {TileSet} handSet - 14-tile hand
 * @param {number} discardIdx - tile type index to discard
 * @param {number} wildIdx
 * @returns {number} shanten of the 13-tile hand after discard
 */
export function shantenAfterDiscard(handSet, discardIdx, wildIdx) {
  const d = handSet.data;
  if (d[discardIdx] <= 0) return 99;

  d[discardIdx]--;
  const s = computeShanten(handSet, wildIdx);
  d[discardIdx]++;
  return s;
}

// ----------------------------------------------------------
// Greedy action selection
// ----------------------------------------------------------

/**
 * Pick the best discard using greedy shanten minimization.
 * @param {SimGameState} state
 * @param {number} playerId
 * @returns {{ tileIdx: number, shanten: number }} the best discard
 */
export function chooseBestDiscard(state, playerId) {
  const hand = state.hands[playerId];
  const wildIdx = state.wildIdx;
  let bestIdx = -1;
  let bestShanten = 99;
  let bestScore = -1; // tiebreaker: higher = better to discard

  for (let i = 0; i < 34; i++) {
    if (hand.data[i] <= 0 || i === wildIdx) continue; // don't discard wild tiles
    const s = shantenAfterDiscard(hand, i, wildIdx);
    const score = tileDiscardScore(i, hand);
    if (s < bestShanten || (s === bestShanten && score > bestScore)) {
      bestShanten = s;
      bestScore = score;
      bestIdx = i;
    }
  }

  // Fallback: discard the first non-wild tile
  if (bestIdx === -1) {
    for (let i = 0; i < 34; i++) {
      if (hand.data[i] > 0 && i !== wildIdx) {
        return { tileIdx: i, shanten: computeShanten(hand, wildIdx) };
      }
    }
  }

  return { tileIdx: bestIdx, shanten: bestShanten };
}

/**
 * Tile discard priority score (tiebreaker when shanten is equal).
 * Higher score = better to discard.
 * Honors (isolated) > terminals (isolated) > middle tiles.
 * Penalizes breaking pairs/triplets.
 */
function tileDiscardScore(tileIdx, hand) {
  const count = hand.data[tileIdx];
  let score = 0;

  // Heavily penalize breaking pairs (2) or triplets (3+)
  if (count >= 3) score -= 30;
  else if (count >= 2) score -= 20;

  // Honor tiles (27-33): prefer discarding isolated ones
  if (tileIdx >= 27) {
    score += 10;
  }
  // Terminals (1 or 9): prefer over middle tiles
  else if (tileIdx % 9 === 0 || tileIdx % 9 === 8) {
    score += 5;
  }
  // Middle tiles (2-8): baseline

  return score;
}

// ----------------------------------------------------------
// Rollout
// ----------------------------------------------------------

/**
 * Randomly draw a tile from the pool (weighted by count).
 * @param {TileSet} pool
 * @returns {number} tile type index drawn, or -1 if empty
 */
export function drawRandom(pool) {
  const total = pool.total();
  if (total === 0) return -1;

  let r = Math.floor(Math.random() * total);
  const d = pool.data;
  for (let i = 0; i < 34; i++) {
    r -= d[i];
    if (r < 0) return i;
  }
  // Fallback: find first non-zero
  for (let i = 0; i < 34; i++) {
    if (d[i] > 0) return i;
  }
  return -1;
}

/**
 * Run a greedy rollout from the current state to a terminal state.
 * All players use the greedy (min-shanten) policy.
 *
 * @param {SimGameState} state - starting state (will be mutated!)
 * @returns {SimGameState} the same state object, now in terminal state
 */
export function rollout(state) {
  let safety = 200; // prevent infinite loops
  const wildIdx = state.wildIdx;

  while (safety-- > 0) {
    // Check terminal conditions
    if (state.winner >= 0) break;
    if (state.drawReason) break;

    const playerId = state.turn;
    const hand = state.hands[playerId];

    // If piaoCai, auto-discard drawn tile
    if (state.piaoCai[playerId]) {
      // Draw
      if (state.pool.total() === 0) {
        state.drawReason = '牌已摸完，本局流局。';
        break;
      }
      const drawn = drawRandom(state.pool);
      state.pool.remove(drawn);
      hand.add(drawn);
      state.roundCount++;

      if (isWinningHandSet(hand, wildIdx, state.melds[playerId])) {
        state.winner = playerId;
        state.winKind = '飘财';
        state.winningTileIdx = drawn;
        break;
      }

      // Auto-discard the drawn tile
      hand.remove(drawn);
      state.discards[playerId].add(drawn);
      state.discardCount++;

      // Check if anyone wants to claim
      const claims = collectClaims(state, playerId, drawn);
      if (claims.length > 0) {
        const bestClaim = resolveClaimGreedy(state, claims, playerId);
        if (bestClaim) {
          executeClaim(state, bestClaim, playerId, drawn);
          continue;
        }
      }

      state.turn = (playerId + 1) % 4;
      continue;
    }

    // Normal turn: draw
    if (state.pool.total() === 0) {
      state.drawReason = '牌已摸完，本局流局。';
      break;
    }
    const drawn = drawRandom(state.pool);
    state.pool.remove(drawn);
    hand.add(drawn);
    state.roundCount++;

    // Check win after draw
    if (isWinningHandSet(hand, wildIdx, state.melds[playerId])) {
      state.winner = playerId;
      state.winKind = '自摸';
      state.winningTileIdx = drawn;
      break;
    }

    // Greedy discard
    const { tileIdx } = chooseBestDiscard(state, playerId);
    if (tileIdx < 0) {
      state.drawReason = '无法出牌，流局。';
      break;
    }

    hand.remove(tileIdx);
    state.discards[playerId].add(tileIdx);
    state.discardCount++;

    // Check claims
    const claims = collectClaims(state, playerId, tileIdx);
    if (claims.length > 0) {
      const bestClaim = resolveClaimGreedy(state, claims, playerId);
      if (bestClaim) {
        executeClaim(state, bestClaim, playerId, tileIdx);
        continue;
      }
    }

    state.turn = (playerId + 1) % 4;
  }

  if (safety <= 0) {
    state.drawReason = '模拟超时，流局。';
  }

  return state;
}

/**
 * Collect all possible claims on a discarded tile.
 */
function collectClaims(state, discarderId, tileIdx) {
  const claims = [];
  const wildIdx = state.wildIdx;

  const nextPlayerId = (discarderId + 1) % 4;
  if (!state.piaoCai[nextPlayerId]) {
    const chiOptions = getChiOptionsSim(state.hands[nextPlayerId], tileIdx, wildIdx);
    for (const chiPair of chiOptions) {
      claims.push({ playerId: nextPlayerId, type: 'chi', priority: 1, chiPair });
    }
  }

  for (let offset = 1; offset <= 3; offset++) {
    const playerId = (discarderId + offset) % 4;
    if (state.piaoCai[playerId]) continue;
    const hand = state.hands[playerId];
    const cnt = hand.data[tileIdx];
    if (cnt >= 2) {
      const type = cnt >= 3 ? 'gang' : 'peng';
      claims.push({ playerId, type, priority: type === 'gang' ? 2.5 : 2 });
    }
  }

  return claims;
}

/**
 * Get chi options for a tile in simulation.
 */
function getChiOptionsSim(handSet, tileIdx, wildIdx) {
  if (tileIdx >= 27) return []; // honor can't chi

  const suitBase = Math.floor(tileIdx / 9) * 9;
  const rank = tileIdx - suitBase + 1;
  const d = handSet.data;

  // 吃只能吃两口：顺吃（弃牌最左）和中吃（弃牌中间）
  const patterns = [];
  if (rank + 2 <= 9) patterns.push([tileIdx + 1, tileIdx + 2]);
  if (rank - 1 >= 1 && rank + 1 <= 9) patterns.push([tileIdx - 1, tileIdx + 1]);

  const options = [];
  for (const [i1, i2] of patterns) {
    if (d[i1] > 0 && d[i2] > 0) {
      options.push([i1, i2]);
    }
  }
  return options;
}

/**
 * Resolve claims greedily: pick the claim that improves shanten the most.
 * Returns the best claim object, or null if no claim is beneficial.
 */
function resolveClaimGreedy(state, claims, discarderId) {
  if (claims.length === 0) return null;

  // Sort by priority (gang > peng > chi), then by shanten improvement
  claims.sort((a, b) => b.priority - a.priority);

  // For greedy rollout: always accept the highest-priority claim
  // Peng/gang convert a pair into a meld, which is always beneficial
  // Chi is accepted if it doesn't worsen the hand structure
  for (const claim of claims) {
    const hand = state.hands[claim.playerId];
    const wildIdx = state.wildIdx;

    if (claim.type === 'gang' || claim.type === 'peng') {
      // Always accept peng/gang in greedy rollout — converting pair→meld is beneficial
      return claim;
    }

    if (claim.type === 'chi') {
      if (!claim.chiPair) continue;
      // Verify player has the needed tiles
      for (const idx of claim.chiPair) {
        if (hand.data[idx] === 0) continue; // shouldn't happen in well-formed claims, but be safe
      }
      return claim;
    }
  }

  return null;
}

/**
 * Execute a claim in the simulation state.
 */
function executeClaim(state, claim, discarderId, tileIdx) {
  const playerId = claim.playerId;
  const hand = state.hands[playerId];
  const wildIdx = state.wildIdx;

  if (claim.type === 'peng') {
    hand.remove(tileIdx);
    hand.remove(tileIdx);
    state.melds[playerId].add(tileIdx);
    state.melds[playerId].add(tileIdx);
    state.melds[playerId].add(tileIdx); // 3 copies in meld
    state.discards[discarderId].remove(tileIdx); // remove from discarder's river
    state.meldInfos[playerId].push({ type: 'peng', concealed: false });
    // Player must now discard
    const { tileIdx: discard } = chooseBestDiscard(state, playerId);
    if (discard >= 0) {
      hand.remove(discard);
      state.discards[playerId].add(discard);
      state.discardCount++;
    }
    state.turn = playerId;
  } else if (claim.type === 'gang') {
    hand.remove(tileIdx);
    hand.remove(tileIdx);
    hand.remove(tileIdx);
    state.melds[playerId].add(tileIdx);
    state.melds[playerId].add(tileIdx);
    state.melds[playerId].add(tileIdx);
    state.melds[playerId].add(tileIdx);
    state.discards[discarderId].remove(tileIdx);
    state.meldInfos[playerId].push({ type: 'gang', concealed: false });
    // Draw replacement tile
    if (state.pool.total() > 0) {
      const drawn = drawRandom(state.pool);
      state.pool.remove(drawn);
      hand.add(drawn);

      if (isWinningHandSet(hand, wildIdx, state.melds[playerId])) {
        state.winner = playerId;
        state.winKind = '杠开';
        state.winningTileIdx = drawn;
        return;
      }

      const { tileIdx: discard } = chooseBestDiscard(state, playerId);
      if (discard >= 0) {
        hand.remove(discard);
        state.discards[playerId].add(discard);
        state.discardCount++;
      }
    }
    state.turn = playerId;
  } else if (claim.type === 'chi') {
    const chiPair = claim.chiPair;
    if (!chiPair) return;
    for (const idx of chiPair) {
      hand.remove(idx);
    }
    state.melds[playerId].add(chiPair[0]);
    state.melds[playerId].add(chiPair[1]);
    state.melds[playerId].add(tileIdx);
    state.discards[discarderId].remove(tileIdx);
    state.meldInfos[playerId].push({ type: 'chi', concealed: false });
    const { tileIdx: discard } = chooseBestDiscard(state, playerId);
    if (discard >= 0) {
      hand.remove(discard);
      state.discards[playerId].add(discard);
      state.discardCount++;
    }
    state.turn = playerId;
  }
}

// ----------------------------------------------------------
// Determinization helpers (for ISMCTS step 6b)
// ----------------------------------------------------------

/**
 * Given a SimGameState and the current player's perspective,
 * randomly assign unseen tiles to opponent hands and shuffle the wall.
 * Returns a new SimGameState (does not mutate input).
 *
 * @param {SimGameState} state
 * @param {number} observerId - the AI player whose perspective we use
 * @returns {SimGameState} determinized state
 */
export function determinize(state, observerId) {
  const det = state.clone();

  // Pool contains all unseen tiles (opponent hands + wall)
  // We need to keep only the wall tiles in the pool and assign
  // proper hands to opponents
  // In the initial call, opponent hands are empty and all unseen
  // tiles are in pool. We randomly assign to opponents.

  const wildIdx = state.wildIdx;

  // Collect opponent hand sizes, then clear and reassign randomly
  const oppSizes = [];
  for (let p = 0; p < 4; p++) {
    if (p === observerId) continue;
    const size = det.hands[p].total();
    if (size > 0) oppSizes.push({ playerId: p, size });
  }

  // Clear opponent hands and reassign from pool
  for (const { playerId, size } of oppSizes) {
    det.hands[playerId].clear();
    for (let i = 0; i < size; i++) {
      const drawn = drawRandom(det.pool);
      if (drawn < 0) break;
      det.pool.remove(drawn);
      det.hands[playerId].add(drawn);
      if (drawn === wildIdx) det.handWildCounts[playerId]++;
    }
  }

  // Shuffle remaining pool (for wall order)
  // The pool is already a multiset; random draw during rollout
  // effectively shuffles it.

  return det;
}

// ----------------------------------------------------------
// Simulation result
// ----------------------------------------------------------

/**
 * Compute reward from rollout result.
 * @param {SimGameState} state - terminal state
 * @param {number} playerId - the AI player's perspective
 * @returns {number} 1 = win, 0 = loss, 0.5 = draw
 */
export function rolloutReward(state, playerId) {
  if (state.drawReason) return 0.5; // draw
  if (state.winner === playerId) return 1.0; // win
  return 0.0; // loss
}

/**
 * Convenience: run N rollouts from a state and return win rate.
 * @param {SimGameState} baseState - will be cloned for each rollout
 * @param {number} playerId - perspective player
 * @param {number} count - number of rollouts
 * @returns {{ wins: number, draws: number, losses: number, winRate: number }}
 */
export function runRollouts(baseState, playerId, count) {
  let wins = 0, draws = 0, losses = 0;

  for (let i = 0; i < count; i++) {
    const sim = baseState.clone();
    rollout(sim);
    const r = rolloutReward(sim, playerId);
    if (r === 1.0) wins++;
    else if (r === 0.5) draws++;
    else losses++;
  }

  return { wins, draws, losses, winRate: wins / count };
}
