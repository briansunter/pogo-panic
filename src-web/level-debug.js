export const LEVEL_COUNT = 80;
export const SCREEN_TILES_W = 20;
export const SCREEN_TILES_H = 18;
export const TILE_PX = 8;

export const TILE = Object.freeze({
  EMPTY: 0,
  SOLID: 1,
  CRACK: 2,
  SPRING: 3,
  SPIKE: 4,
  COIN: 5,
  BATTERY: 6,
  EXIT: 7,
  SWITCH: 8,
  TOGGLE: 9,
  FAN_L: 10,
  FAN_R: 11,
  CONV_L: 12,
  CONV_R: 13,
  WATER: 14,
  BUBBLE: 15,
  MOVING: 16,
  ROCK: 17,
  TOGGLE_OFF: 18,
  ARROW_D: 19,
  ARROW_R: 20,
  WALL: 21
});

const CEILING_Y = 2;
const WORLD_NAMES = ["Groove", "Switch", "Drift", "Motion", "Gauntlet"];
const PALETTE = {
  white: "#f8f8f8",
  light: "#d8d8d8",
  mid: "#8a8a8a",
  dark: "#4b4b4b",
  ink: "#111111"
};

const TILE_NAMES = new Map([
  [TILE.EMPTY, "empty"],
  [TILE.SOLID, "solid"],
  [TILE.CRACK, "crack"],
  [TILE.SPRING, "spring"],
  [TILE.SPIKE, "spike"],
  [TILE.COIN, "coin"],
  [TILE.BATTERY, "battery"],
  [TILE.EXIT, "exit"],
  [TILE.SWITCH, "switch"],
  [TILE.TOGGLE, "toggle"],
  [TILE.FAN_L, "fan-left"],
  [TILE.FAN_R, "fan-right"],
  [TILE.CONV_L, "conveyor-left"],
  [TILE.CONV_R, "conveyor-right"],
  [TILE.WATER, "water"],
  [TILE.BUBBLE, "bubble"],
  [TILE.MOVING, "moving"],
  [TILE.ROCK, "rock"],
  [TILE.TOGGLE_OFF, "toggle-off"],
  [TILE.ARROW_D, "arrow-down"],
  [TILE.ARROW_R, "arrow-right"],
  [TILE.WALL, "wall"]
]);

function emptyStage() {
  return Array.from({ length: SCREEN_TILES_H }, () => Array(SCREEN_TILES_W).fill(TILE.EMPTY));
}

function resetCommon(room) {
  room.stage = emptyStage();
  for (let x = 0; x < SCREEN_TILES_W; x += 1) {
    room.stage[CEILING_Y][x] = TILE.WALL;
    room.stage[17][x] = TILE.WALL;
  }
  for (let y = CEILING_Y; y < SCREEN_TILES_H; y += 1) {
    room.stage[y][0] = TILE.WALL;
    room.stage[y][19] = TILE.WALL;
  }
  room.enemies = [];
  room.moving = null;
  room.switchOn = true;
  room.spawn = { x: 18, y: 80 };
}

function beginRoom(world) {
  const room = { stage: emptyStage(), world, local: 0, enemies: [], moving: null, switchOn: true };
  resetCommon(room);
  placeExit(room, 18, 16);
  return room;
}

function setRoomSwitch(room, on) {
  room.switchOn = Boolean(on);
}

function addPlatform(room, x, y, w, tile) {
  for (let i = 0; i < w; i += 1) {
    const tx = x + i;
    if (tx < SCREEN_TILES_W && y < SCREEN_TILES_H) room.stage[y][tx] = tile;
  }
}

function addColumn(room, x, y, h, tile) {
  for (let i = 0; i < h; i += 1) {
    const ty = y + i;
    if (ty < SCREEN_TILES_H && x < SCREEN_TILES_W) room.stage[ty][x] = tile;
  }
}

function addCoinLine(room, x, y, w) {
  for (let i = 0; i < w; i += 1) {
    const tx = x + i;
    if (tx < SCREEN_TILES_W && y < SCREEN_TILES_H && room.stage[y][tx] === TILE.EMPTY) {
      room.stage[y][tx] = TILE.COIN;
    }
  }
}

function addHazardLine(room, x, y, w, tile) {
  for (let i = 0; i < w; i += 1) {
    const tx = x + i;
    if (tx < SCREEN_TILES_W && y < SCREEN_TILES_H && room.stage[y][tx] === TILE.EMPTY) {
      room.stage[y][tx] = tile;
    }
  }
}

