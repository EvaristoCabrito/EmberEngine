import test from 'node:test';
import assert from 'node:assert/strict';
import { expToLevel, MAX_LEVEL } from './data.ts';

test('every level follows the requested XP bands', () => {
  const bands = [[1,2,100],[3,7,150],[8,12,225],[13,17,300],[18,22,400],[23,27,500],[28,29,600]];
  for (const [first,last,cost] of bands) for(let level=first;level<=last;level++) assert.equal(expToLevel(level),cost,`level ${level} to ${level+1}`);
});

test('out-of-range levels retain the first or final band', () => {
  assert.equal(expToLevel(0),100);
  assert.equal(expToLevel(-5),100);
  assert.equal(expToLevel(MAX_LEVEL),600);
  assert.equal(expToLevel(100),600);
});
