import assert from "node:assert/strict";
import { test } from "node:test";
import { cleanResistances, effectiveResistance, elementalDamage, RESISTANCE_ELEMENTS, spellElement, sumResistances } from "./resistances.ts";
import { cleanHeroSkills, rollSkillGain, skillResistances, SKILL_IDS } from "./skills.ts";

test("percentage damage accounts for penetration, vulnerability and immunity", () => {
  assert.equal(effectiveResistance(70, 40), 50);
  assert.equal(elementalDamage(30, 70, 40), 15);
  assert.ok(Math.abs(elementalDamage(30, 71, 40) - 14.7) < 1e-10);
  assert.equal(elementalDamage(30, 20, 80), 36);
  assert.equal(elementalDamage(30, 0, 999), 45);
  assert.equal(elementalDamage(30, 120, 40), 0);
  assert.equal(effectiveResistance(140, 100), 90);
});

test("eight independent resistances combine class, skill and gear without cross-element leakage", () => {
  const combined = sumResistances({ fire: 10, ember: 5 }, { fire: 20, ice: 40 }, { lightning: 30 });
  assert.equal(RESISTANCE_ELEMENTS.length, 8);
  assert.deepEqual(combined, { fire: 30, lightning: 30, ice: 40, arcane: 0, darkness: 0, holy: 0, poison: 0, ember: 5 });
});

test("damage spells use their matching resistance while weapon skills and healing stay unaffected", () => {
  assert.equal(spellElement("fireball"), "fire");
  assert.equal(spellElement("burningHands"), "fire");
  assert.equal(spellElement("lightningTier3"), "lightning");
  assert.equal(spellElement("magicMissileV2"), "arcane");
  assert.equal(spellElement("lifeDrain"), "darkness");
  assert.equal(spellElement("divineWrath"), "holy");
  assert.equal(spellElement("poisonBreath"), "poison");
  assert.equal(spellElement("cleave"), undefined);
  assert.equal(spellElement("cureMinor"), undefined);
  assert.equal(spellElement(null), undefined);
});

test("old and malformed saves default safely without erasing fractional resistance", () => {
  assert.equal(cleanResistances(undefined), undefined);
  const clean = cleanResistances({ fire: 70.1, lightning: NaN, ice: Infinity, holy: "100", poison: -20 });
  assert.equal(clean?.fire, 70.1);
  assert.equal(clean?.lightning, 0);
  assert.equal(clean?.ice, 0);
  assert.equal(clean?.holy, 0);
  assert.equal(clean?.poison, -20);
});

test("each resistance trains independently in tenths and persists through skill cleaning", () => {
  assert.equal(SKILL_IDS.filter(id => id.endsWith("Resistance")).length, 8);
  assert.equal(rollSkillGain(0, () => 0), 0.1);
  assert.equal(rollSkillGain(99.9, () => 0), 100);
  assert.equal(rollSkillGain(100, () => 0), null);
  const skills = cleanHeroSkills({ Kael: { poisonResistance: 70.1, fireResistance: 12.3, holyResistance: 120 } }, ["Kael"]);
  const values = skillResistances(skills, "Kael");
  assert.equal(values.poison, 70.1);
  assert.equal(values.fire, 12.3);
  assert.equal(values.holy, 100);
  assert.equal(values.ice, 0);
});

test("Healing keeps fractional percentages and persists independently of resistances", async () => {
  const { healingAmount, skillValue } = await import("./skills.ts");
  assert.equal(healingAmount(1000, 11.4), 1114);
  assert.equal(healingAmount(9, 11.4), 10);
  assert.equal(healingAmount(9, 0), 9);
  assert.equal(healingAmount(9, 100), 18);
  const saved = cleanHeroSkills({ Kael: { healing: 11.4, arcaneResistance: 2.3 } }, ["Kael"]);
  assert.equal(skillValue(saved, "Kael", "healing"), 11.4);
  assert.equal(skillValue(saved, "Neera", "healing"), 0);
  assert.equal(saved.Kael.arcaneResistance, 2.3);
});