function addEnemy(room, tx, ty, vx) {
  room.enemies.push({ tx, ty, vx });
}

function addMovingPlatform(room, x, y, w, dir) {
  room.moving = { x, y, w, dir };
  addPlatform(room, x, y, w, TILE.MOVING);
}

function setSpawnPx(room, x, y) {
  room.spawn = { x, y };
}

function placeMarkerDown(room, x, y) {
  if (x >= SCREEN_TILES_W || y <= CEILING_Y + 1) return;
  const markerY = y - 1;
  if (room.stage[markerY][x] === TILE.EMPTY) room.stage[markerY][x] = TILE.ARROW_D;
}

function placeExit(room, x, y) {
  if (x >= SCREEN_TILES_W || y >= SCREEN_TILES_H) return;
  room.stage[y][x] = TILE.EXIT;
  placeMarkerDown(room, x, y);
}

function placeBattery(room, x, y) {
  if (x >= SCREEN_TILES_W || y >= SCREEN_TILES_H) return;
  room.stage[y][x] = TILE.BATTERY;
  placeMarkerDown(room, x, y);
}

function tileIsDanger(tile) {
  return tile === TILE.SPIKE || tile === TILE.WATER;
}

function clearIfDanger(room, x, y) {
  if (tileIsDanger(room.stage[y][x])) room.stage[y][x] = TILE.EMPTY;
}

function softenExit(room) {
  clearIfDanger(room, 15, 16);
  clearIfDanger(room, 16, 16);
  clearIfDanger(room, 17, 16);
  clearIfDanger(room, 16, 15);
  clearIfDanger(room, 17, 15);
  clearIfDanger(room, 18, 15);
  placeMarkerDown(room, 18, 16);
}

function buildTutorialRoom(room, local) {
  if (local === 0) {
    addPlatform(room, 2, 14, 5, TILE.SOLID);
    addPlatform(room, 8, 12, 4, TILE.SOLID);
    addPlatform(room, 13, 10, 5, TILE.SOLID);
    placeBattery(room, 4, 13);
    addCoinLine(room, 8, 11, 4);
    room.stage[13][7] = TILE.ARROW_R;
  } else if (local === 1) {
    addPlatform(room, 2, 14, 4, TILE.SOLID);
    addColumn(room, 5, 12, 5, TILE.SOLID);
    addColumn(room, 13, 12, 5, TILE.SOLID);
    addPlatform(room, 6, 12, 7, TILE.CRACK);
    placeBattery(room, 9, 15);
    addCoinLine(room, 7, 10, 5);
    room.stage[11][9] = TILE.ARROW_D;
  } else if (local === 2) {
    addPlatform(room, 2, 13, 6, TILE.SOLID);
    addPlatform(room, 11, 11, 5, TILE.SOLID);
    placeBattery(room, 3, 12);
    addCoinLine(room, 12, 10, 3);
    room.stage[16][17] = TILE.ARROW_R;
  } else if (local === 3) {
    addPlatform(room, 2, 14, 5, TILE.SOLID);
    addPlatform(room, 12, 14, 5, TILE.SOLID);
    addPlatform(room, 7, 10, 4, TILE.SOLID);
    addHazardLine(room, 7, 16, 5, TILE.SPIKE);
    placeBattery(room, 8, 9);
    addCoinLine(room, 13, 13, 3);
  } else if (local === 4) {
    addPlatform(room, 2, 14, 4, TILE.SOLID);
    room.stage[14][6] = TILE.SPRING;
    addPlatform(room, 11, 7, 5, TILE.SOLID);
    placeBattery(room, 14, 6);
    addCoinLine(room, 12, 8, 3);
  } else if (local === 5) {
    addPlatform(room, 3, 13, 5, TILE.CRACK);
    addPlatform(room, 10, 11, 5, TILE.CRACK);
    addPlatform(room, 4, 16, 4, TILE.SOLID);
    placeBattery(room, 12, 10);
    addCoinLine(room, 4, 12, 3);
    room.stage[12][12] = TILE.ARROW_D;
  } else if (local === 6) {
    addPlatform(room, 2, 14, 5, TILE.SOLID);
    addPlatform(room, 9, 12, 4, TILE.SOLID);
    placeBattery(room, 14, 15);
    addCoinLine(room, 10, 11, 3);
    addEnemy(room, 9, 16, 1);
  } else {
    setRoomSwitch(room, false);
    addPlatform(room, 2, 14, 4, TILE.SOLID);
    room.stage[15][4] = TILE.SWITCH;
    addPlatform(room, 8, 12, 6, TILE.TOGGLE);
    placeBattery(room, 13, 11);
    addCoinLine(room, 9, 11, 3);
  }
}

