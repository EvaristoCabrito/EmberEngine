import type { WebGL2DRenderer } from "./WebGL2DRenderer";

export const PROVOKE_FX_DURATION = 1.15;

/** Existing 2D WebGL path/glow FX style: a sharp flare of agitation on each enemy. */
export function drawProvokeVFX(ctx: WebGL2DRenderer, x: number, y: number, size: number, elapsed: number): void {
  if (elapsed < 0 || elapsed >= PROVOKE_FX_DURATION) return;
  const t = elapsed / PROVOKE_FX_DURATION;
  const fade = Math.min(1, t / .065) * Math.pow(1 - t, .8);
  ctx.save();
  ctx.globalCompositeOperation = "lighter";
  // Rising, curling strands use the same layered glow and bright-core strokes as lightning FX.
  for (let i = 0; i < 7; i++) {
    const side = (i - 3) / 3;
    const phase = t * 4 + i * 1.7;
    const baseX = x + side * size * .34;
    const baseY = y - size * .1;
    const height = size * (1.05 + .23 * Math.sin(i * 3.1));
    const points: [number, number][] = [];
    for (let j = 0; j <= 12; j++) {
      const u = j / 12;
      points.push([baseX + Math.sin(u * 5 + phase) * size * .075 * u + side * size * .14 * u,
        baseY - height * u - t * size * .22]);
    }
    for (let layer = 0; layer < 3; layer++) {
      ctx.beginPath(); points.forEach(([px, py], j) => j ? ctx.lineTo(px, py) : ctx.moveTo(px, py));
      ctx.lineWidth = size * [ .075, .028, .009 ][layer];
      ctx.strokeStyle = [`rgba(235,45,12,${fade * .24})`, `rgba(255,94,23,${fade * .68})`, `rgba(255,222,157,${fade * .88})`][layer];
      ctx.shadowColor = `rgba(255,58,16,${fade})`;
      ctx.shadowBlur = layer === 0 ? size * .2 : 0;
      ctx.stroke();
    }
  }
  ctx.shadowBlur = 0;
  // Small, fast ember trails burst upward and die independently.
  for (let i = 0; i < 28; i++) {
    const delay = (i % 7) * .018;
    const age = Math.max(0, t - delay);
    const seed = Math.sin(i * 73.13) * .5 + .5;
    const speed = .65 + seed;
    const sx = x + Math.sin(i * 4.37) * size * (.12 + age * .48);
    const sy = y - size * (.12 + age * speed * 1.9);
    const alpha = fade * Math.max(0, 1 - age / .8);
    ctx.strokeStyle = `rgba(255,${130 + Math.floor(seed * 95)},85,${alpha})`;
    ctx.lineWidth = Math.max(.7, size * .015 * (1 - age));
    ctx.beginPath();ctx.moveTo(sx, sy);ctx.lineTo(sx - Math.sin(i * 4.37) * size * .025, sy + size * (.035 + seed * .07));ctx.stroke();
  }
  ctx.restore();
}
