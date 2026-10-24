import type { Unit, TerrainId, TerrainDef, SpellKind, Point, Mission, DecorationPlacement } from '../ember/types.ts';
import type { Seq } from './spell-source.ts';
import type { EnmityTable } from './enmity.ts';
import { occupancy } from './pathfinding.ts';
import { hexDef } from './hexprops.ts';
import type { HeroSkills } from './skills.ts';
import {rollDamage,rollDamageCustom} from './combat.ts';
export interface SpellImpact {
    hit: boolean;
    ids: string[];
    tiles: Point[];
    spellKind: SpellKind | null;
    centerId: string | null;
    centerDice: number;
    centerFaces: number;
    centerBonus: number;
    centerMul: number;
    extraDice: number;
    extraFaces: number;
    extraBonus: number;
    moreDice: number;
    moreFaces: number;
    spellMul: number;
    weaponBonusDice: number;
    weaponBonusFaces: number;
    weaponBonusBonus: number;
    dmgMul: number;
    poison: boolean;
    echo: {
        dice: number;
        faces: number;
        bonus: number;
    } | null;
}
export interface CombatImpact {
    stage: 'hit' | 'counterHit';
    customDice?: {
        dice: number;
        faces: number;
        bonus: number;
    };
    counterCustomDice?: {
        dice: number;
        faces: number;
        bonus: number;
    };
    spellKind: SpellKind | null;
    bonusDice: number;
    bonusDiceCount: number;
    bonusFlat: number;
    dmgMul: number;
    stunChance: number;
    wallImpact?: {
        dice: number;
        faces: number;
    } | null;
    knockTo?: Point | null;
}
export interface WebZone {
    cells: Set<string>;
    roundsLeft: number;
    createdAt?: number;
    center?: Point;
    radius?: number;
    sleepChance?: number;
}
export interface IceZone extends WebZone {
    damageDice: number;
    damageFaces: number;
    damageMul: number;
    casterMag: number;
    casterLevel: number;
    casterId: string;
    side: Unit['side'];
}
export interface AuraZone {
    cells: Set<string>;
    roundsLeft: number;
    kind: 'protection' | 'intimidation';
    side: Unit['side'];
    pct: number;
}
/** Rules-only state shared by the preserved source spell planners and Battle. */
export abstract class RuleContext {
    abstract mission: Mission;
    decorations: DecorationPlacement[] = [];
    terrainVersion = 0;
    protected turnRestrained = false;
    protected fogged = false;
    protected awake = new Set<string>();
    protected fieldStamp = '';
    protected fieldCache = new Map<string, Map<string, number>>();
    protected trauma = 0;
    protected doubleStrikeAlt = false;
    protected hexCenter(x: number, y: number) { return { cx: x + (y & 1) * 0.5, cy: y * Math.sqrt(3) / 2 }; }
    /** Numeric elevation/decoration adaptation; Ember's damage formulas and RNG remain intact. */
    protected rollDamageAt(...args:Parameters<typeof rollDamage>){const [actor,target,attTile,defTile,rng,useWeaponSkill=true]=args;return rollDamage(actor,target,attTile,defTile,rng,useWeaponSkill,{att:this.hexAt(actor.x,actor.y),def:this.hexAt(target.x,target.y)})}
    protected rollDamageCustomAt(...args:Parameters<typeof rollDamageCustom>){const [actor,target,attTile,defTile,dice,faces,bonus,rng,useWeaponSkill=true]=args;return rollDamageCustom(actor,target,attTile,defTile,dice,faces,bonus,rng,useWeaponSkill,{att:this.hexAt(actor.x,actor.y),def:this.hexAt(target.x,target.y)})}
    units: Unit[] = [];
    cols = 0;
    rows = 0;
    tiles: TerrainId[] = [];
    elevations: number[] = [];
    decorOverlay = new Uint8Array(0);
    webZones: WebZone[] = [];
    iceStormZones: IceZone[] = [];
    auraZones: AuraZone[] = [];
    enmity: EnmityTable = new Map();
    time = 0;
    lootRations = 0;
    heroSkills: HeroSkills = {};
    affinityScores: Record<string, number> = {};
    partyLeader = 'Kael';
    protected supportAffinityRecipients = new Map<string, Set<string>>();
    protected onNextIdle: null = null;
    protected turn = 1;
    protected queue: Seq[] = [];
    protected onSeqStart = new Map<Seq, () => void>();
    protected chargeMoves = new Set<Seq>();
    protected mode = 'awaitSpell';
    protected selectedId: string | null = null;
    protected spellKind: SpellKind | null = null;
    protected spellAim: Point | null = null;
    protected spellArmed = false;
    protected tip: string | null = null;
    protected missileTargets: {
        id: string;
        cell: Point;
    }[] = [];
    protected debugFreeCast = false;
    protected rng: () => number = () => 0;
    protected occ() { return occupancy(this.units); }
    protected hexAt(x: number, y: number): TerrainDef {
        const terrain = hexDef(this.tiles, this.cols, x, y, this.decorOverlay);
        // Ember terrain high flags remain; numeric elevations add the requested high ground.
        return (this.elevations[y * this.cols + x] ?? 0) > 0 ? { ...terrain, height: 1, atk: 2 } : terrain;
    }
    protected abstract addEnmity(enemy: Unit, hero: Unit, ce: number, ve: number): void;
    protected abstract noteAwareEnmity(hero: Unit, ce: number, ve: number): void;
    protected abstract finishAction(unit: Unit): void;
    protected abstract reapplyGear(unit: Unit): void;
    protected abstract unitHidden(unit: Unit): boolean;
    protected abstract spawnHit(unit: Unit, damage: number, crit: boolean, melee?: boolean): void;
    protected abstract spawnMiss(unit: Unit): void;
    protected abstract markDead(unit: Unit): void;
    protected abstract noteDamageEnmity(actor: Unit, target: Unit, damage: number, kind: 'weapon' | 'spell'): void;
    protected abstract provoke(target: Unit, actor: Unit): void;
    protected abstract evaluateEnd(): void;
    protected abstract refreshDecorOverlay(): void;
    protected shotBlockedTip(_from: Point, _to: Point, _kind: string) { return 'Shot blocked by terrain or decoration.'; }
    // The former renderer-only provoke ring carries no authoritative state.
    protected provokeFx: {
        unitId: string;
        t: number;
    }[] = [];
}
