import { CLASSES, EQUIPMENT, MAX_LEVEL, PROMOTIONS, PROMOTE_LEVEL, STAT_POINTS_PER_LEVEL, expToLevel, gearStatBonus, heroRecruited, statsFor, spellUseGains, tierUses, WEAPONS, partyBagHasRoom } from '../ember/data';
import type { ClassId, SaveData, StatPointAttribute } from '../ember/types';
import { cleanPartyFormation, cleanPartyLeader } from './partyFormation';
import { advanceProgression, isGatedMission, missionAccess } from './progression';
import { lockedMission, progressionExtras } from './access';
import { ALL_LOCATIONS, ALL_MISSIONS, missionById, keepsDefeatedSpawns } from './mapstore';
import { completedAfterWispVictory } from './wispCrossing';
import { hungerPenaltyFor } from './overworld';
import { fullness } from './hunger';

export const HERO_BASE_CLASS: Record<string, ClassId> = { Kael: 'swordsman', Neera: 'archer', Voss: 'mage', Salazar: 'healer', Aldric: 'aldric', Malrec: 'conjurer' };
export function partyMembers(save: SaveData): string[] {
  return Object.keys(HERO_BASE_CLASS).filter(hero => heroRecruited(hero, save.completed, save.flags));
}
export function heroClass(save: SaveData, hero: string): ClassId {
  const cls = hero === 'Kael' ? 'swordsman' : save.promotions[hero] ?? HERO_BASE_CLASS[hero];
  if (!cls || !CLASSES[cls]) throw new Error(`Unknown hero: ${hero}`);
  return cls;
}
export function canStartMission(save: SaveData, id: string, test = false): boolean {
  const mission = missionById(id);
  if (!mission) return false;
  if (!test && isGatedMission(id) && missionAccess(id, save, progressionExtras(save)) !== 'available') return false;
  if (!test && save.completed.includes(id)) return keepsDefeatedSpawns(mission);
  return !lockedMission(id, save.completed, test, ALL_LOCATIONS, ALL_MISSIONS.map(m => m.id), id => missionAccess(id, save, progressionExtras(save), test));
}
export function setPartyFormation(save: SaveData, formation: unknown): SaveData {
  return { ...save, partyFormation: cleanPartyFormation(formation) };
}
export function setPartyLeader(save: SaveData, hero: unknown): SaveData {
  const leader = cleanPartyLeader(hero);
  return leader && partyMembers(save).includes(leader) ? { ...save, partyLeader: leader } : save;
}
/** Same variable XP bands and partial healing as BattleEngine.addExp/levelUpUnit. */
export function awardExperience(save: SaveData, hero: string, amount: number): SaveData {
  if (!Number.isFinite(amount) || amount <= 0 || !partyMembers(save).includes(hero)) return save;
  let level = save.levels[hero] ?? 1;
  if (level >= MAX_LEVEL) return save;
  const from = level;
  const cls = heroClass(save, hero);
  const allocatedHp = save.statPointAllocations[hero]?.hp ?? 0;
  const gearHp = gearStatBonus(Object.values(save.equipment[hero] ?? {})).hp;
  const hungerKeep = fullness(save.heroHunger[hero]) <= 0 ? 1 - hungerPenaltyFor(save.hungerStreak) : 1;
  const initialMaxHp = Math.round((statsFor(cls, from).hp + allocatedHp + gearHp) * hungerKeep);
  let hp = Math.min(initialMaxHp, save.unitHp[hero] ?? initialMaxHp);
  let xp = (save.xp[hero] ?? 0) + amount;
  while (level < MAX_LEVEL && xp >= expToLevel(level)) {
    xp -= expToLevel(level);
    const before = statsFor(cls, level);
    level++;
    const after = statsFor(cls, level);
    // Keep Ember's actual operation order: base/allocated cap, then reapplyGear.
    hp = Math.min(after.hp + allocatedHp, hp + after.hp - before.hp);
    hp = Math.min(Math.round((after.hp + allocatedHp + gearHp) * hungerKeep), hp);
  }
  if (level >= MAX_LEVEL) xp = 0;
  const spent = { ...save.spellUses[hero] };
  // The save stores spent uses; grant only the newly gained capacity, retaining expenditure.
  for (const gain of spellUseGains(cls, from, level)) {
    const remainingBefore = tierUses(cls, gain.tier, from) - (spent[gain.key] ?? 0);
    spent[gain.key] = Math.max(0, tierUses(cls, gain.tier, level) - Math.min(tierUses(cls, gain.tier, level), remainingBefore + gain.gain));
  }
  return { ...save, levels: { ...save.levels, [hero]: level }, xp: { ...save.xp, [hero]: xp }, unitHp: { ...save.unitHp, [hero]: hp }, spellUses: { ...save.spellUses, [hero]: spent } };
}
export function promoteHero(save: SaveData, hero: string, cls: ClassId): SaveData {
  const options = PROMOTIONS[HERO_BASE_CLASS[hero]];
  if ((save.levels[hero] ?? 1) < PROMOTE_LEVEL || save.promotions[hero] || !options?.includes(cls)) return save;
  return { ...save, promotions: { ...save.promotions, [hero]: cls } };
}
export function spendStatPoint(save: SaveData, hero: string, stat: StatPointAttribute, delta: 1 | -1): SaveData {
  if (!partyMembers(save).includes(hero) || !['hp', 'atk', 'mag', 'def', 'dex'].includes(stat)) return save;
  const allocation = { ...save.statPointAllocations[hero] };
  const used = Object.values(allocation).reduce((n, value) => n + (value ?? 0), 0);
  if (delta === 1 && used >= ((save.levels[hero] ?? 1) - 1) * STAT_POINTS_PER_LEVEL) return save;
  if (delta === -1 && (allocation[stat] ?? 0) < 1) return save;
  allocation[stat] = (allocation[stat] ?? 0) + delta;
  return { ...save, statPointAllocations: { ...save.statPointAllocations, [hero]: allocation } };
}

