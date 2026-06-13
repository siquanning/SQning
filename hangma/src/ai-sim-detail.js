// ai-sim-detail.js — 打印每局详细胡牌信息
import { createNewGame, tileKey, sortTiles, cloneTile,
  isWinningWithMelds, getWaitTiles, getChiOptions, getPengOptions,
  getConcealedGangChoices, chooseDiscard, shouldPiaoCai, resolveClaim,
  chooseChi, chooseConcealedGang,
  calculateWinScore, checkFourWindDiscard, countWildInHand } from './engine.js';

const WILD_KEY = 'honor:white';

function simDraw(game, pid) {
  const p = game.players[pid];
  if (game.tilePool.length === 0) return null;
  const idx = p._justGanged ? game.tilePool.length - 2 : game.tilePool.length - 1;
  if (idx < 0) return null;
  const t = game.tilePool.splice(idx, 1)[0];
  p._justGanged = false;
  return t;
}
function simDiscard(game, pid, tile) {
  const p = game.players[pid];
  const i = p.hand.findIndex(t => t.id === tile.id);
  if (i < 0) return false;
  p.discards.push(p.hand.splice(i, 1)[0]);
  game.lastDiscard = { playerId: pid, tile, fromDraw: true };
  return true;
}
function simMeld(game, pid, type, choice, dTile) {
  const p = game.players[pid];
  let tiles = [];
  if (type === 'peng') tiles = p.hand.filter(t => tileKey(t) === tileKey(dTile)).slice(0, 2);
  else if (type === 'gang') tiles = p.hand.filter(t => tileKey(t) === tileKey(dTile)).slice(0, 3);
  else if (type === 'chi') tiles = choice.tiles;
  else if (type === 'concealed_gang') tiles = choice;
  for (const t of tiles) { const i = p.hand.findIndex(h => h.id === t.id); if (i >= 0) p.hand.splice(i, 1); }
  p.melds.push({ type, tiles: [...tiles, cloneTile(dTile)], fromPlayer: game.lastDiscard?.playerId });
  if (type === 'gang' || type === 'concealed_gang') p._justGanged = true;
}
function simWin(game, pid, kind, fromPid, winTile) {
  const p = game.players[pid];
  const hand = [...p.hand];
  for (const m of p.melds) for (const t of m.tiles) hand.push(cloneTile(t));
  const r = calculateWinScore({ hand, wildKey: WILD_KEY, kind, winningTile: winTile,
    piaoCount: p.piaoCount || 0, isDealer: pid === game.dealer,
    isFirstDraw: false, isFirstTurn: (game._discardCount || 0) === 0 });
  game.winner = { playerId: pid, fromPlayerId: fromPid, kind,
    flags: r.flags, multiplier: r.multiplier, total: r.total, isSelfDraw: r.isSelfDraw };
  for (let i = 0; i < 4; i++) {
    if (i === pid) continue;
    game.players[i].score -= r.total;
    game.players[pid].score += r.total;
  }
}

