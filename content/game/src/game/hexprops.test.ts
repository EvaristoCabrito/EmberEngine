import assert from "node:assert/strict";
import { test } from "node:test";
import { BIG_HOUSE_DECOR_IDS, clearRockColumnTiles, DECORATIONS, FOOTPRINT_TYPE_6, HOUSE_DECOR_IDS, MISSIONS, parseLayout, placedFootprint, placedBlockingFootprint, SOLID_CART_DECOR_IDS, SOLID_HOUSE_DECOR_IDS, SOLID_ROCK_DECOR_IDS, TERRAIN } from "./data.ts";
import { EMPTY_OVERLAY, HEX_BLOCKED, HEX_HIGH, buildDecorOverlay, hexDef, hexProps } from "./hexprops.ts";
import type { DecorationPlacement, TerrainId } from "./types.ts";

const COLS = 10;
const ROWS = 6;
const board = (fill: TerrainId = "plains"): TerrainId[] => Array.from({ length: COLS * ROWS }, () => fill);
const at = (x: number, y: number) => y * COLS + x;
/** A one-hex footprint, so the tests say what they mean without the DECORATIONS table. */
const oneHex = () => [{ dx: 0, dy: 0 }];

test("legacy columns under rotated rocks become ground while rocks still block", () => {
  const p: DecorationPlacement = { id: "spike-rocks", x: 4, y: 2, rot: 2 };
  const tiles = board("nave");
  const cells = placedFootprint(p);
  for (const { dx, dy } of cells) tiles[at(p.x + dx, p.y + dy)] = "column";
  tiles[0] = "column";
  const cleaned = clearRockColumnTiles(tiles, COLS, ROWS, [p]);
  const overlay = buildDecorOverlay([p], COLS, ROWS, placedBlockingFootprint);
  for (const { dx, dy } of cells) {
    const x = p.x + dx, y = p.y + dy;
    assert.equal(cleaned[at(x, y)], "nave");
    assert.equal(tiles[at(x, y)], "column", "source map remains immutable");
    assert.equal(hexDef(cleaned, COLS, x, y, overlay).passable, false);
    assert.equal(hexDef(cleaned, COLS, x, y, overlay).blocksShot, true);
    assert.equal(hexDef(cleaned, COLS, x, y, EMPTY_OVERLAY).passable, true, "removing rock restores walkable ground");
  }
  assert.equal(cleaned[0], "column", "independent columns are preserved");
});

test("rock cleanup preserves authored ground and uses the map base for legacy columns", () => {
  const tiles = board("snow");
  const p: DecorationPlacement = { id: "rocks-3d-grey-outcrop", x: 4, y: 2 };
  tiles[at(4, 2)] = "column";
  tiles[at(5, 2)] = "hill";
  const cleaned = clearRockColumnTiles(tiles, COLS, ROWS, [p], "woods");
  assert.equal(cleaned[at(4, 2)], "woods");
  assert.equal(cleaned[at(5, 2)], "hill");
});

test("generated mission rocks block without underlying column terrain", () => {
  let checked = 0;
  for (const mission of MISSIONS) {
    const tiles = parseLayout(mission.layout);
    const rocks = (mission.decorations ?? []).filter(p => SOLID_ROCK_DECOR_IDS.has(p.id));
    const overlay = buildDecorOverlay(rocks, mission.cols, mission.rows, placedBlockingFootprint);
    for (const p of rocks) for (const { dx, dy } of placedFootprint(p)) {
      const x = p.x + dx, y = p.y + dy;
      if (x < 0 || y < 0 || x >= mission.cols || y >= mission.rows) continue;
      assert.notEqual(tiles[y * mission.cols + x], "column", `${mission.id} rock at ${x},${y}`);
      assert.equal(hexDef(tiles, mission.cols, x, y, overlay).passable, false);
      checked++;
    }
  }
  assert.ok(checked > 0);
});

test("no overlay means the base terrain, object identity included", () => {
  const tiles = board("woods");
  const d = hexDef(tiles, COLS, 3, 2);
  assert.equal(d, TERRAIN.woods, "must hand back the shared entry, not a copy");
});

test("a cell no decoration touches keeps the base terrain", () => {
  const tiles = board();
  const overlay = buildDecorOverlay([{ id: "x", x: 1, y: 1, blocksPath: true }], COLS, ROWS, oneHex);
  assert.equal(hexDef(tiles, COLS, 5, 5, overlay), TERRAIN.plains);
});

test("blocksPath consolidates to impassable without touching tiles", () => {
  const tiles = board();
  const before = [...tiles];
  const overlay = buildDecorOverlay([{ id: "x", x: 4, y: 2, blocksPath: true }], COLS, ROWS, oneHex);
  const d = hexDef(tiles, COLS, 4, 2, overlay);
  assert.equal(d.passable, false);
  assert.equal(d.moveCost, 99);
  assert.equal(d.blocksShot, true, "solid also stops arrows and sight");
  assert.equal(d.height, undefined, "asking for blocked must not grant height");
  assert.deepEqual(tiles, before, "the painted board must be untouched");
  assert.equal(tiles[at(4, 2)], "plains");
});

