import { ThreeTrees } from "./ThreeTrees";
import { vauBackdropBounds } from "../../vauBackdrop";
import { MagicMissileForeground } from "./MagicMissileForeground";
import { BARRICADE_LIKE_DECOR } from "../../data";
import { tacticalGridStyleQuiet as tacticalGridStyle, GRID_ROUTE, GRID_MOVE, GRID_ENEMY_TARGET, GRID_ENEMY_GLOW, GRID_OFFHAND_TARGET } from "../../tacticalGrid";
/** MILESTONE 1 (done) — terrain, ground/behind-layer decorations, and animated unit sprites all
 * render through a real Three.js scene instead of the Canvas2D-shim WebGL renderer, as the first
 * slice of migrating the battlefield to a genuine spatial rendering environment (see the
 * architecture note below). "Front"-layer/foreground props still render through the existing
 * Canvas2D-shim units canvas, unchanged, stacked on top — this renderer replaces the
 * ground/terrain canvas and everything meant to draw under a unit, never anything meant to draw
 * in front of one (see BattleCanvas.tsx).
 *
 * MILESTONE 2 (done) — real DirectionalLight + AmbientLight-ish HemisphereLight, and real cast
 * shadows from invisible per-unit/per-decoration boxes with a synthetic elevation (see
 * shadowCasterMaterial/updateSun and THREEJS_MILESTONE2_HANDOFF.md).
 *
 * MILESTONE 3 (in progress) — real world-space ground mist + GPU-instanced drift particles (see
 * ThreeAtmosphere.ts, which owns this entirely — this file only constructs it, syncs it once per
 * frame, and disposes it). No bloom/post-processing yet, that's Milestone 4.
 *
 * ARCHITECTURE: every tile mesh is built ONCE at its fixed WORLD position (the same formula
 * BattleEngine.effectAnchor already uses for worldX/worldY) and never moves again. Camera
 * panning moves the CAMERA, not the tiles — a real spatial scene, not screen-space coordinates
 * recomputed every frame. This is what makes later milestones (real DirectionalLight/shadows,
 * world-space fog, depth-sorted particles) possible without another rewrite: every tile has an
 * actual, stable position in a 3D world a light or a fog volume can reason about.
 *
 * COORDINATE CONVENTION: the orthographic camera's frustum is a STANDARD (left=0, right=cssW,
 * top=cssH, bottom=0) one — top > bottom, the normal orientation. An orthographic camera with
 * an inverted frustum (top < bottom), which is what a naive "world Y increases down the screen"
 * port of BattleEngine's cx/cy convention would want, renders nothing at all in this Three.js
 * version (confirmed empirically: identical scene/camera/mesh renders correctly with a standard
 * frustum and renders nothing with an inverted one, regardless of camera or mesh position —
 * some part of the projection/clipping pipeline silently assumes top > bottom). So the Y-flip
 * BattleEngine's convention needs happens elsewhere instead: every mesh is placed at Y = -wy
 * (see hexWorld) rather than +wy, and the camera's own Y position is offset by -camY - cssH to
 * match. Both are derived once, together, in render()/ensureBuilt() — verified numerically
 * against BattleEngine's own cx/cy formula, not just visually. This still lets every other
 * system (mouse picking via BattleEngine.cellAt, hover/selection highlighting, unit sprites on
 * the Canvas2D-shim units canvas, panBy/zoom) keep working completely unchanged: they all
 * operate in CSS-pixel space, which this renderer's on-screen RESULT still matches exactly —
 * only the intermediate Three.js coordinates carry the flip, nothing outside this file does. */

import * as THREE from "three";
import { isHexGroundVariant } from "../../assets";
import { GROUND_VARIANT_LAYERS, groundVariantMaps } from "../../../ember/tileVariants";
import { drawGroundTexture, drawHexGround, GROUND_TEXTURE_INSET, GROUND_TEXTURE_SPAN } from "../../hexGround";
import { configureWallDepth, createWallGeometry } from "./ThreeWalls";
import { lightTacticsMaterial, tacticsProp } from "./ThreeTacticsGeometry";
import { buildLandscape, type LandscapeSurface } from "./ThreeLandscape";
import { buildSteppedHexLandscape } from "./ThreeSteppedLandscape";
import { mapFloorRects, floorRectContains, floorRectParts, hasSquareMapBorder } from "../../mapFloor";
import { closeWatchtowerWalls } from "../../watchtowerDungeon";
const OUTER_WALL_TEXTURE = "/game/textures/walls/cave-v2.png?v=outer-wall-0.337.3";
const OUTER_WALL_COLOR = 0xb4a28b;
const WARP_PORTAL_VERTEX = `
varying vec2 vUv;
void main() {
  vUv = uv;
  gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
}`;
const WARP_PORTAL_FRAGMENT = `
uniform sampler2D u_map;
uniform float u_time;
varying vec2 vUv;
float hash21(vec2 p) {
  p = fract(p * vec2(123.34, 456.21));
  p += dot(p, p + 45.32);
  return fract(p.x * p.y);
}
float noise21(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash21(i), hash21(i + vec2(1.0, 0.0)), f.x),
    mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0, 1.0)), f.x), f.y);
}
void main() {
  vec4 base = texture2D(u_map, vUv);
  if (base.a < 0.003) discard;
  float flow = noise21(vUv * vec2(54.0, 34.0) + vec2(0.0, -u_time * 0.24));
  float current = sin(vUv.y * 46.0 + vUv.x * 12.0 - u_time * 2.1 + flow * 4.0);
  vec2 bend = vec2(sin(vUv.y * 20.0 + u_time * 1.35 + flow * 3.0) * 0.0028,
    (flow - 0.5) * 0.0022) * base.a;
  vec4 energy = texture2D(u_map, vUv + bend);
  float filament = pow(max(0.0, current * 0.5 + 0.5), 8.0) * energy.a;
  vec3 color = energy.rgb + vec3(0.015, 0.07, 0.2) * filament;
  gl_FragColor = vec4(color, energy.a);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;
import { decorationPlacementArt } from "../../data";
import { ThreeWater } from "./ThreeWater";
import { groundDepthLayer } from "./groundDepthLayer";
import { ThreeElevationSteps } from "./ThreeElevationSteps";
import { EffectComposer } from "three/examples/jsm/postprocessing/EffectComposer.js";
import { RenderPass } from "three/examples/jsm/postprocessing/RenderPass.js";
import { UnrealBloomPass } from "three/examples/jsm/postprocessing/UnrealBloomPass.js";
import { ShaderPass } from "three/examples/jsm/postprocessing/ShaderPass.js";
import { OutputPass } from "three/examples/jsm/postprocessing/OutputPass.js";
import { WEB_SHOT_TRAVEL, ZOOM_RADII, type BattleEngine, type BurningHandsV2VfxRequest, type CleaveVfxRequest, type MagicMissileV2VfxRequest, type VarreduraVfxRequest } from "../../engine";
import { BIG_HOUSE_DECOR_IDS, BLESS, CHEST_DECOR_IDS, DECOR_ART_SCALE, DECORATIONS, HOUSE_ART_SCALE, HOUSE_DECOR_IDS, SOLID_HOUSE_DECOR_IDS, TERRAIN, decorationFacing, decorationImage, decorationImageRetryWebp, isBossClass, placedFootprint } from "../../data";
import { footprint, footprintFrontRow, hexNeighbors, tileAt, unitSize } from "../../pathfinding";
import type { DecorationDef, DecorationPlacement, ElementalFxPlacement, MapTimeOfDay, TerrainId } from "../../types";
import { GroundAO, type AoOccluder } from "./ThreeGroundAO";
import { FOG_EXPLORED, FOG_UNSEEN, FOG_VISIBLE, FogMask } from "./ThreeFogMask";
import { BOUNCE_FRACTION, BOUNCE_HEIGHT, BOUNCE_RADIUS_MUL, LIGHT_DECAY, LIGHT_DEFS, LIGHT_RADIUS_MUL, MAP_LIGHT_DECAY, MAP_LIGHT_MIN_HEIGHT, UNIT_LIGHT_DEFS, flickerAt, type EnvLight, type LightDef } from "../../lighting";
import { ThreeAtmosphere } from "./ThreeAtmosphere";
import { decorationAnchor } from "../decorationAnchor";
import { FireballVFX } from "./FireballVFX";
import { CausticVenomVFX } from "./CausticVenomVFX";
import { getActivePhantasmalForceSettings, PhantasmalForceVFX } from "./PhantasmalForceVFX";
import { BlessVFX, getActiveBlessVfxSettings } from "./BlessVFX";
import { getActiveMagicMissileV2Settings, MagicMissileV2VFX } from "./MagicMissileV2VFX";
import { WebOfDreamsVFX } from "./WebOfDreamsVFX";
import { BurningHandsV2VFX, getActiveBurningHandsV2Settings } from "./BurningHandsV2VFX";
import { OldFireBall } from "./OldFireBall";
import { getDevGfx } from "./devGfx";
import { pixelPreset, ProceduralElementEmitter, type PixelElement } from "./ProceduralElementEmitter";
import { CleaveSweepVFX, VarreduraVFX, type VfxLightPool } from "./VarreduraVFX";

const SQRT3 = Math.sqrt(3);
const SPELL_VFX_LAYER = MagicMissileForeground.layer;
/** Must match BattleEngine's private boardPad() (tile * 2.4) — duplicated here rather than
 * exposed because it's one number, not worth widening engine.ts's public surface for. */
const BOARD_PAD_MUL = 2.4;

/** Fraction of a unit/decoration's drawn height used as its invisible shadow-casting elevation.
 * The visible art stays flat billboards (see module comment) — this is a synthetic "how tall
 * would this actually stand" number for the light alone, not a real 3D height. Kept modest on
 * purpose (see the AtmosphereFX caution in THREEJS_MILESTONE2_HANDOFF.md) — tune after checking
 * screenshots, not blindly. */
const UNIT_SHADOW_HEIGHT_SCALE = 0.85;
const DECOR_SHADOW_HEIGHT_SCALE = 0.9;

/** renderOrder floor for a decoration drawn in front of the ground-mist atmosphere sheets
 * (GroundMist2/3/4 now draw at 1.5-1.7, under the units — see ThreeAtmosphere.ts).
 * Those materials render with depthTest disabled, so they paint over anything behind them by
 * draw order alone; a prop that must read through the mist (any LIGHT_DEFS light source, or a
 * DecorationDef.aboveGroundMist prop) needs a renderOrder safely above that range instead. */
const ABOVE_GROUND_MIST_ORDER = 15;
// Authored scenery above fog must also follow Fog 01 (101) and Fog 5 (101+).
const ABOVE_FOG_SCENERY_ORDER = 110;

/** How far BELOW the ground plane (z=0) every standing shadow-caster's base now extends, world
 * units. The caster's base sits exactly AT z=0 — coplanar with the ground mesh it casts onto —
 * which is the textbook cause of peter-panning (the shadow test can't reliably tell which surface
 * is in front right at that shared boundary, so the shadow reads as detached from the caster's own
 * base). This was already solved once for the old box caster (see git history) by sinking its base
 * slightly below ground instead of touching it exactly, then removed when the caster became a
 * standing silhouette plane on the (wrong) assumption a flat plane wouldn't have the same
 * coplanarity problem — it does, since its base still touches z=0 either way. Only the caster's
 * own base moves; its top (what actually governs the shadow's shape/reach) stays exactly where it
 * was. Not a bias fix — this is geometry-only, per direct instruction to leave `bias`/`normalBias`
 * alone. Units and decorations get their OWN value each (not one shared constant) — sharing one
 * and bumping it for units visibly broke decorations, since they don't have the same proportions. */
const UNIT_SHADOW_GROUND_INSET = 0.5;
const SHADOW_UP_AXIS = new THREE.Vector3(0, 0, 1);
const STANDING_SHADOW_CARD = new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(1, 0, 0), Math.PI / 2);
const DECOR_SHADOW_GROUND_INSET = 3;

/** Direction the sun travels (not where it sits) — X/Y chosen so a shadow cast from height H
 * lands at world offset (0.6H, -0.8H), i.e. the exact same (0.6, 0.8) screen-space direction
 * (down-right; world Y is negated, see module comment) the old fake Canvas2D ellipse shadow
 * already used (see engine.ts's shadowDirX/shadowDirY) — so the sun's on-screen angle doesn't
 * visibly change when the fake shadow is eventually retired. Z=-1 (travelling toward -Z, i.e.
 * from the elevated shadow-caster boxes down onto the z=0 ground plane) derived alongside that:
 * a point at height H casts onto z=0 at (x - H*dir.x/dir.z, y - H*dir.y/dir.z) — solving for the
 * desired (0.6H, -0.8H) offset with dir.z=-1 gives dir.x=0.6, dir.y=-0.8 exactly. */
const SUN_DIRECTION = new THREE.Vector3(0.6, -0.8, -1).normalize();
const SUN_DISTANCE = 2000;
/** Per-map time of day (Mission.timeOfDay, picked in the Map Editor's "Iluminação"): which sky
 * light is the key light (Sun or Moon), the default key/ambient intensities the editor's sliders
 * snap to when the time is picked, the key light and sky-fill colors, and for dawn/dusk a low sun
 * elevation (long shadows) in place of Dev Controls' "Sol — altura". Dark night is the old Dev
 * Controls "Noite" (Moon 0.9, sky fill 12% of the daytime 2). */
export const TIME_OF_DAY_LIGHT: Record<MapTimeOfDay, { label: string; moon: boolean; key: number; ambient: number; keyColor: number; skyColor: number; elevation?: number }> = {
  day: { label: "Dia", moon: false, key: 3.5, ambient: 1.4, keyColor: 0xfff0d6, skyColor: 0xfff2df },
  noon: { label: "Meio-dia", moon: false, key: 5, ambient: 2, keyColor: 0xfff0d6, skyColor: 0xfff2df },
  dawn: { label: "Amanhecer", moon: false, key: 3.2, ambient: 1.3, keyColor: 0xffc8a8, skyColor: 0xf2d8d4, elevation: 20 },
  dusk: { label: "Entardecer", moon: false, key: 2.8, ambient: 1.1, keyColor: 0xffca9f, skyColor: 0xf7ddc3, elevation: 15 },
  brightNight: { label: "Noite clara", moon: true, key: 1.8, ambient: 0.6, keyColor: 0x9fb4ff, skyColor: 0xb4c0e4 },
  darkNight: { label: "Noite escura", moon: true, key: 0.9, ambient: 0.24, keyColor: 0x9fb4ff, skyColor: 0xb4c0e4 },
};

/** Default sun/ambient intensities, used whenever a mission doesn't set its own
 * `sunIntensity`/`ambientIntensity` (see types.ts) — also what the Map Editor's "Iluminação"
 * sliders default a new/untouched map to (see GameApp.tsx), so the editor's default and the
 * renderer's fallback can never drift apart. Set to match the exact values the user tuned by
 * hand on "O Vau" (saved as vau016.json). Those previous noon values stay available through
 * TIME_OF_DAY_LIGHT.noon; the everyday daytime default is intentionally softer. */
export const DEFAULT_SUN_INTENSITY = 3.5;
export const DEFAULT_AMBIENT_INTENSITY = 1.4;
/** "indoor" environment preset (see Mission.environment): a raking outdoor sun makes no sense
 * inside a building, so indoor missions get a much weaker directional light and a much stronger
 * ambient fill instead — flatter, but not fully unlit. Only applied when the mission doesn't
 * also set an explicit sunIntensity/ambientIntensity of its own. */
const INDOOR_SUN_INTENSITY = 0.35;
const INDOOR_AMBIENT_INTENSITY = 0.65;

/** Restrained default for full-scene bloom. Individual maps can still keep their authored value. */
export const DEFAULT_BLOOM_INTENSITY = 0.16;

/** An overlay fill with its alpha scaled by `fade`, quantized to 0.02 so the per-fill material
 * cache (overlayMaterialFor) only ever sees a small, bounded set of fade steps. */
function fadedFill(fill: string, fade: number): string {
  const m = /rgba?\(([^,]+),([^,]+),([^,)]+)(?:,([^)]+))?\)/.exec(fill);
  if (!m) return fill;
  const a = (m[4] !== undefined ? Number(m[4]) : 1) * fade;
  return `rgba(${m[1]},${m[2]},${m[3]},${(Math.round(a * 50) / 50).toFixed(2)})`;
}
const BLOOM_RADIUS = 0.3;
/** Full-scene bloom (see render()'s own comment) needs a threshold well above the old
 * selective-only 0.2 — that value only ever had to separate wisp embers from a pass that was
 * otherwise pure black. Against the REAL rendered scene, 0.2 would catch huge swaths of
 * ordinary lit ground art and wash the whole board out in a permanent haze. 0.75 keeps it to
 * genuine highlights: sun glints, the active-turn glow, bright embers/holy/fire FX. */
const BLOOM_THRESHOLD = 0.75;

/** Same formula as BattleEngine.effectAnchor's worldX/worldY — a hex's position independent of
 * camera pan. Duplicated (not imported) because effectAnchor is keyed to the engine's live
 * layout.tile, whereas this renderer needs it before/without going through a render call. */
function hexWorld(col: number, row: number, tile: number, squareTiles = false): { wx: number; wy: number } {
  return {
    wx: tile * SQRT3 * (col + (squareTiles ? 0 : 0.5 * (row & 1)) + 0.5),
    wy: tile * BOARD_PAD_MUL + tile * (1.5 * row + 1),
  };
}

/** Architecture uses aligned columns, allowing straight rectangular rooms over the board. */
function architectureWorld(col: number, row: number, tile: number): { wx: number; wy: number } {
  return { wx: tile * SQRT3 * (col + 0.5), wy: tile * BOARD_PAD_MUL + tile * (1.5 * row + 1) };
}

/** Pointy-top hex fan (center + 6 rim vertices + 6 triangles), radius 0.5 so scaling by
 * tile*2 matches Canvas2D's hexPath(ctx, cx, cy, tile*1.0) exactly — same vertex angles
 * (60*i-30 degrees), Y negated to compensate for local Y=+0.5 landing at the screen TOP in
 * this renderer (Canvas2D's Y-down convention has that same vertex angle read as "below
 * center"; see the module comment on the Y-flip). UVs use the same 0..1 mapping a plain
 * PlaneGeometry uses (localX+0.5, localY+0.5, on the SAME already-flipped local Y), so a
 * texture drawn as if filling the full tile*2 square shows through only inside the hex
 * outline — identical to Canvas2D's clip()-then-drawImage. */
function buildHexGeometry(): THREE.BufferGeometry {
  const positions: number[] = [0, 0, 0];
  const uvs: number[] = [0.5, 0.5];
  for (let i = 0; i < 6; i++) {
    const a = (Math.PI / 180) * (60 * i - 30);
    const x = Math.cos(a) * 0.5;
    const y = -Math.sin(a) * 0.5;
    positions.push(x, y, 0);
    uvs.push(x + 0.5, y + 0.5);
  }
  // Winding order matters: (0, i, i%6+1) comes out clockwise-from-+Z here (culled by the
  // default FrontSide material, since this camera looks down -Z from +Z) — reversed to
  // (0, i%6+1, i) so the hex actually faces the camera.
  const indices: number[] = [];
  for (let i = 1; i <= 6; i++) indices.push(0, (i % 6) + 1, i);
  const geo = new THREE.BufferGeometry();
  geo.setAttribute("position", new THREE.Float32BufferAttribute(positions, 3));
  geo.setAttribute("uv", new THREE.Float32BufferAttribute(uvs, 2));
  geo.setIndex(indices);
  // Needed for MILESTONE 2's MeshLambertMaterial (unlit MeshBasicMaterial never reads normals) —
  // every vertex lies in the same z=0 plane facing the camera, so this is just (0,0,1) everywhere.
  geo.computeVertexNormals();
  return geo;
}

/** A true hex border, kept in world space beneath props and characters. */
function buildHexBorder(inner: number): THREE.BufferGeometry {
  const vertices: number[] = [];
  const indices: number[] = [];
  for (let i = 0; i < 6; i++) {
    const a = (60 * i - 30) * Math.PI / 180;
    for (const r of [0.5, inner]) vertices.push(Math.cos(a) * r, -Math.sin(a) * r, 0);
    const j = i * 2, k = ((i + 1) % 6) * 2;
    indices.push(j, j + 1, k, k, j + 1, k + 1);
  }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute("position", new THREE.Float32BufferAttribute(vertices, 3));
  geo.setIndex(indices);
  return geo;
}

interface TileMeshEntry {
  mesh: THREE.Mesh;
  id: TerrainId;
  variant: number;
  rot: number;
}

/** Ported verbatim from BattleEngine.drawDecorations' sizing math (see that method's own
 * comments for the reasoning behind each special case) — pure numbers, no canvas calls, so
 * there was nothing renderer-specific to translate. Kept in exact sync with engine.ts by hand;
 * a mismatch here means a prop is sized differently on the two renderers, not a crash, so it
 * won't show up as a type error — check against drawDecorations if a prop looks off. */
function decorSize(id: string, def: DecorationDef, tile: number): { w: number; h: number; dy: number } {
  if (id === "stone-stairs-up-001" || id === "stone-stairs-down-001") return { w: tile * SQRT3, h: tile * 3.5, dy: 0 };
  if (def.propModel?.startsWith("watchtower-")) return { w: tile * ((def.propModel === "watchtower-command-table-v01" || def.propModel === "watchtower-command-table-v02") ? 1.9 : 1.45), h: tile * ((def.propModel === "watchtower-signal-brazier-v01" || def.propModel === "watchtower-signal-brazier-v02") ? 1.65 : (def.propModel === "watchtower-arms-rack-v01" || def.propModel === "watchtower-arms-rack-v02") ? 2.0 : 1.1), dy: 0 };
  if (def.propModel && !def.propModel.startsWith("tavern-")) return decorSize(def.propModel === "grey-outcrop" ? "rocky-outcrop" : def.propModel, DECORATIONS[def.propModel === "grey-outcrop" ? "rocky-outcrop" : def.propModel]!, tile);
  if (def.treeModel) return { w: tile * 3.2, h: tile * 4.8, dy: 0 };
  let minDx = 0;
  let maxDx = 0;
  let minDy = 0;
  let maxDy = 0;
  for (const { dx, dy } of def.footprint) {
    minDx = Math.min(minDx, dx);
    maxDx = Math.max(maxDx, dx);
    minDy = Math.min(minDy, dy);
    maxDy = Math.max(maxDy, dy);
  }
  const one = def.footprint.length === 1;
  const item = CHEST_DECOR_IDS.has(id);
  const tree = id === "dead-tree";
  const log = id === "fallen-log";
  const wall = id === "barricade";
  const waypoint = !!def.exitKind;
  const anyHouse = HOUSE_DECOR_IDS.has(id) || BIG_HOUSE_DECOR_IDS.has(id);
  const w = tree
    ? tile * 1.28
    : log
      ? tile * SQRT3 * 2.05
      : wall
        ? tile * 1.42
        : anyHouse
          ? tile * 1.45 * 3
          : waypoint
            ? tile * SQRT3 * (one ? 1 : 2)
            : item
              ? tile * 0.92
              : one
                ? tile * 1.55
                : tile * SQRT3 * (maxDx - minDx + 1.7);
  const baseH = tree
    ? tile * 2.55
    : log
      ? tile * 0.82
      : wall
        ? tile * 1.18
        : anyHouse
          ? tile * 1.58 * 3
          : waypoint
            ? tile * 2
            : item
              ? tile * 0.72
              : one
                ? tile * 1.65
                : tile * (1.5 * (maxDy - minDy) + 2.3);
  const h = def.artAspect ? w / def.artAspect : baseH * (def.heightScale ?? 1);
  // Chests use the hex's ground anchor directly; a per-item nudge displaced them off-center.
  const dy = waypoint ? 0 : (tree ? -tile * 0.55 : wall ? -tile * 0.12 : anyHouse ? -tile * 0.28 * 3 : 0) - (h - baseH) * 0.42;
  // Global art scale (see DECOR_ART_SCALE), grown from the bottom edge so the base stays put.
  const s = waypoint ? 1 : (anyHouse ? HOUSE_ART_SCALE : DECOR_ART_SCALE) * (def.artScale ?? 1);
  return { w: w * s, h: h * s, dy: dy - (h * (s - 1)) / 2 };
}

/** PCF filter radius (shadow-map texels) — 1 is Three's default hard-ish edge; the Dev Controls
 * "soft shadows" toggle raises it. This Three.js version's PCF path samples a 5-tap Vogel disk
 * scaled by this radius (see shadowmap_pars_fragment), so it softens without costing more taps. */
const SHADOW_RADIUS_HARD = 1;
const SHADOW_RADIUS_SOFT = 4;

/** Contact shadow = short-range grounding only, never a second cast shadow. The scene is flat
 * orthographic billboards (no depth relationship between sprite and ground to sample), so this
 * is a per-object ground decal sized from the art's own opaque base (see artBase) — transparent
 * sprite pixels never count as contact, and one object's decal can never darken another object
 * (decals sit just above the floor; true 3D surfaces depth-occlude them).
 * W: decal width as a multiple of the measured opaque base width (a little spill past the edge).
 * H: decal height as a fraction of its width (ground seen at the board's 3/4 angle).
 * OPACITY: peak darkening at the contact point — a multiply, so 0.75 keeps 25% of the ground's
 * own light; never black. MAX_W caps the decal against the sprite's drawn width. */
const CONTACT_SHADOW_W = 1.4;
const CONTACT_SHADOW_H = 0.5;
const CONTACT_SHADOW_OPACITY = 0.75;
const CONTACT_SHADOW_MAX_W = 0.6;
/** Ground decals sit just above the floor so any real 3D wall or prop depth-occludes them. */
const CONTACT_SHADOW_Z = 0.15;
/** Absolute cap on decal height, in hex radii — keeps a wide base (wall, log) from growing a
 * deep oval that reaches far in front of/behind the contact line. */
const CONTACT_SHADOW_MAX_H = 0.5;
/** Shifts the decal toward the viewer by this fraction of its height, so more of it lies on the
 * ground visible in front of the base instead of under the sprite. The first pass (0.2 tile cap,
 * 0.4 peak, no shift) measured as a ~2px line at normal battle zoom — invisible in real play. */
const CONTACT_SHADOW_FORWARD = 0.2;

/** Real THREE.PointLights for map light sources (see syncLights). The minimum pool size — the
 * pool grows at load to cover every light the map carries, then never changes (Three compiles
 * the light count into every lit shader); unused ones sit at intensity 0. No castShadow yet —
 * shadows are a separate, later decision. */
const POINT_LIGHT_POOL = 8;
/** Bounce-fill lights (see BOUNCE_FRACTION): a fixed pool given to the map lights nearest the
 * view, so the per-fragment light count stays bounded however many lamps a map carries. */
const BOUNCE_LIGHT_POOL = 8;
/** Lights shared by per-cast effects — see ThreeBattleRenderer.vfxLights. */
const VFX_LIGHT_POOL = 4;
/** Spare room in the fixed point-light budget (see balanceLightCount) for spell lights. */
const LIGHT_BUDGET_HEADROOM = 6;
const SHADOW_LIGHT_BUDGET_HEADROOM = 3;
const HEALING_SPELL_GLOW_KINDS = new Set(["holyMinor", "healingHands", "holyMedium", "disease", "food"]);
/** Rim-glow canvas size relative to the sprite (room for the blur to spread). */
const GLOW_PAD = 1.7;
/** How much brighter than its own art a sprite (character or decoration, light props included)
 * may get from map lights. Without a cap anything beside a candle/fire — the candle itself
 * too — blew out to white and read as translucent. */
const SPRITE_LIGHT_CAP = 1.35;
/** Shared cap for every decoration material; units each carry their own (hit flash lifts it). */
const DECOR_LIGHT_CAP = { value: SPRITE_LIGHT_CAP };
/** Light cap for SELF_LIT_UNITS: never brighter than their own art. */
const SELF_LIT_UNIT_CAP = 1;
/** Light-emitting creatures, drawn under SELF_LIT_UNIT_CAP instead of the sprite cap: their own
 * carried light (UNIT_LIGHT_DEFS) dulled/washed out their colors instead of enhancing them, and
 * a creature that emits light shouldn't be lit by it. Rule, per request: every familiar. */
const SELF_LIT_UNITS = new Set<string>(["familiar", "familiar2", "familiar3", "familiar4"]);

/** 2.5D perspective between characters and props: whoever stands on the nearer row (lower on
 * screen) covers whoever stands further back, whatever layer either is drawn in. Each sprite
 * sits at a depth from its ground line, and an invisible alpha-tested copy of its art (an
 * "occluder", opaque pass, so it lands before every sprite's color) writes that depth; a sprite
 * further back then fails the depth test wherever something nearer stands over it. */
const DEPTH_Z_BASE = 1;
/** Depth per tile-radius of ground Y — one hex row (1.5 radii) is 0.006, far above precision. */
const DEPTH_Z_PER_TILE = 0.004;
/** On the same row, the character stands in front of the prop. */
const UNIT_DEPTH_TIE = 0.001;
/** Explicit unitLayer "behind" props and flat Waypoints: always under every character. */
const DEPTH_Z_BEHIND = 0.95;
/** Explicit unitLayer "front"/foreground props: always over every character. */
const DEPTH_Z_FRONT = 1.9;
/** Only the solid body of an art occludes — soft glows and fringes baked into it do not. */
const OCCLUDER_ALPHA = 0.7;
function spriteDepthZ(groundWy: number, tile: number): number {
  return DEPTH_Z_BASE + (groundWy / tile) * DEPTH_Z_PER_TILE;
}
function occluderMaterial(map: THREE.Texture | null): THREE.MeshBasicMaterial {
  return new THREE.MeshBasicMaterial({ map, alphaTest: OCCLUDER_ALPHA, colorWrite: false, depthWrite: true });
}

/** Sprites are flat cards facing the camera, so a lamp on either side lit them the same.
 * For map point lights only, each sprite is shaded as if it were rounded side to side: its
 * normal turns toward screen left/right across its width, by up to this much at the edges
 * (0 = flat card), so the side facing the lamp is brighter than the side away from it. The sun,
 * moon and sky keep the flat normal, so daytime/ambient sprite exposure is unchanged. */
const SPRITE_NORMAL_BEND = 0.5;
/** Three's lights_fragment_begin with only the point-light loop using the sprite's bent normal. */
const SPRITE_LIGHTS_FRAGMENT = (() => {
  const chunk = THREE.ShaderChunk.lights_fragment_begin;
  const start = chunk.indexOf("#if ( NUM_POINT_LIGHTS > 0 ) && defined( RE_Direct )");
  const end = start < 0 ? -1 : chunk.indexOf("#pragma unroll_loop_end", start);
  const call = "RE_Direct( directLight, geometryPosition, geometryNormal,";
  if (end < 0 || !chunk.slice(start, end).includes(call)) return chunk;
  return chunk.slice(0, start) + chunk.slice(start, end).replace(call, "RE_Direct( directLight, geometryPosition, spriteBentNormal,") + chunk.slice(end);
})();

/** Clamps a lit sprite's final color to `cap` times its own art — see SPRITE_LIGHT_CAP — and
 * bends its normal for point lights (see SPRITE_NORMAL_BEND). */
function capSpriteLight(material: THREE.MeshLambertMaterial, cap: { value: number }): void {
  material.onBeforeCompile = (shader) => {
    shader.uniforms.spriteLightCap = cap;
    // Which side of the card this fragment is on, in world terms (a mirrored sprite has a
    // negative x scale, so its uv.x runs right-to-left on screen).
    shader.vertexShader = shader.vertexShader
      .replace("#include <common>", "#include <common>\nvarying float vSpriteSide;")
      .replace("#include <uv_vertex>", "#include <uv_vertex>\nvSpriteSide = ( uv.x - 0.5 ) * ( modelMatrix[ 0 ][ 0 ] < 0.0 ? -1.0 : 1.0 );");
    shader.fragmentShader = shader.fragmentShader
      .replace("#include <common>", "#include <common>\nuniform float spriteLightCap;\nvarying float vSpriteSide;")
      .replace(
        "#include <lights_fragment_begin>",
        `vec3 spriteBentNormal = normalize( vec3( vSpriteSide * 2.0 * ${SPRITE_NORMAL_BEND.toFixed(3)}, 0.0, 1.0 ) );\n${SPRITE_LIGHTS_FRAGMENT}`,
      )
      .replace("#include <envmap_fragment>", "outgoingLight = min(outgoingLight, sampledDiffuseColor.rgb * spriteLightCap);\n#include <envmap_fragment>");
  };
  material.customProgramCacheKey = () => "spriteLightCap";
}
/** How many of the pool (always the lights nearest the view) cast real cube-map shadows: the
 * hidden proxy volumes (PROXY_LAYER) of props and characters block the lamp's light, so a
 * tombstone or a character throws its shadow away from the flame. Each one re-renders the
 * proxies six times per frame, so only the nearest few; the rest stay unshadowed. */
const POINT_SHADOW_LIGHTS = 2;
/** Point-shadow camera near plane, in hex radii. At the old fixed 2 world px the cube shadow
 * map drew a stray dark line straight through every shadowed light's foot (measured on a lone
 * brazier with nothing to cast); 6–12 px (~0.2–0.35 hex) removes it. Scaled by zoom. */
const POINT_SHADOW_NEAR = 0.2;
/** Render layer of the hidden 3D proxy volumes (a box per prop, an upright cylinder per
 * character): the physical shapes map lights hit and are blocked by. The main camera and the
 * sun's shadow camera never see this layer — the art stays what the player sees and the sun
 * keeps its silhouette shadows; only the point lights' shadow cameras render it. */
const PROXY_LAYER = 3;
/** The ground's normal sun + sky irradiance in this renderer (sun 5 x N.L 0.78 + hemi ~1):
 * a point light adding this much irradiance doubles the ground's brightness — the same scale
 * LightDef.intensity uses for sprites (1 = twice as bright). */
const GROUND_BASE_IRRADIANCE = 4.9;
/** Fraction of an image's height, measured up from its lowest opaque row, that counts as "the
 * base touching the ground" (feet, paws, trunk, wall foot). */

/** Flame halo: a soft additive glow in the air around every light prop's flame, standing in
 * for the haze that makes a real lamp read as light spreading. Radius is this fraction of the
 * light's reach (LightDef.radius, hex radii); HALO_STRENGTH is its peak additive opacity at
 * night, scaled down by HALO_DAYLIGHT in daylight and HALO_TWILIGHT at dawn/dusk. */
const HALO_RADIUS_FRACTION = 0.45;
const HALO_STRENGTH = 0.55;
const HALO_DAYLIGHT = 0.35;
const HALO_TWILIGHT = 0.7;

/** White radial glow, alpha (1 - r²)³: a soft core that fades to exactly zero at the rim. */
function makeFlameHaloTexture(): THREE.CanvasTexture {
  const size = 128;
  const c = document.createElement("canvas");
  c.width = c.height = size;
  const ctx = c.getContext("2d")!;
  const img = ctx.createImageData(size, size);
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const dx = (x + 0.5) / (size / 2) - 1;
      const dy = (y + 0.5) / (size / 2) - 1;
      const k = Math.max(0, 1 - (dx * dx + dy * dy));
      const i = (y * size + x) * 4;
      img.data[i] = img.data[i + 1] = img.data[i + 2] = 255;
      img.data[i + 3] = Math.round(255 * k * k * k);
    }
  }
  ctx.putImageData(img, 0, 0);
  return new THREE.CanvasTexture(c);
}

/** Falloff mask shared by every contact decal. Alpha only — the material (see
 * makeContactShadowMaterial) multiplies the ground by (1 - alpha), so the color channels are
 * irrelevant. (1 - r²)²: strongest at the contact, ~56% at half radius, ~26% at 70%, and exactly
 * zero with zero slope at the edge, so no ring marks where it stops. */
function makeContactShadowTexture(): THREE.CanvasTexture {
  const size = 128;
  const c = document.createElement("canvas");
  c.width = c.height = size;
  const ctx = c.getContext("2d")!;
  const img = ctx.createImageData(size, size);
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const dx = (x + 0.5) / (size / 2) - 1;
      const dy = (y + 0.5) / (size / 2) - 1;
      const k = Math.max(0, 1 - (dx * dx + dy * dy));
      const i = (y * size + x) * 4;
      img.data[i] = img.data[i + 1] = img.data[i + 2] = 255;
      img.data[i + 3] = Math.round(255 * k * k);
    }
  }
  ctx.putImageData(img, 0, 0);
  return new THREE.CanvasTexture(c);
}

/** dst * (1 - srcAlpha): can only darken what is already on the ground, never lighten or tint it.
 * The previous alpha-blended dark-grey gradient measured as LIGHTENING dark grass by up to +45
 * luminance (a grey film, not a shadow) — its "dark" color landed mid-grey after output encoding. */
function makeContactShadowMaterial(map: THREE.Texture): THREE.MeshLambertMaterial {
  const material = new THREE.MeshLambertMaterial({
    map,
    opacity: CONTACT_SHADOW_OPACITY,
    transparent: true,
    side: THREE.DoubleSide,
    depthWrite: false,
    polygonOffset: true,
    polygonOffsetFactor: -1,
    polygonOffsetUnits: -4,
    blending: THREE.CustomBlending,
    blendEquation: THREE.AddEquation,
    blendSrc: THREE.ZeroFactor,
    blendDst: THREE.OneMinusSrcAlphaFactor,
  });
  material.onBeforeCompile = (shader) => {
    const mask = THREE.ShaderChunk.shadowmask_pars_fragment
      .replace("receiveShadow ? getShadow( directionalShadowMap", "receiveShadow && dot(directionalLights[ i ].color, vec3(1.0)) > 0.001 ? getShadow( directionalShadowMap")
      .replace("receiveShadow ? getPointShadow( pointShadowMap", "receiveShadow && dot(pointLights[ i ].color, vec3(1.0)) > 0.001 ? getPointShadow( pointShadowMap")
      .replace("directionalLight.shadowBias,", "max(-0.00005, directionalLight.shadowBias * 0.03),");
    shader.fragmentShader = shader.fragmentShader.replace(
      "#include <shadowmap_pars_fragment>",
      `#include <shadowmap_pars_fragment>\n${mask}`,
    ).replace("#include <opaque_fragment>", "diffuseColor.a *= getShadowMask();\n#include <opaque_fragment>");
  };
  material.customProgramCacheKey = () => "contact-gap-only-v1";
  return material;
}

