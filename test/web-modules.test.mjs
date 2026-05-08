import assert from "node:assert/strict";
import { test } from "node:test";

import { createAudioPreference, createCanvasScreen, createPocketPogoRunner } from "../src-web/emulator-runner.js";
import { bindDirectionalPads, bindKeyboardControls, bindVirtualButtons } from "../src-web/input.js";
import {
  generateAdventureLevel,
  generateAdventureLevels,
  LEVEL_COUNT,
  TILE,
  tileIsDanger,
  tileIsMechanic
} from "../src-web/level-debug.js";
import { base64ToBuffer, bufferToBase64, createSaveStore } from "../src-web/save-store.js";
import {
  findOneTilePockets,
  isOneTilePocket,
  roomIsSolvable
} from "../src-web/reachability.js";
import tileTableJson from "../src-web/tile-table.json" with { type: "json" };
import tileArtJson from "../src-web/tile-art.json" with { type: "json" };

function memoryStorage(initial = {}) {
  const values = new Map(Object.entries(initial));
  return {
    getItem(key) {
      return values.has(key) ? values.get(key) : null;
    },
    setItem(key, value) {
      values.set(key, value);
    },
    removeItem(key) {
      values.delete(key);
    },
    dump() {
      return Object.fromEntries(values);
    }
  };
}

test("SaveStore round-trips SRAM and clears corrupt persisted data", () => {
  const bytes = Uint8Array.from([1, 2, 3, 250]);
  const encoded = bufferToBase64(bytes.buffer);
  assert.deepEqual([...new Uint8Array(base64ToBuffer(encoded))], [...bytes]);

  const storage = memoryStorage({ sram: encoded });
  const saveStore = createSaveStore({ storage, key: "sram", logger: { warn() {} } });
  const instance = {
    restored: null,
    setCartridgeSaveRam(buffer) {
      this.restored = [...new Uint8Array(buffer)];
    },
    getCartridgeSaveRam() {
      return Uint8Array.from([7, 8, 9]).buffer;
    },
    setOnWriteToCartridgeRam(callback) {
      this.callback = callback;
    }
  };

  assert.equal(saveStore.restore(instance), true);
  assert.deepEqual(instance.restored, [1, 2, 3, 250]);
  assert.equal(saveStore.persist(instance), true);
  assert.deepEqual([...new Uint8Array(base64ToBuffer(storage.getItem("sram")))], [7, 8, 9]);

  saveStore.bind(instance);
  instance.callback();
  assert.deepEqual([...new Uint8Array(base64ToBuffer(storage.getItem("sram")))], [7, 8, 9]);

  const corruptStorage = memoryStorage({ sram: "not valid base64%%%" });
  const corruptStore = createSaveStore({ storage: corruptStorage, key: "sram", logger: { warn() {} } });
  const throwingInstance = {
    setCartridgeSaveRam() {
      throw new Error("bad sram");
    }
  };
  assert.equal(corruptStore.restore(throwingInstance), false);
  assert.deepEqual(corruptStorage.dump(), {});
});

test("bindVirtualButtons maps pointer state to emulator input and can detach", () => {
  const events = new Map();
  const classList = new Set();
  const captured = new Set();
  const button = {
    dataset: { key: "a" },
    classList: {
      add(value) {
        classList.add(value);
      },
      remove(value) {
        classList.delete(value);
      }
    },
    addEventListener(type, listener) {
      events.set(type, listener);
    },
    removeEventListener(type, listener) {
      if (events.get(type) === listener) events.delete(type);
    },
    setPointerCapture(pointerId) {
      captured.add(pointerId);
    },
    hasPointerCapture(pointerId) {
      return captured.has(pointerId);
    },
    releasePointerCapture(pointerId) {
      captured.delete(pointerId);
    }
  };
  const input = { isPressingA: false };
  const unbind = bindVirtualButtons({ root: { querySelectorAll: () => [button] }, input });
  const event = { pointerId: 7, prevented: false, preventDefault() { this.prevented = true; } };

  events.get("pointerdown")(event);
  assert.equal(event.prevented, true);
  assert.equal(input.isPressingA, true);
  assert.equal(classList.has("active"), true);
  assert.equal(captured.has(7), true);

  events.get("pointerup")({ pointerId: 7, preventDefault() {} });
  assert.equal(input.isPressingA, false);
  assert.equal(classList.has("active"), false);
  assert.equal(captured.has(7), false);

  unbind();
  assert.equal(events.size, 0);
});