function buildIntroRoom(room, local) {
  const route = local & 7;
  if (route === 0) {
    addPlatform(room, 2, 14, 5, TILE.SOLID);
    addPlatform(room, 9, 12, 4, TILE.SOLID);
    addPlatform(room, 14, 9, 4, TILE.SOLID);
    addHazardLine(room, 7, 16, 3, TILE.SPIKE);
    placeBattery(room, 15, 8);
    addCoinLine(room, 9, 11, 3);
  } else if (route === 1) {
    addPlatform(room, 2, 15, 4, TILE.SOLID);
    room.stage[15][7] = TILE.SPRING;
    addPlatform(room, 11, 8, 5, TILE.SOLID);
    addHazardLine(room, 9, 16, 4, TILE.SPIKE);
    placeBattery(room, 13, 7);
  } else if (route === 2) {
    addPlatform(room, 2, 13, 5, TILE.SOLID);
    addPlatform(room, 8, 10, 5, TILE.CRACK);
    addPlatform(room, 13, 13, 4, TILE.SOLID);
    placeBattery(room, 10, 9);
    addCoinLine(room, 13, 12, 3);
  } else if (route === 3) {
    addPlatform(room, 2, 14, 5, TILE.SOLID);
    addPlatform(room, 8, 11, 4, TILE.SOLID);
    addPlatform(room, 13, 14, 4, TILE.SOLID);
    placeBattery(room, 9, 10);
    addEnemy(room, 12, 16, -1);
  } else if (route === 4) {
    setRoomSwitch(room, false);
    room.stage[15][4] = TILE.SWITCH;
    addPlatform(room, 2, 14, 4, TILE.SOLID);
    addPlatform(room, 7, 11, 4, TILE.TOGGLE);
    addPlatform(room, 13, 9, 4, TILE.SOLID);
    placeBattery(room, 15, 8);
  } else if (route === 5) {
    addPlatform(room, 2, 15, 4, TILE.CONV_R);
    addPlatform(room, 8, 12, 4, TILE.SOLID);
    addPlatform(room, 13, 10, 4, TILE.SOLID);
    room.stage[13][6] = TILE.SPIKE;
    placeBattery(room, 14, 9);
    addCoinLine(room, 8, 11, 4);
  } else if (route === 6) {
    addPlatform(room, 2, 14, 5, TILE.SOLID);
    addPlatform(room, 9, 12, 5, TILE.CRACK);
    room.stage[15][15] = TILE.SPRING;
    placeBattery(room, 11, 11);
    addHazardLine(room, 7, 16, 3, TILE.SPIKE);
  } else {
    addPlatform(room, 2, 14, 5, TILE.SOLID);
    addPlatform(room, 8, 10, 4, TILE.SOLID);
    addPlatform(room, 13, 13, 4, TILE.SOLID);
    room.stage[9][14] = TILE.BUBBLE;
    addHazardLine(room, 9, 16, 6, TILE.WATER);
    placeBattery(room, 9, 9);
    addEnemy(room, 14, 16, -1);
  }
}

function buildSwitchRoom(room, local) {
  const route = local & 3;
  setRoomSwitch(room, (local & 1) === 0);
  addPlatform(room, 2, 14, 4, TILE.SOLID);
  room.stage[(local & 4) ? 13 : 15][4] = TILE.SWITCH;
  if (route === 0) {
    addPlatform(room, 7, 12, 6, TILE.TOGGLE);
    addPlatform(room, 14, 9, 4, TILE.SOLID);
    placeBattery(room, 15, 8);
    addCoinLine(room, 8, 11, 4);
  } else if (route === 1) {
    addPlatform(room, 7, 10, 4, TILE.SOLID);
    addPlatform(room, 12, 13, 5, TILE.TOGGLE);
    addHazardLine(room, 8, 16, 5, TILE.SPIKE);
    placeBattery(room, 8, 9);
  } else if (route === 2) {
    addPlatform(room, 6, 13, 4, TILE.TOGGLE);
    addPlatform(room, 12, 10, 4, TILE.TOGGLE);
    room.stage[15][15] = TILE.SPRING;
    placeBattery(room, 13, 9);
    addCoinLine(room, 6, 12, 3);
  } else {
    addPlatform(room, 6, 11, 5, TILE.SOLID);
    addPlatform(room, 12, 8, 4, TILE.TOGGLE);
    addHazardLine(room, 7, 16, 4, TILE.SPIKE);
    placeBattery(room, 13, 7);
    addEnemy(room, 11, 16, 1);
  }
  if (local >= 8) addEnemy(room, 8 + (local & 3), 16, (local & 1) ? -1 : 1);
}

