// ============================================================
// app.js — 杭州麻将 UI 层
// ============================================================
// 本文件负责所有渲染、音效、输入处理和游戏流程编排。
// 所有规则判定和 AI 决策通过 engine.js 提供的接口完成。
//
// 引擎接口见 engine.js 中的 JSDoc 文档和 [TODO: IMPLEMENT] 标记。
// ============================================================

import {
  SUIT_CONFIG,
  HONOR_LABELS,
  CHINESE_NUMERALS,
  SEATS,
  INITIAL_SCORE,
  tileKey,
  tileLabel,
  tileSortValue,
  sortTiles,
  cloneTile,
  tileAssetName,
  tileAssetPath,
  isTileWild,
  createNewGame,
  isWinningHand,
  isWinningWithMelds,
  getWaitTiles,
  getWaitTilesWithMelds,
  getDiscardHintsWithMelds,
  getChiOptions,
  getPengOptions,
  getConcealedGangChoices,
  getDiscardHints,
  chooseDiscard,
  shouldPeng,
  shouldMeldGang,
  chooseChi,
  chooseConcealedGang,
  shouldPiaoCai,
  resolveClaim,
  calculateWinScore,
  checkFourWindDiscard,
  countWildInHand,
} from "./engine.js";
import { fromGameState, chooseBestDiscard, tileTypeIndex, computeShanten } from './ai/game-sim.js';
import { getCoachMode, isCoachMode, onModeChange, createModeToggleButton } from './mode-toggle.js';
import { initCoachPanel, showCoachPanel, hideCoachPanel, minimizeForDraw, expandAfterDiscard, setCoachMessages, clearCoachMessages } from './coach-panel.js';

const TURN_DELAY = 550;
const CLAIM_DELAY = 400;

// ============================================================
// AudioManager
// ============================================================

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

// ============================================================
// ParticleSpawner
// ============================================================

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

// ============================================================
// DOM 引用
// ============================================================

const dom = {
  playerHand: document.querySelector("#playerHand"),
  playerMelds: document.querySelector("#playerMelds"),
  seatTop: document.querySelector("#seatTop"),
  seatLeft: document.querySelector("#seatLeft"),
  seatRight: document.querySelector("#seatRight"),
  riverBoard: document.querySelector("#riverBoard"),
  actionBar: document.querySelector("#actionBar"),
  remainingCount: document.querySelector("#remainingCount"),
  turnText: document.querySelector("#turnText"),
  statusLine: document.querySelector("#statusLine"),
  wildTileBadge: document.querySelector("#wildTileBadge"),
  handCount: document.querySelector("#handCount"),
  hintList: document.querySelector("#hintList"),
  logList: document.querySelector("#logList"),
  scoreStrip: document.querySelector("#scoreStrip"),
  lastDiscard: document.querySelector("#lastDiscard"),
  newGameBtn: document.querySelector("#newGameBtn"),
  resetScoreBtn: document.querySelector("#resetScoreBtn"),
  soundBtn: document.querySelector("#soundBtn"),
  difficultySelect: document.querySelector("#difficultySelect"),
};

// ============================================================
// 游戏状态
// ============================================================

let game = null;

// ============================================================
// 工具函数
// ============================================================

function getPlayer(playerId) {
  return game.players[playerId];
}

function addLog(text) {
  game.logs.unshift(text);
  game.logs = game.logs.slice(0, 18);
}

function setMessage(text) {
  game.message = text;
}