test("bindDirectionalPads supports press-and-slide direction control", () => {
  const events = new Map();
  const classStates = new Map();
  const buttonFor = (key) => ({
    classList: {
      add(value) {
        classStates.set(key, value);
      },
      remove() {
        classStates.delete(key);
      },
      toggle(value, enabled) {
        if (enabled) classStates.set(key, value);
        else classStates.delete(key);
      }
    }
  });
  const buttons = new Map(["up", "down", "left", "right"].map((key) => [key, buttonFor(key)]));
  const pad = {
    querySelector(selector) {
      const key = selector.match(/"(.+)"/)[1];
      return buttons.get(key);
    },
    getBoundingClientRect() {
      return { left: 10, top: 20, width: 100, height: 100 };
    },
    addEventListener(type, listener) {
      events.set(type, listener);
    },
    removeEventListener(type, listener) {
      if (events.get(type) === listener) events.delete(type);
    }
  };
  const input = {
    isPressingUp: false,
    isPressingDown: false,
    isPressingLeft: false,
    isPressingRight: false
  };
  const unbind = bindDirectionalPads({ root: { querySelectorAll: () => [pad] }, input });
  const event = (clientX, clientY) => ({
    clientX,
    clientY,
    prevented: false,
    preventDefault() {
      this.prevented = true;
    }
  });

  const hover = event(105, 70);
  events.get("pointermove")(hover);
  assert.equal(hover.prevented, false);
  assert.equal(input.isPressingRight, false);
  assert.equal(classStates.size, 0);

  const right = event(105, 70);
  events.get("pointerdown")(right);
  assert.equal(right.prevented, true);
  assert.equal(input.isPressingRight, true);
  assert.equal(classStates.has("right"), true);

  events.get("pointermove")(event(60, 115));
  assert.equal(input.isPressingRight, false);
  assert.equal(input.isPressingDown, true);
  assert.equal(classStates.has("right"), false);
  assert.equal(classStates.has("down"), true);

  events.get("pointerup")(event(60, 115));
  assert.equal(input.isPressingDown, false);
  assert.equal(classStates.size, 0);

  events.get("pointerdown")(event(15, 70));
  assert.equal(input.isPressingLeft, true);
  unbind();
  assert.equal(input.isPressingLeft, false);
  assert.equal(events.size, 0);
});

test("bindKeyboardControls maps keyboard state to emulator input and can detach", () => {
  const events = new Map();
  const root = {
    addEventListener(type, listener) {
      events.set(type, listener);
    },
    removeEventListener(type, listener) {
      if (events.get(type) === listener) events.delete(type);
    }
  };
  const input = {
    isPressingA: false,
    isPressingB: false,
    isPressingDown: false,
    isPressingLeft: false,
    isPressingSelect: false,
    isPressingStart: false
  };
  const unbind = bindKeyboardControls({ root, input });
  const event = (code) => ({
    code,
    prevented: false,
    preventDefault() {
      this.prevented = true;
    }
  });

  const pressA = event("KeyX");
  events.get("keydown")(pressA);
  assert.equal(pressA.prevented, true);
  assert.equal(input.isPressingA, true);

  events.get("keyup")(event("KeyX"));
  assert.equal(input.isPressingA, false);

  events.get("keydown")(event("x"));
  assert.equal(input.isPressingA, true);
  events.get("keyup")(event("x"));
  assert.equal(input.isPressingA, false);

  events.get("keydown")(event("KeyZ"));
  assert.equal(input.isPressingB, true);
  events.get("keyup")(event("KeyZ"));
  assert.equal(input.isPressingB, false);

  events.get("keydown")(event("Enter"));
  assert.equal(input.isPressingStart, true);

  events.get("keydown")(event("Space"));
  assert.equal(input.isPressingSelect, true);

  const ignored = event("KeyQ");
  events.get("keydown")(ignored);
  assert.equal(ignored.prevented, false);

  events.get("keydown")(event("ArrowLeft"));
  assert.equal(input.isPressingLeft, true);
  events.get("keydown")(event("ArrowDown"));
  assert.equal(input.isPressingDown, true);
  unbind();
  assert.equal(input.isPressingLeft, false);
  assert.equal(input.isPressingDown, false);
  assert.equal(input.isPressingSelect, false);
  assert.equal(input.isPressingStart, false);
  assert.equal(events.size, 0);
});

