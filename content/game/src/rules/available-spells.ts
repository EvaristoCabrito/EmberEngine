// Preserved pure spell availability from Ember GameApp.tsx.
import type { ClassId, SpellKind } from "../ember/types.ts";
import { rulesClass, tierUses, spellTier, BULL_RUSH_UNLOCK_LEVEL, BLESS, POISON_BREATH, BLOODY_SHOT, PROVOKE, PHANTASMAL_FORCE_UNLOCK_LEVEL, SUMMON_FAMILIAR2_UNLOCK_LEVEL, ICE_STORM } from "../ember/data.ts";
import { FROST } from "./frost.ts";
const PRESTIGE_SPELLS: Partial<Record<ClassId, SpellKind[]>> = {
  paladin: ["cureLight", "auraOfProtection", "divineWrath"],
  heavyKnight: ["shoulderSmash", "intimidatingPresence", "stampede"],
  elementalist: ["lightningTier3"],
};
export function classSpells(classId: ClassId, level = Number.POSITIVE_INFINITY, heroName?: string): SpellKind[] {
  // Promoted classes keep everything the base class already granted (hybrid, nothing
  // lost at PROMOTE_LEVEL), plus their own prestige-only spells from PRESTIGE_SPELLS.
  const base = ((): SpellKind[] => {
    switch (rulesClass(classId)) {
      case "swordsman":
        return ["doubleStrike", "provoke", "bullRush", "cleave", "shieldBash", "executionerStrike"];
      case "cultistV2": return ["frost", "magicMissile", "lightning"];
      case "mage":
        return ["magicMissile", "poisonBreath", "frost", "lightning", "fireball", "iceStorm", "causticVenom", "warp"];
      case "conjurer":
        // Summon Swarm (tiers 3-4) joins this list as it's built — see SPELL_TIER for the
        // intended tier assignment. Phantasmal Force is tier 1's own second spell (see
        // PHANTASMAL_FORCE_UNLOCK_LEVEL — it shares tier 1's pool with summonFamiliar but
        // isn't selectable/castable until level 2); summonFamiliar2 (Familiar Maior) is the
        // same deal at tier 2 (shares that tier's pool of uses with webOfDreams).
        // summonZombieDog: tier 5 here for testing — meant to become a Necromancer tier 6 spell.
        return ["summonFamiliar", "phantasmalForce", "webOfDreams", "summonFamiliar2", "summonFamiliar4", "summonFamiliar3", "summonZombieDog"];
      case "familiar":
        // Familiar's own hotbar, once summoned — Magic Missile is its only action beyond a
        // plain attack (see FAMILIAR_SPELL/familiarMagicMissileCharges).
        return ["magicMissile"];
      case "familiar2":
        // Familiar Maior's own hotbar — Magic Missile plus its own second spell, Toque
        // Vampírico (see LIFE_DRAIN/familiarLifeDrainCharges), each with an independent
        // charge pool.
        return ["magicMissile", "lifeDrain"];
      case "familiar4":
        // Familiar Radiante has three Shock casts plus Life Drain.
        return ["shock", "lifeDrain"];
      case "familiar3":
        // The Big Guy's own hotbar, once summoned — its only action beyond a plain attack.
        return ["fireball"];
      case "zombieDog":
        // Cão Zumbi's own hotbar: Veneno Menor, twice per battle (FAMILIAR_SPELL charges).
        return ["minorVenom"];
      case "archer":
        return ["longShot", "piercing", "bloodyShot", "multiShot"];
      case "healer":
        return ["cureMinor", "bless", "cureWounds", "burningHands", "cureDisease", "turnUndead", "createFoodAndWater"];
      case "lancer":
      case "aldric":
        return ["piercingThrust", "sweep", "trip"];
      default:
        return [];
    }
  })();
  const salazarOnly: SpellKind[] = heroName === "Salazar" && rulesClass(classId) === "healer" ? ["divineBolt"] : [];
  const spells = [...base, ...salazarOnly, ...(PRESTIGE_SPELLS[classId] ?? [])].filter((spell) =>
    (spell !== "frost" || classId === "cultistV2" || level >= FROST.unlockLevel) &&
    (spell !== "bullRush" || level >= BULL_RUSH_UNLOCK_LEVEL) &&
    (spell !== "bless" || level >= BLESS.unlockLevel) &&
    (spell !== "poisonBreath" || level >= POISON_BREATH.unlockLevel) &&
    (spell !== "burningHands" || level >= 5) &&
    (spell !== "bloodyShot" || level >= BLOODY_SHOT.unlockLevel) &&
    (spell !== "provoke" || level >= PROVOKE.unlockLevel) &&
    (spell !== "phantasmalForce" || level >= PHANTASMAL_FORCE_UNLOCK_LEVEL) &&
    (spell !== "summonFamiliar2" || level >= SUMMON_FAMILIAR2_UNLOCK_LEVEL) &&
    (spell !== "iceStorm" || level >= ICE_STORM.unlockLevel) &&
    (spell !== "warp" || level >= 7) &&
    (["familiar", "familiar2", "familiar3", "familiar4", "zombieDog", "cultistV2"].includes(classId) || !spellTier(spell) || tierUses(classId, spellTier(spell)!, level) > 0),
  );
  return spells;
}
