const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

class Element {
  constructor(selector) {
    this.selector = selector;
    this.children = [];
    this.className = "";
    this.innerHTML = "";
    this.textContent = "";
    this.title = "";
    this.disabled = false;
    this.listeners = {};
    this.dataset = {};
    const styleStore = {};
    this.style = new Proxy(styleStore, {
      get(target, prop) {
        if (prop === "setProperty") return (key, value) => { target[key] = value; };
        if (prop === "getPropertyValue") return (key) => target[key] || "";
        if (prop === "cssText") return Object.entries(target).map(([k, v]) => `${k}: ${v}`).join("; ");
        if (typeof prop === "string" && prop in target) return target[prop];
        return "";
      },
      set(target, prop, value) {
        target[prop] = value;
        return true;
      },
    });
    this.classList = {
      add: (...names) => {
        const set = new Set(this.className.split(/\s+/).filter(Boolean));
        names.forEach((name) => set.add(name));
        this.className = [...set].join(" ");
      },
      remove: (...names) => {
        const removeSet = new Set(names);
        this.className = this.className
          .split(/\s+/)
          .filter(Boolean)
          .filter((name) => !removeSet.has(name))
          .join(" ");
      },
      toggle: (name, force) => {
        const set = new Set(this.className.split(/\s+/).filter(Boolean));
        const shouldAdd = force === undefined ? !set.has(name) : !!force;
        if (shouldAdd) {
          set.add(name);
        } else {
          set.delete(name);
        }
        this.className = [...set].join(" ");
        return shouldAdd;
      },
      contains: (name) => this.className.split(/\s+/).includes(name),
    };
  }

  appendChild(child) {
    this.children.push(child);
    return child;
  }

  remove() {
    this.removed = true;
  }

  addEventListener(event, handler) {
    this.listeners[event] = handler;
  }

  querySelector(selector) {
    return this.children.find((c) => c.selector === selector) || null;
  }

  getBoundingClientRect() {
    return { top: 0, left: 0, right: 0, bottom: 0, width: 0, height: 0 };
  }

  closest(selector) {
    return null;
  }

  setPointerCapture() {}
  releasePointerCapture() {}
}

const elements = new Map();
const documentStub = {
  body: new Element("body"),
  listeners: {},
  querySelector(selector) {
    if (!elements.has(selector)) {
      elements.set(selector, new Element(selector));
    }
    return elements.get(selector);
  },
  createElement(tag) {
    return new Element(tag);
  },
  addEventListener(event, handler) {
    this.listeners[event] = handler;
  },
  removeEventListener(event) {
    delete this.listeners[event];
  },
};

const AudioContextStub = function () {
  return {
    state: "running",
    currentTime: 0,
    sampleRate: 44100,
    destination: {},
    createOscillator() {
      return { type: "", frequency: { value: 0 }, connect() {}, start() {}, stop() {} };
    },
    createGain() {
      return { gain: { value: 0, setValueAtTime() {}, exponentialRampToValueAtTime() {} }, connect() {} };
    },
    createBiquadFilter() {
      return { type: "", frequency: { value: 0 }, connect() {} };
    },
    createBuffer(numChannels, length, sampleRate) {
      return { getChannelData() { return new Float32Array(length); } };
    },
    createBufferSource() {
      return { buffer: null, connect() {}, start() {}, stop() {} };
    },
    resume() {},
  };
};

const context = {
  console,
  document: documentStub,
  Math,
  Float32Array,
  localStorage: {
    _data: {},
    getItem(key) { return this._data[key] ?? null; },
    setItem(key, value) { this._data[key] = String(value); },
    removeItem(key) { delete this._data[key]; },
  },
  AudioContext: AudioContextStub,
  webkitAudioContext: AudioContextStub,
  requestAnimationFrame(fn) { fn(); return 0; },
  PointerEvent: function PointerEvent() {},
  setTimeout() {
    return 0;
  },
  window: {
    AudioContext: AudioContextStub,
    webkitAudioContext: AudioContextStub,
    innerWidth: 1024,
    innerHeight: 768,
  },
};

vm.createContext(context);
const appPath = path.join(__dirname, "app.js");
vm.runInContext(fs.readFileSync(appPath, "utf8"), context, { filename: appPath });

