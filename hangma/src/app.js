const SUIT_CONFIG = {
  bamboo: { symbol: "条", className: "bamboo" },
  dot: { symbol: "筒", className: "dot" },
  character: { symbol: "万", className: "character" },
};

const HONOR_LABELS = {
  east: "东",
  south: "南",
  west: "西",
  north: "北",
  red: "中",
  green: "发",
  white: "白",
};

const CHINESE_NUMERALS = {
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

const SEATS = [
  { id: 0, name: "你", wind: "东", isHuman: true, seatKey: "bottom" },
  { id: 1, name: "下家", wind: "南", isHuman: false, seatKey: "right" },
  { id: 2, name: "对家", wind: "西", isHuman: false, seatKey: "top" },
  { id: 3, name: "上家", wind: "北", isHuman: false, seatKey: "left" },
];

const INITIAL_SCORE = 250;
const TURN_DELAY = 550;
const CLAIM_DELAY = 400;

const AudioManager = (() => {
  let ctx = null;
  const SOUNDS_ENABLED_KEY = "hangma_sound_enabled";
  let enabled = localStorage.getItem(SOUNDS_ENABLED_KEY) !== "false";

  function ensureCtx() {
    if (!ctx) {
      ctx = new (window.AudioContext || window.webkitAudioContext)();
    }
    if (ctx.state === "suspended") ctx.resume();
    return ctx;
  }

  function playTone(freq, duration, type = "sine", vol = 0.12) {
    if (!enabled) return;
    try {
      const c = ensureCtx();
      const osc = c.createOscillator();
      const gain = c.createGain();
      osc.type = type;
      osc.frequency.value = freq;
      gain.gain.setValueAtTime(vol, c.currentTime);
      gain.gain.exponentialRampToValueAtTime(0.001, c.currentTime + duration);
      osc.connect(gain);
      gain.connect(c.destination);
      osc.start(c.currentTime);
      osc.stop(c.currentTime + duration);
    } catch (e) { /* audio not available */ }
  }

  function playNoise(duration, vol = 0.06) {
    if (!enabled) return;
    try {
      const c = ensureCtx();
      const bufferSize = c.sampleRate * duration;
      const buffer = c.createBuffer(1, bufferSize, c.sampleRate);
      const data = buffer.getChannelData(0);
      for (let i = 0; i < bufferSize; i++) data[i] = Math.random() * 2 - 1;
      const source = c.createBufferSource();
      source.buffer = buffer;
      const gain = c.createGain();
      const filter = c.createBiquadFilter();
      filter.type = "highpass";
      filter.frequency.value = 2000;
      gain.gain.setValueAtTime(vol, c.currentTime);
      gain.gain.exponentialRampToValueAtTime(0.001, c.currentTime + duration);
      source.connect(filter);
      filter.connect(gain);
      gain.connect(c.destination);
      source.start(c.currentTime);
      source.stop(c.currentTime + duration);
    } catch (e) { /* */ }
  }

  return {
    get enabled() { return enabled; },
    toggle() {
      enabled = !enabled;
      localStorage.setItem(SOUNDS_ENABLED_KEY, enabled);
      return enabled;
    },
    discard() { playNoise(0.08, 0.07); playTone(800, 0.1, "triangle", 0.08); },
    draw() { playTone(600, 0.12, "sine", 0.06); playTone(900, 0.08, "sine", 0.04); },
    click() { playTone(1200, 0.06, "square", 0.05); },
    peng() { playTone(500, 0.15, "triangle", 0.1); setTimeout(() => playTone(700, 0.12, "triangle", 0.08), 60); },
    gang() { playTone(400, 0.18, "triangle", 0.11); setTimeout(() => playTone(600, 0.14, "triangle", 0.09), 80); setTimeout(() => playTone(800, 0.1, "triangle", 0.07), 160); },
    chi() { playTone(550, 0.12, "sine", 0.07); setTimeout(() => playTone(700, 0.1, "sine", 0.06), 50); },
    win() {
      [523, 659, 784, 1047].forEach((freq, i) => {
        setTimeout(() => playTone(freq, 0.3, "sine", 0.13), i * 120);
      });
    },
    pass() { playTone(300, 0.1, "sine", 0.04); },
  };
})();

const ParticleSpawner = (() => {
  function burst(x, y) {
    const colors = ["#f4c767", "#c94337", "#14804e", "#236fad", "#fff6df", "#ff5d78", "#ffe9ad"];
    const count = 60;
    for (let i = 0; i < count; i++) {
      const particle = document.createElement("div");
      const angle = (Math.PI * 2 * i) / count + (Math.random() - 0.5) * 0.5;
      const velocity = 180 + Math.random() * 280;
      const size = 6 + Math.random() * 10;
      particle.className = "confetti-particle";
      particle.style.cssText = `
        --vx: ${Math.cos(angle) * velocity}px;
        --vy: ${Math.sin(angle) * velocity - 100}px;
        --size: ${size}px;
        --spin: ${(Math.random() - 0.5) * 720}deg;
        --delay: ${Math.random() * 0.15}s;
        --color: ${colors[Math.floor(Math.random() * colors.length)]};
        left: ${x}px;
        top: ${y}px;
      `;
      document.body.appendChild(particle);
      particle.addEventListener("animationend", () => particle.remove());
    }
  }
  function centerBurst() {
    burst(window.innerWidth / 2, window.innerHeight / 2);
  }
  return { burst, centerBurst };
})();

const dom = {
  playerHand: document.querySelector("#playerHand"),
  playerMelds: document.querySelector("#playerMelds"),
  seatTop: document.querySelector("#seatTop"),
  seatLeft: document.querySelector("#seatLeft"),
  seatRight: document.querySelector("#seatRight"),
  riverBoard: document.querySelector("#riverBoard"),
  actionBar: document.querySelector("#actionBar"),
  wallCount: document.querySelector("#wallCount"),
  turnText: document.querySelector("#turnText"),
  statusLine: document.querySelector("#statusLine"),
  wildTileBadge: document.querySelector("#wildTileBadge"),
  handCount: document.querySelector("#handCount"),
  hintList: document.querySelector("#hintList"),
  logList: document.querySelector("#logList"),
  scoreStrip: document.querySelector("#scoreStrip"),
  lastDiscard: document.querySelector("#lastDiscard"),
  newGameBtn: document.querySelector("#newGameBtn"),
  soundBtn: document.querySelector("#soundBtn"),
};

let game = null;
let sharedMemo = new Map();

function resetSharedMemo() { sharedMemo = new Map(); }

function setupKeyboardShortcuts() {
  document.addEventListener("keydown", (event) => {
    if (!game || game.locked) return;
    if (event.target.closest("input, textarea, [contenteditable]")) return;

    if (game.phase === "human-discard" && !game.winner && !game.drawReason) {
      if (event.key === "d" || event.key === "D") {
        const selected = game.selectedTileId;
        if (selected) { discardPlayerTile(selected); return; }
        const player = getPlayer(0);
        if (player.hand.length > 0) {
          discardPlayerTile(player.hand[player.hand.length - 1].id);
        }
        return;
      }
    }

    if (game.phase === "claim" && game.claimOptions.length > 0) {
      if (event.key === " " || event.key === "Escape") {
        event.preventDefault();
        passClaim();
        return;
      }
      const num = parseInt(event.key);
      if (num >= 1 && num <= game.claimOptions.length) {
        event.preventDefault();
        game.claimOptions[num - 1].handler();
        return;
      }
    }
  });
}

function createTile(suit, rank) {
  return { id: `${suit}-${rank}-${Math.random().toString(36).slice(2, 9)}`, suit, rank };
}

function createWall() {
  const wall = [];
  ["bamboo", "dot", "character"].forEach((suit) => {
    for (let rank = 1; rank <= 9; rank += 1) {
      for (let copy = 0; copy < 4; copy += 1) {
        wall.push(createTile(suit, rank));
      }
    }
  });

  Object.keys(HONOR_LABELS).forEach((honor) => {
    for (let copy = 0; copy < 4; copy += 1) {
      wall.push(createTile("honor", honor));
    }
  });

  return shuffle(wall);
}

function shuffle(list) {
  const copy = [...list];
  for (let i = copy.length - 1; i > 0; i -= 1) {
    const j = Math.floor(Math.random() * (i + 1));
    [copy[i], copy[j]] = [copy[j], copy[i]];
  }
  return copy;
}

function tileSortValue(tile) {
  const suitOrder = { character: 0, dot: 1, bamboo: 2, honor: 3 };
  if (tile.suit === "honor") {
    const honorOrder = ["east", "south", "west", "north", "red", "green", "white"];
    return suitOrder.honor * 100 + honorOrder.indexOf(tile.rank);
  }
  return suitOrder[tile.suit] * 100 + Number(tile.rank);
}

function sortTiles(tiles) {
  tiles.sort((a, b) => tileSortValue(a) - tileSortValue(b));
}

function tileKey(tile) {
  return `${tile.suit}:${tile.rank}`;
}

function tileLabel(tile) {
  if (tile.suit === "honor") {
    return HONOR_LABELS[tile.rank];
  }
  return `${CHINESE_NUMERALS[tile.rank]}${SUIT_CONFIG[tile.suit].symbol}`;
}

function tileGlyph(tile) {
  if (tile.suit === "honor") {
    const glyphs = {
      east: "🀀",
      south: "🀁",
      west: "🀂",
      north: "🀃",
      red: "🀄",
      green: "🀅",
      white: "🀆",
    };
    return glyphs[tile.rank];
  }
  const glyphs = {
    character: ["", "🀇", "🀈", "🀉", "🀊", "🀋", "🀌", "🀍", "🀎", "🀏"],
    dot: ["", "🀙", "🀚", "🀛", "🀜", "🀝", "🀞", "🀟", "🀠", "🀡"],
    bamboo: ["", "🀐", "🀑", "🀒", "🀓", "🀔", "🀕", "🀖", "🀗", "🀘"],
  };
  return glyphs[tile.suit][tile.rank];
}

function tileAssetName(tile) {
  return tile.suit === "honor" ? `honor-${tile.rank}` : `${tile.suit}-${tile.rank}`;
}

function tileAssetPath(tile) {
  return `./assets/tiles/${tileAssetName(tile)}.svg`;
}

function getTileFaceMarkup(tile) {
  return `
    <div class="tile-face">
      <img class="tile-art" src="${tileAssetPath(tile)}" alt="" aria-hidden="true">
    </div>
  `;
}

function tilesToText(tiles) {
  return tiles.map(tileLabel).join(" ");
}

function cloneTile(tile) {
  return { ...tile };
}

function countWildcards(tiles, wildKey) {
  let count = 0;
  for (const tile of tiles) {
    if (tileKey(tile) === wildKey) {
      count += 1;
    }
  }
  return count;
}

function isBaotou(tiles, wildKey, winningTile) {
  // Baotou (爆头): the wildcard serves as the pair (将牌), paired with the winning tile.
  // After removing the winning tile and one wildcard, the remaining 12 tiles must form 4 melds.
  if (!winningTile || tileKey(winningTile) === wildKey) {
    return false;
  }

  const { counts, wildCount } = getCanonicalCounts(tiles, wildKey);
  const winKey = tileKey(winningTile);
  const winCount = counts.get(winKey) || 0;
  if (winCount < 1 || wildCount < 1) {
    return false;
  }

  counts.set(winKey, winCount - 1);
  if (counts.get(winKey) === 0) {
    counts.delete(winKey);
  }

  return canFormSets(counts, wildCount - 1, new Map());
}

function getCanonicalCounts(tiles, wildKey) {
  const counts = new Map();
  let wildCount = 0;
  for (const tile of tiles) {
    const key = tileKey(tile);
    if (key === wildKey) {
      wildCount += 1;
    } else {
      counts.set(key, (counts.get(key) || 0) + 1);
    }
  }
  return { counts, wildCount };
}

function keyToTile(key) {
  const [suit, rank] = key.split(":");
  return { suit, rank: suit === "honor" ? rank : Number(rank) };
}

function isSuitKey(key) {
  return !key.startsWith("honor:");
}

function nextSuitKey(key) {
  const tile = keyToTile(key);
  if (tile.suit === "honor" || tile.rank >= 9) {
    return null;
  }
  return `${tile.suit}:${tile.rank + 1}`;
}

function nextNextSuitKey(key) {
  const tile = keyToTile(key);
  if (tile.suit === "honor" || tile.rank >= 8) {
    return null;
  }
  return `${tile.suit}:${tile.rank + 2}`;
}

function serializeCounts(counts) {
  const items = [...counts.entries()].sort(([a], [b]) => (a > b ? 1 : -1));
  return items.map(([key, value]) => `${key}:${value}`).join("|");
}

function canFormSets(counts, wildCount, memo = new Map()) {
  const signature = `${wildCount}#${serializeCounts(counts)}`;
  if (memo.has(signature)) {
    return memo.get(signature);
  }

  const keys = [...counts.keys()].filter((key) => counts.get(key) > 0).sort();
  if (keys.length === 0) {
    const result = wildCount % 3 === 0;
    memo.set(signature, result);
    return result;
  }

  const firstKey = keys[0];
  const firstCount = counts.get(firstKey);

  if (firstCount >= 3) {
    counts.set(firstKey, firstCount - 3);
    if (counts.get(firstKey) === 0) {
      counts.delete(firstKey);
    }
    if (canFormSets(counts, wildCount, memo)) {
      memo.set(signature, true);
      counts.set(firstKey, firstCount);
      return true;
    }
    counts.set(firstKey, firstCount);
  }

  if (firstCount < 3 && wildCount >= 3 - firstCount) {
    counts.delete(firstKey);
    if (canFormSets(counts, wildCount - (3 - firstCount), memo)) {
      memo.set(signature, true);
      counts.set(firstKey, firstCount);
      return true;
    }
    counts.set(firstKey, firstCount);
  }

  if (isSuitKey(firstKey)) {
    const secondKey = nextSuitKey(firstKey);
    const thirdKey = nextNextSuitKey(firstKey);
    if (secondKey && thirdKey) {
      const missing = [];
      const usedKeys = [firstKey];

      const secondCount = counts.get(secondKey) || 0;
      const thirdCount = counts.get(thirdKey) || 0;

      if (secondCount === 0) {
        missing.push(secondKey);
      } else {
        usedKeys.push(secondKey);
      }

      if (thirdCount === 0) {
        missing.push(thirdKey);
      } else {
        usedKeys.push(thirdKey);
      }

      if (wildCount >= missing.length) {
        const snapshot = new Map();
        usedKeys.forEach((key) => snapshot.set(key, counts.get(key)));

        counts.set(firstKey, firstCount - 1);
        if (counts.get(firstKey) === 0) {
          counts.delete(firstKey);
        }

        if (secondCount > 0) {
          counts.set(secondKey, secondCount - 1);
          if (counts.get(secondKey) === 0) {
            counts.delete(secondKey);
          }
        }

        if (thirdCount > 0) {
          counts.set(thirdKey, thirdCount - 1);
          if (counts.get(thirdKey) === 0) {
            counts.delete(thirdKey);
          }
        }

        if (canFormSets(counts, wildCount - missing.length, memo)) {
          memo.set(signature, true);
          snapshot.forEach((value, key) => counts.set(key, value));
          return true;
        }

        snapshot.forEach((value, key) => counts.set(key, value));
      }
    }
  }

  memo.set(signature, false);
  return false;
}

function isSevenPairs(tiles, wildKey) {
  if (tiles.length !== 14) {
    return false;
  }

  const { counts, wildCount } = getCanonicalCounts(tiles, wildKey);
  let oddCount = 0;
  let pairSlots = 0;

  counts.forEach((value) => {
    oddCount += value % 2;
    pairSlots += Math.floor(value / 2);
  });

  if (wildCount < oddCount) {
    return false;
  }

  const remainingWild = wildCount - oddCount;
  return pairSlots + oddCount + Math.floor(remainingWild / 2) >= 7;
}

function isStandardWin(tiles, wildKey) {
  if (tiles.length % 3 !== 2) {
    return false;
  }

  const { counts, wildCount } = getCanonicalCounts(tiles, wildKey);
  const memo = new Map();
  const uniqueKeys = [...counts.keys()];

  for (const key of uniqueKeys) {
    const count = counts.get(key);
    if (count >= 2) {
      counts.set(key, count - 2);
      if (counts.get(key) === 0) {
        counts.delete(key);
      }
      if (canFormSets(counts, wildCount, memo)) {
        counts.set(key, count);
        return true;
      }
      counts.set(key, count);
    }

    if (count >= 1 && wildCount >= 1) {
      counts.set(key, count - 1);
      if (counts.get(key) === 0) {
        counts.delete(key);
      }
      if (canFormSets(counts, wildCount - 1, memo)) {
        counts.set(key, count);
        return true;
      }
      counts.set(key, count);
    }
  }

  if (wildCount >= 2 && canFormSets(counts, wildCount - 2, memo)) {
    return true;
  }

  return false;
}

function isWinningHand(tiles, wildKey) {
  return isSevenPairs(tiles, wildKey) || isStandardWin(tiles, wildKey);
}

function generateAllDistinctTiles() {
  const tiles = [];
  ["character", "dot", "bamboo"].forEach((suit) => {
    for (let rank = 1; rank <= 9; rank += 1) {
      tiles.push({ suit, rank });
    }
  });
  Object.keys(HONOR_LABELS).forEach((rank) => {
    tiles.push({ suit: "honor", rank });
  });
  return tiles;
}

const DISTINCT_TILES = generateAllDistinctTiles();

function getWaitTiles(tiles, wildKey) {
  if (tiles.length % 3 !== 1) {
    return [];
  }
  const waits = [];
  for (const tile of DISTINCT_TILES) {
    const trial = [...tiles.map(cloneTile), cloneTile(tile)];
    if (isWinningHand(trial, wildKey)) {
      waits.push(tile);
    }
  }
  return waits;
}

function handSignature(tiles) {
  return tiles.map(tileKey).sort().join(",");
}

function uniqueTilesByKey(tiles) {
  const seen = new Set();
  const unique = [];
  tiles.forEach((tile) => {
    const key = tileKey(tile);
    if (!seen.has(key)) {
      seen.add(key);
      unique.push(tile);
    }
  });
  return unique;
}

function removeOneTileByKey(tiles, key) {
  let removed = false;
  return tiles
    .filter((tile) => {
      if (!removed && tileKey(tile) === key) {
        removed = true;
        return false;
      }
      return true;
    })
    .map(cloneTile);
}

function removeTilesByKey(tiles, key, count) {
  let removed = 0;
  return tiles
    .filter((tile) => {
      if (removed < count && tileKey(tile) === key) {
        removed += 1;
        return false;
      }
      return true;
    })
    .map(cloneTile);
}

function simulatedTile(tile) {
  return { ...tile, id: `sim-${tileKey(tile)}-${Math.random().toString(36).slice(2, 7)}` };
}

function countKnownTiles(key, playerId, ownHandOverride = null) {
  let count = 0;
  game.players.forEach((player) => {
    const ownHand = player.id === playerId ? ownHandOverride || player.hand : null;
    if (ownHand) {
      ownHand.forEach((tile) => {
        if (tileKey(tile) === key) {
          count += 1;
        }
      });
    }

    player.discards.forEach((tile) => {
      if (tileKey(tile) === key) {
        count += 1;
      }
    });

    player.melds.forEach((meld) => {
      meld.tiles.forEach((tile) => {
        if (tileKey(tile) === key) {
          count += 1;
        }
      });
    });
  });
  return count;
}

function getPlayerMeldCount(playerId) {
  const player = getPlayer(playerId);
  return player ? player.melds.length : 0;
}

function remainingTileCount(tile, playerId, ownHandOverride = null) {
  return Math.max(0, 4 - countKnownTiles(tileKey(tile), playerId, ownHandOverride));
}

function availableWaitCount(waits, playerId, ownHandOverride = null) {
  let count = 0;
  const seen = new Set();
  waits.forEach((tile) => {
    const key = tileKey(tile);
    if (!seen.has(key)) {
      seen.add(key);
      count += remainingTileCount(tile, playerId, ownHandOverride);
    }
  });
  return count;
}

function publicDiscardCount(key) {
  return game.players.reduce((sum, player) => sum + player.discards.filter((tile) => tileKey(tile) === key).length, 0);
}

function estimateDiscardDanger(tile, playerId) {
  const key = tileKey(tile);
  const visiblePublic = publicDiscardCount(key);
  let danger = 6 - visiblePublic * 2.4;

  if (tile.suit === "honor") {
    if (visiblePublic === 0) danger += 2.0;
    else if (visiblePublic >= 2) danger -= 1.8;
  } else {
    const rank = Number(tile.rank);
    danger += 5 - Math.abs(5 - rank) * 0.8;
    if (rank === 1 || rank === 9) danger -= 2.8;
    if (rank >= 3 && rank <= 7) danger += 1.0;
  }

  if (tileKey(tile) === game.wildKey) {
    danger += 5;
  }

  let allDiscarded = 0;
  game.players.forEach((p) => {
    if (p.id !== playerId) {
      p.discards.forEach((d) => { if (tileKey(d) === key) allDiscarded += 1; });
    }
  });
  danger -= allDiscarded * 1.1;

  let opponentMeldBonus = 0;
  game.players.forEach((p) => {
    if (p.id !== playerId && p.melds.length >= 2) {
      opponentMeldBonus += (p.melds.length - 1) * 1.2;
    }
  });
  danger += opponentMeldBonus;

  const wallRatio = game.wall.length / 83;
  const lateGame = 1 - wallRatio;
  danger += Math.max(0, lateGame - 0.3) * 5.0;

  return Math.max(0, danger);
}

function evaluateShape(tiles, wildKey, meldCount = 0) {
  const { counts, wildCount } = getCanonicalCounts(tiles, wildKey);
  let score = wildCount * 52;
  let pairLike = Math.floor(wildCount / 2);
  let singletonHonors = 0;

  counts.forEach((count, key) => {
    const tile = keyToTile(key);
    if (count >= 4) {
      score += 44;
      pairLike += 2;
    } else if (count === 3) {
      score += 34;
      pairLike += 1;
    } else if (count === 2) {
      score += 18;
      pairLike += 1;
    } else if (tile.suit === "honor") {
      singletonHonors += 1;
      score -= 9;
    }
  });

  ["character", "dot", "bamboo"].forEach((suit) => {
    const ranks = Array.from({ length: 10 }, () => 0);
    tiles.forEach((tile) => {
      if (tile.suit === suit && tileKey(tile) !== wildKey) {
        ranks[Number(tile.rank)] += 1;
      }
    });

    for (let rank = 1; rank <= 9; rank += 1) {
      if (ranks[rank] > 0) {
        score += Math.max(0, 5 - Math.abs(5 - rank)) * ranks[rank] * 1.0;
      }
    }

    const usedInSeq = Array.from({ length: 10 }, () => false);

    for (let rank = 1; rank <= 7; rank += 1) {
      if (ranks[rank] && ranks[rank + 1] && ranks[rank + 2]) {
        score += 28;
        usedInSeq[rank] = usedInSeq[rank + 1] = usedInSeq[rank + 2] = true;
      }
    }

    for (let rank = 1; rank <= 8; rank += 1) {
      if (ranks[rank] && ranks[rank + 1] && !usedInSeq[rank] && !usedInSeq[rank + 1]) {
        const isTwoSided = rank >= 2 && rank <= 7 && rank + 1 <= 8;
        score += isTwoSided ? 20 : 10;
      }
    }

    for (let rank = 1; rank <= 7; rank += 1) {
      if (ranks[rank] && ranks[rank + 2] && !usedInSeq[rank] && !usedInSeq[rank + 2]) {
        score += 8;
      }
    }

    for (let rank = 1; rank <= 9; rank += 1) {
      if (ranks[rank] === 1) {
        const hasLeft = rank > 1 && ranks[rank - 1] > 0;
        const hasRight = rank < 9 && ranks[rank + 1] > 0;
        if (!hasLeft && !hasRight && !usedInSeq[rank]) {
          score -= 3;
        }
      }
    }
  });

  if (tiles.length === 13 || tiles.length === 14) {
    const cappedPairs = Math.min(pairLike, 6);
    score += cappedPairs * 8;
    if (pairLike >= 5) {
      score += 24;
    }
    if (pairLike >= 6) {
      score += 16;
    }
  }

  score += meldCount * 180;
  return { score, pairLike, singletonHonors, meldCount };
}

function evaluateWaitQuality(waits, playerId) {
  if (waits.length === 0) return 0;
  const distinctKeys = new Set(waits.map(tileKey));
  let totalRemain = 0;
  distinctKeys.forEach((key) => {
    totalRemain += remainingTileCount(keyToTile(key), playerId);
  });
  const diversityBonus = Math.min(distinctKeys.size - 1, 2) * 0.12;
  return totalRemain * (0.76 + diversityBonus);
}

function evaluateWaitingHand(tiles, playerId, depth = 1, memo = new Map(), meldCountOverride = null) {
  const meldCount = meldCountOverride ?? getPlayerMeldCount(playerId);
  const signature = `wait:${playerId}:${meldCount}:${depth}:${handSignature(tiles)}`;
  if (memo.has(signature)) {
    return memo.get(signature);
  }

  const shape = evaluateShape(tiles, game.wildKey, meldCount);
  const waits = getWaitTiles(tiles, game.wildKey);
  const waitRemain = availableWaitCount(waits, playerId, tiles);
  let score = shape.score;
  let improvement = { tileCount: 0, weightedWaits: 0, bestScore: 0 };

  if (waits.length > 0) {
    const waitQual = evaluateWaitQuality(waits, playerId);
    score += 10000 + waitQual * 380 + waits.length * 55;
  } else if (depth > 0) {
    improvement = evaluateImprovingDraws(tiles, playerId, depth, memo, meldCount);
    score += improvement.tileCount * 38 + improvement.weightedWaits * 28 + improvement.bestScore * 0.08 + improvement.shapeGain;
  }

  const result = { score, waits, waitRemain, improvement, shape };
  memo.set(signature, result);
  return result;
}

function evaluateImprovingDraws(waitingHand, playerId, depth, memo, meldCount) {
  let tileCount = 0;
  let weightedWaits = 0;
  let bestScore = 0;
  let shapeGain = 0;
  const baseShapeScore = evaluateShape(waitingHand, game.wildKey, meldCount).score;

  DISTINCT_TILES.forEach((tile) => {
    const remaining = remainingTileCount(tile, playerId, waitingHand);
    if (remaining <= 0) {
      return;
    }

    const trial = [...waitingHand.map(cloneTile), simulatedTile(tile)];
    const bestAfterDraw = evaluateBestDiscardForHand(trial, playerId, depth - 1, memo, meldCount);
    bestScore = Math.max(bestScore, bestAfterDraw.score);
    if (bestAfterDraw.waitRemain > 0) {
      tileCount += remaining;
      weightedWaits += bestAfterDraw.waitRemain * remaining;
    } else {
      const delta = bestAfterDraw.shape.score - baseShapeScore;
      if (delta > 0) {
        shapeGain += delta * remaining * 0.16;
      }
    }
  });

  return { tileCount, weightedWaits, bestScore, shapeGain };
}

function evaluateBestDiscardForHand(hand, playerId, depth = 1, memo = new Map(), meldCountOverride = null) {
  const meldCount = meldCountOverride ?? getPlayerMeldCount(playerId);
  const signature = `discard:${playerId}:${meldCount}:${depth}:${handSignature(hand)}`;
  if (memo.has(signature)) {
    return memo.get(signature);
  }

  let best = null;
  uniqueTilesByKey(hand).forEach((tile) => {
    const afterDiscard = removeOneTileByKey(hand, tileKey(tile));
    const state = evaluateWaitingHand(afterDiscard, playerId, depth, memo, meldCount);
    const wildPenalty = tileKey(tile) === game.wildKey ? 6800 : 0;
    const dangerPenalty = estimateDiscardDanger(tile, playerId) * 16;
    const score = state.score - wildPenalty - dangerPenalty;
    const candidate = {
      tile,
      score,
      waits: state.waits,
      waitRemain: state.waitRemain,
      improvement: state.improvement,
      shape: state.shape,
      danger: dangerPenalty,
    };

    if (!best || candidate.score > best.score || (candidate.score === best.score && tileSortValue(candidate.tile) > tileSortValue(best.tile))) {
      best = candidate;
    }
  });

  memo.set(signature, best);
  return best;
}

function chooseAiDiscard(hand, playerId) {
  resetSharedMemo();
  const best = evaluateBestDiscardForHand(hand, playerId, 1, sharedMemo);
  return best ? best.tile : hand[0];
}

function evaluateClaimBaseline(playerId) {
  resetSharedMemo();
  return evaluateWaitingHand(getPlayer(playerId).hand.map(cloneTile), playerId, 1, sharedMemo, getPlayerMeldCount(playerId));
}

function evaluateClaimDiscardState(hand, playerId, meldCountOverride) {
  return evaluateBestDiscardForHand(hand.map(cloneTile), playerId, 1, sharedMemo, meldCountOverride);
}

function chooseAiChiChoice(playerId, tile, choices) {
  const player = getPlayer(playerId);
  const baseline = evaluateClaimBaseline(playerId);
  const claimMeldCount = getPlayerMeldCount(playerId) + 1;
  let bestChoice = null;

  choices.forEach((choiceIds) => {
    const simulatedHand = player.hand
      .filter((handTile) => !choiceIds.includes(handTile.id))
      .map(cloneTile);
    const afterClaim = evaluateClaimDiscardState(simulatedHand, playerId, claimMeldCount);
    const value = afterClaim.score;
    if (!bestChoice || value > bestChoice.value) {
      bestChoice = { choiceIds, value, afterClaim };
    }
  });

  if (!bestChoice) {
    return null;
  }

  const openPenalty = baseline.waitRemain > 0 ? 40 : 18;
  const goodEnough = bestChoice.afterClaim.waitRemain > baseline.waitRemain || bestChoice.value >= baseline.score - openPenalty;
  return goodEnough ? bestChoice.choiceIds : null;
}

function shouldAiPeng(playerId, tile) {
  const player = getPlayer(playerId);
  const baseline = evaluateClaimBaseline(playerId);
  const claimMeldCount = getPlayerMeldCount(playerId) + 1;
  const simulatedHand = removeTilesByKey(player.hand, tileKey(tile), 2);
  const afterClaim = evaluateClaimDiscardState(simulatedHand, playerId, claimMeldCount);
  const openPenalty = baseline.waitRemain > 0 ? 25 : 12;
  return afterClaim.waitRemain > baseline.waitRemain || afterClaim.score >= baseline.score - openPenalty;
}

function shouldAiMeldGang(playerId, tile) {
  const player = getPlayer(playerId);
  const baseline = evaluateClaimBaseline(playerId);
  const claimMeldCount = getPlayerMeldCount(playerId) + 1;
  const simulatedHand = removeTilesByKey(player.hand, tileKey(tile), 3);
  const afterGang = evaluateWaitingHand(simulatedHand, playerId, 1, sharedMemo, claimMeldCount);
  const openPenalty = baseline.waitRemain > 0 ? 15 : 8;
  return afterGang.waitRemain > baseline.waitRemain || afterGang.score >= baseline.score - openPenalty;
}

function chooseAiConcealedGang(playerId, choices) {
  const player = getPlayer(playerId);
  const currentBest = evaluateBestDiscardForHand(player.hand.map(cloneTile), playerId, 1, sharedMemo, getPlayerMeldCount(playerId));
  const nextMeldCount = getPlayerMeldCount(playerId) + 1;

  for (const tiles of choices) {
    const key = tileKey(tiles[0]);
    const afterGang = player.hand.filter((tile) => tileKey(tile) !== key).map(cloneTile);
    const future = evaluateWaitingHand(afterGang, playerId, 1, sharedMemo, nextMeldCount);
    if (future.waitRemain > currentBest.waitRemain || future.score >= currentBest.score - 10) {
      return tiles;
    }
  }

  return null;
}

function createPlayers() {
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
  }));
}