/** Where an image's opaque art actually meets the ground, normalized to the image (u across,
 * v down): the alpha-weighted 5th–95th percentile span of opaque pixels within CONTACT_BASE_BAND
 * of the lowest opaque row. Percentiles (not min/max) so a stray sword tip or claw doesn't
 * stretch the decal. null = no opaque pixels / unreadable image. Cached per image. */
interface ArtBase {
  u0: number;
  u1: number;
  v: number;
}
const artBaseCache = new WeakMap<HTMLImageElement, ArtBase | null>();
function artBase(img: HTMLImageElement): ArtBase | null {
  if (artBaseCache.has(img)) return artBaseCache.get(img)!;
  const result = decorationAnchor(img);
  artBaseCache.set(img, result);
  return result;
}

/** Where a light prop's flame (or lit lantern glass) sits in its own art, normalized (u across,
 * v down): the centroid of its bright fire-coloured opaque pixels, weighted by brightness. The
 * light is placed there, not at the prop's ground pivot. Falls back to the top third's center.
 * Cached per image. */
const flameCache = new WeakMap<HTMLImageElement, { u: number; v: number }>();
function artFlame(img: HTMLImageElement): { u: number; v: number } {
  const hit = flameCache.get(img);
  if (hit) return hit;
  let result = { u: 0.5, v: 0.3 };
  try {
    const scale = Math.min(1, 192 / Math.max(img.naturalWidth, img.naturalHeight));
    const w = Math.max(1, Math.round(img.naturalWidth * scale));
    const h = Math.max(1, Math.round(img.naturalHeight * scale));
    const c = document.createElement("canvas");
    c.width = w;
    c.height = h;
    const ctx = c.getContext("2d", { willReadFrequently: true })!;
    ctx.drawImage(img, 0, 0, w, h);
    const d = ctx.getImageData(0, 0, w, h).data;
    let sw = 0;
    let su = 0;
    let sv = 0;
    for (let y = 0; y < h; y++) {
      for (let x = 0; x < w; x++) {
        const i = (y * w + x) * 4;
        const r = d[i]!;
        const g = d[i + 1]!;
        const b = d[i + 2]!;
        if (d[i + 3]! < 128 || r < 190 || g < 90 || r < g || g < b + 20) continue;
        const wt = (r + g - b) / 255;
        sw += wt;
        su += wt * (x + 0.5);
        sv += wt * (y + 0.5);
      }
    }
    if (sw > 4) result = { u: su / sw / w, v: sv / sw / h };
  } catch {
    // unreadable image: keep the fallback
  }
  flameCache.set(img, result);
  return result;
}

interface DecorMeshEntry {
  mesh: THREE.Mesh;
  placement: DecorationPlacement;
  /** Same quadGeo + alpha-tested copy of the prop's own art (colorWrite off, see
   * decorShadowMaterialFor) that casts this prop's real shadow as its own silhouette, not a box.
   * Null for a DecorationDef.noShadow prop, which skips this entirely. */
  shadowMesh: THREE.Mesh | null;
  /** Contact decal at the prop's opaque base (see artBase); null when the art has no readable
   * base or the prop is a spun placeholder (facing fallback) with no meaningful "bottom". */
  contactMesh: THREE.Mesh | null;
  /** Hidden 3D volume (PROXY_LAYER) blocking point lights; null for light-source props. */
  proxy: THREE.Mesh | null;
  /** Environmental light this prop emits (LIGHT_DEFS), at its flame; world pixels, y-down. */
  light: { x: number; y: number; h: number; def: LightDef; seed: number } | null;
  /** Additive glow around the flame (see HALO_STRENGTH); null for props that emit no light. */
  halo: THREE.Mesh | null;
  /** Houses only: invisible depth-only copy of the art, drawn just before the fog-of-war sheet
   * so the fog skips the house's pixels — a house always shows at full strength. */
  fogCut: THREE.Mesh | null;
}

interface UnitMeshEntry {
  mesh: THREE.Mesh;
  /** Level-up / heal rim glow: a blurred white silhouette of the current frame behind the
   * sprite, tinted and faded like the Canvas2D shadowBlur pass (engine renderUnitsAndOverlays). */
  glowMesh: THREE.Mesh;
  glowMaterial: THREE.MeshBasicMaterial;
  /** Owned (not shared) per unit — see unitTexCache's comment on why opacity needs this. */
  material: THREE.MeshLambertMaterial;
  /** See SPRITE_LIGHT_CAP — per unit, so a hit flash can lift its own cap. */
  lightCap: { value: number };
  /** Depth-only cutout of the current frame, a child of `mesh` — see DEPTH_Z_BASE. */
  occluder: THREE.Mesh;
  img: HTMLImageElement | null;
  /** Same quadGeo + alpha-tested copy of the unit's own sprite (colorWrite off) that casts this
   * unit's real shadow as its own silhouette, not a box — see shadowMaterial's own comment. */
  shadowMesh: THREE.Mesh;
  /** Owned per unit, same reasoning as `material` — swapped in step with entry.img/material.map
   * whenever the unit's current sprite frame changes, so the shadow always matches the current
   * pose instead of freezing on whatever frame first built this entry. */
  shadowMaterial: THREE.MeshBasicMaterial;
  /** Dev Controls "contact shadows" footprint — owned per unit (its opacity tracks this unit's
   * own fade/lift); the gradient texture itself is shared (contactShadowTexture). */
  contactMesh: THREE.Mesh;
  contactMaterial: THREE.MeshLambertMaterial;
  /** Smoothed decal center/width in world units — the base is re-measured from each animation
   * frame, so this eases between frames instead of snapping (null until first placed). */
  contactFit: { dx: number; dy: number; w: number } | null;
  /** Hidden upright cylinder (PROXY_LAYER) standing at the character's feet. */
  proxy: THREE.Mesh;
  /** Same trick as a house's DecorMeshEntry.fogCut — an invisible depth-only copy of the unit's
   * own sprite, drawn just under the fog-of-war sheet, so the fog never blacks out a unit that
   * is actually standing there (a unit at the edge of sight was getting half-erased by the
   * unseen hex right next to it). */
  fogCut: THREE.Mesh;
}

export class ThreeBattleRenderer {
  private boardWorld(col: number, row: number, tile: number) {
    return hexWorld(col, row, tile, !!this.engine.mission.squareTiles);
  }

  private renderer: THREE.WebGLRenderer;
  private scene = new THREE.Scene();
  private readonly spellVfxScene = new THREE.Group();
  private readonly blizzardLightPulses: { light: THREE.PointLight; age:number }[] = [];
  private fireballVfx: FireballVFX | null = null;
  private causticVenomVfx: CausticVenomVFX | null = null;
  private phantasmalForceVfx: PhantasmalForceVFX | null = null;
  private blessVfx: BlessVFX | null = null;
  private magicMissileV2Vfx: MagicMissileV2VFX | null = null;
  private magicMissileForeground: MagicMissileForeground | null = null;
  private magicMissileForegroundCanvas: HTMLCanvasElement | null = null;
  private webOfDreamsVfx: WebOfDreamsVFX | null = null;
  private readonly burningHandsVfx: BurningHandsV2VFX[] = [];
  private readonly pixelElementEmitters: { placement: ElementalFxPlacement; emitter: ProceduralElementEmitter }[] = [];
  private pixelFxClock = 0;
  private readonly varreduraVfx: VarreduraVFX[] = [];
  private readonly cleaveVfx: CleaveSweepVFX[] = [];
  /** Keep one complete hero-missile effect per queued Magic Missile target. */
  private readonly pendingMagicMissileV2VfxRequests: MagicMissileV2VfxRequest[] = [];
  private activeMagicMissileV2VfxRequestId: string | null = null;
  /** The former tavern preview model, now reserved for the actual cast trajectory. */
  private readonly tavernFireball = new OldFireBall(LIGHT_DECAY);
  private disposed = false;
  private camera: THREE.OrthographicCamera;
  private cameraRight = new THREE.Vector3(1, 0, 0);
  private cameraUp = new THREE.Vector3(0, 1, 0);
  private cameraGroundDown = new THREE.Vector3(0, -1, 0);
  private cameraBack = new THREE.Vector3(0, 0, 1);
  private cameraSpriteScale = 1;
  private unitUp = new THREE.Vector3(0, 1, 0);
  private unitFacing = new THREE.Quaternion();
  private shadowFootOffset = new THREE.Vector3();
  private terrainSolid: THREE.Mesh | null = null;
  private terrainSolidKey = "";
  private water = new ThreeWater();
  private waterKey = "";
  private groundDepthLayerEnabled = { value: 1 };
  private elevationSteps = new ThreeElevationSteps(this.groundDepthLayerEnabled);
  private elevationStepsKey = "";
  private landscape: LandscapeSurface | null = null;
  private landscapeTexture: THREE.CanvasTexture | null = null;
  private landscapeMaterial: THREE.MeshLambertMaterial | null = null;
  private terrainRevision = 0;
  private cliffMaterial: THREE.MeshStandardMaterial | null = null;
  private tileGroup = new THREE.Group();
  private tileMeshes = new Map<number, TileMeshEntry>();
  // A camera-locked, cover-cropped copy of BattleEngine.renderGround's painted backdrop.
  // The legacy 2D path had this from day one; keeping it here prevents WebGL missions from
  // silently dropping any mission-specific background artwork.
  private backdropGeometry = new THREE.PlaneGeometry(1, 1);
  private backdropMaterial = new THREE.MeshBasicMaterial({ color: 0x949494, depthWrite: false, depthTest: false });
  private backdropMesh = new THREE.Mesh(this.backdropGeometry, this.backdropMaterial);
  private backdropTexture: THREE.Texture | null = null;
  private backdropImage: HTMLImageElement | null = null;
  // MeshLambertMaterial (not MeshBasicMaterial) — MILESTONE 2 terrain needs to actually receive
  // light/shadow. Decor and unit sprites deliberately stay MeshBasicMaterial (unlit) below, so
  // their art is untouched by this — only the ground gets the uniform lit tint (see the
  // architecture note in THREEJS_MILESTONE2_HANDOFF.md on why a flat scene can only tint, not
  // per-object shade).
  private materialCache = new Map<string, THREE.MeshLambertMaterial>();
  private fallbackMaterial = new THREE.MeshLambertMaterial({ color: 0x1e1b18 });
  /** Environmental AO in the terrain's lighting (see ThreeGroundAO.ts) — every terrain
   * material is patched to read it; aoTerrainVersion bumps whenever syncDirtyTiles swaps a
   * tile's terrain, so the field rebuilds only when the board actually reshapes. */
  private groundAO = new GroundAO();
  private aoTerrainVersion = 0;
  /** Fog-of-war overlay — one world-aligned quad (see ThreeFogMask.ts / syncFog). */
  private fogMask = new FogMask();
  /** Real point lights for map light sources (see POINT_LIGHT_POOL / syncLights). */
  private pointLights: THREE.PointLight[] = [];
  /** Wide, dim bounce fill for the nearest map lights (see BOUNCE_LIGHT_POOL / syncLights). */
  private bounceLights: THREE.PointLight[] = [];
  /** Lights per-cast effects (Cleave, Sweep) borrow, so the scene's light count never changes
   * mid-battle — a change there recompiles every lit material on screen (the spell stall). */
  private readonly vfxLightPool: THREE.PointLight[] = [];
  private readonly vfxLights: VfxLightPool = {
    take: (color, distance, decay) => {
      const light = this.vfxLightPool.find((l) => l.userData.vfxFree) ?? new THREE.PointLight();
      light.userData.vfxFree = false;
      light.color.setHex(color);
      light.distance = distance;
      light.decay = decay;
      light.intensity = 0;
      return light;
    },
    give: (light) => {
      light.intensity = 0;
      if (this.vfxLightPool.includes(light)) light.userData.vfxFree = true;
    },
  };
  /** One real spell light follows the active holy-heal target; the approved holy-light art
   * remains on the units canvas unchanged. */
  private healingSpellLight = new THREE.PointLight(0xfff4d2, 0, 1, LIGHT_DECAY);
  /** Shared proxy geometry: unit box and a Z-up unit cylinder, scaled per object. */
  private proxyBox = new THREE.BoxGeometry(1, 1, 1);
  private proxyCylinder = new THREE.CylinderGeometry(0.5, 0.5, 1, 16).rotateX(Math.PI / 2);
  private proxyMaterial = new THREE.MeshBasicMaterial({ colorWrite: false, depthWrite: false });
  /** Every lit sprite material (decorations + unit billboards) — see syncSpriteExposure. */
  private litSpriteMats = new Set<THREE.MeshLambertMaterial>();
  /** Unit billboards need exposure compensation for their changing camera-facing normal. */
  private unitSpriteMats = new Set<THREE.MeshLambertMaterial>();
  /** Local lamp light gets a much higher sprite ceiling after daylight fades, so it can light
   * nearby scenery and units instead of brightening only the ground beneath the source. */
  private currentSpriteLightCap = SPRITE_LIGHT_CAP;
  private hexGeo = buildHexGeometry();
  private focusBorderGeo = buildHexBorder(0.47);
  private builtCols = -1;
  private builtRows = -1;
  private builtMissionId = "";
  private builtTile = -1;

  // MILESTONE 2 — lighting/shadows. hemiLight is a soft sky/ground fill so unlit-facing surfaces
  // don't go fully black (a single DirectionalLight alone would do that — see handoff doc);
  // sunLight is the one real shadow-casting light, aimed by updateSun() every frame to track the
  // camera (see SUN_DIRECTION's comment for why its direction is fixed). Shadow casters (units,
  // decorations) live on shadowCasterGroup — visible=true (required: WebGLShadowMap skips
  // object.visible===false entirely, so this can't be used to hide them). Each caster is now the
  // unit/prop's own alpha-tested art (colorWrite off — see decorShadowMaterialFor's/the per-unit
  // shadowMaterial's own comments for how invisibility in the normal pass is achieved) rather than
  // an invisible box, so the cast shadow matches the real silhouette instead of a rectangle.
  // Intensity args here are placeholders — the constructor immediately overrides both from
  // Mission.sunIntensity/ambientIntensity (or DEFAULT_SUN_INTENSITY/DEFAULT_AMBIENT_INTENSITY),
  // see that assignment's own comment.
  private hemiLight = new THREE.HemisphereLight(0xfff2df, 0x14110d, 0.45);
  private sunLight = new THREE.DirectionalLight(0xfff0d6, 1.8);
  /** The Moon: a second real DirectionalLight with its own shadow map (Dev Controls "Noite"). */
  private moonLight = new THREE.DirectionalLight(0x9fb4ff, 0);
  /** Current travel directions of sunlight/moonlight (see skyDirection). */
  private sunDir = SUN_DIRECTION.clone();
  private moonDir = SUN_DIRECTION.clone();
  /** The mission's own daytime sun/sky intensities — the reference the sprite exposure is
   * calibrated against, so night/sun-angle changes reach the sprites physically. */
  private baseSunIntensity = DEFAULT_SUN_INTENSITY;
  private baseHemiIntensity = DEFAULT_AMBIENT_INTENSITY;
  /** Mission.timeOfDay ("day" when unset). */
  private timeOfDay: MapTimeOfDay = "day";
  /** Daytime sun/sky the sprite exposure is calibrated against (see syncSpriteExposure) — the
   * mission's own values by day, the standard daytime defaults at any other time, so dawn/dusk/
   * night darken and tint the sprites like everything else instead of being compensated away. */
  private calibSunIntensity = DEFAULT_SUN_INTENSITY;
  private calibHemiIntensity = DEFAULT_AMBIENT_INTENSITY;
  private calibSunColor = new THREE.Color(TIME_OF_DAY_LIGHT.day.keyColor);
  private calibHemiColor = new THREE.Color(TIME_OF_DAY_LIGHT.day.skyColor);
  private shadowCasterGroup = new THREE.Group();
  private lastShadowFrustumW = -1;
  private lastShadowFrustumH = -1;

  // Ground/behind-layer decorations only (trees, houses, rubble, ...) — see ensureDecorBuilt's
  // comment for why "front"-layer/foreground props stay on the existing Canvas2D-shim units
  // canvas instead of moving here.
  private quadGeo = new THREE.PlaneGeometry(1, 1);
  private decorGroup = new THREE.Group();
  private decorMatCache = new Map<string, THREE.MeshLambertMaterial>();
  // Shadow-only twin of decorMatCache — same cached texture, but alphaTest instead of plain alpha
  // blending (shadow depth passes need a hard cutout, not a blend) and colorWrite/depthWrite off
  // (see decorShadowMaterialFor's own comment), so it can't just reuse the visible material.
  private decorShadowMatCache = new Map<string, THREE.MeshBasicMaterial>();
  private decorFogCutMatCache = new Map<string, THREE.MeshBasicMaterial>();
  private trees = new ThreeTrees();
  private decorEntries: DecorMeshEntry[] = [];
  private wallEntries: { mesh: THREE.Mesh<THREE.BufferGeometry, THREE.MeshStandardMaterial>; shadowMesh: THREE.Mesh<THREE.BufferGeometry, THREE.MeshBasicMaterial>; placement: DecorationPlacement }[] = [];
  private wallGroup = new THREE.Group();
  private wallTextures = new Map<string, THREE.Texture>();
  private builtDecorKey = "";

  // Movement/attack/spell-range highlight + the active-turn ring (see
  // BattleEngine.boardOverlayLayers/activeTurnHighlight) — real world-space hex meshes at
  // z=0.5, between flat terrain (z=0, opaque, drawn first) and decorations (z=1, transparent).
  // Three draws transparent objects back-to-front by camera distance regardless of draw order,
  // so this lands the highlight visually ABOVE terrain but BELOW decorations and units (z=2+)
  // for free, the same depth trick tiles/decor/units already rely on — a blocking house or a
  // unit standing on a highlighted hex always stays legible instead of the highlight's tint
  // painting over it. The flat fill and its soft radial halo use separate pooled meshes in
  // syncOverlay, with the halo just behind the cell fill.
  private overlayGroup = new THREE.Group();
  /** Soft red glow beneath enemy target cells. */
  private gridGlowGroup = new THREE.Group();
  private gridGlowMeshes: THREE.Mesh[] = [];
  private gridGlowMatCache = new Map<string, THREE.MeshBasicMaterial>();
  /** Summoning portals (BattleEngine.drawPortalFxLayer), painted each frame they're open onto
   * a portal-sized canvas (see portalFxBounds) and shown as a ground layer at z=0.55 — above the board overlay,
   * below decorations and units — so a familiar stepping out stands in front of its portal. */
  private portalCanvas: HTMLCanvasElement | null = null;
  private portalTexture: THREE.CanvasTexture | null = null;
  private portalMesh: THREE.Mesh | null = null;
  private portalBasicMaterial: THREE.MeshBasicMaterial | null = null;
  private portalWarpMaterial: THREE.ShaderMaterial | null = null;
  /** Dreaming Web's floor patch (engine.webZones) — the webfloor photo on each covered hex,
   * same as Canvas2D renderGround draws it (which this renderer replaces). Pooled meshes. */
  private webGroup = new THREE.Group();
  private webMeshes: THREE.Mesh[] = [];
  private webMat: THREE.MeshLambertMaterial | null = null;
  private webMatDim: THREE.MeshLambertMaterial | null = null;
  private overlayMatCache = new Map<string, THREE.MeshBasicMaterial>();
  private overlayMeshPool: THREE.Mesh[] = [];
  // Animated units (see THREEJS_MILESTONE1_HANDOFF.md) — one persistent mesh per live unit id,
  // repositioned/retextured/rescaled every frame in syncUnits rather than rebuilt, since units
  // (unlike terrain/decor) change position, pose and art every frame. HP bars, hover/selection
  // highlight, and portal FX stay on the old Canvas2D-shim units canvas on purpose (see
  // BattleEngine.renderUnitsAndOverlays' skipUnitSprites param) — only the character sprite art
  // itself moves here.
  private unitGroup = new THREE.Group();
  /** Flame halos (see HALO_STRENGTH), in the air behind the props and units around them. */
  private flameHaloGroup = new THREE.Group();
  private flameHaloTexture = makeFlameHaloTexture();
  /** Contact-shadow footprints sit just above the ground; wall depth can occlude them.
   * Deliberately NOT tied to unitGroup's visibility: BattleCanvas hides the Three unit sprites
   * (units draw on the Canvas2D top layer), but these are ground marks, so they stay here. */
  private contactShadowGroup = new THREE.Group();
  private contactShadowTexture = makeContactShadowTexture();
  /** Decoration contact decals — separate from contactShadowGroup because, unlike unit decals,
   * these must hide whenever decorGroup does (no decal left under a prop that isn't drawn). One
   * shared material: props don't fade individually (fog-of-war toggles mesh.visible instead). */
  private decorContactGroup = new THREE.Group();
  private decorContactMaterial = makeContactShadowMaterial(this.contactShadowTexture);
  // Textures are shared by image (same pattern as tiles/decor — cheap, no per-unit GPU upload),
  // but each unit gets its OWN material (see UnitMeshEntry) so u.fade can drive real per-unit
  // opacity: a shared material (the tile/decor pattern) would make every unit sharing one sprite
  // frame fade in/out together, which is wrong the instant two of them are mid-death at once.
  private unitTexCache = new Map<HTMLImageElement, THREE.Texture>();
  /** Blurred white silhouettes for the rim glow, built on first use per frame image. */
  private glowTexCache = new Map<HTMLImageElement, THREE.Texture>();
  private unitEntries = new Map<string, UnitMeshEntry>();
  private unitDrawPositions = new Map<string, { x: number; y: number }>();
  private architectureFxMaskKey = "";
  private architectureFxMaskUri: string | null = null;

  /** The elemental-FX canvas is intentionally between the ground renderer and the visual
   * actors/props canvas. Toggle the two Three-owned layers independently: sprites must move
   * above FX whenever it is active, while architectural decorations may need their scene depth. */
  setSpritesAndDecorationsVisible(unitsVisible: boolean, decorationsVisible = unitsVisible): void {
    this.unitGroup.visible = unitsVisible;
    this.decorGroup.visible = decorationsVisible;
  }

  hasArchitecture(): boolean {
    return this.engine.decorations.some(p => !!DECORATIONS[p.id]?.model3d);
  }

  /** Water FX is a screen-space layer, so keep its broad quads off tactical buildings. */
  waterFxTouchesArchitecture(col: number, row: number, radiusTiles = 1): boolean {
    const source = this.engine.effectAnchor(col, row);
    const fxRadius = source.tile * radiusTiles;
    return this.engine.decorations.some(placement => {
      if (!DECORATIONS[placement.id]?.model3d) return false;
      const cells = placedFootprint(placement);
      return cells.some(cell => {
        const anchor = this.engine.effectAnchor(placement.x + cell.dx, placement.y + cell.dy);
        return Math.hypot(source.worldX - anchor.worldX, source.worldY - anchor.worldY) < fxRadius + source.tile * 0.45;
      });
    });
  }

  /** Editor edits replace gameplay data while retaining shaders, textures and targets. */
  setPreviewEngine(engine: BattleEngine): void {
    const previous = this.engine;
    const groundChanged = previous.cols !== engine.cols || previous.rows !== engine.rows ||
      previous.tiles.some((id, i) => id !== engine.tiles[i] || previous.tileVariants[i] !== engine.tileVariants[i] || previous.tileRots[i] !== engine.tileRots[i]);
    if (groundChanged) this.builtMissionId = "";
    this.engine.architectureRenderedInThree = false;
    this.engine = engine;
    engine.architectureRenderedInThree = true;
    this.builtDecorKey = "";
  }

  pickArchitecture(x: number, y: number, width: number, height: number): DecorationPlacement | null {
    const ray = new THREE.Raycaster();
    ray.setFromCamera(new THREE.Vector2(x / width * 2 - 1, 1 - y / height * 2), this.camera);
    const entries = [...this.wallEntries, ...this.decorEntries.filter(e => !!DECORATIONS[e.placement.id]?.model3d)];
    const hits = ray.intersectObjects(entries.filter(e => e.mesh.visible).map(e => e.mesh));
    const hit = hits[0];
    return hit ? entries.find(e => e.mesh === hit.object)?.placement ?? null : null;
  }

  // MILESTONE 3 — real world-space ground mist + drift particles, owned end-to-end by
  // ThreeAtmosphere (see that file's header comment for why scene.fog isn't used and why this
  // sits at Z > 3, strictly above every mesh above). lastFrameTime is only for this: nothing
  // else in the file needs a real dt (render() takes cssW/cssH only, see its own comment).
  private atmosphere = new ThreeAtmosphere();
  private lastFrameTime = performance.now();

  // MILESTONE 4 — bloom applies to the whole scene, per direct instruction (previously
  // selective, wisps-only — see git history if that's ever wanted back). bloomComposer renders
  // the real scene through UnrealBloomPass (which extracts/blurs whatever clears
  // BLOOM_THRESHOLD on its own), finalComposer renders it again normally and additively mixes
  // that bloom texture back in via mixPass.
  private bloomComposer: EffectComposer;
  private finalComposer: EffectComposer;
  private bloomPass: UnrealBloomPass;
  // Embers still mark themselves onto this layer (see ThreeAtmosphere.markBloomLayer) from
  // when bloom was selective — harmless now that bloom applies to everything regardless of
  // layer, kept only so that call site doesn't need its own removal too.
  private readonly bloomLayerIndex = 1;

