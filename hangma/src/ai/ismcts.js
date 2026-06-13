// ============================================================
// ismcts.js — ISMCTS search core for hangma AI
// ============================================================
// Information Set MCTS: handles imperfect information via
// determinization + shared tree search across samples.
// ============================================================

import {
  SimGameState, TileSet, drawRandom, rollout, rolloutReward,
  isWinningHandSet, computeShanten, chooseBestDiscard,
  shantenAfterDiscard, determinize, fromGameState
} from './game-sim.js';

// Re-export for convenience
export { determinize, fromGameState } from './game-sim.js';

// ----------------------------------------------------------
// ISMCTS Node
// ----------------------------------------------------------

class ISMCTSNode {
  constructor(parent, actionKey) {
    /** @type {ISMCTSNode|null} */
    this.parent = parent;
    /** @type {string} action key that led to this node */
    this.actionKey = actionKey;
    /** @type {Map<string, ISMCTSNode>} */
    this.children = new Map();
    /** Total visits across all determinizations */
    this.visits = 0;
    /** Total reward for the observer */
    this.value = 0;
    /** @type {object[]|null} cached untried actions (from last determinization) */
    this.untriedActions = null;
  }

  /** UCB1 score */
  ucb1(explorationConst) {
    if (this.visits === 0) return Infinity;
    return (this.value / this.visits) +
      explorationConst * Math.sqrt(Math.log(this.parent.visits) / this.visits);
  }

  /** Win rate */
  winRate() {
    if (this.visits === 0) return 0;
    return this.value / this.visits;
  }

  /** Fully expanded = all cached actions have been tried */
  isFullyExpanded() {
    return this.untriedActions !== null && this.untriedActions.length === 0;
  }
}

// ----------------------------------------------------------
// Action helpers
// ----------------------------------------------------------

function actionKey(action) {
  if (action.type === 'chi' && action.chiPair) {
    return `chi:${action.tileIdx}:${action.chiPair[0]}:${action.chiPair[1]}`;
  }
  return `${action.type}:${action.tileIdx}`;
}

/**
 * Get legal discard actions for a player.
 * Cannot discard wild tiles (财神).
 */
function getDiscardActions(state, playerId) {
  const hand = state.hands[playerId];
  const wildIdx = state.wildIdx;
  const actions = [];
  const d = hand.data;
  for (let i = 0; i < 34; i++) {
    if (d[i] > 0 && i !== wildIdx) {
      actions.push({ type: 'discard', tileIdx: i });
    }
  }
  return actions;
}

// ----------------------------------------------------------
// Game advance — play from a discard until observer's next turn
// ----------------------------------------------------------

/**
 * Apply a discard action and advance the game until:
 * - It's the observer's turn again, OR
 * - The game ends (win/draw)
 *
 * This covers: discard → claims → opponent turns → observer's next draw.
 *
 * Mutates `state` in place.
 *
 * @param {SimGameState} state
 * @param {number} playerId - the discarding player
 * @param {number} tileIdx - the tile to discard
 * @param {number} observerId - the AI player
 * @returns {boolean} true if state is now at observer's turn (not terminal)
 */
function applyDiscardAndAdvance(state, playerId, tileIdx, observerId) {
  const wildIdx = state.wildIdx;
  const hand = state.hands[playerId];

  // Discard
  hand.remove(tileIdx);
  state.discards[playerId].add(tileIdx);
  state.discardCount++;

  // Check claims from other players
  const claims = collectClaims(state, playerId, tileIdx);
  if (claims.length > 0) {
    // Resolve claims: highest priority wins
    claims.sort((a, b) => b.priority - a.priority);
    const bestClaim = claims[0];

    if (bestClaim.playerId === observerId) {
      // Observer gets to claim — stop here so tree can branch on claim
      // Revert discard so the state is at "discard just happened, observer can claim"
      hand.add(tileIdx);
      state.discards[playerId].remove(tileIdx);
      state.discardCount--;
      state._pendingClaim = { discarderId: playerId, tileIdx, claims };
      return true; // observer needs to decide
    }

    // Opponent claims — execute it
    executeClaim(state, bestClaim, playerId, tileIdx);
    // After claim execution, turn is set to the claimer
    // We need to continue until observer's turn

    // Run through turns until observer's turn or terminal
    return advanceToObserverTurn(state, observerId);
  }

  // No claims, advance to next player's turn
  state.turn = (playerId + 1) % 4;
  return advanceToObserverTurn(state, observerId);
}