function newRound() {
  const wall = createWall();
  const players = createPlayers();

  for (let round = 0; round < 13; round += 1) {
    players.forEach((player) => {
      player.hand.push(wall.pop());
    });
  }

  players[0].hand.push(wall.pop());
  players.forEach((player) => sortTiles(player.hand));

  const wildTile = { suit: "honor", rank: "white" };
  const wildKey = tileKey(wildTile);

  return {
    players,
    wall,
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
    message: "你先出牌，上推手牌即可打出。",
    logs: ["新局开始，白板作财神。"],
    locked: false,
  };
}

function addLog(text) {
  game.logs.unshift(text);
  game.logs = game.logs.slice(0, 18);
}

function setMessage(text) {
  game.message = text;
}

function getPlayer(playerId) {
  return game.players[playerId];
}

function isTileWild(tile) {
  return tileKey(tile) === game.wildKey;
}

function render() {
  requestAnimationFrame(() => {
    renderScores();
    renderSeats();
    renderRiver();
    renderPlayerHand();
    renderActionBar();
    renderSidebar();
    renderStatus();
    renderLastDiscard();
    renderWinnerModal();
  });
}

function renderScores() {
  dom.scoreStrip.innerHTML = "";
  game.players.forEach((player, index) => {
    const card = document.createElement("div");
    card.className = `score-card ${index === game.turn ? "active" : ""}`;
    card.innerHTML = `<span>${player.wind}位 ${player.name}</span><strong>${player.score}</strong>`;
    dom.scoreStrip.appendChild(card);
  });
}

