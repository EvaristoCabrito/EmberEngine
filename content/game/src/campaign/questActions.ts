import { POTION_CARRY_MAX } from '../ember/data';
import type { SaveData } from '../ember/types';
import { progressionExtras } from './access';
import { evaluate } from './progression';
import { questById, questStatus, questsFor, type QuestNpcId } from './quests';

export function talkToNpc(save: SaveData, npcId: QuestNpcId): SaveData {
  let next = save;
  if (!(save.npcTalked ?? []).includes(npcId)) next = { ...next, npcTalked: [...(next.npcTalked ?? []), npcId] };
  const offered = questsFor(npcId).filter(quest => questStatus(next, quest) === 'available' && evaluate(quest.availability, next, progressionExtras(next)) && !(next.questsDiscovered ?? []).includes(quest.id));
  if (offered.length) next = { ...next, questsDiscovered: [...(next.questsDiscovered ?? []), ...offered.map(quest => quest.id)] };
  return next;
}
export function acceptQuest(save: SaveData, id: string): SaveData | null {
  const quest = questById(id);
  if (!quest || questStatus(save, quest) !== 'available') return null;
  return { ...save, questsActive: [...(save.questsActive ?? []), id] };
}
export function turnInQuest(save: SaveData, id: string): SaveData | null {
  const quest = questById(id);
  if (!quest || questStatus(save, quest) !== 'ready') return null;
  const bags = Object.fromEntries(Object.entries(save.bags).map(([hero, bag]) => [hero, { ...bag }]));
  for (const kind of quest.rewardPotions) {
    const heroes = Object.keys(bags);
    const order = kind === 'manaMid' || kind === 'manaSmall' || kind === 'manaLarge' ? [...heroes].sort((a, b) => Number(b === 'Voss' || b === 'Salazar') - Number(a === 'Voss' || a === 'Salazar')) : heroes;
    const taker = order.find(hero => (bags[hero]![kind] ?? 0) < POTION_CARRY_MAX[kind]);
    if (taker) bags[taker]![kind] = (bags[taker]![kind] ?? 0) + 1;
  }
  return { ...save, bags, ember: save.ember + quest.reward, emberSeeded: true, questsActive: (save.questsActive ?? []).filter(questId => questId !== id), questsDone: [...(save.questsDone ?? []), id] };
}