function buildWaterRoom(room, local) {
  const route = local & 3;
  addPlatform(room, 2, 14, 4, TILE.SOLID);
  if (route === 0) {
    addHazardLine(room, 6, 16, 8, TILE.WATER);
    addPlatform(room, 8, 12, 4, TILE.SOLID);
    room.stage[12][3] = TILE.BUBBLE;
    placeBattery(room, 9, 11);
    addCoinLine(room, 11, 11, 3);
  } else if (route === 1) {
    addHazardLine(room, 5, 16, 5, TILE.WATER);
    addHazardLine(room, 13, 16, 3, TILE.WATER);
    room.stage[10][5] = TILE.FAN_R;
    addPlatform(room, 10, 11, 4, TILE.SOLID);
    placeBattery(room, 12, 10);
  } else if (route === 2) {
    addHazardLine(room, 6, 16, 9, TILE.WATER);
    room.stage[13][4] = TILE.BUBBLE;
    room.stage[9][15] = TILE.FAN_L;
    addPlatform(room, 8, 9, 4, TILE.SOLID);
    placeBattery(room, 9, 8);
    addEnemy(room, 13, 16, -1);
  } else {
    addHazardLine(room, 4, 16, 4, TILE.WATER);
    addHazardLine(room, 11, 16, 5, TILE.WATER);
    room.stage[15][6] = TILE.SPRING;
    room.stage[8][14] = (local & 4) ? TILE.FAN_L : TILE.FAN_R;
    addPlatform(room, 10, 8, 5, TILE.SOLID);
    room.stage[12][3] = TILE.BUBBLE;
    placeBattery(room, 12, 7);
  }
  if (local >= 8) addHazardLine(room, 7 + (local & 3), 15, 2, TILE.SPIKE);
}

function buildMotionRoom(room, local) {
  const route = local & 3;
  addPlatform(room, 2, 14, 4, (local & 1) ? TILE.CONV_R : TILE.CONV_L);
  if (route === 0) {
    addPlatform(room, 8, 12, 5, TILE.CONV_R);
    addPlatform(room, 14, 9, 4, TILE.SOLID);
    placeBattery(room, 15, 8);
  } else if (route === 1) {
    addPlatform(room, 7, 10, 4, TILE.CONV_L);
    addPlatform(room, 12, 13, 5, TILE.CONV_R);
    addHazardLine(room, 6, 16, 4, TILE.SPIKE);
    placeBattery(room, 8, 9);
  } else if (route === 2) {
    addPlatform(room, 5, 12, 4, TILE.SOLID);
    addPlatform(room, 13, 8, 4, TILE.CONV_L);
    room.stage[14][10] = TILE.FAN_R;
    placeBattery(room, 14, 7);
    addCoinLine(room, 5, 11, 3);
  } else {
    addPlatform(room, 4, 11, 3, TILE.CRACK);
    addPlatform(room, 12, 10, 4, TILE.CONV_R);
    room.stage[13][15] = TILE.SPRING;
    addHazardLine(room, 8, 16, 4, TILE.SPIKE);
    placeBattery(room, 13, 9);
  }
  if (local >= 4) {
    addMovingPlatform(room, 6 + (local & 3), (local & 4) ? 7 : 8, 3, (local & 1) ? 1 : -1);
  }
  if (local >= 10) addEnemy(room, 10 + (local & 3), 16, (local & 1) ? -1 : 1);
}

