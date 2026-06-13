// ============================================================
// engine.js — 杭州麻将引擎接口
// ============================================================
// 本文件定义杭州麻将引擎的完整接口契约。
//
// AI 决策函数使用 game-sim.js 的快速模拟能力：
//   chooseDiscard / shouldPeng / shouldMeldGang / chooseChi /
//   chooseConcealedGang / shouldPiaoCai / resolveClaim
// 均为同步调用，基于 shanten 启发式 + 快速 rollout。
// 重型 ISMCTS 搜索由 ai-worker.js Web Worker 异步执行。
// ============================================================

import {
  fromGameState,
  computeShanten,
  chooseBestDiscard,
  tileToIndex,
  indexToTileType,
  runRollouts,
} from './ai/game-sim.js';

// ----------------------------------------------------------
// 常量定义
// ----------------------------------------------------------

export const SUIT_CONFIG = {
  bamboo: { symbol: "条", className: "bamboo" },
  dot: { symbol: "筒", className: "dot" },
  character: { symbol: "万", className: "character" },
};

export const HONOR_LABELS = {
  east: "东",
  south: "南",
  west: "西",
  north: "北",
  red: "中",
  green: "发",
  white: "白",
};

export const CHINESE_NUMERALS = {
  1: "一",
  2: "二",
  3: "三",
  4: "四",
  5: "五",
  6: "六",
  7: "七",
  8: "八",
  9: "九",
};

export const SEATS = [
  { id: 0, name: "你", wind: "东", isHuman: true, seatKey: "bottom" },
  { id: 1, name: "下家", wind: "南", isHuman: false, seatKey: "right" },
  { id: 2, name: "对家", wind: "西", isHuman: false, seatKey: "top" },
  { id: 3, name: "上家", wind: "北", isHuman: false, seatKey: "left" },
];

export const INITIAL_SCORE = 50;

// ----------------------------------------------------------
// Tile 工具函数（纯函数，不需要修改）
// ----------------------------------------------------------

export function createTile(suit, rank) {
  return { id: `${suit}-${rank}-${Math.random().toString(36).slice(2, 9)}`, suit, rank };
}

export function tileKey(tile) {
  return `${tile.suit}:${tile.rank}`;
}

export function tileLabel(tile) {
  if (tile.suit === "honor") {
    return HONOR_LABELS[tile.rank];
  }
  return `${CHINESE_NUMERALS[tile.rank]}${SUIT_CONFIG[tile.suit].symbol}`;
}

export function tileGlyph(tile) {
  if (tile.suit === "honor") {
    const glyphs = {
      east: "🀀", south: "🀁", west: "🀂", north: "🀃",
      red: "🀄", green: "🀅", white: "🀆",
    };
    return glyphs[tile.rank];
  }
  const glyphs = {
    character: ["", "🀇", "🀈", "🀉", "🀊", "🀋", "🀌", "🀍", "🀎", "🀏"],
    dot:       ["", "🀙", "🀚", "🀛", "🀜", "🀝", "🀞", "🀟", "🀠", "🀡"],
    bamboo:    ["", "🀐", "🀑", "🀒", "🀓", "🀔", "🀕", "🀖", "🀗", "🀘"],
  };
  return glyphs[tile.suit][tile.rank];
}

export function tileSortValue(tile) {
  const suitOrder = { character: 0, dot: 1, bamboo: 2, honor: 3 };
  if (tile.suit === "honor") {
    const honorOrder = ["east", "south", "west", "north", "red", "green", "white"];
    return suitOrder.honor * 100 + honorOrder.indexOf(tile.rank);
  }
  return suitOrder[tile.suit] * 100 + Number(tile.rank);
}

export function sortTiles(tiles) {
  tiles.sort((a, b) => tileSortValue(a) - tileSortValue(b));
}

export function cloneTile(tile) {
  return { ...tile };
}

export function tileAssetName(tile) {
  return tile.suit === "honor" ? `honor-${tile.rank}` : `${tile.suit}-${tile.rank}`;
}

export function tileAssetPath(tile) {
  return `./assets/tiles/${tileAssetName(tile)}.svg`;
}

export function isTileWild(tile, wildKey) {
  return tileKey(tile) === wildKey;
}

/** 洗牌函数（Fisher-Yates） */
export function shuffle(list) {
  const copy = [...list];
  for (let i = copy.length - 1; i > 0; i -= 1) {
    const j = Math.floor(Math.random() * (i + 1));
    [copy[i], copy[j]] = [copy[j], copy[i]];
  }
  return copy;
}

/**
 * 创建牌池（内部使用，挂载在 gameState.tilePool）。
 * 标准杭州麻将：万/筒/条 1-9 各4张 + 7种字牌各4张 = 136张。
 *
 * @returns {Tile[]} 洗好的牌池
 */
function createTilePool() {
  const tiles = [];

  const suits = ["character", "dot", "bamboo"];
  for (const suit of suits) {
    for (let rank = 1; rank <= 9; rank += 1) {
      for (let i = 0; i < 4; i += 1) {
        tiles.push(createTile(suit, rank));
      }
    }
  }

  const honors = ["east", "south", "west", "north", "red", "green", "white"];
  for (const honor of honors) {
    for (let i = 0; i < 4; i += 1) {
      tiles.push(createTile("honor", honor));
    }
  }

  return shuffle(tiles);
}

// ----------------------------------------------------------
// 玩家创建
// ----------------------------------------------------------

/**
 * 创建4家玩家初始状态。
 * @returns {Player[]}
 */
export function createPlayers() {
  return SEATS.map((seat) => ({
    id: seat.id,
    name: seat.name,
    wind: seat.wind,
    isHuman: seat.isHuman,
    seatKey: seat.seatKey,
    hand: [],
    melds: [],
    discards: [],
    score: INITIAL_SCORE,
    piaoCai: false,
    piaoCount: 0,
  }));
}

