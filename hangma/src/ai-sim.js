// ============================================================
// ai-sim.js — AI vs AI 对局模拟器
// 运行: node src/ai-sim.js [局数]
// ============================================================

import {
  createNewGame, createTile, tileKey, tileLabel, sortTiles, cloneTile,
  isWinningHand, isWinningWithMelds, getWaitTiles, getChiOptions, getPengOptions,
  getConcealedGangChoices,
  chooseDiscard, shouldPeng, shouldMeldGang, chooseChi,
  chooseConcealedGang, shouldPiaoCai, resolveClaim,
  calculateWinScore, checkFourWindDiscard, countWildInHand,
} from './engine.js';

const NUM_GAMES = parseInt(process.argv[2], 10) || 20;
const WILD_KEY = 'honor:white';

// ============================================================
// Simulator
// ============================================================

function simDraw(game, playerId) {
  const player = game.players[playerId];
  if (game.tilePool.length === 0) return null;

  // 杠后补牌从倒数第二张
  const idx = player._justGanged ? game.tilePool.length - 2 : game.tilePool.length - 1;
  if (idx < 0) return null;

  const tile = game.tilePool.splice(idx, 1)[0];
  player._justGanged = false;
  return tile;
}

function simDiscard(game, playerId, tile) {
  const player = game.players[playerId];
  const idx = player.hand.findIndex(t => t.id === tile.id);
  if (idx < 0) return false;
  player.discards.push(player.hand.splice(idx, 1)[0]);
  game.lastDiscard = { playerId, tile, fromDraw: true };
  return true;
}

function simMeld(game, playerId, type, choice, discardTile) {
  const player = game.players[playerId];
  let tiles = [];

  if (type === 'peng') {
    tiles = player.hand.filter(t => tileKey(t) === tileKey(discardTile)).slice(0, 2);
  } else if (type === 'gang') {
    tiles = player.hand.filter(t => tileKey(t) === tileKey(discardTile)).slice(0, 3);
  } else if (type === 'chi') {
    tiles = choice.tiles;
  } else if (type === 'concealed_gang') {
    tiles = choice;
  }

  for (const tile of tiles) {
    const idx = player.hand.findIndex(t => t.id === tile.id);
    if (idx >= 0) player.hand.splice(idx, 1);
  }

  const meldTiles = [...tiles, cloneTile(discardTile)];
  const meld = { type, tiles: meldTiles, fromPlayer: game.lastDiscard?.playerId };
  player.melds.push(meld);

  if (type === 'gang' || type === 'concealed_gang') {
    player._justGanged = true;
  }

  return meld;
}

function simDeclareWin(game, playerId, kind, fromPlayerId, winningTile) {
  const player = game.players[playerId];
  // 重建完整14张手牌：hand + melds中所有牌（drawn/winning tile 已在hand中）
  const hand = [...player.hand];
  for (const meld of player.melds) {
    for (const tile of meld.tiles) {
      hand.push(cloneTile(tile));
    }
  }

  const result = calculateWinScore({
    hand, wildKey: WILD_KEY, kind,
    winningTile,
    piaoCount: player.piaoCount || 0,
    isDealer: playerId === game.dealer,
    isFirstDraw: false,
    isFirstTurn: game._discardCount === 0,
  });

  game.winner = {
    playerId, fromPlayerId, kind,
    summary: result.flags.join('+') + (player.piaoCount > 0 ? ` 飘${player.piaoCount}` : ''),
    flags: result.flags,
    multiplier: result.multiplier,
    total: result.total,
    isSelfDraw: result.isSelfDraw,
  };

  // Payment
  const isDealer = playerId === game.dealer;
  const losePerNonDealer = result.total;
  const losePerDealer = isDealer ? 0 : result.total;

  for (let i = 0; i < 4; i++) {
    if (i === playerId) continue;
    const lose = i === game.dealer ? losePerDealer : losePerNonDealer;
    game.players[i].score -= lose;
    game.players[playerId].score += lose;
  }
}

function simDeclarePiaoCai(game, playerId) {
  const player = game.players[playerId];
  if (player.piaoCai) return false;

  // 打出手中一张财神
  const wildTile = player.hand.find(t => tileKey(t) === WILD_KEY);
  if (!wildTile) return false;

  const idx = player.hand.indexOf(wildTile);
  player.hand.splice(idx, 1);
  player.discards.push(wildTile);
  sortTiles(player.hand);

  player.piaoCai = true;
  player.piaoCount = (player.piaoCount || 0) + 1;
  game._piaoCaiOrigin = playerId;
  game._piaoCaiTurnsTaken = 0;
  return true;
}

// ============================================================
// Main simulation loop
// ============================================================

