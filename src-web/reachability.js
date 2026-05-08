/* Reachability Module.
 *
 * Given a room (stage[][], spawn pixel coords, switchOn), answers whether
 * the Goal sequence is achievable: from spawn, can the player reach a
 * surface adjacent to the key, then from any such surface reach a surface
 * adjacent to the exit? Also reports one-tile pockets the player could fall
 * into and never escape.
 *
 * The movement model is a deliberate approximation of the bounce physics
 * the ROM runs: nodes are "standing on tile (x, y) with switch state s",
 * edges are "the player can launch from here and land there in one bounce".
 * Spring launches reach higher and farther than rock-platform bounces.
 *
 * Used by:
 *   - tests in test/web-modules.test.mjs to assert every Adventure room is
 *     solvable
 *   - any future tooling that wants to reject generated Panic rooms before
 *     showing them to the player
 */

import { TILE, TILE_PX, tileIsSolid, tileIsDanger } from "./level-debug.js";

const FLOOR_Y_MIN = 3;
const FLOOR_Y_MAX = 17;
const ROOM_X_MIN = 1;
const ROOM_X_MAX = 18;

function nodeKey(node) {
  return `${node.x},${node.y},${node.switchOn ? 1 : 0}`;
}

function nodeFromKey(key) {
  const [x, y, switchFlag] = key.split(",").map(Number);
  return { x, y, switchOn: switchFlag === 1 };
}

function landingAt(room, x, y, switchOn) {
  const tile = room.stage[y][x];
  return { x, y, switchOn: tile === TILE.SWITCH ? !switchOn : switchOn };
}

/* A "surface" is a solid tile at (x, supportY) with a non-solid, non-danger
 * tile directly above it. Player feet sit on supportY, head occupies y-1. */
function isSurface(room, x, supportY, switchOn) {
  if (x < ROOM_X_MIN || x > ROOM_X_MAX || supportY <= FLOOR_Y_MIN - 1 || supportY > FLOOR_Y_MAX) return false;
  if (!tileIsSolid(room.stage[supportY][x], switchOn)) return false;
  const headTile = room.stage[supportY - 1][x];
  return !tileIsSolid(headTile, switchOn) && !tileIsDanger(headTile);
}

/* One-tile pocket = an empty/coin tile sealed on three sides by pocket-solid
 * tiles. Distinct from "exit pocket" — this is the trap shape we never want
 * the player to be able to land in. */
export function isOneTilePocket(room, x, y, switchOn) {
  const tile = room.stage[y][x];
  if (tileIsDanger(tile) || tileIsSolid(tile, switchOn)) return false;
  if (tile === TILE.EXIT) return false;
  return tileIsSolid(room.stage[y + 1][x], switchOn) &&
    tileIsSolid(room.stage[y][x - 1], switchOn) &&
    tileIsSolid(room.stage[y][x + 1], switchOn);
}

/* Surfaces near the player's spawn that they can immediately stand on. */
export function startNodes(room) {
  const spawnX = Math.floor(room.spawn.x / TILE_PX);
  const spawnFootY = Math.floor((room.spawn.y + TILE_PX) / TILE_PX);
  const nodes = [];
  for (let dy = 0; dy <= 7; dy += 1) {
    for (let dx = -2; dx <= 2; dx += 1) {
      const x = spawnX + dx;
      const y = spawnFootY + dy;
      if (isSurface(room, x, y, room.switchOn)) nodes.push(landingAt(room, x, y, room.switchOn));
    }
    if (nodes.length) return nodes;
  }
  return nodes;
}

/* Surfaces adjacent to (below or beside) the goal tile. Both switch states
 * are considered because the player may need to flip the switch first. */
export function targetNodes(room, targetTile) {
  const candidates = [];
  for (let y = FLOOR_Y_MIN; y < room.stage.length - 1; y += 1) {
    for (let x = ROOM_X_MIN; x < room.stage[y].length - 1; x += 1) {
      if (room.stage[y][x] !== targetTile) continue;
      for (const switchOn of [false, true]) {
        for (let sx = x - 1; sx <= x + 1; sx += 1) {
          for (let sy = y + 1; sy <= Math.min(FLOOR_Y_MAX, y + 4); sy += 1) {
            if (isSurface(room, sx, sy, switchOn)) candidates.push(nodeKey(landingAt(room, sx, sy, switchOn)));
          }
        }
      }
    }
  }
  return new Set(candidates);
}

/* True if the player standing on `from` can launch and land on `to`.
 * Spring launches: rise <= 7, |dx| <= 8.
 * Other launches:  rise <= 4, |dx| up to 7 (less the further they rise).
 * Falls: |dx| up to maxDx + 2.  */
function canMove(room, from, to) {
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

function neighbors(room, node) {
  const out = [];
  for (let y = FLOOR_Y_MIN; y <= FLOOR_Y_MAX; y += 1) {
    for (let x = ROOM_X_MIN; x < ROOM_X_MAX + 1; x += 1) {
      if (!isSurface(room, x, y, node.switchOn)) continue;
      if (canMove(room, node, { x, y })) out.push(landingAt(room, x, y, node.switchOn));
    }
  }
  return out;
}

/* Flood-fill the surface graph from `starts`. Returns the set of reached
 * node keys. Use `nodeFromKey` to decode back to {x, y, switchOn}. */
export function reachableFrom(room, starts) {
  const queue = [...starts];
  const seen = new Set(queue.map(nodeKey));
  for (let i = 0; i < queue.length; i += 1) {
    for (const next of neighbors(room, queue[i])) {
      const key = nodeKey(next);
      if (!seen.has(key)) {
        seen.add(key);
        queue.push(next);
      }
    }
  }
  return seen;
}

function intersects(seen, targets) {
  for (const key of targets) if (seen.has(key)) return true;
  return false;
}

/* Top-level Goal-sequence check. Returns { solvable, reason } where
 * `reason` is null on success or a short string identifying the first
 * gate that failed. */
export function roomIsSolvable(room) {
  const starts = startNodes(room);
  if (!starts.length) return { solvable: false, reason: "no landing below spawn" };
  const keyTargets = targetNodes(room, TILE.KEY);
  const exitTargets = targetNodes(room, TILE.EXIT);
  if (!keyTargets.size) return { solvable: false, reason: "no key approach surface" };
  if (!exitTargets.size) return { solvable: false, reason: "no exit approach surface" };

  const fromStart = reachableFrom(room, starts);
  const keyStarts = [...keyTargets].filter((key) => fromStart.has(key)).map(nodeFromKey);
  if (!keyStarts.length) return { solvable: false, reason: "key unreachable from spawn" };

  if (!intersects(reachableFrom(room, keyStarts), exitTargets)) {
    return { solvable: false, reason: "exit unreachable from key" };
  }

  for (const key of fromStart) {
    if (!intersects(reachableFrom(room, [nodeFromKey(key)]), exitTargets)) {
      return { solvable: false, reason: `surface ${key} cannot escape to exit` };
    }
  }
  return { solvable: true, reason: null };
}

/* Scans every cell in both switch states and returns coordinates of any
 * one-tile pockets — the trap shape that pins the player. Empty result
 * means the room has no such traps. */
export function findOneTilePockets(room) {
  const traps = [];
  for (const switchOn of [false, true]) {
    for (let y = FLOOR_Y_MIN; y < FLOOR_Y_MAX; y += 1) {
      for (let x = ROOM_X_MIN; x < ROOM_X_MAX + 1; x += 1) {
        if (isOneTilePocket(room, x, y, switchOn)) traps.push({ x, y, switchOn });
      }
    }
  }
  return traps;
}
