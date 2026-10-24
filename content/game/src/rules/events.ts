import type { Point, SpellKind, Unit, DecorationPlacement } from '../ember/types.ts';
export type BattleEvent = {
    type: 'battleStarted';
    round: number;
    turnOrder: string[];
    units: Unit[];
} | {
    type:'zonesChanged';
    web: {cells:string[];roundsLeft:number;center?:Point;radius?:number;sleepChance?:number}[];
    ice: {cells:string[];roundsLeft:number;center?:Point;radius?:number;damageDice:number;damageFaces:number;damageMul:number;casterMag:number;casterLevel:number;casterId:string;side:Unit['side']}[];
    aura:{cells:string[];roundsLeft:number;kind:'protection'|'intimidation';side:Unit['side'];pct:number}[];
} | {
    type: 'turnStarted' | 'turnEnded';
    unitId: string;
    round: number;
} | {
    type: 'roundStarted';
    round: number;
    turnOrder: string[];
} | {
    type: 'moved';
    unitId: string;
    from: Point;
    to: Point;
    path: Point[];
    cost: number;
    forced: boolean;
} | {
    type: 'attacked';
    unitId: string;
    targetId: string;
    counter: boolean;
    offHand: boolean;
    spellKind: SpellKind | null;
    from: Point;
    to: Point;
} | {
    type: 'damaged';
    unitId: string;
    sourceId: string | null;
    amount: number;
    hp: number;
    crit: boolean;
    cause: string;
    position: Point;
} | {
    type: 'missed';
    unitId: string;
    sourceId: string | null;
} | {
    type: 'died';
    unitId: string;
    position: Point;
} | {
    type: 'castSpell';
    unitId: string;
    kind: SpellKind;
    castId: number;
    from: Point;
    targets: Point[];
    targetIds: string[];
    impact: 'resolved';
} | {
    type: 'healed';
    unitId: string;
    sourceId: string;
    amount: number;
    hp: number;
} | {
    type: 'statusChanged';
    unitId: string;
    statuses: Record<string, unknown>;
} | {
    type: 'summoned';
    unitId: string;
    sourceId: string;
    unit: Unit;
} | {
    type: 'unitChanged';
    unitId: string;
    unit: Unit;
} | {
    type: 'terrainChanged';
    tiles: string[];
    decorations: DecorationPlacement[];
} | {
    type: 'loot';
    weapons: string[];
    equipment: string[];
    ember: number;
    rations: number;
} | {
    type: 'victoryAvailable';
    available: boolean;
    exit: DecorationPlacement | null;
} | {
    type: 'battleEnded';
    result: 'victory' | 'defeat';
} | {
    type: 'questPickup';
    key: string;
    unitId: string;
};
