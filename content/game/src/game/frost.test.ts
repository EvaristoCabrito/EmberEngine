import test from 'node:test';import assert from 'node:assert/strict';
import {FROST,frostPower,frostCharges,frostAreaTiles} from './frost.ts';
import {hexNeighbors,hexDist,axisDir} from './pathfinding.ts';
import {spellTier} from './data.ts';import {spellElement} from './resistances.ts';
test('Frost is tier 2 Ice and mage progression starts at level 5',()=>{assert.equal(FROST.unlockLevel,5);assert.equal(spellTier('frost'),2);assert.equal(spellElement('frost'),'ice');for(const[level,length]of [[1,2],[5,2],[8,2],[9,3],[13,4],[17,5],[21,6],[25,7],[29,8],[30,8]])assert.equal(frostPower(level).length,length);});
test('Cultist V2 charge milestones',()=>{for(const[level,charges]of [[1,1],[4,1],[5,2],[11,2],[12,3],[19,3],[20,4],[29,4],[30,4]])assert.equal(frostCharges(level),charges);});
test('six facing directions on odd and even rows make adjacent straight lines excluding caster',()=>{for(const row of [10,11]){const origin={x:10,y:row};for(const target of hexNeighbors(origin.x,origin.y)){const cells=frostAreaTiles(origin,target,30,40,40);assert.equal(cells.length,8);assert.equal(new Set(cells.map(c=>`${c.x},${c.y}`)).size,8);cells.forEach((cell,i)=>{assert.equal(hexDist(origin,cell),i+1);assert.equal(hexDist(i?cells[i-1]:origin,cell),1);assert.deepEqual(axisDir(origin,cell),axisDir(origin,target));});}}});
test('Frost clips at board edges and cannot aim at caster',()=>{assert.deepEqual(frostAreaTiles({x:0,y:0},{x:0,y:0},5,10,10),[]);const cells=frostAreaTiles({x:0,y:0},{x:1,y:0},30,3,3);assert.equal(cells.length,2);});