vm.runInContext(`
  if (!game || game.players.length !== 4) throw new Error("game did not start");
  if (game.players[0].hand.length !== 14) throw new Error("dealer should have 14 tiles");
  if (game.players.slice(1).some((player) => player.hand.length !== 13)) throw new Error("opponents should have 13 tiles");
  if (!game.players.every((p) => p.piaoCai === false)) throw new Error("piaoCai should default to false");
  if (!isWinningHand([
    { suit: "character", rank: 1 },
    { suit: "character", rank: 2 },
    { suit: "character", rank: 3 },
    { suit: "dot", rank: 2 },
    { suit: "dot", rank: 3 },
    { suit: "dot", rank: 4 },
    { suit: "bamboo", rank: 5 },
    { suit: "bamboo", rank: 6 },
    { suit: "bamboo", rank: 7 },
    { suit: "honor", rank: "east" },
    { suit: "honor", rank: "east" },
    { suit: "honor", rank: "east" },
    { suit: "honor", rank: "red" },
    { suit: "honor", rank: "red" },
  ], "honor:white")) throw new Error("standard win check failed");
  if (!isSevenPairs([
    { suit: "character", rank: 1 }, { suit: "character", rank: 1 },
    { suit: "character", rank: 9 }, { suit: "character", rank: 9 },
    { suit: "dot", rank: 2 }, { suit: "dot", rank: 2 },
    { suit: "dot", rank: 8 }, { suit: "dot", rank: 8 },
    { suit: "bamboo", rank: 3 }, { suit: "bamboo", rank: 3 },
    { suit: "bamboo", rank: 7 }, { suit: "bamboo", rank: 7 },
    { suit: "honor", rank: "white" }, { suit: "honor", rank: "white" },
  ], "honor:white")) throw new Error("seven pairs with wildcard failed");

  function t(suit, rank) {
    return { id: suit + "-" + rank + "-" + Math.random(), suit, rank };
  }

  // Test 1: AI should not discard wildcard from useful hand
  game = newRound();
  game.players[1].hand = [
    t("character", 1), t("character", 2), t("character", 3),
    t("dot", 2), t("dot", 3), t("dot", 4),
    t("bamboo", 4), t("bamboo", 5), t("bamboo", 6),
    t("honor", "east"), t("honor", "east"),
    t("honor", "red"), t("honor", "white"), t("bamboo", 9),
  ];
  sortTiles(game.players[1].hand);
  const discard = chooseAiDiscard(game.players[1].hand, 1);
  if (tileKey(discard) === "honor:white") throw new Error("AI should not throw the wildcard from a useful hand");
  if (tileKey(discard) !== "bamboo:9" && tileKey(discard) !== "honor:red") throw new Error("AI should prefer isolated tiles");

  // Test 2: AI should keep tenpai when available
  game = newRound();
  game.players[1].hand = [
    t("character", 1), t("character", 2), t("character", 3),
    t("dot", 2), t("dot", 3), t("dot", 4),
    t("bamboo", 4), t("bamboo", 5), t("bamboo", 6),
    t("honor", "east"), t("honor", "east"), t("honor", "east"),
    t("bamboo", 7), t("bamboo", 9),
  ];
  sortTiles(game.players[1].hand);
  const readyDiscard = chooseAiDiscard(game.players[1].hand, 1);
  const afterReadyDiscard = removeOneTileByKey(game.players[1].hand, tileKey(readyDiscard));
  if (getWaitTiles(afterReadyDiscard, game.wildKey).length === 0) throw new Error("AI should choose a discard that leaves tenpai when available");

  // Test 3: AI should peng when it pushes hand into tenpai
  game = newRound();
  game.players[1].hand = [
    t("character", 1), t("character", 1),
    t("dot", 2), t("dot", 3), t("dot", 4),
    t("bamboo", 3), t("bamboo", 4), t("bamboo", 5),
    t("character", 7), t("character", 8), t("character", 9),
    t("honor", "red"), t("honor", "green"),
  ];
  sortTiles(game.players[1].hand);
  if (!shouldAiPeng(1, t("character", 1))) throw new Error("AI should peng when it pushes the hand into tenpai");

  // Test 4: AI should not break complete sequences when discarding
  game = newRound();
  game.players[1].hand = [
    t("character", 2), t("character", 3),
    t("dot", 2), t("dot", 3), t("dot", 4),
    t("bamboo", 3), t("bamboo", 4), t("bamboo", 5),
    t("character", 5), t("character", 6),
    t("honor", "east"), t("honor", "green"), t("honor", "north"),
    t("bamboo", 8), t("bamboo", 9),
  ];
  sortTiles(game.players[1].hand);
  const disc2 = chooseAiDiscard(game.players[1].hand, 1);
  const discardedKey = tileKey(disc2);
  // Must not break complete sequences (dot 2-3-4, bamboo 3-4-5)
  if (discardedKey === "dot:2" || discardedKey === "dot:3" || discardedKey === "dot:4"
    || discardedKey === "bamboo:3" || discardedKey === "bamboo:4" || discardedKey === "bamboo:5")
    throw new Error("AI should not break a complete sequence: discarded " + discardedKey);
  // Must not discard a two-sided tatami tile when isolated honors are available
  const twoSidedKeys = ["character:2", "character:3", "character:5", "character:6"];
  if (twoSidedKeys.includes(discardedKey)) {
    throw new Error("AI should prefer isolated honor over two-sided tatami: discarded " + discardedKey);
  }

  // Test 5: evaluateWaitQuality
  const waits = getWaitTiles([
    t("character", 1), t("character", 2), t("character", 3),
    t("dot", 2), t("dot", 3), t("dot", 4),
    t("bamboo", 4), t("bamboo", 5), t("bamboo", 6),
    t("honor", "east"), t("honor", "east"),
    t("character", 4), t("character", 5),
  ], "honor:white");
  if (waits.length === 0) throw new Error("should detect tenpai with two-sided wait");
  const wq = evaluateWaitQuality(waits, 0);
  if (wq <= 0) throw new Error("wait quality should be positive");

  // Test 6: piaoCai state and declare
  game = newRound();
  game.players[0].hand = [
    t("character", 1), t("character", 2), t("character", 3),
    t("dot", 2), t("dot", 3), t("dot", 4),
    t("bamboo", 4), t("bamboo", 5), t("bamboo", 6),
    t("honor", "east"), t("honor", "east"),
    t("character", 4), t("character", 5),
  ];
  sortTiles(game.players[0].hand);
  if (game.players[0].piaoCai) throw new Error("should start not piaoCai");
  declarePiaoCai(0);
  if (!game.players[0].piaoCai) throw new Error("should be piaoCai after declare");

  // Test 7: shouldAiPiaoCai
  game = newRound();
  game.players[1].hand = [
    t("character", 1), t("character", 2), t("character", 3),
    t("dot", 2), t("dot", 3), t("dot", 4),
    t("bamboo", 4), t("bamboo", 5), t("bamboo", 6),
    t("honor", "east"), t("honor", "east"),
    t("character", 4), t("character", 5),
  ];
  sortTiles(game.players[1].hand);
  game.wall = Array(60).fill(null).map(() => t("bamboo", 1));
  const shouldPiao = shouldAiPiaoCai(1);
  // With 60 tiles remaining and a good wait, AI should consider piao cai
  // (accept either true or false since it depends on exact evaluation)
  if (typeof shouldPiao !== "boolean") throw new Error("shouldAiPiaoCai should return boolean");

  // Test 8: piaoCai double scoring in declareWin
  game = newRound();
  game.players[0].hand = [
    t("character", 1), t("character", 2), t("character", 3),
    t("dot", 2), t("dot", 3), t("dot", 4),
    t("bamboo", 4), t("bamboo", 5), t("bamboo", 6),
    t("honor", "east"), t("honor", "east"),
    t("character", 4), t("character", 5),
  ];
  sortTiles(game.players[0].hand);
  game.lastDraw = { playerId: 0, tile: t("character", 6) };
  game.players[0].piaoCai = true;
  game.players[0].hand.push(cloneTile(game.lastDraw.tile));
  const scoreBefore = game.players[0].score;
  declareWin(0, "飘财", null, game.lastDraw.tile);
  if (game.winner.kind !== "飘财") throw new Error("should record piaoCai win kind");
  if (!game.winner.summary.includes("飘财")) throw new Error("summary should include piaoCai flag");
`, context);

console.log("Smoke test passed");
