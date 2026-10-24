import assert from "node:assert/strict";
import { test } from "node:test";
import { POISON_TIERS, effectivePoisonResistance, poisonTickDamage, poisonChance, poisonDice, poisonTierOf, strongerPoison } from "./poison.ts";

test("poison uses percentage resistance, fixed tier penalty and half MAG", () => {
  assert.equal(effectivePoisonResistance(70, "greater", 40), 30);
  assert.equal(effectivePoisonResistance(71, "greater", 40), 31);
  assert.equal(poisonTickDamage(30, 30), 21);
  assert.equal(poisonChance(30), 70);
});

test("negative resistance amplifies poison, while damage and chance have separate caps", () => {
  assert.equal(effectivePoisonResistance(30, "deadly", 40), -30);
  assert.equal(poisonTickDamage(30, -30), 39);
  assert.equal(effectivePoisonResistance(0, "deadly", 999), -50);
  assert.equal(poisonTickDamage(30, -50), 45);
  assert.equal(poisonTickDamage(30, 100), 0);
  assert.equal(poisonChance(-50), 100);
  assert.equal(poisonChance(100), 0);
});

test("each tier rolls its own dice", () => {
  assert.equal(poisonDice("lesser"), "1D4");
  assert.equal(poisonDice("poison"), "1D10");
  assert.equal(poisonDice("greater"), "1D10");
  assert.equal(poisonDice("deadly"), "2D8");
  assert.equal(poisonDice("lethal"), "3D6");
});

test("tiers subtract fixed percentage points", () => {
  assert.equal(POISON_TIERS.lesser.penalty, 0);
  assert.equal(POISON_TIERS.poison.penalty, 10);
  assert.equal(POISON_TIERS.greater.penalty, 20);
  assert.equal(POISON_TIERS.deadly.penalty, 40);
  assert.equal(POISON_TIERS.lethal.penalty, 50);
});

test("a weaker poison never replaces a stronger one", () => {
  assert.equal(strongerPoison(undefined, "lesser"), "lesser");
  assert.equal(strongerPoison("lesser", "deadly"), "deadly");
  assert.equal(strongerPoison("greater", "poison"), "greater");
  assert.equal(strongerPoison("deadly", "lethal"), "lethal");
  assert.equal(strongerPoison("lethal", "deadly"), "lethal");
});

test("old saves map to the matching tier", () => {
  assert.equal(poisonTierOf(true), "lesser");
  assert.equal(poisonTierOf(4), "lesser");
  assert.equal(poisonTierOf(10), "poison");
  assert.equal(poisonTierOf("deadly"), "deadly");
  assert.equal(poisonTierOf("lethal"), "lethal");
  assert.equal(poisonTierOf("nope"), undefined);
  assert.equal(poisonTierOf(undefined), undefined);
});