function schedule(callback, delay) {
  game.locked = true;
  setTimeout(() => {
    game.locked = false;
    callback();
  }, delay);
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

// ============================================================
// 渲染函数
// ============================================================

function getTileFaceMarkup(tile) {
  return `
    <div class="tile-face">
      <img class="tile-art" src="${tileAssetPath(tile)}" alt="" aria-hidden="true">
    </div>
  `;
}

function renderTile(tile, options = {}) {
  const { clickable = false, selected = false, small = false, showWild = false } = options;
  const element = document.createElement("div");
  const classes = ["tile"];
  const className = tile.suit === "honor" ? "honor" : SUIT_CONFIG[tile.suit].className;
  classes.push(className);
  if (clickable) classes.push("clickable");
  if (selected) classes.push("selected");
  if (small) classes.push("small");
  if (showWild && isTileWild(tile, game.wildKey)) classes.push("wild");
  element.className = classes.join(" ");

  element.innerHTML = getTileFaceMarkup(tile);
  element.title = tileLabel(tile);
  return element;
}

function renderMeld(meld) {
  const container = document.createElement("div");
  container.className = "meld";
  meld.tiles.forEach((tile) => {
    container.appendChild(renderTile(tile, { small: true, showWild: true }));
  });
  return container;
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

// ============================================================
// 手动理牌 — 自定义手牌顺序
// ============================================================

function saveHandOrder() {
  game._handOrder = getPlayer(0).hand.map(t => t.id);
  game._handCustomOrder = true;
}

function restoreHandOrder(player) {
  if (!game._handOrder || game._handOrder.length === 0) {
    sortTiles(player.hand);
    saveHandOrder();
    return;
  }
  const tileMap = new Map(player.hand.map(t => [t.id, t]));
  const ordered = [];
  for (const id of game._handOrder) {
    const tile = tileMap.get(id);
    if (tile) {
      ordered.push(tile);
      tileMap.delete(id);
    }
  }
  const newTiles = [...tileMap.values()];
  sortTiles(newTiles);
  for (let i = newTiles.length - 1; i >= 0; i--) {
    const t = newTiles[i];
    ordered.unshift(t);
    game._handOrder.unshift(t.id);
  }
  player.hand = ordered;
}

function sortPlayerHand(player) {
  if (player.isHuman && game._handCustomOrder) {
    restoreHandOrder(player);
  } else {
    sortTiles(player.hand);
  }
}

function resetHandOrder() {
  game._handCustomOrder = false;
  game._handOrder = [];
}

function renderPlayerHand() {
  const player = getPlayer(0);
  dom.playerHand.innerHTML = "";
  if (game._handCustomOrder) {
    restoreHandOrder(player);
  }
  player.hand.forEach((tile) => {
    const selected = game.selectedTileId === tile.id;
    const el = renderTile(tile, { clickable: true, selected, showWild: true });
    bindDiscardGesture(el, tile.id);
    dom.playerHand.appendChild(el);
  });
  dom.handCount.textContent = `${player.hand.length} 张`;
}

function renderActionBar() {
  dom.actionBar.innerHTML = "";
  if (game.winner || game.drawReason) {
    return;
  }

  const actions = [];

  if (game.phase === "human-discard") {
    const player = getPlayer(0);
    const canSelfHu = isWinningWithMelds(player.hand, player.melds, game.wildKey);
    const canGang = !player.piaoCai && getConcealedGangChoices(player.hand, game.wildKey).length > 0;
    const wildCount = countWildInHand(player.hand, game.wildKey);

    if (canSelfHu) {
      const kind = player.piaoCai ? "飘财" : "自摸";
      actions.push({ label: "胡", primary: true, handler: () => declareWin(0, kind, null, game.lastDraw?.tile || null) });
      // 暴头状态可飘财：手中有≥2张财神时可放弃胡牌，打出财神等下一轮暴头
      if (!player.piaoCai && wildCount >= 2) {
        actions.push({ label: "飘财", handler: () => declarePiaoCai(0) });
      }
      // 飘财回轮时，若还有≥2张财神可继续飘
      if (player.piaoCai && wildCount >= 2) {
        actions.push({ label: "继续飘财", handler: () => declarePiaoCai(0) });
      }
    }
    if (canGang) {
      actions.push({ label: "暗杠", handler: () => promptConcealedGang() });
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
  const waits = game.phase === "human-discard" ? getDiscardHintsWithMelds(player.hand, player.melds, game.wildKey) : getWaitTilesWithMelds(player.hand, player.melds, game.wildKey);
  dom.hintList.innerHTML = "";

  if (player.piaoCai) {
    const line = document.createElement("div");
    line.style.color = "#ffe4a4";
    line.textContent = "飘财中 · 胡牌双倍";
    dom.hintList.appendChild(line);
    const rawWaits = getWaitTilesWithMelds(player.hand, player.melds, game.wildKey);
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
  dom.remainingCount.textContent = `剩余 ${game.tilePool.length} 张`;
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

// ============================================================
// 手势 + 键盘输入
// ============================================================

function bindDiscardGesture(element, tileId) {
  let startX = 0, startY = 0;
  let currentX = 0, currentY = 0;
  let pointerId = null;
  let dragging = false;
  let ghost = null;
  let everAboveHand = false;
  let rearranging = false;
  let originIndex = -1;
  let insertIndicator = null;
  let detachDocumentListeners = null;

  const HAND_REARRANGE_THRESHOLD = 12;

  function handTop() {
    return dom.playerHand.getBoundingClientRect().top;
  }

  function isAboveHand(y) {
    return y < handTop() - 12;
  }

  function canDiscard() {
    return game.phase === "human-discard" && !game.locked;
  }

  function createGhost(srcEl) {
    const el = document.createElement("div");
    el.className = "tile-ghost";
    el.innerHTML = srcEl.querySelector(".tile-face")?.outerHTML || "";
    document.body.appendChild(el);
    return el;
  }

  function handTileElements() {
    return [...dom.playerHand.querySelectorAll(".tile:not(.insert-indicator)")];
  }

  function insertIndexAt(clientX) {
    const tiles = handTileElements();
    if (tiles.length === 0) return 0;
    for (let i = 0; i < tiles.length; i++) {
      const rect = tiles[i].getBoundingClientRect();
      if (clientX < rect.left + rect.width / 2) return i;
    }
    return tiles.length;
  }

  function showInsertIndicator(clientX) {
    hideInsertIndicator();
    const idx = insertIndexAt(clientX);
    insertIndicator = document.createElement("div");
    insertIndicator.className = "insert-indicator";
    const tiles = handTileElements();
    if (idx < tiles.length) {
      dom.playerHand.insertBefore(insertIndicator, tiles[idx]);
    } else {
      dom.playerHand.appendChild(insertIndicator);
    }
  }

  function hideInsertIndicator() {
    if (insertIndicator) {
      insertIndicator.remove();
      insertIndicator = null;
    }
    handTileElements().forEach(c => {
      c.classList.remove("make-room-left", "make-room-right");
    });
  }

  element.addEventListener("pointerdown", (event) => {
    event.preventDefault();
    pointerId = event.pointerId;
    startX = event.clientX;
    startY = event.clientY;
    currentX = startX;
    currentY = startY;
    everAboveHand = false;
    rearranging = false;
    dragging = true;
    originIndex = getPlayer(0).hand.findIndex(t => t.id === tileId);
    game.selectedTileId = tileId;
    ghost = createGhost(element);
    ghost.style.left = `${event.clientX}px`;
    ghost.style.top = `${event.clientY}px`;
    element.classList.add("ghosting");
    element.setPointerCapture?.(pointerId);
    attachDocumentGestureListeners();
    setMessage(canDiscard() ? "松手打出，水平拖拽可理牌。" : "水平拖拽可理牌。");
    renderStatus();
  });

  function moveGesture(event) {
    if (!dragging || event.pointerId !== pointerId) {
      return;
    }
    currentX = event.clientX;
    currentY = event.clientY;

    if (isAboveHand(currentY) && canDiscard()) {
      everAboveHand = true;
      if (rearranging) {
        rearranging = false;
        hideInsertIndicator();
        element.classList.remove("rearranging");
        renderPlayerHand();
      }
    }

    if (!everAboveHand) {
      const dx = Math.abs(currentX - startX);
      const dy = Math.abs(currentY - startY);
      if (dx > HAND_REARRANGE_THRESHOLD && dx > dy * 2) {
        if (!rearranging) {
          rearranging = true;
          element.classList.add("rearranging");
        }
        showInsertIndicator(currentX);
      } else if (rearranging && dy > HAND_REARRANGE_THRESHOLD) {
        rearranging = false;
        hideInsertIndicator();
        element.classList.remove("rearranging");
      }
    }

    if (ghost) {
      ghost.style.left = `${currentX}px`;
      ghost.style.top = rearranging ? `${handTop() + 34}px` : `${currentY}px`;
      ghost.classList.toggle("ghost-rearrange", rearranging);
      ghost.classList.toggle("ghost-ready", !rearranging && isAboveHand(currentY));
      ghost.classList.toggle("ghost-return", !rearranging && !isAboveHand(currentY));
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
    element.classList.remove("ghosting", "selected", "rearranging");
    hideInsertIndicator();
    if (ghost) {
      ghost.remove();
      ghost = null;
    }
    pointerId = null;

    if (rearranging) {
      const targetIdx = insertIndexAt(currentX);
      const player = getPlayer(0);
      const curIdx = player.hand.findIndex(t => t.id === tileId);
      if (curIdx !== -1 && targetIdx !== curIdx) {
        const adjusted = targetIdx > curIdx ? targetIdx - 1 : targetIdx;
        const [tile] = player.hand.splice(curIdx, 1);
        player.hand.splice(adjusted, 0, tile);
      }
      saveHandOrder();
      game.selectedTileId = null;
      setMessage("");
      render();
      return;
    }

    if (!canDiscard()) {
      game.selectedTileId = null;
      setMessage("");
      render();
      return;
    }

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

// ============================================================
// 工具：去重牌
// ============================================================

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

// ============================================================
// 游戏流程 — 出牌
// ============================================================

function discardPlayerTile(tileId) {
  const player = getPlayer(0);
  const index = player.hand.findIndex((tile) => tile.id === tileId);
  if (index === -1) {
    return;
  }

  const [tile] = player.hand.splice(index, 1);
  sortPlayerHand(player);
  player.discards.push(tile);

  const drawRecord = game.lastDraw;
  const drewThisTurn = drawRecord && drawRecord.playerId === 0;
  const sameAsDrawn = drewThisTurn && tileKey(tile) === tileKey(drawRecord.tile);
  game.lastDiscard = { playerId: 0, tile, fromDraw: drewThisTurn ? !sameAsDrawn : null };

  game.selectedTileId = null;
  addLog(`你打出 ${tileLabel(tile)}。`);
  setMessage("等待其他三家判断。");
  AudioManager.discard();
  _coachAfterDiscard(tile);
  checkClaimsAfterDiscard(0, tile);
  saveGame();
  render();
}

// ============================================================
// 游戏流程 — 吃碰杠判定
// ============================================================

function checkClaimsAfterDiscard(discarderId, tile) {
  // 飘财期间：任何人不能吃碰明杠（只能暗杠/自摸胡）
  const someonePiaoCai = game.players.some(p => p.piaoCai);

  const claimList = [];

  if (!someonePiaoCai) {
    const nextPlayerId = (discarderId + 1) % 4;
    const nextPlayer = getPlayer(nextPlayerId);
    if (!nextPlayer.piaoCai) {
      // 庄下家吃庄需先亮财神（手牌中有财神才能吃）
      const isDealerNext = discarderId === game.dealer;
      const hasWild = nextPlayer.hand.some((t) => tileKey(t) === game.wildKey);
      if (!isDealerNext || hasWild) {
        const chiChoices = getChiOptions(nextPlayer.hand, tile, game.wildKey);
        if (chiChoices.length > 0) {
          claimList.push({ playerId: nextPlayerId, type: "chi", priority: 1, choices: chiChoices });
        }
      }
    }

    for (let offset = 1; offset <= 3; offset += 1) {
      const playerId = (discarderId + offset) % 4;
      const claimant = getPlayer(playerId);
      if (claimant.piaoCai) continue;
      const claim = getPengOptions(claimant.hand, tile, game.wildKey);
      if (claim) {
        claimList.push({ playerId, type: claim.type, priority: claim.type === "gang" ? 2.5 : 2 });
      }
    }
  }

  if (claimList.length === 0) {
    advanceTurnFromDiscard(discarderId);
    return;
  }

  const chosenClaim = chooseClaimToResolve(claimList, tile, discarderId);
  if (!chosenClaim) {
    advanceTurnFromDiscard(discarderId);
    return;
  }
  executeClaim(chosenClaim, tile, discarderId);
}

function chooseClaimToResolve(claimList, tile, discarderId) {
  return resolveClaim(claimList, tile, discarderId, game);
}

function executeClaim(claim, tile, discarderId) {
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
      claim.choices.forEach((choice) => {
        options.push({
          label: `吃 · ${choice.label}`,
          handler: () => performChi(0, tile, discarderId, choice.ids),
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
      const choice = chooseChi(claim.playerId, tile, claim.choices, game);
      if (choice) {
        performChi(claim.playerId, tile, discarderId, choice.ids);
      } else {
        advanceTurnFromDiscard(discarderId);
      }
    }
  }, CLAIM_DELAY);
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

// ============================================================
// 游戏流程 — 四风连打
// ============================================================

function handleFourWindDiscard(windLabel) {
  const dice1 = Math.floor(Math.random() * 6) + 1;
  const dice2 = Math.floor(Math.random() * 6) + 1;
  const penalty = dice1 + dice2;

  const dealer = getPlayer(game.dealer);
  game.players.forEach((p) => {
    if (p.id !== game.dealer) {
      dealer.score -= penalty;
      p.score += penalty;
    }
  });

  addLog(`四风连打！四家首弃均为${windLabel}，庄家掷骰子 ${dice1}+${dice2}=${penalty}，付每家${penalty}分。本局重开。`);
  setMessage(`四风连打 — 庄家赔付${penalty * 3}分，同庄重开。`);
  game.winner = null;
  game.drawReason = "四风连打";
  game.phase = null;
  game.claimOptions = [];
  saveGame();
  render();

  schedule(() => {
    restartWithDealer(game.dealer);
  }, 1800);
}

function restartWithDealer(dealerId) {
  game = createNewGame();
  game.dealer = dealerId;
  game.turn = dealerId;
  game._firstDiscards = [];
  game._revealedTile = null;
  // Reveal last tile of the wall for 杠后补牌
  if (game.tilePool.length > 0) {
    game._revealedTile = game.tilePool[game.tilePool.length - 1];
  }
  // If dealer is not player 0, re-deal: dealer gets 14 tiles
  if (dealerId !== 0) {
    const dealerPlayer = getPlayer(dealerId);
    const player0 = getPlayer(0);
    // Swap starting hand sizes: original code always gives player0 14 tiles
    // For non-0 dealer, we need to adjust
    while (dealerPlayer.hand.length < 14) {
      dealerPlayer.hand.push(player0.hand.pop());
    }
    sortPlayerHand(dealerPlayer);
    sortPlayerHand(player0);
  }
  game.phase = "human-discard";
  game.message = `新局开始（${getPlayer(dealerId).wind}位连庄），上推手牌出牌。`;
  game.logs.unshift(`新局开始，${getPlayer(dealerId).wind}位连庄。`);
  saveGame();
  render();
}

// ============================================================
// 游戏流程 — 回合推进
// ============================================================

function advanceTurnFromDiscard(discarderId) {
  game._discardCount = (game._discardCount || 0) + 1;

  // 记录首弃牌（四风连打检测）
  if (game._firstDiscards && game.lastDiscard) {
    const alreadyRecorded = game._firstDiscards.some(d => d.playerId === discarderId);
    if (!alreadyRecorded) {
      game._firstDiscards.push({ playerId: discarderId, tile: game.lastDiscard.tile });
      if (game._firstDiscards.length === 4) {
        const result = checkFourWindDiscard(game._firstDiscards);
        if (result.isFourWind) {
          handleFourWindDiscard(result.windLabel);
          return;
        }
      }
    }
  }

  const nextPlayerId = (discarderId + 1) % 4;
  game.turn = nextPlayerId;
  schedule(() => takeTurn(nextPlayerId), TURN_DELAY);
}

function advanceTurn(playerId) {
  game.turn = (playerId + 1) % 4;
  if (game.winner || game.drawReason) return;
  schedule(() => takeTurn(game.turn), TURN_DELAY);
}

function takeTurn(playerId) {
  if (game.winner || game.drawReason) return;
  game.turn = playerId;
  drawTileForPlayer(playerId, "摸牌");
}

function drawTileForPlayer(playerId, reason) {
  if (game.tilePool.length === 0) {
    game.drawReason = "牌已摸完，本局流局。";
    game._nextDealer = game.dealer; // 流局原庄连庄
    setMessage(game.drawReason);
    render();
    return;
  }

  // 杠后补牌：从倒数第二张补，倒数第一张翻开公示
  let tile;
  if (reason === "杠后补牌" && game.tilePool.length >= 2) {
    tile = game.tilePool.splice(game.tilePool.length - 2, 1)[0];
    // _revealedTile stays as the last tile
  } else {
    tile = game.tilePool.pop();
    // Update revealed tile to new last tile (if any)
    game._revealedTile = game.tilePool.length > 0 ? game.tilePool[game.tilePool.length - 1] : null;
  }

  const player = getPlayer(playerId);
  player.hand.push(tile);
  sortPlayerHand(player);
  game.lastDraw = { playerId, tile, reason };
  addLog(`${player.name}${reason === "摸牌" ? "摸到" : "补到"}一张牌。`);
  AudioManager.draw();

  const winningNow = isWinningWithMelds(player.hand, player.melds, game.wildKey);
  if (winningNow) {
    if (player.isHuman) {
      game.phase = "human-discard";
      const extra = player.piaoCai ? "（飘财双倍）" : "";
      setMessage((reason === "杠后补牌" ? "杠开可胡" : "你已成胡") + extra + "，可以点胡或继续上推出牌。");
      minimizeForDraw();
      render();
      return;
    }
    // AI: 暴头状态可飘财（首次或继续）
    if (countWildInHand(player.hand, game.wildKey) >= 2) {
      if (shouldPiaoCai(playerId, game)) {
        declarePiaoCai(playerId);
        return;
      }
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
    minimizeForDraw();
    render();
    // Trigger coach analysis in background worker (non-blocking)
    _requestCoachAnalysis('brief');
    return;
  }

  schedule(() => aiDiscard(playerId), TURN_DELAY);
}

// ============================================================
// 游戏流程 — 吃碰杠执行
// ============================================================

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
  sortPlayerHand(player);
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

// ============================================================
// 游戏流程 — 杠
// ============================================================

function promptConcealedGang() {
  const choices = getConcealedGangChoices(getPlayer(0).hand, game.wildKey);
  if (choices.length === 0) return;

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
  if (matching.length < 4) return;

  player.hand = player.hand.filter((tile) => tileKey(tile) !== tileKey(referenceTile));
  player.melds.push({ type: "gang", concealed: true, tiles: matching.slice(0, 4) });
  addLog(`${player.name} 暗杠 ${tileLabel(referenceTile)}。`);
  AudioManager.gang();
  game.claimOptions = [];
  game.phase = null;
  drawTileForPlayer(playerId, "杠后补牌");
}

// ============================================================
// 游戏流程 — 飘财
// ============================================================

function declarePiaoCai(playerId) {
  const player = getPlayer(playerId);

  // 飘财需要手中至少2张财神
  const wildCount = countWildInHand(player.hand, game.wildKey);
  if (wildCount < 2) return;

  // 找到手中一张财神牌，将其打出（保留刚摸的牌）
  const drawnTile = game.lastDraw?.tile;
  const wildTile = player.hand.find(t =>
    tileKey(t) === game.wildKey && (!drawnTile || t.id !== drawnTile.id)
  ) || player.hand.find(t => tileKey(t) === game.wildKey);
  if (!wildTile) return;

  const idx = player.hand.indexOf(wildTile);
  player.hand.splice(idx, 1);
  sortPlayerHand(player);
  player.discards.push(wildTile);
  game.lastDiscard = { playerId, tile: wildTile, fromDraw: false };

  // --- 多飘：已在飘财模式，摸到胡牌后选择继续飘 ---
  if (player.piaoCai) {
    player.piaoCount = (player.piaoCount || 0) + 1;

    const piaoMult = 1 << (player.piaoCount + 1);
    addLog(`${player.name} 继续飘财（${player.piaoCount}飘），倍率×${piaoMult}。`);
    AudioManager.peng();
    setMessage(`${player.name} 继续飘财，倍率×${piaoMult}。`);
    game.phase = null;
    game.claimOptions = [];
    saveGame();
    render();
    advanceTurn(playerId);
    return;
  }

  // --- 首次飘财 ---
  player.piaoCai = true;
  player.piaoCount = (player.piaoCount || 0) + 1;
  game._piaoCaiOrigin = playerId;
  game._piaoCaiTurnsTaken = 0;
  addLog(`${player.name} 飘财！打出财神亮出暴头，等下一轮自摸。`);
  AudioManager.peng();
  setMessage(`${player.name} 已飘财，下一轮摸任意牌暴头胡牌。`);
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
  sortPlayerHand(player);
  player.discards.push(discarded);

  game.lastDiscard = { playerId, tile: discarded, fromDraw: false };
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

// ============================================================
// 游戏流程 — AI 出牌（ISMCTS via Web Worker）
// ============================================================

const HONOR_ORDER = ['east', 'south', 'west', 'north', 'red', 'green', 'white'];

function _tileIdxToSuitRank(idx) {
  if (idx < 9) return { suit: 'character', rank: idx + 1 };
  if (idx < 18) return { suit: 'dot', rank: idx - 9 + 1 };
  if (idx < 27) return { suit: 'bamboo', rank: idx - 18 + 1 };
  return { suit: 'honor', rank: HONOR_ORDER[idx - 27] };
}

// --- Web Worker management ---

let _aiWorker = null;
let _aiCallback = null;

function _getAIWorker() {
  if (!_aiWorker) {
    try {
      _aiWorker = new Worker('./ai/ai-worker.js', { type: 'module' });
      _aiWorker.onmessage = (e) => {
        if (!_aiCallback) return;
        const cb = _aiCallback;
        _aiCallback = null;
        const data = e.data;
        if (data.error) {
          cb.reject(new Error(data.message));
        } else {
          cb.resolve(data);
        }
      };
      _aiWorker.onerror = () => {
        if (!_aiCallback) return;
        const cb = _aiCallback;
        _aiCallback = null;
        cb.reject(new Error('Worker error'));
      };
    } catch (_e) {
      _aiWorker = null;
    }
  }
  return _aiWorker;
}

// --- Coach Web Worker (always-on, receives analysis results) ---

let _coachWorker = null;
let _coachAnalysisPending = false;

function _getCoachWorker() {
  if (!_coachWorker) {
    try {
      _coachWorker = new Worker('./coach/coach-worker.js?v=3', { type: 'module' });
      _coachWorker.onmessage = (e) => {
        _coachAnalysisPending = false;
        const data = e.data;
        if (data.error) {
          console.warn('Coach worker error:', data.error);
          return;
        }
        if (data.type === 'coachAnalysis' && data.messages) {
          // Augment messages with reasoning field for the UI
          const msgs = data.messages.map(m => ({
            ...m,
            summary: (m.summary || '').replace(/\bundefined\b/g, '?'),
            reasoning: (m.candidateBreakdown || m.reasoning || '').replace(/\bundefined\b/g, '?'),
          }));
          setCoachMessages(msgs);
        }
      };
      _coachWorker.onerror = () => {
        _coachAnalysisPending = false;
        console.warn('Coach worker crashed');
      };
    } catch (_e) {
      _coachWorker = null;
    }
  }
  return _coachWorker;
}

/** Trigger coach analysis from Web Worker (non-blocking) */
function _requestCoachAnalysis(depth = 'standard') {
  if (!isCoachMode()) return;
  if (_coachAnalysisPending) return; // don't queue up multiple requests

  const worker = _getCoachWorker();
  if (!worker) return;

  _coachAnalysisPending = true;
  const gameData = _serializeGameForWorker(game);
  worker.postMessage({ type: 'analyze', gameData, depth, playerDiscard: null });
}

function _serializeGameForWorker(g) {
  return {
    turn: g.turn,
    dealer: g.dealer,
    wildTile: { suit: g.wildTile.suit, rank: g.wildTile.rank },
    _discardCount: g._discardCount || 0,
    players: g.players.map(p => ({
      hand: p.hand.map(t => ({ suit: t.suit, rank: t.rank })),
      melds: p.melds.map(m => ({
        type: m.type,
        concealed: m.concealed,
        tiles: m.tiles.map(t => ({ suit: t.suit, rank: t.rank })),
      })),
      discards: p.discards.map(t => ({ suit: t.suit, rank: t.rank })),
      piaoCai: p.piaoCai,
    })),
    tilePool: g.tilePool.map(t => ({ suit: t.suit, rank: t.rank })),
  };
}

function _fallbackDiscard(playerId) {
  try {
    const sim = fromGameState(game);
    const result = chooseBestDiscard(sim, playerId);
    if (result.tileIdx >= 0) {
      const sr = _tileIdxToSuitRank(result.tileIdx);
      return { tileIdx: result.tileIdx, suit: sr.suit, rank: sr.rank, source: 'fallback' };
    }
  } catch (_e) { /* fall through */ }
  // Absolute last resort
  const player = getPlayer(playerId);
  const last = player.hand[player.hand.length - 1];
  return { tileIdx: -1, suit: last.suit, rank: last.rank, source: 'fallback' };
}

function _requestAIDiscard(playerId) {
  return new Promise((resolve) => {
    const worker = _getAIWorker();
    const gameData = _serializeGameForWorker(game);
    const level = game.aiDifficulty || 'normal';

    const timeoutId = setTimeout(() => {
      _aiCallback = null;
      resolve(_fallbackDiscard(playerId));
    }, 1000);

    if (!worker) {
      clearTimeout(timeoutId);
      resolve(_fallbackDiscard(playerId));
      return;
    }

    _aiCallback = {
      resolve: (data) => {
        clearTimeout(timeoutId);
        resolve({ tileIdx: data.tileIdx, suit: data.suit, rank: data.rank, source: data.source || 'ismcts' });
      },
      reject: () => {
        clearTimeout(timeoutId);
        resolve(_fallbackDiscard(playerId));
      },
    };

    worker.postMessage({ type: 'chooseDiscard', gameData, playerId, difficulty: level });
  });
}

function _finishAIDiscard(playerId, suit, rank) {
  const player = getPlayer(playerId);
  const tile = player.hand.find(t => t.suit === suit && t.rank === rank);
  if (!tile) {
    // Fallback: AI returned a tile not in hand — discard the last tile
    const lastTile = player.hand[player.hand.length - 1];
    if (!lastTile) {
      game.locked = false;
      return;
    }
    doDiscardAndAdvance(playerId, lastTile);
    return;
  }

  doDiscardAndAdvance(playerId, tile);
}

function doDiscardAndAdvance(playerId, tile) {
  const player = getPlayer(playerId);
  const index = player.hand.findIndex(item => item.id === tile.id);
  player.hand.splice(index, 1);
  sortPlayerHand(player);
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

  game.locked = false;
  AudioManager.discard();
  setMessage(`轮到 ${getPlayer((playerId + 1) % 4).name}。`);
  checkClaimsAfterDiscard(playerId, tile);
  saveGame();
  render();
}

function aiDiscard(playerId) {
  const player = getPlayer(playerId);
  if (!player || player.hand.length === 0 || game.winner) return;

  const gangChoices = getConcealedGangChoices(player.hand, game.wildKey);
  const gangChoice = chooseConcealedGang(playerId, gangChoices, game);
  if (gangChoice) {
    performConcealedGang(playerId, gangChoice[0]);
    return;
  }

  game.locked = true;
  _requestAIDiscard(playerId).then(({ suit, rank }) => {
    if (game.winner) { game.locked = false; return; }
    _finishAIDiscard(playerId, suit, rank);
  }).catch(() => {
    // Safety: if anything fails, discard last tile to avoid deadlock
    game.locked = false;
    if (game.winner) return;
    const p = getPlayer(playerId);
    if (p && p.hand.length > 0) {
      doDiscardAndAdvance(playerId, p.hand[p.hand.length - 1]);
    }
  });
}

// ============================================================
// 游戏流程 — 胡牌
// ============================================================

function declareWin(playerId, kind, fromPlayerId, winningTile = null) {
  const winner = getPlayer(playerId);
  const selfDrawTile = game.lastDraw?.playerId === playerId ? game.lastDraw.tile : null;

  if (winningTile && fromPlayerId != null) {
    winner.hand.push(cloneTile(winningTile));
    sortPlayerHand(winner);
  }

  const fullHand = [...winner.hand];
  for (const meld of winner.melds) {
    for (const tile of meld.tiles) fullHand.push(cloneTile(tile));
  }

  const isDealer = playerId === game.dealer;
  const isFirstTurn = (game._discardCount || 0) === 0;
  const isFirstDraw = !isDealer && winner.discards.length === 0 && (game._discardCount || 0) <= 1;

  const { flags, multiplier, isSelfDraw } = calculateWinScore({
    hand: fullHand,
    wildKey: game.wildKey,
    kind,
    winningTile: winningTile || selfDrawTile,
    piaoCount: winner.piaoCount || 0,
    isDealer,
    isFirstDraw,
    isFirstTurn,
  });

  if (isSelfDraw) {
    game.players.forEach((player) => {
      if (player.id !== playerId) {
        const isDealerPayer = player.id === game.dealer;
        const amount = isDealer
          ? multiplier * 8
          : (isDealerPayer ? multiplier * 8 : multiplier * 1);
        player.score -= amount;
        winner.score += amount;
      }
    });
  } else {
    const amount = multiplier * 8 * 2;
    getPlayer(fromPlayerId).score -= amount;
    winner.score += amount;
  }

  game.winner = {
    playerId,
    fromPlayerId,
    kind,
    summary: `${flags.join(" · ")}，倍率×${multiplier}，${isSelfDraw ? "自摸三家付分" : "点炮付分"}`,
  };
  game._nextDealer = playerId; // 赢家当庄
  game.phase = null;
  game.claimOptions = [];
  setMessage(`${winner.name}${kind}。`);
  addLog(`${winner.name}${kind}，牌型：${flags.join("、")}。`);
  AudioManager.win();
  ParticleSpawner.centerBurst();
  localStorage.removeItem("hangma_save");
  render();
}

// ============================================================
// 存档
// ============================================================

function startNewGame() {
  const prevDealer = game?._nextDealer;
  const prevScores = game ? game.players.map(p => p.score) : null;
  game = createNewGame();
  // Apply saved difficulty
  game.aiDifficulty = localStorage.getItem("hangma_difficulty") || "normal";
  // Preserve scores across rounds
  if (prevScores) {
    game.players.forEach((p, i) => { p.score = prevScores[i]; });
  }
  if (prevDealer != null) {
    game.dealer = prevDealer;
    game.turn = prevDealer;
    game.message = `流局 — ${getPlayer(prevDealer).wind}位连庄，上推手牌出牌。`;
    game.logs.unshift(`流局，${getPlayer(prevDealer).wind}位连庄。`);
  }
  // Reveal last tile of the wall for 杠后补牌 display
  if (game.tilePool.length > 0) {
    game._revealedTile = game.tilePool[game.tilePool.length - 1];
  }
  saveGame();
  clearCoachMessages();
  render();
}

// ---- 教练面板：弃牌后生成分析消息 ----

function _coachAfterDiscard(discardedTile) {
  if (!isCoachMode()) return;
  expandAfterDiscard();

  const worker = _getCoachWorker();
  if (!worker) return;

  // Temporarily add the discarded tile back to player's hand for pre-discard analysis
  const player = getPlayer(0);
  const tileIndex = player.hand.findIndex(t =>
    t.suit === discardedTile.suit && t.rank === discardedTile.rank);
  let addedBack = false;
  if (tileIndex === -1 && !player.piaoCai) {
    player.hand.push(discardedTile);
    addedBack = true;
  }

  _coachAnalysisPending = true;
  const gameData = _serializeGameForWorker(game);
  worker.postMessage({
    type: 'analyze',
    gameData,
    depth: 'brief',
    playerDiscard: { suit: discardedTile.suit, rank: discardedTile.rank },
  });

  // Restore hand
  if (addedBack) {
    player.hand.pop();
  }
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
        hand: p.hand.map(cloneTile), melds: p.melds, discards: p.discards.map(cloneTile), score: p.score, piaoCai: p.piaoCai, piaoCount: p.piaoCount || 0,
      })),
      tilePool: game.tilePool.map(cloneTile),
      dealer: game.dealer, turn: game.turn, phase: game.phase,
      selectedTileId: game.selectedTileId, claimOptions: game.claimOptions.map((o) => ({ label: o.label })),
      lastDiscard: game.lastDiscard ? { playerId: game.lastDiscard.playerId, tile: cloneTile(game.lastDiscard.tile), fromDraw: game.lastDiscard.fromDraw } : null,
      lastDraw: game.lastDraw ? { playerId: game.lastDraw.playerId, tile: cloneTile(game.lastDraw.tile), reason: game.lastDraw.reason } : null,
      winner: game.winner, drawReason: game.drawReason,
      wildTile: cloneTile(game.wildTile), wildKey: game.wildKey,
      message: game.message, logs: [...game.logs], locked: false,
      _discardCount: game._discardCount || 0,
      _firstDiscards: (game._firstDiscards || []).map(d => ({ playerId: d.playerId, tile: cloneTile(d.tile) })),
      _revealedTile: game._revealedTile ? cloneTile(game._revealedTile) : null,
      _piaoCaiOrigin: game._piaoCaiOrigin ?? -1,
      _handCustomOrder: game._handCustomOrder || false,
      _handOrder: game._handOrder ? [...game._handOrder] : [],
      _nextDealer: game._nextDealer,
      aiDifficulty: game.aiDifficulty || 'normal',
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
    data.tilePool = data.tilePool.map((t) => ({ ...t }));
    if (data.lastDiscard) data.lastDiscard.tile = { ...data.lastDiscard.tile };
    if (data.lastDraw) data.lastDraw.tile = { ...data.lastDraw.tile };
    data.wildTile = { ...data.wildTile };
    data.locked = false;
    data._discardCount = data._discardCount || 0;
    data._firstDiscards = (data._firstDiscards || []).map(d => ({ playerId: d.playerId, tile: { ...d.tile } }));
    data._revealedTile = data._revealedTile ? { ...data._revealedTile } : null;
    data._piaoCaiOrigin = data._piaoCaiOrigin ?? -1;
    data._piaoCaiTurnsTaken = data._piaoCaiTurnsTaken || 0;
    data._handCustomOrder = data._handCustomOrder || false;
    data._handOrder = data._handOrder || [];
    data._nextDealer = data._nextDealer ?? undefined;
    data.aiDifficulty = data.aiDifficulty || 'normal';
    data.claimOptions = [];
    data.selectedTileId = null;
    if (data.phase !== "human-discard") data.phase = "human-discard";
    return data;
  } catch (e) { return null; }
}

// ============================================================
// 初始化
// ============================================================

dom.newGameBtn.addEventListener("click", startNewGame);

dom.resetScoreBtn.addEventListener("click", () => {
  game.players.forEach(p => { p.score = INITIAL_SCORE; });
  game._nextDealer = undefined;
  localStorage.removeItem("hangma_save");
  startNewGame();
});

// Difficulty selector
const DIFFICULTY_KEY = "hangma_difficulty";
if (dom.difficultySelect) {
  const savedDifficulty = localStorage.getItem(DIFFICULTY_KEY) || "normal";
  dom.difficultySelect.value = savedDifficulty;
  dom.difficultySelect.addEventListener("change", () => {
    const val = dom.difficultySelect.value;
    localStorage.setItem(DIFFICULTY_KEY, val);
    if (game) game.aiDifficulty = val;
  });
}

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

// 听牌建议折叠
const HINT_COLLAPSED_KEY = "hangma_hint_collapsed";
const hintToggle = document.querySelector("#hintToggle");
const hintPanel = hintToggle?.closest(".hint-panel");
if (hintToggle && hintPanel) {
  const isCollapsed = localStorage.getItem(HINT_COLLAPSED_KEY) !== "false";
  hintPanel.classList.toggle("collapsed", isCollapsed);
  hintToggle.textContent = isCollapsed ? "听牌 ▶" : "听牌 ▼";
  hintToggle.addEventListener("click", () => {
    const collapsed = hintPanel.classList.toggle("collapsed");
    hintToggle.textContent = collapsed ? "听牌 ▶" : "听牌 ▼";
    localStorage.setItem(HINT_COLLAPSED_KEY, String(collapsed));
  });
}

const saved = restoreGame();
if (saved) {
  game = saved;
  game.message = "恢复上一局，上推手牌出牌。";
} else {
  game = createNewGame();
  game.aiDifficulty = localStorage.getItem("hangma_difficulty") || "normal";
}
render();

// ---- 教练面板初始化 ----
initCoachPanel();

// 模式切换按钮
const modeSlot = document.querySelector("#modeToggleSlot");
if (modeSlot) {
  modeSlot.appendChild(createModeToggleButton());
}

// 模式变化 → 显示/隐藏教练面板
onModeChange((mode) => {
  if (mode === 'coach') {
    showCoachPanel();
  } else {
    hideCoachPanel();
  }
});

// 启动时自动显示教练面板（如果已是教练模式）
if (isCoachMode()) {
  showCoachPanel();
}
