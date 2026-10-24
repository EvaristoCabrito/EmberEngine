// Battle presentation events for the cinematic camera (cinematicCamera.ts). The engine keeps
// deciding everything; this only listens: its own presentation calls are wrapped so the camera
// hears about them right after they happen (the result is never read back or changed), and
// the wrappers are removed when the battle closes.
//
// Stage 1 publishes the critical hit only — the shot that proves the director can take the
// camera, present an action and give the player's exact view back. Other events follow in
// stage 2 (attack start, projectile, area spell, defeat, boss ability).

import * as THREE from "three";
import type { BattleEngine } from "./engine";
import type { Unit } from "./types";
import { CinematicDirector, ShotPriority } from "../render/cinematicCamera";

type Any = Record<string, any>;

export type CombatPresentationEvent =
  | { kind: "criticalHit"; attackerId: string | null; targetId: string; damage: number };

export class BattleCinematics {
  private readonly undo: (() => void)[] = [];

  constructor(
    private readonly engine: BattleEngine,
    readonly director: CinematicDirector,
    /** A unit's feet and head in world space, or null when it isn't on the board. */
    private readonly unitPoints: (unitId: string) => THREE.Vector3[] | null,
  ) {
    const e = engine as unknown as Any;
    this.listen(e, "spawnHit", (target: Unit, dmg: number, crit: boolean) => {
      if (crit) this.publish({ kind: "criticalHit", attackerId: (e.active?.att as string | undefined) ?? null, targetId: target.id, damage: dmg });
    });
  }

  /** Call the engine's method as before, then tell the camera; a camera error never reaches combat. */
  private listen(obj: Any, method: string, after: (...args: any[]) => void): void {
    const original = obj[method] as ((...args: unknown[]) => unknown) | undefined;
    if (typeof original !== "function") return;
    obj[method] = function (this: unknown, ...args: unknown[]) {
      const result = original.apply(this, args);
      try { after(...args); } catch { /* presentation only */ }
      return result;
    };
    this.undo.push(() => { obj[method] = original; });
  }

  publish(event: CombatPresentationEvent): void {
    if (event.kind === "criticalHit") {
      // Critical hit (spec 0.7–1.1 s): a noticeable push-in on attacker and target with a brief
      // impact shake at the confirmed hit, a short hold on the reaction, then the player's view.
      const ids = [event.attackerId, event.targetId].filter((id): id is string => !!id);
      this.director.play({
        name: "criticalHit",
        priority: ShotPriority.High,
        subjects: () => {
          const pts = ids.flatMap(id => this.unitPoints(id) ?? []);
          return this.unitPoints(event.targetId) ? pts : null; // target gone: end the shot
        },
        inTime: 0.22, hold: 0.38, outTime: 0.42,
        push: 0.55, pitch: -4,
        shake: { amp: 0.09, at: 0, dur: 0.28 },
        margin: 0.45,
      });
    }
  }

  dispose(): void {
    for (const u of this.undo.splice(0)) u();
    this.director.dispose();
  }
}