test("CanvasScreen remaps frames to a crisp Game Boy Pocket palette", () => {
  const calls = [];
  const canvas = {
    width: 2,
    height: 2,
    getContext() {
      return {
        imageSmoothingEnabled: true,
        createImageData(width, height) {
          return { width, height, data: new Uint8ClampedArray(width * height * 4) };
        },
        putImageData(imageData, x, y) {
          calls.push({ imageData, x, y, smoothing: this.imageSmoothingEnabled });
        }
      };
    }
  };
  const screen = createCanvasScreen(canvas);
  screen.render({
    data: Uint8ClampedArray.from([
      255, 255, 255, 255,
      170, 170, 170, 255,
      90, 90, 90, 255,
      0, 0, 0, 255
    ])
  });

  assert.equal(calls.length, 1);
  assert.equal(calls[0].smoothing, false);
  assert.deepEqual([...calls[0].imageData.data], [
    248, 248, 240, 255,
    184, 184, 176, 255,
    104, 104, 96, 255,
    24, 24, 22, 255
  ]);
});

test("Pocket Pogo runner owns emulator boot ordering behind one interface", async () => {
  const calls = [];
  const instance = {
    apu: {
      enableSound() {
        calls.push(["enableSound"]);
      }
    },
    input: {},
    loadGame(rom) {
      calls.push(["loadGame", rom.byteLength]);
    },
    onFrameFinished(callback) {
      calls.push(["onFrameFinished"]);
      this.frameCallback = callback;
    },
    setOnWriteToCartridgeRam(callback) {
      calls.push(["setOnWriteToCartridgeRam"]);
      this.saveCallback = callback;
    },
    run() {
      calls.push(["run"]);
    }
  };
  class FakeGameboy {
    constructor() {
      return instance;
    }
  }
  const statuses = [];
  const runner = createPocketPogoRunner({
    GameboyCtor: FakeGameboy,
    romUrl: "/rom.gb",
    saveStore: {
      restore(gameboy) { calls.push(["restore", gameboy === instance]); },
      bind(gameboy) { calls.push(["bindSave", gameboy === instance]); }
    },
    screen: { render(imageData) { calls.push(["render", imageData]); } },
    controlsRoot: { querySelectorAll: () => [] },
    status: { set textContent(value) { statuses.push(value); } },
    bindControls({ input, onInputStart }) {
      calls.push(["bindControls", input === instance.input, typeof onInputStart]);
      onInputStart();
      onInputStart();
      return () => {};
    },
    fetchRom: async (url) => ({
      ok: true,
      status: 200,
      arrayBuffer: async () => Uint8Array.from([1, 2, 3]).buffer,
      url
    }),
    now: () => 123
  });

  assert.equal(await runner.boot(), instance);
  assert.deepEqual(statuses, ["Loading ROM...", "Running"]);
  assert.deepEqual(calls, [
    ["loadGame", 3],
    ["restore", true],
    ["onFrameFinished"],
    ["bindSave", true],
    ["bindControls", true, "function"],
    ["enableSound"],
    ["run"]
  ]);
  instance.frameCallback("frame");
  assert.deepEqual(calls.at(-1), ["render", "frame"]);
});

test("AudioPreference persists muted state", () => {
  const storage = memoryStorage({ audio: "1" });
  const preference = createAudioPreference({ storage, key: "audio" });

  assert.equal(preference.muted, true);
  preference.setMuted(false);
  assert.equal(preference.muted, false);
  assert.equal(storage.getItem("audio"), "0");
  preference.setMuted(true);
  assert.equal(preference.muted, true);
  assert.equal(storage.getItem("audio"), "1");
});

