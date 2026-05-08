import assert from "node:assert/strict";
import { test } from "node:test";

import { createCanvasScreen, createPocketPogoRunner } from "../src-web/emulator-runner.js";
import { bindDirectionalPads, bindKeyboardControls, bindVirtualButtons } from "../src-web/input.js";
import { generateAdventureLevel, generateAdventureLevels, LEVEL_COUNT, TILE } from "../src-web/level-debug.js";
import { base64ToBuffer, bufferToBase64, createSaveStore } from "../src-web/save-store.js";

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

  const pressA = event("KeyA");
  events.get("keydown")(pressA);
  assert.equal(pressA.prevented, true);
  assert.equal(input.isPressingA, true);

  events.get("keyup")(event("KeyA"));
  assert.equal(input.isPressingA, false);

  events.get("keydown")(event("a"));
  assert.equal(input.isPressingA, true);
  events.get("keyup")(event("a"));
  assert.equal(input.isPressingA, false);

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

test("debug level generator exposes all adventure rooms with goals", () => {
  const levels = generateAdventureLevels();
  assert.equal(levels.length, LEVEL_COUNT);

  for (const room of levels) {
    const flat = room.stage.flat();
    assert.ok(flat.includes(TILE.BATTERY), `level ${room.level + 1} should include a battery`);
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

test("spring tutorial keeps the spring beside the starter platform", () => {
  const room = generateAdventureLevel(4);
  assert.equal(room.stage[14][6], TILE.SPRING);
  assert.equal(room.stage[15][5], TILE.EMPTY);
});
