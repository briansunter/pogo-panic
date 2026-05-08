import roomsJson from "./levels.json" with { type: "json" };

export const LEVEL_COUNT = roomsJson.length;
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
  KEY: 6,
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
  [TILE.KEY, "key"],
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

export function generateAdventureLevel(level) {
  const safeLevel = Math.max(0, Math.min(LEVEL_COUNT - 1, Number(level) || 0));
  return roomsJson[safeLevel];
}

export function generateAdventureLevels() {
  return roomsJson;
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
  if (tile === TILE.ROCK) {
    ctx.fillStyle = PALETTE.ink;
    ctx.beginPath();
    ctx.moveTo(x + 3, y + 1);
    ctx.lineTo(x + 6, y + 2);
    ctx.lineTo(x + 7, y + 5);
    ctx.lineTo(x + 5, y + 7);
    ctx.lineTo(x + 2, y + 7);
    ctx.lineTo(x + 1, y + 4);
    ctx.fill();
    ctx.fillStyle = PALETTE.dark;
    ctx.fillRect(x + 3, y + 2, 3, 4);
    ctx.fillStyle = PALETTE.light;
    ctx.fillRect(x + 3, y + 2, 2, 1);
    ctx.fillRect(x + 2, y + 4, 1, 1);
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
  if (tile === TILE.COIN || tile === TILE.KEY || tile === TILE.BUBBLE) {
    if (tile === TILE.KEY) {
      drawPixelPattern(ctx, x, y, [
        " ###    ",
        "#   #   ",
        "#   #   ",
        " ###    ",
        "  #     ",
        "  #     ",
        "  ###   ",
        "  #     "
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
  for (const tile of [
    TILE.SPRING,
    TILE.ROCK,
    TILE.SPIKE,
    TILE.WATER,
    TILE.TOGGLE,
    TILE.CONV_R,
    TILE.CONV_L,
    TILE.FAN_R,
    TILE.FAN_L
  ]) {
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
