/** Lightweight CPU-side particle pool for the two elements that need discrete motes rather
 * than a pure shader fill: fire embers and holy's slow-rising motes with motion-blur trails.
 * Kept deliberately small (particle counts are capped) — each is one small billboard draw
 * using the shared PARTICLE program, so instance count only ever affects draw-call count,
 * never program switches or FBO binds. */

import type { ElementKind } from "./params";

export interface FxParticle {
  /** Position and velocity are in emitter-radius units, not screen pixels. This keeps a
   * particle attached to its map effect as the camera pans or zooms. */
  x: number;
  y: number;
  vx: number;
  vy: number;
  age: number;
  life: number;
  size: number;
  color: [number, number, number];
}

const MAX_PARTICLES_PER_EMITTER = 28;

export class ParticleEmitter {
  particles: FxParticle[] = [];
  private spawnAccum = 0;

  constructor(private kind: Extract<ElementKind, "fire" | "holy">) {}

  update(dt: number, radiusPx: number, color: [number, number, number], rate = 10): void {
    this.spawnAccum += dt * rate;
    while (this.spawnAccum >= 1 && this.particles.length < MAX_PARTICLES_PER_EMITTER) {
      this.spawnAccum -= 1;
      this.spawn(Math.max(1, radiusPx), color);
    }
    for (const p of this.particles) {
      p.age += dt;
      p.x += p.vx * dt;
      p.y += p.vy * dt;
      if (this.kind === "fire") p.vy -= (6 / Math.max(1, radiusPx)) * dt; // embers accelerate upward slightly
    }
    this.particles = this.particles.filter((p) => p.age < p.life);
  }

  private spawn(radiusPx: number, color: [number, number, number]): void {
    const spread = 0.6;
    const angle = Math.random() * Math.PI * 2;
    const r = Math.random() * spread;
    const x = Math.cos(angle) * r;
    const y = Math.sin(angle) * r * 0.5;
    if (this.kind === "fire") {
      this.particles.push({
        x,
        y,
        vx: ((Math.random() - 0.5) * 24) / radiusPx,
        vy: (-40 - Math.random() * 50) / radiusPx,
        age: 0,
        life: 0.6 + Math.random() * 0.5,
        size: 0.08 + Math.random() * 0.08,
        color,
      });
    } else {
      this.particles.push({
        x: (Math.random() - 0.5) * 0.8,
        y: 0.4 + Math.random() * 0.4,
        vx: ((Math.random() - 0.5) * 6) / radiusPx,
        vy: (-18 - Math.random() * 14) / radiusPx,
        age: 0,
        life: 1.4 + Math.random() * 0.8,
        size: 0.05 + Math.random() * 0.05,
        color,
      });
    }
  }

  clear(): void {
    this.particles = [];
  }
}