export interface BattleCampaignResult {
  missionId: string;
  victory: boolean;
  heroes: Pick<SaveData, 'unitHp' | 'levels' | 'xp' | 'bags' | 'spellUses'> & Partial<Pick<SaveData, 'heroHunger' | 'heroDiseases' | 'heroPoisons' | 'heroPoisonMag' | 'heroSkills' | 'affinityScores'>>;
  defeatedSpawnIds?: string[];
  questItems?: string[];
  questKills?: string[];
  lootWeapons?: string[];
  lootEquipment?: string[];
  /** Already resolved by battle rules: never roll drops again during campaign persistence. */
  ember?: number;
  rations?: number;
}
/** Commit renderer-independent battle results once per battle run, as Ember's victory guard does.
 * Caller supplies live battle totals/rewards; revisited dungeons can yield fresh results. */
export function finishBattle(save: SaveData, result: BattleCampaignResult): { save: SaveData; rejectedLoot: string[] } {
  const rejectedLoot: string[] = [];
  let next: SaveData = { ...save, ...structuredClone(result.heroes), pendingMission: null, battle: null };
  if (!result.victory) return { save: next, rejectedLoot };
  next = { ...next, completed: completedAfterWispVictory(result.missionId, save.completed), crossingDefeatedSpawns: { ...save.crossingDefeatedSpawns }, questItems: [...new Set([...(save.questItems ?? []), ...(result.questItems ?? [])])], questKills: [...new Set([...(save.questKills ?? []), ...(result.questKills ?? [])])], weapons: { ...save.weapons }, looseEquipment: { ...save.looseEquipment }, ember: save.ember + (result.ember ?? 0), rations: save.rations + (result.rations ?? 0), emberSeeded: true };
  const mission = missionById(result.missionId);
  // Caller emits stable defeated ids only for persistent dungeon encounters.
  if (result.defeatedSpawnIds && mission && keepsDefeatedSpawns(mission)) next.crossingDefeatedSpawns[result.missionId] = [...new Set([...(save.crossingDefeatedSpawns[result.missionId] ?? []), ...result.defeatedSpawnIds])];
  for (const id of result.lootWeapons ?? []) {
    if (!WEAPONS[id] || next.weapons[id] != null) continue;
    const probe = { ...next, weapons: { ...next.weapons, [id]: 0 } };
    if (!partyBagHasRoom(probe, 1)) { rejectedLoot.push(id); continue; }
    next.weapons[id] = 0;
  }
  for (const id of result.lootEquipment ?? []) {
    if (!EQUIPMENT[id]) continue;
    const probe = { ...next, looseEquipment: { ...next.looseEquipment, [id]: (next.looseEquipment[id] ?? 0) + 1 } };
    if (!partyBagHasRoom(probe, 1)) { rejectedLoot.push(id); continue; }
    next.looseEquipment[id] = (next.looseEquipment[id] ?? 0) + 1;
  }
  return { save: advanceProgression(next, progressionExtras(next)), rejectedLoot };
}