function buildMixedRoom(room, local) {
  const route = local & 7;
  setRoomSwitch(room, (local & 1) === 0);
  addPlatform(room, 2, 14, 4, TILE.SOLID);
  if (route === 0) {
    room.stage[15][4] = TILE.SWITCH;
    addPlatform(room, 7, 12, 4, TILE.TOGGLE);
    addPlatform(room, 12, 9, 4, TILE.CRACK);
    addHazardLine(room, 8, 16, 5, TILE.SPIKE);
    placeBattery(room, 13, 8);
  } else if (route === 1) {
    addHazardLine(room, 5, 16, 8, TILE.WATER);
    room.stage[12][3] = TILE.BUBBLE;
    room.stage[10][14] = TILE.FAN_L;
    addPlatform(room, 9, 10, 4, TILE.CONV_R);
    placeBattery(room, 10, 9);
    addEnemy(room, 14, 16, -1);
  } else if (route === 2) {
    room.stage[14][6] = TILE.SPRING;
    addPlatform(room, 9, 9, 5, TILE.TOGGLE);
    room.stage[13][4] = TILE.SWITCH;
    addHazardLine(room, 7, 16, 4, TILE.SPIKE);
    placeBattery(room, 11, 8);
  } else if (route === 3) {
    addPlatform(room, 5, 12, 4, TILE.CRACK);
    addPlatform(room, 12, 10, 5, TILE.CONV_L);
    addHazardLine(room, 7, 16, 3, TILE.WATER);
    placeBattery(room, 14, 9);
    addEnemy(room, 9, 16, 1);
  } else if (route === 4) {
    addHazardLine(room, 6, 16, 6, TILE.SPIKE);
    room.stage[13][3] = TILE.BUBBLE;
    addMovingPlatform(room, 8, 10, 3, 1);
    placeBattery(room, 9, 9);
  } else if (route === 5) {
    addPlatform(room, 7, 13, 4, TILE.CONV_R);
    room.stage[11][15] = TILE.FAN_L;
    addPlatform(room, 12, 8, 4, TILE.CRACK);
    addHazardLine(room, 5, 16, 4, TILE.WATER);
    placeBattery(room, 14, 7);
    addEnemy(room, 13, 16, -1);
  } else if (route === 6) {
    room.stage[15][4] = TILE.SWITCH;
    addPlatform(room, 7, 12, 3, TILE.TOGGLE);
    addPlatform(room, 11, 9, 4, TILE.TOGGLE);
    room.stage[15][14] = TILE.SPRING;
    addHazardLine(room, 7, 16, 5, TILE.WATER);
    placeBattery(room, 12, 8);
  } else {
    addPlatform(room, 6, 12, 4, TILE.CRACK);
    addPlatform(room, 12, 11, 5, TILE.CONV_R);
    addHazardLine(room, 5, 16, 4, TILE.SPIKE);
    addHazardLine(room, 12, 16, 4, TILE.WATER);
    room.stage[10][4] = TILE.BUBBLE;
    placeBattery(room, 13, 10);
    addEnemy(room, 8, 16, 1);
    addEnemy(room, 14, 16, -1);
  }
}

export function generateAdventureLevel(level) {
  const safeLevel = Math.max(0, Math.min(LEVEL_COUNT - 1, Number(level) || 0));
  const world = safeLevel >> 4;
  const local = safeLevel & 15;
  const room = beginRoom(world);
  room.level = safeLevel;
  room.local = local;

  if (world === 0 && local < 8) buildTutorialRoom(room, local);
  else if (world === 0) buildIntroRoom(room, local);
  else if (world === 1) buildSwitchRoom(room, local);
  else if (world === 2) buildWaterRoom(room, local);
  else if (world === 3) buildMotionRoom(room, local);
  else buildMixedRoom(room, local);

  softenExit(room);
  return room;
}

export function generateAdventureLevels() {
  return Array.from({ length: LEVEL_COUNT }, (_, index) => generateAdventureLevel(index));
}

function fillBase(ctx, x, y) {
  ctx.fillStyle = PALETTE.white;
  ctx.fillRect(x, y, TILE_PX, TILE_PX);
}

function drawPixelPattern(ctx, x, y, rows, color = PALETTE.ink) {
  ctx.fillStyle = color;
  for (let py = 0; py < rows.length; py += 1) {
    const row = rows[py];
    for (let px = 0; px < row.length; px += 1) {
      if (row[px] !== " ") ctx.fillRect(x + px, y + py, 1, 1);
    }
  }
}