function renderSeats() {
  const seatMap = {
    top: dom.seatTop,
    left: dom.seatLeft,
    right: dom.seatRight,
  };

  game.players
    .filter((player) => !player.isHuman)
    .forEach((player) => {
      const seat = seatMap[player.seatKey];
      seat.dataset.seat = player.seatKey;
      seat.classList.toggle("active", player.id === game.turn);
      seat.innerHTML = "";

      const wrapper = document.createElement("div");
      wrapper.className = "opponent";

      const header = document.createElement("div");
      header.className = "opponent-name";
      header.innerHTML = `
        <span class="seat-avatar">${player.wind}</span>
        <span class="seat-info"><strong>${player.name}</strong><em>${player.hand.length}张</em></span>
      `;
      wrapper.appendChild(header);

      const melds = document.createElement("div");
      melds.className = "meld-zone";
      player.melds.forEach((meld) => {
        melds.appendChild(renderMeld(meld));
      });
      wrapper.appendChild(melds);

      const backs = document.createElement("div");
      backs.className = "back-row";
      for (let i = 0; i < player.hand.length; i += 1) {
        const back = document.createElement("div");
        back.className = "tile-back";
        backs.appendChild(back);
      }
      wrapper.appendChild(backs);

      seat.appendChild(wrapper);
    });

  dom.playerMelds.innerHTML = "";
  getPlayer(0).melds.forEach((meld) => {
    dom.playerMelds.appendChild(renderMeld(meld));
  });
}

