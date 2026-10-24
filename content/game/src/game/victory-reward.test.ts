import test from 'node:test';
import assert from 'node:assert/strict';
import { victoryRewardFor } from './victory-reward.ts';

const mission = { victoryReward: { ember: 400, rations: 12, requiredNpcNames: ['Mara'] } };
const npc = { name: 'Mara', side: 'neutral' as const, alive: true };
const enemy = { name: 'Bandit', side: 'enemy' as const, alive: true };
test('rescue pays the promised Gold and rations after attackers are defeated', () => {
  assert.deepEqual(victoryRewardFor(mission, [npc, { ...enemy, alive: false }]), { ember: 400, rations: 12 });
});
test('early escape, missing NPC, or dead NPC earns no rescue payment', () => {
  for (const units of [[npc, enemy], [], [{ ...npc, alive: false }]]) {
    assert.deepEqual(victoryRewardFor(mission, units), { ember: 0, rations: 0 });
  }
});
test('old missions earn no additional payment and malformed amounts cannot corrupt inventory', () => {
  assert.deepEqual(victoryRewardFor({}, [npc]), { ember: 0, rations: 0 });
  assert.deepEqual(victoryRewardFor({ victoryReward: { ember: NaN, rations: -3 } }, []), { ember: 0, rations: 0 });
  assert.deepEqual(victoryRewardFor({ victoryReward: { ember: 5.9, rations: Infinity } }, []), { ember: 5, rations: 0 });
});
