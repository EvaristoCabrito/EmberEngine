import type { SpellKind } from '../ember/types';
import { spellIcon } from '../ember/data';

/** Spell-to-icon mapping preserved from Ember's battle hotbar. Several skills deliberately
 * share one icon there too; the ids below keep those established choices intact. */
export const SPELL_ICON_ID: Readonly<Record<SpellKind, string>> = {
  fireball: 'fireball', iceStorm: 'ice-storm', frost: 'frost', bless: 'bless',
  cureMinor: 'cure-minor', cureWounds: 'cure-wounds', cureLight: 'cure-light', cureDisease: 'cure-disease',
  longShot: 'long-shot', bloodyShot: 'bloody-shot', provoke: 'provoke', piercing: 'piercing',
  lightning: 'lightning', lightningTier3: 'lightning', magicMissile: 'magic-missile', magicMissileV2: 'magic-missile',
  causticVenom: 'caustic-venom', divineBolt: 'divine-bolt', minorVenom: 'caustic-venom',
  doubleStrike: 'cleave-crossed-blades', cleave: 'cleave', piercingThrust: 'piercing-thrust', sweep: 'sweep', trip: 'trip',
  summonFamiliar: 'summon-familiar', phantasmalForce: 'phantasmal-force', fantomForce: 'phantasmal-force',
  summonFamiliar2: 'summon-familiar2', summonFamiliar3: 'summon-familiar3', summonFamiliar4: 'summon-familiar4',
  summonZombieDog: 'summon-zombie-dog', lifeDrain: 'life-drain', webOfDreams: 'web-of-dreams', warp: 'warp',
  multiShot: 'multi-shot', secondWind: 'cure-light', auraOfProtection: 'aura-of-protection',
  divineWrath: 'divine-wrath', shoulderSmash: 'shoulder-smash', intimidatingPresence: 'intimidating-presence',
  stampede: 'stampede', shock: 'lightning', bullRush: 'bull-rush', executionerStrike: 'executioner-strike',
  shieldBash: 'shield-bash', poisonBreath: 'poison-breath', tendrilSwipe: 'sweep', burningHands: 'burning-hands',
  turnUndead: 'turn-undead', createFoodAndWater: 'create-food-and-water',
};

export function spellIconFor(kind: SpellKind): string {
  return spellIcon(SPELL_ICON_ID[kind]);
}

export function spellLabel(kind: SpellKind): string {
  return kind.replace(/([a-z])([A-Z])/g, '$1 $2').replace(/^./, (letter) => letter.toUpperCase());
}