function renderMeld(meld) {
  const container = document.createElement("div");
  container.className = "meld";
  meld.tiles.forEach((tile) => {
    container.appendChild(renderTile(tile, { small: true, showWild: true }));
  });
  return container;
}

function renderRiver() {
  dom.riverBoard.innerHTML = "";
  game.players.forEach((player) => {
    const river = document.createElement("div");
    river.className = "river";
    river.dataset.seat = player.seatKey;
    river.classList.toggle("active", player.id === game.turn);
    river.innerHTML = `<div class="river-title"><span>${player.wind}位 ${player.name}</span><span>${player.discards.length}张</span></div>`;
    const tiles = document.createElement("div");
    tiles.className = "river-tiles";
    player.discards.forEach((tile, index) => {
      const discardedTile = renderTile(tile, { small: true, showWild: true });
      discardedTile.style.setProperty("--rx", `${discardGridX(index, player.seatKey)}px`);
      discardedTile.style.setProperty("--ry", `${discardGridY(index, player.seatKey)}px`);
      discardedTile.style.setProperty("--rr", `${discardGridRotation(index, player.seatKey)}deg`);
      discardedTile.style.setProperty("--rz", String(index + 1));
      tiles.appendChild(discardedTile);
    });
    river.appendChild(tiles);
    dom.riverBoard.appendChild(river);
  });
}