function drawPlatformTile(ctx, x, y, { off = false, moving = false, conveyor = 0 } = {}) {
  const body = off ? PALETTE.light : moving ? PALETTE.dark : PALETTE.mid;
  const shade = off ? PALETTE.mid : PALETTE.ink;
  ctx.fillStyle = PALETTE.ink;
  ctx.fillRect(x, y, TILE_PX, TILE_PX);
  ctx.fillStyle = body;
  ctx.fillRect(x + 1, y + 1, 6, 4);
  ctx.fillStyle = PALETTE.light;
  ctx.fillRect(x + 1, y + 1, 6, 1);
  ctx.fillStyle = shade;
  ctx.fillRect(x + 1, y + 5, 6, 2);
  ctx.fillStyle = body;
  ctx.fillRect(x + 3, y + 3, 2, 1);
  if (moving) {
    ctx.fillStyle = PALETTE.light;
    ctx.fillRect(x + 2, y + 3, 4, 2);
  }
  if (conveyor) {
    ctx.fillStyle = PALETTE.ink;
    ctx.beginPath();
    if (conveyor > 0) {
      ctx.moveTo(x + 5, y + 2);
      ctx.lineTo(x + 7, y + 4);
      ctx.lineTo(x + 5, y + 6);
    } else {
      ctx.moveTo(x + 3, y + 2);
      ctx.lineTo(x + 1, y + 4);
      ctx.lineTo(x + 3, y + 6);
    }
    ctx.fill();
  }
}

function drawWallTile(ctx, x, y) {
  ctx.fillStyle = PALETTE.ink;
  ctx.fillRect(x, y, TILE_PX, TILE_PX);
  ctx.fillStyle = PALETTE.dark;
  ctx.fillRect(x + 1, y + 1, 6, 6);
  ctx.fillStyle = PALETTE.mid;
  ctx.fillRect(x + 2, y + 2, 4, 1);
  ctx.fillRect(x + 2, y + 5, 4, 1);
  ctx.fillStyle = PALETTE.light;
  ctx.fillRect(x + 2, y + 2, 1, 1);
}

function drawTile(ctx, tile, x, y, switchOn = true) {
  fillBase(ctx, x, y);
  if (tile === TILE.EMPTY) return;

  if (tile === TILE.WALL) {
    drawWallTile(ctx, x, y);
    return;
  }
  if (tile === TILE.SOLID) {
    drawPlatformTile(ctx, x, y);
    return;
  }
  if (tile === TILE.MOVING) {
    drawPlatformTile(ctx, x, y, { moving: true });
    return;
  }
  if (tile === TILE.TOGGLE || tile === TILE.TOGGLE_OFF) {
    drawPlatformTile(ctx, x, y, { off: tile === TILE.TOGGLE_OFF || !switchOn });
    return;
  }
  if (tile === TILE.CONV_L || tile === TILE.CONV_R) {
    drawPlatformTile(ctx, x, y, { conveyor: tile === TILE.CONV_R ? 1 : -1 });
    return;
  }
  if (tile === TILE.CRACK) {
    drawPlatformTile(ctx, x, y);
    ctx.fillStyle = PALETTE.ink;
    ctx.fillRect(x + 2, y + 1, 1, 5);
    ctx.fillRect(x + 4, y + 1, 1, 5);
    ctx.fillRect(x + 6, y + 1, 1, 5);
    ctx.fillRect(x + 3, y + 4, 1, 1);
    ctx.fillRect(x + 5, y + 3, 1, 1);
    return;
  }
  if (tile === TILE.SPRING) {
    ctx.fillStyle = PALETTE.ink;
    ctx.fillRect(x + 2, y + 1, 4, 1);
    ctx.fillRect(x + 3, y + 2, 2, 1);
    ctx.fillRect(x + 2, y + 3, 4, 1);
    ctx.fillRect(x + 1, y + 4, 6, 1);
    ctx.fillRect(x + 2, y + 5, 4, 1);
    ctx.fillRect(x + 3, y + 6, 2, 1);
    ctx.fillRect(x + 2, y + 7, 4, 1);
    return;
  }
  if (tile === TILE.SPIKE) {
    ctx.fillStyle = PALETTE.ink;
    ctx.fillRect(x, y + 7, 8, 1);
    ctx.fillStyle = PALETTE.ink;
    ctx.beginPath();
    ctx.moveTo(x + 1, y + 7);
    ctx.lineTo(x + 3, y + 1);
    ctx.lineTo(x + 5, y + 7);
    ctx.moveTo(x + 4, y + 7);
    ctx.lineTo(x + 6, y + 2);
    ctx.lineTo(x + 7, y + 7);
    ctx.fill();
    ctx.fillStyle = PALETTE.light;
    ctx.fillRect(x + 3, y + 2, 1, 3);
    return;
  }
  if (tile === TILE.WATER) {
    ctx.fillStyle = PALETTE.dark;
    ctx.fillRect(x, y + 1, TILE_PX, 7);
    ctx.fillStyle = PALETTE.mid;
    ctx.fillRect(x, y, TILE_PX, 5);
    ctx.fillStyle = PALETTE.light;
    ctx.fillRect(x + 1, y + 2, 3, 1);
    ctx.fillRect(x + 4, y + 5, 3, 1);
    return;
  }
  if (tile === TILE.COIN || tile === TILE.BATTERY || tile === TILE.BUBBLE) {
    if (tile === TILE.BATTERY) {
      drawPixelPattern(ctx, x, y, [
        "  ####  ",
        " #    # ",
        "# #  # #",
        "# #  # #",
        "# #  # #",
        " #    # ",
        "  ####  "
      ]);
      return;
    }
    drawPixelPattern(ctx, x, y, [
      "        ",
      "   ##   ",
      "  #  #  ",
      " #    # ",
      " #    # ",
      "  #  #  ",
      "   ##   "
    ]);
    return;
  }
  if (tile === TILE.EXIT) {
    ctx.fillStyle = PALETTE.ink;
    ctx.fillRect(x + 1, y, 6, 8);
    ctx.fillStyle = PALETTE.dark;
    ctx.fillRect(x + 2, y + 1, 4, 6);
    ctx.fillStyle = PALETTE.light;
    ctx.fillRect(x + 3, y + 1, 2, 1);
    ctx.fillStyle = PALETTE.white;
    ctx.fillRect(x + 5, y + 4, 1, 1);
    return;
  }
  if (tile === TILE.SWITCH) {
    ctx.fillStyle = PALETTE.ink;
    ctx.fillRect(x + 1, y + 5, 6, 2);
    ctx.fillStyle = PALETTE.dark;
    ctx.fillRect(x + 3, y + 1, 2, 4);
    ctx.fillStyle = PALETTE.white;
    ctx.fillRect(x + 3, y + 1, 2, 1);
    return;
  }
  if (tile === TILE.FAN_L || tile === TILE.FAN_R) {
    const right = tile === TILE.FAN_R;
    ctx.fillStyle = PALETTE.ink;
    ctx.beginPath();
    ctx.moveTo(x + (right ? 6 : 2), y + 1);
    ctx.lineTo(x + (right ? 6 : 2), y + 7);
    ctx.lineTo(x + (right ? 1 : 7), y + 4);
    ctx.fill();
    ctx.fillStyle = PALETTE.light;
    ctx.fillRect(x + 3, y + 3, 2, 2);
    return;
  }
  if (tile === TILE.ARROW_D || tile === TILE.ARROW_R) {
    if (tile === TILE.ARROW_D) {
      drawPixelPattern(ctx, x, y, [
        "        ",
        "   ##   ",
        "   ##   ",
        "   ##   ",
        " ###### ",
        "  ####  ",
        "   ##   "
      ]);
    } else {
      drawPixelPattern(ctx, x, y, [
        "        ",
        "   #    ",
        "   ##   ",
        " #######",
        " #######",
        "   ##   ",
        "   #    "
      ]);
    }
  }
}

