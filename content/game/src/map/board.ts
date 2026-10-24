// The board: one cell per hex, built straight from an Ember map draft.
// Elevation follows Ember's rule: level = max(authored terrainElevations, terrain's own height).

import { type HexLayout, makeLayout, hexX, hexZ, indexOf, STEP } from '../core/hex';
import type { EmberMapDraft, TerrainId } from './emberMap';
import { CLAUDE_VARIANT, ENGINE2_V3_GROUND_VARIANTS, ENGINE2_V3_CITY_VARIANT, GROUND_VARIANT_LAYERS, groundVariantLayer } from '../ember/tileVariants';

/** Ground surfaces the terrain material knows how to paint. */
export const SURFACE = { grass: 0, forest: 1, pavers: 2, snow: 3, rock: 4, gravel: 5, ash: 6, plains049: 7 } as const;
export type Surface = keyof typeof SURFACE | 'void';

/** The 12 baked ground tiles (Claude's and GPT's sets): one layer per basic Ember terrain. */
export const GROUND_LAYERS: TerrainId[] = ['plains', 'woods', 'ruins', 'water', 'ember', 'hill', 'flame', 'column', 'nave', 'barricade', 'door', 'snow'];
/** V3 adds City plus two water variants: shallows and a rocky ford bed. */
export const GROUND_LAYER_COUNT = GROUND_LAYERS.length + 3 + GROUND_VARIANT_LAYERS.length;
export const GROUND_WEIGHT_ATTRIBUTES = Array.from({ length: Math.ceil(GROUND_LAYER_COUNT / 4) }, (_, i) => `ground${String.fromCharCode(65 + i)}`);

export interface TerrainInfo {
  name: string;
  surface: Surface;
  passable: boolean;
  /** Built-in minimum level (Ember: hill has height 1). */
  minLevel?: number;
  /** Extra levels the ground rises (a stone column stands tall). */
  raise?: number;
}

export const TERRAIN_INFO: Record<TerrainId, TerrainInfo> = {
  plains: { name: 'Planície', surface: 'grass', passable: true },
  woods: { name: 'Bosque', surface: 'forest', passable: true },
  ruins: { name: 'Ruínas', surface: 'pavers', passable: true },
  water: { name: 'Água', surface: 'gravel', passable: false },
  ember: { name: 'Brasa', surface: 'ash', passable: true },
  hill: { name: 'Colina', surface: 'grass', passable: true, minLevel: 1 },
  flame: { name: 'Chama', surface: 'ash', passable: true },
  column: { name: 'Coluna', surface: 'rock', passable: false, raise: 4 },
  nave: { name: 'Laje', surface: 'pavers', passable: true },
  barricade: { name: 'Barricada', surface: 'grass', passable: false },
  door: { name: 'Porta trancada', surface: 'pavers', passable: false },
  void: { name: 'Vazio', surface: 'void', passable: false },
  snow: { name: 'Neve', surface: 'snow', passable: true },
};

/** Levels at which open ground turns to bare rock, and to snow. */
export const MOUNTAIN_ROCK = 7;
export const MOUNTAIN_SNOW = 11;
/** Editable elevation range. */
export const LEVEL_MIN = -1;
export const LEVEL_MAX = 15;

/** Base thickness of the ground above the table, in levels (level 0 still has body). */
const BASE_LEVELS = 2;

export interface Cell {
  c: number;
  r: number;
  i: number;
  x: number;
  z: number;
  type: TerrainId;
  /** Gameplay elevation level (Ember's rule). */
  level: number;
  /** Surface index for the terrain material, or -1 for void. */
  surface: number;
  /** Baked layer index: base terrain layers followed by V3 City and water variants; -1 uses prototype. */
  ground: number;
  /** World height of the top of the ground. */
  groundY: number;
  water: boolean;
  /** World height of the water surface (water cells only). */
  waterY: number;
}

