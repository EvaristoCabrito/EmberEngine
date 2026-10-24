import { openingPartyCamera } from '../render/partyCamera';
import { CarriedLight } from '../render/carriedLight';
import { getDevGfx } from './gfx/three/devGfx';
// Engine2's 3D battlefield, standing in for Ember's 2D BattleCanvas (same props). Ember's
// BattleEngine still runs every rule, turn and AI decision; this only draws its state on the 3D
// map (Engine2 terrain, decorations, full-resolution sprites) and feeds clicks back as hexes.
// Units snap/walk between hexes with idle, walk, attack and death poses; the grid (reach,
// targets, route, turn marker, cursor) is Ember's own, hidden on city hubs. Spell effects use
// OG Ember's systems, canvas art and event callbacks through Engine2 coordinate adapters.

import { useEffect, useRef } from "react";
import type { BattleEngine } from "./engine";
import type { HudSnapshot, Unit } from "./types";
import { DioramaView, type GridMark } from "../editor/ember/dioramaView";
import { tacticalGridStyleQuiet as tacticalGridStyle, GRID_ROUTE, GRID_MOVE, GRID_ENEMY_TARGET, GRID_OFFHAND_TARGET } from "./tacticalGrid";
import { CHEST_DECOR_IDS, TERRAIN } from "./data";
import { tileAt } from "./pathfinding";

/** Inner edge of Ember's hex border mesh (buildHexBorder(0.47) on a 0.5 hex). */
const BORDER_INNER = 0.47 / 0.5;

/** Ember's ThreeBattleRenderer fadedFill: an rgba colour with its alpha scaled by `fade`. */
function fadedFill(fill: string, fade: number): string {
  const m = /rgba?\(([^,]+),([^,]+),([^,)]+)(?:,([^)]+))?\)/.exec(fill);
  if (!m) return fill;
  const a = (m[4] !== undefined ? Number(m[4]) : 1) * fade;
  return `rgba(${m[1]},${m[2]},${m[3]},${(Math.round(a * 50) / 50).toFixed(2)})`;
}
import { UnitActor } from "../units/actor";
import { unitForSpawn, type Pose } from "../units/catalog";
import type { Mission } from "../ember/types";
import { BattleAtmosphere3D } from "./battleAtmosphere3d";
import { BattleVignettes } from "./BattleVignettes";
import { moonlightFor } from "./moonPhase";
import { ArrowFx3D } from "./battleProjectiles3d";
import { BattleHud3D } from "./battleHud3d";
import { BattleCinematics } from "./battleCinematics";
import { CinematicDirector } from "../render/cinematicCamera";
import * as THREE from "three";
import { BattleSpells3D } from "./battleSpells3d";
import { BattleSpellOverlay3D } from "./battleSpellOverlay3d";

type Any = Record<string, any>; // the engine's private helpers (hexCenter, active) are read as-is

/** The caster's point of light at full strength (Frost's strike flash is 8). */
const CAST_LIGHT = 9;
/** Skills that are swings, shoves and arrow shots, not spells: no casting light. */
const NOT_CASTS = new Set(["doubleStrike", "cleave", "piercingThrust", "sweep", "trip", "shoulderSmash", "stampede", "tendrilSwipe",
  "longShot", "bloodyShot", "multiShot", "piercing", "bullRush", "shieldBash", "executionerStrike"]);
/** Malrec's own lights: every spell he casts glows blue, his attacks a regular yellow. */
const MALREC_CAST_LIGHT = 0x4f9dff, MALREC_ATTACK_LIGHT = 0xffd45a;
/** Spell colours for the casting light; anything else glows warm white. */
const CAST_TINT: Record<string, number> = {
  fireball: 0xff8a3c, burningHands: 0xff8a3c, frost: 0x9fd6ff, iceStorm: 0x9fd6ff, lightning: 0xb8d2ff, shock: 0xb8d2ff,
  causticVenom: 0x86ff7c, minorVenom: 0x86ff7c, poisonBreath: 0x86ff7c, lifeDrain: 0xff5a6e, magicMissile: 0xc7b4ff, magicMissileV2: 0xc7b4ff,
  phantasmalForce: 0xc7b4ff, fantomForce: 0xc7b4ff, webOfDreams: 0xd8c8ff, warp: 0xb48cff,
};
/** Ember's art pools → Engine2's poses (same source sheets). Earlier entries win when two pools
 * share an image (a sprite's `sprites` pool is its idle). */
