import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { test } from "node:test";
import { DECORATIONS, CLASSES, CAUSTIC_VENOM, placedBlockingFootprint } from "./data.ts";
import { buildDecorOverlay, hexDef } from "./hexprops.ts";
import { footprint, hexNeighbors, hexDist, clearShot } from "./pathfinding.ts";
import { WISP_CROSSING_ID, WISP_BOSS_ID, completedAfterWispVictory, routeWispCrossing, wispCrossingCompleted } from "./wispCrossing.ts";
import type { Mission, TerrainId } from "./types.ts";

// Engine2 keeps Ember's maps in maps/ember (src/game/maps was removed); the latest saves.
const readDraft = (name: string) => JSON.parse(readFileSync(new URL(`../../maps/ember/${name}`, import.meta.url), "utf8")).draft;
const crossing = readDraft("wisp-forest-crossing012.json") as Mission;
const room = readDraft("wisp-forest-crossing-boss005.json");

test("all forward exits lead to the boss only until the crossing is completed", () => {
  const first = routeWispCrossing(crossing, []);
  const exits = first.decorations!.filter(p => p.targetMapId === WISP_BOSS_ID);
  assert.equal(exits.length, 3);
  assert.ok(exits.every(p => DECORATIONS[p.id]?.exitKind === "connector"));
  assert.equal(first.win, "escape", "clearing the original enemies cannot bypass the boss");
  for (const completed of [[WISP_CROSSING_ID], [WISP_BOSS_ID]]) {
    const repeat = routeWispCrossing(crossing, completed);
    assert.ok(wispCrossingCompleted(completed));
    assert.ok(!repeat.decorations!.some(p => p.targetMapId === WISP_BOSS_ID));
    assert.equal(repeat.decorations!.filter(p => p.id === "dungeon-exit").length, 3);
  }
  assert.equal(crossing.decorations!.filter(p => p.targetMapId === WISP_BOSS_ID).length, 3, "routing does not mutate saved map data");
});

test("only the boss victory completes the first crossing", () => {
  assert.deepEqual(completedAfterWispVictory(WISP_CROSSING_ID, []), []);
  assert.deepEqual(completedAfterWispVictory(WISP_BOSS_ID, []), [WISP_BOSS_ID, WISP_CROSSING_ID]);
  assert.deepEqual(completedAfterWispVictory(WISP_BOSS_ID, [WISP_BOSS_ID, WISP_CROSSING_ID]), [WISP_BOSS_ID, WISP_CROSSING_ID]);
});

test("the rooted boss is reachable only at the front sweep, with a closed tree barrier behind", () => {
  assert.equal(room.win, "boss");
  const bosses = room.enemySpawns.filter((p: { classId: string }) => p.classId === "carnivorousPlant");
  assert.equal(bosses.length, 1);
  const plant = bosses[0];
  assert.equal(plant.classId, "carnivorousPlant");
  assert.equal(plant.holdsPosition, true);
  assert.ok(CLASSES.carnivorousPlant.boss);
  assert.ok(!room.decorations.some((p: { id: string }) => DECORATIONS[p.id]?.exitKind), "there is no exit to bypass the Plant");
  const tiles = room.tiles as TerrainId[];
  const overlay = buildDecorOverlay(room.decorations, room.cols, room.rows, placedBlockingFootprint);
  const body = footprint({ ...plant, size: 4, footprintOffsets: CLASSES.carnivorousPlant.footprintOffsets });
  const bodyKeys = new Set(body.map(p => `${p.x},${p.y}`));
  for (const p of body) assert.ok(hexDef(tiles, room.cols, p.x, p.y, overlay).passable, "no tree overlaps the boss");
  const seen = new Set<string>();
  const queue = [...room.playerSpawns];
  for (const p of queue) {
    assert.ok(hexDef(tiles, room.cols, p.x, p.y, overlay).passable);
    seen.add(`${p.x},${p.y}`);
  }
  for (let i = 0; i < queue.length; i++) for (const p of hexNeighbors(queue[i].x, queue[i].y)) {
    const key = `${p.x},${p.y}`;
    if (p.x < 0 || p.y < 0 || p.x >= room.cols || p.y >= room.rows || seen.has(key) || bodyKeys.has(key)) continue;
    if (!hexDef(tiles, room.cols, p.x, p.y, overlay).passable) continue;
    seen.add(key); queue.push(p);
  }
  const adjacent = new Map(body.flatMap(p => hexNeighbors(p.x, p.y)).map(p => [`${p.x},${p.y}`,p]));
  const accessible = [...adjacent].filter(([key]) => seen.has(key)).map(([,p]) => p);
  assert.equal(accessible.length, 2, "only the two front approach hexes are reachable");
  assert.ok(accessible.every(p => p.y > plant.y && p.y >= plant.y - 2), "approaches lie in her sweep area");
});

test("the long decorated approach leaves ranged sight and room for sapling defenders", () => {
  const plant = room.enemySpawns.find((p: { classId: string }) => p.classId === "carnivorousPlant");
  const saplings = room.enemySpawns.filter((p: { classId: string }) => p.classId === "sapling");
  const overlay = buildDecorOverlay(room.decorations, room.cols, room.rows, placedBlockingFootprint);
  // boss005: eight defenders on the approach between the boss and the party, plus four flank
  // guards out at the sides, level with the boss
  const approach = saplings.filter((s: { y: number }) => s.y > plant.y);
  const flanks = saplings.filter((s: { y: number }) => s.y <= plant.y);
  assert.equal(approach.length, 8);
  assert.equal(flanks.length, 4);
  assert.ok(room.waterLevels.every((level: number | null) => level === null), "the entire boss clearing is dry ground");
  assert.ok(!room.tiles.includes("water"));
  for (const key of ["tiles", "tileVariants", "tileRots", "terrainElevations", "waterLevels"]) {
    assert.equal(room[key].length, room.cols * room.rows, key);
  }
  for (const spawn of room.playerSpawns) assert.ok(hexDist(spawn, plant) >= 12);
  for (const spawn of saplings) assert.ok(hexDef(room.tiles, room.cols, spawn.x, spawn.y, overlay).passable);
  for (const spawn of approach) assert.ok(spawn.y < room.playerSpawns[0].y);
  for (const spawn of flanks) assert.ok(Math.abs(spawn.x - plant.x) >= 8, "flank guards stand well off the central lane");
  // The central advance stays open, with two exposed lanes aligned to the boss's front.
  for (let y = 9; y <= 13; y++) for (let x = 14; x <= 16; x++) {
    const cell = { x, y };
    assert.ok(hexDef(room.tiles, room.cols, x, y, overlay).passable);
    assert.ok(hexDist(plant, cell) <= CAUSTIC_VENOM.range);
    if (x >= 15) assert.ok(clearShot(plant, cell, room.tiles, room.cols, "bolt", overlay));
  }
});