function discardGridCols(seatKey) {
  if (seatKey === "left" || seatKey === "right") return 3;
  return 6;
}

function discardTileGap() {
  return 6;
}

function pseudoRandom(seed) {
  let s = seed;
  s = (s * 1103515245 + 12345) & 0x7fffffff;
  return (s % 100) / 100;
}

function discardGridX(index, seatKey) {
  const cols = discardGridCols(seatKey);
  const col = index % cols;
  const tileW = 30;
  const gap = discardTileGap();
  const startX = seatKey === "bottom" || seatKey === "top" ? 8 : 4;
  const jitter = (pseudoRandom(index * 7 + (seatKey === "bottom" ? 0 : seatKey === "top" ? 1 : seatKey === "left" ? 2 : 3) + 13) - 0.5) * 16;
  return startX + col * (tileW + gap) + jitter;
}

function discardGridY(index, seatKey) {
  const cols = discardGridCols(seatKey);
  const row = Math.floor(index / cols);
  const tileH = 47;
  const gap = discardTileGap();
  const startY = seatKey === "bottom" || seatKey === "top" ? 4 : 4;
  const jitter = (pseudoRandom(index * 11 + (seatKey === "bottom" ? 3 : seatKey === "top" ? 2 : seatKey === "left" ? 1 : 0) + 7) - 0.5) * 14;
  return startY + row * (tileH + gap) + jitter;
}

function discardGridRotation(index, seatKey) {
  const rots = {
    bottom: [-2, 6, -5, 9, -8, 4, -3, 7, -6, 2, -1, 8],
    top: [3, -6, 7, -4, 0, -8, 5, -7, 2, -3, 8, -5],
    left: [2, 7, -5, 4, -3, 6, -7, 0, 3, -6, 7, -4],
    right: [4, -4, 2, -6, 7, -2, 0, -8, 5, -3, 3, -1],
  };
  const base = (rots[seatKey] || rots.bottom)[index % 12];
  const wobble = (pseudoRandom(index * 3 + 31) - 0.5) * 10;
  return base + wobble;
}

function renderPlayerHand() {
  const player = getPlayer(0);
  dom.playerHand.innerHTML = "";
  player.hand.forEach((tile) => {
    const selected = game.selectedTileId === tile.id;
    const clickable = game.phase === "human-discard" && !game.locked;
    const el = renderTile(tile, { clickable, selected, showWild: true });
    if (clickable) {
      bindDiscardGesture(el, tile.id);
    }
    dom.playerHand.appendChild(el);
  });
  dom.handCount.textContent = `${player.hand.length} 张`;
}

function bindDiscardGesture(element, tileId) {
  let startY = 0;
  let currentY = 0;
  let pointerId = null;
  let dragging = false;
  let ghost = null;
  let everAboveHand = false;
  let detachDocumentListeners = null;

  function handTop() {
    return dom.playerHand.getBoundingClientRect().top;
  }

  function isAboveHand(y) {
    return y < handTop() - 12;
  }

  function createGhost(srcEl) {
    const el = document.createElement("div");
    el.className = "tile-ghost";
    el.innerHTML = srcEl.querySelector(".tile-face")?.outerHTML || "";
    document.body.appendChild(el);
    return el;
  }

  element.addEventListener("pointerdown", (event) => {
    if (game.phase !== "human-discard" || game.locked) {
      return;
    }
    event.preventDefault();
    pointerId = event.pointerId;
    startY = event.clientY;
    currentY = startY;
    everAboveHand = false;
    dragging = true;
    game.selectedTileId = tileId;
    ghost = createGhost(element);
    ghost.style.left = `${event.clientX}px`;
    ghost.style.top = `${event.clientY}px`;
    element.classList.add("ghosting");
    element.setPointerCapture?.(pointerId);
    attachDocumentGestureListeners();
    setMessage("松手打出，拖回牌面可放回。");
    renderStatus();
  });

  function moveGesture(event) {
    if (!dragging || event.pointerId !== pointerId) {
      return;
    }
    currentY = event.clientY;
    if (isAboveHand(currentY)) {
      everAboveHand = true;
    }
    if (ghost) {
      ghost.style.left = `${event.clientX}px`;
      ghost.style.top = `${event.clientY}px`;
      ghost.classList.toggle("ghost-ready", isAboveHand(currentY));
      ghost.classList.toggle("ghost-return", !isAboveHand(currentY));
    }
  }

  function finishGesture(event) {
    if (!dragging || event.pointerId !== pointerId) {
      return;
    }
    dragging = false;
    detachDocumentListeners?.();
    detachDocumentListeners = null;
    element.releasePointerCapture?.(pointerId);
    element.classList.remove("ghosting", "selected");
    if (ghost) {
      ghost.remove();
      ghost = null;
    }
    pointerId = null;

    if (isAboveHand(currentY)) {
      discardPlayerTile(tileId);
      return;
    }

    if (!everAboveHand) {
      discardPlayerTile(tileId);
      return;
    }

    game.selectedTileId = null;
    setMessage("上推手牌可以直接打出。");
    render();
  }

  function attachDocumentGestureListeners() {
    if (typeof document.addEventListener !== "function" || detachDocumentListeners) {
      return;
    }
    document.addEventListener("pointermove", moveGesture);
    document.addEventListener("pointerup", finishGesture);
    document.addEventListener("pointercancel", finishGesture);
    detachDocumentListeners = () => {
      document.removeEventListener("pointermove", moveGesture);
      document.removeEventListener("pointerup", finishGesture);
      document.removeEventListener("pointercancel", finishGesture);
    };
  }

  element.addEventListener("pointermove", moveGesture);
  element.addEventListener("pointerup", finishGesture);
  element.addEventListener("pointercancel", finishGesture);
}

function renderTile(tile, options = {}) {
  const { clickable = false, selected = false, small = false, showWild = false } = options;
  const element = document.createElement("div");
  const classes = ["tile"];
  const className = tile.suit === "honor" ? "honor" : SUIT_CONFIG[tile.suit].className;
  classes.push(className);
  if (clickable) {
    classes.push("clickable");
  }
  if (selected) {
    classes.push("selected");
  }
  if (small) {
    classes.push("small");
  }
  if (showWild && isTileWild(tile)) {
    classes.push("wild");
  }
  element.className = classes.join(" ");

  element.innerHTML = getTileFaceMarkup(tile);
  element.title = tileLabel(tile);
  return element;
}

function renderActionBar() {
  dom.actionBar.innerHTML = "";
  if (game.winner || game.drawReason) {
    return;
  }

  const actions = [];

  if (game.phase === "human-discard") {
    const player = getPlayer(0);
    const canSelfHu = isWinningHand(player.hand, game.wildKey);
    const canGang = !player.piaoCai && getConcealedGangChoices(player.hand, game.wildKey).length > 0;
    if (canSelfHu) {
      actions.push({ label: "胡", primary: true, handler: () => declareWin(0, player.piaoCai ? "飘财" : "自摸", null, game.lastDraw?.tile || null) });
    }
    if (canGang) {
      actions.push({ label: "暗杠", handler: () => promptConcealedGang() });
    }
    if (!player.piaoCai && !canSelfHu) {
      const waits = getWaitTiles(player.hand, game.wildKey);
      if (waits.length > 0) {
        actions.push({ label: "飘财", handler: () => declarePiaoCai(0) });
      }
    }
  }

  if (game.phase === "claim" && game.claimOptions.length > 0) {
    game.claimOptions.forEach((option) => {
      actions.push({
        label: option.label,
        primary: option.label === "胡",
        handler: option.handler,
      });
    });
    actions.push({ label: "过", handler: () => passClaim() });
  }

  if (actions.length === 0 && getPlayer(0).piaoCai) {
    const badge = document.createElement("span");
    badge.className = "hud-pill wild-badge";
    badge.textContent = "飘财中 · 等胡";
    dom.actionBar.appendChild(badge);
    return;
  }

  actions.forEach((action) => {
    const button = document.createElement("button");
    button.type = "button";
    button.className = `action-button ${action.primary ? "primary" : ""}`;
    button.textContent = action.label;
    button.addEventListener("click", action.handler);
    dom.actionBar.appendChild(button);
  });
}