const ART_POSES: [string, Pose][] = [
  ["idles", "idle"], ["sprites", "idle"], ["idles2", "idle2"],
  ["walks", "walk"], ["walksLeft", "walkLeft"], ["walks2", "walk"], ["walksLeft2", "walkLeft"], ["walksUp", "walkUp"], ["walksDown", "walkDown"],
  ["attacks", "attack"], ["attacksLeft", "attackLeft"], ["attacks2", "attack2"], ["attacks2Left", "attack2Left"], ["attacksShort", "attackShort"],
  ["casts", "cast"], ["castsLeft", "castLeft"], ["castsHeal", "heal"], ["counters", "counter"], ["countersLeft", "counter"],
  ["deaths", "death"], ["deaths2", "death2"], ["hits", "hit"], ["hits2", "hit2"],
];

export function Battle3D({
  engine,
  onHud,
  onInspectUnit,
  paused,
  onTileReadout,
}: {
  engine: BattleEngine;
  onHud: (hud: HudSnapshot) => void;
  onInspectUnit?: (unitId: string) => void;
  paused?: boolean;
  onTileReadout?: (showing: boolean) => void;
}) {
  const hostRef = useRef<HTMLDivElement>(null);
  const hudRef = useRef<HTMLCanvasElement>(null);
  const spellArtRef = useRef<HTMLCanvasElement>(null);
  const spellFxRef = useRef<HTMLCanvasElement>(null);
  const pausedRef = useRef(paused);
  pausedRef.current = paused;
  const onHudRef = useRef(onHud);
  onHudRef.current = onHud;

  useEffect(() => {
    const host = hostRef.current!;
    const view = new DioramaView(host);
    const e = engine as unknown as Any;
    // the board only: battle units come from the engine, not the map's spawn list
    // Ember's loading curtain lifts on "ember:battle-ready": sent once the board and its
    // decorations are in the scene and a frame has been drawn.
    // GameApp's battle bar shows these real steps: the board's decoration images, then shaders.
    const loadStep = (step: "board" | "shaders", done: number, total: number) =>
      window.dispatchEvent(new CustomEvent("ember:battle-step", { detail: { step, done, total } }));
    void view.setMission({ ...(engine.mission as unknown as Mission), playerSpawns: [], enemySpawns: [], neutralSpawns: [] }, (done, total) => loadStep("board", done, total))
      .then(async () => {
        // Compile the preserved spell materials and upload existing textures before the
        // gameplay loading screen lifts; a first cast must not initialize GPU state mid-frame.
        await view.stage.prepareMaterials((done, total) => loadStep("shaders", done, total));
        // gameplay limits: zoom in right up against a unit, zoom out and pan no further than the
        // user's Tática reference view (see CameraRig.maxViewWidth / panBounds)
        view.rig.minDist = 1;
        view.rig.maxDist = Math.max(view.rig.maxDist * 4, 400);
        view.rig.maxViewWidth = 58;
        view.rig.panBounds = view.board!.bounds();
        const party = (engine.units as Unit[]).filter(unit => unit.side === 'player' && unit.alive);
        const points = party.map(unit => {
          const cell = view.board!.cell(unit.x, unit.y);
          return { x: cell.x, y: view.groundAt(cell.x, cell.z), z: cell.z };
        });
        const opening = openingPartyCamera(points, view.stage.camera.aspect, view.rig.yaw, view.rig.pitch);
        if (opening) {
          view.rig.target.set(opening.target.x, opening.target.y, opening.target.z);
          view.rig.dist = Math.min(view.rig.farthest, Math.max(view.rig.minDist, opening.distance));
          view.rig.apply();
        }

        requestAnimationFrame(() => window.dispatchEvent(new Event("ember:battle-ready")));
      });

    const actors = new Map<string, { actor: UnitActor; dead: boolean; lastAction: unknown; px?: number; pz?: number; facing?: number }>();
    // Ember's engine decides each unit's frame (unitVisual), timed to its own wind-ups, arrow
    // releases, hits, counters and deaths: find which pose and point of the sheet that image is,
    // and show the same on the 3D card.
    const frameIndex = new Map<string, Map<unknown, { pose: Pose; k: number }>>();
    const poseOf = (sprite: string, img: unknown) => {
      let index = frameIndex.get(sprite);
      if (!index) {
        index = new Map();
        const art = e.art as Any;
        for (const [pool, pose] of ART_POSES) {
          const frames = art[pool]?.[sprite] as unknown[] | undefined;
          frames?.forEach((f, i) => { if (!index!.has(f)) index!.set(f, { pose, k: (i + 0.5) / frames.length }); });
        }
        const dirs = art.walkDirs?.[sprite] as { front: unknown; back: unknown; side: unknown } | undefined;
        if (dirs) { index.set(dirs.front, { pose: "walkFront", k: 0.5 }); index.set(dirs.back, { pose: "walkBack", k: 0.5 }); index.set(dirs.side, { pose: "walkSide", k: 0.5 }); }
        frameIndex.set(sprite, index);
      }
      return index.get(img);
    };
    const toWorld = (wx: number, wy: number) => {
      const a0 = e.effectAnchor(0, 0) as { worldX: number; worldY: number; tile: number };
      const L = view.board!.layout;
      return { x: (wx - a0.worldX) / a0.tile - L.ox, z: (wy - a0.worldY) / a0.tile - L.oz };
    };
    const syncUnits = () => {
      if (!view.board) return;
      const seen = new Set<string>();
      for (const u of engine.units as Unit[]) {
        seen.add(u.id);
        let entry = actors.get(u.id);
        if (!entry) {
          const def = unitForSpawn({ name: u.name, classId: u.classId }, u.side === "player" ? "player" : u.side === "enemy" ? "enemy" : "neutral");
          if (!def) continue;
          const actor = new UnitActor(def);
          actor.silent = true; // Ember's engine plays the battle's sounds
          view.stage.scene.add(actor.mesh);
          void actor.ready().catch(() => undefined);
          entry = { actor, dead: false, lastAction: null };
          actors.set(u.id, entry);
        }
        const { actor } = entry;
        const anchor = e.unitAnchor(u) as { worldX: number; worldY: number };
        const p = toWorld(anchor.worldX, anchor.worldY);
        // Facing on the 3D camera's screen, not Ember's fixed 2D one (with the camera turned, or
        // on a diagonal step, Ember's left/right is not the screen's: units moonwalked). Moving:
        // the way the unit actually goes on screen. Otherwise: where the engine has it looking
        // (faceDx/faceDy, toward its last move or target), turned to the camera.
        const yaw = view.rig.facingYaw;
        const onScreen = (dx: number, dz: number) => dx * Math.cos(yaw) - dz * Math.sin(yaw);
        // only a real move steers by its step: an attack's lunge and pull-back would turn the
        // attacker away from its target mid-swing
        const stepping = (e.active as Any | null)?.type === "move" && (e.active as Any).id === u.id;
        const stepX = entry.px === undefined || !stepping ? 0 : onScreen(p.x - entry.px, p.z - (entry.pz ?? p.z));
        const lookX = u.faceDx != null && u.faceDy != null ? onScreen(u.faceDx, u.faceDy) : (u.facing ?? 1);
        const facing = Math.abs(stepX) > 1e-4 ? Math.sign(stepX) : Math.abs(lookX) > 1e-6 ? Math.sign(lookX) : (entry.facing ?? 1);
        entry.px = p.x; entry.pz = p.z; entry.facing = facing;
        actor.x = p.x; actor.z = p.z;
        actor.facing = facing < 0 ? -1 : 1;
        // fog of war: an enemy out of the party's sight is not drawn (Ember's unitHidden)
        actor.mesh.visible = (u.fade ?? 1) > 0 && !engine.unitHidden(u);
        actor.lightFade = u.alive && !engine.unitHidden(u) ? (u.fade ?? 1) : 0;
        const shown = poseOf(u.sprite, (e.unitVisual(u, (e.effectAnchor(0, 0) as { tile: number }).tile) as { img: unknown }).img);
        if (shown && actor.has(shown.pose)) {
          // Ember chose a left or right cut for its own screen; take the one that matches this
          // camera, keeping any sprite whose cuts read the other way round (the Lancer) as Ember had it
          let cut = shown.pose;
          const base = cut.replace(/Left$/, "") as Pose, left = `${base}Left` as Pose;
          if (base !== cut || actor.has(left)) {
            const swapped = cut.endsWith("Left") !== ((u.facing ?? 1) < 0);
            const wantLeft = (facing < 0) !== swapped;
            cut = wantLeft && actor.has(left) ? left : base;
          }
          actor.drive(cut, shown.k);
          entry.dead = !u.alive;
          continue;
        }
        actor.drive("idle", null); // no matching cut: fall back to playing poses on our own clock
        const active = e.active as Any | null;
        if (!u.alive) {
          if (!entry.dead) { entry.dead = true; void actor.play("death"); }
          continue;
        }
        if (entry.dead) { entry.dead = false; actor.revive(); }
        const moving = active?.type === "move" && active.id === u.id;
        const attacking = active && (active.type === "combat" || active.type === "spell" || active.type === "heal") && active.att === u.id;
        if (attacking && entry.lastAction !== active) {
          entry.lastAction = active;
          void actor.play(active.type === "combat" ? (String(active.stage ?? "").startsWith("counter") ? "counter" : "attack") : "cast");
        } else if (moving && entry.lastAction !== active) {
          entry.lastAction = active;
          void actor.play(actor.has("walk") ? "walk" : "idle");
        } else if (!moving && !attacking && entry.lastAction) {
          entry.lastAction = null;
          void actor.play("idle");
        }
      }
      for (const [id, entry] of actors) if (!seen.has(id)) { view.stage.scene.remove(entry.actor.mesh); entry.actor.dispose(); actors.delete(id); }
    };

    // Ember's battle grid, exactly as its ThreeBattleRenderer.syncOverlay drew it: the engine's
    // own boardOverlayLayers (movement wash + edges, targets, spell areas…), the route, the
    // active-turn marker and the cursor. City hubs and free-roam maps show no grid — only the
    // cursor, so the player still sees which hex a click goes to.
    const gridMarks = (): GridMark[] => {
      const marks: GridMark[] = [];
      const add = (x: number, y: number, color: string, rOut: number, rIn?: number, flat = false) => marks.push({ x, y, color, rOut, rIn, flat });
      const border = (r: number) => r * BORDER_INNER; // Ember's buildHexBorder(0.47) of a 0.5 hex
      if (!engine.mission.hub && !engine.mission.explore) {
        const fade = (e.overlayFade as number | undefined) ?? 1;
        for (const layer of engine.boardOverlayLayers()) {
          const f = layer.fill === GRID_MOVE ? fade : 1;
          const style = tacticalGridStyle(layer.fill);
          for (const c of layer.cells) add(c.x, c.y, fadedFill(style.fill, f), 1);
          if (layer.fill === GRID_MOVE || layer.fill === GRID_ENEMY_TARGET || layer.fill === GRID_OFFHAND_TARGET) {
            for (const c of layer.cells) add(c.x, c.y, fadedFill(style.edge, f), 0.94, border(0.94));
          }
        }
        const route = engine.movementPreview();
        route.forEach((c, i) => {
          const last = i === route.length - 1;
          if (last) add(c.x, c.y, "rgba(8,12,16,0.95)", 0.985, border(0.985)); else add(c.x, c.y, "rgba(8,12,16,0.95)", 0.16);
          if (last) add(c.x, c.y, "rgba(220,226,235,0.12)", 1.01, border(1.01));
          if (last) add(c.x, c.y, GRID_ROUTE, 0.94, border(0.94)); else add(c.x, c.y, GRID_ROUTE, 0.12);
        });
        const active = engine.activeTurnHighlight();
        if (active) {
          if (active.player) {
            add(active.x, active.y, "rgba(220,226,235,0.012)", 1.035, border(1.035));
            add(active.x, active.y, "rgba(220,226,235,0.022)", 1.01, border(1.01));
            add(active.x, active.y, "rgba(220,226,235,0.04)", 0.985, border(0.985));
          }
          add(active.x, active.y, "rgba(12,20,25,0.85)", 0.98, border(0.98));
          add(active.x, active.y, active.player ? fadedFill(active.fill, 0.28) : active.fill, 0.94, border(0.94));
        }
      }
      const cur = (e.hover ?? e.cursor) as { x: number; y: number } | null;
      if (cur && tileAt(engine.tiles, engine.cols, cur.x, cur.y) !== "void" && e.explored(cur.x, cur.y)) {
        const blocked = !TERRAIN[tileAt(engine.tiles, engine.cols, cur.x, cur.y)].passable;
        add(cur.x, cur.y, "rgba(8,12,16,0.95)", 0.985, border(0.985), true);
        add(cur.x, cur.y, blocked ? "rgba(231,133,115,0.95)" : "rgba(220,226,235,1)", 0.94, border(0.94), true);
      }
      return marks;
    };

    // The HUD's camera buttons (Normal/Tática, tilt, turn, reset) still drive Ember's
    // engine.cameraTilt / cameraTiltSide. Their changes move the 3D rig by the same degrees, on
    // top of whatever the mouse did; back to 0/0 (the reset button) is the rig's normal view.
    // Tática (engine.tacticsCamera) is Ember's original camera: the old-school 2D view, straight
    // overhead with flat sprites. Leaving it lands on the normal view.
    let camTilt = e.cameraTilt as number, camSide = e.cameraTiltSide as number;
    let leavingFlat = false; // the HUD animates back to 0/0 on the way out of Tática: skip those steps
    const followCameraButtons = () => {
      // a camera button while a cinematic shot runs: the player's view comes back first
      if (cinematics.director.active && (e.tacticsCamera !== view.rig.flat || e.cameraTilt !== camTilt || e.cameraTiltSide !== camSide)) cinematics.director.skip();
      const flat = !!e.tacticsCamera;
      const tilt = e.cameraTilt as number, side = e.cameraTiltSide as number;
      if (flat !== view.rig.flat) {
        view.setFlat(flat);
        leavingFlat = !flat && (tilt !== 0 || side !== 0);
        if (!flat && !leavingFlat) view.resetAngle();
      }
      if (tilt === camTilt && side === camSide) return;
      if (flat || leavingFlat) {
        if (leavingFlat && tilt === 0 && side === 0) { leavingFlat = false; view.resetAngle(); }
        camTilt = tilt; camSide = side;
        return;
      }
      // the HUD normalizes the turn into -180..180 after each move; only the real change counts
      const dSide = ((side - camSide + 540) % 360) - 180;
      if (tilt === 0 && side === 0) view.resetAngle();
      else { view.tilt(tilt - camTilt); view.turn(dSide); }
      camTilt = tilt; camSide = side;
    };

    const atmosphere = new BattleAtmosphere3D(view, engine);
    const arrows = new ArrowFx3D(view, engine);
    // cinematic camera (stage 1): takes the camera for a shot and gives the player's exact view back
    const cinematics = new BattleCinematics(engine, new CinematicDirector(view.rig, view.stage.camera, () => view.surfaces, view.groundAt), (unitId) => {
      const actor = actors.get(unitId)?.actor;
      const u = (engine.units as Unit[]).find(x => x.id === unitId);
      if (!actor || !u?.alive || !actor.mesh.visible) return null;
      const feet = actor.mesh.position.clone();
      return [feet, actor.pickMesh.localToWorld(new THREE.Vector3(0, 1, 0))];
    });
    // a unit's body centre: the middle of its card (the trimmed frame of what it shows)
    const spells = new BattleSpells3D(view, engine, (unitId) => {
      const entry = actors.get(unitId);
      return entry?.actor.mesh.visible ? entry.actor.pickMesh.localToWorld(new THREE.Vector3(0, 0.5, 0)) : null;
    });
    // a unit's card: its body centre and its height (potion bursts are sized and centred on it)
    const spellOverlay = new BattleSpellOverlay3D(spellArtRef.current!, spellFxRef.current!, view, engine, (unitId) => {
      const entry = actors.get(unitId);
      if (!entry?.actor.mesh.visible) return null;
      const pick = entry.actor.pickMesh;
      return { centre: pick.localToWorld(new THREE.Vector3(0, 0.5, 0)), height: pick.localToWorld(new THREE.Vector3(0, 1, 0)).distanceTo(pick.localToWorld(new THREE.Vector3(0, 0, 0))) };
    });
    const unitHud = new BattleHud3D(hudRef.current!, view, engine);
    // Ember's own debug handle (its BattleCanvas set the same): the live engine, for QA scripts
    const w = window as Window & { __emberEngine?: BattleEngine };
    w.__emberEngine = engine;
    // QA handle for the camera (player rig + cinematic director), same idea as __emberEngine
    (window as Window & { __emberCamera?: unknown }).__emberCamera = { rig: view.rig, director: cinematics.director, camera: view.stage.camera, actors };
    // tonight's moon lights night battles by its phase (Mission.moonPhase, set by the campaign);
    // a time of day changed live (QA, later the battle's own clock) re-lights the scene too
    let moon: string | undefined, tod: string | undefined;
    const followMoon = () => {
      const phase = engine.mission.moonPhase, t = engine.mission.timeOfDay;
      if (phase === moon && t === tod) return;
      const relight = tod !== undefined && t !== tod && !!t;
      moon = phase; tod = t;
      if (relight) view.stage.timeOfDay = t as typeof view.stage.timeOfDay;
      view.stage.setMoon(phase ? moonlightFor(phase) : { strength: 1 });
    };

    // A point of light at a caster's hands while it casts. One light, always in the scene (adding or
    // removing lights mid-battle recompiles every lit material), dark unless someone is casting.
    // Physical skills and arrow shots are not casts; heals glow gold, elements take their colour.
    const castLight = new THREE.PointLight(0xffe6c0, 0, 6, 1.8);
    view.stage.scene.add(castLight);
    let castGlow = 0;
    const castColor = new THREE.Color();
    const syncCastLight = (dt: number) => {
      const a = e.active as Any | null;
      let id: string | null = null, step: Any | null = null, attack = false;
      if (a?.type === "windup" && a.pose === "cast") { id = a.id; step = (e.queue as Any[])[0] ?? null; }
      else if ((a?.type === "spell" || a?.type === "heal") && !NOT_CASTS.has(String(a.spellKind ?? ""))) { id = a.att; step = a; }
      else if (a?.type === "windup" || a?.type === "combat") {
        // Malrec alone also lights his attacks (a plain yellow light); nobody else's swings glow
        const who = a.type === "windup" ? a.id : a.att;
        if ((e.units as Unit[]).find(u => u.id === who)?.sprite === "malrec") { id = who; attack = true; }
      }
      const entry = id ? actors.get(id) : undefined;
      const on = !!entry?.actor.mesh.visible;
      castGlow = Math.max(0, Math.min(1, castGlow + dt * (on ? 5 : -2.5)));
      if (on) {
        const pick = entry!.actor.pickMesh;
        const hands = pick.localToWorld(new THREE.Vector3(0, 0.6, 0));
        castLight.position.copy(hands).addScaledVector(view.stage.camera.getWorldDirection(new THREE.Vector3()), -0.45);
        const malrec = (e.units as Unit[]).find(u => u.id === id)?.sprite === "malrec";
        castColor.set(attack ? MALREC_ATTACK_LIGHT : malrec ? MALREC_CAST_LIGHT
          : step?.type === "heal" || a?.heal ? 0xffd98a : CAST_TINT[String(step?.spellKind ?? "")] ?? 0xffe6c0);
        castLight.color.copy(castColor);
      }
      castLight.intensity = CarriedLight.enabled ? CAST_LIGHT * castGlow * (0.92 + Math.sin(performance.now() / 70) * 0.08) : 0;
    };

    let hudClock = 0, lastTick = 0, chestKey = "";
    view.stage.onFrame((dt) => {
      // Options → Graphics: the sun's (or moon's) cast shadows and the map's local lights
      const gfx = getDevGfx();
      view.stage.sun.castShadow = gfx.realShadows;
      CarriedLight.enabled = gfx.localLights;
      followMoon();
      followCameraButtons();
      // keeps the engine's sight/fog current exactly as its own renderer does every frame
      e.updateCameraLayout(1280, 800);
      // The engine's clock follows real time (Ember's 0.05 s per-step cap kept, in sub-steps):
      // sounds are scheduled on the real clock (Neera's bow snap lands 3 s after her draw), so a
      // frame rate below 20 fps must not slow the engine down and put them out of sync.
      const now = performance.now();
      let real = Math.min(0.25, (now - (lastTick || now)) / 1000);
      lastTick = now;
      if (!pausedRef.current) while (real > 1e-4) { const step = Math.min(0.05, real); engine.tick(step); real -= step; }
      syncUnits();
      syncCastLight(dt);
      // an opened chest leaves the engine's decoration list: take its card off the 3D board too
      // (checked whenever the chest count or the loaded card count changes)
      const engineDecor = engine.decorations as { id: string; x: number; y: number }[];
      const key = `${engineDecor.filter((d) => CHEST_DECOR_IDS.has(d.id)).length}|${view.decor.count}`;
      if (key !== chestKey) { chestKey = key; view.decor.dropOpenedChests(engineDecor); }
      // the cinematic camera moves first, so cards, effects and the HUD all face/project onto this frame's view
      cinematics.director.update(dt);
      const yaw = view.rig.facingYaw;
      for (const { actor } of actors.values()) actor.update(dt, yaw, view.unitGroundAt, view.flatTop);
      view.setGrid(gridMarks());
      atmosphere.sync(dt);
      arrows.sync();
      spells.sync(pausedRef.current ? 0 : dt);
      spellOverlay.sync(pausedRef.current ? 0 : dt);
      unitHud.render(actors);
      hudClock += dt;
      if (hudClock > 0.1) { hudClock = 0; onHudRef.current(engine.getHud()); }
    });

    // clicks: the hex under the pointer, handed to the engine as that hex's own screen point
    const cssPoint = (x: number, y: number) => e.hexCenter(x, y) as { cx: number; cy: number };
    // the hex under the pointer; on a Warp gate's body (it stands above its hex) that gate's hex,
    // unless the floor under the pointer is nearer than the gate (something in front of it)
    const pickCell = (x: number, y: number) => {
      const cell = view.cellAt(x, y), gate = spellOverlay.gateAt(x, y);
      if (!gate) return cell;
      if (!cell) return gate;
      const cam = view.stage.camera.position, at = (c: { x: number; y: number }) => { const p = view.board!.cell(c.x, c.y); return Math.hypot(p.x - cam.x, p.z - cam.z); };
      return at(cell) > at(gate) ? gate : cell;
    };
    view.rig.onClick = (ev) => {
      if (pausedRef.current) return;
      const cell = pickCell(ev.clientX, ev.clientY);
      if (!cell) return;
      const p = cssPoint(cell.x, cell.y);
      engine.pointerDown(p.cx, p.cy, "click");
    };
    const onMove = (ev: PointerEvent) => {
      const cell = pickCell(ev.clientX, ev.clientY);
      if (!cell) { onTileReadout?.(false); return; }
      const p = cssPoint(cell.x, cell.y);
      engine.pointerMove(p.cx, p.cy);
    };
    // a short right click (no drag) is Ember's: inspect a unit, or step back / cancel
    let rightDown: { x: number; y: number } | null = null;
    const onDown = (ev: PointerEvent) => { if (ev.button === 2) rightDown = { x: ev.clientX, y: ev.clientY }; };
    const onUp = (ev: PointerEvent) => {
      if (ev.button !== 2 || !rightDown || pausedRef.current) return;
      const moved = Math.hypot(ev.clientX - rightDown.x, ev.clientY - rightDown.y) > 5;
      rightDown = null;
      if (moved) return;
      const cell = view.cellAt(ev.clientX, ev.clientY);
      const unit = cell ? (engine.units as Unit[]).find(u => u.alive && u.x === cell.x && u.y === cell.y) : null;
      if (unit) { onInspectUnit?.(unit.id); return; }
      const hud = engine.getHud();
      const showAct = hud.mode === "awaitAction" || hud.mode === "awaitAttack" || hud.mode === "selected" || hud.mode === "awaitSpell" || hud.mode === "awaitOffHand";
      if (!showAct || hud.busy) return;
      if (engine.mission.explore && (hud.mode === "selected" || hud.mode === "awaitAction")) { engine.deselect(true); return; }
      engine.cancel();
    };
    const onKey = (ev: KeyboardEvent) => {
      if ((ev.target as HTMLElement | null)?.closest("input, textarea, select")) return;
      // C skips a cinematic shot (the engine already uses Escape and Space); the action plays on
      if (ev.code === "KeyC" && cinematics.director.active) { cinematics.director.skip(); return; }
      // Esc is the options menu's (GameApp); right-click cancels an action
      if (ev.code === "Escape" || pausedRef.current) return;
      engine.keyDown(ev.code);
    };
    const canvas = view.stage.renderer.domElement;
    canvas.addEventListener("pointermove", onMove);
    canvas.addEventListener("pointerdown", onDown);
    canvas.addEventListener("pointerup", onUp);
    window.addEventListener("keydown", onKey);
    return () => {
      canvas.removeEventListener("pointermove", onMove);
      canvas.removeEventListener("pointerdown", onDown);
      canvas.removeEventListener("pointerup", onUp);
      window.removeEventListener("keydown", onKey);
      CarriedLight.enabled = true;
      for (const { actor } of actors.values()) actor.dispose();
      atmosphere.dispose();
      castLight.removeFromParent(); castLight.dispose();
      arrows.dispose();
      cinematics.dispose();
      spells.dispose();
      spellOverlay.dispose();
      if (w.__emberEngine === engine) delete w.__emberEngine;
      delete (window as Window & { __emberCamera?: unknown }).__emberCamera;
      view.dispose();
    };
  }, [engine]);

  return (
    <div className="absolute inset-0">
      <div ref={hostRef} className="absolute inset-0 bg-black" onContextMenu={(ev) => ev.preventDefault()} />
      <canvas ref={spellFxRef} className="pointer-events-none absolute inset-0 block h-full w-full" />
      <canvas ref={spellArtRef} className="pointer-events-none absolute inset-0 block h-full w-full" />
      {/* Ember's unit HUD: HP bars, HP numbers, damage/miss/heal/level-up text (battleHud3d.ts) */}
      <canvas ref={hudRef} className="pointer-events-none absolute inset-0 block h-full w-full" />
      <BattleVignettes engine={engine} />
    </div>
  );
}
