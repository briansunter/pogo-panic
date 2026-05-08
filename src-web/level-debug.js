import roomsJson from "./levels.json" with { type: "json" };
import tileTableJson from "./tile-table.json" with { type: "json" };
import tileArtJson from "./tile-art.json" with { type: "json" };

export const LEVEL_COUNT = roomsJson.length;
export const SCREEN_TILES_W = 20;
export const SCREEN_TILES_H = 18;
export const TILE_PX = 8;

/* Decoded 2bpp pixel grids for src-rom/tile-art.c::bg_tiles[]. Indexed by
 * tile id (TILE.*); each entry is an 8x8 array of GB color indices 0-3
 * (default BGP order: 0 = lightest). The host tool tools/dump-tiles.c
 * regenerates tile-art.json from the same bytes the ROM uploads via
 * set_bkg_data, so the pixel data has exactly one source of truth. */
const TILE_PIXELS = tileArtJson.tiles.map((entry) => entry.pixels);

/* TILE map derived from tile-table.json's name field, which is stringified
 * from enum TileType in src-rom/game-logic.h via the TILE_LIST macro. The
 * pair is the canonical Tile vocabulary; do not maintain a parallel list. */
export const TILE = Object.freeze(
  tileTableJson.tiles.reduce((acc, row) => {
    acc[row.name] = row.id;
    return acc;
  }, {})
);

const TILE_CATEGORY_FLAG = tileTableJson.flags;
const TILE_FLAGS_BY_ID = (() => {
  const flags = new Uint8Array(tileTableJson.count);
  for (const row of tileTableJson.tiles) flags[row.id] = row.flags;
  return flags;
})();

function tileHasFlag(tile, flag) {
  return tile >= 0 && tile < TILE_FLAGS_BY_ID.length && (TILE_FLAGS_BY_ID[tile] & flag) !== 0;
}

export function tileIsSolid(tile, switchOn = true) {
  if (tile === TILE.TOGGLE) return Boolean(switchOn);
  return tileHasFlag(tile, TILE_CATEGORY_FLAG.solid);
}

export function tileIsDanger(tile) {
  return tileHasFlag(tile, TILE_CATEGORY_FLAG.danger);
}

export function tileIsMechanic(tile) {
  return tileHasFlag(tile, TILE_CATEGORY_FLAG.mechanic);
}

export function tileIsPlatform(tile) {
  return tileHasFlag(tile, TILE_CATEGORY_FLAG.platform);
}

export function tileIsPocketSolid(tile) {
  return tileHasFlag(tile, TILE_CATEGORY_FLAG.pocketSolid);
}

const WORLD_NAMES = ["Groove", "Switch", "Drift", "Motion", "Gauntlet"];
const PALETTE = {
  white: "#f8f8f8",
  light: "#d8d8d8",
  mid: "#8a8a8a",
  dark: "#4b4b4b",
  ink: "#111111"
};

/* GB BGP shades 0-3 mapped to the Pocket palette. Index 0 is null so
 * background pixels stay whatever fillBase painted (saves redundant fills). */
const TILE_PALETTE = [null, PALETTE.light, PALETTE.mid, PALETTE.ink];

/* Display labels for the room summary. Default rule: lowercase the canonical
 * name and turn underscores into hyphens (FAN_L -> "fan-l"). Override the few
 * tiles where a friendlier label exists. */
const TILE_LABEL_OVERRIDES = {
  FAN_L: "fan-left",
  FAN_R: "fan-right",
  CONV_L: "conveyor-left",
  CONV_R: "conveyor-right",
  ARROW_D: "arrow-down",
  ARROW_R: "arrow-right"
};
const TILE_NAMES = new Map(
  tileTableJson.tiles.map((row) => [
    row.id,
    TILE_LABEL_OVERRIDES[row.name] ?? row.name.toLowerCase().replace(/_/g, "-")
  ])
);

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

/* Render an 8x8 tile straight from the JSON-decoded bg_tiles[] grid.
 * `palette` maps each GB color index 0-3 to a CSS color or null (skip). */
function drawArtTile(ctx, tile, x, y, palette) {
  const rows = TILE_PIXELS[tile];
  if (!rows) return;
  for (let py = 0; py < rows.length; py += 1) {
    const row = rows[py];
    for (let px = 0; px < row.length; px += 1) {
      const fill = palette[row[px]];
      if (!fill) continue;
      ctx.fillStyle = fill;
      ctx.fillRect(x + px, y + py, 1, 1);
    }
  }
}

/* The Tile renderer Module. Single Interface: tile id (+ switchOn) -> 8x8
 * pixels. Reads the canonical pixel grid the ROM uploads via set_bkg_data
 * (decoded into tile-art.json by tools/dump-tiles.c), so the debug viewer
 * is a true preview of what the ROM renders rather than a hand-mirror. */
function drawTile(ctx, tile, x, y, switchOn = true) {
  fillBase(ctx, x, y);
  /* TOGGLE shows its TOGGLE_OFF art when the room's switch is off; this is
   * the same id swap the ROM does in bg_for_tile(). */
  const drawn = (tile === TILE.TOGGLE && !switchOn) ? TILE.TOGGLE_OFF : tile;
  drawArtTile(ctx, drawn, x, y, TILE_PALETTE);
}

const PLAYER_SPRITE = [
  "  ###   ",
  "  ###   ",
  " #####  ",
  "  ###   ",
  "  # #   ",
  "   #    ",
  "  ###   ",
  "   #    "
];

/* The player is a sprite (OBJ), not a background tile, so it doesn't live
 * in tile-art.c. The debug viewer keeps a tiny ASCII pattern for it. */
function drawDebugPlayer(ctx, x, y) {
  ctx.fillStyle = PALETTE.ink;
  for (let py = 0; py < PLAYER_SPRITE.length; py += 1) {
    const row = PLAYER_SPRITE[py];
    for (let px = 0; px < row.length; px += 1) {
      if (row[px] !== " ") ctx.fillRect(x + px, y + py, 1, 1);
    }
  }
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
