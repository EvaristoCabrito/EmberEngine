import assert from "node:assert/strict";
import { test } from "node:test";
import { WEAPONS, EQUIPMENT } from "./data.ts";
import { equippedWeaponType, trainedWeaponSkills, weaponModifiers, weaponTypesForClass, isWeaponAbility } from "./weaponSkills.ts";
import { cleanHeroSkills, skillResistances, rollWeaponSkillGain, rollSkillGain } from "./skills.ts";
import { weaponSkillAccuracy, weaponSkillDamageMultiplier } from "./weaponTypes.ts";
import { isRearAttack, makeForecast, previewDamage, rollDamage, rollDamageCustom } from "./combat.ts";
import { hexNeighbors } from "./pathfinding.ts";
import type { Unit } from "./types.ts";

const unit = (values: Partial<Unit> = {}): Unit => ({ side: "player", classId: "swordsman", atk: 40, mag: 0, def: 0, dex: 0, weaponId: "espada-larga", weaponEnh: 0, summoned: false, weaponSkills: { sword: 0 }, ...values } as Unit);

test("enemies start at 90 accuracy, rear attacks at 100, and defender DEX still applies", () => {
  const enemy = unit({ side: "enemy", classId: "soldier", weaponId: null, x: 3, y: 4 });
  const defender = unit({ x: 3, y: 3, faceDx: 0, faceDy: 1 });
  for (const dex of [0, 5, 20]) {
    const front = { ...defender, dex };
    const rear = { ...front, faceDy: -1 };
    assert.equal(isRearAttack(enemy, front), false);
    assert.equal(isRearAttack(enemy, rear), true);
    assert.equal(rollDamage(enemy, front, "plains", "plains", () => .95).hitChance, 90 - dex);
    assert.equal(rollDamage(enemy, front, "plains", "plains", () => .95).landed, false);
    assert.equal(previewDamage(enemy, rear, "plains", "plains").hitChance, 100 - dex);
    assert.equal(rollDamageCustom(enemy, rear, "plains", "plains", 1, 4, 0, () => .5).hitChance, 100 - dex);
  }
  assert.equal(rollDamage(enemy, { ...defender, faceDy: -1 }, "plains", "plains", () => .99).landed, true);
  assert.equal(rollDamage(enemy, { ...defender, dex: 20 }, "plains", "plains", () => .99, false).hitChance, 100);
  assert.equal(weaponModifiers(unit(), "sword").accuracy, 75);
});

test("enemy DEX adds accuracy before rear bonuses, defender DEX and the final cap", () => {
  const enemy = unit({ side: "enemy", classId: "soldier", weaponId: null, x: 3, y: 4, dex: 7 });
  const defender = unit({ x: 3, y: 3, faceDx: 0, faceDy: 1, dex: 12 });
  assert.equal(previewDamage(enemy, defender, "plains", "plains").hitChance, 85);
  assert.equal(rollDamage(enemy, defender, "plains", "plains", () => .9).landed, false);
  const rear = { ...defender, faceDy: -1 };
  assert.equal(rollDamage(enemy, rear, "plains", "plains", () => .9).hitChance, 95);
  assert.equal(rollDamageCustom(enemy, rear, "plains", "plains", 1, 4, 0, () => .9).landed, true);
  assert.equal(previewDamage({ ...enemy, dex: 30 }, defender, "plains", "plains").hitChance, 100);
  assert.equal(previewDamage({ ...enemy, dex: 2.5 }, { ...defender, dex: 5.2 }, "plains", "plains").hitChance, 87.3);
  assert.equal(weaponModifiers(unit({ dex: 50 }), "sword").accuracy, 75);
});

test("exactly two rear hexes grant ten accuracy points and ten percent weapon damage", () => {
  for (const y of [2, 3]) {
    const defender = unit({ x: 3, y, faceDx: 0, faceDy: -1, dex: 30 });
    const neighbors = hexNeighbors(3, y);
    const rear = neighbors.filter(p => isRearAttack(unit(p), defender));
    assert.equal(rear.length, 2);
    assert.ok(rear.every(p => p.y === y + 1));
    const frontDefender = { ...defender, faceDy: 1 };
    for (const position of rear) {
      const attacker = unit({ ...position, atk: 100 });
      const front = rollDamage(attacker, frontDefender, "plains", "plains", () => .5);
      const back = rollDamage(attacker, defender, "plains", "plains", () => .5);
      assert.equal(back.hitChance, front.hitChance + 10);
      assert.equal(back.preCritDmg, Math.floor(front.preCritDmg * 1.1));
      assert.equal(previewDamage(attacker, defender, "plains", "plains").hitChance, back.hitChance);
      const offhand = { ...attacker, classId: "archer" as const, offHandId: "punhal-curvo", weaponSkills: { dagger: 20 } };
      const customFront = rollDamageCustom(offhand, frontDefender, "plains", "plains", 1, 4, 0, () => .5);
      const customBack = rollDamageCustom(offhand, defender, "plains", "plains", 1, 4, 0, () => .5);
      assert.equal(customBack.hitChance, customFront.hitChance + 10);
      assert.ok(customBack.preCritDmg > customFront.preCritDmg);
      assert.equal(previewDamage(offhand, defender, "plains", "plains", true).hitChance, customBack.hitChance);
    }
    assert.equal(isRearAttack(unit({ x: 3, y: y + 2 }), defender), false);
    assert.equal(previewDamage(unit(rear[0]), { ...defender, dex: 0 }, "plains", "plains").hitChance, 85);
    assert.equal(previewDamage(unit({ ...rear[0], weaponSkills: { sword: 100 } }), defender, "plains", "plains").hitChance, 100);
  }
});