test("Pocket Pogo runner suppresses muted unlocks and wires the sound toggle", async () => {
  const calls = [];
  const listeners = new Map();
  const attrs = {};
  const classes = new Set();
  let inputStart = null;
  const button = {
    textContent: "",
    classList: {
      toggle(name, enabled) {
        if (enabled) classes.add(name);
        else classes.delete(name);
      }
    },
    setAttribute(name, value) {
      attrs[name] = value;
    },
    addEventListener(type, listener) {
      listeners.set(type, listener);
    },
    removeEventListener(type, listener) {
      if (listeners.get(type) === listener) listeners.delete(type);
    }
  };
  const storage = memoryStorage({ audio: "1" });
  const instance = {
    apu: {
      enableSound() {
        calls.push(["enableSound"]);
      },
      disableSound() {
        calls.push(["disableSound"]);
      }
    },
    input: {},
    loadGame(rom) {
      calls.push(["loadGame", rom.byteLength]);
    },
    onFrameFinished() {
      calls.push(["onFrameFinished"]);
    },
    run() {
      calls.push(["run"]);
    }
  };
  class FakeGameboy {
    constructor() {
      return instance;
    }
  }

  const runner = createPocketPogoRunner({
    GameboyCtor: FakeGameboy,
    romUrl: "/rom.gb",
    saveStore: {
      restore(gameboy) { calls.push(["restore", gameboy === instance]); },
      bind(gameboy) { calls.push(["bindSave", gameboy === instance]); }
    },
    screen: { render() {} },
    controlsRoot: { querySelectorAll: () => [] },
    status: { set textContent(value) { calls.push(["status", value]); } },
    soundToggle: button,
    audioPreference: createAudioPreference({ storage, key: "audio" }),
    bindControls({ onInputStart }) {
      inputStart = onInputStart;
      calls.push(["bindControls"]);
      onInputStart();
      return () => {};
    },
    fetchRom: async () => ({
      ok: true,
      status: 200,
      arrayBuffer: async () => Uint8Array.from([1, 2, 3]).buffer
    }),
    now: () => 123
  });

  assert.equal(await runner.boot(), instance);
  assert.equal(button.textContent, "MUTE");
  assert.equal(attrs["aria-label"], "Enable sound");
  assert.equal(attrs["aria-pressed"], "false");
  assert.equal(classes.has("muted"), true);
  assert.equal(calls.some(([name]) => name === "enableSound"), false);

  listeners.get("click")();
  assert.equal(storage.getItem("audio"), "0");
  assert.equal(button.textContent, "SOUND");
  assert.equal(classes.has("muted"), false);
  assert.deepEqual(calls.at(-1), ["enableSound"]);

  inputStart();
  assert.equal(calls.filter(([name]) => name === "enableSound").length, 1);

  listeners.get("click")();
  assert.equal(storage.getItem("audio"), "1");
  assert.equal(button.textContent, "MUTE");
  assert.deepEqual(calls.at(-1), ["disableSound"]);

  inputStart();
  assert.equal(calls.filter(([name]) => name === "enableSound").length, 1);
});

/* Reachability checks live in src-web/reachability.js as a proper Module.
 * Tests below consume the Module's interface (roomIsSolvable, isOneTilePocket,
 * findOneTilePockets) rather than re-implementing the flood-fill here. */

test("debug level generator exposes all adventure rooms with goals", () => {
  const levels = generateAdventureLevels();
  assert.equal(levels.length, LEVEL_COUNT);

  for (const room of levels) {
    const flat = room.stage.flat();
    assert.ok(flat.includes(TILE.KEY), `level ${room.level + 1} should include a key`);
    assert.ok(flat.includes(TILE.EXIT), `level ${room.level + 1} should include an exit`);
    assert.equal(room.stage[2][0], TILE.WALL, `level ${room.level + 1} should render boundary walls separately`);
    assert.ok(
      flat.includes(TILE.SOLID) ||
        flat.includes(TILE.CRACK) ||
        flat.includes(TILE.TOGGLE) ||
        flat.includes(TILE.CONV_L) ||
        flat.includes(TILE.CONV_R) ||
        flat.includes(TILE.MOVING),
      `level ${room.level + 1} should include jumpable platforms`
    );
    assert.equal(room.stage[16][18], TILE.EXIT, `level ${room.level + 1} should keep the exit visible`);
    assert.notEqual(room.stage[16][15], TILE.SPIKE, `level ${room.level + 1} should keep spikes off the exit pocket`);
    assert.notEqual(room.stage[16][15], TILE.WATER, `level ${room.level + 1} should keep water off the exit pocket`);
    assert.notEqual(room.stage[16][17], TILE.SPIKE, `level ${room.level + 1} should keep spikes off the exit approach`);
    assert.notEqual(room.stage[16][17], TILE.WATER, `level ${room.level + 1} should keep water off the exit approach`);
  }
});

