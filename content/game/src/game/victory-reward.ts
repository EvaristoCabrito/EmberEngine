import type { Mission, Unit, VictoryReward } from './types.ts';

const amount = (value: number): number => Number.isFinite(value) ? Math.max(0, Math.floor(value)) : 0;

/** Payments are resolved by the existing guarded victory-save flow, never by replaying dialogue.
 * Exiting early cannot earn a rescue payment while attackers remain on the board. */
export function victoryRewardFor(
  mission: Pick<Mission, 'victoryReward'>,
  units: Pick<Unit, 'name' | 'side' | 'alive'>[],
): Pick<VictoryReward, 'ember' | 'rations'> {
  const reward = mission.victoryReward;
  if (!reward || units.some(u => u.side === 'enemy' && u.alive)) return { ember: 0, rations: 0 };
  if (reward.requiredNpcNames?.some(name => !units.some(u => u.name === name && u.side === 'neutral' && u.alive))) return { ember: 0, rations: 0 };
  return { ember: amount(reward.ember), rations: amount(reward.rations) };
}