/**
 * Advance the game from the current turn until it's the observer's turn
 * or the game ends. Uses greedy policy for all non-observer decisions.
 *
 * @returns {boolean} true if state is at observer's turn (not terminal)
 */
function advanceToObserverTurn(state, observerId) {
  let safety = 50;
  const wildIdx = state.wildIdx;

  while (safety-- > 0 && state.winner === -1 && !state.drawReason) {
    if (state.turn === observerId) {
      // It's the observer's turn — draw a tile
      if (state.pool.total() === 0) {
        state.drawReason = '牌已摸完，本局流局。';
        return false;
      }

      // Check if observer is in piaoCai mode
      if (state.piaoCai[observerId]) {
        const drawn = drawRandom(state.pool);
        state.pool.remove(drawn);
        state.hands[observerId].add(drawn);
        state.roundCount++;

        if (isWinningHandSet(state.hands[observerId], wildIdx, state.melds[observerId])) {
          state.winner = observerId;
          state.winKind = '飘财';
          state.winningTileIdx = drawn;
          return false;
        }

        // Auto-discard
        state.hands[observerId].remove(drawn);
        state.discards[observerId].add(drawn);
        state.discardCount++;

        const claims = collectClaims(state, observerId, drawn);
        if (claims.length > 0) {
          claims.sort((a, b) => b.priority - a.priority);
          executeClaim(state, claims[0], observerId, drawn);
          continue;
        }

        state.turn = (observerId + 1) % 4;
        continue;
      }

      // Normal draw for observer
      if (state.pool.total() === 0) {
        state.drawReason = '牌已摸完，本局流局。';
        return false;
      }
      const drawn = drawRandom(state.pool);
      state.pool.remove(drawn);
      state.hands[observerId].add(drawn);
      if (drawn === wildIdx) state.handWildCounts[observerId]++;
      state.roundCount++;

      // Check win
      if (isWinningHandSet(state.hands[observerId], wildIdx, state.melds[observerId])) {
        state.winner = observerId;
        state.winKind = '自摸';
        state.winningTileIdx = drawn;
        return false;
      }

      // Check concealed gang
      const gangChoices = getConcealedGangChoicesForPlayer(state, observerId);
      if (gangChoices.length > 0) {
        // Execute first concealed gang greedily
        executeConcealedGang(state, observerId, gangChoices[0]);
        // After gang, observer may have won or needs to discard
        if (state.winner === observerId) return false;
        // After concealed gang, observer needs to discard
        // Fall through to observer's discard decision
      }

      // Observer is at decision point (14 tiles, needs to discard)
      return true;
    }

    // It's an opponent's turn — play greedily
    const playerId = state.turn;

    if (state.piaoCai[playerId]) {
      if (state.pool.total() === 0) {
        state.drawReason = '牌已摸完，本局流局。';
        return false;
      }
      const drawn = drawRandom(state.pool);
      state.pool.remove(drawn);
      state.hands[playerId].add(drawn);
      state.roundCount++;

      if (isWinningHandSet(state.hands[playerId], wildIdx, state.melds[playerId])) {
        state.winner = playerId;
        state.winKind = '飘财';
        state.winningTileIdx = drawn;
        return false;
      }

      state.hands[playerId].remove(drawn);
      state.discards[playerId].add(drawn);
      state.discardCount++;

      const claims = collectClaims(state, playerId, drawn);
      if (claims.length > 0) {
        claims.sort((a, b) => b.priority - a.priority);
        executeClaim(state, claims[0], playerId, drawn);
        continue;
      }

      state.turn = (playerId + 1) % 4;
      continue;
    }

    // Draw
    if (state.pool.total() === 0) {
      state.drawReason = '牌已摸完，本局流局。';
      return false;
    }
    const drawn = drawRandom(state.pool);
    state.pool.remove(drawn);
    state.hands[playerId].add(drawn);
    if (drawn === wildIdx) state.handWildCounts[playerId]++;
    state.roundCount++;

    if (isWinningHandSet(state.hands[playerId], wildIdx, state.melds[playerId])) {
      state.winner = playerId;
      state.winKind = '自摸';
      state.winningTileIdx = drawn;
      return false;
    }

    // Greedy discard
    const result = chooseBestDiscard(state, playerId);
    if (result.tileIdx < 0) {
      state.drawReason = '无法出牌，流局。';
      return false;
    }

    state.hands[playerId].remove(result.tileIdx);
    state.discards[playerId].add(result.tileIdx);
    state.discardCount++;

    const claims = collectClaims(state, playerId, result.tileIdx);
    if (claims.length > 0) {
      claims.sort((a, b) => b.priority - a.priority);
      const bestClaim = claims[0];

      if (bestClaim.playerId === observerId) {
        // Observer has a claim opportunity — stop here for tree branching
        // Revert the discard
        state.hands[playerId].add(result.tileIdx);
        state.discards[playerId].remove(result.tileIdx);
        state.discardCount--;
        state._pendingClaim = { discarderId: playerId, tileIdx: result.tileIdx, claims };
        return true;
      }

      executeClaim(state, bestClaim, playerId, result.tileIdx);
      continue;
    }

    state.turn = (playerId + 1) % 4;
  }

  if (safety <= 0) {
    state.drawReason = '模拟超时，流局。';
  }
  return false;
}