test("adventure rooms keep a completable route and no reachable dead-end pockets", () => {
  for (const room of generateAdventureLevels()) {
    const result = roomIsSolvable(room);
    assert.ok(result.solvable, `level ${room.level + 1} must be solvable: ${result.reason}`);

    const traps = findOneTilePockets(room);
    assert.equal(
      traps.length,
      0,
      `level ${room.level + 1} should not trap the player in a one-tile pocket: ${
        traps.map((t) => `(${t.x},${t.y},switch=${t.switchOn})`).join(", ")
      }`
    );
  }
});

test("reachability module rejects an obviously unsolvable room", () => {
  /* Synthetic room: walls everywhere, key floating in mid-air, exit pinned
   * on a different floor with no bouncing path between them. */
  const room = {
    level: 0,
    world: 0,
    local: 0,
    spawn: { x: 8, y: 16 },
    switchOn: true,
    moving: null,
    enemies: [],
    stage: Array.from({ length: 18 }, (_, y) =>
      Array.from({ length: 20 }, (_, x) => {
        if (y === 2 || y === 17) return TILE.WALL;
        if (x === 0 || x === 19) return TILE.WALL;
        if (y === 16 && x === 18) return TILE.EXIT;
        if (y === 5 && x === 10) return TILE.KEY;
        return TILE.EMPTY;
      })
    )
  };
  const result = roomIsSolvable(room);
  assert.equal(result.solvable, false, "isolated key/exit must be flagged unsolvable");
  assert.ok(result.reason && result.reason.length > 0, "unsolvable result must include a reason");
});

test("isOneTilePocket detects the trap shape it is named for", () => {
  const room = generateAdventureLevel(0);
  /* Construct a known pocket: empty cell sealed on three sides. */
  const stage = room.stage.map((row) => row.slice());
  stage[10][10] = TILE.EMPTY;
  stage[11][10] = TILE.SOLID;
  stage[10][9] = TILE.SOLID;
  stage[10][11] = TILE.SOLID;
  assert.ok(isOneTilePocket({ ...room, stage }, 10, 10, true));
  /* Remove a wall: no longer a pocket. */
  stage[10][9] = TILE.EMPTY;
  assert.equal(isOneTilePocket({ ...room, stage }, 10, 10, true), false);
});

test("non-tutorial adventure rooms use upper routes and world mechanics", () => {
  const levels = generateAdventureLevels();
  const interestingTiles = new Set([
    TILE.SOLID,
    TILE.CRACK,
    TILE.SPRING,
    TILE.SPIKE,
    TILE.COIN,
    TILE.KEY,
    TILE.EXIT,
    TILE.SWITCH,
    TILE.TOGGLE,
    TILE.FAN_L,
    TILE.FAN_R,
    TILE.CONV_L,
    TILE.CONV_R,
    TILE.WATER,
    TILE.BUBBLE,
    TILE.MOVING,
    TILE.ROCK,
    TILE.TOGGLE_OFF
  ]);
  const mechanicTotals = new Map();

  for (const room of levels) {
    if (room.world === 0 && room.local < 8) continue;

    let minY = Infinity;
    let maxY = -Infinity;
    let mechanicCount = 0;

    for (let y = 0; y < room.stage.length; y += 1) {
      for (let x = 0; x < room.stage[y].length; x += 1) {
        const tile = room.stage[y][x];
        if (interestingTiles.has(tile)) {
          minY = Math.min(minY, y);
          maxY = Math.max(maxY, y);
        }
        if (tileIsMechanic(tile)) mechanicCount += 1;
      }
    }

    assert.ok(minY <= 7, `level ${room.level + 1} should place route content in the upper half`);
    assert.ok(maxY - minY + 1 >= 10, `level ${room.level + 1} should span at least 10 rows`);

    if (!mechanicTotals.has(room.world)) mechanicTotals.set(room.world, []);
    mechanicTotals.get(room.world).push(mechanicCount);
  }

  const minimumAverageMechanics = new Map([
    [0, 4],
    [1, 7],
    [2, 3],
    [3, 12],
    [4, 10]
  ]);

  for (const [world, minimum] of minimumAverageMechanics) {
    const counts = mechanicTotals.get(world);
    const average = counts.reduce((sum, count) => sum + count, 0) / counts.length;
    assert.ok(average >= minimum, `world ${world + 1} should average at least ${minimum} mechanic tiles`);
  }
});