test("each character's pool follows main-hand and off-hand equipment permissions", () => {
  assert.deepEqual(weaponTypesForClass("swordsman"), ["sword", "axe", "mace", "hammer"]);
  assert.deepEqual(weaponTypesForClass("archer"), ["bow", "crossbow", "dagger"]);
  assert.deepEqual(weaponTypesForClass("mage"), ["staff"]);
  // Salazar (healer/salazar classes) fights with maces and hammers as well as staves.
  assert.deepEqual(weaponTypesForClass("healer"), ["mace", "hammer", "staff"]);
  assert.deepEqual(weaponTypesForClass("salazar"), ["mace", "hammer", "staff"]);
  assert.deepEqual(weaponTypesForClass("aldric"), ["spear"]);
  assert.deepEqual(weaponTypesForClass("conjurer"), ["staff"]);
  assert.deepEqual(weaponTypesForClass("cleric"), ["mace", "hammer", "staff"]);
});

test("every main-hand and off-hand weapon has an explicit proficiency type", () => {
  for (const weapon of Object.values(WEAPONS)) assert.ok(weapon.weaponType, weapon.id);
  for (const item of Object.values(EQUIPMENT)) if (item.kind === "weapon") assert.ok(item.weaponType, item.id);
});

test("skill grants precise accuracy and damage improvements, bounded at 100", () => {
  assert.equal(weaponSkillAccuracy(0), 75);
  assert.equal(weaponSkillAccuracy(50), 125);
  assert.equal(weaponSkillAccuracy(100), 175);
  assert.equal(weaponSkillDamageMultiplier(0), 1);
  assert.equal(weaponSkillDamageMultiplier(50), 1.5);
  assert.equal(weaponSkillDamageMultiplier(100), 2);
  assert.equal(weaponSkillDamageMultiplier(999), 2);
});

test("forecast matches damage and accuracy; untrained attacks can miss, mastered attacks cannot", () => {
  const defender = unit({ side: "enemy", weaponId: null });
  const attacker = unit();
  const forecast = previewDamage(attacker, defender, "plains", "plains");
  const miss = rollDamage(attacker, defender, "plains", "plains", () => 0.9);
  assert.equal(forecast.hitChance, 75);
  assert.equal(miss.hitChance, 75);
  assert.equal(miss.landed, false);
  const master = unit({ weaponSkills: { sword: 100 } });
  const hit = rollDamage(master, defender, "plains", "plains", () => 0.9);
  assert.equal(hit.landed, true);
  assert.equal(hit.hitChance, 100);
  assert.ok(hit.dmg > miss.dmg);
  assert.equal(previewDamage(master, defender, "plains", "plains").hitChance, hit.hitChance);
  assert.deepEqual(weaponModifiers(unit({ side: "enemy" }), "sword"), { accuracy: 90, damage: 1 });
  assert.equal(weaponModifiers(unit({ blessedHitBonusPct: 0.1 }), "sword").accuracy, 85);
});

test("off-hand dagger uses dagger mastery instead of the bow's mastery", () => {
  const archer = unit({ classId: "archer", weaponId: "arco-composto", offHandId: "punhal-curvo", weaponSkills: { bow: 100, dagger: 0 } });
  assert.equal(equippedWeaponType(archer), "bow");
  assert.equal(equippedWeaponType(archer, true), "dagger");
  const hit = rollDamageCustom(archer, unit({ side: "enemy" }), "plains", "plains", 1, 4, 0, () => 0.9);
  assert.equal(hit.hitChance, 75);
  assert.equal(hit.landed, false);
  assert.equal(equippedWeaponType(unit({ weaponId: "arco-composto" })), undefined);
});

test("saved weapon skills remain within the class pool and never become resistances", () => {
  const clean = cleanHeroSkills({ Kael: { swordWeapon: 25.4, bowWeapon: 90, fireResistance: 30 } }, ["Kael"], (_, type) => weaponTypesForClass("swordsman").includes(type), true);
  assert.equal(clean.Kael.swordWeapon, 26);
  assert.equal(clean.Kael.bowWeapon, undefined);
  assert.equal(trainedWeaponSkills(clean, "Kael", "swordsman").sword, 26);
  assert.equal(skillResistances(clean, "Kael").fire, 30);
  assert.equal(Object.keys(skillResistances(clean, "Kael")).length, 8);
  assert.equal(isWeaponAbility("longShot"), true);
  assert.equal(isWeaponAbility("cleave"), true);
  assert.equal(isWeaponAbility("magicMissile"), false);
  assert.equal(isWeaponAbility("shieldBash"), false);
});

