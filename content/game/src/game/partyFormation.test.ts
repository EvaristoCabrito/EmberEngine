import assert from "node:assert/strict";
import { test } from "node:test";
import { applyPartyFormation, cleanPartyFormation } from "./partyFormation.ts";
import type { Mission } from "./types";

const mission = {
  playerSpawns: [
    { name: "Kael", classId: "kaelFinal", x: 2, y: 4 },
    { name: "Neera", classId: "neera", x: 3, y: 4 },
    { name: "Voss", classId: "voss", x: 2, y: 5 },
    { name: "Familiar", classId: "familiar", x: 1, y: 4 },
  ],
} as Mission;

test("formation permutes existing hero slots and preserves allied creatures", () => {
  const result = applyPartyFormation(mission, ["Voss", "Kael", "Neera"]);
  assert.deepEqual(result.playerSpawns.map(s => [s.name, s.x, s.y]), [
    ["Kael", 3, 4], ["Neera", 2, 5], ["Voss", 2, 4], ["Familiar", 1, 4],
  ]);
  assert.equal(result.playerSpawns[3], mission.playerSpawns[3]);
  assert.equal(mission.playerSpawns[0].x, 2);
});

test("scripted, dangerous and free-roam starts retain authored positions", () => {
  const order = ["Voss", "Kael", "Neera"];
  assert.equal(applyPartyFormation(mission, order, true), mission);
  const locked = { ...mission, lockPartyFormation: true };
  assert.equal(applyPartyFormation(locked, order), locked);
  const explore = { ...mission, explore: true };
  assert.equal(applyPartyFormation(explore, order), explore);
  assert.equal(applyPartyFormation(mission, undefined), mission);
});

test("new or absent members never create spawns or duplicate starting positions", () => {
  assert.deepEqual(cleanPartyFormation(["Neera", "Neera", "Unknown", "Kael"]), ["Neera", "Kael"]);
  const result = applyPartyFormation(mission, ["Malrec", "Neera", "Kael"]);
  assert.equal(result.playerSpawns.length, mission.playerSpawns.length);
  assert.equal(new Set(result.playerSpawns.map(s => `${s.x},${s.y}`)).size, 4);
  assert.equal(result.playerSpawns.some(s => s.name === "Malrec"), false);
});

test("formation slot 1 takes the start hex closest to the enemy, the last slot the farthest", () => {
  const withEnemy = { ...mission, enemySpawns: [{ name: "Foe", classId: "soldier", x: 2, y: 8 }] } as Mission;
  const result = applyPartyFormation(withEnemy, ["Neera", "Kael", "Voss"]);
  // Voss's authored hex (2,5) is closest to the enemy, then (2,4), then (3,4).
  assert.deepEqual(result.playerSpawns.slice(0, 3).map(s => [s.name, s.x, s.y]), [
    ["Kael", 2, 4], ["Neera", 2, 5], ["Voss", 3, 4],
  ]);
});