function runOneGame(gameNum) {
  const game = createNewGame();
  // All 4 players are AI
  for (const p of game.players) p.isHuman = false;

  let turn = 0;
  let consecutivePasses = 0;
  const MAX_TURNS = 500;

  while (consecutivePasses < 4 && turn < MAX_TURNS && !game.winner) {
    const playerId = turn % 4;
    const player = game.players[playerId];

    if (game._piaoCaiOrigin >= 0 && playerId !== game._piaoCaiOrigin) {
      // 飘财中：其他玩家只能摸打
      if (player.piaoCai) {
        turn++;
        consecutivePasses++;
        continue;
      }
      const tile = simDraw(game, playerId);
      if (!tile) { turn++; consecutivePasses = 4; break; }
      player.hand.push(tile);
      sortTiles(player.hand);

      // 自摸胡
      if (isWinningWithMelds(player.hand, player.melds, WILD_KEY)) {
        simDeclareWin(game, playerId, '自摸', null, tile);
        break;
      }

      const discard = chooseDiscard(player.hand, playerId, game);
      simDiscard(game, playerId, discard);
      sortTiles(player.hand);

      game._piaoCaiTurnsTaken++;
      if (game._piaoCaiTurnsTaken >= 3) {
        // 飘财者轮回 → 自动摸牌爆头胡
        const originPlayer = game.players[game._piaoCaiOrigin];
        const originTile = simDraw(game, game._piaoCaiOrigin);
        if (originTile) {
          originPlayer.hand.push(originTile);
          sortTiles(originPlayer.hand);
          simDeclareWin(game, game._piaoCaiOrigin, '飘财', null, originTile);
        }
        break;
      }

      turn++;
      continue;
    }

    // ---- 正常回合 ----

    // 1. Draw
    const drawnTile = simDraw(game, playerId);
    if (!drawnTile) {
      // 流局
      game.drawReason = '流局';
      break;
    }
    player.hand.push(drawnTile);
    sortTiles(player.hand);

    // 2. Check self-draw win → may choose to piaoCai instead
    if (isWinningWithMelds(player.hand, player.melds, WILD_KEY)) {
      // First piao: can win (暴头) but choose to discard 财神 instead
      if (!player.piaoCai && countWildInHand(player.hand, WILD_KEY) >= 2 && shouldPiaoCai(playerId, game)) {
        simDeclarePiaoCai(game, playerId);
        turn++;
        continue;
      }
      // Multi-piao: already in piaoCai, can continue if has 2+ wilds
      if (player.piaoCai && countWildInHand(player.hand, WILD_KEY) >= 2 && shouldPiaoCai(playerId, game)) {
        const wildTile = player.hand.find(t => tileKey(t) === WILD_KEY);
        if (wildTile) {
          const idx = player.hand.indexOf(wildTile);
          player.hand.splice(idx, 1);
          player.discards.push(wildTile);
          sortTiles(player.hand);
          player.piaoCount = (player.piaoCount || 0) + 1;
          game._piaoCaiTurnsTaken = 0;
          turn++;
          continue;
        }
      }
      simDeclareWin(game, playerId, player.piaoCai ? '飘财' : '自摸', null, drawnTile);
      break;
    }

    // 3. Check concealed gang
    const gangChoices = getConcealedGangChoices(player.hand, WILD_KEY);
    if (gangChoices.length > 0) {
      const gangChoice = chooseConcealedGang(playerId, gangChoices, game);
      if (gangChoice) {
        simMeld(game, playerId, 'concealed_gang', gangChoice, null);
        // 补牌
        const bonusTile = simDraw(game, playerId);
        if (bonusTile) {
          player.hand.push(bonusTile);
          sortTiles(player.hand);
          if (isWinningWithMelds(player.hand, player.melds, WILD_KEY)) {
            simDeclareWin(game, playerId, '杠开', null, bonusTile);
            break;
          }
        }
      }
    }

    // 5. Discard
    const discard = chooseDiscard(player.hand, playerId, game);
    if (!simDiscard(game, playerId, discard)) {
      turn++;
      continue;
    }
    sortTiles(player.hand);

    // 6. Track first discards for four wind check
    if (!game._discardCount) game._discardCount = 0;
    if (game._discardCount < 4) {
      if (!game._firstDiscards) game._firstDiscards = [];
      game._firstDiscards.push({ playerId, tile: discard });
      game._discardCount++;
      if (game._discardCount === 4) {
        const result = checkFourWindDiscard(game._firstDiscards);
        if (result.isFourWind) {
          game.winner = {
            playerId: game.dealer,
            kind: '四风连打',
            summary: `四风连打 (${result.windLabel})`,
            flags: [],
            multiplier: 0,
            total: 0,
            isSelfDraw: true,
          };
          break;
        }
      }
    }

    // 7. Check other players' claims
    const claims = [];
    for (let pid = 0; pid < 4; pid++) {
      if (pid === playerId) continue;
      const p = game.players[pid];
      if (p.piaoCai) continue;

      // 飘财中其他玩家不能吃碰明杠
      if (game._piaoCaiOrigin >= 0 && pid !== game._piaoCaiOrigin) continue;

      const isNextPlayer = (pid - playerId + 4) % 4;
      const pengOpt = getPengOptions(p.hand, discard, WILD_KEY);
      if (pengOpt) {
        claims.push({
          playerId: pid, type: pengOpt.type, priority: pengOpt.type === 'gang' ? 2.5 : 2,
          tiles: pengOpt.tiles,
        });
      }

      if (isNextPlayer === 1 && !p.piaoCai) {
        const chiOpts = getChiOptions(p.hand, discard, WILD_KEY);
        if (chiOpts.length > 0) {
          claims.push({
            playerId: pid, type: 'chi', priority: 1, choices: chiOpts,
          });
        }
      }
    }

    if (claims.length > 0) {
      const resolved = resolveClaim(claims, discard, playerId, game);
      if (resolved) {
        game.lastDiscard = { playerId, tile: discard, fromDraw: true };

        if (resolved.type === 'chi') {
          const chiChoice = chooseChi(resolved.playerId, discard, resolved.choices, game);
          if (chiChoice) {
            simMeld(game, resolved.playerId, 'chi', chiChoice, discard);
            // 需要再弃一张牌
            const cp = game.players[resolved.playerId];
            const chiDiscard = chooseDiscard(cp.hand, resolved.playerId, game);
            simDiscard(game, resolved.playerId, chiDiscard);
            sortTiles(cp.hand);
          }
        } else {
          simMeld(game, resolved.playerId, resolved.type, null, discard);
          if (resolved.type === 'gang') {
            // 杠后补牌
            const gp = game.players[resolved.playerId];
            const bonusTile = simDraw(game, resolved.playerId);
            if (bonusTile) {
              gp.hand.push(bonusTile);
              sortTiles(gp.hand);
              if (isWinningHand(gp.hand, WILD_KEY)) {
                simDeclareWin(game, resolved.playerId, '杠开', null, bonusTile);
                break;
              }
            }
            // 杠后还需弃牌
            const gangDiscard = chooseDiscard(gp.hand, resolved.playerId, game);
            simDiscard(game, resolved.playerId, gangDiscard);
            sortTiles(gp.hand);
          } else {
            // 碰后需弃牌
            const pp = game.players[resolved.playerId];
            const pengDiscard = chooseDiscard(pp.hand, resolved.playerId, game);
            simDiscard(game, resolved.playerId, pengDiscard);
            sortTiles(pp.hand);
          }
        }

        // 检查点炮：碰/吃者打出的牌是否让其他家胡
        const newDiscard = game.lastDiscard;
        if (newDiscard && newDiscard.playerId !== resolved.playerId) {
          // Actually it IS the claimer's discard
        }
        // 简化：不检查二次点炮
        turn = resolved.playerId; // 跳转到行动者之后
        consecutivePasses = 0;
        turn++;
        continue;
      }
    }

    consecutivePasses = 0;
    turn++;
  }

  // 流局处理
  if (!game.winner && game.drawReason !== '流局' && game.tilePool.length === 0) {
    game.drawReason = '流局';
  }

  return game;
}