// ----------------------------------------------------------
// Claim handling (from game-sim.js patterns)
// ----------------------------------------------------------

function collectClaims(state, discarderId, tileIdx) {
  const claims = [];
  const wildIdx = state.wildIdx;

  // Chi only from next player
  const nextPlayerId = (discarderId + 1) % 4;
  if (!state.piaoCai[nextPlayerId]) {
    const chiOptions = getChiOptionsSim(state.hands[nextPlayerId], tileIdx, wildIdx);
    for (const chiPair of chiOptions) {
      claims.push({ playerId: nextPlayerId, type: 'chi', priority: 1, chiPair });
    }
  }

  // Peng/gang from any other player
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

function getChiOptionsSim(handSet, tileIdx, wildIdx) {
  if (tileIdx >= 27) return [];

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

function executeClaim(state, claim, discarderId, tileIdx) {
  const playerId = claim.playerId;
  const hand = state.hands[playerId];
  const wildIdx = state.wildIdx;

  if (claim.type === 'peng') {
    hand.remove(tileIdx);
    hand.remove(tileIdx);
    state.melds[playerId].add(tileIdx);
    state.melds[playerId].add(tileIdx);
    state.melds[playerId].add(tileIdx);
    state.discards[discarderId].remove(tileIdx);
    state.meldInfos[playerId].push({ type: 'peng', concealed: false });

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

/** Get concealed gang choices for a player in simulation state */
function getConcealedGangChoicesForPlayer(state, playerId) {
  const hand = state.hands[playerId];
  const wildIdx = state.wildIdx;
  const choices = [];
  const d = hand.data;
  for (let i = 0; i < 34; i++) {
    if (i === wildIdx) continue;
    if (d[i] >= 4) {
      choices.push(i);
    }
  }
  return choices;
}

function executeConcealedGang(state, playerId, tileIdx) {
  const hand = state.hands[playerId];
  hand.remove(tileIdx);
  hand.remove(tileIdx);
  hand.remove(tileIdx);
  hand.remove(tileIdx);
  state.melds[playerId].add(tileIdx);
  state.melds[playerId].add(tileIdx);
  state.melds[playerId].add(tileIdx);
  state.melds[playerId].add(tileIdx);
  state.meldInfos[playerId].push({ type: 'concealedGang', concealed: true });

  // Draw replacement from end of wall
  if (state.pool.total() > 0) {
    const drawn = drawRandom(state.pool);
    state.pool.remove(drawn);
    hand.add(drawn);
    if (drawn === state.wildIdx) state.handWildCounts[playerId]++;

    if (isWinningHandSet(hand, state.wildIdx, state.melds[playerId])) {
      state.winner = playerId;
      state.winKind = '杠开';
      state.winningTileIdx = drawn;
    }
  }
}

// ----------------------------------------------------------
// ISMCTS search
// ----------------------------------------------------------

/**
 * Run ISMCTS search to choose the best discard.
 *
 * @param {SimGameState} rootState - base state from AI's perspective
 * @param {number} observerId - the AI player ID
 * @param {number} iterations - number of MCTS iterations to run
 * @param {object} [options]
 * @param {number} [options.explorationConst=Math.SQRT2] - UCB1 exploration constant
 * @param {function} [options.determinizeFn] - custom determinization function
 * @param {number} [options.timeLimit=0] - time limit in ms (0 = no limit)
 * @returns {{ action: object, stats: Map<string, {visits:number, winRate:number}> }}
 */
export function ismctsSearch(rootState, observerId, iterations, options = {}) {
  const {
    explorationConst = Math.SQRT2,
    determinizeFn = null,
    timeLimit = 0,
  } = options;

  const root = new ISMCTSNode(null, null);
  const detFunc = determinizeFn || ((s, o) => determinize(s, o));

  const startTime = performance.now();

  for (let iter = 0; iter < iterations; iter++) {
    // Time check
    if (timeLimit > 0 && (performance.now() - startTime) > timeLimit) break;

    // 1. Determinize
    const det = detFunc(rootState, observerId);
    det._pendingClaim = null;

    // 2. Selection
    let node = root;
    let state = det;

    while (node.isFullyExpanded() && state.winner === -1 && !state.drawReason) {
      // Get legal actions at this state
      const actions = getLegalActionsForState(state, observerId);
      if (actions.length === 0) break;

      // Find best child by UCB1
      let bestChild = null;
      let bestUCB = -Infinity;

      for (const action of actions) {
        const key = actionKey(action);
        const child = node.children.get(key);
        if (!child) {
          // This action hasn't been tried in the tree yet
          // It should be in untriedActions; break to expansion
          bestChild = null;
          break;
        }
        const ucb = child.ucb1(explorationConst);
        if (ucb > bestUCB) {
          bestUCB = ucb;
          bestChild = child;
        }
      }

      if (!bestChild) break;

      // Apply the action
      applyActionToState(state, observerId, bestChild.actionKey);
      node = bestChild;

      // If applying the action brought us to another observer decision point, continue
      // If terminal, break
      if (state.winner !== -1 || state.drawReason) break;
      if (state.turn !== observerId || state._pendingClaim) {
        // Need to advance to observer's next decision
        if (state._pendingClaim) {
          // Observer has a claim decision — handle it
          const claim = resolveClaimGreedyForObserver(state, observerId);
          if (claim) {
            applyClaimToState(state, claim, state._pendingClaim.discarderId, state._pendingClaim.tileIdx, observerId);
          } else {
            // Pass on claim
            const pc = state._pendingClaim;
            state._pendingClaim = null;
            // Put discard back into effect
            state.hands[pc.discarderId].add(pc.tileIdx);
            state.discards[pc.discarderId].remove(pc.tileIdx);
            state.discardCount--;
            // Re-discard without claims
            state.hands[pc.discarderId].remove(pc.tileIdx);
            state.discards[pc.discarderId].add(pc.tileIdx);
            state.discardCount++;
            state.turn = (pc.discarderId + 1) % 4;
            advanceToObserverTurn(state, observerId);
          }
        } else {
          advanceToObserverTurn(state, observerId);
        }
      }
      if (state.winner !== -1 || state.drawReason) break;
    }

    // 3. Expansion
    if (state.winner === -1 && !state.drawReason && state.turn === observerId) {
      if (node.untriedActions === null) {
        node.untriedActions = getLegalActionsForState(state, observerId);
      }

      if (node.untriedActions.length > 0) {
        const action = node.untriedActions.pop();
        const child = new ISMCTSNode(node, actionKey(action));
        node.children.set(actionKey(action), child);
        node = child;

        applyActionToState(state, observerId, actionKey(action));

        // Advance after action
        if (state.winner === -1 && !state.drawReason && state.turn !== observerId) {
          if (state._pendingClaim) {
            const claim = resolveClaimGreedyForObserver(state, observerId);
            if (claim) {
              applyClaimToState(state, claim, state._pendingClaim.discarderId, state._pendingClaim.tileIdx, observerId);
            } else {
              const pc = state._pendingClaim;
              state._pendingClaim = null;
              state.hands[pc.discarderId].add(pc.tileIdx);
              state.discards[pc.discarderId].remove(pc.tileIdx);
              state.discardCount--;
              state.hands[pc.discarderId].remove(pc.tileIdx);
              state.discards[pc.discarderId].add(pc.tileIdx);
              state.discardCount++;
              state.turn = (pc.discarderId + 1) % 4;
              advanceToObserverTurn(state, observerId);
            }
          } else {
            advanceToObserverTurn(state, observerId);
          }
        }
      }
    }

    // 4. Rollout
    if (state.winner === -1 && !state.drawReason) {
      rollout(state);
    }
    const reward = rolloutReward(state, observerId);

    // 5. Backprop
    while (node !== null) {
      node.visits++;
      node.value += reward;
      node = node.parent;
    }
  }

  // Build stats from root children
  const stats = new Map();
  for (const [key, child] of root.children) {
    stats.set(key, {
      visits: child.visits,
      winRate: child.winRate(),
      value: child.value,
    });
  }

  // Pick best action by win rate (fallback: most visits)
  let bestAction = null;
  let bestWinRate = -1;
  let bestVisits = -1;

  for (const [key, child] of root.children) {
    if (child.visits > 0 && child.winRate() > bestWinRate) {
      bestWinRate = child.winRate();
      bestAction = key;
      bestVisits = child.visits;
    } else if (child.visits > 0 && child.winRate() === bestWinRate && child.visits > bestVisits) {
      bestAction = key;
      bestVisits = child.visits;
    }
  }

  // Parse action key back to action object
  const action = bestAction ? parseActionKey(bestAction) : null;

  return { action, stats };
}

/**
 * Get legal actions for the observer at the current state.
 * Includes discard actions and claim decisions.
 */
function getLegalActionsForState(state, observerId) {
  // Check for pending claim decision
  if (state._pendingClaim) {
    const { claims, tileIdx } = state._pendingClaim;
    const observerClaims = claims.filter(c => c.playerId === observerId);
    const actions = [];
    for (const claim of observerClaims) {
      actions.push({ type: claim.type, tileIdx, playerId: observerId, chiPair: claim.chiPair || null });
    }
    actions.push({ type: 'pass', tileIdx }); // pass on the claim
    return actions;
  }

  // Observer's turn to discard
  if (state.turn === observerId) {
    return getDiscardActions(state, observerId);
  }

  return [];
}

function parseActionKey(key) {
  const parts = key.split(':');
  if (parts[0] === 'chi' && parts.length >= 4) {
    return { type: 'chi', tileIdx: parseInt(parts[1], 10), chiPair: [parseInt(parts[2], 10), parseInt(parts[3], 10)] };
  }
  return { type: parts[0], tileIdx: parseInt(parts[1], 10) };
}

/**
 * Apply an action to a state. Mutates state in place.
 */
function applyActionToState(state, observerId, key) {
  const action = parseActionKey(key);

  if (action.type === 'discard') {
    // Just remove the tile and set up for claim checking
    const hand = state.hands[observerId];
    hand.remove(action.tileIdx);
    state.discards[observerId].add(action.tileIdx);
    state.discardCount++;

    // Check claims
    const claims = collectClaims(state, observerId, action.tileIdx);
    if (claims.length > 0) {
      claims.sort((a, b) => b.priority - a.priority);
      const bestClaim = claims[0];

      if (bestClaim.playerId === observerId) {
        // Observer can claim their own discard — skip, just continue
        // (shouldn't normally happen in real game, but handle gracefully)
        state.turn = (observerId + 1) % 4;
        advanceToObserverTurn(state, observerId);
      } else {
        // Opponent claims
        executeClaim(state, bestClaim, observerId, action.tileIdx);
        advanceToObserverTurn(state, observerId);
      }
    } else {
      state.turn = (observerId + 1) % 4;
      advanceToObserverTurn(state, observerId);
    }
  } else if (action.type === 'pass') {
    // Pass on claim — advance past the claim opportunity
    const pc = state._pendingClaim;
    state._pendingClaim = null;
    // Re-discard and skip the claiming observer
    state.hands[pc.discarderId].add(pc.tileIdx);
    state.discards[pc.discarderId].remove(pc.tileIdx);
    state.discardCount--;
    state.hands[pc.discarderId].remove(pc.tileIdx);
    state.discards[pc.discarderId].add(pc.tileIdx);
    state.discardCount++;
    state.turn = (pc.discarderId + 1) % 4;
    advanceToObserverTurn(state, observerId);
  } else if (action.type === 'peng' || action.type === 'gang' || action.type === 'chi') {
    // Observer claims
    const pc = state._pendingClaim;
    state._pendingClaim = null;
    executeClaim(state, { playerId: observerId, type: action.type, priority: action.type === 'gang' ? 2.5 : action.type === 'peng' ? 2 : 1, chiPair: action.chiPair || null }, pc.discarderId, pc.tileIdx);
    advanceToObserverTurn(state, observerId);
  }
}

function resolveClaimGreedyForObserver(state, observerId) {
  if (!state._pendingClaim) return null;
  const observerClaims = state._pendingClaim.claims.filter(c => c.playerId === observerId);
  if (observerClaims.length === 0) return null;

  // Sort by priority
  observerClaims.sort((a, b) => b.priority - a.priority);

  // Always accept peng/gang in greedy rollout
  for (const claim of observerClaims) {
    if (claim.type === 'gang' || claim.type === 'peng') return claim;
  }

  // Accept chi
  if (observerClaims.length > 0) return observerClaims[0];

  return null;
}

function applyClaimToState(state, claim, discarderId, tileIdx, observerId) {
  state._pendingClaim = null;
  executeClaim(state, claim, discarderId, tileIdx);
  advanceToObserverTurn(state, observerId);
}

// ----------------------------------------------------------
// High-level AI decision functions
// ----------------------------------------------------------

/**
 * Choose the best tile to discard using ISMCTS.
 *
 * @param {SimGameState} simState - game state in simulation format
 * @param {number} playerId - the AI player
 * @param {number} iterations - MCTS iterations
 * @param {object} [options] - additional options
 * @returns {{ tileIdx: number, stats: Map }}
 */
export function chooseDiscardISMCTS(simState, playerId, iterations, options = {}) {
  // Ensure it's the player's turn and they have 14 tiles
  const handSize = simState.hands[playerId].total();
  if (handSize !== 14) {
    // Fallback to greedy
    const result = chooseBestDiscard(simState, playerId);
    return { tileIdx: result.tileIdx, stats: new Map() };
  }

  const result = ismctsSearch(simState, playerId, iterations, options);

  if (result.action && result.action.type === 'discard') {
    return { tileIdx: result.action.tileIdx, stats: result.stats };
  }

  // Fallback to greedy
  const greedyResult = chooseBestDiscard(simState, playerId);
  return { tileIdx: greedyResult.tileIdx, stats: result.stats };
}

/**
 * Decide whether to claim a discarded tile (peng/gang/chi).
 * Compares expected win rate of claiming vs. passing.
 *
 * @param {SimGameState} simState - state before the discard was claimed
 * @param {number} playerId - the AI player considering claiming
 * @param {string} claimType - 'peng', 'gang', or 'chi'
 * @param {number} tileIdx - the discarded tile
 * @param {number} iterations - MCTS iterations per option
 * @returns {boolean}
 */
export function shouldClaimISMCTS(simState, playerId, claimType, tileIdx, iterations) {
  // Create two branches: claim vs. pass
  // For efficiency, we do a simpler evaluation: check shanten improvement

  const hand = simState.hands[playerId];
  const wildIdx = simState.wildIdx;
  const currentShanten = computeShanten(hand, wildIdx);

  // Simulate claiming: add tile to hand, remove claim cost tiles, check shanten
  const handClone = hand.clone();
  handClone.add(tileIdx);

  if (claimType === 'peng') {
    // Need 2 of the same tile in hand; after adding 1 (the claimed tile), we have 3
    // Remove 2 from hand (the pair we already had), the 3rd is the claimed tile
    // Net: hand grows by 1 (the claimed tile), but 2 are moved to meld
    handClone.remove(tileIdx);
    handClone.remove(tileIdx);
    // handClone now has: original hand - 2 copies + 1 copy = original - 1
  } else if (claimType === 'gang') {
    handClone.remove(tileIdx);
    handClone.remove(tileIdx);
    handClone.remove(tileIdx);
    // handClone: original hand - 3 copies + 1 copy = original - 2
  } else if (claimType === 'chi') {
    // Hard to determine which tiles are used — skip for now
    // Just check if claiming improves shanten by running a quick ISMCTS
  }

  // After claiming, we'll need to discard back to proper hand size
  // For peng: hand size = original, we discard 1 → 13 before next draw → 14
  // For gang: hand size = original-2, discard 1 → 13 before next draw → 14, plus extra draw from wall

  // Simpler heuristic for now: does the claimed tile help form a meld?
  if (claimType === 'gang') {
    // Gang always preserves hand strength and adds a draw → usually good
    return true;
  }

  if (claimType === 'peng') {
    // Only peng if the tile is useful (shanten improves or stays same)
    const afterShanten = computeShanten(handClone, wildIdx);
    // After peng, we discard one to get back to proper hand size
    // For a quick check: if the pair was loose (not part of a meld), peng is good
    return afterShanten <= currentShanten + 1;
  }

  if (claimType === 'chi') {
    // Chi is weakest claim — only if it clearly improves the hand
    // Check if using the tile + 2 from hand improves structure
    // Quick heuristic: only chi if not tenpai
    return currentShanten > 0;
  }

  return false;
}

/**
 * Create a SimGameState from the UI game state, suitable for ISMCTS.
 * @param {object} game - engine.js GameState
 * @returns {SimGameState}
 */
export function prepareSimState(game) {
  return fromGameState(game);
}
