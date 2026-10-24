import type { WebGL2DRenderer } from "./WebGL2DRenderer";

export const APPARITION_LIGHTNING_RELEASE = 3.5;
export const APPARITION_LIGHTNING_END = 4.55;
export interface LightningPoint { x: number; y: number }

/** ATT-synchronized 2D discharge. The ordered points are the centers of the
 * affected straight hex ray, projected at hand height, never an area circle. */
export function drawApparitionLightningV1(ctx: WebGL2DRenderer, hands: LightningPoint,
  ray: readonly LightningPoint[], seconds: number, scale: number): void {
  if (seconds < .65 || seconds >= APPARITION_LIGHTNING_END || !ray.length) return;
  const released = seconds >= APPARITION_LIGHTNING_RELEASE;
  const age = seconds - APPARITION_LIGHTNING_RELEASE;
  const strength = released ? Math.min(1, age / .035) * Math.min(1, (APPARITION_LIGHTNING_END - seconds) / .3)
    : Math.min(1, (seconds - .65) / 1.5) * .5;
  const phase = Math.floor(seconds * 24);
  const noise = (n: number) => Math.sin(n * 127.1 + phase * 43.7) * Math.cos(n * 19.3 + phase * 9.1);
  const stroke = (points: LightningPoint[], width: number, alpha: number, color: string) => {
    ctx.strokeStyle = color; ctx.globalAlpha = alpha * strength; ctx.lineWidth = width * scale;
    ctx.beginPath(); points.forEach((p, i) => i ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y)); ctx.stroke();
  };
  const bolt = (points: LightningPoint[], alpha = 1) => {
    stroke(points, .28, .09 * alpha, '#248cff');
    stroke(points, .12, .25 * alpha, '#39a9ff');
    stroke(points, .047, .8 * alpha, '#72d7ff');
    stroke(points, .014, alpha, '#edfcff');
  };
  ctx.save(); ctx.globalCompositeOperation = 'lighter';
  // Small electrical arcs join her two hands during the recorded charge.
  for (let j = 0; j < 4; j++) {
    const points = Array.from({ length: 7 }, (_, i) => ({
      x: hands.x + (i / 6 - .5) * scale * .24,
      y: hands.y + noise(i + j * 7) * scale * .085 + (j - 1.5) * scale * .025,
    })); bolt(points, .6);
  }
  if (released) {
    const distance = Math.min(1, age / .3) * ray.length;
    const route = [hands, ...ray];
    const points: LightningPoint[] = [hands];
    for (let cell = 0; cell < ray.length && cell < distance; cell++) {
      const a = route[cell]!, b = route[cell + 1]!;
      const portion = Math.min(1, distance - cell);
      for (let i = 1; i <= 8; i++) {
        const f = Math.min(i / 8, portion);
        const p = { x: a.x + (b.x - a.x) * f,
          y: a.y + (b.y - a.y) * f + (i === 8 ? 0 : noise(cell * 8 + i) * scale * .13) };
        points.push(p);
        if (i % 3 === 0) bolt([p, { x: p.x + noise(i + cell + 50) * scale * .2,
          y: p.y + noise(i + cell + 70) * scale * .3 }], .45);
        if (f === portion) break;
      }
    }
    bolt(points);
  }
  ctx.restore();
}