test("yieldsHighGround consolidates to the same numbers a hill carries", () => {
  const tiles = board();
  const overlay = buildDecorOverlay([{ id: "x", x: 4, y: 2, yieldsHighGround: true }], COLS, ROWS, oneHex);
  const d = hexDef(tiles, COLS, 4, 2, overlay);
  assert.equal(d.height, 1);
  assert.equal(d.atk, TERRAIN.hill.atk, "+2 damage, same as standing on a hill");
  assert.equal(d.def, TERRAIN.hill.def);
  assert.equal(d.moveCost, TERRAIN.hill.moveCost);
  assert.equal(d.passable, true, "high ground is somewhere you can stand");
  assert.equal(tiles[at(4, 2)], "plains", "the painted board must be untouched");
});

test("both switches need no terrain of their own", () => {
  const tiles = board();
  const overlay = buildDecorOverlay([{ id: "x", x: 4, y: 2, blocksPath: true, yieldsHighGround: true }], COLS, ROWS, oneHex);
  const d = hexDef(tiles, COLS, 4, 2, overlay);
  assert.equal(d.passable, false, "nobody stands on it");
  assert.equal(d.height, 1, "and it is tall enough to stop a low arrow");
  assert.equal(tiles[at(4, 2)], "plains");
  // The combination exists as an answer, not as a TerrainId — nothing in the table has it.
  const both = (Object.keys(TERRAIN) as TerrainId[]).filter((id) => !TERRAIN[id].passable && TERRAIN[id].height);
  assert.deepEqual(both, [], "no painted terrain is both, which is why consolidating beats stamping");
});

test("switches are additive: they never take a property away", () => {
  const tiles = board();
  for (const base of Object.keys(TERRAIN) as TerrainId[]) {
    tiles[at(2, 2)] = base;
    for (const bp of [false, true]) {
      for (const hg of [false, true]) {
        const overlay = buildDecorOverlay(
          [{ id: "x", x: 2, y: 2, blocksPath: bp || undefined, yieldsHighGround: hg || undefined }],
          COLS, ROWS, oneHex,
        );
        const d = hexDef(tiles, COLS, 2, 2, overlay);
        const b = TERRAIN[base];
        if (!b.passable) assert.equal(d.passable, false, `${base} ${bp}/${hg}: lost impassability`);
        if (b.height) assert.equal(!!d.height, true, `${base} ${bp}/${hg}: lost height`);
        if (bp) assert.equal(d.passable, false, `${base} ${bp}/${hg}: asked to block, did not`);
        if (hg) assert.equal(!!d.height, true, `${base} ${bp}/${hg}: asked for height, did not`);
        if (!bp && !hg) assert.equal(d, b, `${base}: untouched cell must be the shared entry`);
      }
    }
  }
});

test("high ground on a solid hex adds height and leaves it solid", () => {
  const tiles = board("barricade");
  const overlay = buildDecorOverlay([{ id: "x", x: 1, y: 1, yieldsHighGround: true }], COLS, ROWS, oneHex);
  const d = hexDef(tiles, COLS, 1, 1, overlay);
  assert.equal(d.passable, false, "a barricade must not become walkable");
  assert.equal(d.height, 1);
  assert.equal(d.id, "barricade", "and it is still a barricade, so the troll rule survives");
});

test("a multi-hex footprint marks every cell it covers", () => {
  const tiles = board();
  const pair = () => [{ dx: 0, dy: 0 }, { dx: 1, dy: 0 }];
  const overlay = buildDecorOverlay([{ id: "x", x: 3, y: 1, blocksPath: true }], COLS, ROWS, pair);
  assert.equal(hexProps(tiles, COLS, 3, 1, overlay).blocked, true);
  assert.equal(hexProps(tiles, COLS, 4, 1, overlay).blocked, true);
  assert.equal(hexProps(tiles, COLS, 5, 1, overlay).blocked, false);
});

test("each house blocks its complete ground footprint even when its saved checkbox is off", () => {
  const tiles = board();
  const houseIds = [...HOUSE_DECOR_IDS, ...BIG_HOUSE_DECOR_IDS, ...SOLID_HOUSE_DECOR_IDS];
  assert.ok(houseIds.length > 0);
  for (const id of houseIds) {
    const placement = { id, x: 4, y: 3 };
    const overlay = buildDecorOverlay([placement], COLS, ROWS, placedBlockingFootprint);
    for (const { dx, dy } of placedBlockingFootprint(placement)) {
      assert.equal(hexDef(tiles, COLS, placement.x + dx, placement.y + dy, overlay).passable, false, `${id} blocks ${placement.x + dx},${placement.y + dy}`);
    }
    if (id === "burnt-house-ruins") {
      assert.deepEqual(placedBlockingFootprint(placement).slice(0, 6), FOOTPRINT_TYPE_6, `${id} retains Type 6 without the erroneous leftward shift`);
      assert.ok(placedBlockingFootprint(placement).slice(6).every(cell => cell.dy <= -2), 'extra collision covers the rear only');
    } else if (HOUSE_DECOR_IDS.has(id) || id === "burning-hamlet") {
      assert.equal(placedBlockingFootprint(placement).length, id === "burning-house" || id === "burning-hamlet" ? 9 : 5, `${id} keeps its ground blocking footprint`);
    }
  }
});