test("weapon and resistance progression gain tenths and preserve smaller weapon gains", () => {
  assert.equal(rollWeaponSkillGain(20, () => 0), 20.1);
  assert.equal(rollWeaponSkillGain(99, () => 0), 99.1);
  assert.equal(rollWeaponSkillGain(20, () => 0, 0.05), 20.05);
  assert.equal(rollWeaponSkillGain(99.95, () => 0), 100);
  const clean = cleanHeroSkills({ Kael: { swordWeapon: 20.1 } }, ["Kael"]);
  assert.equal(trainedWeaponSkills(clean, "Kael", "swordsman").sword, 20.1);
  assert.equal(rollWeaponSkillGain(99, () => 0.5), null);
  assert.equal(rollWeaponSkillGain(100, () => 0), null);
  assert.equal(rollSkillGain(20, () => 0), 20.1);
});

test("each skill point contributes its exact dice percentage", () => {
  for (const [skill, result] of [[0,20],[1,20.2],[10,22],[50,30],[100,40]]) assert.equal(20 * weaponSkillDamageMultiplier(skill), result);
  assert.notEqual(weaponSkillDamageMultiplier(20), weaponSkillDamageMultiplier(21));
});

test("mastery boosts only weapon dice, leaving flat bonuses, enhancement, ATK and protection separate", () => {
  const id = "__weapon_dice_test";
  WEAPONS[id] = { ...WEAPONS["espada-larga"], id, dice: 1, faces: 20, bonus: 7 };
  try {
    const defender = unit({ side: "enemy", def: 8 });
    const attacker = unit({ weaponId: id, weaponEnh: 3, atk: 40, weaponSkills: { sword: 50 } });
    // Rolled 20, boosted to 30; power 20 + flat 7 + enhancement 3 - protection 4 = 56.
    const hit = rollDamage(attacker, defender, "plains", "plains", () => 0.99);
    assert.equal(hit.preCritDmg, 56);
    assert.equal(hit.dmg, 56);
    assert.equal(rollDamage({ ...attacker, dex: 99 }, defender, "plains", "plains", () => 0.99).dmg, 56);
    const average = previewDamage(attacker, defender, "plains", "plains");
    assert.equal(average.dmg, Math.floor(10.5 * 1.5 + 20 + 7 + 3 - 4));
    assert.equal(rollDamage({ ...attacker, mag: 40 }, defender, "plains", "plains", () => 0.99).dmg, 60);
  } finally { delete WEAPONS[id]; }
});

test("pure spells and shield bashes bypass mastery and weapon dodge", () => {
  const attacker = unit({ weaponSkills: { sword: 100, dagger: 100 }, offHandId: "punhal-curvo" });
  const defender = unit({ side: "enemy", dex: 999 });
  assert.deepEqual(rollDamage(attacker, defender, "plains", "plains", () => 0.9, false), rollDamage({ ...attacker, weaponSkills: {} }, defender, "plains", "plains", () => 0.9, false));
  const bash = rollDamageCustom(attacker, defender, "plains", "plains", 1, 20, 7, () => 0.99, false);
  assert.equal(bash.preCritDmg, 47);
  assert.equal(bash.hitChance, 100);
});

test("high skill and Bless are not clamped until defender DEX is subtracted", () => {
  const attacker = unit({ weaponSkills: { sword: 100 }, blessedHitBonusPct: 0.1 });
  assert.equal(weaponModifiers(attacker, "sword").accuracy, 185);
  assert.equal(previewDamage(attacker, unit({ dex: 100 }), "plains", "plains").hitChance, 85);
  assert.equal(rollDamage(attacker, unit({ dex: 100 }), "plains", "plains", () => 0.9).hitChance, 85);
});

test("off-hand counter forecast selects dagger dice and proficiency; nonweapon forecasts bypass them", () => {
  const attacker = unit({ id: "att", x: 0, y: 0, hp: 500, alive: true, minRange: 1, maxRange: 1 });
  const defender = unit({ id: "def", x: 1, y: 0, hp: 500, alive: true, classId: "archer", weaponId: "arco-composto", offHandId: "punhal-curvo", minRange: 2, maxRange: 3, weaponSkills: { bow: 100, dagger: 20 } });
  const forecast = makeForecast(attacker, defender, "plains", "plains", ["plains", "plains"], 2);
  assert.equal(forecast.canCounter, true);
  assert.equal(forecast.hitBack, 95);
  assert.equal(forecast.dmgBack, previewDamage(defender, attacker, "plains", "plains", true).dmg);
  const bash = makeForecast(attacker, defender, "plains", "plains", ["plains", "plains"], 2, false, false);
  assert.equal(bash.hitOut, 100);
  assert.equal(bash.dmgOut, previewDamage(attacker, defender, "plains", "plains", false, false).dmg);
});