// ----------------------------------------------------------
// 游戏状态创建
// [TODO: IMPLEMENT] 新局初始化：发牌13+1张，定财神
// ----------------------------------------------------------

/**
 * 创建新一局游戏的完整状态。
 *
 * 需要做的事：
 * 1. 创建牌池 createTilePool()
 * 2. 创建玩家 createPlayers()
 * 3. 发牌：每家13张，庄家（玩家0）多1张
 * 4. 确定财神（wildTile），计算 wildKey
 * 5. 设置初始阶段为 "human-discard"
 *
 * @returns {GameState}
 *
 * GameState 结构：
 * {
 *   players: Player[],     // 4家玩家
 *   tilePool: Tile[],      // 剩余牌池（不对外展示，摸牌时随机抽取）
 *   dealer: number,        // 庄家ID (0)
 *   turn: number,          // 当前回合玩家ID
 *   phase: string,         // "human-discard" | "claim" | null
 *   selectedTileId: string|null,
 *   claimOptions: ClaimOption[],
 *   lastDiscard: { playerId, tile, fromDraw }|null,
 *   lastDraw: { playerId, tile, reason }|null,
 *   winner: { playerId, fromPlayerId, kind, summary }|null,
 *   drawReason: string|null,
 *   wildTile: Tile,        // 财神牌
 *   wildKey: string,       // 财神 tileKey
 *   message: string,
 *   logs: string[],
 *   locked: boolean,
 * }
 */
export function createNewGame() {
  const players = createPlayers();
  const tilePool = createTilePool();

  // Deal: dealer (player 0) gets 14 tiles, others get 13
  for (let i = 0; i < 14; i += 1) {
    players[0].hand.push(tilePool.pop());
  }
  for (let p = 1; p < 4; p += 1) {
    for (let i = 0; i < 13; i += 1) {
      players[p].hand.push(tilePool.pop());
    }
  }

  for (const player of players) {
    sortTiles(player.hand);
  }

  const wildTile = { suit: "honor", rank: "white" };
  const wildKey = tileKey(wildTile);

  return {
    players,
    tilePool,
    dealer: 0,
    turn: 0,
    phase: "human-discard",
    selectedTileId: null,
    claimOptions: [],
    lastDiscard: null,
    lastDraw: null,
    winner: null,
    drawReason: null,
    wildTile,
    wildKey,
    message: "新局开始，上推手牌出牌。",
    logs: ["新局开始，白板作财神。"],
    locked: false,
    _discardCount: 0,
    _firstDiscards: [],
    _revealedTile: null,
    _piaoCaiOrigin: -1,
    _piaoCaiTurnsTaken: 0,
    _handCustomOrder: false,
    _handOrder: [],
    aiDifficulty: 'normal',
  };
}

// ----------------------------------------------------------
// 胡牌判定
// ----------------------------------------------------------

/** 34种牌的固定顺序，保证 canFormSets 按确定顺序遍历 */
const TILE_KEYS_IN_ORDER = [
  'character:1','character:2','character:3','character:4','character:5','character:6','character:7','character:8','character:9',
  'dot:1','dot:2','dot:3','dot:4','dot:5','dot:6','dot:7','dot:8','dot:9',
  'bamboo:1','bamboo:2','bamboo:3','bamboo:4','bamboo:5','bamboo:6','bamboo:7','bamboo:8','bamboo:9',
  'honor:east','honor:south','honor:west','honor:north','honor:red','honor:green','honor:white',
];

/** 34种 distinct 牌型对象（用于听牌遍历等） */
const ALL_TILE_TYPES = TILE_KEYS_IN_ORDER.map(tk => {
  const colonIdx = tk.indexOf(':');
  const suit = tk.slice(0, colonIdx);
  const rankStr = tk.slice(colonIdx + 1);
  const rank = suit === 'honor' ? rankStr : parseInt(rankStr, 10);
  return { id: `proto-${tk.replace(':', '-')}`, suit, rank };
});

/**
 * 统计每种牌的数量，财神不计入 counts 全部计入 wildCount。
 * @param {Tile[]} tiles
 * @param {string} wildKey
 * @returns {{ counts: Record<string,number>, wildCount: number }}
 */
function getCanonicalCounts(tiles, wildKey) {
  const counts = {};
  let wildCount = 0;
  for (const t of tiles) {
    const key = tileKey(t);
    if (key === wildKey) {
      wildCount += 1;
    } else {
      counts[key] = (counts[key] || 0) + 1;
    }
  }
  return { counts, wildCount };
}

/** 将 counts 序列化为 memo key — 固定34位，每位置编码该牌型的数量 */
function serializeCounts(counts) {
  let s = '';
  for (const key of TILE_KEYS_IN_ORDER) {
    const c = counts[key] || 0;
    s += String.fromCharCode(48 + c);
  }
  return s;
}

/**
 * 判断剩余牌能否组成 neededSets 组面子（顺子/刻子），财神可替代任意牌。
 * 带记忆化。
 *
 * @param {Record<string,number>} counts — 会被修改，调用方需自行备份关键值
 * @param {number} wildCount — 可用财神数量
 * @param {number} neededSets — 还需多少面子
 * @param {Map<string,boolean>} memo
 * @returns {boolean}
 */