function renderSidebar() {
  const player = getPlayer(0);
  const waits = game.phase === "human-discard" ? buildDiscardHints(player.hand) : getWaitTiles(player.hand, game.wildKey);
  dom.hintList.innerHTML = "";

  if (player.piaoCai) {
    const line = document.createElement("div");
    line.style.color = "#ffe4a4";
    line.textContent = "飘财中 · 胡牌双倍";
    dom.hintList.appendChild(line);
    const rawWaits = getWaitTiles(player.hand, game.wildKey);
    if (rawWaits.length > 0) {
      const row = document.createElement("div");
      row.className = "wait-row";
      const head = document.createElement("span");
      head.textContent = "听牌:";
      row.appendChild(head);
      uniqueTilesByKey(rawWaits).slice(0, 8).forEach((tile) => {
        const tag = renderTile(tile, { small: true, showWild: true });
        tag.classList.add("wait-tile");
        row.appendChild(tag);
      });
      dom.hintList.appendChild(row);
    }
  } else if (game.phase !== "human-discard") {
    const line = document.createElement("div");
    line.textContent = "等待轮到你时会显示听牌建议。";
    dom.hintList.appendChild(line);
  } else if (waits.length === 0) {
    const line = document.createElement("div");
    line.textContent = "当前还没成听。";
    dom.hintList.appendChild(line);
  } else {
    waits.slice(0, 8).forEach((item) => {
      const row = document.createElement("div");
      row.className = "wait-row";
      const head = document.createElement("span");
      head.textContent = `${item.discard ? `打 ${tileLabel(item.discard)}` : "听牌"}:`;
      row.appendChild(head);
      item.waits.forEach((tile) => {
        const tag = renderTile(tile, { small: true, showWild: true });
        tag.classList.add("wait-tile");
        row.appendChild(tag);
      });
      dom.hintList.appendChild(row);
    });
  }

  dom.logList.innerHTML = "";
  game.logs.forEach((entry) => {
    const item = document.createElement("div");
    item.className = "log-item";
    item.textContent = entry;
    dom.logList.appendChild(item);
  });
}

function renderStatus() {
  dom.wallCount.textContent = `牌墙 ${game.wall.length}`;
  dom.turnText.textContent = `当前 ${getPlayer(game.turn).wind}位 ${getPlayer(game.turn).name}`;
  dom.statusLine.textContent = game.message;
  dom.wildTileBadge.innerHTML = `<img class="badge-tile" src="${tileAssetPath(game.wildTile)}" alt=""><span>${tileLabel(game.wildTile)}财神</span>`;
}

function renderLastDiscard() {
  dom.lastDiscard.innerHTML = "";
  if (!game.lastDiscard) {
    return;
  }
  const block = document.createElement("div");
  block.className = "last-discard-card";
  const text = document.createElement("span");

  const { fromDraw } = game.lastDiscard;
  if (fromDraw === true) {
    text.textContent = `${getPlayer(game.lastDiscard.playerId).name} 进牌打出`;
    block.classList.add("draw-keep");
  } else if (fromDraw === false) {
    text.textContent = `${getPlayer(game.lastDiscard.playerId).name} 摸到即打`;
    block.classList.add("draw-dump");
  } else {
    text.textContent = `${getPlayer(game.lastDiscard.playerId).name} 刚打出`;
  }

  block.appendChild(text);
  block.appendChild(renderTile(game.lastDiscard.tile, { showWild: true }));
  dom.lastDiscard.appendChild(block);
}

function renderWinnerModal() {
  const existing = document.querySelector(".winner-modal");
  if (existing) {
    existing.remove();
  }

  if (!game.winner && !game.drawReason) {
    return;
  }

  const modal = document.createElement("div");
  modal.className = "winner-modal";
  const card = document.createElement("div");
  card.className = "winner-card";

  if (game.drawReason) {
    card.innerHTML = `<h2>流局</h2><p>${game.drawReason}</p><p>点击下方开始新局继续。</p>`;
  } else {
    const winner = getPlayer(game.winner.playerId);
    const fromText = game.winner.fromPlayerId == null ? "自摸" : `点炮：${getPlayer(game.winner.fromPlayerId).name}`;
    const handRack = document.createElement("div");
    handRack.className = "hand-rack";
    winner.hand.forEach((tile) => handRack.appendChild(renderTile(tile, { showWild: true })));

    card.innerHTML = `<h2>${winner.name} ${game.winner.kind}</h2><p>${game.winner.summary}</p><p>${fromText}</p>`;
    card.appendChild(handRack);
  }

  const button = document.createElement("button");
  button.type = "button";
  button.className = "action-button primary";
  button.textContent = "再来一局";
  button.addEventListener("click", startNewGame);
  card.appendChild(button);
  modal.appendChild(card);
  document.body.appendChild(modal);
}

function buildDiscardHints(hand) {
  const hints = [];
  const seen = new Set();

  hand.forEach((tile) => {
    const key = tileKey(tile);
    if (seen.has(key)) {
      return;
    }
    seen.add(key);
    const trial = hand.filter((item) => item.id !== tile.id);
    const waits = getWaitTiles(trial, game.wildKey);
    if (waits.length > 0) {
      hints.push({ discard: tile, waits });
    }
  });

  hints.sort((a, b) => b.waits.length - a.waits.length || tileSortValue(a.discard) - tileSortValue(b.discard));
  return hints;
}

function discardPlayerTile(tileId) {
  const player = getPlayer(0);
  const index = player.hand.findIndex((tile) => tile.id === tileId);
  if (index === -1) {
    return;
  }

  const [tile] = player.hand.splice(index, 1);
  sortTiles(player.hand);
  player.discards.push(tile);

  const drawRecord = game.lastDraw;
  const drewThisTurn = drawRecord && drawRecord.playerId === 0;
  const sameAsDrawn = drewThisTurn && tileKey(tile) === tileKey(drawRecord.tile);
  game.lastDiscard = { playerId: 0, tile, fromDraw: drewThisTurn ? !sameAsDrawn : null };

  game.selectedTileId = null;
  addLog(`你打出 ${tileLabel(tile)}。`);
  setMessage("等待其他三家判断。");
  AudioManager.discard();
  checkClaimsAfterDiscard(0, tile);
  saveGame();
  render();
}

function getConcealedGangChoices(hand, wildKey) {
  const counts = new Map();
  hand.forEach((tile) => {
    const key = tileKey(tile);
    counts.set(key, (counts.get(key) || []).concat(tile));
  });

  return [...counts.values()].filter((tiles) => tiles.length === 4 && tileKey(tiles[0]) !== wildKey);
}

function promptConcealedGang() {
  const choices = getConcealedGangChoices(getPlayer(0).hand, game.wildKey);
  if (choices.length === 0) {
    return;
  }

  game.phase = "claim";
  game.claimOptions = choices.map((tiles) => ({
    label: `杠 ${tileLabel(tiles[0])}`,
    handler: () => performConcealedGang(0, tiles[0]),
  }));
  setMessage("选择要暗杠的牌。");
  render();
}

function performConcealedGang(playerId, referenceTile) {
  const player = getPlayer(playerId);
  const matching = player.hand.filter((tile) => tileKey(tile) === tileKey(referenceTile));
  if (matching.length < 4) {
    return;
  }

  player.hand = player.hand.filter((tile) => tileKey(tile) !== tileKey(referenceTile));
  player.melds.push({ type: "gang", concealed: true, tiles: matching.slice(0, 4) });
  addLog(`${player.name} 暗杠 ${tileLabel(referenceTile)}。`);
  AudioManager.gang();
  game.claimOptions = [];
  game.phase = null;
  drawTileForPlayer(playerId, "杠后补牌");
}

function declarePiaoCai(playerId) {
  const player = getPlayer(playerId);
  if (player.piaoCai) return;
  const waits = getWaitTiles(player.hand, game.wildKey);
  if (waits.length === 0) return;

  player.piaoCai = true;
  addLog(`${player.name} 飘财！亮出财神，听牌等胡。`);
  AudioManager.peng();
  setMessage(`${player.name} 已飘财，摸到即打，胡牌双倍。`);
  game.phase = null;
  game.claimOptions = [];
  saveGame();
  render();
  advanceTurn(playerId);
}