function runOneGame() {
  const g = createNewGame();
  for (const p of g.players) p.isHuman = false;
  let turn = 0;
  while (turn < 500 && !g.winner) {
    const pid = turn % 4;
    const p = g.players[pid];

    if (g._piaoCaiOrigin >= 0 && pid !== g._piaoCaiOrigin) {
      if (p.piaoCai) { turn++; continue; }
      const t = simDraw(g, pid); if (!t) { g.drawReason = '流局'; break; }
      p.hand.push(t); sortTiles(p.hand);
      if (isWinningWithMelds(p.hand, p.melds, WILD_KEY)) { simWin(g, pid, '自摸', null, t); break; }
      const d = chooseDiscard(p.hand, pid, g); simDiscard(g, pid, d); sortTiles(p.hand);
      g._piaoCaiTurnsTaken++;
      if (g._piaoCaiTurnsTaken >= 3) {
        const op = g.players[g._piaoCaiOrigin];
        const ot = simDraw(g, g._piaoCaiOrigin);
        if (ot) { op.hand.push(ot); sortTiles(op.hand); simWin(g, g._piaoCaiOrigin, '飘财', null, ot); }
        break;
      }
      turn++; continue;
    }

    const dt = simDraw(g, pid); if (!dt) { g.drawReason = '流局'; break; }
    p.hand.push(dt); sortTiles(p.hand);
    if (isWinningWithMelds(p.hand, p.melds, WILD_KEY)) {
      if (!p.piaoCai && countWildInHand(p.hand, WILD_KEY) >= 2 && shouldPiaoCai(pid, g)) {
        const wt = p.hand.find(t => tileKey(t) === WILD_KEY);
        if (wt) { const wi = p.hand.indexOf(wt); p.hand.splice(wi, 1); p.discards.push(wt); sortTiles(p.hand); }
        p.piaoCai = true; p.piaoCount = (p.piaoCount || 0) + 1;
        g._piaoCaiOrigin = pid; g._piaoCaiTurnsTaken = 0;
        turn++; continue;
      }
      if (p.piaoCai && countWildInHand(p.hand, WILD_KEY) >= 2 && shouldPiaoCai(pid, g)) {
        const wt = p.hand.find(t => tileKey(t) === WILD_KEY);
        if (wt) { const wi = p.hand.indexOf(wt); p.hand.splice(wi, 1); p.discards.push(wt); sortTiles(p.hand); }
        p.piaoCount = (p.piaoCount || 0) + 1;
        g._piaoCaiTurnsTaken = 0;
        turn++; continue;
      }
      simWin(g, pid, p.piaoCai ? '飘财' : '自摸', null, dt); break;
    }

    const gc = getConcealedGangChoices(p.hand, WILD_KEY);
    if (gc.length > 0) {
      const ch = chooseConcealedGang(pid, gc, g);
      if (ch) {
        simMeld(g, pid, 'concealed_gang', ch, null);
        const bt = simDraw(g, pid);
        if (bt) { p.hand.push(bt); sortTiles(p.hand);
          if (isWinningWithMelds(p.hand, p.melds, WILD_KEY)) { simWin(g, pid, '杠开', null, bt); break; } }
      }
    }

    const d = chooseDiscard(p.hand, pid, g);
    if (!simDiscard(g, pid, d)) { turn++; continue; }
    sortTiles(p.hand);

    if (!g._discardCount) g._discardCount = 0;
    if (g._discardCount < 4) {
      if (!g._firstDiscards) g._firstDiscards = [];
      g._firstDiscards.push({ playerId: pid, tile: d });
      g._discardCount++;
      if (g._discardCount === 4) {
        const fw = checkFourWindDiscard(g._firstDiscards);
        if (fw.isFourWind) { g.winner = { playerId: g.dealer, kind: '四风连打', flags:[], multiplier:0, total:0 }; break; }
      }
    }

    const claims = [];
    for (let i = 0; i < 4; i++) {
      if (i === pid || g.players[i].piaoCai) continue;
      if (g._piaoCaiOrigin >= 0 && i !== g._piaoCaiOrigin) continue;
      const po = getPengOptions(g.players[i].hand, d, WILD_KEY);
      if (po) claims.push({ playerId: i, type: po.type, priority: po.type === 'gang' ? 2.5 : 2 });
      if ((i - pid + 4) % 4 === 1) {
        const co = getChiOptions(g.players[i].hand, d, WILD_KEY);
        if (co.length > 0) claims.push({ playerId: i, type: 'chi', priority: 1, choices: co });
      }
    }
    if (claims.length > 0) {
      const rv = resolveClaim(claims, d, pid, g);
      if (rv) {
        g.lastDiscard = { playerId: pid, tile: d, fromDraw: true };
        if (rv.type === 'chi') {
          const cc = chooseChi(rv.playerId, d, rv.choices, g);
          if (cc) { simMeld(g, rv.playerId, 'chi', cc, d);
            const cd = chooseDiscard(g.players[rv.playerId].hand, rv.playerId, g);
            simDiscard(g, rv.playerId, cd); sortTiles(g.players[rv.playerId].hand); }
        } else {
          simMeld(g, rv.playerId, rv.type, null, d);
          const rp = g.players[rv.playerId];
          if (rv.type === 'gang') {
            const bt = simDraw(g, rv.playerId);
            if (bt) { rp.hand.push(bt); sortTiles(rp.hand);
              if (isWinningWithMelds(rp.hand, rp.melds, WILD_KEY)) { simWin(g, rv.playerId, '杠开', null, bt); break; } }
          }
          const rd = chooseDiscard(rp.hand, rv.playerId, g);
          simDiscard(g, rv.playerId, rd); sortTiles(rp.hand);
        }
        turn = rv.playerId; turn++; continue;
      }
    }
    turn++;
  }
  if (!g.winner && g.tilePool.length === 0) g.drawReason = '流局';
  return g;
}

const WINDS = ['东', '南', '西', '北'];
const SUIT_NAMES = { character: '万', dot: '筒', bamboo: '条' };
function tDesc(t) {
  if (t.suit === 'honor') return { east:'东',south:'南',west:'西',north:'北',red:'中',green:'发',white:'白' }[t.rank];
  return t.rank + SUIT_NAMES[t.suit];
}

let totalScore = 0;

for (let g = 1; g <= 10; g++) {
  const game = runOneGame();
  const w = game.winner;
  const p = w ? game.players[w.playerId] : null;

  if (w) {
    const wind = WINDS[w.playerId];
    const patterns = w.flags && w.flags.length > 0 ? w.flags.join('+') : '平胡';
    const piaoTag = p.piaoCount > 0 ? ' 一飘' : '';
    const handDesc = p ? p.hand.map(tDesc).join(' ') : '';

    console.log('第' + g + '局: ' + wind + '家 ' + w.kind + '胡 | ' + patterns + piaoTag + ' | ×' + w.multiplier + ' = ' + w.total + '分');
    if (p.melds.length > 0) {
      const meldDescs = p.melds.map(m => '[' + m.type + ':' + m.tiles.map(tDesc).join('') + ']').join(' ');
      console.log('      副露: ' + meldDescs + '  手牌: ' + handDesc);
    }

    const p0 = game.players[0];
    const p1 = game.players[1];
    const p2 = game.players[2];
    const p3 = game.players[3];
    console.log('      分数: 你(' + p0.score + ') 下家(' + p1.score + ') 对家(' + p2.score + ') 上家(' + p3.score + ')');
    totalScore += p0.score + p1.score + p2.score + p3.score;
  } else {
    console.log('第' + g + '局: 流局 (牌墙摸完)');
  }
}
console.log('总分池: ' + totalScore + ' (应为 1000)');