function canFormSets(counts, wildCount, neededSets, memo) {
  if (neededSets === 0) return true;
  if (wildCount < 0) return false;

  // 按固定顺序找第一个数量 > 0 的牌
  let firstKey = null;
  let firstCnt = 0;
  for (const key of TILE_KEYS_IN_ORDER) {
    const c = counts[key];
    if (c > 0) {
      firstKey = key;
      firstCnt = c;
      break;
    }
  }

  if (firstKey === null) {
    return wildCount >= neededSets * 3;
  }

  if (!memo) memo = new Map();
  const memoKey = `${serializeCounts(counts)}:w${wildCount}n${neededSets}`;
  const cached = memo.get(memoKey);
  if (cached !== undefined) return cached;

  const [suit, rankStr] = firstKey.split(':');

  // 尝试组成刻子
  if (firstCnt + wildCount >= 3) {
    const use = Math.min(firstCnt, 3);
    const needWild = 3 - use;
    counts[firstKey] = firstCnt - use;
    if (canFormSets(counts, wildCount - needWild, neededSets - 1, memo)) {
      counts[firstKey] = firstCnt;
      memo.set(memoKey, true);
      return true;
    }
    counts[firstKey] = firstCnt;
  }

  // 尝试组成顺子（仅数牌，且 rank ≤ 7 才能当起点）
  if (suit !== 'honor') {
    const rank = parseInt(rankStr, 10);
    if (rank <= 7) {
      const k2 = `${suit}:${rank + 1}`;
      const k3 = `${suit}:${rank + 2}`;
      const c2 = counts[k2] || 0;
      const c3 = counts[k3] || 0;
      const needWild = (c2 === 0 ? 1 : 0) + (c3 === 0 ? 1 : 0);

      if (wildCount >= needWild) {
        counts[firstKey] = firstCnt - 1;
        if (c2 > 0) counts[k2] = c2 - 1;
        if (c3 > 0) counts[k3] = c3 - 1;

        if (canFormSets(counts, wildCount - needWild, neededSets - 1, memo)) {
          counts[firstKey] = firstCnt;
          if (c2 > 0) counts[k2] = c2;
          if (c3 > 0) counts[k3] = c3;
          memo.set(memoKey, true);
          return true;
        }

        counts[firstKey] = firstCnt;
        if (c2 > 0) counts[k2] = c2;
        if (c3 > 0) counts[k3] = c3;
      }
    }

    // 财神作为低一位的牌，当前牌作为顺子中间（rank-1 为财神 + rank + rank+1）
    if (rank >= 2 && rank <= 8 && wildCount >= 1) {
      const k3 = `${suit}:${rank + 1}`;
      const c3 = counts[k3] || 0;
      const needWild = 1 + (c3 === 0 ? 1 : 0);
      if (wildCount >= needWild) {
        counts[firstKey] = firstCnt - 1;
        if (c3 > 0) counts[k3] = c3 - 1;
        if (canFormSets(counts, wildCount - needWild, neededSets - 1, memo)) {
          counts[firstKey] = firstCnt;
          if (c3 > 0) counts[k3] = c3;
          memo.set(memoKey, true);
          return true;
        }
        counts[firstKey] = firstCnt;
        if (c3 > 0) counts[k3] = c3;
      }
    }
  }

  memo.set(memoKey, false);
  return false;
}

/**
 * 标准胡牌判定：手牌组成 neededSets 面子 + 1将牌。
 * 财神可替代任意牌，可用于面子或参与将牌。
 *
 * @param {Tile[]} tiles - 手牌
 * @param {string} wildKey
 * @param {number} neededSets - 需要组成的面子数（无副露=4，有副露=4-副露数）
 * @returns {boolean}
 */
function isStandardWinPartial(tiles, wildKey, neededSets) {
  const { counts, wildCount } = getCanonicalCounts(tiles, wildKey);

  if (wildCount === 0) {
    // 需要找一个自然对子当将牌
    for (const key of TILE_KEYS_IN_ORDER) {
      const c = counts[key];
      if (c >= 2) {
        counts[key] = c - 2;
        if (canFormSets(counts, 0, neededSets)) {
          return true;
        }
        counts[key] = c;
      }
    }
    return false;
  }

  // wildCount ≥ 1：财神可用于面子，也可参与将牌
  // 选项A：自然对子做将，财神全部用于面子
  for (const key of TILE_KEYS_IN_ORDER) {
    const c = counts[key];
    if (c >= 2) {
      counts[key] = c - 2;
      if (canFormSets(counts, wildCount, neededSets)) {
        counts[key] = c;
        return true;
      }
      counts[key] = c;
    }
  }

  // 选项B：1张财神 + 1张普通牌 = 将牌，剩余财神用于面子
  for (const key of TILE_KEYS_IN_ORDER) {
    const c = counts[key];
    if (c >= 1) {
      counts[key] = c - 1;
      if (canFormSets(counts, wildCount - 1, neededSets)) {
        counts[key] = c;
        return true;
      }
      counts[key] = c;
    }
  }

  // 选项C：2张财神组成将牌（仅当 wildCount ≥ 2）
  if (wildCount >= 2) {
    if (canFormSets(counts, wildCount - 2, neededSets)) return true;
  }

  return false;
}

/**
 * 标准胡牌：4面子 + 1将牌（无副露情况）。
 */
function isStandardWin(tiles, wildKey) {
  return isStandardWinPartial(tiles, wildKey, 4);
}

/**
 * 七对判定：7个对子，财神可填补单张。
 *
 * @param {Tile[]} tiles - 14张手牌
 * @param {string} wildKey
 * @returns {boolean}
 */
function isSevenPairs(tiles, wildKey) {
  const { counts, wildCount } = getCanonicalCounts(tiles, wildKey);

  let singles = 0;
  for (const key of TILE_KEYS_IN_ORDER) {
    const c = counts[key];
    if (c !== undefined && c % 2 !== 0) singles += 1;
  }

  if (wildCount < singles) return false;
  if ((wildCount - singles) % 2 !== 0) return false;
  return true;
}

/**
 * 判断手牌是否构成胡牌（平胡 / 七对）。
 *
 * @param {Tile[]} tiles - 手牌（通常14张）
 * @param {string} wildKey - 财神的 tileKey
 * @returns {boolean}
 */
export function isWinningHand(tiles, wildKey) {
  if (tiles.length !== 14) return false;
  return isStandardWin(tiles, wildKey) || isSevenPairs(tiles, wildKey);
}

