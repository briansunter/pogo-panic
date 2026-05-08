import assert from "node:assert/strict";
import { test } from "node:test";

import { createCanvasScreen, createPocketPogoRunner } from "../src-web/emulator-runner.js";
import { bindDirectionalPads, bindKeyboardControls, bindVirtualButtons } from "../src-web/input.js";
import { generateAdventureLevel, generateAdventureLevels, LEVEL_COUNT, TILE, TILE_PX } from "../src-web/level-debug.js";
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

const AUDIT_SOLIDS = new Set([
  TILE.SOLID,
  TILE.CRACK,
  TILE.SPRING,
  TILE.SWITCH,
  TILE.CONV_L,
  TILE.CONV_R,
  TILE.MOVING,
  TILE.ROCK,
  TILE.WALL
]);

function auditSolid(tile, switchOn) {
  if (tile === TILE.TOGGLE) return switchOn;
  return AUDIT_SOLIDS.has(tile);
}

function auditDanger(tile) {
  return tile === TILE.SPIKE || tile === TILE.WATER;
}

function auditOneTilePocket(room, x, y, switchOn) {
  const tile = room.stage[y][x];
  if (auditDanger(tile) || auditSolid(tile, switchOn)) return false;
  if (tile === TILE.EXIT) return false;
  return auditSolid(room.stage[y + 1][x], switchOn) &&
    auditSolid(room.stage[y][x - 1], switchOn) &&
    auditSolid(room.stage[y][x + 1], switchOn);
}

function auditSurface(room, x, supportY, switchOn) {
  if (x <= 0 || x >= 19 || supportY <= 2 || supportY > 17) return false;
  if (!auditSolid(room.stage[supportY][x], switchOn)) return false;
  const headTile = room.stage[supportY - 1][x];
  return !auditSolid(headTile, switchOn) && !auditDanger(headTile);
}

function auditKey(node) {
  return `${node.x},${node.y},${node.switchOn ? 1 : 0}`;
}

function auditNode(key) {
  const [x, y, switchFlag] = key.split(",").map(Number);
  return { x, y, switchOn: switchFlag === 1 };
}

function auditLand(room, x, y, switchOn) {
  const tile = room.stage[y][x];
  return { x, y, switchOn: tile === TILE.SWITCH ? !switchOn : switchOn };
}

function auditStartNodes(room) {
  const spawnX = Math.floor(room.spawn.x / TILE_PX);
  const spawnFootY = Math.floor((room.spawn.y + TILE_PX) / TILE_PX);
  const nodes = [];

  for (let dy = 0; dy <= 7; dy += 1) {
    for (let dx = -2; dx <= 2; dx += 1) {
      const x = spawnX + dx;
      const y = spawnFootY + dy;
      if (auditSurface(room, x, y, room.switchOn)) nodes.push(auditLand(room, x, y, room.switchOn));
    }
    if (nodes.length) return nodes;
  }

  return nodes;
}

function auditTargetNodes(room, targetTile) {
  const candidates = [];

  for (let y = 3; y < room.stage.length - 1; y += 1) {
    for (let x = 1; x < room.stage[y].length - 1; x += 1) {
      if (room.stage[y][x] !== targetTile) continue;

      for (const switchOn of [false, true]) {
        for (let sx = x - 1; sx <= x + 1; sx += 1) {
          for (let sy = y + 1; sy <= Math.min(17, y + 4); sy += 1) {
            if (auditSurface(room, sx, sy, switchOn)) candidates.push(auditKey(auditLand(room, sx, sy, switchOn)));
          }
        }
      }
    }
  }

  return new Set(candidates);
}