function piaoCaiAutoDiscard(playerId) {
  const player = getPlayer(playerId);
  const drawnTile = game.lastDraw.tile;
  const idx = player.hand.findIndex((t) => t.id === drawnTile.id);
  if (idx === -1) {
    advanceTurn(playerId);
    return;
  }
  const [discarded] = player.hand.splice(idx, 1);
  sortTiles(player.hand);
  player.discards.push(discarded);

  game.lastDiscard = { playerId, tile: discarded, fromDraw: true };
  game.claimOptions = [];
  setMessage(`${player.name} 飘财摸到 ${tileLabel(discarded)}，即打。`);

  if (player.isHuman) {
    addLog(`你飘财摸到 ${tileLabel(discarded)}，即打。`);
  } else {
    addLog(`${player.name} 飘财，打出 ${tileLabel(discarded)}。`);
  }
  AudioManager.discard();
  checkClaimsAfterDiscard(playerId, discarded);
  saveGame();
  render();
}

function advanceTurn(playerId) {
  game.turn = (playerId + 1) % 4;
  if (game.winner || game.drawReason) return;
  schedule(() => takeTurn(game.turn), TURN_DELAY);
}

function shouldAiPiaoCai(playerId) {
  const player = getPlayer(playerId);
  const waits = getWaitTiles(player.hand, game.wildKey);
  if (waits.length === 0) return false;

  resetSharedMemo();
  const waitRemain = availableWaitCount(waits, playerId);
  if (waitRemain < 5) return false;

  const waitQuality = evaluateWaitQuality(waits, playerId);
  const wallsRemain = game.wall.length;
  const drawChance = waitRemain / Math.max(1, wallsRemain);
  const expectedValue = drawChance * waitQuality * 1.6;

  return expectedValue > 0.22;
}

function passClaim() {
  game.claimOptions = [];
  game.phase = null;
  AudioManager.pass();
  if (game.lastDiscard) {
    advanceTurnFromDiscard(game.lastDiscard.playerId);
  }
  saveGame();
  render();
}

function advanceTurnFromDiscard(discarderId) {
  const nextPlayerId = (discarderId + 1) % 4;
  game.turn = nextPlayerId;
  schedule(() => takeTurn(nextPlayerId), TURN_DELAY);
}

function checkClaimsAfterDiscard(discarderId, tile) {
  const claimList = [];

  const nextPlayerId = (discarderId + 1) % 4;
  const nextPlayer = getPlayer(nextPlayerId);
  if (!nextPlayer.piaoCai) {
    const chiChoices = getChiOptions(nextPlayer.hand, tile, game.wildKey);
    if (chiChoices.length > 0) {
      claimList.push({ playerId: nextPlayerId, type: "chi", priority: 1, choices: chiChoices });
    }
  }

  for (let offset = 1; offset <= 3; offset += 1) {
    const playerId = (discarderId + offset) % 4;
    const claimant = getPlayer(playerId);
    if (claimant.piaoCai) continue;
    const hand = claimant.hand;
    const count = hand.filter((handTile) => tileKey(handTile) === tileKey(tile)).length;
    if (count >= 2 && tileKey(tile) !== game.wildKey) {
      claimList.push({ playerId, type: "peng", priority: 2 });
    }
    if (count >= 3 && tileKey(tile) !== game.wildKey) {
      claimList.push({ playerId, type: "gang", priority: 2.5 });
    }
  }

  if (claimList.length === 0) {
    advanceTurnFromDiscard(discarderId);
    return;
  }

  const resolvedClaim = chooseClaimToResolve(claimList, tile, discarderId);
  if (!resolvedClaim) {
    advanceTurnFromDiscard(discarderId);
    return;
  }
  resolveClaim(resolvedClaim, tile, discarderId);
}

function distanceFromDiscarder(discarderId, playerId) {
  return (playerId - discarderId + 4) % 4;
}

function chooseClaimToResolve(claimList, tile, discarderId) {
  const viableClaims = claimList.filter((claim) => {
    const player = getPlayer(claim.playerId);
    if (player.isHuman || claim.type === "hu") {
      return true;
    }
    if (claim.type === "gang") {
      return shouldAiMeldGang(claim.playerId, tile);
    }
    if (claim.type === "peng") {
      return shouldAiPeng(claim.playerId, tile);
    }
    if (claim.type === "chi") {
      return chooseAiChiChoice(claim.playerId, tile, claim.choices) !== null;
    }
    return false;
  });

  if (viableClaims.length === 0) {
    return null;
  }

  viableClaims.sort((a, b) => b.priority - a.priority || distanceFromDiscarder(discarderId, a.playerId) - distanceFromDiscarder(discarderId, b.playerId));
  return viableClaims[0];
}

function resolveClaim(claim, tile, discarderId) {
  const player = getPlayer(claim.playerId);

  if (player.isHuman) {
    const options = [];
    if (claim.type === "peng") {
      options.push({ label: "碰", handler: () => performPeng(0, tile, discarderId) });
    }
    if (claim.type === "gang") {
      options.push({ label: "杠", handler: () => performMeldGang(0, tile, discarderId) });
    }
    if (claim.type === "chi") {
      claim.choices.forEach((choice, index) => {
        options.push({
          label: `吃${index + 1}`,
          handler: () => performChi(0, tile, discarderId, choice),
        });
      });
    }

    game.phase = "claim";
    game.claimOptions = options;
    setMessage(`你可以对 ${tileLabel(tile)} 操作。（按键 1-${options.length} 选择，空格跳过）`);
    render();
    return;
  }

  schedule(() => {
    if (claim.type === "gang") {
      performMeldGang(claim.playerId, tile, discarderId);
      return;
    }
    if (claim.type === "peng") {
      performPeng(claim.playerId, tile, discarderId);
      return;
    }
    if (claim.type === "chi") {
      const choice = chooseAiChiChoice(claim.playerId, tile, claim.choices);
      if (choice) {
        performChi(claim.playerId, tile, discarderId, choice);
      } else {
        advanceTurnFromDiscard(discarderId);
      }
    }
  }, CLAIM_DELAY);
}

function getChiOptions(hand, tile, wildKey) {
  if (tile.suit === "honor" || tileKey(tile) === wildKey) {
    return [];
  }
  const options = [];
  const rank = Number(tile.rank);
  const candidates = [
    [rank - 2, rank - 1],
    [rank - 1, rank + 1],
    [rank + 1, rank + 2],
  ];

  candidates.forEach((pair) => {
    if (pair.some((value) => value < 1 || value > 9)) {
      return;
    }
    const chosen = [];
    for (const value of pair) {
      const tileFound = hand.find((handTile) => handTile.suit === tile.suit && Number(handTile.rank) === value && tileKey(handTile) !== wildKey && !chosen.includes(handTile.id));
      if (!tileFound) {
        return;
      }
      chosen.push(tileFound.id);
    }
    options.push(chosen);
  });

  return options;
}

function takeTurn(playerId) {
  if (game.winner || game.drawReason) {
    return;
  }
  game.turn = playerId;
  drawTileForPlayer(playerId, "摸牌");
}

function drawTileForPlayer(playerId, reason) {
  if (game.wall.length === 0) {
    game.drawReason = "牌墙已空，本局流局。";
    setMessage(game.drawReason);
    render();
    return;
  }

  const tile = game.wall.pop();
  const player = getPlayer(playerId);
  player.hand.push(tile);
  sortTiles(player.hand);
  game.lastDraw = { playerId, tile, reason };
  addLog(`${player.name}${reason === "摸牌" ? "摸到" : "补到"}一张牌。`);
  AudioManager.draw();

  const winningNow = isWinningHand(player.hand, game.wildKey);
  if (winningNow) {
    if (player.isHuman) {
      game.phase = "human-discard";
      const extra = player.piaoCai ? "（飘财双倍）" : "";
      setMessage((reason === "杠后补牌" ? "杠开可胡" : "你已成胡") + extra + "，可以点胡或继续上推出牌。");
      render();
      return;
    }
    const kind = player.piaoCai ? "飘财" : reason === "杠后补牌" ? "杠开" : "自摸";
    schedule(() => declareWin(playerId, kind, null, tile), TURN_DELAY);
    return;
  }

  if (player.piaoCai) {
    if (player.isHuman) {
      game.phase = null;
      setMessage("飘财中，自动打出刚摸的牌...");
      render();
      schedule(() => piaoCaiAutoDiscard(playerId), 380);
      return;
    }
    schedule(() => piaoCaiAutoDiscard(playerId), TURN_DELAY);
    return;
  }

  if (player.isHuman) {
    game.phase = "human-discard";
    setMessage(reason === "杠后补牌" ? "补牌完成，上推或按 D 键出牌。" : "轮到你，上推或按 D 键出牌。");
    render();
    return;
  }

  schedule(() => aiDiscard(playerId), TURN_DELAY);
}

function performPeng(playerId, tile, discarderId) {
  const player = getPlayer(playerId);
  const taken = removeMatchingTiles(player.hand, tileKey(tile), 2);
  player.melds.push({ type: "peng", concealed: false, tiles: [...taken, tile] });
  removeLastDiscardFromRiver(discarderId);
  game.lastDiscard = null;
  game.claimOptions = [];
  addLog(`${player.name} 碰 ${tileLabel(tile)}。`);
  AudioManager.peng();
  game.turn = playerId;
  if (player.isHuman) {
    game.phase = "human-discard";
    setMessage("你已碰牌，上推一张手牌打出。");
    render();
  } else {
    schedule(() => aiDiscard(playerId), TURN_DELAY);
  }
}