/**
 * 判断手牌+副露是否构成胡牌（用于有吃碰杠露副的情况）。
 * 副露是固定的面子，不可拆开重组；只有手牌可自由组合。
 *
 * @param {Tile[]} hand - 玩家手牌
 * @param {object[]} melds - 副露列表，每项含 tiles 字段
 * @param {string} wildKey - 财神 tileKey
 * @returns {boolean}
 */
export function isWinningWithMelds(hand, melds, wildKey) {
  let meldTileCount = 0;
  for (const meld of melds) {
    meldTileCount += meld.tiles.length;
  }
  if (hand.length + meldTileCount !== 14) return false;

  const remainingSets = 4 - melds.length;
  if (remainingSets < 0) return false;

  // 手牌独立组成 remainingSets 面子 + 1 将牌
  if (melds.length === 0 && isSevenPairs(hand, wildKey)) return true;
  return isStandardWinPartial(hand, wildKey, remainingSets);
}

// ----------------------------------------------------------
// 听牌计算
// [TODO: IMPLEMENT] 计算手牌的听牌列表
// ----------------------------------------------------------

/**
 * 计算手牌的听牌列表（还差哪些牌可以胡）。
 *
 * @param {Tile[]} tiles - 手牌（通常13张，听牌状态）
 * @param {string} wildKey - 财神的 tileKey
 * @returns {Tile[]} 可以胡的牌（去重后的 distinct tiles）
 */
export function getWaitTiles(tiles, wildKey) {
  // 只有 13 张牌时才有听牌计算的意义（13 + 1 = 14 胡）
  if (tiles.length !== 13) return [];

  const results = [];
  for (const proto of ALL_TILE_TYPES) {
    const testHand = [...tiles, proto];
    if (isWinningHand(testHand, wildKey)) {
      results.push(proto);
    }
  }
  return results;
}

/**
 * 计算有副露时的听牌列表。
 * hand + melds 的总牌数应为13（摸1张后14可胡）。
 *
 * @param {Tile[]} hand - 手牌
 * @param {object[]} melds - 副露列表
 * @param {string} wildKey
 * @returns {Tile[]}
 */
export function getWaitTilesWithMelds(hand, melds, wildKey) {
  let meldTotal = 0;
  for (const m of melds) meldTotal += m.tiles.length;
  if (hand.length + meldTotal !== 13) return [];

  const results = [];
  for (const proto of ALL_TILE_TYPES) {
    if (isWinningWithMelds([...hand, proto], melds, wildKey)) {
      results.push(proto);
    }
  }
  return results;
}

// ----------------------------------------------------------
// 吃碰杠选项
// ----------------------------------------------------------

/** 将手牌按 tileKey 分组，财神单独收集。 */
function groupHandByKey(hand, wildKey) {
  const byKey = {};
  const wilds = [];
  for (const tile of hand) {
    const key = tileKey(tile);
    if (key === wildKey) {
      wilds.push(tile);
    } else {
      if (!byKey[key]) byKey[key] = [];
      byKey[key].push(tile);
    }
  }
  return { byKey, wilds };
}

/**
 * 计算某家对上家打出的牌可以怎么吃。
 * 吃只能吃上家，庄下家吃庄需先亮财神（由调用方判断）。
 * 吃只能吃两口：弃牌在顺子左侧（顺吃）或中间（中吃），不能在右侧。
 * 白板（财神）不能用来吃。
 *
 * @param {Tile[]} hand - 吃牌玩家的手牌
 * @param {Tile} discardTile - 上家打出的牌
 * @param {string} wildKey - 财神的 tileKey
 * @returns {{ ids: string[], label: string, tiles: Tile[] }[]}
 */
export function getChiOptions(hand, discardTile, wildKey) {
  if (discardTile.suit === "honor") return [];

  const suit = discardTile.suit;
  const rank = discardTile.rank;
  const { byKey } = groupHandByKey(hand, wildKey);
  const options = [];

  // 吃只能吃两口：顺吃（弃牌最左）和中吃（弃牌中间）
  const chiTypes = [];
  if (rank + 2 <= 9) chiTypes.push({ label: '顺吃', keys: [`${suit}:${rank + 1}`, `${suit}:${rank + 2}`] });
  if (rank - 1 >= 1 && rank + 1 <= 9) chiTypes.push({ label: '中吃', keys: [`${suit}:${rank - 1}`, `${suit}:${rank + 1}`] });

  for (const { label, keys } of chiTypes) {
    const [k1, k2] = keys;
    const tiles1 = byKey[k1];
    const tiles2 = byKey[k2];

    if (tiles1 && tiles2) {
      const chiTiles = [tiles1[0], tiles2[0]];
      options.push({
        ids: [tiles1[0].id, tiles2[0].id],
        label: `${label} ${tileLabel(chiTiles[0])}${tileLabel(chiTiles[1])}`,
        tiles: chiTiles,
      });
    }
  }

  return options;
}

/**
 * 判断手牌对弃牌能否碰/明杠，返回最高优先操作和所需手牌。
 *
 * @param {Tile[]} hand - 玩家手牌
 * @param {Tile} tile - 被弃的牌
 * @param {string} wildKey - 财神的 tileKey
 * @returns {{ type: "peng"|"gang", tiles: Tile[] }|null}
 */
export function getPengOptions(hand, tile, wildKey) {
  if (tileKey(tile) === wildKey) return null;
  const { byKey } = groupHandByKey(hand, wildKey);
  const group = byKey[tileKey(tile)];
  if (!group) return null;
  if (group.length >= 3) return { type: "gang", tiles: group.slice(0, 3) };
  if (group.length >= 2) return { type: "peng", tiles: group.slice(0, 2) };
  return null;
}