function auditCanMove(room, from, to) {
  const dx = Math.abs(to.x - from.x);
  const rise = from.y - to.y;
  const fall = to.y - from.y;
  const fromTile = room.stage[from.y][from.x];
  const spring = fromTile === TILE.SPRING;
  const maxRise = spring ? 7 : 4;
  const maxDx = spring ? 8 : 7;

  if (dx === 0 && rise === 0) return false;
  if (rise > 0 && spring) return rise <= maxRise && dx <= maxDx;
  if (rise > 0) return rise <= maxRise && dx <= Math.max(3, maxDx - Math.max(0, rise - 2));
  if (fall >= 0) return dx <= maxDx + 2;
  return false;
}

function auditNeighbors(room, node) {
  const neighbors = [];

  for (let y = 3; y <= 17; y += 1) {
    for (let x = 1; x < 19; x += 1) {
      if (!auditSurface(room, x, y, node.switchOn)) continue;
      if (auditCanMove(room, node, { x, y })) neighbors.push(auditLand(room, x, y, node.switchOn));
    }
  }

  return neighbors;
}

function auditFlood(room, starts) {
  const queue = [...starts];
  const seen = new Set(queue.map(auditKey));

  for (let index = 0; index < queue.length; index += 1) {
    for (const next of auditNeighbors(room, queue[index])) {
      const key = auditKey(next);
      if (!seen.has(key)) {
        seen.add(key);
        queue.push(next);
      }
    }
  }

  return seen;
}

function auditIntersects(seen, targets) {
  for (const key of targets) {
    if (seen.has(key)) return true;
  }
  return false;
}

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
    const starts = auditStartNodes(room);
    const keyTargets = auditTargetNodes(room, TILE.KEY);
    const exitTargets = auditTargetNodes(room, TILE.EXIT);

    assert.ok(starts.length > 0, `level ${room.level + 1} should have a reachable landing below spawn`);
    assert.ok(keyTargets.size > 0, `level ${room.level + 1} should expose key approach surfaces`);
    assert.ok(exitTargets.size > 0, `level ${room.level + 1} should expose exit approach surfaces`);

    const fromStart = auditFlood(room, starts);
    const keyStarts = [...keyTargets].filter((key) => fromStart.has(key)).map(auditNode);

    assert.ok(keyStarts.length > 0, `level ${room.level + 1} key should be route-reachable`);
    assert.ok(
      auditIntersects(auditFlood(room, keyStarts), exitTargets),
      `level ${room.level + 1} exit should be reachable after the key route`
    );

    for (const key of fromStart) {
      assert.ok(
        auditIntersects(auditFlood(room, [auditNode(key)]), exitTargets),
        `level ${room.level + 1} should let reachable surface ${key} escape to the exit`
      );
    }

    for (const switchOn of [false, true]) {
      for (let y = 3; y < 17; y += 1) {
        for (let x = 1; x < 19; x += 1) {
          assert.equal(
            auditOneTilePocket(room, x, y, switchOn),
            false,
            `level ${room.level + 1} should not trap the player in a one-tile pocket at ${x},${y}`
          );
        }
      }
    }
  }
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
  const mechanicTiles = new Set([
    TILE.SPRING,
    TILE.SWITCH,
    TILE.TOGGLE,
    TILE.FAN_L,
    TILE.FAN_R,
    TILE.CONV_L,
    TILE.CONV_R,
    TILE.MOVING,
    TILE.CRACK,
    TILE.BUBBLE,
    TILE.ROCK
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
        if (mechanicTiles.has(tile)) mechanicCount += 1;
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

test("spring tutorial keeps the spring beside the starter platform", () => {
  const room = generateAdventureLevel(4);
  assert.equal(room.stage[14][6], TILE.SPRING);
  assert.equal(room.stage[15][5], TILE.EMPTY);
});

test("crack tutorial keeps the key route open instead of caged", () => {
  const room = generateAdventureLevel(1);
  assert.equal(room.stage[11][9], TILE.KEY);
  assert.equal(room.stage[13][5], TILE.EMPTY);
  assert.equal(room.stage[13][13], TILE.EMPTY);
});
