import type { Mission, Unit, Point, SpellKind, TerrainId, Spawn, DecorationPlacement, TierKey, BattleSnapshot, BattleUnitSnap } from '../ember/types.ts';
import { SpellRules, spawnUnit, unitFromSnap, takesTurns, type Roster, type Seq } from './spell-source.ts';
import type { SpellImpact, CombatImpact } from './context.ts';
import type { BattleEvent } from './events.ts';
import { classSpells } from './available-spells.ts';
import { CLASSES, DECORATIONS, EQUIPMENT, WEAPONS, TERRAIN, parseLayout, placedFootprint, placedBlockingFootprint, enemyLevelFor, KILL_DROP_CHANCE, weightedLootPick, weightedWeaponPick, BLESS, FAMILIAR_SPELL, FANTOM_FORCE, fantomForceDice, SECOND_WIND, WEB_OF_DREAMS, SIGHT_RADIUS, spellTier, tierKey, rollCure, isBossClass, magicMissileCount, multiShotTargets, warpPartyRadius, hexAreaTiles } from '../ember/data.ts';
import { elementalDamage } from '../ember/resistances.ts';
import { fullness, drainHunger, ACTION_HUNGER_COST } from '../ember/hunger.ts';
import { dexEscapeChance } from './dexterity.ts';
import { buildDecorOverlay } from './hexprops.ts';
import { computeReachable, reconstructPath, footprintCost, footprint, canHitFrom, hexDist, hexNeighbors, occupies, inBounds, key, unitSize, canClimb, type ReachCell } from './pathfinding.ts';
import { canCounter, mulberry32 } from './combat.ts';
import { addToEntry, ENMITY, enmityFromSnapshot, enmityToSnapshot } from './enmity.ts';
import { relight, VISIBLE, packExplored, unpackExplored } from './fog.ts';
import { trainedWeaponSkills, weaponTypesForClass } from './weaponSkills.ts';
import { cleanHeroSkills, skillValue } from './skills.ts';
/** The JSON exports in maps/ember contain a { draft } envelope and an explicit tiles array. */
export type BattleMap = Partial<Mission> & Pick<Mission, 'cols' | 'rows'> & {
    tiles?: TerrainId[];
    playerSpawns?: Spawn[];
    enemySpawns?: Spawn[];
    neutralSpawns?: Spawn[];
    decorations?: DecorationPlacement[];
};
export interface BattleOptions {
    seed?: number;
    roster?: Roster;
    playerSpawns?: Spawn[];
    enemySpawns?: Spawn[];
    neutralSpawns?: Spawn[];
    debugFreeCast?: boolean;
}
export interface BattleSave {
    version: 1;
    mission: Mission;
    tiles: TerrainId[];
    decorations: DecorationPlacement[];
    units: Unit[];
    round: number;
    turnOrder: string[];
    activeUnitId: string | null;
    finished: string[];
    rngSeed: number;
    rngCalls: number;
    elevations: number[];
    winAvailable: boolean;
    result: 'victory' | 'defeat' | null;
    loot: {
        weapons: string[];
        equipment: string[];
        ember: number;
        rations: number;
    };
    heroSkills: Roster['heroSkills'];
    affinityScores: Record<string, number>;
    zones: {
        web: unknown[];
        ice: unknown[];
        aura: unknown[];
    };
    enmity: [
        string,
        [
            string,
            {
                ce: number;
                ve: number;
            }
        ][]
    ][];
    awake: string[];
    fog: number[];
    restrained: boolean;
    ownedWeapons: string[];
    questFound: string[];
    legacyMetadata: Partial<BattleSnapshot>;
    castSequence: number;
    debugFreeCast: boolean;
    partyLeader: string;
    questPickups: {
        key: string;
        name: string;
        x: number;
        y: number;
    }[];
}
/** Synchronous authoritative battle. Renderer consumes returned events; it never controls mutations. */
export class Battle extends SpellRules {
    mission: Mission;
    activeUnitId: string | null = null;
    turnOrder: string[] = [];
    winAvailable = false;
    result: 'victory' | 'defeat' | null = null;
    lootWeapons: string[] = [];
    lootEquipment: string[] = [];
    lootEmber = 0;
    questPickups: {
        key: string;
        name: string;
        x: number;
        y: number;
    }[] = [];
    questFound: string[] = [];
    /** Host-owned legacy metadata is preserved without presentation dependencies. */
    private legacyMetadata: Partial<BattleSnapshot> = {};
    private finished = new Set<string>();
    private out: BattleEvent[] = [];
    private rngSeed = 0;
    private rngCalls = 0;
    private sourceId: string | null = null;
    private cause = 'status';
    private castSequence = 0;
    private ownedWeapons = new Set<string>();
    private vis = new Uint8Array(0);
    private draining = false;
    private activeExit: DecorationPlacement | null = null;
    get round() { return this.turn; }
    get activeUnit() { return this.units.find(u => u.id === this.activeUnitId) ?? null; }
    constructor(input: BattleMap | {
        draft: BattleMap;
    }, options: BattleOptions = {}) {
        super();
        const draft = 'draft' in input ? input.draft : input;
        if (!Number.isInteger(draft.cols) || !Number.isInteger(draft.rows) || draft.cols < 1 || draft.rows < 1)
            throw new Error('Invalid board dimensions.');
        const cloned = structuredClone(draft);
        this.mission = { id: 'battle', index: 0, title: 'Battle', place: '', briefing: '', objective: '', win: 'rout', layout: [], ...cloned, playerSpawns: options.playerSpawns ?? cloned.playerSpawns ?? [], enemySpawns: options.enemySpawns ?? cloned.enemySpawns ?? [], neutralSpawns: options.neutralSpawns ?? cloned.neutralSpawns ?? [] };
        this.cols = this.mission.cols;
        this.rows = this.mission.rows;
        this.tiles = cloned.tiles?.slice() ?? parseLayout(this.mission.layout);
        if (this.tiles.length !== this.cols * this.rows)
            throw new Error('Board tiles must cover every cell.');
        this.elevations = Array.from({ length: this.tiles.length }, (_, i) => this.mission.terrainElevations?.[i] ?? TERRAIN[this.tiles[i]].height ?? 0);
        this.decorations = structuredClone(cloned.decorations ?? []);
        for (const d of this.decorations) {
            const def = DECORATIONS[d.id];
            if (!def?.tile)
                continue;
            for (const f of placedFootprint(d)) {
                const x = d.x + f.dx, y = d.y + f.dy;
                if (inBounds(x, y, this.cols, this.rows))
                    this.tiles[y * this.cols + x] = def.tile;
            }
        }
        this.refreshDecorOverlay();
        const roster = options.roster ?? { hp: {}, levels: {} };
        this.heroSkills = structuredClone(roster.heroSkills ?? {});
        this.affinityScores = structuredClone(roster.affinityScores ?? {});
        this.partyLeader = roster.partyLeader ?? 'Kael';
        this.questPickups = structuredClone(roster.questPickups ?? []);
        this.ownedWeapons = new Set(roster.ownedWeaponIds ?? []);
        this.rngSeed = (options.seed ?? 1) + this.mission.index * 97;
        this.resetRng();
        this.debugFreeCast = options.debugFreeCast ?? false;
        const defeated = new Set(roster.crossingDefeatedSpawns ?? []);
        this.units = [...this.mission.playerSpawns.filter(s => !((roster.hungerPenaltyPct ?? 0) >= 0.9 && fullness(roster.heroHunger?.[s.name]) <= 0)).map((s, i) => spawnUnit(s, 'player', i, roster)), ...this.mission.enemySpawns.flatMap((s, i) => defeated.has(`enemy-${s.name}-${i}`) ? [] : [spawnUnit(s, 'enemy', i, roster, enemyLevelFor(this.mission.index))]), ...this.uniqueNeutralNpcSpawns(this.mission.neutralSpawns ?? []).flatMap(({ spawn: s, index: i }) => defeated.has(`neutral-${s.name}-${i}`) ? [] : [spawnUnit(s, 'neutral', i, roster, enemyLevelFor(this.mission.index))])];
        // Ember consumes bob RNG before initiative; retain those calls although no sprite is drawn here.
        for (const u of this.units) {
            this.nudgeOffHazard(u);
            if (u.side === 'player')
                this.nudgeOffWaypoint(u);
            u.bob = this.rng() * 16;
        }
        this.rollOpeningInitiative(this.units.filter(takesTurns));
        this.turnOrder = this.sortByInitiative(this.units.filter(takesTurns));
        this.fogged = !!this.mission.fog;
        this.vis = new Uint8Array(this.tiles.length);
        this.refreshFog();
        this.evaluateEnd();
        this.out.unshift({ type: 'battleStarted', round: this.round, turnOrder: [...this.turnOrder], units: structuredClone(this.units) });
    }
    private resetRng(skip = 0) { const generator = mulberry32(this.rngSeed); for (let i = 0; i < skip; i++)
        generator(); this.rngCalls = skip; this.rng = () => { this.rngCalls++; return generator(); }; }
    events() { const result = this.out; this.out = []; return result; }
    private transaction(action: () => void): BattleEvent[] {
        if (this.result) throw new Error('Battle has ended.');
        const original=this.captureUnitChanges();
        const zonesBefore=JSON.stringify([this.webZones,this.iceStormZones,this.auraZones],(_key,value)=>value instanceof Set?[...value]:value);
        const terrainBefore=this.terrainVersion;
        this.out=[];action();this.refreshFog();this.evaluateEnd();
        this.emitUnitChanges(original);
        const zonesAfter=JSON.stringify([this.webZones,this.iceStormZones,this.auraZones],(_key,value)=>value instanceof Set?[...value]:value);
        if(zonesBefore!==zonesAfter)this.out.push({type:'zonesChanged',web:this.webZones.map(z=>({...z,cells:[...z.cells]})),ice:this.iceStormZones.map(z=>({...z,cells:[...z.cells]})),aura:this.auraZones.map(z=>({...z,cells:[...z.cells]}))});
        if(terrainBefore!==this.terrainVersion)this.out.push({type:'terrainChanged',tiles:[...this.tiles],decorations:structuredClone(this.decorations)});
        return this.events();
    }
    private acting(id: string, needsAction = false) { const u = this.units.find(u => u.id === id); if (!u?.alive)
        throw new Error('Unit is unavailable.'); if (this.activeUnitId !== id)
        throw new Error('Unit does not own the active turn.'); if (needsAction && u.acted)
        throw new Error('Unit has already acted.'); return u; }
    /** Begins/skips status-disabled turns; never automatically runs enemy AI. */
    beginTurn(): BattleEvent[] { return this.transaction(() => this.advance()); }
    private advance() {
        if (this.activeUnitId)
            return;
        for (let guard = 0; guard < this.units.length * 3 + 3; guard++) {
            let u = this.turnOrder.map(id => this.units.find(u => u.id === id)).find(u => u?.alive && !this.finished.has(u.id));
            if (!u) {
                if (!this.units.some(takesTurns))
                    return;
                this.newRound();
                u = this.turnOrder.map(id => this.units.find(u => u.id === id)).find(u => u?.alive);
                if (!u)
                    return;
            }
            this.activeUnitId = u.id;
            this.selectedId = u.id;
            u.moveBudgetUsed = 0;
            u.bleedMovedThisTurn = false;
            u.idleAlt = !u.idleAlt;
            this.sourceId = null;
            this.cause = 'startOfTurn';
            this.out.push({ type: 'turnStarted', unitId: u.id, round: this.round });
            this.startOfTurnEffects(u);
            if (this.result)
                return;
            if (!u.alive) {
                this.complete(u);
                continue;
            }
            if (u.stunned) {
                u.stunTurns = Math.max(0, u.stunTurns - 1);
                u.stunned = u.stunTurns > 0;
                this.complete(u);
                continue;
            }
            if (this.isWebCell(u.x, u.y) && this.rng() < this.webCellSleepChance(u.x, u.y)) {
                u.asleep = true;
                u.sleepTurns += 1 + Math.floor(this.rng() * WEB_OF_DREAMS.sleepFaces);
            }
            if (u.asleep) {
                u.sleepTurns = Math.max(0, u.sleepTurns - 1);
                u.asleep = u.sleepTurns > 0;
                this.complete(u);
                continue;
            }
            this.turnRestrained = this.isWebCell(u.x, u.y);
            if ((u.fearTurns ?? 0) > 0) {
                u.fearTurns!--;
                const source = this.units.find(t => t.id === u.fearSourceId);
                const threats = source ? [source] : this.units.filter(t => t.alive && t.side !== u.side && t.side !== 'neutral');
                const distance = (p: Point) => threats.length ? Math.min(...threats.map(t => hexDist(p, t))) : 0;
                let dest: Point = u;
                for (const p of this.reachable(u.id).values())
                    if (distance(p) > distance(dest))
                        dest = p;
                if (dest !== u)
                    this.walk(u, dest);
                this.complete(u);
                continue;
            }
            u.acted = false;
            return;
        }
    }
    private newRound() {
        for (const row of this.enmity.values())
            for (const [id, e] of row)
                row.set(id, addToEntry(e, 0, -ENMITY.volatileDecayPerRound));
        for (const u of this.units) {
            u.moved = false;
            u.acted = false;
            if (u.bleedRoundsLeft != null && u.bleedRoundMarker != null && u.bleedRoundMarker < this.turn) {
                u.bleedRoundsLeft = Math.max(0, u.bleedRoundsLeft - 1);
                u.bleedRoundMarker = this.turn;
                if (u.bleedRoundsLeft === 0) {
                    u.bleeding = false;
                    u.bleedRoundMarker = undefined;
                }
            }
            if ((u.blessedRoundsLeft ?? 0) > 0) {
                u.blessedRoundsLeft!--;
                if (!u.blessedRoundsLeft)
                    u.blessedHitBonusPct = 0;
            }
        }
        for (const z of [...this.webZones, ...this.iceStormZones, ...this.auraZones])
            z.roundsLeft--;
        this.webZones = this.webZones.filter(z => z.roundsLeft > 0);
        this.iceStormZones = this.iceStormZones.filter(z => z.roundsLeft > 0);
        this.auraZones = this.auraZones.filter(z => z.roundsLeft > 0);
        this.turn++;
        this.finished.clear();
        this.turnOrder = this.sortByInitiative(this.units.filter(takesTurns));
        this.out.push({ type: 'roundStarted', round: this.round, turnOrder: [...this.turnOrder] });
    }
    private complete(u: Unit) { u.moved = true; u.acted = true; this.finished.add(u.id); this.activeUnitId = null; this.selectedId = null; this.out.push({ type: 'turnEnded', unitId: u.id, round: this.round }); }
    endTurn(id = this.activeUnitId!): BattleEvent[] { return this.transaction(() => { const u = this.acting(id); this.complete(u); this.advance(); }); }
    wait(id = this.activeUnitId!) { return this.endTurn(id); }
    protected finishAction(u: Unit) {
        if (!u.acted) {
            this.gainAdjacentAffinity(u);
            if(u.side==='player'&&!u.summoned)u.fullness=drainHunger(u.fullness,ACTION_HUNGER_COST);
        }
        u.acted = true;
        this.evaluateEnd();
        if(!u.alive||u.mov-u.moveBudgetUsed<=0)this.complete(u);
    }
    reachable(id: string, prune = true): Map<string, ReachCell> { const u = this.units.find(u => u.id === id); if (!u?.alive)
        return new Map(); return computeReachable(this.effectiveUnitForReach(u), this.tiles, this.cols, this.rows, this.units, prune, this.decorOverlay, this.elevations); }
    move(id: string, to: Point): BattleEvent[] { return this.transaction(() => this.walk(this.acting(id), to)); }
    private walk(u: Unit, to: Point) { const reach = this.reachable(u.id); if (!reach.has(key(to.x, to.y)))
        throw new Error('Destination is unreachable.'); const paths = this.reachable(u.id, false); const path = reconstructPath(paths, to); if (path.length < 2)
        return; if (!this.applyBleedingActionDamage(u, true))
        return; this.applyPath(u, path, false); }
    private applyPath(u: Unit, path: Point[], forced: boolean) {
        const from = { x: u.x, y: u.y };
        let cost = 0;
        const traveled: Point[] = [from];
        for (const to of path.slice(1)) {
            if (!u.alive)
                break;
            if (hexDist(u, to) !== 1 || !canClimb(u, to, this.elevations, this.cols, u))
                throw new Error('Invalid movement step.');
            const step = footprintCost(to.x, to.y, unitSize(u), this.tiles, this.cols, this.rows, this.occ(), u, false, this.decorOverlay);
            if (step === null)
                throw new Error('Body zone or terrain blocks movement.');
            this.face(u, to);
            u.x = to.x;
            u.y = to.y;
            u.drawX = u.x;
            u.drawY = u.y;
            cost += step;
            traveled.push({ ...to });
            this.smashBarricades(u);
            this.applyTileHazard(u, to);
        }
        if (!forced)
            u.moveBudgetUsed += cost;
        u.moved = true;
        this.refreshFog();
        this.out.push({ type: 'moved', unitId: u.id, from, to: { x: u.x, y: u.y }, path: traveled, cost, forced });
    }
    private face(u: Unit, to: Point) { u.faceDx = to.x + (to.y & 1) * .5 - u.x - (u.y & 1) * .5; u.faceDy = (to.y - u.y) * Math.sqrt(3) / 2; u.facing = u.faceDx < 0 ? -1 : 1; }
    attack(id: string, targetId: string, offHand = false): BattleEvent[] { return this.transaction(() => { const u = this.acting(id, true), target = this.units.find(t => t.id === targetId); if (!target?.alive || target.dialog || target.side === u.side || this.unitHidden(target))
        throw new Error('Invalid attack target.'); const item = offHand && u.offHandId ? EQUIPMENT[u.offHandId] : null; const attacker = item?.kind === 'weapon' ? { ...u, minRange: item.minRange ?? 1, maxRange: item.maxRange ?? 1 } : u; if (offHand && item?.kind !== 'weapon')
        throw new Error('No off-hand weapon.'); if (!canHitFrom(attacker, u, target, this.tiles, this.cols, this.decorOverlay))
        throw new Error('Target is out of weapon range or shot is blocked.'); if (!this.applyBleedingActionDamage(u, false))
        return; this.queue.push({ type: 'combat', att: id, def: targetId, ...(item?.kind === 'weapon' ? { customDice: { dice: item.dice ?? 1, faces: item.faces ?? 4, bonus: item.bonus ?? 0 } } : {}) }); this.drain(); this.finishAction(u); }); }
    availableSpells(id: string) { const u = this.units.find(u => u.id === id); return u ? classSpells(u.classId, u.level, u.name) : []; }
    remainingSpellUses(id: string, kind: SpellKind) { const u = this.units.find(u => u.id === id); if (!u)
        return 0; if (kind === 'lifeDrain')
        return u.lifeDrainCharges ?? 0; if (kind === 'shock')
        return FAMILIAR_SPELL[u.classId] === 'shock' ? u.spellCharges ?? 0 : u.shockCharges; if (kind === 'fantomForce')
        return u.fantomForceCharges ?? 0; return this.familiarSpellRemaining(u, kind === 'magicMissileV2' ? 'magicMissile' : kind); }
    /** N targets for missiles/volleys may repeat; one slot is spent for the complete cast. */
    castSpell(id: string, kind: SpellKind, target: Point | Point[] = this.activeUnit ?? { x: 0, y: 0 }): BattleEvent[] {
        return this.transaction(() => {
            const u = this.acting(id, true), cells = Array.isArray(target) ? target : [target];
            if (!cells.length)
                throw new Error('A spell needs a target.');
            if (kind === 'secondWind')
                throw new Error('Second Wind is a passive start-of-turn effect.');
            const normalized = kind === 'magicMissileV2' ? 'magicMissile' : kind;
            const wanted = normalized === 'magicMissile' ? magicMissileCount(u.level) : kind === 'multiShot' ? multiShotTargets(u.level) : 1;
            if (cells.length !== wanted)
                throw new Error(`Supply ${wanted} target(s) for ${kind}.`);
            if (u.side === 'player' && !this.debugFreeCast && !this.availableSpells(id).includes(normalized))
                throw new Error('Class or level does not permit this spell.');
            if (this.remainingSpellUses(id, kind) <= 0 && !this.debugFreeCast)
                throw new Error('Spell has no remaining charges.');
            this.selectedId = id;
            this.spellKind = normalized;
            this.mode = 'awaitSpell';
            this.tip = null;
            this.missileTargets = [];
            const selfKinds = ['sweep', 'turnUndead', 'bless', 'auraOfProtection', 'intimidatingPresence', 'createFoodAndWater'];
            if (!selfKinds.includes(kind) && kind !== 'fantomForce' && kind !== 'tendrilSwipe')
                for (const cell of cells) {
                    if (!inBounds(cell.x, cell.y, this.cols, this.rows) || !this.spellAimValid(u, cell))
                        throw new Error(this.spellAimError(u, cell));
                }
            const original = this.captureUnitChanges();
            const castId = ++this.castSequence;
            this.sourceId = id;
            this.cause = kind;
            if (!this.applyBleedingActionDamage(u, false))
                return;
            const cell = cells[0];
            switch (kind) {
                case 'fireball':
                    this.castFireball(u, cell);
                    break;
                case 'iceStorm':
                    this.castIceStorm(u, cell);
                    break;
                case 'frost':
                    this.castFrost(u, cell);
                    break;
                case 'bless':
                    this.startBless();
                    break;
                case 'cureMinor':
                case 'cureWounds':
                case 'cureLight':
                    this.castHeal(u, cell, kind);
                    break;
                case 'cureDisease':
                    this.castCureDisease(u, cell);
                    break;
                case 'longShot':
                    this.castLongShot(u, cell);
                    break;
                case 'bloodyShot':
                    this.castBloodyShot(u, cell);
                    break;
                case 'provoke':
                    this.castProvoke(u, cell);
                    break;
                case 'piercing':
                    this.castPiercing(u, cell);
                    break;
                case 'lightning':
                    this.castLightning(u, cell);
                    break;
                case 'lightningTier3':
                    this.castLightningTier3(u, cell);
                    break;
                case 'magicMissile':
                case 'magicMissileV2':
                    for (const p of cells)
                        this.castMagicMissile(u, p);
                    break;
                case 'multiShot':
                    for (const p of cells)
                        this.castMultiShot(u, p);
                    break;
                case 'causticVenom':
                    this.castCausticVenom(u, cell);
                    break;
                case 'divineBolt':
                    this.castDivineBolt(u, cell);
                    break;
                case 'minorVenom':
                    this.castMinorVenom(u, cell);
                    break;
                case 'doubleStrike':
                    this.castDoubleStrike(u, cell);
                    break;
                case 'cleave':
                    this.castCleave(u, cell);
                    break;
                case 'piercingThrust':
                    this.castPiercingThrust(u, cell);
                    break;
                case 'sweep':
                    this.confirmSweep();
                    break;
                case 'trip':
                    this.castTrip(u, cell);
                    break;
                case 'summonFamiliar':
                    this.castSummonFamiliar(u, cell, 1);
                    break;
                case 'summonFamiliar2':
                    this.castSummonFamiliar(u, cell, 2);
                    break;
                case 'summonFamiliar3':
                    this.castSummonFamiliar(u, cell, 3);
                    break;
                case 'summonFamiliar4':
                    this.castSummonFamiliar(u, cell, 4);
                    break;
                case 'summonZombieDog':
                    this.castSummonFamiliar(u, cell, 5);
                    break;
                case 'phantasmalForce':
                    this.castPhantasmalForce(u, cell);
                    break;
                case 'fantomForce': {
                    const foe = this.occ().get(key(cell.x, cell.y));
                    if (!foe?.alive || foe.side === u.side || hexDist(u, foe) > FANTOM_FORCE.range)
                        throw new Error('Invalid Fantom Force target.');
                    const p = fantomForceDice(u.level);
                    u.fantomForceCharges = Math.max(0, (u.fantomForceCharges ?? 0) - 1);
                    this.queue.push({ type: 'spell', att: id, tiles: [cell], ids: [foe.id], dice: p.dice, faces: p.faces, bonus: 0, spellMul: 1, spellKind: kind });
                    break;
                }
                case 'lifeDrain':
                    this.castLifeDrain(u, cell);
                    break;
                case 'webOfDreams':
                    this.castWebOfDreams(u, cell);
                    break;
                case 'auraOfProtection':
                    this.startAuraOfProtection();
                    break;
                case 'divineWrath':
                    this.castDivineWrath(u, cell);
                    break;
                case 'shoulderSmash':
                    this.castShoulderSmash(u, cell);
                    break;
                case 'intimidatingPresence':
                    this.startIntimidatingPresence();
                    break;
                case 'stampede':
                    this.castStampede(u, cell);
                    break;
                case 'shock':
                    this.castShock(u, cell);
                    break;
                case 'bullRush':
                    this.castBullRush(u, cell);
                    break;
                case 'executionerStrike':
                    this.castExecutionerStrike(u, cell);
                    break;
                case 'shieldBash':
                    this.castShieldBash(u, cell);
                    break;
                case 'poisonBreath':
                case 'burningHands':
                    this.castBurningHands(u, cell, kind);
                    break;
                case 'tendrilSwipe': {
                    const tiles = this.plantSwipeTiles(u, u);
                    const ids = this.units.filter(t => t.alive && t.side !== u.side && tiles.some(p => footprint(t).some(f => f.x === p.x && f.y === p.y))).map(t => t.id);
                    this.queue.push({ type: 'spell', att: id, tiles, ids, spellKind: kind });
                    break;
                }
                case 'turnUndead':
                    this.castTurnUndead(u);
                    break;
                case 'createFoodAndWater':
                    this.startCreateFoodAndWater();
                    break;
                case 'warp':
                    {
                        if (u.level < 7 || this.tierRemaining(u, kind) <= 0) throw new Error('Warp is not available.');
                        const radius = warpPartyRadius(u.level);
                        const dungeonText = `${this.mission.id} ${this.mission.title} ${this.mission.place}`.toLowerCase();
                        const dungeon = /dungeon|crossing|travessia|cripta|crypt|cave|caverna|ruins|ruínas|colina|passagem|profundezas|watchtower.*(undercroft|prison)|masmorra/.test(dungeonText) || (this.mission.decorations ?? []).some(placement => ["dungeon", "connector"].includes(DECORATIONS[placement.id]?.exitKind ?? ""));
                        const party = this.units.filter(ally => ally.alive && ally.side === 'player' && !ally.summoned && hexDist(u, ally) <= radius);
                        if (!party.includes(u)) party.unshift(u);
                        const anchors = dungeon ? this.mission.playerSpawns.map(({ x, y }) => ({ x, y })) : [cell];
                        if (!anchors.length) throw new Error('Warp has no entrance on this map.');
                        const partyIds = new Set(party.map(ally => ally.id));
                        const places = new Map<string, Point>();
                        for (const anchor of anchors) for (const point of hexAreaTiles(anchor, radius + 1, this.cols, this.rows)) {
                            if (!this.hexAt(point.x, point.y).passable) continue;
                            if (this.units.some(other => other.alive && !partyIds.has(other.id) && occupies(other, point.x, point.y))) continue;
                            places.set(key(point.x, point.y), point);
                        }
                        const destinations = [...places.values()].sort((a, b) => Math.min(...anchors.map(anchor => hexDist(a, anchor))) - Math.min(...anchors.map(anchor => hexDist(b, anchor))) || a.y - b.y || a.x - b.x);
                        if (destinations.length < party.length) throw new Error('There is not enough room beyond the Warp gate.');
                        party.sort((a, b) => Number(b.id === u.id) - Number(a.id === u.id));
                        for (let index = 0; index < party.length; index++) {
                            const ally = party[index]!;
                            const destination = destinations[index]!;
                            ally.x = ally.drawX = destination.x;
                            ally.y = ally.drawY = destination.y;
                            ally.moved = true;
                        }
                        this.spendTier(u, kind);
                        this.queue.push({ type: 'spell', att: id, tiles: destinations.slice(0, party.length), ids: [], label: 'Warp', spellKind: kind });
                        break;
                    }
                default: {
                    const exhaustive: never = kind;
                    throw new Error(`Unknown spell ${exhaustive}`);
                }
            }
            if (this.missileTargets.length) {
                this.missileTargets = [];
                throw new Error('Supply the complete authored number of missile/volley targets.');
            }
            const queued = this.queue.slice();
            if (!queued.length && kind !== 'provoke')
                throw new Error(this.tip ?? 'Spell was not cast.');
            const tiles = queued.flatMap(s => s.type === 'spell' ? s.tiles : []);
            const ids = [...new Set(queued.flatMap(s => s.type === 'spell' ? s.ids : s.type === 'combat' ? [s.def] : []))];
            this.out.push({ type: 'castSpell', unitId: id, kind, castId, from: { x: u.x, y: u.y }, targets: tiles.length ? tiles : cells, targetIds: ids, impact: 'resolved' });
            this.drain(true);
            this.finishAction(u);
            this.emitUnitChanges(original);
        });
    }
    runEnemyTurn(id = this.activeUnitId!): BattleEvent[] { return this.transaction(() => { const u = this.acting(id); if (u.side !== 'enemy')
        throw new Error('Active unit is not an enemy.'); const before = this.captureUnitChanges(); this.runAiFor(u); this.drain(); this.emitUnitChanges(before); this.complete(u); this.advance(); }); }
    /** Source action plans are resolved once in sequence; each projectile retains its own damage roll. */
    private drain(userCast=false) { if (this.draining)
        return; this.draining = true; try {
        while (this.queue.length) {
            const s = this.queue.shift()!;
            this.onSeqStart.get(s)?.();
            this.onSeqStart.delete(s);
            if (s.type === 'move') {
                const u = this.units.find(u => u.id === s.id);
                if (u?.alive)
                    this.applyPath(u, s.path, this.chargeMoves.has(s));
                this.chargeMoves.delete(s);
                continue;
            }
            if (s.type === 'combat') {
                const u = this.units.find(u => u.id === s.att), foe = this.units.find(u => u.id === s.def);
                if (!u?.alive || !foe?.alive)
                    continue;
                this.resolveCombat(u, foe, s);
                continue;
            }
            if (s.type === 'spell') {
                const u = this.units.find(u => u.id === s.att);
                if (!u?.alive)
                    continue;
                this.sourceId = u.id;
                this.cause = s.spellKind ?? 'spell';
                if(!userCast&&s.spellKind)this.out.push({type:'castSpell',unitId:u.id,kind:s.spellKind,castId:++this.castSequence,from:{x:u.x,y:u.y},targets:structuredClone(s.tiles),targetIds:[...s.ids],impact:'resolved'});
                const look = this.units.find(t => t.id === s.ids[0]) ?? s.tiles[0];
                if (look)
                    this.face(u, look);
                if (s.spellKind === 'bless') {
                    for (const id of s.ids) {
                        const t = this.units.find(t => t.id === id && t.alive);
                        if (t) {
                            t.blessedHitBonusPct = Math.max(t.blessedHitBonusPct ?? 0, BLESS.hitBonusPct(u.level) / 100);
                            t.blessedRoundsLeft = BLESS.durationRounds(u.level);
                        }
                    }
                    continue;
                }
                this.resolveSpellSource({ hit: false, ids: s.ids, tiles: s.tiles, spellKind: s.spellKind ?? null, centerId: s.centerId ?? null, centerDice: s.centerDice ?? 0, centerFaces: s.centerFaces ?? 8, centerBonus: s.centerBonus ?? 0, centerMul: s.centerMul ?? s.spellMul ?? 1, extraDice: s.dice ?? 0, extraFaces: s.faces ?? 8, extraBonus: s.bonus ?? 0, moreDice: s.moreDice ?? 0, moreFaces: s.moreFaces ?? 6, spellMul: s.spellMul ?? 1, weaponBonusDice: s.weaponBonusDice ?? 0, weaponBonusFaces: s.weaponBonusFaces ?? 8, weaponBonusBonus: s.weaponBonusBonus ?? 0, dmgMul: s.dmgMul ?? 1, poison: s.poison ?? false, echo: s.echo ?? null }, u);
                continue;
            }
            if (s.type === 'heal') {
                const u = this.units.find(u => u.id === s.att), target = this.units.find(u => u.id === s.def);
                if (u?.alive && target?.alive) {
                    const heal = this.healingPower(u, rollCure(s.kind, this.affinityUnit(u).mag, this.rng));
                    const gained = Math.min(heal, target.maxHp - target.hp);
                    target.hp += gained;
                    if (gained > 0) {
                        this.gainSupportAffinity(u, target);
                        this.trainHealing(u);
                        this.noteAwareEnmity(u, gained * ENMITY.heal.ce, gained * ENMITY.heal.ve);
                    }
                    this.gainExp(u, target.level, gained);
                    this.out.push({ type: 'healed', unitId: target.id, sourceId: u.id, amount: gained, hp: target.hp });
                }
                continue;
            }
            if (s.type === 'cureDisease') {
                const u = this.units.find(u => u.id === s.att), t = this.units.find(t => t.id === s.def);
                if (u?.alive && t?.alive) {
                    this.curePlayerDisease(t);
                    this.gainSupportAffinity(u, t);
                    this.trainHealing(u);
                    this.noteAwareEnmity(u, ENMITY.support.ce, ENMITY.support.ve);
                }
            }
        }
    }
    finally {
        this.draining = false;
        this.sourceId = null;
        this.cause = 'status';
    } }
    private resolveCombat(u: Unit, target: Unit, s: Extract<Seq, {
        type: 'combat';
    }>) {
        this.sourceId = u.id;
        this.cause = s.spellKind ?? 'attack';
        this.face(u, target);
        this.out.push({ type: 'attacked', unitId: u.id, targetId: target.id, counter: false, offHand: !!s.customDice, spellKind: s.spellKind ?? null, from: { x: u.x, y: u.y }, to: { x: target.x, y: target.y } });
        const a: CombatImpact = { stage: 'hit', customDice: s.customDice, spellKind: s.spellKind ?? null, bonusDice: s.bonusDice ?? 0, bonusDiceCount: s.bonusDiceCount ?? 1, bonusFlat: s.bonusFlat ?? 0, dmgMul: s.dmgMul ?? 1, stunChance: s.stunChance ?? 0, wallImpact: s.wallImpact, knockTo: s.knockTo };
        this.resolveCombatSource(a, u, target);
        if (!s.noCounter && !target.stunned && !target.asleep && u.alive && target.alive && canCounter(u, target, u, this.tiles, this.cols)) {
            this.face(target, u);
            const off = target.offHandId ? EQUIPMENT[target.offHandId] : null;
            const dice = off?.kind === 'weapon' && hexDist(target, u) <= (off.maxRange ?? 1) ? { dice: off.dice ?? 1, faces: off.faces ?? 4, bonus: off.bonus ?? 0 } : undefined;
            this.sourceId = target.id;
            this.cause = 'counter';
            this.out.push({ type: 'attacked', unitId: target.id, targetId: u.id, counter: true, offHand: !!dice, spellKind: null, from: { x: target.x, y: target.y }, to: { x: u.x, y: u.y } });
            this.resolveCombatSource({ ...a, stage: 'counterHit', counterCustomDice: dice }, target, u);
        }
    }
    protected spawnHit(u: Unit, amount: number, crit: boolean) { this.out.push({ type: 'damaged', unitId: u.id, sourceId: this.sourceId, amount, hp: u.hp, crit, cause: this.cause, position: { x: u.x, y: u.y } }); }
    protected spawnMiss(u: Unit) { this.out.push({ type: 'missed', unitId: u.id, sourceId: this.sourceId }); }
    protected markDead(u: Unit) { if (!u.alive)
        return; u.alive = false; u.diedAt = this.time; this.out.push({ type: 'died', unitId: u.id, position: { x: u.x, y: u.y } }); if (u.side === 'enemy' && u.guaranteedDrop) {
        const drop = weightedLootPick(this.rng, this.highestEnemyLevel(), this.ownedWeapons);
        if (drop.kind === 'weapon') {
            if (this.ownedWeapons.has(drop.id))
                this.lootEmber += 15;
            else {
                this.ownedWeapons.add(drop.id);
                this.lootWeapons.push(drop.id);
            }
        }
        else
            this.lootEquipment.push(drop.id);
    }
    else if (u.side === 'enemy' && this.rng() < KILL_DROP_CHANCE) {
        const id = weightedWeaponPick(this.rng, Object.keys(WEAPONS), this.highestEnemyLevel());
        if (this.ownedWeapons.has(id))
            this.lootEmber += 15;
        else {
            this.ownedWeapons.add(id);
            this.lootWeapons.push(id);
        }
    } }
    protected addEnmity(foe: Unit, hero: Unit, ce: number, ve: number) { if (foe.side !== 'enemy' || hero.side !== 'player' || !foe.alive || !hero.alive)
        return; let row = this.enmity.get(foe.id); if (!row) {
        row = new Map();
        this.enmity.set(foe.id, row);
    } row.set(hero.id, addToEntry(row.get(hero.id), ce, ve)); }
    protected noteAwareEnmity(hero: Unit, ce: number, ve: number) { for (const foe of this.units)
        if (foe.alive && foe.side === 'enemy' && (!this.fogged || this.awake.has(foe.id)))
            this.addEnmity(foe, hero, ce, ve); }
    protected noteDamageEnmity(actor: Unit, target: Unit, damage: number, kind: 'weapon' | 'spell') { if (damage <= 0)
        return; if (actor.side === 'player' && target.side === 'enemy') {
        const rate = ENMITY[kind];
        this.addEnmity(target, actor, damage * rate.ce, damage * rate.ve);
    } if (actor.side === 'enemy' && target.side === 'player') {
        const row = this.enmity.get(actor.id);
        const entry = row?.get(target.id);
        if (entry)
            row!.set(target.id, addToEntry(entry, -damage * ENMITY.damageTakenCe, 0));
    } }
    protected provoke(target: Unit, actor: Unit) { if (target.side !== 'neutral' || actor.side !== 'player')
        return; for (const u of this.units)
        if (u.alive && u.side === 'neutral' && u.classId === target.classId)
            u.side = 'enemy'; this.evaluateEnd(); }
    protected refreshDecorOverlay() { this.decorOverlay = buildDecorOverlay(this.decorations, this.cols, this.rows, placedBlockingFootprint, this.elevations); }
    private refreshFog() { if (this.fogged) {
        const eyes = this.units.filter(u => u.alive && u.side === 'player');
        relight(this.vis, eyes, eyes.map(u => u.visionRange ?? SIGHT_RADIUS), this.tiles, this.cols, this.rows, this.decorOverlay);
    } }
    protected unitHidden(u: Unit) { return this.fogged && !footprint(u).some(p => this.vis[p.y * this.cols + p.x] === VISIBLE); }
    protected evaluateEnd() {
        if (this.result)
            return;
        for (const p of [...this.questPickups]) {
            const u = this.units.find(u => u.alive && u.side === 'player' && u.x === p.x && u.y === p.y);
            if (u) {
                this.questPickups = this.questPickups.filter(x => x.key !== p.key);
                this.questFound.push(p.key);
                this.out.push({ type: 'questPickup', key: p.key, unitId: u.id });
            }
        }
        this.activeExit = this.decorations.find(d => DECORATIONS[d.id]?.exitKind && this.units.some(u => u.side === 'player' && u.alive && placedFootprint(d).some(f => d.x + f.dx === u.x && d.y + f.dy === u.y))) ?? null;
        const before = this.winAvailable;
        if (this.mission.explore)
            this.winAvailable = !!this.activeExit;
        else {
            const boss = this.units.some(u => u.alive && u.side === 'enemy' && isBossClass(u.classId));
            const enemy = this.units.some(u => u.alive && u.side === 'enemy');
            this.winAvailable = !!this.activeExit || (this.mission.win === 'boss' ? !boss : this.mission.win === 'escape' ? false : !enemy);
            if (!this.units.some(u => u.side === 'player' && u.alive && !u.summoned)) {
                this.result = 'defeat';
                this.out.push({ type: 'battleEnded', result: 'defeat' });
            }
        }
        if (before !== this.winAvailable)
            this.out.push({ type: 'victoryAvailable', available: this.winAvailable, exit: structuredClone(this.activeExit) });
    }
    confirmFinish(): BattleEvent[] {
        return this.transaction(()=>{
            if(!this.winAvailable)throw new Error('Victory is not available.');
            if(this.activeExit&&DECORATIONS[this.activeExit.id]?.exitKind==='escape'){
                const runner=this.units.find(u=>u.alive&&u.side==='player'&&placedFootprint(this.activeExit!).some(f=>u.x===this.activeExit!.x+f.dx&&u.y===this.activeExit!.y+f.dy));
                if(!runner)throw new Error('No hero occupies the escape waypoint.');
                if(this.rng()*100>=dexEscapeChance(this.affinityUnit(runner).dex)){this.complete(runner);return;}
            }
            this.result='victory';this.out.push({type:'battleEnded',result:'victory'},{type:'loot',weapons:[...this.lootWeapons],equipment:[...this.lootEquipment],ember:this.lootEmber,rations:this.lootRations});
        });
    }
    private captureUnitChanges() { return new Map(this.units.map(u => [u.id, JSON.stringify(u)])); }
    /** Reads Ember's original BattleSnapshot unchanged. Its old format has no RNG cursor. */
    static fromEmberSnapshot(map: BattleMap | {
        draft: BattleMap;
    }, snap: BattleSnapshot, options: BattleOptions = {}) {
        const b = new Battle(map, options);
        if (snap.missionId !== b.mission.id)
            throw new Error('Snapshot mission does not match the map.');
        if (snap.tiles.length !== b.tiles.length)
            throw new Error('Snapshot board dimensions do not match the map.');
        b.tiles = [...snap.tiles];
        b.decorations = structuredClone(snap.decorations);
        b.refreshDecorOverlay();
        b.heroSkills = cleanHeroSkills(snap.heroSkills ?? b.heroSkills, Object.keys(snap.heroSkills ?? b.heroSkills));
        b.affinityScores = { ...(snap.affinityScores ?? b.affinityScores) };
        b.units = snap.units.map(saved => {
            const u = unitFromSnap(saved);
            u.moveBudgetUsed = saved.moveBudgetUsed ?? 0;
            if (u.side === 'player' && !u.summoned) {
                for (const type of weaponTypesForClass(u.classId)) {
                    const id = `${type}Weapon` as const;
                    if (b.heroSkills[u.name]?.[id] == null && saved.weaponSkills?.[type] != null)
                        b.heroSkills[u.name] = { ...b.heroSkills[u.name], [id]: saved.weaponSkills[type] };
                }
                u.weaponSkills = trainedWeaponSkills(b.heroSkills, u.name, u.classId);
                u.healingSkill = skillValue(b.heroSkills, u.name, 'healing');
            }
            return u;
        });
        b.turn = snap.turn;
        b.turnOrder = [...snap.turnOrder];
        if (snap.units.some(u => u.initiative == null || u.initiativeRoll == null)) {
            b.rollOpeningInitiative(b.units.filter(takesTurns));
            b.turnOrder = b.sortByInitiative(b.units.filter(takesTurns));
        }
        b.finished = new Set(b.units.filter(u => u.moved && u.acted).map(u => u.id));
        b.activeUnitId = snap.turnBegan ? snap.activeUnitId : null;
        b.selectedId = b.activeUnitId;
        b.turnRestrained = snap.turnRestrained;
        b.lootWeapons = [...snap.lootWeapons];
        b.lootEquipment = [...snap.lootEquipment];
        b.lootEmber = snap.lootEmber;
        b.lootRations = snap.lootRations ?? 0;
        b.ownedWeapons = new Set(snap.ownedWeapons);
        b.questFound = [...(snap.questFound ?? [])];
        b.questPickups = b.questPickups.filter(p => !b.questFound.includes(p.key));
        b.webZones = snap.webZones.map(z => ({ ...structuredClone(z), cells: new Set(z.cells) }));
        b.iceStormZones = (snap.iceStormZones ?? []).map(z => ({ ...structuredClone(z), cells: new Set(z.cells) }));
        b.auraZones = snap.auraZones.map(z => ({ ...structuredClone(z), cells: new Set(z.cells) }));
        b.enmity = enmityFromSnapshot(snap.enmity);
        b.awake = new Set(snap.awake ?? []);
        if (snap.explored)
            b.vis = unpackExplored(snap.explored, b.tiles.length) ?? new Uint8Array(b.tiles.length);
        b.refreshFog();
        b.winAvailable = snap.winAvailable;
        b.result = null;
        b.legacyMetadata = structuredClone(snap);
        b.out = [];
        return b;
    }
    /** Original-compatible snapshot for campaign/save integration; no actions are in-flight. */
    snapshotForEmber(): BattleSnapshot {
        return { ...structuredClone(this.legacyMetadata), missionId: this.mission.id, turn: this.round, phase: this.activeUnit?.side === 'enemy' ? 'enemy' : 'player', units: structuredClone(this.units) as BattleUnitSnap[], tiles: [...this.tiles], decorations: structuredClone(this.decorations), turnOrder: [...this.turnOrder], activeUnitId: this.activeUnitId, selectedId: this.selectedId, lootEmber: this.lootEmber, lootRations: this.lootRations, questFound: [...this.questFound], lootWeapons: [...this.lootWeapons], lootEquipment: [...this.lootEquipment], ownedWeapons: [...this.ownedWeapons], heroSkills: structuredClone(this.heroSkills), affinityScores: { ...this.affinityScores }, enmity: enmityToSnapshot(this.enmity), webZones: this.webZones.map(z => ({ ...z, cells: [...z.cells] })), iceStormZones: this.iceStormZones.map(z => ({ ...z, cells: [...z.cells], createdAt: z.createdAt ?? 0, center: z.center ?? { x: 0, y: 0 }, radius: z.radius ?? 0 })), auraZones: this.auraZones.map(z => ({ ...z, cells: [...z.cells] })), log: [...(this.legacyMetadata.log ?? [])], winAvailable: this.winAvailable, chestLoot: this.legacyMetadata.chestLoot ?? null, pendingDialog: this.legacyMetadata.pendingDialog ?? null, turnRestrained: this.turnRestrained, turnBegan: !!this.activeUnitId, explored: this.fogged ? packExplored(this.vis) : undefined, awake: this.fogged ? [...this.awake] : undefined };
    }
    private emitUnitChanges(before: Map<string, string>) { for (const u of this.units) {
        if (!before.has(u.id))
            this.out.push({ type: 'summoned', unitId: u.id, sourceId: u.summonerId ?? '', unit: structuredClone(u) });
        else if (before.get(u.id) !== JSON.stringify(u)) {
            this.out.push({ type: 'unitChanged', unitId: u.id, unit: structuredClone(u) });
            this.out.push({ type: 'statusChanged', unitId: u.id, statuses: { poisoned: u.poisoned, poisonTier: u.poisonTier, diseased: u.diseased, bleeding: u.bleeding, stunned: u.stunned, stunTurns: u.stunTurns, asleep: u.asleep, sleepTurns: u.sleepTurns, fearTurns: u.fearTurns, blessedRoundsLeft: u.blessedRoundsLeft, blessedHitBonusPct: u.blessedHitBonusPct } });
        }
    } }
    snapshot(): BattleSave { const pack = (z: {
        cells: Set<string>;
    }) => ({ ...z, cells: [...z.cells] }); return { version: 1, mission: structuredClone(this.mission), tiles: [...this.tiles], decorations: structuredClone(this.decorations), units: structuredClone(this.units), round: this.round, turnOrder: [...this.turnOrder], activeUnitId: this.activeUnitId, finished: [...this.finished], rngSeed: this.rngSeed, rngCalls: this.rngCalls, elevations: [...this.elevations], winAvailable: this.winAvailable, result: this.result, loot: { weapons: [...this.lootWeapons], equipment: [...this.lootEquipment], ember: this.lootEmber, rations: this.lootRations }, heroSkills: structuredClone(this.heroSkills), affinityScores: { ...this.affinityScores }, zones: { web: this.webZones.map(pack), ice: this.iceStormZones.map(pack), aura: this.auraZones.map(pack) }, enmity: [...this.enmity].map(([id, row]) => [id, [...row]]), awake: [...this.awake], fog: [...this.vis], restrained: this.turnRestrained, ownedWeapons: [...this.ownedWeapons], questFound: [...this.questFound], legacyMetadata: structuredClone(this.legacyMetadata), castSequence: this.castSequence, debugFreeCast: this.debugFreeCast, partyLeader: this.partyLeader, questPickups: structuredClone(this.questPickups) }; }
    static restore(save: BattleSave) { const b = new Battle({ ...save.mission, tiles: save.tiles, decorations: save.decorations }, { seed: save.rngSeed - save.mission.index * 97 }); b.mission=structuredClone(save.mission); b.units = structuredClone(save.units); b.turn = save.round; b.turnOrder = [...save.turnOrder]; b.activeUnitId = save.activeUnitId; b.selectedId = save.activeUnitId; b.finished = new Set(save.finished); b.elevations = [...save.elevations];b.refreshDecorOverlay(); b.winAvailable = save.winAvailable; b.result = save.result; b.rngSeed = save.rngSeed; b.resetRng(save.rngCalls); b.lootWeapons = [...save.loot.weapons]; b.lootEquipment = [...save.loot.equipment]; b.lootEmber = save.loot.ember; b.lootRations = save.loot.rations; b.heroSkills = structuredClone(save.heroSkills ?? {}); b.affinityScores = { ...save.affinityScores }; const unpack = (z: unknown) => { const obj = z as Record<string, unknown>; return { ...obj, cells: new Set(obj.cells as string[]) }; }; b.webZones = save.zones.web.map(unpack) as typeof b.webZones; b.iceStormZones = save.zones.ice.map(unpack) as typeof b.iceStormZones; b.auraZones = save.zones.aura.map(unpack) as typeof b.auraZones; b.enmity = new Map(save.enmity.map(([id, row]) => [id, new Map(row)])); b.awake = new Set(save.awake); b.vis = Uint8Array.from(save.fog); b.turnRestrained = save.restrained; b.partyLeader = save.partyLeader; b.questPickups = structuredClone(save.questPickups); b.ownedWeapons=new Set(save.ownedWeapons??[]);b.questFound=[...(save.questFound??[])];b.legacyMetadata=structuredClone(save.legacyMetadata??{});b.castSequence=save.castSequence??0;b.debugFreeCast=save.debugFreeCast??false;b.out = []; return b; }
}