test("advanced adventure rooms add stomp rocks and upper patrol pressure", () => {
  const levels = generateAdventureLevels();

  for (const room of levels) {
    if (room.world === 0 && room.local < 8) continue;

    const rockCount = room.stage.flat().filter((tile) => tile === TILE.ROCK).length;
    if (room.local >= 4) {
      assert.ok(rockCount > 0, `level ${room.level + 1} should include stomp-breakable rocks`);
    }
    if (room.local >= 12) {
      assert.ok(room.enemies.length > 0, `level ${room.level + 1} should add late-room patrol pressure`);
    }
  }
});

function roomPressureScore(room) {
  let score = room.enemies.length * 3 + (room.moving ? 3 : 0);

  for (const tile of room.stage.flat()) {
    if (tileIsMechanic(tile)) score += 1;
    if (tileIsDanger(tile)) score += 1;
  }

  return score;
}

function roomTileDistance(a, b) {
  let distance = 0;

  for (let y = 2; y < a.stage.length; y += 1) {
    for (let x = 1; x < a.stage[y].length - 1; x += 1) {
      if (a.stage[y][x] !== b.stage[y][x]) distance += 1;
    }
  }

  return distance;
}

test("adventure room pressure ramps inside each world", () => {
  const levels = generateAdventureLevels();

  for (let world = 0; world < 5; world += 1) {
    const rooms = levels.filter((room) => room.world === world);
    const early = rooms.slice(0, 4).reduce((sum, room) => sum + roomPressureScore(room), 0) / 4;
    const late = rooms.slice(12, 16).reduce((sum, room) => sum + roomPressureScore(room), 0) / 4;

    assert.ok(late >= early + 5, `world ${world + 1} should end with meaningfully more pressure`);
  }
});

test("advancing adventure rooms do not recycle four-room templates", () => {
  const levels = generateAdventureLevels();

  for (let world = 0; world < 5; world += 1) {
    const rooms = levels.filter((room) => room.world === world);

    for (let local = 0; local < 12; local += 1) {
      assert.ok(
        roomTileDistance(rooms[local], rooms[local + 4]) >= 8,
        `world ${world + 1} levels ${local + 1} and ${local + 5} should have meaningfully different layouts`
      );
    }
  }
});

test("opening adventure rooms use distinct silhouettes", () => {
  const levels = generateAdventureLevels();
  const groups = [
    levels.slice(0, 8),
    levels.slice(16, 20),
    levels.slice(32, 36),
    levels.slice(48, 52),
    levels.slice(64, 68)
  ];

  for (const rooms of groups) {
    for (let a = 0; a < rooms.length; a += 1) {
      for (let b = a + 1; b < rooms.length; b += 1) {
        assert.ok(
          roomTileDistance(rooms[a], rooms[b]) >= 20,
          `levels ${rooms[a].level + 1} and ${rooms[b].level + 1} should not share the same early-room silhouette`
        );
      }
    }
  }
});

test("switch world rooms require turning routes on", () => {
  const levels = generateAdventureLevels().filter((room) => room.world === 1);

  for (const room of levels) {
    assert.equal(room.switchOn, false, `level ${room.level + 1} should start with toggle routes off`);
    assert.equal(room.stage[14][4], TILE.SWITCH, `level ${room.level + 1} should put the first switch on the starter lane`);
    assert.ok(room.stage.flat().includes(TILE.TOGGLE), `level ${room.level + 1} should route across toggle platforms`);
  }
});