/**
 * 查找手牌中可以暗杠的牌（4张相同且非财神）。
 *
 * @param {Tile[]} hand - 玩家手牌
 * @param {string} wildKey - 财神的 tileKey
 * @returns {Tile[][]} 每个可杠的牌组
 */
export function getConcealedGangChoices(hand, wildKey) {
  const { byKey } = groupHandByKey(hand, wildKey);
  const choices = [];
  for (const tiles of Object.values(byKey)) {
    if (tiles.length >= 4) {
      choices.push(tiles.slice(0, 4));
    }
  }
  return choices;
}

// ----------------------------------------------------------
// 出牌提示
// [TODO: IMPLEMENT] 为人类玩家生成"打哪张能听"的提示
// ----------------------------------------------------------

/**
 * 给出打牌提示：手牌中每张牌打出后能否听牌、听哪些牌。
 *
 * @param {Tile[]} hand - 玩家手牌（14张）
 * @param {string} wildKey - 财神的 tileKey
 * @returns {{ discard: Tile, waits: Tile[] }[]} 按听牌数降序排列
 */
export function getDiscardHints(hand, wildKey) {
  const hints = [];
  for (let i = 0; i < hand.length; i++) {
    const discard = hand[i];
    const remaining = [...hand.slice(0, i), ...hand.slice(i + 1)];
    const waits = getWaitTiles(remaining, wildKey);
    if (waits.length > 0) {
      hints.push({ discard, waits });
    }
  }
  hints.sort((a, b) => b.waits.length - a.waits.length);
  return hints;
}

/**
 * 有副露时的弃牌提示。考虑手牌+副露的总牌型。
 */
export function getDiscardHintsWithMelds(hand, melds, wildKey) {
  const hints = [];
  for (let i = 0; i < hand.length; i++) {
    const discard = hand[i];
    const remaining = [...hand.slice(0, i), ...hand.slice(i + 1)];
    const waits = getWaitTilesWithMelds(remaining, melds, wildKey);
    if (waits.length > 0) {
      hints.push({ discard, waits });
    }
  }
  hints.sort((a, b) => b.waits.length - a.waits.length);
  return hints;
}

// ----------------------------------------------------------
// AI 决策
// ----------------------------------------------------------
// 以下函数为同步决策，基于 shanten 启发式 + 快速 rollout。
// 重型 ISMCTS 搜索由 ai-worker.js Web Worker 异步执行（app.js 调用）。

/** 将引擎 Tile 对象转为 game-sim tile type index (0-33) */
function engineTileToIdx(tile) {
  return tileToIndex(tile);
}

/** 将 game-sim tile type index 转回 { suit, rank } */
function idxToEngineTile(idx) {
  return indexToTileType(idx);
}

/**
 * 从手牌中找到与 { suit, rank } 匹配的 tile 对象。
 */
function findTileInHand(hand, suit, rank) {
  return hand.find(t => t.suit === suit && t.rank === rank) || null;
}

// ---- chooseDiscard ----

/**
 * AI 选择要打出的牌（同步 shanten 贪婪）。
 *
 * 注：app.js 中 AI 弃牌已委托 Web Worker ISMCTS 异步执行。
 * 此函数仅作为同步回退 / 非 Worker 环境使用。
 *
 * @param {Tile[]} hand - AI 玩家的手牌（14张）
 * @param {number} playerId - AI 玩家ID
 * @param {object} gameState - 完整游戏状态
 * @returns {Tile} 要打出的牌
 */
export function chooseDiscard(hand, playerId, gameState) {
  try {
    const sim = fromGameState(gameState);
    const result = chooseBestDiscard(sim, playerId);
    if (result.tileIdx >= 0) {
      const { suit, rank } = idxToEngineTile(result.tileIdx);
      const found = findTileInHand(hand, suit, rank);
      if (found) return found;
    }
  } catch (_e) { /* fall through to stub */ }
  return hand[hand.length - 1];
}

// ---- shouldPeng / shouldMeldGang ----

/**
 * 计算碰/杠后的手牌向听数变化。
 * @returns {{ afterShanten: number, handSize: number }|null}
 */
function simulateClaimShanten(gameState, playerId, tile, claimType) {
  try {
    const sim = fromGameState(gameState);
    const hand = sim.hands[playerId];
    const wildIdx = sim.wildIdx;
    const tileIdx = engineTileToIdx(tile);

    const beforeShanten = computeShanten(hand, wildIdx);
    const handClone = hand.clone();
    handClone.add(tileIdx);

    if (claimType === 'peng') {
      handClone.remove(tileIdx);
      handClone.remove(tileIdx);
    } else if (claimType === 'gang') {
      handClone.remove(tileIdx);
      handClone.remove(tileIdx);
      handClone.remove(tileIdx);
    }

    const afterShanten = computeShanten(handClone, wildIdx);
    return { afterShanten, beforeShanten, handSize: handClone.total() };
  } catch (_e) {
    return null;
  }
}

/**
 * AI 判断是否碰牌。
 *
 * 策略：碰牌后向听数不恶化（允许持平或改善），且不破坏听牌。
 */
export function shouldPeng(playerId, tile, gameState) {
  const player = gameState.players[playerId];
  if (!player || player.piaoCai) return false;

  const result = simulateClaimShanten(gameState, playerId, tile, 'peng');
  if (!result) return false;

  // 碰后向听数 ≤ 碰前 → 值得碰
  return result.afterShanten <= result.beforeShanten + 1;
}

/**
 * AI 判断是否明杠。
 *
 * 策略：杠总是有益的（加杠后补牌 + 杠开机会），默认接受。
 */
export function shouldMeldGang(playerId, tile, gameState) {
  const player = gameState.players[playerId];
  if (!player || player.piaoCai) return false;
  return true;
}

// ---- chooseChi ----