function drawDebugPlayer(ctx, x, y) {
  drawPixelPattern(ctx, x, y, [
    "  ###   ",
    "  ###   ",
    " #####  ",
    "  ###   ",
    "  # #   ",
    "   #    ",
    "  ###   ",
    "   #    "
  ]);
}

export function drawLevelCanvas(canvas, room, scale = 1) {
  canvas.width = SCREEN_TILES_W * TILE_PX;
  canvas.height = SCREEN_TILES_H * TILE_PX;
  canvas.style.setProperty("--debug-scale", String(scale));
  const ctx = canvas.getContext("2d");
  ctx.imageSmoothingEnabled = false;
  for (let y = 0; y < SCREEN_TILES_H; y += 1) {
    for (let x = 0; x < SCREEN_TILES_W; x += 1) {
      drawTile(ctx, room.stage[y][x], x * TILE_PX, y * TILE_PX, room.switchOn);
    }
  }

  drawDebugPlayer(ctx, room.spawn.x, room.spawn.y);

  for (const enemy of room.enemies) {
    const ex = enemy.tx * TILE_PX;
    const ey = enemy.ty * TILE_PX;
    ctx.fillStyle = PALETTE.ink;
    ctx.fillRect(ex + 1, ey + 1, 6, 6);
    ctx.fillStyle = PALETTE.white;
    ctx.fillRect(ex + 2, ey + 3, 1, 1);
    ctx.fillRect(ex + 5, ey + 3, 1, 1);
  }
}

