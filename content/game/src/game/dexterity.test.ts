import assert from "node:assert/strict";
import { test } from "node:test";
import { dexAccuracy, dexEscapeChance, savedDex } from "./dexterity.ts";
import { protOf } from "./combat.ts";
import type { Unit } from "./types.ts";

test("every defender DEX point subtracts a hit percentage point; attacker DEX is irrelevant", () => {
  for (const [skill, dex, expected] of [[0,0,75],[20,20,75],[21,20,76],[21,21,75],[0,999,0],[100,50,100],[100,90,85]]) {
    assert.equal(dexAccuracy(75 + skill, dex), expected);
  }
  assert.equal(dexAccuracy(175 + 10, 100), 85);
});

test("escape retains the exact DEX/3 bonus and reaches 100%", () => {
  for (const [dex, chance] of [[0,60],[3,61],[15,65],[30,70],[60,80],[120,100],[999,100]]) assert.equal(dexEscapeChance(dex), chance);
  assert.equal(dexEscapeChance(1), 60 + 1/3);
});

test("legacy RES and allocated points migrate without overwriting an explicit DEX", () => {
  assert.equal(savedDex({ res: 20 }), 20);
  assert.equal(savedDex({ res: 20, dex: 15 }), 15);
  assert.equal(savedDex({ res: 20, dex: 0 }), 0);
});

test("DEX is never used as flat magical protection", () => {
  const defender = { def: 20, dex: 99 } as Unit;
  assert.equal(protOf({ mag: 40 } as Unit, defender), 0);
  assert.equal(protOf({ mag: 0 } as Unit, defender), 10);
});