test("every cart, wagon and handcart blocks its full footprint and line of sight", () => {
  const tiles = board();
  for (const id of SOLID_CART_DECOR_IDS) {
    const placement = { id, x: 4, y: 3 };
    assert.ok(DECORATIONS[id], `${id} has a decoration definition`);
    const footprint = placedBlockingFootprint(placement);
    assert.ok(footprint.length > 0, `${id} has a blocking footprint`);
    const overlay = buildDecorOverlay([placement], COLS, ROWS, placedBlockingFootprint);
    for (const { dx, dy } of footprint) {
      const cell = hexDef(tiles, COLS, placement.x + dx, placement.y + dy, overlay);
      assert.equal(cell.passable, false, `${id} blocks movement at ${placement.x + dx},${placement.y + dy}`);
      assert.equal(cell.blocksShot, true, `${id} blocks shots and sight at ${placement.x + dx},${placement.y + dy}`);
    }
  }
});

test("two props on one cell contribute both flags", () => {
  const tiles = board();
  const placements: DecorationPlacement[] = [
    { id: "a", x: 6, y: 3, blocksPath: true },
    { id: "b", x: 6, y: 3, yieldsHighGround: true },
  ];
  const overlay = buildDecorOverlay(placements, COLS, ROWS, oneHex);
  assert.equal(overlay[at(6, 3)], HEX_BLOCKED | HEX_HIGH);
  const p = hexProps(tiles, COLS, 6, 3, overlay);
  assert.deepEqual(p, { blocked: true, highGround: true });
});

test("a footprint hanging off the board does not write outside it", () => {
  const tiles = board();
  const wide = () => Array.from({ length: 5 }, (_, dx) => ({ dx, dy: 0 }));
  const overlay = buildDecorOverlay([{ id: "x", x: COLS - 2, y: 0, blocksPath: true }], COLS, ROWS, wide);
  assert.equal(overlay.length, COLS * ROWS, "no growth");
  assert.equal(hexProps(tiles, COLS, COLS - 2, 0, overlay).blocked, true);
  assert.equal(hexProps(tiles, COLS, COLS - 1, 0, overlay).blocked, true);
  // The three cells that fell off the right edge must not have wrapped onto row 1.
  for (let x = 0; x < 3; x++) {
    assert.equal(hexProps(tiles, COLS, x, 1, overlay).blocked, false, `wrapped onto 1,${x}`);
  }
});

test("a placement with neither switch contributes nothing", () => {
  const overlay = buildDecorOverlay([{ id: "x", x: 2, y: 2 }], COLS, ROWS, oneHex);
  assert.equal(overlay.every((b) => b === 0), true);
});


test("painted elevation grants high ground independently of terrain art and camera", () => {
  const tiles = board();
  const heights = Array(COLS * ROWS).fill(0);
  heights[at(3, 2)] = 4;
  const overlay = buildDecorOverlay([], COLS, ROWS, oneHex, heights);
  const d = hexDef(tiles, COLS, 3, 2, overlay);
  assert.equal(d.height, 1);
  assert.equal(d.atk, TERRAIN.hill.atk);
  assert.equal(d.def, TERRAIN.hill.def);
  assert.equal(d.moveCost, TERRAIN.hill.moveCost);
  assert.equal(tiles[at(3, 2)], "plains");
  assert.equal(hexDef(tiles, COLS, 2, 2, overlay), TERRAIN.plains);
});

test("elevation preserves blocking and does not stack the existing high-ground bonus", () => {
  const heights = Array(COLS * ROWS).fill(0);
  heights[at(3, 2)] = 12;
  const overlay = buildDecorOverlay([{ id: "x", x: 3, y: 2, blocksPath: true, yieldsHighGround: true }], COLS, ROWS, oneHex, heights);
  const d = hexDef(board(), COLS, 3, 2, overlay);
  assert.equal(d.passable, false);
  assert.equal(d.blocksShot, true);
  assert.equal(d.atk, TERRAIN.hill.atk);
  assert.equal(d.def, TERRAIN.hill.def);
});

test("absent, zero and invalid sculpted elevations retain the original terrain rules", () => {
  const heights = Array(COLS * ROWS).fill(0);
  heights[at(1, 1)] = NaN; heights[at(2, 1)] = Infinity; heights[at(3, 1)] = -1;
  const overlay = buildDecorOverlay([], COLS, ROWS, oneHex, heights);
  assert(overlay.every(bits => bits === 0));
  assert.equal(hexDef(board("hill"), COLS, 2, 1, overlay), TERRAIN.hill);
});