  constructor(
    canvas: HTMLCanvasElement,
    private engine: BattleEngine,
  ) {
    groundDepthLayer(this.fallbackMaterial, this.groundDepthLayerEnabled);
    // An open battle keeps its engine through hot reload. Apply the same enclosure
    // repair here so it receives the wall changes without restarting the battle.
    if (engine.mission.id.startsWith("watchtower-")) {
      const walls = closeWatchtowerWalls(engine.mission.id, engine.tiles, engine.cols, engine.rows, engine.decorations);
      engine.decorations.splice(0, engine.decorations.length, ...walls);
      engine.refreshDecorOverlay();
    }
    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: false });
    this.renderer.setClearColor(0x000000, 1);
    this.renderer.shadowMap.enabled = true;
    // Medium's lighter shadows use PCF filtering; the low preset disables shadows entirely.
    this.renderer.shadowMap.type = getDevGfx().softShadows ? THREE.PCFShadowMap : THREE.BasicShadowMap;
    this.camera = new THREE.OrthographicCamera(0, 1, 0, 1, 0.1, 20000);
    this.camera.position.z = 100;
    // Author-controlled lighting (Mission.environment/sunIntensity/ambientIntensity, editable in
    // the Map Editor's "Iluminação" section — see GameApp.tsx) — an explicit sunIntensity/
    // ambientIntensity always wins; otherwise "indoor" gets its own flatter preset, and anything
    // else (including missing/"outdoor") gets the renderer's own default.
    const indoor = engine.mission.environment === "indoor";
    this.timeOfDay = engine.mission.timeOfDay ?? "day";
    const tod = TIME_OF_DAY_LIGHT[this.timeOfDay];
    const isDay = this.timeOfDay === "day" || this.timeOfDay === "noon";
    this.sunLight.intensity = engine.mission.sunIntensity ?? (indoor ? INDOOR_SUN_INTENSITY : isDay ? DEFAULT_SUN_INTENSITY : tod.key);
    this.hemiLight.intensity = engine.mission.ambientIntensity ?? (indoor ? INDOOR_AMBIENT_INTENSITY : isDay ? DEFAULT_AMBIENT_INTENSITY : tod.ambient);
    this.baseSunIntensity = this.sunLight.intensity;
    this.baseHemiIntensity = this.hemiLight.intensity;
    this.calibSunIntensity = isDay ? this.baseSunIntensity : DEFAULT_SUN_INTENSITY;
    this.calibHemiIntensity = isDay ? this.baseHemiIntensity : DEFAULT_AMBIENT_INTENSITY;
    this.calibSunColor.copy(this.sunLight.color);
    this.calibHemiColor.copy(this.hemiLight.color);
    if (!isDay) {
      (tod.moon ? this.moonLight : this.sunLight).color.setHex(tod.keyColor);
      this.hemiLight.color.setHex(tod.skyColor);
    }
    this.moonLight.shadow.mapSize.set(2048, 2048);
    this.moonLight.shadow.bias = -0.0015;
    this.scene.add(this.moonLight);
    this.scene.add(this.moonLight.target);
    this.sunLight.castShadow = true;
    // 2048, not 1024 — casters are small boxes (a fraction of a unit's own width), so a coarser
    // map under-resolves them into faint/noisy blobs even at full light intensity.
    this.sunLight.shadow.mapSize.set(2048, 2048);
    this.sunLight.shadow.bias = -0.0015;
    this.scene.add(this.hemiLight);
    this.scene.add(this.sunLight);
    this.scene.add(this.sunLight.target);
    this.scene.add(this.shadowCasterGroup);
    this.scene.add(this.backdropMesh);
    this.scene.add(this.tileGroup);
    this.scene.add(this.webGroup);

    this.scene.add(this.gridGlowGroup);
    this.scene.add(this.overlayGroup);

    this.scene.add(this.contactShadowGroup);
    this.scene.add(this.decorContactGroup);
    this.scene.add(this.decorGroup);
    this.scene.add(this.wallGroup);
    this.scene.add(this.unitGroup);
    this.scene.add(this.flameHaloGroup);
    this.scene.add(this.fogMask.mesh);
    for (const placement of engine.elementalFxPlacements) {
      if (placement.family !== "procedural_pixel" || !placement.element) continue;
      const emitter = new ProceduralElementEmitter(this.scene, pixelPreset(placement.element as PixelElement, placement.preset), placement.parameters);
      this.pixelElementEmitters.push({ placement, emitter });
    }
    // Sized to cover every light the map carries, so no torch goes dark just because the
    // camera is looking elsewhere; POINT_LIGHT_POOL stays the floor.
    const mapUnitLights = engine.units.reduce((count, u) => {
      if (!UNIT_LIGHT_DEFS[u.classId]) return count;
      return count + (u.classId === "familiar3" ? footprint(u).length : u.classId === "familiar4" ? 3 : 1);
    }, 0);
    // Familiar lights are created after the renderer when a Conjurer summons them. Reserve
    // room now for every tier (one + one + Radiante's three + the Titan's six footprint lights).
    const futureFamiliarLights = engine.units.filter((u) => u.classId === "conjurer").length * 11;
    const mapPixelLights = engine.elementalFxPlacements.filter((p) => p.family === "procedural_pixel" && p.parameters?.lightEnabled !== false).length;
    const mapLights = engine.decorations.filter((p) => LIGHT_DEFS[p.id]).length + mapUnitLights + futureFamiliarLights + mapPixelLights;
    const poolSize = Math.max(POINT_LIGHT_POOL, mapLights);
    for (let i = 0; i < poolSize; i++) {
      const pl = new THREE.PointLight(0xffffff, 0, 1, LIGHT_DECAY);
      if (i < POINT_SHADOW_LIGHTS) {
        pl.castShadow = true;
        pl.shadow.mapSize.set(1024, 1024);
        pl.shadow.bias = -0.003;
        pl.shadow.camera.near = 2;
        pl.shadow.camera.layers.set(PROXY_LAYER);
      }
      this.pointLights.push(pl);
      this.scene.add(pl);
    }
    for (let i = 0; i < BOUNCE_LIGHT_POOL; i++) {
      const bl = new THREE.PointLight(0xffffff, 0, 1, 1);
      this.bounceLights.push(bl);
      this.scene.add(bl);
    }
    // Always in the scene, intensity 0 when idle (see vfxLights). A borrower past the pool gets a
    // detached light: no glow for that extra target, but no recompile either.
    for (let i = 0; i < VFX_LIGHT_POOL; i++) {
      const light = new THREE.PointLight(0xffffff, 0, 1, 2);
      light.userData.vfxFree = true;
      this.vfxLightPool.push(light);
      this.scene.add(light);
    }

    this.scene.add(this.healingSpellLight);
    this.scene.add(this.atmosphere.group);
    this.scene.add(this.spellVfxScene);
    for(let i=0;i<3;i++){
      const light=new THREE.PointLight(0xb6e7ff,0,1,1.8);light.layers.enable(SPELL_VFX_LAYER);
      if(i===0){light.castShadow=true;light.shadow.mapSize.set(512,512);light.shadow.bias=-.001;}
      this.scene.add(light);this.blizzardLightPulses.push({light,age:10});
    }
    // Construct synchronously: every live Fireball impact particle is procedural, so no image
    // request can leave the spell without its flight or explosion when cast immediately.
    this.fireballVfx = new FireballVFX(this.spellVfxScene, this.camera, this.tavernFireball);
    this.engine.fireballVfxAvailable = true;
    this.causticVenomVfx = new CausticVenomVFX(this.spellVfxScene);
    this.engine.causticVenomVfxAvailable = true;
    this.phantasmalForceVfx = new PhantasmalForceVFX(this.spellVfxScene);
    this.phantasmalForceVfx.setSettings(getActivePhantasmalForceSettings());
    this.engine.phantasmalForceVfxAvailable = true;
    this.blessVfx = new BlessVFX(this.spellVfxScene);
    this.blessVfx.setSettings(getActiveBlessVfxSettings());
    this.engine.blessVfxAvailable = true;
    this.magicMissileV2Vfx = new MagicMissileV2VFX(this.spellVfxScene);
    this.magicMissileV2Vfx.setSettings(getActiveMagicMissileV2Settings());
    this.engine.magicMissileV2VfxAvailable = true;
    this.webOfDreamsVfx = new WebOfDreamsVFX(this.spellVfxScene);
    this.engine.burningHandsV2VfxAvailable = true;

    // Only the wisp embers ever render into the bloom-only pass (everything else gets forced to
    // black during it, see render()) — a low fixed threshold is correct now, since there's
    // nothing else present that could wrongly cross it regardless of setting; the intensity
    // slider maps directly to strength, which alone gets dramatic at high values against a
    // black backdrop.
    this.atmosphere.markBloomLayer(this.bloomLayerIndex);

    // Built at (1,1) here; setSize() (always called at least once before the first real render,
    // same as the camera/renderer above) gives both composers real dimensions.
    this.bloomPass = new UnrealBloomPass(new THREE.Vector2(1, 1), engine.mission.bloomIntensity ?? DEFAULT_BLOOM_INTENSITY, BLOOM_RADIUS, BLOOM_THRESHOLD);
    this.bloomComposer = new EffectComposer(this.renderer);
    this.bloomComposer.renderToScreen = false;
    this.bloomComposer.addPass(new RenderPass(this.scene, this.camera));
    this.bloomComposer.addPass(this.bloomPass);

    const mixPass = new ShaderPass(
      new THREE.ShaderMaterial({
        uniforms: {
          baseTexture: { value: null },
          bloomTexture: { value: this.bloomComposer.renderTarget2.texture },
        },
        vertexShader: /* glsl */ `
          varying vec2 vUv;
          void main() {
            vUv = uv;
            gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
          }
        `,
        fragmentShader: /* glsl */ `
          uniform sampler2D baseTexture;
          uniform sampler2D bloomTexture;
          varying vec2 vUv;
          void main() {
            gl_FragColor = texture2D(baseTexture, vUv) + vec4(1.0) * texture2D(bloomTexture, vUv);
          }
        `,
        defines: {},
      }),
      "baseTexture",
    );
    mixPass.needsSwap = true;

    this.finalComposer = new EffectComposer(this.renderer);
    this.finalComposer.addPass(new RenderPass(this.scene, this.camera));
    this.finalComposer.addPass(mixPass);
    // Combining two textures with raw shader math (above) bypasses the renderer's own automatic
    // output color-space encoding that a composer's LAST pass would normally apply when it
    // renders straight to the canvas — this pass restores it, matching the official Three.js
    // selective-bloom example's own pipeline exactly.
    this.finalComposer.addPass(new OutputPass());
  }

  /** Aims the sun so its fixed-direction shadow follows whatever's actually on screen (camera
   * pans; the shadow-caster geometry doesn't move relative to the world, so the light has to
   * instead) — same reasoning as why tiles are built once and only the camera moves (see module
   * comment). The shadow camera's frustum SIZE only depends on viewport size, so that part is
   * cached and skipped most frames; target/position are cheap vector math, recomputed every
   * frame unconditionally. */
  private updateSun(cssW: number, cssH: number, camX: number, camY: number): void {
    const centerX = camX + cssW / 2;
    const centerY = -camY - cssH / 2;
    this.sunLight.target.position.set(centerX, centerY, 0);
    this.sunLight.position.set(centerX, centerY, 0).addScaledVector(this.sunDir, -SUN_DISTANCE);
    this.moonLight.target.position.set(centerX, centerY, 0);
    this.moonLight.position.set(centerX, centerY, 0).addScaledVector(this.moonDir, -SUN_DISTANCE);
    if (cssW === this.lastShadowFrustumW && cssH === this.lastShadowFrustumH) return;
    this.lastShadowFrustumW = cssW;
    this.lastShadowFrustumH = cssH;
    // Half-diagonal (plus margin for shadow-caster elevation reach) rather than half-width/
    // height: the shadow camera looks along SUN_DIRECTION, not straight down -Z like the main
    // camera, so it needs to cover the visible box from an angle, not just match its footprint.
    const half = Math.hypot(cssW, cssH) * 0.65 + 250;
    for (const light of [this.sunLight, this.moonLight]) {
      const shadowCam = light.shadow.camera as THREE.OrthographicCamera;
      shadowCam.left = -half;
      shadowCam.right = half;
      shadowCam.top = half;
      shadowCam.bottom = -half;
      shadowCam.near = 10;
      shadowCam.far = SUN_DISTANCE * 2.2;
      shadowCam.updateProjectionMatrix();
    }
  }

  /** Travel direction of a sky light from its azimuth (screen direction its shadows fall, deg,
   * 0 = right, 90 = down) and elevation (deg above the horizon). The default sun (53.13°, 45°)
   * gives exactly SUN_DIRECTION. */
  private static skyDirection(out: THREE.Vector3, azimuthDeg: number, elevationDeg: number): THREE.Vector3 {
    const az = (azimuthDeg * Math.PI) / 180;
    const el = (Math.max(3, Math.min(89, elevationDeg)) * Math.PI) / 180;
    return out.set(Math.cos(el) * Math.cos(az), -Math.cos(el) * Math.sin(az), -Math.sin(el)).normalize();
  }

  /** Per-frame Sun/Moon state: directions from Dev Controls, which one is up from the map's
   * time of day (see TIME_OF_DAY_LIGHT), and their shadows. Night: the Sun goes out and the Moon
   * (its own DirectionalLight + shadow map) lights the scene at the map's key intensity. The
   * silhouette shadow casters are vertical cards; they're turned about Z by the lit sky light's
   * azimuth change from the default so they stay broadside to it (the default sun leaves them
   * exactly as built). */
  private syncSky(): void {
    const gfx = getDevGfx();
    const tod = TIME_OF_DAY_LIGHT[this.timeOfDay];
    const night = tod.moon;
    ThreeBattleRenderer.skyDirection(this.sunDir, gfx.sunAzimuth, tod.elevation ?? gfx.sunElevation);
    ThreeBattleRenderer.skyDirection(this.moonDir, gfx.moonAzimuth, gfx.moonElevation);
    this.sunLight.intensity = night ? 0 : this.baseSunIntensity;
    this.moonLight.intensity = night ? this.baseSunIntensity : 0;
    this.hemiLight.intensity = this.baseHemiIntensity;
    this.sunLight.castShadow = !night && gfx.realShadows;
    this.moonLight.castShadow = night && gfx.realShadows;
    this.moonLight.shadow.radius = gfx.softShadows ? SHADOW_RADIUS_SOFT : SHADOW_RADIUS_HARD;
    const delta = (((night ? gfx.moonAzimuth : gfx.sunAzimuth) - 53.13) * Math.PI) / 180;
    const spin = new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(0, 0, 1), -delta);
    for (const m of this.shadowCasterGroup.children) {
      if (!m.userData.baseQuat) m.userData.baseQuat = m.quaternion.clone();
      m.quaternion.copy(m.userData.baseQuat as THREE.Quaternion).premultiply(spin);
      // Turning a silhouette towards the sun/moon must pivot about its opaque feet,
      // not the image rectangle's center (asymmetric sprites otherwise slide sideways).
      if (m.userData.contactLocal && m.userData.contactAnchor) {
        const offset = this.shadowFootOffset.copy(m.userData.contactLocal as THREE.Vector3).multiply(m.scale).applyQuaternion(m.quaternion);
        m.position.copy(m.userData.contactAnchor as THREE.Vector3).sub(offset);
      }
    }
  }

  private lastSize = "";
  setSize(cssW: number, cssH: number, dpr: number): void {
    const sizeKey = `${cssW}:${cssH}:${dpr}`;
    if (this.lastSize === sizeKey) return;
    this.lastSize = sizeKey;
    this.renderer.setPixelRatio(dpr);
    this.renderer.setSize(Math.max(1, cssW), Math.max(1, cssH), false);
    this.camera.left = 0;
    this.camera.right = Math.max(1, cssW);
    this.camera.top = Math.max(1, cssH);
    this.camera.bottom = 0;
    this.camera.updateProjectionMatrix();
    // MILESTONE 4 — EffectComposer captures the renderer's pixel ratio ONCE at construction
    // time and never re-reads it; without this explicit setPixelRatio() call, bloom would stay
    // locked to whatever dpr was active when the composer was built (effectively 1, since this
    // constructor runs before the first real setSize()), rendering at the wrong resolution on
    // any HiDPI display. setPixelRatio() also calls setSize() internally (with the CURRENT
    // this._width/_height, still 1x1 the very first time — composer.setSize() right after this
    // is what gives it real dimensions), which is why both calls are needed here, in this order,
    // on BOTH composers now (selective bloom uses two).
    this.bloomComposer.setPixelRatio(dpr);
    this.bloomComposer.setSize(Math.max(1, cssW), Math.max(1, cssH));
    this.finalComposer.setPixelRatio(dpr);
    this.finalComposer.setSize(Math.max(1, cssW), Math.max(1, cssH));
  }

  private materialFor(id: TerrainId, variant: number): THREE.MeshLambertMaterial {
    const key = `${id}:${variant}`;
    const hit = this.materialCache.get(key);
    if (hit) return hit;
    const groundVariant = GROUND_VARIANT_LAYERS.find((entry) => entry.terrain === id && entry.variant === variant);
    if (groundVariant) {
      const texture = this.tacticsTexture(groundVariantMaps(groundVariant).color);
      const material = new THREE.MeshLambertMaterial({ map: texture });
      this.groundAO.patch(material);
      groundDepthLayer(material, this.groundDepthLayerEnabled);
      this.materialCache.set(key, material);
      return material;
    }
    const variants = this.engine.art.tiles[id];
    const img = variants?.[variant] ?? variants?.[0];
    if (!img) return this.fallbackMaterial;
    const tex = new THREE.Texture(img);
    // Three's default flipY=true is what this needs: with the Y-negation in hexWorld/
    // render() (see module comment), a plane's local Y=-0.5 edge ends up at the screen
    // BOTTOM, and flipY=true samples the image's bottom row there — matching Canvas2D's
    // drawImage orientation. (Verified numerically, not just by eye — see module comment.)
    tex.needsUpdate = true;
    tex.colorSpace = THREE.SRGBColorSpace;
    tex.minFilter = THREE.LinearFilter;
    tex.magFilter = THREE.LinearFilter;
    tex.wrapS = THREE.ClampToEdgeWrapping;
    tex.wrapT = THREE.ClampToEdgeWrapping;
    tex.repeat.set(GROUND_TEXTURE_SPAN, GROUND_TEXTURE_SPAN);
    tex.offset.set(GROUND_TEXTURE_INSET, GROUND_TEXTURE_INSET);
    const mat = new THREE.MeshLambertMaterial({ map: tex });
    this.groundAO.patch(mat);
    groundDepthLayer(mat, this.groundDepthLayerEnabled);
    this.materialCache.set(key, mat);
    return mat;
  }

  /** (Re)builds every tile mesh at its fixed world position. Called once per map and again
   * whenever board dimensions or the mission itself changes — never on an ordinary camera
   * pan/zoom, which only ever moves the camera (see render()). */
  private ensureBuilt(tile: number): void {
    const engine = this.engine;
    // tile is part of the identity check (not just cols/rows/missionId) — every mesh's world
    // position and scale is baked in at build time from this.boardWorld(..., tile), so a zoom change
    // (which changes `tile` without touching cols/rows/missionId) has to trigger a full rebuild
    // too. Decorations already keyed on tile (see ensureDecorBuilt's builtDecorKey); terrain
    // didn't, so zooming left the ground grid frozen at its old scale/position while the camera,
    // decorations and units all repositioned themselves for the new tile size every frame —
    // reads as tiles vanishing/sliding out from under everything else on zoom.
    if (this.builtCols === engine.cols && this.builtRows === engine.rows && this.builtMissionId === engine.mission.id && this.builtTile === tile)
      return;
    for (const entry of this.tileMeshes.values()) this.tileGroup.remove(entry.mesh);
    this.tileMeshes.clear();
    this.terrainSolidKey = "";
    this.builtCols = engine.cols;
    this.builtRows = engine.rows;
    this.builtMissionId = engine.mission.id;
    this.builtTile = tile;

    for (let row = 0; row < engine.rows; row++) {
      for (let col = 0; col < engine.cols; col++) {
        const key = row * engine.cols + col;
        const id = tileAt(engine.tiles, engine.cols, col, row);
        const variant = engine.tileVariants[key] ?? 0;
        const rot = engine.tileRots[key] ?? 0;
        const mat = this.materialFor(id, variant);
        const mesh = new THREE.Mesh(this.hexGeo, mat);
        const { wx, wy } = this.boardWorld(col, row, tile);
        mesh.scale.set(tile * 2, tile * 2, 1);
        // Y negated — see module comment on the frustum/Y-flip.
        mesh.position.set(wx, -wy, 0);
        // A turned hex spins about its own center — see Canvas2D renderGround's identical
        // rot*PI/3 comment. Negated: a mesh's own local rotation isn't touched by the
        // position negation above, so Three's standard (non-inverted-frustum) CCW-positive
        // Z-rotation would appear CCW on screen — the opposite of ctx.rotate()'s CW-positive
        // screen convention — unless flipped here.
        if (rot) mesh.rotation.z = (-rot * Math.PI) / 3;
        // MILESTONE 2 — the ground is the one surface real shadows land on (see handoff doc);
        // it never casts (stays flat, castShadow defaults to false).
        mesh.receiveShadow = true;
        // Void is an eraser, not a tile: nothing is drawn, so the mission backdrop shows through
        // and a map's outline doesn't have to be a full rows x cols rectangle. Kept as a hidden
        // mesh (not skipped) so syncDirtyTiles can reveal it if the cell ever stops being void.
        mesh.visible = id !== "void";
        this.tileGroup.add(mesh);
        this.tileMeshes.set(key, { mesh, id, variant, rot });
      }
    }
  }

  private tacticsTexture(path: string): THREE.Texture {
    let texture = this.wallTextures.get(path);
    if (!texture) {
      texture = new THREE.TextureLoader().load(path);
      texture.colorSpace = THREE.SRGBColorSpace;
      texture.wrapS = texture.wrapT = THREE.RepeatWrapping;
      this.wallTextures.set(path, texture);
    }
    return texture;
  }

  private groundHeight(col: number, row: number, tile = this.builtTile): number {
    if (!this.engine.tacticsCamera) {
      const index = row * this.engine.cols + col;
      const id = tileAt(this.engine.tiles, this.engine.cols, col, row);
      const authored = this.engine.mission.terrainElevations?.[index] ?? 0;
      const level = Math.max(authored, TERRAIN[id].height ?? 0);
      return level * tile * 1.5;
    }
    const point = this.boardWorld(col, row, tile);
    return this.landscape?.heightAt(point.wx, -point.wy) ?? ((this.engine.mission.terrainElevations?.[row * this.engine.cols + col] ?? TERRAIN[tileAt(this.engine.tiles, this.engine.cols, col, row)].height ?? 0) * tile * 1.5);
  }

  private syncElevationSteps(tile: number): void {
    const engine = this.engine;
    this.elevationSteps.group.visible = !engine.tacticsCamera && !!engine.mission.squareTiles;
    if (engine.tacticsCamera || !engine.mission.squareTiles) return;
    const key = JSON.stringify([tile, engine.cols, engine.rows, engine.tiles, engine.mission.terrainElevations]);
    if (key === this.elevationStepsKey) return;
    this.elevationSteps.rebuild(engine.cols, engine.rows, tile, (col, row) => {
      if (col < 0 || row < 0 || col >= engine.cols || row >= engine.rows) return 0;
      const id = tileAt(engine.tiles, engine.cols, col, row);
      if (id === "void") return 0;
      const index = row * engine.cols + col;
      return Math.max(engine.mission.terrainElevations?.[index] ?? 0, TERRAIN[id].height ?? 0);
    }, tile * 1.5);
    this.scene.add(this.elevationSteps.group);
    this.elevationStepsKey = key;
  }

  private syncWater(tile: number): void {
    const engine = this.engine;
    this.water.setVersion(engine.mission.waterVersion ?? "v2");
    const key = JSON.stringify([tile, engine.cols, engine.rows, engine.mission.waterLevels, engine.mission.waterPatches, engine.mission.waterFootprints, engine.tiles, this.terrainSolidKey, engine.fogged ? engine.visVersion : "clear"]);
    if (key !== this.waterKey) {
      // Straight-border maps cut the ground on a rectangle (see syncTerrainHeight's bounds);
      // the water is cut on that same rectangle and fills edge hexes out to it.
      const board = hasSquareMapBorder(engine.tiles, engine.cols, engine.rows, engine.mission.squareTiles)
        ? { minX: 0, maxX: SQRT3 * engine.cols, minY: BOARD_PAD_MUL + 0.25, maxY: BOARD_PAD_MUL + 0.25 + engine.rows * 1.5 }
        : undefined;
      this.water.rebuild(engine.cols, engine.rows, tile, engine.mission.waterLevels ?? [], (col, row) =>
        tileAt(engine.tiles, engine.cols, col, row) !== "void" && (!engine.fogged || engine.explored(col, row) || engine.visible(col, row)),
        (x, y) => this.landscape?.heightAt(x, y) ?? 0, engine.mission.waterFootprints, engine.mission.waterPatches, board);
      this.scene.add(this.water.mesh);
      this.waterKey = key;
    }
    this.water.mesh.visible = true;
    this.water.flat.value = engine.tacticsCamera ? 0 : 1;
    // Use battle simulation time so water pauses with the scene behind a briefing/dialog.
    this.water.time.value = engine.time;
  }

  private syncTerrainHeight(tile: number): void {
    const squareBorder = hasSquareMapBorder(this.engine.tiles, this.engine.cols, this.engine.rows, this.engine.mission.squareTiles);
    const floorRects = mapFloorRects(this.engine.tiles, this.engine.cols, this.engine.rows, this.engine.decorations);
    const cells: { x: number; y: number; height: number; col: number; row: number; entry: TileMeshEntry }[] = [];
    for (const [key, entry] of this.tileMeshes) {
      const col = key % this.engine.cols, row = Math.floor(key / this.engine.cols);
      const terrainLevel = Math.max(this.engine.mission.terrainElevations?.[key] ?? 0, TERRAIN[entry.id].height ?? 0);
      entry.mesh.position.z = this.engine.tacticsCamera ? 0 : terrainLevel * tile * 1.5 + 0.02;
      entry.mesh.castShadow = false;
      const floorRect = floorRects.get(key);
      const trimmed = floorRect && (floorRect.minX > col * SQRT3 + 1e-6 || floorRect.maxX < (col+1)*SQRT3 - 1e-6 || floorRect.minY > row*1.5 + 1e-6 || floorRect.maxY < (row+1)*1.5 - 1e-6);
      const mapEdge = trimmed ||
        (col === 0 || row === 0 || col === this.engine.cols - 1 || row === this.engine.rows - 1 ||
          [-1, 0, 1].some(dy => [-1, 0, 1].some(dx =>
            tileAt(this.engine.tiles, this.engine.cols, col + dx, row + dy) === "void")));
      // The continuous floor supplies the straight perimeter; a hex decal at its
      // boundary would otherwise protrude beyond it and restore scalloped edges.
      entry.mesh.visible = !this.engine.mission.squareTiles && !this.engine.tacticsCamera && entry.id !== "void" && (!squareBorder || !mapEdge);
      entry.mesh.userData.cell = { col, row };
      if (entry.id !== "void") cells.push({ x: entry.mesh.position.x, y: entry.mesh.position.y,
        height: terrainLevel * tile * 1.5, col, row, entry });
    }
    // Both cameras use the continuous ground fill around the outer hexes. In 2D it
    // sits beneath the original tiles, preserving their authored elevation steps.
    const stamp = `continuous-atlas-v7:${squareBorder ? "square" : "hex"}:${JSON.stringify([...floorRects])}:${this.engine.tacticsCamera}:${this.engine.cols}:${this.engine.rows}:${tile}:` + cells.map(c =>
      `${c.col},${c.row},${c.height},${c.entry.id},${c.entry.variant},${c.entry.rot}`).join(";");
    if (stamp !== this.terrainSolidKey && cells.length) {
      if (!this.cliffMaterial) {
        this.cliffMaterial = new THREE.MeshStandardMaterial({
          map: this.tacticsTexture(this.engine.mission.id.startsWith("watchtower-")
            ? this.engine.art.tiles[this.engine.mission.baseTile ?? "nave"]?.[this.engine.mission.baseVariant ?? 0]?.src ?? OUTER_WALL_TEXTURE
            : OUTER_WALL_TEXTURE), color: this.engine.mission.id.startsWith("watchtower-") ? 0xffffff : OUTER_WALL_COLOR, roughness: 1,
        });
        lightTacticsMaterial(this.cliffMaterial);
        groundDepthLayer(this.cliffMaterial, this.groundDepthLayerEnabled);
      }
      const heights = new Map(cells.map(c => [c.row * this.engine.cols + c.col, c.height]));
      // Floor cells share the walls' aligned columns on every map. Subdivide those exact
      // rectangles so triangle-centroid clipping cannot leave diagonal edge gaps.
      const bounds = { minX: 0,
        minY: -tile * (BOARD_PAD_MUL + (squareBorder ? 0.25 : 0.5) + this.engine.rows * 1.5),
        maxX: tile * SQRT3 * (this.engine.cols + (squareBorder ? 0 : 0.5)), maxY: -tile * (BOARD_PAD_MUL + (squareBorder ? 0.25 : 0)) };
      const elevation = (x: number, y: number) => {
        if (!this.engine.tacticsCamera) return -0.02;
        // Interpolate nearby terrain elevations into connected slopes. The movement grid
        // supplies placement coordinates but no longer defines the ground's polygon edges.
        const row0 = Math.round((-y / tile - BOARD_PAD_MUL - 1) / 1.5);
        let weighted = 0, total = 0;
        for (let row = row0-1; row <= row0+1; row++) {
          const col0 = Math.round(x / (tile * SQRT3) - 0.5 * (row & 1) - 0.5);
          for (let col = col0-1; col <= col0+1; col++) {
            const height = heights.get(row * this.engine.cols + col);
            if (height === undefined || col < 0 || col >= this.engine.cols || row < 0 || row >= this.engine.rows) continue;
            const center = this.boardWorld(col, row, tile);
            const distance = ((x-center.wx)**2 + (y+center.wy)**2) / (tile*tile);
            const weight = 1 / (distance + 0.04)**2;
            weighted += height * weight; total += weight;
          }
        }
        return total ? weighted / total : 0;
      };
      const surface = squareBorder ? buildLandscape(bounds, { x: tile * SQRT3 / 4, y: tile * 1.5 / 4 }, tile * 0.45, elevation, (x, y) => {
        // Landscape cutouts use square map-cell bounds, not the nearest-hex classifier.
        // The latter puts six-sided scallops back on every outer edge despite the fine mesh.
        const cell = this.cellAtSquareWorld(x / tile, -y / tile);
        // Void is an eraser: never fill authored empty cells with tactical ground.
        if (cell < 0) return false;
        if (!heights.has(cell)) return false;
        const rect = floorRects.get(cell);
        const fx = x / tile, fy = -y / tile - BOARD_PAD_MUL - 0.25;
        if (!rect || !floorRectContains(rect, fx, fy)) return false;
        // Keep ground continuous under fog. FogMask is the opaque visual cover for
        // unseen cells; cutting the terrain here exposed square gaps at its edges.
        return true;
      }, true, {
        x: [...new Set([...floorRects.values()].flatMap(floorRectParts).flatMap(r => [r.minX * tile, r.maxX * tile]))],
        y: [...new Set([...floorRects.values()].flatMap(floorRectParts).flatMap(r => [-tile * (r.minY + BOARD_PAD_MUL + 0.25), -tile * (r.maxY + BOARD_PAD_MUL + 0.25)]))],
      }) : buildSteppedHexLandscape(bounds, cells, tile, tile * 0.45);
      const canvas = document.createElement("canvas");
      const scale = Math.min(1, 4096 / Math.max(bounds.maxX-bounds.minX, bounds.maxY-bounds.minY));
      canvas.width = Math.max(1, Math.ceil((bounds.maxX-bounds.minX)*scale));
      canvas.height = Math.max(1, Math.ceil((bounds.maxY-bounds.minY)*scale));
      const ctx = canvas.getContext("2d")!;
      ctx.fillStyle = "#343127";
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      // Fill seams with the map's authored base terrain. Older maps without a baseTile
      // use their most common terrain, including its most common texture variant.
      const terrainCounts = new Map<TerrainId, number>();
      for (const cell of cells) terrainCounts.set(cell.entry.id, (terrainCounts.get(cell.entry.id) ?? 0) + 1);
      const baseTerrain = this.engine.mission.baseTile
        ?? [...terrainCounts].sort((a, b) => b[1] - a[1])[0]?.[0];
      const variantCounts = new Map<number, number>();
      for (const cell of cells) if (cell.entry.id === baseTerrain)
        variantCounts.set(cell.entry.variant, (variantCounts.get(cell.entry.variant) ?? 0) + 1);
      const baseVariant = [...variantCounts].sort((a, b) => b[1] - a[1])[0]?.[0] ?? 0;
      const borderImage = baseTerrain ? this.engine.art.tiles[baseTerrain]?.[baseVariant]
        ?? this.engine.art.tiles[baseTerrain]?.[0] : undefined;
      if (borderImage?.naturalWidth) {
        const repeatCanvas = document.createElement("canvas");
        repeatCanvas.width = repeatCanvas.height = Math.max(1, Math.ceil(tile * scale * 2));
        drawGroundTexture(repeatCanvas.getContext("2d")!, borderImage, 0, 0, repeatCanvas.width, repeatCanvas.height);
        const pattern = ctx.createPattern(repeatCanvas, "repeat");
        if (pattern) {
          ctx.fillStyle = pattern;
          ctx.fillRect(0, 0, canvas.width, canvas.height);
        }
      }
      for (const cell of cells) {
        const image = this.engine.art.tiles[cell.entry.id]?.[cell.entry.variant] ?? this.engine.art.tiles[cell.entry.id]?.[0];
        const px = (cell.x-bounds.minX)*scale, py = (bounds.maxY-cell.y)*scale, radius = tile*scale;
        // Overlap atlas paint slightly past each hex edge. Linear filtering while the
        // continuous terrain is viewed obliquely otherwise samples the underlay as seams.
        const bleed = 1.5;
        ctx.save(); ctx.translate(px, py); ctx.beginPath();
        if (this.engine.mission.squareTiles) {
          ctx.rect(-radius * SQRT3 / 2 - bleed, -radius * 0.75 - bleed, radius * SQRT3 + bleed * 2, radius * 1.5 + bleed * 2);
        } else for (let i=0; i<6; i++) {
          const angle = (60*i-30)*Math.PI/180;
          const x = Math.cos(angle)*(radius+bleed), y = Math.sin(angle)*(radius+bleed);
          if (i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
        }
        ctx.closePath(); ctx.clip();
        if (image?.naturalWidth && isHexGroundVariant(cell.entry.id, cell.entry.variant)) {
          drawHexGround(ctx, image, 0, 0, cell.x * scale, -cell.y * scale, radius);
        } else {
          ctx.rotate(cell.entry.rot*Math.PI/3);
          if (image?.naturalWidth) drawGroundTexture(ctx, image,-radius-bleed,-radius-bleed,(radius+bleed)*2,(radius+bleed)*2);
        }
        ctx.restore();
      }
      this.landscapeTexture?.dispose();
      this.landscapeTexture = new THREE.CanvasTexture(canvas);
      this.landscapeTexture.colorSpace = THREE.SRGBColorSpace;
      this.landscapeTexture.anisotropy = Math.min(8, this.renderer.capabilities.getMaxAnisotropy());
      if (!this.landscapeMaterial) {
        this.landscapeMaterial = new THREE.MeshLambertMaterial();
        this.groundAO.patch(this.landscapeMaterial);
        groundDepthLayer(this.landscapeMaterial, this.groundDepthLayerEnabled);
      }
      this.landscapeMaterial.map = this.landscapeTexture;
      this.landscapeMaterial.needsUpdate = true;
      if (this.terrainSolid) {
        this.terrainSolid.geometry.dispose();
        this.terrainSolid.geometry = surface.geometry;
        this.terrainSolid.material = [this.landscapeMaterial, this.cliffMaterial];
      } else {
        this.terrainSolid = new THREE.Mesh(surface.geometry, [this.landscapeMaterial, this.cliffMaterial]);
        this.terrainSolid.castShadow = this.terrainSolid.receiveShadow = true;
        this.tileGroup.add(this.terrainSolid);
      }
      this.landscape = surface;
      this.terrainSolidKey = stamp;
      this.terrainRevision++;
    }
    if (this.terrainSolid) this.terrainSolid.visible = cells.length > 0;
  }

  private drapeTerrainOverlay(mesh: THREE.Mesh, source: THREE.BufferGeometry): void {
    if (!this.engine.tacticsCamera || !this.landscape) return;
    const key = `${this.terrainRevision}:${mesh.position.x}:${mesh.position.y}:${mesh.scale.x}:${mesh.scale.y}`;
    if (mesh.userData.surfaceKey !== key || mesh.userData.surfaceSource !== source) {
      (mesh.userData.surfaceGeometry as THREE.BufferGeometry | undefined)?.dispose();
      const geometry = source.clone();
      const position = geometry.getAttribute("position");
      const base = this.landscape.heightAt(mesh.position.x, mesh.position.y);
      for (let i=0; i<position.count; i++) position.setZ(i, position.getZ(i) +
        this.landscape.heightAt(mesh.position.x+position.getX(i)*mesh.scale.x,
          mesh.position.y+position.getY(i)*mesh.scale.y)-base);
      position.needsUpdate = true;
      geometry.computeBoundingSphere();
      mesh.userData.surfaceGeometry = geometry;
      mesh.userData.surfaceSource = source;
      mesh.userData.surfaceKey = key;
    }
    mesh.geometry = mesh.userData.surfaceGeometry;
  }

  /** Repositions/retextures only the tiles that actually changed since the last build (a chest
   * opened, a terrain-changing effect fired, ...) — cheap, since most frames change nothing. */
  private syncDirtyTiles(): void {
    const engine = this.engine;
    for (const [key, entry] of this.tileMeshes) {
      const row = Math.floor(key / engine.cols);
      const col = key % engine.cols;
      const id = tileAt(engine.tiles, engine.cols, col, row);
      const variant = engine.tileVariants[key] ?? 0;
      const rot = engine.tileRots[key] ?? 0;
      if (id !== entry.id || variant !== entry.variant) {
        if (id !== entry.id) this.aoTerrainVersion++;
        entry.mesh.visible = id !== "void";
        entry.mesh.material = this.materialFor(id, variant);
        entry.id = id;
        entry.variant = variant;
      }
      if (rot !== entry.rot) {
        entry.mesh.rotation.z = (-rot * Math.PI) / 3;
        entry.rot = rot;
      }
    }
  }

  /** Lazily loads a decoration's art file the same way BattleEngine.decorArtReady does — a prop
   * whose art hasn't finished loading yet just doesn't get a mesh (see ensureDecorBuilt) rather
   * than falling back to a placeholder, since ensureDecorBuilt reruns on the next dirty check
   * and a loading placeholder box would be more visually wrong than a prop appearing a frame
   * late. Shares `engine.art.decorations` with the existing Canvas2D renderer's own cache —
   * one image, loaded once, however many renderers end up reading it this milestone. */
  private decorImageReady(fileId: string): HTMLImageElement | null {
    let img = this.engine.art.decorations[fileId];
    if (!img) {
      img = new Image();
      img.src = decorationImage(fileId);
      decorationImageRetryWebp(img, fileId);
      this.engine.art.decorations[fileId] = img;
    }
    return img.naturalWidth > 0 ? img : null;
  }

  private decorMaterialFor(fileId: string, img: HTMLImageElement): THREE.MeshLambertMaterial {
    const hit = this.decorMatCache.get(fileId);
    if (hit) return hit;
    const tex = new THREE.Texture(img);
    tex.needsUpdate = true;
    tex.colorSpace = THREE.SRGBColorSpace;
    const photographicCave = fileId.startsWith("cave-basalt-") || fileId.startsWith("cave-limestone-007-") || fileId.startsWith("ice-photographic-010-") || (fileId.startsWith("forest-photographic-011-") || fileId.startsWith("forest-photographic-012-")) || (fileId.startsWith("farmland-photographic-013-") || fileId.startsWith("farmland-photographic-014-"));
    tex.minFilter = photographicCave ? THREE.LinearMipmapLinearFilter : THREE.LinearFilter;
    if (photographicCave) tex.anisotropy = Math.min(8, this.renderer.capabilities.getMaxAnisotropy());
    tex.magFilter = THREE.LinearFilter;
    tex.wrapS = THREE.ClampToEdgeWrapping;
    tex.wrapT = THREE.ClampToEdgeWrapping;
    // Decoration art is cut-out PNGs (alpha, not opaque like terrain tiles) — transparent:true
    // is required or the alpha channel is ignored and every prop draws as an opaque rectangle.
    // Lit (MeshLambertMaterial), so real scene lights — map PointLights included — illuminate
    // the prop; see syncSpriteExposure for how its look under the existing sun/sky is kept.
    const mat = new THREE.MeshLambertMaterial({ map: tex, transparent: true, depthWrite: false });
    mat.userData.lightReflectance = DECORATIONS[fileId]?.lightReflectance ?? 1;
    capSpriteLight(mat, DECOR_LIGHT_CAP);
    this.litSpriteMats.add(mat);
    this.decorMatCache.set(fileId, mat);
    return mat;
  }

  /** Shadow-only twin of decorMaterialFor, for true-silhouette shadow casting instead of the old
   * invisible-box caster: same cached texture (never re-decoded), but alphaTest instead of alpha
   * blending — Three's shadow depth pass respects alphaTest (hard cutout at that texture-alpha
   * threshold), giving a shadow shaped like the prop's actual cutout art, not a box. colorWrite
   * false keeps it invisible in the normal color pass (same trick the old box caster used) since
   * this mesh exists purely to cast into the shadow map. */
  private decorShadowMaterialFor(fileId: string, colorMat: THREE.MeshLambertMaterial): THREE.MeshBasicMaterial {
    const hit = this.decorShadowMatCache.get(fileId);
    if (hit) return hit;
    // side: DoubleSide is required for a flat plane to cast any shadow at all — Three's shadow
    // pass renders back-faces by default (to reduce self-shadow acne on closed volumes), and a
    // single flat PlaneGeometry has no back face for that pass to find, so without this the whole
    // caster silently draws nothing into the shadow map.
    const mat = new THREE.MeshBasicMaterial({ map: colorMat.map, alphaTest: 0.5, colorWrite: false, depthWrite: false, side: THREE.DoubleSide });
    this.decorShadowMatCache.set(fileId, mat);
    return mat;
  }

  /** Depth-only twin of decorMaterialFor for a house's fogCut mesh: no color, writes depth
   * where the art is solid (alphaTest). `transparent` keeps it in the transparent pass so its
   * renderOrder (99, just under the fog's 100) puts it after everything else visible. */
  private decorFogCutMaterialFor(fileId: string, colorMat: THREE.MeshLambertMaterial): THREE.MeshBasicMaterial {
    const hit = this.decorFogCutMatCache.get(fileId);
    if (hit) return hit;
    const mat = new THREE.MeshBasicMaterial({ map: colorMat.map, alphaTest: 0.5, colorWrite: false, depthWrite: true, transparent: true });
    this.decorFogCutMatCache.set(fileId, mat);
    return mat;
  }

  /** (Re)builds every ground/behind-layer decoration mesh at its fixed world position — same
   * "built once, camera moves instead" philosophy as tiles (see ensureBuilt). Skips "front"-
   * layer and foreground=true props on purpose: those are meant to occlude character sprites,
   * which still live on the separate units canvas STACKED ABOVE this one — moving them here
   * would put them permanently behind every unit instead. They keep rendering through
   * BattleEngine.renderUnitsAndOverlays exactly as before, unchanged, until units themselves
   * move onto this renderer and a real depth order between the two exists. */
  private ensureDecorBuilt(tile: number): void {
    const engine = this.engine;
    const wallDefinition = (id: string) => {
      const def = DECORATIONS[id];
      const wall = id === "watchtower-stone-open-door-2hex"
        ? DECORATIONS[engine.decorations.find(p => DECORATIONS[p.id]?.model3d === "wall")?.id ?? "wall-3d-tower"]
        : undefined;
      return engine.mission.id.startsWith("watchtower-") && (def?.model3d === "wall" || id === "watchtower-stone-open-door-2hex")
        ? { ...def, ...(wall ? { heightScale: wall.heightScale ?? 1, wallThicknessScale: wall.wallThicknessScale ?? 1 } : {}), wallTexture: engine.art.tiles[engine.mission.baseTile ?? "nave"]?.[engine.mission.baseVariant ?? 0]?.src ?? OUTER_WALL_TEXTURE }
        : def;
    };
    // Props can move or change visual treatment without changing the mission id/count
    // (editor previews and hot-reloaded DecorationDefs both do this). Include placement and
    // render-only scale/mirror settings so the mesh cache cannot keep the old-sized art.
    const placementKey = engine.decorations.map((p) => {
      const def = wallDefinition(p.id);
      const artId = decorationPlacementArt(p);
      const image = engine.art.decorations[artId];
      const imageReady = image?.naturalWidth ?? 0;
      return `${p.id}:${artId}@${p.x},${p.y},${p.rot ?? 0},${p.wallOrientation ?? "auto"},${def?.heightScale ?? 1},${def?.artScale ?? 1},${def?.mirrorAlternate ? 1 : 0},${def?.wallTexture ?? ""},${imageReady}`;
    }).join(";");
    const key = `${engine.mission.id}:${tile}:${engine.tacticsCamera}:${this.trees.revision}:${placementKey}`;
    if (key === this.builtDecorKey) return;
    for (const entry of this.wallEntries) {
      this.wallGroup.remove(entry.mesh);
      this.wallGroup.remove(entry.shadowMesh);
      entry.mesh.geometry.dispose();
      entry.mesh.material.dispose();
      entry.shadowMesh.geometry.dispose();
      entry.shadowMesh.material.dispose();
    }
    this.wallEntries = [];
    for (const entry of this.decorEntries) {
      this.decorGroup.remove(entry.mesh);
      if (entry.mesh.userData.importedTree) {
        (entry.mesh.material as THREE.Material).dispose();
      } else if (entry.mesh.userData.tacticsModel) {
        entry.mesh.geometry.dispose();
        const materials = Array.isArray(entry.mesh.material) ? entry.mesh.material : [entry.mesh.material];
        materials.forEach(material => material.dispose());
      } else if (entry.mesh.userData.tacticsCard) (entry.mesh.material as THREE.Material).dispose();
      if (entry.shadowMesh) this.shadowCasterGroup.remove(entry.shadowMesh);
      if (entry.contactMesh) this.decorContactGroup.remove(entry.contactMesh);
      if (entry.proxy) this.shadowCasterGroup.remove(entry.proxy);
      if (entry.fogCut) this.decorGroup.remove(entry.fogCut);
      if (entry.halo) {
        this.flameHaloGroup.remove(entry.halo);
        (entry.halo.material as THREE.Material).dispose();
      }
    }
    this.decorEntries = [];
    this.builtDecorKey = key;
    const architectureCells = new Set(engine.decorations.filter(p => DECORATIONS[p.id]?.model3d && !DECORATIONS[p.id]?.rockStyle && !DECORATIONS[p.id]?.treeModel && !DECORATIONS[p.id]?.propModel).flatMap(p =>
      p.id === "watchtower-stone-open-door-2hex" ? placedFootprint(p).map(f => `${p.x + f.dx},${p.y + f.dy}`) : [`${p.x},${p.y}`]));

    for (const p of engine.decorations) {
      const def = wallDefinition(p.id);
      if (!def) continue;
      if ((def.treeModel || def.propModel === "grey-outcrop" || def.propModel?.startsWith("tavern-") || def.propModel?.startsWith("vau-") || def.propModel?.startsWith("watchtower-")) && engine.tacticsCamera) {
        const mesh = this.trees.create(def.treeModel ?? (def.propModel as "grey-outcrop" | "tavern-barrel" | "tavern-chair" | "tavern-candlestick" | "tavern-mug" | "tavern-table" | "tavern-table-small-v01" | "tavern-table-messhall-v01" | "tavern-chair-v04" | "vau-gothic-cottage-v01" | "vau-well-v01" | "watchtower-signal-brazier-v01" | "watchtower-arms-rack-v01" | "watchtower-command-table-v01" | "watchtower-signal-brazier-v02" | "watchtower-arms-rack-v02" | "watchtower-command-table-v02"), tile);
        if (mesh) {
          const { wx, wy } = this.boardWorld(p.x, p.y, tile);
          mesh.position.set(wx, -wy, engine.tacticsCamera ? this.groundHeight(p.x, p.y, tile) : this.groundHeight(p.x, p.y, tile) + 1);
          mesh.rotation.z = -(p.rot ?? 0) * Math.PI / 2;
          mesh.layers.enable(PROXY_LAYER);
          this.decorGroup.add(mesh);
          this.decorEntries.push({ mesh, placement: p, shadowMesh: null, contactMesh: null,
            proxy: null, light: null, halo: null, fogCut: null });
        }
        continue;
      }
      if (def.model3d && !def.treeModel && !def.propModel) {
        const cells = placedFootprint(p);
        const centerX = p.x + cells.reduce((sum, cell) => sum + cell.dx, 0) / cells.length;
        const centerY = p.y + cells.reduce((sum, cell) => sum + cell.dy, 0) / cells.length;
        const { wx, wy } = architectureWorld(centerX, centerY, tile);
        // Connect every occupied neighboring architecture cell. Orientation determines
        // the door opening axis; it must not prevent perpendicular wall corners from joining.
        const connections = [{ x: p.x - 1, y: p.y }, { x: p.x + 1, y: p.y }, { x: p.x, y: p.y - 1 }, { x: p.x, y: p.y + 1 }].filter(n => architectureCells.has(`${n.x},${n.y}`)).map(n => {
          const neighbor = architectureWorld(n.x, n.y, tile);
          return { x: neighbor.wx - wx, y: wy - neighbor.wy };
        });
        // Convert the overhead view's full 3.5x height projection into physical height,
        // so upright characters clear doorway lintels in tactics mode.
        const spatialDef = engine.tacticsCamera ? { ...def, heightScale: (def.heightScale ?? 1) * 3.5 } : def;
        const geometry = createWallGeometry(spatialDef, tile, p.rot ?? 0, connections, { x: wx, y: -wy }, !engine.tacticsCamera, engine.tacticsCamera ? 3.5 : 1);
        let map: THREE.Texture | undefined;
        if (def.wallTexture) {
          map = this.wallTextures.get(def.wallTexture);
          if (!map) {
            map = new THREE.TextureLoader().load(def.wallTexture);
            map.colorSpace = THREE.SRGBColorSpace;
            map.wrapS = map.wrapT = THREE.RepeatWrapping;
            map.magFilter = THREE.LinearFilter;
            map.minFilter = THREE.LinearMipmapLinearFilter;
            map.generateMipmaps = true;
            map.anisotropy = Math.min(16, this.renderer.capabilities.getMaxAnisotropy());
            this.wallTextures.set(def.wallTexture, map);
          }
        }
        const metalDoor = def.doorStyle === "iron" || def.doorStyle === "steel";
        const architectureColor = metalDoor ? (def.doorStyle === "iron" ? 0x555b60 : 0x9ca7b0)
          : def.model3d === "door" ? 0x925b32 : def.model3d === "doorway" ? 0xb7a27c : 0x8b8b86;
        const material = new THREE.MeshStandardMaterial({ map, color: map ? 0xffffff : architectureColor,
          roughness: metalDoor ? 0.52 : 0.94, metalness: metalDoor ? 0.5 : 0, flatShading: true });
        if (!engine.tacticsCamera) configureWallDepth(material, tile, DEPTH_Z_BASE, DEPTH_Z_PER_TILE);
        else {
          material.transparent = true;
          lightTacticsMaterial(material);
        }
        const mesh = new THREE.Mesh(geometry, material);
        mesh.position.set(wx, -wy, engine.tacticsCamera ? this.groundHeight(p.x, p.y, tile) : this.groundHeight(p.x, p.y, tile) + 1);
        mesh.castShadow = false;
        mesh.receiveShadow = true;
        mesh.layers.enable(PROXY_LAYER);
        this.wallGroup.add(mesh);
        // Camera projection stretches the visible walls; lights need the upright volume.
        const shadowGeometry = createWallGeometry(spatialDef, tile, p.rot ?? 0, connections, { x: wx, y: -wy }, false, engine.tacticsCamera ? 3.5 : 1);
        const shadowMaterial = new THREE.MeshBasicMaterial({ colorWrite: false, depthWrite: false });
        const shadowMesh = new THREE.Mesh(shadowGeometry, shadowMaterial);
        shadowMesh.position.set(wx, -wy, this.groundHeight(p.x, p.y, tile));
        shadowMesh.castShadow = true;
        shadowMesh.layers.enable(PROXY_LAYER);
        this.wallGroup.add(shadowMesh);
        this.wallEntries.push({ mesh, shadowMesh, placement: p });
        continue;
      }
      const decorLayer = def.unitLayer ?? (def.foreground ? "front" : "ground");
      // Fog 2 deliberately sits above every decoration but below units. Front props therefore
      // belong in this same Three layer too; keeping them on the top 2D unit canvas would make
      // them unavoidably render over the fog regardless of their world Z.

      if (engine.tacticsCamera) {
        const { w, h } = decorSize(p.id, def, tile);
        const model = tacticsProp(def.propModel ?? p.id, w, h,
          this.tacticsTexture("/game/textures/walls/cave-v2.png"),
          this.tacticsTexture("/game/textures/doors/reinforced-wood-door.png"));
        if (model) {
          const cells = placedFootprint(p).map(f => this.boardWorld(p.x + f.dx, p.y + f.dy, tile));
          model.position.set(cells.reduce((sum, c) => sum + c.wx, 0) / cells.length,
            -cells.reduce((sum, c) => sum + c.wy, 0) / cells.length, this.groundHeight(p.x, p.y, tile));
          model.rotation.z = -(p.rot ?? 0) * Math.PI / 3;
          model.layers.enable(PROXY_LAYER);
          this.decorGroup.add(model);
          this.decorEntries.push({ mesh: model, placement: p, shadowMesh: null, contactMesh: null,
            proxy: null, light: null, halo: null, fogCut: null });
          continue;
        }
      }

      const alternateMirror = !!def.mirrorAlternate && p.x >= engine.cols / 2;
      // Posts on opposite halves of the map use baked mirrored sprites, so paired props read
      // symmetrically across the vertical centerline. Other facings retain decorationFacing's fallback.
      const facingRot = ((p.rot ?? 0) + (alternateMirror ? 3 : 0)) % 6;
      const artId = decorationPlacementArt(p);
      const facing = decorationFacing(artId, facingRot, (file) => this.decorImageReady(file) !== null);
      const facingMirror = facing.mirror;
      const fileId = facing.own ? facing.file : artId;
      const img = this.decorImageReady(fileId);
      if (!img) continue; // art still loading — picked up on the next ensureDecorBuilt (see decorImageReady)

      let sumWx = 0;
      let sumWy = 0;
      let frontWy = -Infinity;
      for (const { dx, dy } of placedFootprint(p)) {
        const { wx, wy } = this.boardWorld(p.x + dx, p.y + dy, tile);
        sumWx += wx;
        sumWy += wy;
        frontWy = Math.max(frontWy, wy);
      }
      const n = def.footprint.length;
      const { w, h, dy: liftY } = decorSize(p.id, def, tile);
      const wx = sumWx / n;
      const groundWy = sumWy / n; // ground contact, before decorSize's liftY visual offset
      const wy = groundWy + liftY;
      const alphaBase = artBase(img);
      let offsetX = (alphaBase ? (0.5 - (alphaBase.u0 + alphaBase.u1) / 2) : 0) * w + (def.artOffsetX ?? 0) * w;
      let offsetY = alphaBase ? (1 - alphaBase.v) * h : 0;
      if (facingMirror) offsetX = -offsetX;
      else if (!facing.own && facing.step && !(engine.tacticsCamera && BARRICADE_LIKE_DECOR.has(p.id))) {
        const angle = (facing.step * Math.PI) / 3;
        [offsetX, offsetY] = [offsetX * Math.cos(angle) - offsetY * Math.sin(angle), offsetX * Math.sin(angle) + offsetY * Math.cos(angle)];
      }

      // Hoisted above the mesh so its renderOrder can already account for it — see
      // ABOVE_GROUND_MIST_ORDER's own comment.
      const lightDef = LIGHT_DEFS[p.id];
      const cachedMat = this.decorMaterialFor(fileId, img);
      const mat = engine.tacticsCamera ? cachedMat.clone() : cachedMat;
      const mesh = new THREE.Mesh(this.quadGeo, mat);
      if (engine.tacticsCamera) {
        mesh.userData.tacticsCard = true;
        mesh.userData.tacticsPlacementId = p.id;
        mesh.userData.sourceMaterial = cachedMat;
        mat.alphaTest = 0.08;
        mat.onBeforeCompile = cachedMat.onBeforeCompile;
        mat.customProgramCacheKey = cachedMat.customProgramCacheKey;
      }
      // Front-layer props render after character billboards (order 2), matching the Canvas
      // renderer; their per-decoration priority resolves overlaps with other foreground props.
      // A light prop explicitly placed behind the characters (the fireplace) stays in the base
      // decoration layer like any other background prop, instead of lifting above them.
      const mistOrder = def.aboveGroundMist ? ABOVE_FOG_SCENERY_ORDER : (lightDef && decorLayer !== "behind") ? ABOVE_GROUND_MIST_ORDER : 0;
      const tacticalOverlayOrder = def.aboveTacticalOverlays ? ABOVE_GROUND_MIST_ORDER + 1 : 0;
      mesh.renderOrder = Math.max(tacticalOverlayOrder, mistOrder + (decorLayer === "front" ? 3 : 0) + (def.decorRenderOrder ?? 0) * 0.01);
      // Y negated to match the tile/camera convention (see module comment). Z is the prop's
      // 2.5D depth (see DEPTH_Z_BASE): from its front-most row, so a character on a nearer row
      // covers it and one further back is covered — except explicit layers and flat Waypoints.
      const pinnedBehind = decorLayer === "behind" || !!def.exitKind;
      const depthZ = pinnedBehind ? DEPTH_Z_BEHIND : decorLayer === "front" ? DEPTH_Z_FRONT : spriteDepthZ(frontWy, tile);
      mesh.position.set(wx + offsetX, -wy - offsetY, depthZ);
      if (!engine.tacticsCamera && !pinnedBehind && decorLayer !== "front") mesh.add(new THREE.Mesh(this.quadGeo, this.decorOccluderMaterialFor(fileId, mat)));
      if (facing.step === 0) {
        mesh.scale.set(w, h, 1);
      } else if (facing.own) {
        // A prop with its own per-side art mirrors instead of rotating — see
        // decorationFacing's own comment for why (a mirrored drawing still faces outward
        // correctly; a rotated one would tilt the art instead of turning which side faces
        // the viewer).
        mesh.scale.set(facingMirror ? -w : w, h, 1);
      } else {
        // No dedicated side art: fall back to spinning the bitmap (a placeholder, same as
        // Canvas2D's own fallback) — negated for the same reason tile rotation is (see
        // ensureBuilt's comment).
          mesh.scale.set(w, h, 1);
        mesh.rotation.z = (-facing.step * Math.PI) / 3;
      }
      if (engine.tacticsCamera) {
        const flat = pinnedBehind || /rubble|ruins|corpse|bones|carpet|bridge|web/.test(p.id);
        const fixed = BARRICADE_LIKE_DECOR.has(p.id);
        if (fixed) {
          // Barricades stand in the authored world direction instead of swivelling
          // toward the camera independently, which breaks a continuous fence line.
          mesh.quaternion.setFromAxisAngle(new THREE.Vector3(0, 0, 1), -(p.rot ?? 0) * Math.PI / 3)
            .multiply(new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(1, 0, 0), Math.PI / 2));
          mat.side = THREE.DoubleSide;
          // The fixed panel is real world geometry: cast its own alpha silhouette
          // so its shadow cannot drift, shorten, or rotate independently of the art.
          mesh.castShadow = !def.noShadow;
          mesh.layers.enable(PROXY_LAYER);
        }
        mesh.userData.tacticsAnchor = { x: wx, y: -groundWy, z: this.groundHeight(p.x, p.y, tile),
          offsetX, center: (alphaBase ? alphaBase.v - 0.5 : 0.5) * h, flat, fixed };
        mesh.renderOrder = 0;
      }
      this.decorGroup.add(mesh);

      // True-silhouette shadow caster (see decorShadowMaterialFor's own comment): the prop's own
      // cutout art, positioned at ground contact (not wy, which already includes decorSize's
      // liftY visual nudge) so the shadow lands where the prop actually stands. Same width/mirror/
      // facing-spin branches as the visible mesh above so the cast silhouette matches what's on
      // screen — but standing vertically (rotation.x), NOT flat like the visible mesh: a flat
      // plane has no depth, so it sits entirely at one fixed height with nothing touching the
      // ground, making its whole shadow float free of the prop (confirmed empirically — "mega
      // Peter Pan" on the equivalent unit version of this bug). Rotating it up turns local Y
      // (image-space up/down) into world Z, so scale.y=elevation + position.z=elevation/2 puts its
      // BASE exactly at the ground (z=0) at the prop's contact point and its top at `elevation`,
      // same span the old box caster used. Skipped entirely for a DecorationDef.noShadow prop —
      // e.g. Parapeito, where a thin tall railing's cast shadow read as a wrong dark stripe
      // across the board rather than grounding the prop.
      const elevation = Math.max(1, h * DECOR_SHADOW_HEIGHT_SCALE);
      let shadowMesh: THREE.Mesh | null = null;
      if (!def.noShadow && !mesh.userData.tacticsAnchor?.fixed) {
        const shadowMat = this.decorShadowMaterialFor(fileId, mat);
        shadowMesh = new THREE.Mesh(this.quadGeo, shadowMat);
        shadowMesh.castShadow = true;
        shadowMesh.position.set(wx + offsetX, -(groundWy + offsetY), elevation / 2 - DECOR_SHADOW_GROUND_INSET / 2);
        if (facing.step === 0) {
          shadowMesh.rotation.x = Math.PI / 2;
          shadowMesh.scale.set(w, elevation + DECOR_SHADOW_GROUND_INSET, 1);
        } else if (facing.own) {
          shadowMesh.rotation.x = Math.PI / 2;
          shadowMesh.scale.set(facingMirror ? -w : w, elevation + DECOR_SHADOW_GROUND_INSET, 1);
        } else {
          shadowMesh.rotation.set(Math.PI / 2, 0, (-facing.step * Math.PI) / 3);
          shadowMesh.scale.set(w, elevation + DECOR_SHADOW_GROUND_INSET, 1);
        }
        this.shadowCasterGroup.add(shadowMesh);
      }

      // Contact decal at the art's own opaque base (visible-mesh space: centered at wx,wy,
      // spanning w x h). Skipped for the spun-bitmap facing fallback — its "bottom" isn't the
      // ground side any more.
      let contactMesh: THREE.Mesh | null = null;
      const base = facing.step !== 0 && !facing.own ? null : artBase(img);
      if (base) {
        const sign = facingMirror ? -1 : 1;
        const cw = (base.u1 - base.u0) * w * CONTACT_SHADOW_W;
        contactMesh = new THREE.Mesh(this.quadGeo, this.decorContactMaterial);
        const ch = Math.min(cw * CONTACT_SHADOW_H, tile * CONTACT_SHADOW_MAX_H);
        contactMesh.position.set(wx + offsetX + sign * ((base.u0 + base.u1) / 2 - 0.5) * w, -(wy + offsetY - h / 2 + base.v * h + ch * CONTACT_SHADOW_FORWARD), CONTACT_SHADOW_Z);
        contactMesh.scale.set(cw, ch, 1);
        this.decorContactGroup.add(contactMesh);
      }

      // Light-emitting prop: the light sits at the flame in its art, projected to the ground
      // under it (x, groundWy) with the flame's real height above that ground.
      let light: DecorMeshEntry["light"] = null;
      if (lightDef) {
        const f = artFlame(img);
        const sign = facingMirror ? -1 : 1;
        const flameY = wy + offsetY - h / 2 + f.v * h;
        light = { x: wx + offsetX + sign * (f.u - 0.5) * w, y: groundWy + offsetY, h: Math.max(0, groundWy + offsetY - flameY), def: lightDef, seed: (p.x * 7.31 + p.y * 3.17) % 6.28 };
      }
      let halo: THREE.Mesh | null = null;
      if (light) {
        // Depth-tested just below every sprite (DEPTH_Z_BEHIND), so the solid body of a character
        // or prop in front of it hides the glow — it lights the air around them, never washes
        // over them.
        const haloMat = new THREE.MeshBasicMaterial({ map: this.flameHaloTexture, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, opacity: 0 });
        haloMat.color.setRGB(light.def.color[0], light.def.color[1], light.def.color[2]);
        halo = new THREE.Mesh(this.quadGeo, haloMat);
        const size = 2 * light.def.radius * HALO_RADIUS_FRACTION * tile;
        halo.scale.set(size, size, 1);
        halo.position.set(light.x, -(light.y - light.h), DEPTH_Z_BEHIND - 0.02);
        halo.renderOrder = 3;
        this.flameHaloGroup.add(halo);
      }

      // Hidden physical volume: a box standing on the prop's ground spot, as wide as its
      // opaque base, as tall as the shadow elevation, reaching back ("north", +Y) from the
      // contact line. Light-source props get none — their flame sits inside their own volume.
      // A noShadow prop gets none either — this volume exists only to cast/block shadows.
      let proxy: THREE.Mesh | null = null;
      if (!lightDef && !def.noShadow && !mesh.userData.tacticsAnchor?.fixed) {
        const pb = artBase(img);
        const sign = facing.own && facing.mirror ? -1 : 1;
        const bw = pb ? Math.max(tile * 0.3, (pb.u1 - pb.u0) * w) : w * 0.6;
        const depth = Math.min(bw, tile * 1.6) * 0.6;
        const bx = pb ? wx + offsetX + sign * ((pb.u0 + pb.u1) / 2 - 0.5) * w : wx + offsetX;
        proxy = new THREE.Mesh(this.proxyBox, this.proxyMaterial);
        proxy.layers.set(PROXY_LAYER);
        proxy.castShadow = true;
        proxy.scale.set(bw, depth, elevation);
        proxy.position.set(bx, -(groundWy + offsetY) + depth / 2, elevation / 2);
        this.shadowCasterGroup.add(proxy);
      }

      // Same art, position and facing as the visible mesh, but writes depth only (see
      // decorFogCutMaterialFor); the fog sheet depth-tests against it (ThreeFogMask).
      let fogCut: THREE.Mesh | null = null;
      if (HOUSE_DECOR_IDS.has(p.id) || BIG_HOUSE_DECOR_IDS.has(p.id) || SOLID_HOUSE_DECOR_IDS.has(p.id)) {
        fogCut = new THREE.Mesh(this.quadGeo, this.decorFogCutMaterialFor(fileId, mat));
        fogCut.renderOrder = 99;
        fogCut.position.set(wx + offsetX, -wy - offsetY, 60);
        fogCut.scale.copy(mesh.scale);
        fogCut.rotation.copy(mesh.rotation);
        this.decorGroup.add(fogCut);
      }

      this.decorEntries.push({ mesh, placement: p, shadowMesh, contactMesh, light, halo, proxy, fogCut });
    }
  }

  private unitContactMasks = new Map<HTMLImageElement, { texture: THREE.Texture; top: number; bottom: number }>();
  private unitContactMask(img: HTMLImageElement): { texture: THREE.Texture; top: number; bottom: number } {
    const cached = this.unitContactMasks.get(img);
    if (cached) return cached;
    const width = img.naturalWidth, height = img.naturalHeight;
    const footRow = Math.ceil((artBase(img)?.v ?? 1) * height);
    const bottom = footRow + Math.ceil(height * 0.06);
    const top = Math.max(0, footRow - Math.ceil(height * 0.08));
    const canvas = document.createElement("canvas"); canvas.width = width; canvas.height = bottom - top;
    const ctx = canvas.getContext("2d")!;
    ctx.drawImage(img, 0, -top);
    const pixels = ctx.getImageData(0, 0, width, bottom - top), original = pixels.data.slice();
    // Extend each opaque foot column to the support edge. A raised painted boot
    // still contacts the ground; empty columns between feet remain empty.
    for (let x = 0; x < width; x++) {
      let supported = false;
      for (let y = 0; y < canvas.height; y++) {
        const i = (y * width + x) * 4;
        supported ||= original[i + 3] > 128;
        pixels.data[i] = pixels.data[i + 1] = pixels.data[i + 2] = 255;
        pixels.data[i + 3] = supported ? 255 : 0;
      }
    }
    ctx.putImageData(pixels, 0, 0);
    const texture = new THREE.CanvasTexture(canvas); texture.minFilter = texture.magFilter = THREE.LinearFilter;
    const result = { texture, top: top / height, bottom: bottom / height };
    this.unitContactMasks.set(img, result); return result;
  }

  private unitFootShadowTextures = new Map<HTMLImageElement, THREE.Texture>();

  /** Shadow-only foot support: extend opaque columns in the bottom foot band to
   * its lowest row. The visible sprite remains untouched; gaps between legs remain clear. */
  private unitFootShadowTexture(img: HTMLImageElement): THREE.Texture {
    const cached = this.unitFootShadowTextures.get(img);
    if (cached) return cached;
    const canvas = document.createElement("canvas");
    canvas.width = img.naturalWidth; canvas.height = img.naturalHeight;
    const context = canvas.getContext("2d", { willReadFrequently: true })!;
    context.drawImage(img, 0, 0);
    const pixels = context.getImageData(0, 0, canvas.width, canvas.height);
    const { width, height, data } = pixels;
    let bottom = -1;
    for (let y = height - 1; y >= 0 && bottom < 0; y--)
      for (let x = 0; x < width; x++) if (data[(y * width + x) * 4 + 3] > 128) { bottom = y; break; }
    const bandTop = Math.max(0, bottom - Math.round(height * 0.12));
    for (let x = 0; x < width; x++) {
      let foot = -1;
      for (let y = bottom; y >= bandTop; y--) if (data[(y * width + x) * 4 + 3] > 128) { foot = y; break; }
      if (foot < 0) continue;
      for (let y = foot + 1; y <= bottom; y++) data[(y * width + x) * 4 + 3] = 255;
    }
    context.putImageData(pixels, 0, 0);
    const texture = new THREE.CanvasTexture(canvas);
    texture.minFilter = texture.magFilter = THREE.LinearFilter;
    this.unitFootShadowTextures.set(img, texture);
    return texture;
  }

  private unitTextureFor(img: HTMLImageElement): THREE.Texture {
    const hit = this.unitTexCache.get(img);
    if (hit) return hit;
    const tex = new THREE.Texture(img);
    tex.needsUpdate = true;
    tex.colorSpace = THREE.SRGBColorSpace;
    tex.minFilter = THREE.LinearFilter;
    tex.magFilter = THREE.LinearFilter;
    tex.wrapS = THREE.ClampToEdgeWrapping;
    tex.wrapT = THREE.ClampToEdgeWrapping;
    this.unitTexCache.set(img, tex);
    return tex;
  }

  /** The sprite's silhouette in white, blurred, on a canvas GLOW_PAD times the image's size
   * (same center) — at half resolution, since it's a soft halo. */
  private glowTextureFor(img: HTMLImageElement): THREE.Texture {
    const hit = this.glowTexCache.get(img);
    if (hit) return hit;
    const w = Math.max(1, Math.round(img.naturalWidth * 0.5));
    const h = Math.max(1, Math.round(img.naturalHeight * 0.5));
    const sil = document.createElement("canvas");
    sil.width = w;
    sil.height = h;
    const sc = sil.getContext("2d")!;
    sc.drawImage(img, 0, 0, w, h);
    sc.globalCompositeOperation = "source-in";
    sc.fillStyle = "#fff";
    sc.fillRect(0, 0, w, h);
    const out = document.createElement("canvas");
    out.width = Math.round(w * GLOW_PAD);
    out.height = Math.round(h * GLOW_PAD);
    const oc = out.getContext("2d")!;
    oc.filter = `blur(${Math.max(2, w * 0.12)}px)`;
    oc.drawImage(sil, (out.width - w) / 2, (out.height - h) / 2);
    oc.drawImage(sil, (out.width - w) / 2, (out.height - h) / 2);
    const tex = new THREE.CanvasTexture(out);
    tex.colorSpace = THREE.SRGBColorSpace;
    this.glowTexCache.set(img, tex);
    return tex;
  }

  /** Full pose/animation parity with BattleEngine's own Canvas2D draw loop — walk/attack/cast/
   * counter poses, every pose-specific size correction, live idle motion (bob/sway/breath) and
   * high-ground lift, all computed once by BattleEngine.unitVisual (see that method's own
   * comment) so this renderer can never drift out of sync with the Canvas2D-shim path it
   * replaces. Position comes from unitAnchor, the public world-space twin of the private
   * unitPixel/footprintCentroid the Canvas2D path actually draws with — including their
   * front-row-footprint averaging for boss/multi-hex units and their mid-move easing, so this
   * renderer's units track the exact same position the hit boxes and combat math use, not an
   * approximation. */
  private syncUnits(tile: number): void {
    const engine = this.engine;
    const seen = new Set<string>();

    for (const u of engine.units) {
      if (u.fade <= 0 || engine.unitHidden(u)) continue;
      const v = engine.unitVisual(u, tile);
      const img = v.img;
      if (!img || img.naturalWidth === 0) continue; // art still loading — picked up next frame

      seen.add(u.id);
      let entry = this.unitEntries.get(u.id);
      if (!entry) {
        // Lit, like decorations — real scene lights illuminate the character (see
        // syncSpriteExposure).
        // Units mirror by applying a negative X scale (see unitVisual().scaleX). A
        // FrontSide plane is culled after that reflection, so characters such as Kael
        // could not turn toward targets on their left in the 3D renderer.
        const material = new THREE.MeshLambertMaterial({ map: this.unitTextureFor(img), transparent: true, depthWrite: false, side: THREE.DoubleSide });
        const lightCap = { value: SPRITE_LIGHT_CAP };
        capSpriteLight(material, lightCap);
        this.litSpriteMats.add(material);
        this.unitSpriteMats.add(material);
        const mesh = new THREE.Mesh(this.quadGeo, material);
        // Atmosphere's Fog 2 sheets use renderOrder 1: units must remain the final visible
        // sprite layer (2), while decorations remain the base layer (0).
        mesh.renderOrder = 2;
        this.unitGroup.add(mesh);
        // True-silhouette shadow caster: the unit's own sprite art, alpha-tested instead of
        // alpha-blended (colorWrite off — invisible in the normal color pass, same trick the old
        // box caster used) so the shadow map sees this unit's real cutout shape, not a box.
        // Real elevation (see UNIT_SHADOW_HEIGHT_SCALE's comment), repositioned every frame below
        // alongside the visible sprite, same scale as it so the cast silhouette actually matches.
        // side: DoubleSide — see decorShadowMaterialFor's identical comment: a flat plane needs
        // this to cast any shadow at all, since Three's shadow pass renders back-faces by default.
        const shadowMaterial = new THREE.MeshBasicMaterial({ map: this.unitFootShadowTexture(img), alphaTest: 0.5, colorWrite: false, depthWrite: false, side: THREE.DoubleSide });
        const shadowMesh = new THREE.Mesh(this.quadGeo, shadowMaterial);
        shadowMesh.castShadow = true;
        this.shadowCasterGroup.add(shadowMesh);
        const contactMaterial = makeContactShadowMaterial(this.contactShadowTexture);
        const contactMesh = new THREE.Mesh(this.quadGeo, contactMaterial);
        contactMesh.receiveShadow = true;
        this.contactShadowGroup.add(contactMesh);
        const proxy = new THREE.Mesh(this.proxyCylinder, this.proxyMaterial);
        proxy.layers.set(PROXY_LAYER);
        proxy.castShadow = true;
        this.shadowCasterGroup.add(proxy);
        const glowMaterial = new THREE.MeshBasicMaterial({ transparent: true, depthWrite: false, blending: THREE.AdditiveBlending });
        const glowMesh = new THREE.Mesh(this.quadGeo, glowMaterial);
        glowMesh.renderOrder = 2;
        glowMesh.visible = false;
        this.unitGroup.add(glowMesh);
        const occluder = new THREE.Mesh(this.quadGeo, this.unitOccluderMaterialFor(this.unitTextureFor(img)));
        mesh.add(occluder);
        const fogCut = new THREE.Mesh(this.quadGeo, this.unitFogCutMaterialFor(this.unitTextureFor(img)));
        fogCut.renderOrder = 99;
        this.unitGroup.add(fogCut);
        entry = { mesh, glowMesh, glowMaterial, material, lightCap, occluder, img: null, shadowMesh, shadowMaterial, contactMesh, contactMaterial, contactFit: null, proxy, fogCut };
        this.unitEntries.set(u.id, entry);
      }
      entry.mesh.visible = true;
      if (entry.img !== img) {
        entry.material.map = this.unitTextureFor(img);
        entry.material.needsUpdate = true;
        entry.shadowMaterial.map = this.unitFootShadowTexture(img);
        entry.shadowMaterial.needsUpdate = true;
        entry.occluder.material = this.unitOccluderMaterialFor(this.unitTextureFor(img));
        entry.fogCut.material = this.unitFogCutMaterialFor(this.unitTextureFor(img));
        entry.img = img;
      }
      // Same fade-in/out and post-action player-unit dimming as
      // renderUnitsAndOverlays' ctx.globalAlpha — real per-unit opacity, not shared, since
      // entry.material is this unit's own instance (see unitTexCache's comment).
      entry.material.opacity = u.fade * (u.moved && u.side === "player" && engine.phase === "player" && !engine.isAnimating() ? 0.9 : 1);

      const anchor = engine.unitAnchor(u);
      const previousDraw = this.unitDrawPositions.get(u.id);
      if (previousDraw && (previousDraw.x !== u.drawX || previousDraw.y !== u.drawY)) engine.noteUnitDrawAction(u.id);
      this.unitDrawPositions.set(u.id, { x: u.drawX, y: u.drawY });
      // Explicit transparent sorting prevents animation/camera depth from
      // repeatedly swapping two overlapping large creature cards.
      const actionOrder = engine.unitActionOrder.get(u.id) ?? 0;
      entry.mesh.renderOrder = footprint(u).length > 1 ? 2 + (actionOrder + 1) / (actionOrder + 2) * 0.001 : 2;
      entry.glowMesh.renderOrder = entry.mesh.renderOrder;
      const footX = anchor.worldX + this.cameraRight.x*v.sway + this.cameraGroundDown.x*v.footY;
      const footY = -anchor.worldY + this.cameraRight.y*v.sway + this.cameraGroundDown.y*v.footY;
      const groundLift = this.landscape?.heightAt(footX, footY) ?? v.lift;
      // Canvas2D draws the image at local Y in [-h+footOffset, footOffset] (Y-down, relative
      // to the translated anchor+sway/footY/bob/lift origin), then ctx.scale(scaleX, scaleY)
      // stretches that box AWAY FROM the origin (y=0), not around its own center — so the
      // quad's center has to be repositioned by the same scale, not just resized, to land in
      // the same place a plain "scale the mesh in place" would miss. Horizontal is symmetric
      // (image spans -w/2..w/2) so scaleX only ever needs to flip its sign for mirroring, same
      // pattern ensureDecorBuilt already uses for a decoration's own-art facing.
      const centerYLocal = (v.footOffset - v.h / 2) * v.scaleY * this.cameraSpriteScale;
      const base = artBase(img);
      const transparentFootPadding = (1 - (base?.v ?? 1)) * v.h * v.scaleY * this.cameraSpriteScale;
      const visualUpOffset = (engine.tacticsCamera ? -transparentFootPadding : groundLift) - v.bob - centerYLocal;
      // Y negated and Z derived from row — see module comment on the Y-flip and
      // ensureDecorBuilt's own comment on z ordering vs decorations (z=1) and tiles (z=0).
      // 2.5D depth from the ground line (see DEPTH_Z_BASE) — bob/lift are visual only.
      let unitDepth = spriteDepthZ(anchor.worldY + v.footY, tile) + UNIT_DEPTH_TIE;
      for (const wall of engine.tacticsCamera ? [] : this.wallEntries) {
        if (!wall.mesh.visible || DECORATIONS[wall.placement.id]?.model3d !== "doorway") continue;
        const vertical = ((wall.placement.rot ?? 0) & 1) !== 0;
        const dx = anchor.worldX - wall.mesh.position.x;
        const dy = anchor.worldY + wall.mesh.position.y;
        const halfWidth = tile * (vertical ? 1.5 : Math.sqrt(3)) / 2;
        // While crossing the opening, the whole sprite belongs behind the frame.
        // Ground-row sorting alone puts its upper body over the lintel/posts midway through.
        // Odd hex rows shift character anchors sideways relative to the aligned walls.
        const crossingDepth = tile * (vertical ? 0.95 : 0.45);
        if (Math.abs(vertical ? dy : dx) <= halfWidth + tile * 0.02 && Math.abs(vertical ? dx : dy) <= crossingDepth) {
          const backGroundY = -wall.mesh.position.y - (vertical ? halfWidth : tile * 0.16);
          unitDepth = Math.min(unitDepth, spriteDepthZ(backGroundY, tile) - UNIT_DEPTH_TIE);
          entry.material.opacity = u.fade;
        }
      }
      if (engine.tacticsCamera) unitDepth = groundLift + 0.02;
      entry.mesh.position.set(
        anchor.worldX + this.cameraRight.x * v.sway + this.cameraGroundDown.x * v.footY + this.unitUp.x * visualUpOffset,
        -anchor.worldY + this.cameraRight.y * v.sway + this.cameraGroundDown.y * v.footY + this.unitUp.y * visualUpOffset,
        unitDepth + this.unitUp.z * visualUpOffset,
      );
      // Keep the pixel art readable as the board turns: each character card faces the camera
      // while its feet stay anchored to the map. At the default overhead view this is the same
      // orientation as before; as the camera tilts, the cards no longer flatten with the tiles.
      entry.mesh.quaternion.copy(this.unitFacing);
      // A unit fading in/out is see-through, so it must not blot out what stands behind it.
      entry.occluder.visible = u.fade >= 0.999;
      entry.mesh.scale.set(v.scaleX * v.w * this.cameraSpriteScale, v.scaleY * v.h * this.cameraSpriteScale, 1);
      // Fixed z=60: above the fog sheet's z=50 (see FogMask), same height as a house's fogCut,
      // so the fog-of-war sheet always fails its depth test over this unit, whatever row it's on.
      entry.fogCut.position.copy(entry.mesh.position);
      entry.fogCut.position.z = 60;
      entry.fogCut.quaternion.copy(this.unitFacing);
      entry.fogCut.scale.copy(entry.mesh.scale);
      entry.fogCut.visible = u.fade >= 0.999;

      // Hit flash: Canvas2D's ctx.filter brightness(1.8 + flash) on the sprite, as a multiplier
      // on this unit's lit material (on top of syncSpriteExposure's base color, set earlier this
      // frame). brightness() works on gamma-encoded color; the material color is linear, hence
      // the 2.2 power.
      const flashMul = u.flash > 0 ? Math.pow(1.8 + u.flash, 2.2) : 1;
      if (u.flash > 0) entry.material.color.multiplyScalar(flashMul);
      // Familiars sit right under their own light, which washed their colors out: they are kept
      // at their own art's brightness (SELF_LIT_UNIT_CAP) while the light still reaches
      // everything around them. The hit flash still lifts them.
      entry.lightCap.value = (SELF_LIT_UNITS.has(u.classId) ? SELF_LIT_UNIT_CAP : this.currentSpriteLightCap) * flashMul;
      // Level-up / heal rim glow (Canvas2D: a shadowBlur pass of the sprite in the glow color).
      let glowRgb: string | null = null;
      let glowK = 0;
      if (u.levelGlow > 0) {
        glowRgb = "255,208,110";
        glowK = 0.95 * u.levelGlow * (0.75 + Math.sin(engine.time * 7) * 0.25);
      }
      if (u.healGlow > 0) {
        const k = 0.88 * u.healGlow * (0.8 + Math.sin(engine.time * 5) * 0.2);
        if (k > glowK) {
          glowRgb = engine.healHaloRgb(u.healGlowKind).core;
          glowK = k;
        }
      }
      if (glowRgb && glowK > 0.01) {
        const [r, g, b] = glowRgb.split(",").map((c) => Number(c) / 255);
        const glowTex = this.glowTextureFor(img);
        if (entry.glowMaterial.map !== glowTex) {
          entry.glowMaterial.map = glowTex;
          entry.glowMaterial.needsUpdate = true;
        }
        entry.glowMaterial.color.setRGB(r!, g!, b!, THREE.SRGBColorSpace);
        entry.glowMaterial.opacity = Math.min(1, glowK * 1.3) * u.fade;
        entry.glowMesh.position.copy(entry.mesh.position);
        entry.glowMesh.position.z -= 0.0005;
        entry.glowMesh.quaternion.copy(this.unitFacing);
        entry.glowMesh.scale.set(v.scaleX * v.w * this.cameraSpriteScale * GLOW_PAD, v.scaleY * v.h * this.cameraSpriteScale * GLOW_PAD, 1);
        entry.glowMesh.visible = true;
      } else entry.glowMesh.visible = false;

      // Shadow caster tracks the sprite's ground-contact point (anchor + footY, ignoring bob/lift
      // so a mid-step/high-ground unit's shadow stays anchored to the real ground instead of
      // floating with the visual lift trick — see decorSize's groundWy for the same idea applied
      // to props). Elevation grows a little with lift, echoing the fake shadow's own "raised =
      // slightly longer shadow" stretch (see engine.ts's shadowDirX/Y block) without trying to
      // match it exactly.
      // Standing vertically (rotation.x), NOT flat like the visible mesh — a flat plane has no
      // depth, so it would sit entirely at one fixed height with nothing touching the ground,
      // making its whole shadow float free of the character (confirmed: this was tried first and
      // looked badly detached, "mega Peter Pan"). Rotating it up turns local Y (image-space
      // up/down) into world Z, so scale.y=elevation + position.z=elevation/2 puts its BASE
      // exactly at the ground (z=0) at the foot anchor and its top at `elevation`, same span the
      // old box caster used — except the caster is now the real silhouette, not a box.
      const elevation = Math.max(1, v.h * this.cameraSpriteScale * UNIT_SHADOW_HEIGHT_SCALE + v.lift * 0.6);
      // Euler XYZ rotates the card's horizontal axis into Z when yaw is nonzero,
      // lifting one boot (especially Aldric's off-center stance). Yaw around world Z
      // after standing the card upright, so the entire foot row remains level.
      entry.shadowMesh.quaternion.setFromAxisAngle(SHADOW_UP_AXIS, THREE.MathUtils.degToRad(this.engine.cameraTiltSide))
        .multiply(STANDING_SHADOW_CARD);
      (entry.shadowMesh.userData.baseQuat ??= new THREE.Quaternion()).copy(entry.shadowMesh.quaternion);
      // Sprite frames contain transparent padding below their actual feet. Anchor the
      // opaque foot row, not the bottom of the image rectangle, to the receiver.
      const footRow = Math.max(0.1, base?.v ?? 1);
      const casterHeight = (elevation + UNIT_SHADOW_GROUND_INSET) / footRow;
      const casterCenter = (footRow - 0.5) * casterHeight - UNIT_SHADOW_GROUND_INSET;
      entry.shadowMesh.scale.set(v.scaleX * v.w * this.cameraSpriteScale, casterHeight, 1);
      const flatFootOffset = (v.footOffset - (1 - footRow) * v.h) * v.scaleY * this.cameraSpriteScale;
      entry.shadowMesh.position.set(anchor.worldX + v.sway, -(anchor.worldY + v.footY + (engine.tacticsCamera ? 0 : flatFootOffset)), casterCenter + (engine.tacticsCamera ? groundLift : 0));
      if (engine.tacticsCamera) {
        entry.shadowMesh.position.set(entry.mesh.position.x - this.unitUp.x * visualUpOffset,
          entry.mesh.position.y - this.unitUp.y * visualUpOffset,
          groundLift + casterCenter);
      }
      const contactLocal = (entry.shadowMesh.userData.contactLocal ??= new THREE.Vector3()) as THREE.Vector3;
      contactLocal.set(((base?.u0 ?? 0) + (base?.u1 ?? 1)) / 2 - 0.5, 0.5 - footRow, 0);
      (entry.shadowMesh.userData.contactAnchor ??= new THREE.Vector3()).copy(contactLocal).multiply(entry.shadowMesh.scale)
        .applyQuaternion(entry.shadowMesh.quaternion).add(entry.shadowMesh.position);
      entry.shadowMesh.visible = true;
      // Hidden upright cylinder at the feet: the character's physical body for point lights.
      const bodyR = Math.max(tile * 0.18, Math.abs(v.scaleX) * v.w * 0.22);
      entry.proxy.scale.set(bodyR * 2, bodyR * 2, elevation);
      entry.proxy.position.set(anchor.worldX + v.sway, -(anchor.worldY + v.footY) + bodyR * 0.5, elevation / 2 + (engine.tacticsCamera ? groundLift : 0));
      if (engine.tacticsCamera) entry.proxy.position.set(entry.shadowMesh.position.x,
        entry.shadowMesh.position.y, groundLift + elevation / 2);
      entry.proxy.visible = true;

      // Contact shadow: at this frame's opaque base (feet/paws — see artBase), in the same
      // local frame the sprite is drawn in (image spans y in [-h+footOffset, footOffset] before
      // scale), but from the ground origin (ignores bob/lift so it stays on the ground), fading
      // and shrinking as the unit lifts off it.
      const liftFade = engine.tacticsCamera ? 1 : Math.max(0, 1 - v.lift / Math.max(1, tile * 0.6));
      const footW = Math.abs(v.scaleX) * v.w * this.cameraSpriteScale;
      const target = base
        ? {
            dx: ((base.u0 + base.u1) / 2 - 0.5) * v.w * v.scaleX * this.cameraSpriteScale,
            dy: (-v.h + v.footOffset + base.v * v.h) * v.scaleY * this.cameraSpriteScale,
            w: Math.min(footW * CONTACT_SHADOW_MAX_W, (base.u1 - base.u0) * footW * CONTACT_SHADOW_W),
          }
        : { dx: 0, dy: 0, w: footW * 0.35 };
      const fit = entry.contactFit;
      if (!fit) entry.contactFit = target;
      else {
        // Width and sideways offset ease slowly: a walk cycle alternates feet-apart and
        // feet-together frames (measured: 20–42px on Neera, jumping 7px/frame at a 0.3 rate),
        // which read as the shadow pulsing. It follows the average stance instead; position
        // itself still tracks the unit exactly (anchor + fit, above).
        fit.dx += (target.dx - fit.dx) * 0.08;
        fit.dy += (target.dy - fit.dy) * 0.3;
        fit.w += (target.w - fit.w) * 0.08;
      }
      const cf = entry.contactFit!;
      if (engine.tacticsCamera) cf.dy = 0;
      const cw = cf.w * (0.7 + 0.3 * liftFade);
      const ch = Math.min(cw * CONTACT_SHADOW_H, tile * CONTACT_SHADOW_MAX_H);
      entry.contactMesh.position.set(anchor.worldX + v.sway + cf.dx, -(anchor.worldY + v.footY + cf.dy + ch * CONTACT_SHADOW_FORWARD), CONTACT_SHADOW_Z + (engine.tacticsCamera ? groundLift : 0));
      if (engine.tacticsCamera) entry.contactMesh.position.set(
        entry.shadowMesh.position.x + this.cameraRight.x * cf.dx + this.cameraGroundDown.x * ch * CONTACT_SHADOW_FORWARD,
        entry.shadowMesh.position.y + this.cameraRight.y * cf.dx + this.cameraGroundDown.y * ch * CONTACT_SHADOW_FORWARD,
        groundLift + CONTACT_SHADOW_Z);
      entry.contactMesh.scale.set(cw, ch, 1);
      if (engine.tacticsCamera && this.landscape) entry.contactMesh.position.z =
        this.landscape.heightAt(entry.contactMesh.position.x, entry.contactMesh.position.y) + CONTACT_SHADOW_Z;
      entry.contactMaterial.opacity = CONTACT_SHADOW_OPACITY * u.fade * liftFade;
      entry.contactMesh.visible = liftFade > 0;
      if (engine.tacticsCamera) {
        const contact = this.unitContactMask(img);
        if (entry.contactMaterial.map !== contact.texture) {
          entry.contactMaterial.map = contact.texture; entry.contactMaterial.needsUpdate = true;
        }
        // Project the actual visible boot band onto the receiver along the camera
        // ray. Its screen position therefore meets the painted boots exactly.
        const bandCenter = (0.5 - (contact.top + contact.bottom) / 2) * entry.mesh.scale.y;
        const center = entry.contactMesh.position.copy(entry.mesh.position).addScaledVector(this.unitUp, bandCenter);
        const floor = this.landscape?.heightAt(center.x, center.y) ?? groundLift;
        center.addScaledVector(this.cameraBack, -(center.z - floor) / Math.max(0.1, this.cameraBack.z));
        center.z = (this.landscape?.heightAt(center.x, center.y) ?? floor) + CONTACT_SHADOW_Z;
        const projectedUp = this.unitUp.clone().addScaledVector(this.cameraBack, -this.unitUp.z / Math.max(0.1, this.cameraBack.z));
        const heightScale = projectedUp.length(); projectedUp.normalize();
        entry.contactMesh.quaternion.setFromRotationMatrix(new THREE.Matrix4().makeBasis(this.cameraRight, projectedUp, SHADOW_UP_AXIS));
        entry.contactMesh.scale.set(entry.mesh.scale.x, entry.mesh.scale.y * (contact.bottom - contact.top) * heightScale, 1);
        entry.contactMaterial.opacity = 0.5 * u.fade;
        entry.contactMaterial.alphaTest = 0.1;
        entry.contactMesh.visible = getDevGfx().contactShadows && getDevGfx().realShadows;
      }
    }

    for (const [id, entry] of this.unitEntries) {
      if (seen.has(id)) continue;
      if (engine.units.some((u) => u.id === id)) {
        // Still exists (just off-screen/out of sight/faded this frame) — hide, don't discard,
        // so it doesn't need rebuilding the instant it's visible again.
        entry.mesh.visible = false;
        entry.glowMesh.visible = false;
        entry.shadowMesh.visible = false;
        entry.contactMesh.visible = false;
        entry.proxy.visible = false;
        entry.fogCut.visible = false;
      } else {
        this.unitGroup.remove(entry.mesh);
        this.unitGroup.remove(entry.glowMesh);
        this.unitGroup.remove(entry.fogCut);
        entry.glowMaterial.dispose();
        this.shadowCasterGroup.remove(entry.shadowMesh);
        this.contactShadowGroup.remove(entry.contactMesh);
        this.shadowCasterGroup.remove(entry.proxy);
        entry.material.dispose(); // owned per-unit — see unitTexCache's comment; the texture itself is shared, kept
        entry.shadowMaterial.dispose(); // same reasoning, shadowMaterial is this unit's own instance too
        entry.contactMaterial.dispose();
        this.unitEntries.delete(id);
      }
    }
  }

  /** Fade only scenery between the camera and the selected sprite or cursor. */
  private syncTacticsScenery(tile: number): void {
    if (!this.engine.tacticsCamera) return;
    for (const entry of this.wallEntries) {
      const height = this.landscape?.heightAt(entry.mesh.position.x, entry.mesh.position.y) ?? 0;
      entry.mesh.position.z = entry.shadowMesh.position.z = height;
    }
    for (const entry of this.decorEntries) {
      const anchorPoint = entry.mesh.userData.tacticsAnchor;
      const height = this.landscape?.heightAt(anchorPoint?.x ?? entry.mesh.position.x, anchorPoint?.y ?? entry.mesh.position.y) ?? 0;
      if (entry.mesh.userData.tacticsModel || entry.mesh.userData.importedTree) entry.mesh.position.z = height;
      const anchor = entry.mesh.userData.tacticsAnchor;
      if (!anchor) continue;
      anchor.z = height;
      // Sort artwork by its ground anchor, rather than the center of a tall bitmap.
      // This keeps a nearer post in front of trunks whose painted tops overlap it.
      entry.mesh.renderOrder = 1 + (anchor.x * this.cameraBack.x + anchor.y * this.cameraBack.y + anchor.z * this.cameraBack.z) / (tile * 100000);
      // Preserve authored fog layering when the camera updates the ground-anchor sort.
      const def = DECORATIONS[entry.placement.id];
      if (def?.aboveGroundMist) entry.mesh.renderOrder += ABOVE_FOG_SCENERY_ORDER;
      // Fog renders at order 100. This authored foreground signpost must follow it;
      // syncDecorVisibility still hides the entire prop until its cell is explored.
      if (def?.aboveTacticalOverlays) entry.mesh.renderOrder = Math.max(entry.mesh.renderOrder, 101);
      if (anchor.flat) entry.mesh.position.z = anchor.z + 0.02;
      else if (anchor.fixed) {
        const right = new THREE.Vector3(1, 0, 0).applyQuaternion(entry.mesh.quaternion);
        entry.mesh.position.set(anchor.x, anchor.y, anchor.z + anchor.center)
          .addScaledVector(right, anchor.offsetX);
      }
      else {
        entry.mesh.quaternion.copy(this.unitFacing);
        entry.mesh.position.set(anchor.x, anchor.y, anchor.z)
          .addScaledVector(this.cameraRight, anchor.offsetX)
          .addScaledVector(this.unitUp, anchor.center);
      }
      // Clone materials track the calibrated color of their original sprite material.
      const material = entry.mesh.material as THREE.MeshLambertMaterial;
      const source = entry.mesh.userData.sourceMaterial as THREE.MeshLambertMaterial | undefined;
      if (source && anchor.flat) material.color.copy(source.color);
      if (DECORATIONS[entry.placement.id]?.aboveTacticalOverlays) {
        // Preserve the signpost's baked lighting. An upright card's surface normal
        // cannot represent the lighting of the individual surfaces painted in its art.
        if (material.emissiveMap !== material.map) {
          material.emissiveMap = material.map;
          material.needsUpdate = true;
        }
        material.emissive.set(0xffffff);
        material.color.set(0x000000);
        entry.mesh.receiveShadow = false;
      }
      if (entry.shadowMesh) {
        const caster = entry.shadowMesh;
        caster.position.z = anchor.z + caster.scale.y / 2 - DECOR_SHADOW_GROUND_INSET / 2;
        if (anchor.fixed) {
          const right = new THREE.Vector3(1, 0, 0).applyQuaternion(entry.mesh.quaternion);
          caster.quaternion.copy(entry.mesh.quaternion);
          caster.position.set(anchor.x, anchor.y, anchor.z + caster.scale.y / 2 - DECOR_SHADOW_GROUND_INSET / 2)
            .addScaledVector(right, anchor.offsetX);
        } else if (!anchor.flat) {
          caster.quaternion.copy(this.unitFacing);
          caster.position.set(anchor.x, anchor.y, anchor.z)
            .addScaledVector(this.cameraRight, anchor.offsetX)
            .addScaledVector(this.unitUp, caster.scale.y / 2 - DECOR_SHADOW_GROUND_INSET / 2);
        }
      }
      if (entry.contactMesh) entry.contactMesh.position.z = anchor.z + CONTACT_SHADOW_Z;
      if (entry.proxy) entry.proxy.position.z = anchor.z + entry.proxy.scale.z / 2;
    }

    this.scene.updateMatrixWorld(true);
    const targets: THREE.Vector3[] = [];
    const selected = this.unitEntries.get(this.engine.selectedId ?? "");
    if (selected?.mesh.visible) {
      for (const fraction of [-0.25, 0, 0.3]) targets.push(selected.mesh.position.clone()
        .addScaledVector(this.unitUp, selected.mesh.scale.y * fraction));
    }
    const cell = this.engine.hover ?? this.engine.cursor;
    if (this.engine.explored(cell.x, cell.y)) {
      const point = this.boardWorld(cell.x, cell.y, tile);
      targets.push(new THREE.Vector3(point.wx, -point.wy, this.groundHeight(cell.x, cell.y, tile) + tile * 0.08));
    }
    // Architecture is solid scenery, not a billboard that should fade to reveal
    // a unit behind it. Keep wall meshes opaque even when a ray to the selected
    // unit/cursor intersects them; the FX overlay mask exposes the building
    // beneath effects without changing its material opacity.
    for (const entry of [...this.wallEntries, ...this.decorEntries.filter(entry => entry.mesh.userData.tacticsArchitecture || BARRICADE_LIKE_DECOR.has(entry.placement.id))]) {
      entry.mesh.userData.tacticsOpacity = 1;
      const materials = Array.isArray(entry.mesh.material) ? entry.mesh.material : [entry.mesh.material];
      for (const material of materials) {
        material.opacity = 1;
        material.depthWrite = true;
      }
    }
    const fadeableDoors = this.wallEntries.filter(entry => {
      const def = DECORATIONS[entry.placement.id];
      return entry.mesh.visible && def?.doorStyle === "stoneOak" && def.model3d === "doorway";
    }).map(entry => entry.mesh);
    const candidates = [...fadeableDoors, ...this.decorEntries.filter(entry => !entry.mesh.userData.tacticsArchitecture && !BARRICADE_LIKE_DECOR.has(entry.placement.id)).map(entry => entry.mesh)]
      .filter(mesh => mesh.visible);
    const covering = new Set<THREE.Object3D>();
    const distance = this.camera.far;
    const ray = new THREE.Raycaster();
    for (const point of targets) {
      ray.set(point.clone().addScaledVector(this.cameraBack, distance), this.cameraBack.clone().negate());
      ray.far = distance - tile * 0.04;
      for (const hit of ray.intersectObjects(candidates, false)) covering.add(hit.object);
    }
    // The swung timber leaf can cross an entire unit card even though the
    // doorway itself is passable. Include passing units, not just selection.
    for (const wall of this.wallEntries) {
      const def = DECORATIONS[wall.placement.id];
      if (!wall.mesh.visible || def?.doorStyle !== "stoneOak" || def.model3d !== "doorway") continue;
      for (const unit of this.unitEntries.values()) {
        if (!unit.mesh.visible) continue;
        const ground = unit.mesh.position.clone().addScaledVector(this.unitUp, -Math.abs(unit.mesh.scale.y) / 2);
        if (Math.hypot(ground.x - wall.mesh.position.x, ground.y - wall.mesh.position.y) > tile * 1.25) continue;
        for (const height of [-0.3, 0, 0.3]) for (const side of [-0.25, 0, 0.25]) {
          const point = unit.mesh.position.clone()
            .addScaledVector(this.unitUp, Math.abs(unit.mesh.scale.y) * height)
            .addScaledVector(this.cameraRight, Math.abs(unit.mesh.scale.x) * side);
          ray.set(point.clone().addScaledVector(this.cameraBack, distance), this.cameraBack.clone().negate());
          ray.far = distance - tile * 0.04;
          if (ray.intersectObject(wall.mesh, false).length) covering.add(wall.mesh);
        }
      }
    }
    for (const mesh of candidates) {
      const placementId = mesh.userData.tacticsPlacementId;
      const solidHouse = HOUSE_DECOR_IDS.has(placementId) || BIG_HOUSE_DECOR_IDS.has(placementId) || SOLID_HOUSE_DECOR_IDS.has(placementId);
      const goal = !solidHouse && covering.has(mesh) ? 0.25 : 1;
      const opacity = solidHouse ? 1 : THREE.MathUtils.lerp(mesh.userData.tacticsOpacity ?? 1, goal, 0.18);
      mesh.userData.tacticsOpacity = opacity;
      const materials = Array.isArray(mesh.material) ? mesh.material : [mesh.material];
      for (const material of materials) {
        material.opacity = opacity;
        material.depthWrite = !mesh.userData.tacticsCard && opacity > 0.98;
        if (mesh.userData.tacticsCard && DECORATIONS[mesh.userData.tacticsPlacementId]?.aboveTacticalOverlays) {
          material.depthTest = false;
          material.depthWrite = opacity > 0.98;
        }
      }
    }
  }

  /** Fog-of-war visibility, rechecked every frame without touching geometry — cheap, and most
   * missions have `fog` off entirely (see Mission.fog), in which case this is a no-op loop that
   * only ever sets `visible = true`. */
  private syncDecorVisibility(): void {
    const engine = this.engine;
    for (const entry of this.wallEntries) {
      entry.mesh.visible = !engine.fogged || engine.explored(entry.placement.x, entry.placement.y);
      entry.shadowMesh.visible = entry.mesh.visible;
    }
    if (!engine.fogged) {
      for (const entry of this.decorEntries) {
        entry.mesh.visible = true;
        if (entry.shadowMesh) entry.shadowMesh.visible = true;
        if (entry.contactMesh) entry.contactMesh.visible = true;
        if (entry.proxy) entry.proxy.visible = true;
        if (entry.fogCut) entry.fogCut.visible = true;
      }
      return;
    }
    for (const entry of this.decorEntries) {
      const p = entry.placement;
      const visible = placedFootprint(p).some((f) => engine.explored(p.x + f.dx, p.y + f.dy));
      entry.mesh.visible = visible;
      if (entry.shadowMesh) entry.shadowMesh.visible = visible;
      if (entry.contactMesh) entry.contactMesh.visible = visible;
      if (entry.proxy) entry.proxy.visible = visible;
      if (entry.fogCut) entry.fogCut.visible = visible;
    }
  }

  /** rgba(r,g,b[,a]) -> a cached, unlit, transparent material — one per exact fill string
   * (color AND alpha both baked into the cache key, since neither animates once resolved: see
   * overlayGroup's own comment on why the canvas-only glow pulse is skipped here). */
  private overlayMaterialFor(fill: string): THREE.MeshBasicMaterial {
    const hit = this.overlayMatCache.get(fill);
    if (hit) return hit;
    const m = /rgba?\(([^,]+),([^,]+),([^,]+)(?:,([^)]+))?\)/.exec(fill);
    const r = m ? Number(m[1]) / 255 : 1;
    const g = m ? Number(m[2]) / 255 : 1;
    const b = m ? Number(m[3]) / 255 : 1;
    const a = m && m[4] !== undefined ? Number(m[4]) : 1;
    const mat = new THREE.MeshBasicMaterial({ color: new THREE.Color(r, g, b), transparent: true, opacity: a, depthWrite: false });
    this.overlayMatCache.set(fill, mat);
    return mat;
  }

  /** Radial additive color used as a subtle ground glow beneath a highlighted cell. */
  private overlayGlowMaterialFor(fill: string): THREE.MeshBasicMaterial {
    const m = /rgba?\(([^,]+),([^,]+),([^,]+)(?:,([^)]+))?\)/.exec(fill);
    const rgb = m ? `${m[1]},${m[2]},${m[3]}` : fill;
    const hit = this.gridGlowMatCache.get(rgb);
    if (hit) return hit;
    const glow = rgb === "110,0,8";
    const glowMatch = /rgba?\(([^,]+),([^,]+),([^,]+)(?:,([^)]+))?\)/.exec(GRID_ENEMY_GLOW);
    const r = glow && glowMatch ? Number(glowMatch[1]) / 255 : m ? Number(m[1]) / 255 : 1;
    const g = glow && glowMatch ? Number(glowMatch[2]) / 255 : m ? Number(m[2]) / 255 : 1;
    const b = glow && glowMatch ? Number(glowMatch[3]) / 255 : m ? Number(m[3]) / 255 : 1;
    const mat = new THREE.MeshBasicMaterial({
      map: this.flameHaloTexture,
      color: new THREE.Color(r, g, b),
      transparent: true,
      depthWrite: false,
      blending: THREE.AdditiveBlending,
      opacity: 0.3,
    });
    this.gridGlowMatCache.set(rgb, mat);
    return mat;
  }

  /** Mirrors the 2D renderer's `drawImage(..., cover)` plus its 42% black wash. Most scenes
   * remain camera-backed, while O Vau's artwork is pinned to its terrain so the painted river
   * continues the river made from map hexes as the camera pans. */
  private syncBackdrop(cssW: number, cssH: number, tile: number): void {
    const image = this.engine.art.backdrops[this.engine.mission.id] ?? null;
    if (image !== this.backdropImage) {
      this.backdropTexture?.dispose();
      this.backdropImage = image;
      this.backdropTexture = image ? new THREE.Texture(image) : null;
      if (this.backdropTexture) {
        this.backdropTexture.needsUpdate = true;
        this.backdropTexture.colorSpace = THREE.SRGBColorSpace;
      }
      this.backdropMaterial.map = this.backdropTexture;
      this.backdropMaterial.needsUpdate = true;
    }
    this.backdropMesh.visible = !!image;
    if (!image) return;
    const imageRatio = image.width / Math.max(1, image.height);
    this.backdropMesh.renderOrder = -1000;
    if (this.engine.tacticsCamera) {
      // Keep the painted background facing the camera and covering its full viewport.
      // A ground-aligned quad becomes a narrow strip when the board is tilted.
      const viewWidth = (this.camera.right - this.camera.left) / this.camera.zoom;
      const viewHeight = (this.camera.top - this.camera.bottom) / this.camera.zoom;
      const width = Math.max(viewWidth, viewHeight * imageRatio);
      this.backdropMesh.quaternion.copy(this.camera.quaternion);
      this.backdropMesh.position.copy(this.camera.position)
        .addScaledVector(this.cameraBack, -this.camera.far * 0.9)
        .addScaledVector(this.cameraRight, (this.camera.right + this.camera.left) / (2 * this.camera.zoom))
        .addScaledVector(this.cameraUp, (this.camera.top + this.camera.bottom) / (2 * this.camera.zoom));
      this.backdropMesh.scale.set(width, width / imageRatio, 1);
      return;
    }
    this.backdropMesh.quaternion.identity();
    if (this.engine.mission.id === "vau") {
      // O Vau's river occupies rows 5–6 (with shore rows 4 and 7). The supplied panorama's
      // water band sits at ~56% down the frame. Anchor those two centers together in world
      // space; unlike a decorative screen background, it now moves exactly with the map.
      const bounds = vauBackdropBounds(tile, this.engine.cols, cssW, cssH, imageRatio);
      this.backdropMesh.position.set(bounds.left + bounds.width / 2, -bounds.top - bounds.height / 2, -2);
      this.backdropMesh.scale.set(bounds.width, bounds.height, 1);
      return;
    }
    const viewRatio = cssW / Math.max(1, cssH);
    const width = imageRatio > viewRatio ? cssH * imageRatio : cssW;
    const height = imageRatio > viewRatio ? cssH : cssW / imageRatio;
    this.backdropMesh.position.set(this.engine.camX + cssW / 2, -this.engine.camY - cssH / 2, -2);
    this.backdropMesh.scale.set(width, height, 1);
  }

  /** Reuse mesh slots for quiet fills, hex borders, and route breadcrumbs. */
  private syncOverlay(tile: number): void {
    const engine = this.engine;
    let idx = 0;
    let glowIdx = 0;
    const placeGlow = (x: number, y: number, fill: string, fade: number) => {
      let mesh = this.gridGlowMeshes[glowIdx];
      if (!mesh) {
        mesh = new THREE.Mesh(this.quadGeo, this.overlayGlowMaterialFor(fill));
        this.gridGlowGroup.add(mesh);
        this.gridGlowMeshes.push(mesh);
      }
      mesh.material = this.overlayGlowMaterialFor(fill);
      mesh.material.opacity = 0.3 * fade;
      mesh.visible = true;
      const { wx, wy } = this.boardWorld(x, y, tile);
      const size = tile * 2.7;
      mesh.scale.set(size, size, 1);
      mesh.position.set(wx, -wy, 0.49 + this.groundHeight(x, y, tile));
      mesh.geometry = this.quadGeo;
      this.drapeTerrainOverlay(mesh, this.quadGeo);
      glowIdx++;
    };
    const place = (x: number, y: number, fill: string, geometry = this.hexGeo, radius = 0.94, z = 0.5, onTop = false) => {
      let mesh = this.overlayMeshPool[idx];
      if (!mesh) {
        mesh = new THREE.Mesh(geometry, this.overlayMaterialFor(fill));
        this.overlayGroup.add(mesh);
        this.overlayMeshPool.push(mesh);
      }
      mesh.geometry = geometry;
      const sourceMaterial = this.overlayMaterialFor(fill);
      mesh.material = sourceMaterial;
      mesh.renderOrder = 0;
      if (onTop && engine.tacticsCamera) {
        if (mesh.userData.cursorSource !== sourceMaterial) {
          (mesh.userData.cursorMaterial as THREE.Material | undefined)?.dispose();
          mesh.userData.cursorMaterial = sourceMaterial.clone();
          mesh.userData.cursorSource = sourceMaterial;
          mesh.userData.cursorMaterial.depthTest = true;
        }
        mesh.material = mesh.userData.cursorMaterial;
        mesh.renderOrder = 100;
      }
      mesh.visible = true;
      const { wx, wy } = this.boardWorld(x, y, tile);
      mesh.scale.set(tile * radius * 2, tile * radius * 2, 1);
      mesh.position.set(wx, -wy, z + this.groundHeight(x, y, tile));
      this.drapeTerrainOverlay(mesh, geometry);
      idx++;
    };
    const fade = engine.overlayFade;
    for (const layer of engine.boardOverlayLayers()) {
      const layerFade = layer.fill === GRID_MOVE ? fade : 1;
      const style = tacticalGridStyle(layer.fill);
      for (const c of layer.cells) {
        place(c.x, c.y, fadedFill(style.fill, layerFade), this.hexGeo, 1.0);
        if (layer.fill === GRID_MOVE || layer.fill === GRID_ENEMY_TARGET || layer.fill === GRID_OFFHAND_TARGET) {
          place(c.x, c.y, fadedFill(style.edge, layerFade), this.focusBorderGeo, 0.94, 0.505);
        }
      }
    }
    const route = engine.movementPreview();
    route.forEach((c, i) => {
      const last = i === route.length - 1;
      place(c.x, c.y, "rgba(8,12,16,0.95)", last ? this.focusBorderGeo : this.hexGeo, last ? 0.985 : 0.16, 0.512);
      if (last) place(c.x, c.y, "rgba(220,226,235,0.12)", this.focusBorderGeo, 1.01, 0.513);
      place(c.x, c.y, GRID_ROUTE, last ? this.focusBorderGeo : this.hexGeo, last ? 0.94 : 0.12, 0.515);
    });
    const active = engine.activeTurnHighlight();
    if (active) {
      if (active.player) {
        place(active.x, active.y, "rgba(220,226,235,0.012)", this.focusBorderGeo, 1.035, 0.516);
        place(active.x, active.y, "rgba(220,226,235,0.022)", this.focusBorderGeo, 1.01, 0.517);
        place(active.x, active.y, "rgba(220,226,235,0.04)", this.focusBorderGeo, 0.985, 0.518);
      }
      place(active.x, active.y, "rgba(12,20,25,0.85)", this.focusBorderGeo, 0.98, 0.52);
      place(active.x, active.y, active.player ? fadedFill(active.fill, 0.28) : active.fill, this.focusBorderGeo, 0.94, 0.525);
    }
    const cur = engine.hover ?? engine.cursor;
    const curId = tileAt(engine.tiles, engine.cols, cur.x, cur.y);
    if (curId !== "void" && engine.explored(cur.x, cur.y)) {
      const blocked = !TERRAIN[curId].passable;
      place(cur.x, cur.y, "rgba(8,12,16,0.95)", this.focusBorderGeo, 0.985, 0.508, true);
      place(cur.x, cur.y, blocked ? "rgba(231,133,115,0.95)" : "rgba(220,226,235,1)", this.focusBorderGeo, 0.94, 0.51, true);
    }
    for (; idx < this.overlayMeshPool.length; idx++) this.overlayMeshPool[idx]!.visible = false;
    for (; glowIdx < this.gridGlowMeshes.length; glowIdx++) this.gridGlowMeshes[glowIdx]!.visible = false;
  }

  private syncPortalFx(): void {
    const engine = this.engine;
    const b = engine.portalFxBounds();
    if (!b) {
      if (this.portalMesh) this.portalMesh.visible = false;
      return;
    }
    if (!this.portalCanvas) {
      this.portalCanvas = document.createElement("canvas");
      this.portalTexture = new THREE.CanvasTexture(this.portalCanvas);
      this.portalTexture.colorSpace = THREE.SRGBColorSpace;
      // The overlay drew this with "lighter" (additive), so it adds light here too.
      this.portalBasicMaterial = new THREE.MeshBasicMaterial({ map: this.portalTexture, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending });
      this.portalWarpMaterial = new THREE.ShaderMaterial({
        uniforms: { u_map: { value: this.portalTexture }, u_time: { value: 0 } },
        vertexShader: WARP_PORTAL_VERTEX,
        fragmentShader: WARP_PORTAL_FRAGMENT,
        transparent: true,
        depthWrite: false,
        blending: THREE.AdditiveBlending,
        side: THREE.DoubleSide,
        toneMapped: false,
      });
      this.portalMesh = new THREE.Mesh(new THREE.PlaneGeometry(1, 1), this.portalBasicMaterial);
      this.scene.add(this.portalMesh);
    }
    // Only the portal's own box is repainted and uploaded each frame — never the whole viewport.
    const bw = Math.max(1, Math.ceil(b.x1 - b.x0));
    const bh = Math.max(1, Math.ceil(b.y1 - b.y0));
    const canvas = this.portalCanvas;
    if (canvas.width !== bw || canvas.height !== bh) {
      canvas.width = bw;
      canvas.height = bh;
      // A resized canvas needs a fresh GPU texture of the new size.
      this.portalTexture!.dispose();
      this.portalTexture = new THREE.CanvasTexture(canvas);
      this.portalTexture.colorSpace = THREE.SRGBColorSpace;
      this.portalBasicMaterial!.map = this.portalTexture;
      this.portalBasicMaterial!.needsUpdate = true;
      this.portalWarpMaterial!.uniforms.u_map!.value = this.portalTexture;
    }
    const ctx = canvas.getContext("2d")!;
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.clearRect(0, 0, bw, bh);
    ctx.setTransform(1, 0, 0, 1, -b.x0, -b.y0);
    engine.drawPortalFxLayer(ctx);
    this.portalTexture!.needsUpdate = true;
    const warpOpen = engine.portalFxHasWarp();
    if (warpOpen) {
      if (this.portalTexture!.magFilter !== THREE.NearestFilter) {
        this.portalTexture!.magFilter = THREE.NearestFilter;
        this.portalTexture!.minFilter = THREE.NearestFilter;
        this.portalTexture!.generateMipmaps = false;
        this.portalTexture!.needsUpdate = true;
      }
      this.portalWarpMaterial!.uniforms.u_time!.value = performance.now() * 0.001;
      if (this.portalMesh!.material !== this.portalWarpMaterial) this.portalMesh!.material = this.portalWarpMaterial!;
    } else if (this.portalMesh!.material !== this.portalBasicMaterial) {
      this.portalTexture!.magFilter = THREE.LinearFilter;
      this.portalTexture!.minFilter = THREE.LinearFilter;
      this.portalTexture!.needsUpdate = true;
      this.portalMesh!.material = this.portalBasicMaterial!;
    }
    // Screen pixel (sx, sy) sits at world (camX + sx, -camY - sy) — see render()'s camera line.
    const mesh = this.portalMesh!;
    mesh.visible = true;
    mesh.scale.set(bw, bh, 1);
    mesh.position.set(engine.camX + b.x0 + bw / 2, -engine.camY - b.y0 - bh / 2, 0.55);
  }

  /** Keep the scene's camera basis current before positioning camera-facing character cards. */
  private updateCamera(cssW: number, cssH: number, tile: number): void {
    const pitch = THREE.MathUtils.degToRad(THREE.MathUtils.clamp(this.engine.cameraTilt, 0, 55));
    const azimuth = THREE.MathUtils.degToRad(THREE.MathUtils.clamp(this.engine.cameraTiltSide, -360, 360));
    const sinPitch = Math.sin(pitch), cosPitch = Math.cos(pitch);
    const sinAzimuth = Math.sin(azimuth), cosAzimuth = Math.cos(azimuth);
    // Walls retain their authored 3.5x isometric height shear. Match character-card size to
    // the resulting projected wall height so billboards do not grow relative to the scenery.
    this.cameraSpriteScale = Math.max(0.28, (3.5 * cosPitch + sinPitch) / 3.5);
    this.cameraRight.set(cosAzimuth, sinAzimuth, 0);
    this.cameraUp.set(-sinAzimuth * cosPitch, cosAzimuth * cosPitch, sinPitch);
    this.cameraGroundDown.set(sinAzimuth, -cosAzimuth, 0);
    const back = new THREE.Vector3(sinAzimuth * sinPitch, -cosAzimuth * sinPitch, cosPitch);
    this.cameraBack.copy(back);
    this.unitUp.copy(this.engine.tacticsCamera ? new THREE.Vector3(0, 0, 1) : this.cameraUp);
    const unitNormal = new THREE.Vector3().crossVectors(this.cameraRight, this.unitUp).normalize();
    this.unitFacing.setFromRotationMatrix(new THREE.Matrix4().makeBasis(this.cameraRight, this.unitUp, unitNormal));
    const centerX = this.engine.camX + cssW / 2;
    const centerY = -this.engine.camY - cssH / 2;
    const tileWorldW = tile * Math.sqrt(3) * (this.engine.cols + this.engine.rows * 0.5);
    const tileWorldH = tile * 1.5 * this.engine.rows;
    const cameraDistance = Math.max(100, Math.hypot(tileWorldW, tileWorldH) / 2 + Math.max(cssW, cssH));
    const requiredFar = Math.max(2000, cameraDistance * 2.5);
    if (this.camera.far !== requiredFar) {
      this.camera.far = requiredFar;
      this.camera.updateProjectionMatrix();
    }
    this.camera.position.set(centerX, centerY, 0)
      .addScaledVector(this.cameraRight, -cssW / 2)
      .addScaledVector(this.cameraUp, -cssH / 2)
      .addScaledVector(back, cameraDistance);
    const basis = new THREE.Matrix4().makeBasis(this.cameraRight, this.cameraUp, back);
    this.camera.quaternion.setFromRotationMatrix(basis);
    this.camera.updateMatrixWorld(true);
  }

  /** Call once per frame in place of BattleEngine.renderGround — updateCameraLayout runs the
   * exact same camera/visibility bookkeeping renderGround always did (see that method's own
   * comment), just without drawing through the Canvas2D shim afterward. */
  render(cssW: number, cssH: number, paused = false): void {
    this.groundDepthLayerEnabled.value = this.engine.tacticsCamera ? 0 : 1;
    const tile = this.engine.updateCameraLayout(cssW, cssH);
    this.updateCamera(cssW, cssH, tile);
    this.engine.architectureRenderedInThree = true;
    this.syncBackdrop(cssW, cssH, tile);
    this.ensureBuilt(tile);
    this.syncDirtyTiles();
    this.syncTerrainHeight(tile);
    this.syncWater(tile);
    this.syncElevationSteps(tile);
    this.ensureDecorBuilt(tile);
    this.syncGroundAO(tile);
    this.syncFog(tile);
    // Emitter simulation produces its current flicker value, then this same shared light pool
    // applies it to terrain, props and units in the same frame.
    const now = performance.now();
    const dt = paused ? 0 : Math.min(0.1, (now - this.lastFrameTime) / 1000);
    this.lastFrameTime = now;
    this.syncPixelElementEmitters(tile, cssW, cssH, dt);
    this.syncLights(tile, cssW, cssH);
    this.syncHealingSpellLight(tile);
    this.syncSpriteExposure();
    this.syncDecorVisibility();
    this.syncOverlay(tile);
    this.syncPortalFx();
    this.syncUnits(tile);
    this.syncTacticsScenery(tile);
    this.syncSky();
    this.applyDevGfx();
    for(const pulse of this.blizzardLightPulses){pulse.age+=dt;pulse.light.intensity*=Math.exp(-dt*6);if(pulse.age>1)pulse.light.intensity=0;}
    this.syncFireballVfx(dt, cssW, cssH, tile);
    this.syncCausticVenomVfx(dt, tile);
    this.syncPhantasmalForceVfx(dt, tile);
    this.syncBlessVfx(dt, tile);
    this.syncMagicMissileV2Vfx(dt, tile);
    this.syncBurningHandsV2Vfx(dt, tile);
    this.syncVarreduraVfx(dt, tile);
    this.syncCleaveVfx(dt, tile);
    this.webOfDreamsVfx?.update(this.engine, tile, dt);
    this.atmosphere.sync(
      this.engine,
      tile,
      dt,
      this.sunLight,
      this.hemiLight,
      { cssW, cssH, camX: this.engine.camX, camY: this.engine.camY },
      (x, y) => this.cellAtWorld(x, y),
      this.unitFeetForMist(),
      getDevGfx().atmosphericFx,
    );
    // MILESTONE 2 — the sun has to re-aim every frame too, for the same reason the camera does:
    // the shadow-caster boxes are fixed in world space, only the view of them pans.
    this.updateSun(cssW, cssH, this.engine.camX, this.engine.camY);
    // Bloom samples the real 3D scene, except for authored light-source art. Their point
    // lights still illuminate everything normally, but the torch/candle/brazier sprite itself
    // must not turn into a blinding white halo when bloom is enabled.
    // Spell effects switch their lights' visibility on and off. A hidden light drops out of the
    // scene's light count, and any change to that count recompiles every lit material on screen
    // (a visible stall when a spell starts or ends). For the draw, hidden lights count as present
    // but dark, so the count never changes; their own on/off state is restored right after.
    const parked = this.parkHiddenLights();
    this.balanceLightCount();
    try {
      this.syncSpellVfxLayers();
      // Nothing is drawn until every shader is compiled in the background — the loading curtain
      // is up meanwhile — so no frame (first one included) ever freezes compiling them.
      if (this.meleeVfxWarm !== "done") {
        this.warmMeleeVfxOnce();
        return;
      }
      this.renderBloomWithoutLightSourceArt();
      this.finalComposer.render();
    } finally {
      this.restoreParkedLights(parked);
    }
  }

  private lightBudget: { point: number; shadow: number } | null = null;
  private readonly fillerLights: { point: THREE.PointLight[]; shadow: THREE.PointLight[] } = { point: [], shadow: [] };
  /** Pads the drawn point lights up to a fixed budget with dark filler lights, so however spell
   * effects add, remove, hide or nest their lights, the count the shaders see never changes —
   * a change recompiles every lit material on screen. Grows (one recompile) only if a battle
   * ever needs more than the first frame's count plus headroom. */
  private balanceLightCount(): void {
    for (const light of [...this.fillerLights.point, ...this.fillerLights.shadow]) light.visible = false;
    let point = 0;
    let shadow = 0;
    this.scene.traverseVisible((o) => {
      if (!(o as THREE.PointLight).isPointLight) return;
      if ((o as THREE.PointLight).castShadow) shadow++;
      else point++;
    });
    if (!this.lightBudget) this.lightBudget = { point: point + LIGHT_BUDGET_HEADROOM, shadow: shadow + SHADOW_LIGHT_BUDGET_HEADROOM };
    this.lightBudget.point = Math.max(this.lightBudget.point, point);
    this.lightBudget.shadow = Math.max(this.lightBudget.shadow, shadow);
    const fill = (have: number, budget: number, pool: THREE.PointLight[], castsShadow: boolean) => {
      for (let i = 0; i < budget - have; i++) {
        let light = pool[i];
        if (!light) {
          light = new THREE.PointLight(0xffffff, 0, 1, 2);
          light.layers.enable(SPELL_VFX_LAYER);
          if (castsShadow) {
            light.castShadow = true;
            light.shadow.mapSize.set(16, 16);
            light.shadow.autoUpdate = false;
          }
          pool.push(light);
          this.scene.add(light);
        }
        light.visible = true;
      }
    };
    fill(point, this.lightBudget.point, this.fillerLights.point, false);
    fill(shadow, this.lightBudget.shadow, this.fillerLights.shadow, true);
  }

  private parkHiddenLights(): { light: THREE.Light; intensity: number; autoUpdate: boolean }[] {
    const parked: { light: THREE.Light; intensity: number; autoUpdate: boolean }[] = [];
    this.scene.traverse((child) => {
      if (!(child instanceof THREE.Light) || child.visible || child instanceof THREE.AmbientLight || child instanceof THREE.HemisphereLight) return;
      if (this.fillerLights.point.includes(child as THREE.PointLight) || this.fillerLights.shadow.includes(child as THREE.PointLight)) return;
      const shadow = (child as THREE.PointLight).shadow;
      parked.push({ light: child, intensity: child.intensity, autoUpdate: shadow ? shadow.autoUpdate : true });
      child.visible = true;
      child.intensity = 0;
      if (shadow) shadow.autoUpdate = false;
    });
    return parked;
  }

  private restoreParkedLights(parked: { light: THREE.Light; intensity: number; autoUpdate: boolean }[]): void {
    for (const { light, intensity, autoUpdate } of parked) {
      light.visible = false;
      light.intensity = intensity;
      const shadow = (light as THREE.PointLight).shadow;
      if (shadow) shadow.autoUpdate = autoUpdate;
    }
  }

  private meleeVfxWarm: "pending" | "running" | "done" = "pending";
  /** True once the first frame is drawn and every spell shader is compiled and linked. */
  isWarm(): boolean {
    return this.meleeVfxWarm === "done";
  }
  /** On the battle's first rendered frame (lights and shadows final), compile and link the shaders
   * of every spell effect — the hidden ones (Fireball, Caustic Venom, Phantasmal Force, Bless, ...)
   * and the per-cast Cleave/Sweep — so no first cast stalls (0.45-1.8 s each before). */
  private warmMeleeVfxOnce(): void {
    if (this.meleeVfxWarm !== "pending") return;
    this.meleeVfxWarm = "running";
    const before = new Set(this.scene.children);
    const warm = [
      new CleaveSweepVFX(this.spellVfxScene, new THREE.Vector3(), [], 1, {}, this.vfxLights),
      new VarreduraVFX(this.spellVfxScene, new THREE.Vector3(), [], 1, this.vfxLights),
    ];
    for (const child of this.scene.children) if (!before.has(child)) child.traverse((o) => { o.frustumCulled = false; });
    // Effects that only build objects mid-cast get a silent dry run first (no-op callbacks; the
    // loading curtain covers the screen), so their shaders are in the background compile too.
    const dryFrom = new THREE.Vector3(0, 0, 1);
    const dryTo = new THREE.Vector3(40, 0, 1);
    const noop = () => {};
    const caustic = this.causticVenomVfx;
    const phantasmal = this.phantasmalForceVfx;
    if (caustic) {
      caustic.cast({ id: "warmup", origin: dryFrom, target: dryTo, worldScale: 1, impactHexes: [dryTo], onLaunch: noop, onImpact: noop, onComplete: noop });
      for (let i = 0; i < 3; i++) caustic.update(0.4); // charge, travel, into the impact
    }
    if (phantasmal) {
      phantasmal.restartAt(dryTo, 1);
      phantasmal.update(0.3);
    }
    // Every hidden effect is revealed for the whole warm-up (nothing is drawn on screen meanwhile —
    // render() skips drawing until this is done), so the background compile and the final linking
    // draw see exactly the same objects and lights. Revealing them only for the draw changed the
    // light count and made every shader compile again in one blocking frame (7.8 s measured).
    const shown: THREE.Object3D[] = [];
    const unculled: THREE.Object3D[] = [];
    const reveal = () => this.scene.traverse((o) => {
      if (!o.visible && !(o instanceof THREE.Light) && !shown.includes(o)) { o.visible = true; shown.push(o); }
      if (((o as THREE.Mesh).isMesh || (o as THREE.Points).isPoints || (o as THREE.Line).isLine) && o.frustumCulled) { o.frustumCulled = false; unculled.push(o); }
    });
    reveal();
    const parkedForCompile = this.parkHiddenLights();
    this.balanceLightCount();
    // The scene is always drawn into the composers' buffers, which need a different shader
    // variant than drawing straight to the canvas — compile that one, or the warm-up is wasted.
    const previousTarget = this.renderer.getRenderTarget();
    this.renderer.setRenderTarget(this.bloomComposer.renderTarget1);
    const jobs: Promise<unknown>[] = [this.renderer.compileAsync(this.scene, this.camera)];
    this.renderer.setRenderTarget(previousTarget);
    if (this.magicMissileV2Vfx && this.magicMissileForeground) {
      this.magicMissileV2Vfx.showAllForWarmup();
      this.syncSpellVfxLayers();
      jobs.push(this.magicMissileForeground.warm(this.scene, this.camera));
    }
    this.restoreParkedLights(parkedForCompile);
    void Promise.allSettled(jobs).finally(() => {
      // One real draw into a tiny off-screen target links every compiled program now, so no
      // first cast ever does it mid-fight.
      reveal(); // per-frame updates may have hidden something again in the meantime
      this.syncSpellVfxLayers();
      const parked = this.parkHiddenLights();
      this.balanceLightCount();
      const target = new THREE.WebGLRenderTarget(4, 4);
      const previous = this.renderer.getRenderTarget();
      try {
        this.renderer.setRenderTarget(target);
        this.renderer.render(this.scene, this.camera);
        // Magic Missile draws through its own renderer; link its programs the same way.
        const missile = this.magicMissileV2Vfx;
        const foreground = this.magicMissileForeground;
        if (missile && foreground && !this.activeMagicMissileV2VfxRequestId) {
          // At the real canvas size, so the first cast does not also have to resize its buffers.
          const size = this.renderer.getSize(new THREE.Vector2());
          const dpr = this.renderer.getPixelRatio();
          missile.showAllForWarmup();
          foreground.render(this.scene, this.camera, size.x, size.y, dpr, true, this.bloomPass);
          missile.hide();
          foreground.render(this.scene, this.camera, size.x, size.y, dpr, false, this.bloomPass);
        }
      } finally {
        this.renderer.setRenderTarget(previous);
        target.dispose();
        caustic?.cancel();
        phantasmal?.hide();
        this.magicMissileV2Vfx?.hide();
        for (const o of shown) o.visible = false;
        for (const o of unculled) o.frustumCulled = true;
        this.restoreParkedLights(parked);
        for (const fx of warm) fx.dispose();
        this.meleeVfxWarm = "done";
      }
    });
  }

  /** Project a legacy top-down screen point onto the actual camera view for map overlays. */
  projectFlatScreen(x: number, y: number, cssW: number, cssH: number, followSurface = false): { x: number; y: number } {
    const worldX = this.engine.camX + x, worldY = -this.engine.camY - y;
    const height = followSurface && this.engine.tacticsCamera ? this.landscape?.heightAt(worldX, worldY) ?? 0 : 0;
    const point = new THREE.Vector3(worldX, worldY, height).project(this.camera);
    return { x: (point.x + 1) * cssW / 2, y: (1 - point.y) * cssH / 2 };
  }

  /** Screen-space transform for the legacy transparent overlay canvas. Orthographic projection
   * is affine on the ground plane, so three projected points determine its exact 2D matrix. */
  overlayTransform(cssW: number, cssH: number): [number, number, number, number, number, number] {
    const origin = this.projectFlatScreen(0, 0, cssW, cssH);
    const xAxis = this.projectFlatScreen(1, 0, cssW, cssH);
    const yAxis = this.projectFlatScreen(0, 1, cssW, cssH);
    return [xAxis.x - origin.x, xAxis.y - origin.y, yAxis.x - origin.x, yAxis.y - origin.y, origin.x, origin.y];
  }

  /** Map a pointer through the tilted camera onto the z=0 ground plane, then return the
   * top-down screen coordinates BattleEngine expects for hex picking and hover UI. */
  screenToFlatScreen(x: number, y: number, cssW: number, cssH: number, pickTerrain = true): { x: number; y: number } {
    const raycaster = new THREE.Raycaster();
    raycaster.setFromCamera(new THREE.Vector2((x / cssW) * 2 - 1, 1 - (y / cssH) * 2), this.camera);
    if (this.engine.tacticsCamera && pickTerrain) {
      this.tileGroup.updateMatrixWorld(true);
      const hit = raycaster.intersectObjects(this.tileGroup.children, false).find(h => h.object.visible);
      if (hit) {
        const point = hit.point;
        return { x: point.x - this.engine.camX, y: -point.y - this.engine.camY };
      }
    }
    const directionZ = raycaster.ray.direction.z;
    if (Math.abs(directionZ) < 1e-5) return { x, y };
    const distance = -raycaster.ray.origin.z / directionZ;
    if (distance < 0) return { x, y };
    const groundPoint = raycaster.ray.at(distance, new THREE.Vector3());
    return { x: groundPoint.x - this.engine.camX, y: -groundPoint.y - this.engine.camY };
  }

  /**
   * Tactical sprites are camera-facing billboards, so warping their 2D overlay canvas to the
   * ground plane would skew them. When a screen-space elemental effect is active, draw the
   * already-synced billboard frames into the upright HUD canvas instead, above that effect.
   */
  renderTacticalUnitSprites(ctx: CanvasRenderingContext2D, cssW: number, cssH: number): void {
    if (!this.engine.tacticsCamera) return;
    this.camera.updateMatrixWorld(true);
    const viewHeight = Math.max(1e-6, (this.camera.top - this.camera.bottom) / this.camera.zoom);
    const pixelsPerWorldUnit = cssH / viewHeight;
    const sprites = [...this.unitEntries.values()]
      .filter((entry) => entry.mesh.visible && entry.img && entry.img.naturalWidth > 0)
      .map((entry) => {
        const projected = entry.mesh.position.clone().project(this.camera);
        return {
          entry,
          x: (projected.x + 1) * cssW / 2,
          y: (1 - projected.y) * cssH / 2,
        };
      })
      .sort((a, b) => a.entry.mesh.renderOrder - b.entry.mesh.renderOrder || a.y - b.y);

    for (const { entry, x, y } of sprites) {
      const img = entry.img;
      if (!img) continue;
      const w = Math.abs(entry.mesh.scale.x) * pixelsPerWorldUnit;
      const h = Math.abs(entry.mesh.scale.y) * pixelsPerWorldUnit;
      ctx.save();
      ctx.globalAlpha = entry.material.opacity;
      ctx.translate(x, y);
      if (entry.mesh.scale.x < 0) ctx.scale(-1, 1);
      ctx.drawImage(img, -w / 2, -h / 2, w, h);
      ctx.restore();
    }
  }

  /** CSS alpha mask for the effects canvas: leave Three-rendered architecture visible beneath
   * elemental FX. The mask is generated from projected architecture triangles and cached until
   * the camera, viewport, or architecture transforms change. */
  architectureFxMaskDataUri(cssW: number, cssH: number): string | null {
    const cards = this.decorEntries.filter(entry => this.decorGroup?.visible !== false
      && entry.mesh.visible && BARRICADE_LIKE_DECOR.has(entry.placement.id));
    const meshes = [
      ...this.wallEntries.map((entry) => entry.mesh),
      ...this.decorEntries.filter((entry) => entry.mesh.userData.tacticsArchitecture || !!DECORATIONS[entry.placement.id]?.model3d).map((entry) => entry.mesh),
    ].filter((mesh) => mesh.visible && mesh.geometry.getAttribute("position"));
    // Straight-border maps: map-placed FX (water/shore laid out per hex) is clipped to the
    // same rectangle the ground is cut on, so it never pokes past the straight edge.
    const squareBoard = hasSquareMapBorder(this.engine.tiles, this.engine.cols, this.engine.rows, this.engine.mission.squareTiles) && this.builtTile > 0;
    if (!meshes.length && !cards.length && !squareBoard) {
      this.architectureFxMaskKey = "";
      this.architectureFxMaskUri = null;
      return null;
    }

    this.scene.updateMatrixWorld(true);
    this.camera.updateMatrixWorld(true);
    const point = new THREE.Vector3();
    let boardClip = "";
    if (squareBoard) {
      const tile = this.builtTile;
      const top = BOARD_PAD_MUL + 0.25, bottom = top + this.engine.rows * 1.5, right = SQRT3 * this.engine.cols;
      const corners = [[0, top], [right, top], [right, bottom], [0, bottom]].map(([x, y]) => {
        point.set(x! * tile, -y! * tile, 0).project(this.camera);
        return `${(((point.x + 1) * cssW) / 2).toFixed(1)},${(((1 - point.y) * cssH) / 2).toFixed(1)}`;
      });
      boardClip = `M${corners.join("L")}Z`;
    }
    const matrixKey = (matrix: THREE.Matrix4) => matrix.elements.map((n) => n.toFixed(3)).join(",");
    const key = `${cssW}x${cssH}:${boardClip}:${matrixKey(this.camera.matrixWorld)}:${matrixKey(this.camera.projectionMatrix)}:${meshes.map((mesh) => `${mesh.geometry.id}:${mesh.geometry.getAttribute("position").count}:${matrixKey(mesh.matrixWorld)}`).join(";")}:${cards.map(({ mesh }) => `${mesh.id}:${matrixKey(mesh.matrixWorld)}`).join(";")}`;
    if (key === this.architectureFxMaskKey) return this.architectureFxMaskUri;

    const paths: string[] = [];
    for (const mesh of meshes) {
      const geometry = mesh.geometry;
      const positions = geometry.getAttribute("position");
      const index = geometry.getIndex();
      const triangleCount = Math.floor((index?.count ?? positions.count) / 3);
      for (let tri = 0; tri < triangleCount; tri++) {
        const coords: [number, number][] = [];
        for (let corner = 0; corner < 3; corner++) {
          const vertex = index ? index.getX(tri * 3 + corner) : tri * 3 + corner;
          point.fromBufferAttribute(positions, vertex).applyMatrix4(mesh.matrixWorld).project(this.camera);
          const x = ((point.x + 1) * cssW) / 2;
          const y = ((1 - point.y) * cssH) / 2;
          if (!Number.isFinite(x) || !Number.isFinite(y)) {
            coords.length = 0;
            break;
          }
          coords.push([x, y]);
        }
        if (coords.length === 3) {
          // Front/back faces project with opposite winding. Normalize it so
          // overlapping triangles form a solid silhouette instead of cancelling
          // each other under SVG's nonzero fill rule.
          const [a, b, c] = coords;
          if ((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]) < 0) {
            [coords[1], coords[2]] = [coords[2], coords[1]];
          }
          const points = coords.map(([x, y]) => `${x.toFixed(1)},${y.toFixed(1)}`);
          paths.push(`M${points[0]}L${points[1]}L${points[2]}Z`);
        }
      }
    }

    const silhouettes: string[] = [];
    for (const { mesh } of cards) {
      const texture = (mesh.material as THREE.MeshLambertMaterial).map;
      const img = texture?.image as HTMLImageElement | HTMLCanvasElement | undefined;
      if (!texture || !img || !img.width || !img.height) continue;
      // Embed the bitmap: images inside an SVG mask cannot load external URLs.
      // Cache it on the shared texture so repeated fence segments reuse one PNG.
      if (!texture.userData.fxMaskImage) {
        const canvas = document.createElement("canvas");
        canvas.width = img.width;
        canvas.height = img.height;
        const ctx = canvas.getContext("2d");
        if (!ctx) continue;
        ctx.drawImage(img, 0, 0);
        texture.userData.fxMaskImage = canvas.toDataURL();
      }
      const project = (x: number, y: number) => {
        point.set(x, y, 0).applyMatrix4(mesh.matrixWorld).project(this.camera);
        return { x: (point.x + 1) * cssW / 2, y: (1 - point.y) * cssH / 2 };
      };
      const a = project(-0.5, 0.5), b = project(0.5, 0.5), c = project(-0.5, -0.5);
      const transform = [b.x - a.x, b.y - a.y, c.x - a.x, c.y - a.y, a.x, a.y];
      if (!transform.every(Number.isFinite)) continue;
      silhouettes.push(`<image href="${texture.userData.fxMaskImage}" width="1" height="1" preserveAspectRatio="none" transform="matrix(${transform.join(" ")})" filter="url(#black)"/>`);
    }
    const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${cssW}" height="${cssH}" viewBox="0 0 ${cssW} ${cssH}"><defs><filter id="black" color-interpolation-filters="sRGB"><feColorMatrix type="matrix" values="0 0 0 0 0  0 0 0 0 0  0 0 0 0 0  0 0 0 1 0"/></filter><mask id="a" x="0" y="0" width="${cssW}" height="${cssH}" maskUnits="userSpaceOnUse">${boardClip ? `<path d="${boardClip}" fill="white"/>` : `<rect width="${cssW}" height="${cssH}" fill="white"/>`}<path d="${paths.join("")}" fill="black"/>${silhouettes.join("")}</mask></defs><rect width="${cssW}" height="${cssH}" fill="white" mask="url(#a)"/></svg>`;
    this.architectureFxMaskKey = key;
    this.architectureFxMaskUri = `url("data:image/svg+xml,${encodeURIComponent(svg)}")`;
    return this.architectureFxMaskUri;
  }

  /** Draw health bars in screen space over their camera-facing unit sprites. */
  renderUnitHealthHud(ctx: CanvasRenderingContext2D, cssW: number, cssH: number): void {
    const tile = ZOOM_RADII[this.engine.zoom]!;
    const cell = tile * SQRT3;
    const gap = Math.max(8, cell * 0.12) * this.cameraSpriteScale;
    for (const unit of this.engine.units) {
      if (!unit.alive || unit.fade <= 0 || this.engine.unitHidden(unit)) continue;
      const entry = this.unitEntries.get(unit.id);
      if (!entry?.mesh.visible) continue;
      const visual = this.engine.unitVisual(unit, tile);
      const halfHeight = visual.h * visual.scaleY * this.cameraSpriteScale * 0.5;
      const point = entry.mesh.position.clone().addScaledVector(this.unitUp, halfHeight).project(this.camera);
      const x = (point.x + 1) * cssW * 0.5;
      const y = (1 - point.y) * cssH * 0.5 - gap;
      const size = unitSize(unit);
      const boss = isBossClass(unit.classId);
      const width = cell * (size >= 4 ? 1.35 : size === 2 ? 0.9 : boss ? 0.68 : 0.62) * this.cameraSpriteScale;
      const height = Math.max(4, cell * 0.07) * this.cameraSpriteScale;
      const left = x - width / 2;
      const top = y - height;

      ctx.save();
      ctx.globalAlpha = unit.fade * (unit.moved && unit.side === "player" && this.engine.phase === "player" && !this.engine.isAnimating() ? 0.9 : 1);
      ctx.fillStyle = "rgba(12,11,10,0.82)";
      ctx.fillRect(left - 1, top - 1, width + 2, height + 2);
      ctx.fillStyle = "#2c2824";
      ctx.fillRect(left, top, width, height);
      ctx.fillStyle = unit.side === "player" ? "#c8c4bc" : unit.side === "neutral" ? "#5f9e52" : "#b54a32";
      ctx.fillRect(left, top, width * Math.max(0, unit.hp / unit.maxHp), height);
      if (cell >= 32) {
        const fontPx = Math.max(1, Math.round(cell * 0.22 * this.cameraSpriteScale));
        ctx.font = `600 ${fontPx}px Figtree, sans-serif`;
        ctx.textAlign = "center";
        ctx.textBaseline = "bottom";
        ctx.lineJoin = "round";
        ctx.lineWidth = Math.max(1, 3 * this.cameraSpriteScale);
        ctx.strokeStyle = "rgba(12,11,10,0.9)";
        ctx.fillStyle = "#f0ebe3";
        ctx.strokeText(`${unit.hp}`, x, top - 1);
        ctx.fillText(`${unit.hp}`, x, top - 1);
      }
      ctx.restore();
    }
  }

  /** Floating combat text ("Missed", damage, heals, level-up) in screen space for the tactics
   * camera: anchored on the terrain surface, lifted along the same up axis as the upright
   * character cards, so it stays over the unit when the camera tilts or turns. */
  renderFloatingText(ctx: CanvasRenderingContext2D, cssW: number, cssH: number): void {
    const point = new THREE.Vector3();
    this.engine.renderFloatingTextHud(ctx, (cx, cy, up) => {
      const worldX = this.engine.camX + cx, worldY = -this.engine.camY - cy;
      const height = this.landscape?.heightAt(worldX, worldY) ?? 0;
      point.set(worldX, worldY, height).addScaledVector(this.unitUp, up * this.cameraSpriteScale).project(this.camera);
      return { x: (point.x + 1) * cssW / 2, y: (1 - point.y) * cssH / 2 };
    }, this.cameraSpriteScale);
  }

  /** Persistent pixel emitters use one shared instanced-particle implementation. Prioritize
   * real PointLights near the current view; every placement keeps its emissive particles. */
  private syncPixelElementEmitters(tile: number, cssW: number, cssH: number, dt: number): void {
    this.pixelFxClock += dt;
    const centerX = this.engine.camX + cssW / 2;
    const centerY = this.engine.camY + cssH / 2;
    const visible = this.pixelElementEmitters.map((entry) => {
      const pos = this.boardWorld(entry.placement.x, entry.placement.y, tile);
      const seen = !this.engine.fogged || this.engine.visible(entry.placement.x, entry.placement.y);
      return { ...entry, x: pos.wx, y: pos.wy, distance: (pos.wx-centerX)**2+(pos.wy-centerY)**2, seen };
    });
    const lights = visible.filter((entry) => entry.seen && entry.placement.parameters?.lightEnabled !== false).sort((a,b)=>a.distance-b.distance);
    const lightWinners = new Set(lights.slice(0, this.pointLights.length).map((entry)=>entry.placement.id));
    for (const entry of visible) {
      entry.emitter.group.visible = entry.seen;
      entry.emitter.setLightPriority(lightWinners.has(entry.placement.id));
      entry.emitter.update(dt, tile, this.pixelFxClock, entry.x, entry.y);
      if (this.engine.tacticsCamera && this.landscape) entry.emitter.group.position.z = this.landscape.heightAt(entry.x, -entry.y);
    }
  }

  private syncVarreduraVfx(dt:number,tile:number):void{
    const requests:VarreduraVfxRequest[]=this.engine.varreduraVfxRequests.splice(0);
    for(const request of requests){
      const caster=this.engine.units.find((unit)=>unit.id===request.casterId);
      if(!caster)continue;
      const source=this.engine.unitAnchor(caster);
      const targets=request.targetIds.map((id)=>this.engine.units.find((unit)=>unit.id===id)).filter((unit)=>!!unit).map((unit)=>{const anchor=this.engine.unitAnchor(unit);return{id:unit.id,position:new THREE.Vector3(anchor.worldX,-anchor.worldY,1)};});
      if(targets.length===0)for(const cell of request.tiles){const anchor=this.engine.effectAnchor(cell.x,cell.y);targets.push({id:`tile-${cell.x}-${cell.y}`,position:new THREE.Vector3(anchor.worldX,-anchor.worldY,1)});}
      this.varreduraVfx.push(new VarreduraVFX(this.spellVfxScene,new THREE.Vector3(source.worldX,-source.worldY,1),targets,tile,this.vfxLights));
    }
    for(let i=this.varreduraVfx.length-1;i>=0;i--){const fx=this.varreduraVfx[i]!;fx.update(dt);if(fx.finished){fx.dispose();this.varreduraVfx.splice(i,1);}}
  }

  private syncCleaveVfx(dt: number, tile: number): void {
    const requests: CleaveVfxRequest[] = this.engine.cleaveVfxRequests.splice(0);
    for (const request of requests) {
      const caster = this.engine.units.find((unit) => unit.id === request.casterId);
      if (!caster) continue;
      const source = this.engine.unitAnchor(caster);
      const targets = request.targetIds.map((id) => this.engine.units.find((unit) => unit.id === id)).filter((unit) => !!unit).map((unit) => {
        const anchor = this.engine.unitAnchor(unit);
        return { id: unit.id, position: new THREE.Vector3(anchor.worldX, -anchor.worldY, 1) };
      });
      if (targets.length === 0) for (const cell of request.tiles) {
        const anchor = this.engine.effectAnchor(cell.x, cell.y);
        targets.push({ id: `tile-${cell.x}-${cell.y}`, position: new THREE.Vector3(anchor.worldX, -anchor.worldY, 1) });
      }
      this.cleaveVfx.push(new CleaveSweepVFX(this.spellVfxScene, new THREE.Vector3(source.worldX, -source.worldY, 1), targets, tile, {}, this.vfxLights));
    }
    for (let i = this.cleaveVfx.length - 1; i >= 0; i--) {
      const fx = this.cleaveVfx[i]!;
      fx.update(dt);
      if (fx.finished) { fx.dispose(); this.cleaveVfx.splice(i, 1); }
    }
  }

  /** Real surface illumination for the 2D Blizzard sprite pass; no Three.js spell art. */
  pulseBlizzardLight(col:number,row:number,dx:number,dy:number,strength:number):void {
    if(!this.blizzardLightPulses.length)return;
    const pulse=this.blizzardLightPulses.reduce((old,p)=>p.age>old.age?p:old);
    const a=this.engine.effectAnchor(col,row);const tile=a.tile;
    const lift=this.engine.hexElevated(col,row)?tile*.18:0;
    pulse.light.position.set(a.worldX+dx*tile,-a.worldY-dy*tile+lift,spriteDepthZ(a.worldY,tile)+tile*.5);
    pulse.light.distance=tile*3.2;pulse.light.intensity=tile*tile*.012*strength;
    pulse.light.shadow.camera.near=.1;pulse.light.shadow.camera.far=tile*4;pulse.age=0;
  }
  private syncFireballVfx(dt: number, cssW: number, cssH: number, tile: number): void {
    const system = this.fireballVfx;
    if (!system) return;
    const width = Math.max(1, this.renderer.domElement.width);
    const height = Math.max(1, this.renderer.domElement.height);
    const requests = this.engine.fireballVfxRequests.splice(0);
    if (!requests.length) system.update(dt, width, height, tile);
    for (const request of requests) {
      const caster = this.engine.units.find((unit) => unit.id === request.casterId);
      const destination = this.engine.effectAnchor(request.target.x, request.target.y);
      // Measure the damage footprint from the exact resolved hexes, not a guessed spell radius.
      const aoeRadius = request.tiles.reduce((radius, cell) => {
        const anchor = this.engine.effectAnchor(cell.x, cell.y);
        return Math.max(radius, Math.hypot(anchor.worldX - destination.worldX, anchor.worldY - destination.worldY) + tile);
      }, tile);
      // Hidden cells do not receive a luminous effect. Damage remains governed by combat's
      // precomputed AoE list and is intentionally unaffected by this visibility gate.
      if (!caster || (this.engine.fogged && !this.engine.visible(request.target.x, request.target.y))) {
        this.engine.fireballVfxEvents.push({ id: request.id, phase: "impact" }, { id: request.id, phase: "complete" });
        continue;
      }
      const visual = this.engine.unitVisual(caster, tile);
      const anchor = this.engine.unitAnchor(caster);
      const sign = caster.facing < 0 ? -1 : 1;
      const centerYLocal = (visual.footOffset - visual.h / 2) * visual.scaleY;
      const casterY = anchor.worldY + visual.footY + visual.bob - visual.lift + centerYLocal;
      const origin = new THREE.Vector3(
        anchor.worldX + visual.sway + sign * visual.w * visual.scaleX * 0.31,
        -casterY - visual.h * visual.scaleY * 0.13,
        1.08 + (anchor.worldY / tile) * 0.004,
      );
      const elevation = this.engine.hexElevated(request.target.x, request.target.y) ? destination.tile * 0.18 : 0;
      const target = new THREE.Vector3(destination.worldX, -(destination.worldY - elevation), 1.08 + (destination.worldY / tile) * 0.004);
      const unseenOrigin = this.engine.fogged && !this.engine.visible(caster.x, caster.y);
      const unseenTarget = this.engine.fogged && !this.engine.visible(request.target.x, request.target.y);
      if (unseenOrigin || unseenTarget) {
        this.engine.fireballVfxEvents.push({ id: request.id, phase: "impact" }, { id: request.id, phase: "complete" });
        continue;
      }
      system.cast({
        id: request.id,
        origin,
        target,
        worldScale: tile,
        aoeRadius,
        onLaunch: () => this.engine.fireballVfxEvents.push({ id: request.id, phase: "launch" }),
        onImpact: () => this.engine.fireballVfxEvents.push({ id: request.id, phase: "impact" }),
        onComplete: () => this.engine.fireballVfxEvents.push({ id: request.id, phase: "complete" }),
      });
    }
    if (requests.length) system.update(0, width, height, tile);
    void cssW;
    void cssH;
  }

  private syncCausticVenomVfx(dt: number, tile: number): void {
    const system = this.causticVenomVfx;
    if (!system) return;
    const requests = this.engine.causticVenomVfxRequests.splice(0);
    if (!requests.length) system.update(dt);
    for (const request of requests) {
      const caster = this.engine.units.find((unit) => unit.id === request.casterId && unit.alive);
      const destination = this.engine.effectAnchor(request.target.x, request.target.y);
      if (!caster || (this.engine.fogged && !this.engine.visible(request.target.x, request.target.y)) || (this.engine.fogged && !this.engine.visible(caster.x, caster.y))) {
        this.engine.causticVenomVfxEvents.push({ id: request.id, phase: "impact" }, { id: request.id, phase: "complete" });
        continue;
      }
      const visual = this.engine.unitVisual(caster, tile);
      const anchor = this.engine.unitAnchor(caster);
      const direction = caster.facing < 0 ? -1 : 1;
      const handY = anchor.worldY + visual.footY + visual.bob - visual.lift + (visual.footOffset - visual.h * 0.44) * visual.scaleY;
      const origin = new THREE.Vector3(
        anchor.worldX + visual.sway + direction * visual.w * visual.scaleX * 0.27,
        -handY,
        1.1 + (handY / tile) * 0.004 + tile * 0.01,
      );
      if (caster.classId === "zombieDog" && visual.img) {
        // The Zombie Dog spits from the open muzzle on its cast frame, not the generic hand
        // point above. These normalized canvas coordinates are the mouth center on cast-1.png.
        const mouthX = 0.895;
        const mouthY = 0.52;
        const footX = anchor.worldX + this.cameraRight.x * visual.sway + this.cameraGroundDown.x * visual.footY;
        const footY = -anchor.worldY + this.cameraRight.y * visual.sway + this.cameraGroundDown.y * visual.footY;
        const groundLift = this.engine.tacticsCamera ? this.landscape?.heightAt(footX, footY) ?? 0 : visual.lift;
        const centerYLocal = (visual.footOffset - visual.h / 2) * visual.scaleY * this.cameraSpriteScale;
        const base = artBase(visual.img);
        const transparentFootPadding = (1 - (base?.v ?? 1)) * visual.h * visual.scaleY * this.cameraSpriteScale;
        const visualUpOffset = (this.engine.tacticsCamera ? -transparentFootPadding : groundLift) - visual.bob - centerYLocal;
        const center = new THREE.Vector3(
          footX + this.unitUp.x * visualUpOffset,
          footY + this.unitUp.y * visualUpOffset,
          spriteDepthZ(anchor.worldY + visual.footY, tile) + UNIT_DEPTH_TIE + this.unitUp.z * visualUpOffset,
        );
        origin.copy(center)
          .addScaledVector(this.cameraRight, (mouthX - 0.5) * visual.w * visual.scaleX * this.cameraSpriteScale)
          .addScaledVector(this.unitUp, (0.5 - mouthY) * visual.h * visual.scaleY * this.cameraSpriteScale);
      }
      const impactHexes = request.tiles.map((cell) => {
        const cellAnchor = this.engine.effectAnchor(cell.x, cell.y);
        const elevation = this.engine.hexElevated(cell.x, cell.y) ? cellAnchor.tile * 0.18 : 0;
        return new THREE.Vector3(
          cellAnchor.worldX,
          -(cellAnchor.worldY - elevation),
          1.08 + (cellAnchor.worldY / tile) * 0.004 + tile * 0.012,
        );
      });
      const elevation = this.engine.hexElevated(request.target.x, request.target.y) ? destination.tile * 0.18 : 0;
      const target = new THREE.Vector3(
        destination.worldX,
        -(destination.worldY - elevation),
        1.08 + (destination.worldY / tile) * 0.004 + tile * 0.012,
      );
      system.cast({
        id: request.id,
        origin,
        target,
        worldScale: tile,
        impactHexes,
        onLaunch: () => undefined,
        onImpact: () => this.engine.causticVenomVfxEvents.push({ id: request.id, phase: "impact" }),
        onComplete: () => this.engine.causticVenomVfxEvents.push({ id: request.id, phase: "complete" }),
      });
      system.update(0);
    }
  }

  private syncPhantasmalForceVfx(dt: number, tile: number): void {
    const system = this.phantasmalForceVfx;
    if (!system) return;
    const requests = this.engine.phantasmalForceVfxRequests.splice(0);
    if (!requests.length) system.update(dt);
    for (const request of requests) {
      const targetUnit = this.engine.units.find((unit) => unit.id === request.targetUnitId && unit.alive);
      const destination = this.engine.effectAnchor(request.target.x, request.target.y);
      if (!targetUnit || (this.engine.fogged && !this.engine.visible(request.target.x, request.target.y))) {
        this.engine.phantasmalForceVfxEvents.push({ id: request.id, phase: "impact" }, { id: request.id, phase: "complete" });
        continue;
      }
      const visual = this.engine.unitVisual(targetUnit, tile);
      const anchor = this.engine.unitAnchor(targetUnit);
      const centerYLocal = (visual.footOffset - visual.h / 2) * visual.scaleY;
      const groundY = anchor.worldY + visual.footY;
      const target = new THREE.Vector3(
        anchor.worldX + visual.sway,
        -(groundY + visual.bob - visual.lift + centerYLocal),
        spriteDepthZ(groundY, tile) + UNIT_DEPTH_TIE,
      );
      const elevation = this.engine.hexElevated(request.target.x, request.target.y) ? destination.tile * 0.18 : 0;
      target.y += elevation;
      system.restartAt(target, tile, {
        onImpact: () => this.engine.phantasmalForceVfxEvents.push({ id: request.id, phase: "impact" }),
        onComplete: () => this.engine.phantasmalForceVfxEvents.push({ id: request.id, phase: "complete" }),
      });
    }
    if (requests.length) system.update(0);
  }

  private syncBlessVfx(dt: number, tile: number): void {
    const system = this.blessVfx;
    if (!system) return;
    const requests = this.engine.blessVfxRequests.splice(0);
    if (!requests.length) system.update(dt);
    for (const request of requests) {
      const centerAnchor = this.engine.effectAnchor(request.center.x, request.center.y);
      const center = new THREE.Vector3(centerAnchor.worldX, -centerAnchor.worldY + tile * 0.035, spriteDepthZ(centerAnchor.worldY, tile) + UNIT_DEPTH_TIE);
      const allies = request.allies.map((ally) => {
        const unit = this.engine.units.find((candidate) => candidate.id === ally.id && candidate.alive);
        if (!unit) return null;
        const anchor = this.engine.unitAnchor(unit);
        const visual = this.engine.unitVisual(unit, tile);
        const groundY = anchor.worldY + visual.footY;
        const y = -(groundY + visual.bob - visual.lift + (visual.footOffset - visual.h * 0.36) * visual.scaleY);
        return { id: unit.id, position: new THREE.Vector3(anchor.worldX + visual.sway, y, spriteDepthZ(groundY, tile) + UNIT_DEPTH_TIE), distanceHexes: ally.distanceHexes };
      }).filter((ally): ally is NonNullable<typeof ally> => !!ally);
      system.castSpell({
        id: request.id,
        center,
        radiusWorld: BLESS.radius * Math.sqrt(3) * tile,
        allies,
        onApply: (unitId) => this.engine.blessVfxEvents.push({ id: request.id, phase: "apply", unitId }),
        onComplete: () => this.engine.blessVfxEvents.push({ id: request.id, phase: "complete" }),
        onTimelineEvent: (event, unitId) => this.engine.blessTimelineEvents.push({ id: request.id, event, unitId }),
      });
    }
    if (requests.length) system.update(0);
  }

  hasMagicMissileV2Vfx(): boolean {
    return !!this.activeMagicMissileV2VfxRequestId || this.pendingMagicMissileV2VfxRequests.length > 0 || this.engine.magicMissileV2VfxRequests.length > 0;
  }

  attachMagicMissileForeground(canvas: HTMLCanvasElement): void {
    this.magicMissileForeground?.dispose();
    this.magicMissileForeground = null;
    this.magicMissileForegroundCanvas = canvas;
    this.magicMissileV2Vfx?.setRenderLayer(MagicMissileForeground.layer);
    // Build the spell's own renderer and compile its shaders now, at battle load, rather than on
    // the first cast (that first Magic Missile stalled ~0.9 s).
    this.magicMissileForeground = new MagicMissileForeground(canvas, this.scene, this.camera);
    // Its shaders are warmed with every other spell's on the first frame (warmMeleeVfxOnce).
  }

  renderMagicMissileForeground(cssW: number, cssH: number): void {
    this.syncSpellVfxLayers();
    const active = this.hasMagicMissileV2Vfx() || this.hasVisibleSpellVfx();
    if (active && !this.magicMissileForeground && this.magicMissileForegroundCanvas) {
      this.magicMissileForeground = new MagicMissileForeground(this.magicMissileForegroundCanvas, this.scene, this.camera);
    }
    // Same light treatment as the main draw (see render()), so this pass always sees the same
    // light count its shaders were compiled for, and never recompiles on a first cast.
    const parked = this.parkHiddenLights();
    this.balanceLightCount();
    try {
      this.magicMissileForeground?.render(this.scene, this.camera, cssW, cssH, this.renderer.getPixelRatio(), active, this.bloomPass);
    } finally {
      this.restoreParkedLights(parked);
    }
  }

  private hasVisibleSpellVfx(): boolean {
    let active = false;
    this.spellVfxScene.traverse((object) => {
      if (active || (!(object instanceof THREE.Mesh) && !(object instanceof THREE.Line) && !(object instanceof THREE.Points) && !(object instanceof THREE.Sprite))) return;
      let parent: THREE.Object3D | null = object;
      while (parent && parent !== this.spellVfxScene) {
        if (!parent.visible) return;
        parent = parent.parent;
      }
      if (parent === this.spellVfxScene && this.spellVfxScene.visible) active = true;
    });
    return active;
  }

  private syncSpellVfxLayers(): void {
    // Three.js does not inherit layer masks. This also catches meshes created asynchronously
    // during a live cast, such as Burning Hands' fire flipbook emitter.
    this.spellVfxScene.traverse((object) => {
      object.layers.set(SPELL_VFX_LAYER);
      // Light budgets must match in both passes. Spell geometry stays in the foreground,
      // while its lights also illuminate terrain and actors in the main pass.
      if (object instanceof THREE.Light) object.layers.enable(0);
    });
  }

  private syncMagicMissileV2Vfx(dt: number, tile: number): void {
    const system = this.magicMissileV2Vfx;
    if (!system) return;
    this.pendingMagicMissileV2VfxRequests.push(...this.engine.magicMissileV2VfxRequests.splice(0));
    if (this.activeMagicMissileV2VfxRequestId) system.update(dt);

    // Multiple target shots can be emitted in one engine frame. Never call castSpell for each
    // request at once: castSpell reuses one pooled effect and the next call would erase the one
    // before it. Finish each hero missile (including its impact light) before starting the next.
    while (!this.activeMagicMissileV2VfxRequestId && this.pendingMagicMissileV2VfxRequests.length) {
      const request = this.pendingMagicMissileV2VfxRequests.shift()!;
      this.activeMagicMissileV2VfxRequestId = request.id;
      if (!this.startMagicMissileV2Vfx(system, request, tile)) continue;
      system.update(0);
    }
  }

  private syncBurningHandsV2Vfx(dt: number, tile: number): void {
    for (const request of this.engine.burningHandsV2VfxRequests.splice(0)) {
      if (!this.startBurningHandsV2Vfx(request, tile)) {
        this.engine.burningHandsV2VfxEvents.push({ id: request.id, phase: "release" }, { id: request.id, phase: "complete" });
      }
    }
    for (let i = this.burningHandsVfx.length - 1; i >= 0; i--) {
      const effect = this.burningHandsVfx[i]!;
      effect.update(dt);
      if (effect.finished) this.burningHandsVfx.splice(i, 1);
    }
  }

  private startBurningHandsV2Vfx(request: BurningHandsV2VfxRequest, tile: number): boolean {
    const caster = this.engine.units.find((unit) => unit.id === request.casterId && unit.alive);
    if (!caster || request.tiles.length === 0) return false;
    const casterAnchor = this.engine.unitAnchor(caster);
    const visual = this.engine.unitVisual(caster, tile);
    const groundY = casterAnchor.worldY + visual.footY;
    const origin = new THREE.Vector3(
      casterAnchor.worldX + visual.sway + (caster.facing < 0 ? -1 : 1) * visual.w * visual.scaleX * 0.24,
      -(groundY + visual.bob - visual.lift + (visual.footOffset - visual.h * 0.54) * visual.scaleY),
      spriteDepthZ(groundY, tile) + UNIT_DEPTH_TIE + tile * 0.09,
    );
    const cells = request.tiles.map((cell) => this.engine.effectAnchor(cell.x, cell.y));
    const avgX = cells.reduce((sum, cell) => sum + cell.worldX, 0) / cells.length;
    const avgY = cells.reduce((sum, cell) => sum - cell.worldY, 0) / cells.length;
    const axisX = avgX - origin.x;
    const axisY = avgY - origin.y;
    const length = Math.max(tile, Math.hypot(axisX, axisY));
    const direction = new THREE.Vector2(axisX, axisY).normalize();
    let halfWidth = tile * 0.38;
    for (const cell of cells) {
      const relX = cell.worldX - origin.x;
      const relY = -cell.worldY - origin.y;
      halfWidth = Math.max(halfWidth, Math.abs(-direction.y * relX + direction.x * relY));
    }
    const effect = new BurningHandsV2VFX(this.spellVfxScene, {
      poison: request.poison,
      id: request.id,
      origin,
      direction,
      length,
      width: halfWidth * 2,
      worldScale: tile,
      // Burning Hands must keep its primary fire visible in combat; flame-only diagnostic
      // modes in the FX Lab must never leave the actual spell visually disabled.
      settings: { ...getActiveBurningHandsV2Settings(), visuals: true },
      targetPositions: cells.map((cell) => new THREE.Vector3(cell.worldX, -cell.worldY, origin.z + tile * 0.12)),
      onRelease: () => this.engine.burningHandsV2VfxEvents.push({ id: request.id, phase: "release" }),
      onComplete: () => this.engine.burningHandsV2VfxEvents.push({ id: request.id, phase: "complete" }),
    });
    this.burningHandsVfx.push(effect);
    return true;
  }

  private startMagicMissileV2Vfx(system: MagicMissileV2VFX, request: MagicMissileV2VfxRequest, tile: number): boolean {
    const caster = this.engine.units.find((unit) => unit.id === request.casterId && unit.alive);
    const target = this.engine.units.find((unit) => unit.id === request.targetUnitId && unit.alive);
    if (!caster || !target || (this.engine.fogged && !this.engine.visible(target.x, target.y))) {
      this.engine.magicMissileV2VfxEvents.push({ id: request.id, phase: "impact", index: 0 }, { id: request.id, phase: "complete" });
      this.activeMagicMissileV2VfxRequestId = null;
      return false;
    }
    const casterAnchor = this.engine.unitAnchor(caster);
    const casterVisual = this.engine.unitVisual(caster, tile);
    const casterGroundY = casterAnchor.worldY + casterVisual.footY;
    const casterCenterY = (casterVisual.footOffset - casterVisual.h * 0.38) * casterVisual.scaleY;
    const targetAnchor = this.engine.unitAnchor(target);
    const targetVisual = this.engine.unitVisual(target, tile);
    const targetGroundY = targetAnchor.worldY + targetVisual.footY;
    const targetCenterY = (targetVisual.footOffset - targetVisual.h * 0.48) * targetVisual.scaleY;
    // Track the ground-row depth along the shot. A single depth chosen from the nearer
    // endpoint makes the missile pass behind the caster when Voss stands on a farther row.
    // Keep each endpoint just in front of that character; the curved trajectory then
    // interpolates depth naturally between the two planes.
    const casterMissileDepth = spriteDepthZ(casterGroundY, tile) + UNIT_DEPTH_TIE + 0.01;
    const targetMissileDepth = spriteDepthZ(targetGroundY, tile) + UNIT_DEPTH_TIE + 0.01;
    const origin = new THREE.Vector3(
      casterAnchor.worldX + casterVisual.sway,
      -(casterGroundY + casterVisual.bob - casterVisual.lift + casterCenterY),
      casterMissileDepth,
    );
    const destination = new THREE.Vector3(
      targetAnchor.worldX + targetVisual.sway,
      -(targetGroundY + targetVisual.bob - targetVisual.lift + targetCenterY),
      targetMissileDepth,
    );
    system.castSpell({
      id: request.id,
      origin,
      target: destination,
      // One complete hero missile belongs to each queued target shot; damage stays per shot.
      missileCount: 1,
      worldScale: tile,
      onImpact: (index) => this.engine.magicMissileV2VfxEvents.push({ id: request.id, phase: "impact", index }),
      onComplete: () => {
        this.engine.magicMissileV2VfxEvents.push({ id: request.id, phase: "complete" });
        if (this.activeMagicMissileV2VfxRequestId === request.id) this.activeMagicMissileV2VfxRequestId = null;
      },
      onTimelineEvent: (event, index) => this.engine.magicMissileV2TimelineEvents.push({ id: request.id, event, index }),
    });
    return true;
  }

  /** Render the bloom buffer while temporarily omitting the visible art of map light sources.
   * The lights themselves stay in the scene, so this changes post-processing only—not the
   * actual 3D illumination, shadows, or the normal final render. */
  private renderBloomWithoutLightSourceArt(): void {
    const hidden: THREE.Mesh[] = [];
    // Houses that burn (fogCut marks a house) are drawn black instead of hidden: hiding one let
    // the bloom buffer show the ground behind it, which the mix pass then added over the house,
    // making the whole building look see-through. Black still adds no bloom of its own.
    const blacked: { mesh: THREE.Mesh; material: THREE.Material | THREE.Material[] }[] = [];
    const boostedHalos: { mesh: THREE.Mesh; color: THREE.Color; opacity: number }[] = [];
    for (const entry of this.decorEntries) {
      if (entry.halo?.visible) {
        const material = entry.halo.material as THREE.MeshBasicMaterial;
        boostedHalos.push({ mesh: entry.halo, color: material.color.clone(), opacity: material.opacity });
        // Let the actual flame halo pass the highlight threshold; only the bloom buffer is
        // boosted. The normal battle render keeps the original warm, soft halo unchanged.
        material.color.multiplyScalar(2);
        material.opacity = 1;
      }
      if (!entry.light || !entry.mesh.visible) continue;
      if (entry.fogCut) {
        blacked.push({ mesh: entry.mesh, material: entry.mesh.material });
        entry.mesh.material = this.bloomBlackMaterialFor(entry.mesh.material as THREE.MeshLambertMaterial);
        continue;
      }
      entry.mesh.visible = false;
      hidden.push(entry.mesh);
    }
    // Map lights never feed bloom: the bloom buffer is rendered unlit by them, so a candle's
    // bright pool on the floor can't swell into a white disc over everyone standing near it.
    // Only strong target/turn cues feed the silver glow; the blue range stays quiet.
    const mutedOverlays = this.overlayMeshPool.filter((mesh) => mesh.visible && (mesh.material as THREE.MeshBasicMaterial).opacity < 0.5);
    for (const mesh of mutedOverlays) mesh.visible = false;
    const intensities = this.pointLights.map((pl) => pl.intensity);
    for (const pl of this.pointLights) pl.intensity = 0;
    const bounceIntensities = this.bounceLights.map((bl) => bl.intensity);
    for (const bl of this.bounceLights) bl.intensity = 0;
    try {
      this.bloomComposer.render();
    } finally {
      for (const mesh of mutedOverlays) mesh.visible = true;
      for (const mesh of hidden) mesh.visible = true;
      for (const b of blacked) b.mesh.material = b.material;
      for (const { mesh, color, opacity } of boostedHalos) {
        const material = mesh.material as THREE.MeshBasicMaterial;
        material.color.copy(color);
        material.opacity = opacity;
      }
      this.pointLights.forEach((pl, i) => (pl.intensity = intensities[i]!));
      this.bounceLights.forEach((bl, i) => (bl.intensity = bounceIntensities[i]!));
    }
  }

  private decorOccluderMatCache = new Map<string, THREE.MeshBasicMaterial>();
  /** Depth-only cutout of a prop's art — see DEPTH_Z_BASE. */
  private decorOccluderMaterialFor(fileId: string, colorMat: THREE.MeshLambertMaterial): THREE.MeshBasicMaterial {
    const hit = this.decorOccluderMatCache.get(fileId);
    if (hit) return hit;
    const mat = occluderMaterial(colorMat.map);
    this.decorOccluderMatCache.set(fileId, mat);
    return mat;
  }
  private unitOccluderMatCache = new Map<THREE.Texture, THREE.MeshBasicMaterial>();
  private unitOccluderMaterialFor(tex: THREE.Texture): THREE.MeshBasicMaterial {
    const hit = this.unitOccluderMatCache.get(tex);
    if (hit) return hit;
    const mat = occluderMaterial(tex);
    this.unitOccluderMatCache.set(tex, mat);
    return mat;
  }
  /** Where each visible unit's feet are (scene space), for Fog 5's boots-in-mist veil. Read-only
   * over the unit meshes syncUnits already positioned this frame. */
  private unitFeetForMist(): { x: number; y: number; halfW: number; band: number }[] {
    const feet: { x: number; y: number; halfW: number; band: number }[] = [];
    for (const entry of this.unitEntries.values()) {
      if (!entry.mesh.visible || !entry.fogCut.visible) continue;
      const sy = Math.abs(entry.mesh.scale.y);
      const sx = Math.abs(entry.mesh.scale.x);
      feet.push({ x: entry.mesh.position.x, y: entry.mesh.position.y - sy * 0.42, halfW: sx * 0.3, band: sy * 0.28 });
    }
    return feet;
  }
  private unitFogCutMatCache = new Map<THREE.Texture, THREE.MeshBasicMaterial>();
  /** Depth-only twin of a unit's sprite for its fogCut — same recipe as decorFogCutMaterialFor. */
  private unitFogCutMaterialFor(tex: THREE.Texture): THREE.MeshBasicMaterial {
    const hit = this.unitFogCutMatCache.get(tex);
    if (hit) return hit;
    const mat = new THREE.MeshBasicMaterial({ map: tex, alphaTest: 0.5, colorWrite: false, depthWrite: true, transparent: true });
    this.unitFogCutMatCache.set(tex, mat);
    return mat;
  }

  /** Black silhouette of a decoration's art (same texture alpha) for the bloom-only pass. */
  private bloomBlackMatCache = new Map<THREE.Material, THREE.MeshBasicMaterial>();
  private bloomBlackMaterialFor(colorMat: THREE.MeshLambertMaterial): THREE.MeshBasicMaterial {
    const hit = this.bloomBlackMatCache.get(colorMat);
    if (hit) return hit;
    const mat = new THREE.MeshBasicMaterial({ map: colorMat.map, color: 0x000000, transparent: true, depthWrite: false });
    this.bloomBlackMatCache.set(colorMat, mat);
    return mat;
  }

  /** Dreaming Web floor: mirrors Canvas2D renderGround's webZones block — each explored cell
   * of a live zone, once WEB_SHOT_TRAVEL has passed since the cast (the shot has landed), at
   * 0.38 opacity where the cell is explored but not currently in sight. */
  private syncWebZones(tile: number): void {
    const engine = this.engine;
    const img = engine.art.webfloor;
    let n = 0;
    if (img && img.naturalWidth > 0) {
      if (!this.webMat) {
        const tex = new THREE.Texture(img);
        tex.needsUpdate = true;
        tex.colorSpace = THREE.SRGBColorSpace;
        this.webMat = new THREE.MeshLambertMaterial({ map: tex, transparent: true, depthWrite: false });
        this.webMatDim = new THREE.MeshLambertMaterial({ map: tex, transparent: true, depthWrite: false, opacity: 0.38 });
      }
      for (const zone of engine.webZones) {
        if (zone.createdAt != null && engine.time < zone.createdAt + WEB_SHOT_TRAVEL) continue;
        for (const k of zone.cells) {
          const comma = k.indexOf(",");
          const x = Number(k.slice(0, comma));
          const y = Number(k.slice(comma + 1));
          if (!Number.isFinite(x) || !Number.isFinite(y) || !engine.explored(x, y)) continue;
          let mesh = this.webMeshes[n];
          if (!mesh) {
            mesh = new THREE.Mesh(this.hexGeo, this.webMat);
            this.webMeshes.push(mesh);
            this.webGroup.add(mesh);
          }
          mesh.material = engine.visible(x, y) ? this.webMat : this.webMatDim!;
          const { wx, wy } = this.boardWorld(x, y, tile);
          mesh.scale.set(tile * 2, tile * 2, 1);
          // Above the ground (z 0), under the range highlights (0.42+).
          mesh.position.set(wx, -wy, 0.3);
          mesh.visible = true;
          n++;
        }
      }
    }
    for (let i = n; i < this.webMeshes.length; i++) this.webMeshes[i]!.visible = false;
  }

  /** Dev Controls toggles (see devGfx.ts) — read every frame so a flip applies immediately. */
  private applyDevGfx(): void {
    const gfx = getDevGfx();
    const shadowType = gfx.softShadows ? THREE.PCFShadowMap : THREE.BasicShadowMap;
    if (this.renderer.shadowMap.type !== shadowType) {
      this.renderer.shadowMap.type = shadowType;
      this.renderer.shadowMap.needsUpdate = true;
    }
    const requested = [1024, 2048, 4096].includes(gfx.shadowResolution) ? gfx.shadowResolution : 4096;
    const resolution = Math.min(requested, this.renderer.capabilities.maxTextureSize);
    for (const light of [this.sunLight, this.moonLight, ...this.pointLights]) {
      light.shadow.intensity = gfx.softShadows ? 0.55 : 1;
      const size = light instanceof THREE.PointLight ? Math.max(256, resolution / 4) : resolution;
      if (light.shadow.mapSize.x === size) continue;
      light.shadow.mapSize.set(size, size);
      light.shadow.map?.dispose(); light.shadow.map = null;
      light.shadow.mapPass?.dispose(); light.shadow.mapPass = null;
      light.shadow.needsUpdate = true;
    }
    this.sunLight.shadow.radius = gfx.softShadows ? SHADOW_RADIUS_SOFT : SHADOW_RADIUS_HARD;
    this.contactShadowGroup.visible = this.engine.tacticsCamera && gfx.contactShadows && gfx.realShadows;
    this.decorContactGroup.visible = false;
    this.groundAO.setEnabled(gfx.ambientOcclusion);
    this.groundAO.uniforms.groundContactStrength.value = gfx.contactShadows ? 1 : 0;
  }

  /** Occluders for the ground AO field: every decoration footprint cell, plus raised/blocking
   * terrain (hill, column, barricade, door — not water or the void edge trim, which are low or
   * empty, not surrounding geometry). Rebuilt only when the map/terrain/decoration set changes. */
  private syncGroundAO(tile: number): void {
    const engine = this.engine;
    const key = `${engine.mission.id}:${engine.cols}x${engine.rows}:${this.aoTerrainVersion}:${engine.decorations.length}`;
    this.groundAO.update(key, engine.cols, engine.rows, BOARD_PAD_MUL, tile, () => {
      const out: AoOccluder[] = [];
      for (let row = 0; row < engine.rows; row++) {
        for (let col = 0; col < engine.cols; col++) {
          const id = tileAt(engine.tiles, engine.cols, col, row);
          const t = TERRAIN[id];
          const { wx, wy } = this.boardWorld(col, row, 1);
          if (t.height) out.push({ x: wx, y: wy, weight: 0.7 });
          else if (!t.passable && t.blocksShot && id !== "void") out.push({ x: wx, y: wy, weight: 1 });
        }
      }
      for (const p of engine.decorations) {
        if (!DECORATIONS[p.id]) continue;
        for (const { dx, dy } of placedFootprint(p)) {
          const { wx, wy } = this.boardWorld(p.x + dx, p.y + dy, 1);
          out.push({ x: wx, y: wy, weight: 1 });
        }
      }
      return out;
    }, (x, y) => {
      const cell = this.cellAtWorld(x, y);
      if (cell < 0) return false;
      // Same set as the terrain occluders above: a hilltop or a pillar/barricade/door top
      // never occludes itself, only the ground around its foot.
      const cellId = engine.tiles[cell]!;
      const t = TERRAIN[cellId];
      return !!t.height || (!t.passable && !!t.blocksShot && cellId !== "void");
    });
  }

  /** Decorations and unit billboards are lit (MeshLambertMaterial) so real lights reach them.
   * Under the scene's sun + sky alone a lit camera-facing plane comes out brighter than the
   * unlit art it replaced, so each lit sprite's material color is set to the inverse of that
   * sun + sky lighting: with no map light nearby it looks exactly as before; next to a
   * PointLight it receives that light on top, computed by Three. Recomputed every frame since
   * sun/sky intensities are mission- and atmosphere-driven. */
  private syncSpriteExposure(): void {
    this.currentSpriteLightCap =
      this.timeOfDay === "dawn" || this.timeOfDay === "dusk"
        ? 4.5
        : this.timeOfDay === "brightNight"
          ? 7.5
          : this.timeOfDay === "darkNight"
            ? 9.5
            : SPRITE_LIGHT_CAP;
    DECOR_LIGHT_CAP.value = this.currentSpriteLightCap;
    // Keep the actual tile surface from saturating into a hex-shaped hotspot. Nearby scenery
    // and units have the higher sprite cap above, so the same real point lights read as light
    // reaching the environment instead of a bright decal painted on the source tile.
    this.groundAO.uniforms.groundLightCap.value =
      this.timeOfDay === "dawn" || this.timeOfDay === "dusk"
        ? 6
        : this.timeOfDay === "brightNight"
          ? 3.5
          : this.timeOfDay === "darkNight"
            ? 2.5
            : 10;
    // Calibrated against the mission's standing daytime sun (default direction and intensity) —
    // not the live values — so night and sun-angle changes reach the sprites as real light.
    const exposureFor = (normal: THREE.Vector3): [number, number, number] => {
      const sunNdotL = Math.max(0, -normal.dot(SUN_DIRECTION));
      const skyWeight = THREE.MathUtils.clamp(normal.y * 0.5 + 0.5, 0, 1);
      const sky = this.calibHemiColor;
      const ground = this.hemiLight.groundColor;
      const hemi = [
        ground.r + (sky.r - ground.r) * skyWeight,
        ground.g + (sky.g - ground.g) * skyWeight,
        ground.b + (sky.b - ground.b) * skyWeight,
      ];
      const sun = this.calibSunColor;
      const channel = (i: 0 | 1 | 2, sunC: number) =>
        1 / Math.max(0.05, (this.calibSunIntensity * sunNdotL * sunC + this.calibHemiIntensity * hemi[i]!) / Math.PI);
      return [channel(0, sun.r), channel(1, sun.g), channel(2, sun.b)];
    };
    const flatExposure = exposureFor(new THREE.Vector3(0, 0, 1));
    const cameraExposure = exposureFor(new THREE.Vector3(0, 0, 1).applyQuaternion(this.unitFacing));
    for (const m of this.litSpriteMats) m.color.setRGB(...flatExposure).multiplyScalar(m.userData.lightReflectance ?? 1);
    for (const m of this.unitSpriteMats) m.color.setRGB(...cameraExposure);
    if (this.engine.tacticsCamera) for (const entry of this.decorEntries) {
      const anchor = entry.mesh.userData.tacticsAnchor;
      if (anchor && !anchor.flat) {
        const exposure = anchor.fixed
          ? exposureFor(new THREE.Vector3(0, 0, 1).applyQuaternion(entry.mesh.quaternion)) : cameraExposure;
        (entry.mesh.material as THREE.MeshLambertMaterial).color.setRGB(...exposure).multiplyScalar((entry.mesh.material as THREE.MeshLambertMaterial).userData.lightReflectance ?? 1);
      }
    }
  }

  /** Per-frame: every light prop's flame, flickered, drives one real THREE.PointLight (the
   * POINT_LIGHT_POOL nearest the view). Dev Controls "Luzes do mapa" turns them all off for an
   * A/B comparison. */
  private syncLights(tile: number, cssW: number, cssH: number): void {
    const engine = this.engine;
    const out: EnvLight[] = [];
    if (!getDevGfx().localLights) for (const e of this.decorEntries) if (e.halo) e.halo.visible = false;
    if (getDevGfx().localLights) {
      const haloTime =
        this.timeOfDay === "day" || this.timeOfDay === "noon" ? HALO_DAYLIGHT : this.timeOfDay === "dawn" || this.timeOfDay === "dusk" ? HALO_TWILIGHT : 1;
      for (const e of this.decorEntries) {
        const L = e.light;
        if (!L) continue;
        if (e.halo) {
          e.halo.visible = e.mesh.visible;
          (e.halo.material as THREE.MeshBasicMaterial).opacity = HALO_STRENGTH * haloTime * flickerAt(engine.time, L.seed, L.def.flicker);
        }
        const k = L.def.intensity * flickerAt(engine.time, L.seed, L.def.flicker);
        out.push({ x: L.x, y: L.y, h: Math.max(L.h, tile * MAP_LIGHT_MIN_HEIGHT), r: L.def.radius * LIGHT_RADIUS_MUL * tile, decay: MAP_LIGHT_DECAY, normDecay: MAP_LIGHT_DECAY, rgb: [L.def.color[0] * k, L.def.color[1] * k, L.def.color[2] * k] });
      }
      // Units that carry their own light (UNIT_LIGHT_DEFS) — follows the unit's live anchor, so
      // the light walks with it; hidden (fog) or dead units give none, fading ones fade it.
      for (const u of engine.units) {
        const def = UNIT_LIGHT_DEFS[u.classId];
        if (!def || !u.alive || u.fade <= 0 || engine.unitHidden(u)) continue;
        const a = engine.unitAnchor(u);
        let seed = 0;
        for (let i = 0; i < u.id.length; i++) seed = (seed * 31 + u.id.charCodeAt(i)) % 628;
        const k = def.intensity * flickerAt(engine.time * 0.5, seed / 100, def.flicker) * Math.min(1, u.fade);
        const rgb: [number, number, number] = [def.color[0] * k, def.color[1] * k, def.color[2] * k];
        if (u.classId === "familiar3") {
          // Familiar Titã is a six-hex body: distribute its red point lights over the whole
          // footprint so its body and adjacent target area glow together. Offset from the
          // front-row anchor, which follows the interpolated sprite while it walks.
          const cells = footprint(u);
          const frontRow = footprintFrontRow(u);
          const anchorCells = frontRow.length > 0 ? frontRow : [{ x: u.x, y: u.y }];
          const anchorBase = anchorCells.reduce((sum, cell) => {
            const pos = this.boardWorld(cell.x, cell.y, tile);
            return { wx: sum.wx + pos.wx / anchorCells.length, wy: sum.wy + pos.wy / anchorCells.length };
          }, { wx: 0, wy: 0 });
          for (const cell of cells) {
            const pos = this.boardWorld(cell.x, cell.y, tile);
            out.push({
              x: a.worldX + pos.wx - anchorBase.wx,
              y: a.worldY + pos.wy - anchorBase.wy,
              h: tile,
              r: def.radius * tile,
              decay: MAP_LIGHT_DECAY,
              normDecay: MAP_LIGHT_DECAY,
              rgb,
            });
          }
        } else if (u.classId === "familiar4") {
          // Familiar Radiante's reversed Type 3 head and tail occupy the two upper neighboring
          // hexes. Keep a radius-2 pool on each; the offsets follow its interpolated movement.
          out.push({ x: a.worldX, y: a.worldY, h: tile, r: tile * 3, decay: MAP_LIGHT_DECAY, normDecay: MAP_LIGHT_DECAY, rgb });
          const base = this.boardWorld(u.x, u.y, tile);
          const upperCells = hexNeighbors(u.x, u.y)
            .filter((neighbor) => neighbor.y < u.y)
            .map((cell) => this.boardWorld(cell.x, cell.y, tile));
          for (const upperCell of upperCells) {
            out.push({
              x: a.worldX + upperCell.wx - base.wx,
              y: a.worldY + upperCell.wy - base.wy,
              h: tile,
              r: tile * 2,
              decay: MAP_LIGHT_DECAY,
              normDecay: MAP_LIGHT_DECAY,
              rgb,
            });
          }
        } else {
          out.push({ x: a.worldX, y: a.worldY, h: tile, r: def.radius * tile, decay: MAP_LIGHT_DECAY, normDecay: MAP_LIGHT_DECAY, rgb });
        }
      }
      // Procedural Pixel emitters feed the same pooled PointLights as map props and units.
      // Their model is never rendered as a separate light, avoiding duplicate bright spots.
      for (const entry of this.pixelElementEmitters) {
        const anchor = engine.effectAnchor(entry.placement.x, entry.placement.y);
        const sample = entry.emitter.getLightSample(anchor.worldX, anchor.worldY, tile);
        if (sample) out.push(sample);
      }
      // Nearest the view first: they win the pool.
      const cx = engine.camX + cssW / 2;
      const cy = engine.camY + cssH / 2;
      out.sort((a, b) => (a.x - cx) ** 2 + (a.y - cy) ** 2 - ((b.x - cx) ** 2 + (b.y - cy) ** 2));
    }
    // A real PointLight at each flame: X/Y = the flame's ground position (Y negated, the scene's
    // Y-flip), Z = the flame's height above the board (+Z is up toward the camera, the board is
    // the z=0 plane). Intensity is scaled so irradiance one hex radius away is
    // LightDef.intensity x the ground's normal sun + sky irradiance.
    this.pointLights.forEach((pl, i) => {
      const L = out[i];
      if (!L) {
        pl.intensity = 0;
        return;
      }
      const peak = Math.max(L.rgb[0], L.rgb[1], L.rgb[2], 1e-6);
      pl.color.setRGB(L.rgb[0] / peak, L.rgb[1] / peak, L.rgb[2] / peak);
      pl.intensity = peak * GROUND_BASE_IRRADIANCE * Math.pow(tile, L.normDecay ?? LIGHT_DECAY);
      pl.decay = L.decay ?? LIGHT_DECAY;
      pl.position.set(L.x, -L.y, Math.max(L.h, tile * 0.35));
      // Range is measured in 3D from the flame, so it has to include the flame's height to still
      // reach L.r out along the ground. (Three also sets the point-shadow camera's far plane to
      // this distance — ground past it would read as shadowed.)
      pl.distance = Math.hypot(L.r, pl.position.z) * 1.05;
      if (pl.castShadow && pl.shadow.camera.near !== tile * POINT_SHADOW_NEAR) {
        pl.shadow.camera.near = tile * POINT_SHADOW_NEAR;
        pl.shadow.camera.updateProjectionMatrix();
      }
    });
    // Bounce fill for the nearest map lights (props and unit lights; spell/pixel emitters keep
    // their own look). decay 1, normalized so irradiance straight under it is BOUNCE_FRACTION of
    // the main light's one-hex irradiance: E(0) = I / z, so I = fraction x E1 x z.
    const bounced = out.filter((L) => L.normDecay !== undefined);
    const bh = tile * BOUNCE_HEIGHT;
    this.bounceLights.forEach((bl, i) => {
      const L = bounced[i];
      if (!L) {
        bl.intensity = 0;
        return;
      }
      const peak = Math.max(L.rgb[0], L.rgb[1], L.rgb[2], 1e-6);
      bl.color.setRGB(L.rgb[0] / peak, L.rgb[1] / peak, L.rgb[2] / peak);
      bl.intensity = BOUNCE_FRACTION * peak * GROUND_BASE_IRRADIANCE * bh;
      bl.position.set(L.x, -L.y, bh);
      bl.distance = Math.hypot(L.r * BOUNCE_RADIUS_MUL, bh) * 1.05;
    });
  }

  /** Adds real world illumination to healing spells without changing their existing 2D art. */
  private syncHealingSpellLight(tile: number): void {
    const engine = this.engine;
    const target = engine.units
      .filter((u) => u.alive && u.healGlow > 0 && HEALING_SPELL_GLOW_KINDS.has(u.healGlowKind) && !engine.unitHidden(u))
      .sort((a, b) => b.healGlow - a.healGlow)[0];
    const light = this.healingSpellLight;
    if (!target) {
      light.intensity = 0;
      return;
    }
    const anchor = engine.unitAnchor(target);
    const [r, g, b] = engine.healHaloRgb(target.healGlowKind).core.split(",").map((channel) => Number(channel) / 255);
    light.color.setRGB(r!, g!, b!, THREE.SRGBColorSpace);
    light.position.set(anchor.worldX, -(anchor.worldY - tile * 0.8), tile * 0.35);
    light.distance = tile * 1.8;
    light.intensity = 0.42 * target.healGlow * GROUND_BASE_IRRADIANCE * Math.pow(tile, LIGHT_DECAY);
  }

  /** The board cell (row-major index) a tile-normalized world point (hexWorld units, y-down)
   * lies in — nearest hex center, which is exactly the hex tiling — or -1 off the board. */
  private cellAtWorld(x: number, y: number): number {
    if (this.engine.mission.squareTiles) return this.cellAtSquareWorld(x, y);
    const engine = this.engine;
    const row0 = Math.round((y - BOARD_PAD_MUL - 1) / 1.5);
    let best = Infinity;
    let bc = -1;
    let br = -1;
    for (let row = row0 - 1; row <= row0 + 1; row++) {
      const col0 = Math.round(x / SQRT3 - 0.5 * (row & 1) - 0.5);
      for (let col = col0 - 1; col <= col0 + 1; col++) {
        const c = this.boardWorld(col, row, 1);
        const d = (c.wx - x) ** 2 + (c.wy - y) ** 2;
        if (d < best) {
          best = d;
          bc = col;
          br = row;
        }
      }
    }
    // Beyond one hex radius from the nearest center is off the board's outer edge.
    if (bc < 0 || br < 0 || bc >= engine.cols || br >= engine.rows || best > 1) return -1;
    return br * engine.cols + bc;
  }

  /** Square, row-banded cell lookup for the continuous landscape's cutout mask.
   * Terrain artwork still sits on the hex board; only the map's outside contour uses
   * orthogonal edges, preventing the ground mesh from tracing a row of hexes. */
  private cellAtSquareWorld(x: number, y: number): number {
    const engine = this.engine;
    const row = Math.floor((y - BOARD_PAD_MUL - 0.25) / 1.5);
    const col = Math.floor(x / SQRT3);
    if (col < 0 || row < 0 || col >= engine.cols || row >= engine.rows) return -1;
    return row * engine.cols + col;
  }

  /** Fog of war overlay (see ThreeFogMask.ts) — rebuilt only when the engine's visibility
   * grid changes (visVersion), the debug view is toggled, the board changes, or a player unit
   * steps to a new hex (see clearAround/partyKey below). */
  private syncFog(tile: number): void {
    const engine = this.engine;
    if (!engine.fogged) {
      this.fogMask.hide();
      return;
    }
    const debug = getDevGfx().fogDebug;
    const cols = engine.cols;
    // No fog-of-war within one hex of any living player character, ever — not merely a unit
    // drawn in front of it (see the per-unit fogCut in syncUnits, a separate sprite-level fix):
    // the darkened fill itself must not exist right beside where the party actually is.
    const clearAround = new Set<number>();
    for (const u of engine.units) {
      if (u.side !== "player" || !u.alive) continue;
      for (const p of footprint(u)) {
        for (const n of [p, ...hexNeighbors(p.x, p.y)]) {
          if (n.x >= 0 && n.y >= 0 && n.x < cols && n.y < engine.rows) clearAround.add(n.y * cols + n.x);
        }
      }
    }
    const partyKey = engine.units
      .filter((u) => u.side === "player" && u.alive)
      .map((u) => `${u.x},${u.y}`)
      .join("|");
    const key = `${engine.mission.id}:${cols}x${engine.rows}:${engine.visVersion}:${debug ? 1 : 0}:${partyKey}:${engine.tacticsCamera}`;
    this.fogMask.update(key, cols, engine.rows, BOARD_PAD_MUL, tile, debug, (x, y) => {
      const cell = this.cellAtWorld(x, y);
      return engine.tacticsCamera && cell < 0 ? cols * engine.rows : cell;
    }, (i) => {
      if (engine.tacticsCamera && (i >= cols * engine.rows || engine.tiles[i] === "void")) return FOG_VISIBLE;
      if (clearAround.has(i)) return FOG_VISIBLE;
      const x = i % cols;
      const y = (i - x) / cols;
      return engine.visible(x, y) ? FOG_VISIBLE : engine.explored(x, y) ? FOG_EXPLORED : FOG_UNSEEN;
    });
    // The legacy elevated mask slides away from the board when viewed obliquely,
    // revealing a terrain rim around unexplored cells. Keep tactical fog at ground level.
    if (engine.tacticsCamera) this.fogMask.mesh.position.z = 0.6;
  }

  dispose(): void {
    for(const pulse of this.blizzardLightPulses){pulse.light.removeFromParent();pulse.light.dispose();}
    this.blizzardLightPulses.length=0;
    this.water.dispose();
    this.elevationSteps.dispose();
    this.disposed = true;
    this.pendingMagicMissileV2VfxRequests.length = 0;
    this.activeMagicMissileV2VfxRequestId = null;
    this.engine.fireballVfxAvailable = false;
    this.engine.causticVenomVfxAvailable = false;
    this.engine.phantasmalForceVfxAvailable = false;
    this.engine.blessVfxAvailable = false;
    this.engine.magicMissileV2VfxAvailable = false;
    this.engine.burningHandsV2VfxAvailable = false;
    this.fireballVfx?.dispose();
    this.fireballVfx = null;
    this.causticVenomVfx?.dispose();
    this.causticVenomVfx = null;
    this.phantasmalForceVfx?.dispose();
    this.phantasmalForceVfx = null;
    this.blessVfx?.dispose();
    this.blessVfx = null;
    this.magicMissileV2Vfx?.dispose();
    this.magicMissileForeground?.dispose();
    this.magicMissileForeground = null;
    this.magicMissileForegroundCanvas = null;
    this.magicMissileV2Vfx = null;
    this.webOfDreamsVfx?.dispose();
    this.webOfDreamsVfx = null;
    for (const effect of this.burningHandsVfx) effect.dispose();
    this.burningHandsVfx.length = 0;
    for (const entry of this.pixelElementEmitters) entry.emitter.dispose();
    this.pixelElementEmitters.length = 0;
    for (const effect of this.varreduraVfx) effect.dispose();
    this.varreduraVfx.length = 0;
    for (const effect of this.cleaveVfx) effect.dispose();
    this.cleaveVfx.length = 0;
    // MILESTONE 4 — EffectComposer.dispose() only frees its own two ping-pong render targets and
    // internal copy pass, NOT the passes added to it — bloomPass owns several render targets of
    // its own (bright-pass + per-mip horizontal/vertical blur buffers) that leak without this.
    this.bloomComposer.dispose();
    this.finalComposer.dispose();
    this.bloomPass.dispose();
    this.atmosphere.dispose();
    this.hexGeo.dispose();
    this.terrainSolid?.geometry.dispose();
    this.landscapeTexture?.dispose();
    this.landscapeMaterial?.dispose();
    for (const mesh of [...this.overlayMeshPool, ...this.gridGlowMeshes]) (mesh.userData.surfaceGeometry as THREE.BufferGeometry | undefined)?.dispose();
    this.cliffMaterial?.dispose();
    for (const entry of this.decorEntries) {
      if (entry.mesh.userData.importedTree) {
        (entry.mesh.material as THREE.Material).dispose();
      } else if (entry.mesh.userData.tacticsModel) {
        entry.mesh.geometry.dispose();
        const materials = Array.isArray(entry.mesh.material) ? entry.mesh.material : [entry.mesh.material];
        materials.forEach(material => material.dispose());
      } else if (entry.mesh.userData.tacticsCard) (entry.mesh.material as THREE.Material).dispose();
    }
    this.trees.dispose();
    this.quadGeo.dispose();
    this.backdropGeometry.dispose();
    this.backdropMaterial.dispose();
    this.backdropTexture?.dispose();
    this.fallbackMaterial.dispose();
    this.groundAO.dispose();
    this.fogMask.dispose();
    this.proxyBox.dispose();
    this.proxyCylinder.dispose();
    this.portalTexture?.dispose();
    this.portalMesh?.geometry.dispose();
    this.portalBasicMaterial?.dispose();
    this.portalWarpMaterial?.dispose();
    this.flameHaloTexture.dispose();
    this.proxyMaterial.dispose();
    for (const mat of this.materialCache.values()) {
      mat.map?.dispose();
      mat.dispose();
    }
    for (const mat of this.decorMatCache.values()) {
      mat.map?.dispose();
      mat.dispose();
    }
    // decorShadowMatCache/unit shadowMaterial share their texture with decorMatCache/unitTexCache
    // (see decorShadowMaterialFor's/the shadow-material creation's own comment) — the texture is
    // already disposed above/below, so only the material itself needs disposing here.
    for (const mat of this.decorShadowMatCache.values()) mat.dispose();
    for (const tex of this.unitTexCache.values()) tex.dispose();
    for (const tex of this.unitFootShadowTextures.values()) tex.dispose();
    this.unitFootShadowTextures.clear();
    for (const mask of this.unitContactMasks.values()) mask.texture.dispose();
    this.unitContactMasks.clear();
    for (const tex of this.glowTexCache.values()) tex.dispose();
    this.webMat?.map?.dispose();
    this.webMat?.dispose();
    this.webMatDim?.dispose();
    for (const entry of this.unitEntries.values()) {
      entry.material.dispose();
      entry.glowMaterial.dispose();
      entry.shadowMaterial.dispose();
      entry.contactMaterial.dispose();
    }
    for (const mat of this.overlayMatCache.values()) mat.dispose();
    for (const mat of this.gridGlowMatCache.values()) mat.dispose();
    for (const mesh of this.overlayMeshPool) (mesh.userData.cursorMaterial as THREE.Material | undefined)?.dispose();
    this.focusBorderGeo.dispose();
    this.contactShadowTexture.dispose();
    this.decorContactMaterial.dispose();
    this.renderer.dispose();
    for (const texture of this.wallTextures.values()) texture.dispose();
    this.wallTextures.clear();
    this.engine.architectureRenderedInThree = false;
    for (const entry of this.wallEntries) {
      entry.mesh.geometry.dispose();
      entry.mesh.material.dispose();
      entry.shadowMesh.geometry.dispose();
      entry.shadowMesh.material.dispose();
    }
  }
}
