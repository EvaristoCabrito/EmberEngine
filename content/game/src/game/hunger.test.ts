import assert from "node:assert/strict";
import test from "node:test";
import { buyInnMeal, drainHunger, fullness, useRation } from "./hunger.ts";
import type { SaveData } from "./types.ts";

function save(overrides: Partial<SaveData> = {}): SaveData {
  return { rations: 2, ember: 10, unitHp: { Kael: 20 }, heroHunger: { Kael: 35 }, ...overrides } as SaveData;
}

test("travel and combat costs drain through an inn's 20% reserve", () => {
  assert.equal(drainHunger(120, 50), 70);
  assert.equal(drainHunger(3, 50), 0);
  assert.equal(drainHunger(70, 2), 68);
});

test("one ration fills one living hero to 100%", () => {
  const next = useRation(save(), "Kael");
  assert.equal(next.rations, 1);
  assert.equal(next.heroHunger.Kael, 100);
  assert.equal(useRation(next, "Kael"), next, "a ration is not wasted on a full hero");
});

test("an inn meal costs 2 gold and fills the selected hero to 120%", () => {
  const next = buyInnMeal(save(), "Kael");
  assert.equal(next.ember, 8);
  assert.equal(next.heroHunger.Kael, 120);
  assert.equal(fullness(next.heroHunger.Kael), 120);
});