function levelTitle(room) {
  return `${String(room.level + 1).padStart(2, "0")} W${room.world + 1}-${room.local + 1} ${WORLD_NAMES[room.world]}`;
}

function summarizeRoom(room) {
  const counts = new Map();
  for (const row of room.stage) {
    for (const tile of row) counts.set(tile, (counts.get(tile) || 0) + 1);
  }
  const pieces = [];
  for (const tile of [TILE.SPRING, TILE.SPIKE, TILE.WATER, TILE.TOGGLE, TILE.CONV_R, TILE.CONV_L, TILE.FAN_R, TILE.FAN_L]) {
    const count = counts.get(tile) || 0;
    if (count) pieces.push(`${TILE_NAMES.get(tile)} ${count}`);
  }
  if (room.enemies.length) pieces.push(`enemy ${room.enemies.length}`);
  if (room.moving) pieces.push("moving platform");
  return pieces.length ? pieces.join(" / ") : "starter path";
}

function makeButton(text, className, onClick) {
  const button = document.createElement("button");
  button.type = "button";
  button.className = className;
  button.textContent = text;
  button.addEventListener("click", onClick);
  return button;
}

function setDebugUrl(level) {
  const url = new URL(window.location.href);
  url.searchParams.set("debug", "levels");
  if (level == null) url.searchParams.delete("level");
  else url.searchParams.set("level", String(level + 1));
  window.history.replaceState({}, "", url);
}

export function renderLevelDebug({ root, onBack = () => {} }) {
  const params = new URLSearchParams(window.location.search);
  const initialLevel = params.has("level") ? Math.max(0, Math.min(LEVEL_COUNT - 1, Number(params.get("level")) - 1)) : null;
  const levels = generateAdventureLevels();
  let selected = Number.isFinite(initialLevel) ? initialLevel : null;

  root.innerHTML = "";
  root.classList.add("debug-shell");

  const panel = document.createElement("section");
  panel.className = "level-debug";

  const toolbar = document.createElement("header");
  toolbar.className = "level-debug-toolbar";
  const title = document.createElement("h1");
  title.textContent = "Level Debug";
  const actions = document.createElement("div");
  actions.className = "level-debug-actions";
  toolbar.append(title, actions);

  const content = document.createElement("div");
  content.className = "level-debug-content";
  panel.append(toolbar, content);
  root.append(panel);

  function render() {
    actions.innerHTML = "";
    content.innerHTML = "";
    actions.append(
      makeButton("Back", "level-debug-button", onBack),
      makeButton("All", "level-debug-button", () => {
        selected = null;
        setDebugUrl(null);
        render();
      })
    );

    if (selected != null) {
      const room = levels[selected];
      actions.append(
        makeButton("Prev", "level-debug-button", () => {
          selected = (selected + LEVEL_COUNT - 1) % LEVEL_COUNT;
          setDebugUrl(selected);
          render();
        }),
        makeButton("Next", "level-debug-button", () => {
          selected = (selected + 1) % LEVEL_COUNT;
          setDebugUrl(selected);
          render();
        })
      );

      const single = document.createElement("article");
      single.className = "level-debug-single";
      const heading = document.createElement("h2");
      heading.textContent = levelTitle(room);
      const meta = document.createElement("p");
      meta.textContent = summarizeRoom(room);
      const canvas = document.createElement("canvas");
      canvas.className = "level-debug-canvas is-large";
      canvas.setAttribute("aria-label", `Rendered ${levelTitle(room)}`);
      drawLevelCanvas(canvas, room, 3);
      single.append(heading, canvas, meta);
      content.append(single);
      return;
    }

    const grid = document.createElement("div");
    grid.className = "level-debug-grid";
    for (const room of levels) {
      const card = document.createElement("button");
      card.type = "button";
      card.className = "level-debug-card";
      card.dataset.level = String(room.level + 1);
      card.addEventListener("click", () => {
        selected = room.level;
        setDebugUrl(selected);
        render();
      });
      const canvas = document.createElement("canvas");
      canvas.className = "level-debug-canvas";
      canvas.setAttribute("aria-label", `Rendered ${levelTitle(room)}`);
      drawLevelCanvas(canvas, room);
      const label = document.createElement("span");
      label.className = "level-debug-card-label";
      label.textContent = levelTitle(room);
      const meta = document.createElement("span");
      meta.className = "level-debug-card-meta";
      meta.textContent = summarizeRoom(room);
      card.append(canvas, label, meta);
      grid.append(card);
    }
    content.append(grid);
  }

  render();
}