// ============================================================
// Run N games
// ============================================================

const stats = {
  totalGames: 0,
  wins: {},
  draws: 0,
  errors: 0,
  totalTurns: 0,
};

console.log(`Running ${NUM_GAMES} AI vs AI games...\n`);

for (let g = 1; g <= NUM_GAMES; g++) {
  let game;
  try {
    game = runOneGame(g);
  } catch (e) {
    console.error(`Game ${g}: ERROR — ${e.message}`);
    stats.errors++;
    continue;
  }

  stats.totalGames++;
  if (game.winner) {
    const w = game.winner;
    const key = `${w.kind}`;
    stats.wins[key] = (stats.wins[key] || 0) + 1;
    if (g <= 5 || g % 5 === 0) {
      const winnerName = `Player ${w.playerId}`;
      console.log(`Game ${g}: ${winnerName} wins (${w.kind}) — ${w.summary} — ×${w.multiplier} = ${w.total}分`);
    }
  } else {
    stats.draws++;
    if (g <= 5 || g % 5 === 0) {
      console.log(`Game ${g}: 流局 (draw)`);
    }
  }
}

// ============================================================
// Report
// ============================================================

console.log('\n========================================');
console.log('           AI vs AI 对局统计');
console.log('========================================');
console.log(`总对局数:    ${stats.totalGames}`);
console.log(`流局:        ${stats.draws}`);
console.log(`错误:        ${stats.errors}`);
console.log(`胡牌率:      ${((stats.totalGames - stats.draws) / stats.totalGames * 100).toFixed(1)}%`);
console.log('');

if (Object.keys(stats.wins).length > 0) {
  console.log('胡牌方式分布:');
  const sorted = Object.entries(stats.wins).sort((a, b) => b[1] - a[1]);
  for (const [kind, count] of sorted) {
    console.log(`  ${kind}:  ${count} 局`);
  }
}

console.log('');
if (stats.errors > 0) {
  console.log(`⚠ ${stats.errors} games had errors.`);
}
console.log('Done.');