test("mixed world combines switch routes with other mechanics", () => {
  const levels = generateAdventureLevels().filter((room) => room.world === 4);

  for (const room of levels) {
    const flat = room.stage.flat();
    assert.equal(room.switchOn, false, `level ${room.level + 1} should begin with mixed-world toggle routes off`);
    assert.equal(room.stage[14][4], TILE.SWITCH, `level ${room.level + 1} should expose a starter switch`);
    assert.ok(flat.includes(TILE.TOGGLE), `level ${room.level + 1} should use toggles as part of the route`);
    assert.ok(
      flat.includes(TILE.ROCK) ||
        flat.includes(TILE.BUBBLE) ||
        flat.includes(TILE.CONV_L) ||
        flat.includes(TILE.CONV_R) ||
        flat.includes(TILE.FAN_L) ||
        flat.includes(TILE.FAN_R),
      `level ${room.level + 1} should pair switching with another mechanic`
    );
  }
});

test("spring tutorial uses a lower catch ledge", () => {
  const room = generateAdventureLevel(4);
  assert.equal(room.stage[14][6], TILE.SPRING);
  assert.equal(room.stage[15][5], TILE.EMPTY);
  assert.equal(room.stage[7][10], TILE.SOLID);
  assert.equal(room.stage[11][8], TILE.SOLID);
  assert.equal(room.stage[11][12], TILE.SOLID);
  assert.equal(room.stage[10][11], TILE.KEY);
});

test("crack tutorial uses a low-bounce gate instead of a repeated climb", () => {
  const room = generateAdventureLevel(1);
  assert.equal(room.stage[13][9], TILE.KEY);
  assert.equal(room.stage[14][9], TILE.SOLID);
  assert.equal(room.stage[10][9], TILE.CRACK);
  assert.equal(room.stage[13][5], TILE.EMPTY);
  assert.equal(room.stage[13][11], TILE.EMPTY);
});

test("tile vocabulary stays canonical across enum, table, and art", () => {
  /* The Tile vocabulary lives in src-rom/game-logic.h's TILE_LIST macro;
   * tile-table.json (from dump-levels) and tile-art.json (from dump-tiles)
   * are derived from it. This test guards the invariant that all three
   * agree, so callers can trust TILE id <-> name <-> pixel grid. */
  assert.equal(tileTableJson.tiles.length, tileTableJson.count, "tile-table count must match tiles[]");
  assert.equal(tileArtJson.count, tileTableJson.count, "tile-art and tile-table must declare the same tile count");
  assert.equal(tileArtJson.tiles.length, tileTableJson.count, "every tile id must have a pixel grid");

  for (const row of tileTableJson.tiles) {
    assert.equal(typeof row.name, "string", `tile id ${row.id} must have a name`);
    assert.match(row.name, /^[A-Z][A-Z0-9_]*$/, `tile name ${row.name} must be SHOUTING_SNAKE_CASE`);
    assert.equal(TILE[row.name], row.id, `TILE.${row.name} must equal id ${row.id}`);
  }

  for (const tile of tileArtJson.tiles) {
    assert.equal(tile.pixels.length, 8, `tile ${tile.index} must have 8 rows`);
    for (const row of tile.pixels) {
      assert.equal(row.length, 8, `tile ${tile.index} row must have 8 cols`);
      for (const v of row) assert.ok(v >= 0 && v <= 3, `tile ${tile.index} pixel must be GB color 0-3`);
    }
  }
});

test("ROCK is treated as a platform by both flag and behavior", () => {
  /* Pre-deepening, clear_if_platform hardcoded ROCK while tile_is_platform
   * (TILEF_PLATFORM) didn't include it. This locks in the unification. */
  const rock = tileTableJson.tiles.find((t) => t.name === "ROCK");
  assert.ok(rock.platform, "ROCK must carry the platform flag");
  assert.ok(rock.solid, "ROCK must remain solid");
  assert.ok(rock.mechanic, "ROCK must remain a mechanic tile");
});