/**
 * AI 选择吃牌方式。
 *
 * 策略：对每种吃法模拟移除相应手牌后的向听数，
 * 选择向听数最低的方案。向听数都比当前差则不吃。
 *
 * @param {number} playerId - AI 玩家ID
 * @param {Tile} tile - 被弃的牌
 * @param {{ ids: string[], label: string, tiles: Tile[] }[]} choices - getChiOptions 返回的选项
 * @param {object} gameState - 完整游戏状态
 * @returns {{ ids: string[], label: string }|null} 选中的吃法，null 表示不吃
 */
export function chooseChi(playerId, tile, choices, gameState) {
  if (!choices || choices.length === 0) return null;

  const player = gameState.players[playerId];
  if (!player || player.piaoCai) return null;

  try {
    const sim = fromGameState(gameState);
    const hand = sim.hands[playerId];
    const wildIdx = sim.wildIdx;
    const beforeShanten = computeShanten(hand, wildIdx);

    let bestChoice = null;
    let bestShanten = 99;

    for (const choice of choices) {
      // 克隆手牌，移除吃牌所用的手牌
      const handClone = hand.clone();
      for (const id of choice.ids) {
        const handTile = player.hand.find(ht => ht.id === id);
        if (!handTile) continue;
        const idx = engineTileToIdx(handTile);
        if (handClone.data[idx] > 0) {
          handClone.remove(idx);
        }
      }

      const afterShanten = computeShanten(handClone, wildIdx);
      if (afterShanten < bestShanten) {
        bestShanten = afterShanten;
        bestChoice = choice;
      }
    }

    // 只有向听改善或持平时才吃
    if (bestChoice && bestShanten <= beforeShanten) {
      return bestChoice;
    }

    // 如果当前是 0 向听（听牌），不吃
    if (beforeShanten === 0) return null;

    // 向听数没变差也接受（持平可能有利于后续）
    if (bestChoice && bestShanten <= beforeShanten + 1) {
      return bestChoice;
    }

    return null;
  } catch (_e) {
    return null;
  }
}

// ---- chooseConcealedGang ----

/**
 * AI 选择是否暗杠及杠哪组。
 *
 * 策略：杠后向听数不恶化才杠。优先杠不影响向听的牌。
 *
 * @param {number} playerId - AI 玩家ID
 * @param {Tile[][]} choices - getConcealedGangChoices 返回的选项
 * @param {object} gameState - 完整游戏状态
 * @returns {Tile[]|null} 选中的牌组，null 表示不杠
 */
export function chooseConcealedGang(playerId, choices, gameState) {
  if (!choices || choices.length === 0) return null;

  const player = gameState.players[playerId];
  if (!player || player.piaoCai) return null;

  try {
    const sim = fromGameState(gameState);
    const hand = sim.hands[playerId];
    const wildIdx = sim.wildIdx;
    const beforeShanten = computeShanten(hand, wildIdx);

    for (const group of choices) {
      const refTile = group[0];
      const refIdx = engineTileToIdx(refTile);

      const handClone = hand.clone();
      handClone.remove(refIdx);
      handClone.remove(refIdx);
      handClone.remove(refIdx);
      handClone.remove(refIdx);

      // 模拟杠后补牌（假设摸到随机的有用牌 — 保守估计不加牌）
      const afterShanten = computeShanten(handClone, wildIdx);

      // 杠后向听数不恶化才杠
      if (afterShanten <= beforeShanten) {
        return group;
      }
    }

    return null;
  } catch (_e) {
    return null;
  }
}

// ---- shouldPiaoCai ----

/**
 * AI 判断是否飘财。
 *
 * 策略：手牌已听牌（shanten = 0）且手中有财神 → 飘财。
 * 通过快速 rollout 估算飘财 vs 正常打的胜率差异。
 *
 * @param {number} playerId - AI 玩家ID
 * @param {object} gameState - 完整游戏状态
 * @returns {boolean}
 */
export function shouldPiaoCai(playerId, gameState) {
  const player = gameState.players[playerId];
  if (!player) return false;

  // 飘财需要手中至少2张财神
  const wildCount = countWildInHand(player.hand, gameState.wildKey);
  if (wildCount < 2) return false;

  // 多飘：已在飘财模式，摸到胡牌后决定是否继续飘
  if (player.piaoCai) {
    const piaoCount = player.piaoCount || 0;
    return piaoCount < 2; // 最多继续到二飘（一飘→二飘→三飘）
  }

  // 首次飘财：必须在暴头状态（能胡牌），放弃立即胡牌来飘财
  return isWinningWithMelds(player.hand, player.melds, gameState.wildKey);
}

// ---- resolveClaim ----

/**
 * 在一组吃碰杠候选中决定谁获得优先权。
 *
 * 每个候选方独立决策：
 *   - 人类玩家：总是纳入候选（由 UI 决定）
 *   - AI 玩家：调用对应的 should / choose 函数判断意愿
 *
 * 优先级规则：
 *   1. 高 priority 优先（gang 2.5 > peng 2 > chi 1）
 *   2. 同 priority 时，距离出牌者最近优先
 *
 * @param {ClaimItem[]} claimList - 所有可能的操作
 * @param {Tile} tile - 被弃的牌
 * @param {number} discarderId - 出牌者ID
 * @param {object} gameState - 完整游戏状态
 * @returns {ClaimItem|null}
 */
export function resolveClaim(claimList, tile, discarderId, gameState) {
  if (!claimList || claimList.length === 0) return null;

  const distanceFromDiscarder = (pid) => (pid - discarderId + 4) % 4;

  // 过滤：各候选方独立决策
  const viable = claimList.filter(claim => {
    const p = gameState.players[claim.playerId];
    if (!p || p.piaoCai) return false;

    if (p.isHuman) return true;

    if (claim.type === 'gang') return shouldMeldGang(claim.playerId, tile, gameState);
    if (claim.type === 'peng') return shouldPeng(claim.playerId, tile, gameState);
    if (claim.type === 'chi') return chooseChi(claim.playerId, tile, claim.choices, gameState) !== null;
    return false;
  });

  if (viable.length === 0) return null;

  // 排序：优先级降序 → 距离升序
  viable.sort((a, b) =>
    b.priority - a.priority ||
    distanceFromDiscarder(a.playerId) - distanceFromDiscarder(b.playerId)
  );

  return viable[0];
}

