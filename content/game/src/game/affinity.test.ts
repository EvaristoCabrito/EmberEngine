import assert from "node:assert/strict";
import { test } from "node:test";
import { affinityBonus, affinityScore, canUseAffinityDuo, canUseAffinityUltimate, changeAffinity, cleanAffinityScores } from "./affinity.ts";

test("adjacent actions retain tenths through save/load and reversed pairs", () => {
  let scores = {};
  for (let i = 0; i < 249; i++) scores = changeAffinity(scores, "Kael", "Neera", 0.1);
  scores = cleanAffinityScores(JSON.parse(JSON.stringify(scores)));
  assert.equal(affinityScore(scores, "Neera", "Kael"), 24.9);
  assert.equal(affinityBonus(affinityScore(scores, "Kael", "Neera")), 0);
  scores = changeAffinity(scores, "Neera", "Kael", 0.1);
  assert.equal(affinityScore(scores, "Kael", "Neera"), 25);
  assert.equal(affinityBonus(25), 0.02);
});

test("dialogue, healing and collateral damage preserve fractional progress and bounds", () => {
  let scores = changeAffinity({}, "Kael", "Voss", 3.1);
  scores = changeAffinity(scores, "Voss", "Kael", 1);
  scores = changeAffinity(scores, "Kael", "Voss", -1);
  assert.equal(affinityScore(scores, "Kael", "Voss"), 3.1);
  scores = changeAffinity(scores, "Kael", "Voss", -3);
  assert.equal(affinityScore(scores, "Kael", "Voss"), 0.1);
  assert.equal(affinityScore(changeAffinity(scores, "Kael", "Voss", -3), "Kael", "Voss"), 0);
  assert.equal(affinityScore(changeAffinity(scores, "Kael", "Voss", 150), "Kael", "Voss"), 100);
  assert.deepEqual(changeAffinity({}, "Kael", "Kael", 1), {});
});

test("bonus and duo gates use the exact thresholds", () => {
  assert.equal(affinityBonus(49.9), 0.02);
  assert.equal(affinityBonus(50), 0.05);
  assert.equal(affinityBonus(89.9), 0.05);
  assert.equal(affinityBonus(90), 0.08);
  assert.equal(affinityBonus(100), 0.08);
  assert.equal(canUseAffinityDuo(changeAffinity({}, "Kael", "Neera", 79.9), "Neera", "Kael"), false);
  assert.equal(canUseAffinityDuo(changeAffinity({}, "Kael", "Neera", 80), "Neera", "Kael"), true);
});

test("a trio requires three distinct heroes and all three relationships at maximum", () => {
  let scores = changeAffinity({}, "Kael", "Neera", 100);
  scores = changeAffinity(scores, "Kael", "Voss", 100);
  scores = changeAffinity(scores, "Voss", "Neera", 99.9);
  assert.equal(canUseAffinityUltimate(scores, ["Kael", "Neera", "Voss"]), false);
  scores = changeAffinity(scores, "Neera", "Voss", 0.1);
  assert.equal(canUseAffinityUltimate(scores, ["Voss", "Neera", "Kael"]), true);
  assert.equal(canUseAffinityUltimate(scores, ["Kael", "Kael", "Voss"]), false);
});
