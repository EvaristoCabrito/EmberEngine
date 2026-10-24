// Ember's battle atmosphere on the 3D map:
//  - fog of war: engine sight → Engine2's FogOfWar over the terrain + dimmed decorations,
//  - the Névoas ("Tipo de névoa": Névoa 2/3/4, 01, 5): remade as real 3D mist (mist3d.ts),
//  - the wisps: Ember's own ThreeAtmosphere, unchanged, with its flat mist sheets switched off,
//  - the Vinhetas: Ember's screen overlays (BattleVignettes.tsx).
//
// ThreeAtmosphere builds in Ember's renderer space: x = board pixels, y = -board pixels (down
// is negative), z = height toward its overhead camera. One parent transform maps that onto
// Engine2's world (1 unit = one hex radius, y up), lying just above the ground.

import * as THREE from "three";
import type { BattleEngine } from "./engine";
import type { Unit } from "./types";
import type { DioramaView } from "../editor/ember/dioramaView";
import { ThreeAtmosphere } from "./gfx/three/ThreeAtmosphere";
import { getDevGfx } from "./gfx/three/devGfx";
import { footprint, hexNeighbors } from "./pathfinding";
import { FogOfWar, FOG_EXPLORED, FOG_UNSEEN, FOG_VISIBLE } from "../render/fogOfWar";
import { EXPLORED_ALPHA } from "./gfx/three/ThreeFogMask";
import { Mist3D } from "./mist3d";

type Any = Record<string, any>;

/** Height of Ember's wisp layer above the board's average ground. */
const MIST_LIFT = 0.3;

export class BattleAtmosphere3D {
  private readonly fog = new FogOfWar();
  private readonly atmosphere = new ThreeAtmosphere();
  private readonly mistRoot = new THREE.Group();
  private readonly mist: Mist3D;
  private placed = false;
  private shadeKey = "";

  constructor(private readonly view: DioramaView, private readonly engine: BattleEngine) {
    view.stage.scene.add(this.fog.group);
    this.mistRoot.rotation.x = -Math.PI / 2; // Ember (x, y, z) → Engine2 (x, z, -y)
    this.mistRoot.add(this.atmosphere.group);
    view.stage.scene.add(this.mistRoot);
    this.mist = new Mist3D(view.stage.mist);
  }

  /** Ember board pixel → Engine2 world, from hex (0, 0)'s anchor in both. */
  private origin(): { a0x: number; a0y: number; tile: number; ox: number; oz: number } | null {
    const L = this.view.board?.layout;
    if (!L) return null;
    const a0 = (this.engine as unknown as Any).effectAnchor(0, 0) as { worldX: number; worldY: number; tile: number };
    return { a0x: a0.worldX, a0y: a0.worldY, tile: a0.tile, ox: L.ox, oz: L.oz };
  }

  sync(dt: number): void {
    const board = this.view.board, o = this.origin();
    if (!board || !o) return;
    const engine = this.engine;
    this.syncFog();

    if (!this.placed) {
      this.placed = true;
      const groundY = board.cells.reduce((s, c) => s + c.groundY, 0) / board.cells.length;
      const s = 1 / o.tile;
      this.mistRoot.scale.setScalar(s);
      this.mistRoot.position.set(-o.a0x * s - o.ox, groundY + MIST_LIFT, -o.a0y * s - o.oz);
    }
    const fx = getDevGfx().atmosphericFx;
    this.mist.sync(engine, board, this.view.groundAt, fx);
    // Ember's ThreeAtmosphere only for its wisps: told the mist is a vignette, its world-space
    // sheets stay off (the 3D mist replaces them) while the wisps keep their own settings.
    // "Sem névoa" still turns everything off, as in Ember.
    const mission = engine.mission.mistType === "none" ? engine.mission : { ...engine.mission, mistType: "vignette" as const };
    const wispsOnly = Object.create(engine, { mission: { value: mission } }) as BattleEngine;
    this.atmosphere.sync(wispsOnly, o.tile, dt, this.view.stage.sun, this.view.stage.hemi, undefined, undefined, undefined, fx);
  }

  /** Ember's ThreeBattleRenderer.syncFog: the same states and the same "always clear within one
   * hex of the party" rule, rebuilt only when sight changes or the party moves. */
  private syncFog(): void {
    const engine = this.engine, board = this.view.board!;
    if (!engine.fogged) {
      this.fog.hide();
      if (this.shadeKey !== "off") { this.shadeKey = "off"; this.view.decor.shade(null); }
      return;
    }
    const cols = engine.cols;
    const clear = new Set<number>();
    const party = (engine.units as Unit[]).filter(u => u.side === "player" && u.alive);
    for (const u of party) for (const p of footprint(u)) for (const n of [p, ...hexNeighbors(p.x, p.y)]) {
      if (n.x >= 0 && n.y >= 0 && n.x < cols && n.y < engine.rows) clear.add(n.y * cols + n.x);
    }
    const state = (x: number, y: number) => clear.has(y * cols + x) || engine.visible(x, y) ? FOG_VISIBLE : engine.explored(x, y) ? FOG_EXPLORED : FOG_UNSEEN;
    const key = `${engine.mission.id}:${engine.visVersion}:${party.map(u => `${u.x},${u.y}`).join("|")}`;
    this.fog.update(key, board, this.view.surfaces, state);
    // decorations load in after the board, so their count is part of when to re-shade them
    const shadeKey = `${key}:${this.view.decor.count}`;
    if (shadeKey === this.shadeKey) return;
    this.shadeKey = shadeKey;
    this.view.decor.shade((x, y) => {
      if (x < 0 || y < 0 || x >= cols || y >= engine.rows) return 1;
      const s = state(x, y);
      return s === FOG_VISIBLE ? 0 : s === FOG_EXPLORED ? EXPLORED_ALPHA : 1;
    });
  }

  dispose(): void {
    this.view.stage.scene.remove(this.fog.group, this.mistRoot);
    this.fog.dispose();
    this.atmosphere.dispose();
    this.mist.dispose();
  }
}