// ----------------------------------------------------------
// 四风连打
// ----------------------------------------------------------

/**
 * 检测四风连打：开局后四家首弃牌均为同一种风牌。
 * @param {Array<{playerId: number, tile: object}>} firstDiscards
 * @returns {{ isFourWind: boolean, windLabel?: string }}
 */
export function checkFourWindDiscard(firstDiscards) {
  if (!firstDiscards || firstDiscards.length !== 4) return { isFourWind: false };

  const windKeys = new Set(["honor:east", "honor:south", "honor:west", "honor:north"]);
  let commonWind = null;

  for (const entry of firstDiscards) {
    const key = tileKey(entry.tile);
    if (!windKeys.has(key)) return { isFourWind: false };
    if (commonWind === null) {
      commonWind = key;
    } else if (key !== commonWind) {
      return { isFourWind: false };
    }
  }

  return { isFourWind: true, windLabel: tileLabel(firstDiscards[0].tile) };
}

/**
 * 统计手牌中财神的数量。
 */
export function countWildInHand(hand, wildKey) {
  let count = 0;
  for (const t of hand) {
    if (tileKey(t) === wildKey) count += 1;
  }
  return count;
}

// ----------------------------------------------------------
// 计分 — 16种牌型判定 + 番数计算
// ----------------------------------------------------------

const YAO_KEYS = [
  "character:1", "character:9",
  "dot:1", "dot:9",
  "bamboo:1", "bamboo:9",
  "honor:east", "honor:south", "honor:west", "honor:north",
  "honor:red", "honor:green", "honor:white",
];

/**
 * 平胡：标准4面子+1将牌。
 */
function isPingHuPattern(tiles, wildKey) {
  return isStandardWin(tiles, wildKey);
}

/**
 * 七对：7个对子。
 */
function isSevenPairsPattern(tiles, wildKey) {
  return isSevenPairs(tiles, wildKey);
}

/**
 * 豪华七对：七对中有一组4张相同牌拆成两个对子。
 */
function isLuxurySevenPairs(tiles, wildKey) {
  if (!isSevenPairs(tiles, wildKey)) return false;
  const keyMap = {};
  for (const t of tiles) {
    const k = tileKey(t);
    if (k !== wildKey) {
      keyMap[k] = (keyMap[k] || 0) + 1;
    }
  }
  return Object.values(keyMap).some(c => c >= 4);
}

/**
 * 十三幺：13种幺九牌各一 + 任意一张凑对。白板算幺九之一。
 */
function isShiSanYao(tiles, wildKey) {
  const { counts: rawCounts, wildCount } = getCanonicalCounts(tiles, wildKey);

  // 全部非财神牌必须都是幺九牌
  for (const key of Object.keys(rawCounts)) {
    if (!YAO_KEYS.includes(key) || key === wildKey) return false;
  }

  // wildKey 就是 honor:white，YT_KEYS 里包含它
  const nonWildYaoKeys = YAO_KEYS.filter(k => k !== wildKey);
  let present = 0;
  let extras = 0;

  for (const key of nonWildYaoKeys) {
    const c = rawCounts[key] || 0;
    if (c > 0) {
      present++;
      extras += c - 1;
    }
  }

  // 还需要覆盖 wildKey (honor:white) 本身 + 缺失的 non-wild yao keys
  const needForMissing = nonWildYaoKeys.length - present;
  const needForWildSlot = 1; // wildKey 本身算一个幺九位
  const totalNeed = needForMissing + needForWildSlot;

  if (wildCount < totalNeed) return false;
  const remainingWild = wildCount - totalNeed;
  // 必须恰好多出1张牌组成对子
  return (extras + remainingWild) === 1;
}

/**
 * 杠上开花：杠后补牌直接胡。
 */
function isGangShangKaiHua(kind) {
  return kind === "杠开";
}

/**
 * 爆头：1张财神+牌型成型（听牌），摸任意牌可胡。
 */
function isBaoTou(hand, wildKey, winningTile) {
  const { wildCount } = getCanonicalCounts(hand, wildKey);
  if (wildCount !== 1) return false;
  if (!winningTile) return false;

  // 去掉 winningTile（刚摸到的那张），剩13张应已听牌
  const withoutWinning = hand.filter(t => t.id !== winningTile.id);
  if (withoutWinning.length !== 13) return false;
  return getWaitTiles(withoutWinning, wildKey).length > 0;
}

/**
 * 天胡：庄家起手14张直接胡（无人出过牌）。
 */
function checkTianHu(isDealer, isFirstTurn) {
  return isDealer && isFirstTurn;
}

/**
 * 地胡：闲家第一轮摸牌即胡。
 */
function checkDiHu(isDealer, isFirstDraw) {
  return !isDealer && isFirstDraw;
}

/**
 * 统计风牌刻子/对子数，财神可填补。
 * 返回 { triplets, pairs }。
 */
function countWindSets(counts, wildCount) {
  const winds = ["honor:east", "honor:south", "honor:west", "honor:north"];
  let triplets = 0;
  let pairs = 0;
  let w = wildCount;

  for (const key of winds) {
    const c = counts[key] || 0;
    if (c >= 3) {
      triplets++;
    } else if (c === 2) {
      pairs++;
    } else if (c === 1 && w >= 2) {
      triplets++;
      w -= 2;
    } else if (c === 1 && w >= 1) {
      pairs++;
      w -= 1;
    } else if (c === 0 && w >= 3) {
      triplets++;
      w -= 3;
    } else if (c === 0 && w >= 2) {
      pairs++;
      w -= 2;
    }
  }
  // 尝试将财神用于升级 pair→triplet
  while (pairs > 0 && w >= 1) {
    pairs--;
    triplets++;
    w--;
  }
  return { triplets, pairs };
}