export class Board {
  readonly layout: HexLayout;
  readonly cells: Cell[];
  readonly id: string;
  readonly title: string;
  readonly draft: EmberMapDraft;

  constructor(draft: EmberMapDraft) {
    this.draft = draft;
    this.id = draft.id;
    this.title = draft.title;
    this.layout = makeLayout(draft.cols, draft.rows);
    this.cells = [];
    for (let r = 0; r < draft.rows; r++) {
      for (let c = 0; c < draft.cols; c++) {
        const i = indexOf(this.layout, c, r);
        const type = (draft.tiles[i] ?? 'plains') as TerrainId;
        const info = TERRAIN_INFO[type] ?? TERRAIN_INFO.plains;
        const level = Math.max(draft.terrainElevations?.[i] ?? 0, info.minLevel ?? 0, -1);
        const water = type === 'water';
        // high ground reads as mountain: bare rock up high, snow on the peaks
        let surf = info.surface;
        if (surf !== 'void' && !water) {
          if (level >= MOUNTAIN_SNOW) surf = 'snow';
          else if (level >= MOUNTAIN_ROCK && (surf === 'grass' || surf === 'forest' || surf === 'ash')) surf = 'rock';
        }
        const plains049 = type === 'plains' && surf === 'grass' && draft.tileVariants?.[i] === 48;
        const surface = surf === 'void' ? -1 : plains049 ? SURFACE.plains049 : SURFACE[surf];
        // Baked PBR materials are activated only by their saved tile version. City uses its
        // own layer, while V3 terrain versions reuse their terrain's layer in the selected set.
        const variant = draft.tileVariants?.[i] ?? 0;
        const engine2V3 = ENGINE2_V3_GROUND_VARIANTS.find((entry) => entry.terrain === type && entry.variant === variant);
        const cityTile = type === 'plains' && (variant === ENGINE2_V3_CITY_VARIANT || (variant >= 21 && variant <= 38 && variant !== 22));
        const v3Layer = engine2V3?.key === 'city' ? GROUND_LAYERS.length
          : engine2V3 ? GROUND_LAYERS.indexOf(type) : -1;
        const variantLayer = groundVariantLayer(type, variant);
        // HEAD's rule for every tile that existed before (only Claude's version per terrain is
        // baked; the rest keep the painted ground), plus GPT's appended variant layers.
        const claudeTile = CLAUDE_VARIANT[type] !== undefined && variant === CLAUDE_VARIANT[type];
        const ground = surf === 'void' ? -1
          : ((type === 'plains' && variant === 50) || (type === 'nave' && variant === 20)) && variantLayer >= 0 ? GROUND_LAYERS.length + 3 + variantLayer
          : cityTile ? GROUND_LAYERS.length
            : engine2V3 ? v3Layer
              : variantLayer >= 0 ? GROUND_LAYERS.length + 3 + variantLayer
                : claudeTile ? GROUND_LAYERS.indexOf(type) : -1;
        let groundY: number;
        if (info.surface === 'void') groundY = 0;
        else if (water) groundY = (BASE_LEVELS - 1 + level) * STEP;
        else groundY = (BASE_LEVELS + level + (info.raise ?? 0)) * STEP;
        this.cells.push({
          c, r, i, type, level, surface, ground, groundY, water,
          x: hexX(this.layout, c, r), z: hexZ(this.layout, r),
          waterY: water ? (BASE_LEVELS - 0.35 + level) * STEP : 0,
        });
      }
    }
  }

  cell(c: number, r: number): Cell { return this.cells[indexOf(this.layout, c, r)]; }

  /** World-space rectangle the ground covers (hex centres, plus a little at top/bottom). */
  bounds(): { x0: number; x1: number; z0: number; z1: number } {
    const L = this.layout;
    return { x0: -L.ox, x1: L.ox, z0: -L.oz - 0.6, z1: L.oz + 0.6 };
  }

  get hasWater(): boolean { return this.cells.some(c => c.water); }
}