function performMeldGang(playerId, tile, discarderId) {
  const player = getPlayer(playerId);
  const taken = removeMatchingTiles(player.hand, tileKey(tile), 3);
  player.melds.push({ type: "gang", concealed: false, tiles: [...taken, tile] });
  removeLastDiscardFromRiver(discarderId);
  game.lastDiscard = null;
  game.claimOptions = [];
  addLog(`${player.name} 明杠 ${tileLabel(tile)}。`);
  AudioManager.gang();
  game.turn = playerId;
  drawTileForPlayer(playerId, "杠后补牌");
}

function performChi(playerId, tile, discarderId, choiceIds) {
  const player = getPlayer(playerId);
  const chosenTiles = choiceIds
    .map((id) => player.hand.find((handTile) => handTile.id === id))
    .filter(Boolean);
  player.hand = player.hand.filter((handTile) => !choiceIds.includes(handTile.id));
  sortTiles(player.hand);
  const meldTiles = [...chosenTiles, tile].sort((a, b) => tileSortValue(a) - tileSortValue(b));
  player.melds.push({ type: "chi", concealed: false, tiles: meldTiles });
  removeLastDiscardFromRiver(discarderId);
  game.lastDiscard = null;
  game.claimOptions = [];
  addLog(`${player.name} 吃 ${tileLabel(tile)}。`);
  AudioManager.chi();
  game.turn = playerId;
  if (player.isHuman) {
    game.phase = "human-discard";
    setMessage("你已吃牌，上推一张手牌打出。");
    render();
  } else {
    schedule(() => aiDiscard(playerId), TURN_DELAY);
  }
}

function removeMatchingTiles(hand, key, count) {
  const removed = [];
  for (let i = hand.length - 1; i >= 0 && removed.length < count; i -= 1) {
    if (tileKey(hand[i]) === key) {
      removed.push(hand[i]);
      hand.splice(i, 1);
    }
  }
  return removed;
}

function removeLastDiscardFromRiver(playerId) {
  const discards = getPlayer(playerId).discards;
  discards.pop();
}

function aiDiscard(playerId) {
  const player = getPlayer(playerId);
  if (!player || player.hand.length === 0 || game.winner) {
    return;
  }

  if (!player.piaoCai && shouldAiPiaoCai(playerId)) {
    declarePiaoCai(playerId);
    return;
  }

  const gangChoices = getConcealedGangChoices(player.hand, game.wildKey);
  const gangChoice = chooseAiConcealedGang(playerId, gangChoices);
  if (gangChoice) {
    performConcealedGang(playerId, gangChoice[0]);
    return;
  }

  const tile = chooseAiDiscard(player.hand, playerId);
  const index = player.hand.findIndex((item) => item.id === tile.id);
  player.hand.splice(index, 1);
  sortTiles(player.hand);
  player.discards.push(tile);

  const drawRecord = game.lastDraw;
  const drewThisTurn = drawRecord && drawRecord.playerId === playerId;
  const sameAsDrawn = drewThisTurn && tileKey(tile) === tileKey(drawRecord.tile);
  game.lastDiscard = { playerId, tile, fromDraw: drewThisTurn ? !sameAsDrawn : null };

  if (drewThisTurn) {
    if (sameAsDrawn) {
      addLog(`${player.name} 打出刚摸的 ${tileLabel(tile)}。`);
    } else {
      addLog(`${player.name} 进牌，打出 ${tileLabel(tile)}。`);
    }
  } else {
    addLog(`${player.name} 打出 ${tileLabel(tile)}。`);
  }

  AudioManager.discard();
  setMessage(`轮到 ${getPlayer((playerId + 1) % 4).name}。`);
  checkClaimsAfterDiscard(playerId, tile);
  saveGame();
  render();
}

function declareWin(playerId, kind, fromPlayerId, winningTile = null) {
  const winner = getPlayer(playerId);
  const selfDrawTile = game.lastDraw?.playerId === playerId ? game.lastDraw.tile : null;

  if (winningTile && fromPlayerId != null) {
    winner.hand.push(cloneTile(winningTile));
    sortTiles(winner.hand);
  }

  const flags = [];
  if (isSevenPairs(winner.hand, game.wildKey)) {
    flags.push("七对");
  } else {
    flags.push("平胡");
  }
  if (countWildcards(winner.hand, game.wildKey) > 0) {
    flags.push("财神入手");
  }
  if (kind === "杠开") {
    flags.push("杠开");
  }
  if (winner.piaoCai) {
    flags.push("飘财");
  }
  if (isBaotou(winner.hand, game.wildKey, winningTile || selfDrawTile)) {
    flags.push("爆头");
  }

  const base = flags.includes("七对") ? 24 : 12;
  const bonus = flags.includes("杠开") ? 8 : 0;
  let total = base + bonus;
  if (winner.piaoCai) {
    total *= 2;
  }

  if (fromPlayerId == null) {
    game.players.forEach((player) => {
      if (player.id !== playerId) {
        player.score -= total;
        winner.score += total;
      }
    });
  } else {
    getPlayer(fromPlayerId).score -= total * 2;
    winner.score += total * 2;
  }

  game.winner = {
    playerId,
    fromPlayerId,
    kind,
    summary: `${flags.join(" · ")}，${fromPlayerId == null ? "三家付分" : "点炮付分"} ${total}${fromPlayerId == null ? "/家" : "x2"}`,
  };
  game.phase = null;
  game.claimOptions = [];
  setMessage(`${winner.name}${kind}。`);
  addLog(`${winner.name}${kind}，牌型：${flags.join("、")}。`);
  AudioManager.win();
  ParticleSpawner.centerBurst();
  localStorage.removeItem("hangma_save");
  render();
}

function schedule(callback, delay) {
  game.locked = true;
  setTimeout(() => {
    game.locked = false;
    callback();
  }, delay);
}

function startNewGame() {
  game = newRound();
  saveGame();
  render();
}

function saveGame() {
  if (!game || game.winner || game.drawReason) {
    localStorage.removeItem("hangma_save");
    return;
  }
  try {
    const saveData = {
      players: game.players.map((p) => ({
        id: p.id, name: p.name, wind: p.wind, isHuman: p.isHuman, seatKey: p.seatKey,
        hand: p.hand.map(cloneTile), melds: p.melds, discards: p.discards.map(cloneTile), score: p.score, piaoCai: p.piaoCai,
      })),
      wall: game.wall.map(cloneTile),
      dealer: game.dealer, turn: game.turn, phase: game.phase,
      selectedTileId: game.selectedTileId, claimOptions: game.claimOptions.map((o) => ({ label: o.label })),
      lastDiscard: game.lastDiscard ? { playerId: game.lastDiscard.playerId, tile: cloneTile(game.lastDiscard.tile) } : null,
      lastDraw: game.lastDraw ? { playerId: game.lastDraw.playerId, tile: cloneTile(game.lastDraw.tile), reason: game.lastDraw.reason } : null,
      winner: game.winner, drawReason: game.drawReason,
      wildTile: cloneTile(game.wildTile), wildKey: game.wildKey,
      message: game.message, logs: [...game.logs], locked: false,
    };
    localStorage.setItem("hangma_save", JSON.stringify(saveData));
  } catch (e) { /* quota exceeded */ }
}

function restoreGame() {
  try {
    const raw = localStorage.getItem("hangma_save");
    if (!raw) return null;
    const data = JSON.parse(raw);
    data.players.forEach((p) => { p.hand = p.hand.map((t) => ({ ...t })); p.discards = p.discards.map((t) => ({ ...t })); });
    data.wall = data.wall.map((t) => ({ ...t }));
    if (data.lastDiscard) data.lastDiscard.tile = { ...data.lastDiscard.tile };
    if (data.lastDraw) data.lastDraw.tile = { ...data.lastDraw.tile };
    data.wildTile = { ...data.wildTile };
    data.locked = false;
    data.claimOptions = [];
    data.selectedTileId = null;
    if (data.phase !== "human-discard") data.phase = "human-discard";
    return data;
  } catch (e) { return null; }
}

dom.newGameBtn.addEventListener("click", startNewGame);
if (dom.soundBtn) {
  if (!AudioManager.enabled) {
    dom.soundBtn.className = "icon-button sound-off";
    dom.soundBtn.textContent = "✕";
  }
  dom.soundBtn.addEventListener("click", () => {
    const on = AudioManager.toggle();
    dom.soundBtn.className = `icon-button ${on ? "sound-on" : "sound-off"}`;
    dom.soundBtn.textContent = on ? "♪" : "✕";
    if (on) AudioManager.click();
  });
}

setupKeyboardShortcuts();

const saved = restoreGame();
if (saved) {
  game = saved;
  game.message = "恢复上一局，上推手牌出牌。";
} else {
  game = newRound();
}
render();