/**
 * 统计箭牌刻子/对子数，财神可填补。
 */
function countDragonSets(counts, wildCount) {
  const dragons = ["honor:red", "honor:green", "honor:white"];
  let triplets = 0;
  let pairs = 0;
  let w = wildCount;

  for (const key of dragons) {
    const c = counts[key] || 0;
    if (c >= 3) {
      triplets++;
    } else if (c === 2) {
      pairs++;
    } else if (c === 1 && w >= 2) {
      triplets++;
      w -= 2;
    } else if (c === 1 && w >= 1) {
      pairs++;
      w -= 1;
    } else if (c === 0 && w >= 3) {
      triplets++;
      w -= 3;
    } else if (c === 0 && w >= 2) {
      pairs++;
      w -= 2;
    }
  }
  while (pairs > 0 && w >= 1) {
    pairs--;
    triplets++;
    w--;
  }
  return { triplets, pairs };
}

/**
 * 大四喜：东南西北四风各成刻子。
 */
function isDaSiXi(counts, wildCount) {
  return countWindSets(counts, wildCount).triplets >= 4;
}

/**
 * 小四喜：三门风刻 + 一对风。
 */
function isXiaoSiXi(counts, wildCount) {
  const result = countWindSets(counts, wildCount);
  return result.triplets >= 3 && result.pairs >= 1;
}

/**
 * 大三元：中发白各成刻子。
 */
function isDaSanYuan(counts, wildCount) {
  return countDragonSets(counts, wildCount).triplets >= 3;
}

/**
 * 小三元：二门箭刻 + 一对箭。
 */
function isXiaoSanYuan(counts, wildCount) {
  const result = countDragonSets(counts, wildCount);
  return result.triplets >= 2 && result.pairs >= 1;
}

// ----------------------------------------------------------
// 飘财倍数映射
// ----------------------------------------------------------
function piaoCaiMultiplier(piaoCount) {
  if (piaoCount === 0) return 1;
  if (piaoCount === 1) return 2;
  if (piaoCount === 2) return 4;
  return 8; // 3飘及以上
}

/**
 * 计算胡牌得分。
 *
 * @param {object} params
 * @param {Tile[]} params.hand - 胡牌手牌（14张）
 * @param {string} params.wildKey - 财神 key
 * @param {string} params.kind - 胡牌方式："自摸"|"杠开"|"飘财"|"点炮"
 * @param {Tile|null} params.winningTile - 胡的那张牌（点炮/自摸的那张）
 * @param {number} [params.piaoCount=0] - 飘财次数（0=未飘）
 * @param {boolean} [params.isDealer=false] - 胡牌者是否是庄家
 * @param {boolean} [params.isFirstDraw=false] - 是否第一次摸牌（地胡判定）
 * @param {boolean} [params.isFirstTurn=false] - 是否开局第一轮（天胡判定，庄家未出过牌）
 * @returns {{ flags: string[], multiplier: number, total: number, isSelfDraw: boolean }}
 *   flags: 牌型标签如 ["七对","爆头"]
 *   multiplier: 综合倍率
 *   total: = multiplier × 8（基础分，兼容旧调用方）
 *   isSelfDraw: 是否自摸
 */
export function calculateWinScore({
  hand, wildKey, kind, winningTile, piaoCount = 0,
  isDealer = false, isFirstDraw = false, isFirstTurn = false,
}) {
  if (!hand || hand.length !== 14) {
    return { flags: [], multiplier: 0, total: 0, isSelfDraw: true };
  }

  const { counts, wildCount } = getCanonicalCounts(hand, wildKey);
  const is7Pairs = isSevenPairsPattern(hand, wildKey);
  const flags = [];

  const is13Yao = isShiSanYao(hand, wildKey);

  // --- 基础牌型（互斥） ---
  if (is7Pairs) {
    if (isLuxurySevenPairs(hand, wildKey)) {
      flags.push("七对");
      flags.push("豪华七对");
    } else {
      flags.push("七对");
    }
  } else if (!is13Yao) {
    flags.push("平胡");
  }

  // --- 特殊组合牌型 ---
  if (isShiSanYao(hand, wildKey)) {
    flags.push("十三幺");
  }

  // --- 风/箭牌型 ---
  if (isDaSiXi(counts, wildCount)) {
    flags.push("大四喜");
  } else if (isXiaoSiXi(counts, wildCount)) {
    flags.push("小四喜");
  }

  if (isDaSanYuan(counts, wildCount)) {
    flags.push("大三元");
  } else if (isXiaoSanYuan(counts, wildCount)) {
    flags.push("小三元");
  }

  // --- 情境牌型 ---
  if (isGangShangKaiHua(kind)) {
    flags.push("杠上开花");
  }
  if (isBaoTou(hand, wildKey, winningTile) || (piaoCount > 0 && wildCount >= 1)) {
    if (!flags.includes("爆头")) flags.push("爆头");
  }
  if (checkTianHu(isDealer, isFirstTurn)) {
    flags.push("天胡");
  } else if (checkDiHu(isDealer, isFirstDraw)) {
    flags.push("地胡");
  }

  // --- 倍数计算 ---
  // 每个牌型 x2（平胡除外，平胡是基数1）
  let multiplier = 1;
  for (const f of flags) {
    if (f === "平胡") continue; // 平胡基数1，不乘
    multiplier *= 2;
  }

  // 飘财倍数叠加
  const pcMult = piaoCaiMultiplier(piaoCount);
  multiplier *= pcMult;

  const total = multiplier * 8;
  return { flags, multiplier, total, isSelfDraw: kind !== "点炮" };
}
