import type { WebGL2DRenderer } from "./WebGL2DRenderer";

export const TURN_UNDEAD_V1_DURATION = 2.1;
export interface DivineLightCell {
  x: number;
  y: number;
  corners: readonly (readonly [number, number])[];
}

function cellPath(ctx: WebGL2DRenderer, cell: DivineLightCell): void {
  ctx.beginPath();
  cell.corners.forEach(([x, y], i) => i ? ctx.lineTo(x, y) : ctx.moveTo(x, y));
  ctx.closePath();
}

/** Preview-only V1: the game's 2D WebGL glow/energy path. Each light cell is clipped
 * to its exact hex; only edges without another affected neighbor form the perimeter. */
export function drawTurnUndeadV1(ctx: WebGL2DRenderer, cells: readonly DivineLightCell[], elapsed: number): void {
  if (!cells.length || elapsed < 0 || elapsed >= TURN_UNDEAD_V1_DURATION) return;
  const t = elapsed / TURN_UNDEAD_V1_DURATION;
  const envelope = Math.min(1, t / .15) * Math.pow(1 - t, .85);
  const edges = new Map<string, { a: readonly [number, number]; b: readonly [number, number]; count: number }>();
  const vertex = (p: readonly [number, number]) => `${Math.round(p[0] * 100)},${Math.round(p[1] * 100)}`;
  const cx = cells.reduce((n, p) => n + p.x, 0) / cells.length;
  const cy = cells.reduce((n, p) => n + p.y, 0) / cells.length;
  ctx.save();
  ctx.globalCompositeOperation = "lighter";
  cells.forEach((cell, index) => {
    const r = Math.max(...cell.corners.map(([x, y]) => Math.hypot(x - cell.x, y - cell.y)));
    const delay = Math.hypot(cell.x - cx, cell.y - cy) / r * .024;
    const local = Math.max(0, t - delay);
    const pulse = Math.exp(-Math.pow((local - .24) / .16, 2));
    ctx.save();
    cellPath(ctx, cell); ctx.clip();
    const light = ctx.createLinearGradient(cell.x, cell.y - r, cell.x, cell.y + r);
    light.addColorStop(0, `rgba(255,252,226,${envelope * (.12 + pulse * .55)})`);
    light.addColorStop(.45, `rgba(255,219,113,${envelope * (.07 + pulse * .36)})`);
    light.addColorStop(1, `rgba(210,133,39,${envelope * .03})`);
    ctx.fillStyle = light; cellPath(ctx, cell); ctx.fill();
    // Fine, moving light filaments and sparks, never a solid sprite or apparition.
    for (let strand = 0; strand < 4; strand++) {
      const offset = (strand - 1.5) * r * .28;
      const points: [number, number][] = [];
      for (let j = 0; j <= 8; j++) {
        const u = j / 8;
        points.push([cell.x + offset + Math.sin(u * 5 + local * 8 + index * 1.4 + strand) * r * .045,
          cell.y + r * .8 - u * r * 1.7]);
      }
      for (let layer = 0; layer < 3; layer++) {
        ctx.strokeStyle = [`rgba(255,186,62,${envelope * .13})`, `rgba(255,229,145,${envelope * .45})`, `rgba(255,255,230,${envelope * .78})`][layer]!;
        ctx.lineWidth = r * [.12, .045, .014][layer]!;
        ctx.beginPath();points.forEach(([x,y], j) => j ? ctx.lineTo(x,y) : ctx.moveTo(x,y));ctx.stroke();
      }
    }
    for (let i = 0; i < 9; i++) {
      const seed = (Math.sin(i * 41.7 + index * 19.3) + 1) / 2;
      const age = (local * (.7 + seed * .4) + seed) % 1;
      const x = cell.x + (seed - .5) * r * 1.5 + Math.sin(age * 6 + i) * r * .035;
      const y = cell.y + r * .9 - age * r * 1.8;
      const alpha = Math.sin(age * Math.PI) * envelope;
      ctx.strokeStyle = `rgba(255,247,204,${alpha})`;ctx.lineWidth = r * .022;
      ctx.beginPath();ctx.moveTo(x,y);ctx.lineTo(x,y+r*(.06+seed*.09));ctx.stroke();
    }
    ctx.restore();
    for (let i = 0; i < cell.corners.length; i++) {
      const a = cell.corners[i]!;const b = cell.corners[(i+1)%cell.corners.length]!;
      const k = [vertex(a),vertex(b)].sort().join('|');
      const edge = edges.get(k);if(edge)edge.count++;else edges.set(k,{a,b,count:1});
    }
  });
  for (const edge of edges.values()) {
    if (edge.count !== 1) continue;
    for(let layer=0;layer<3;layer++) {
      ctx.strokeStyle = [`rgba(255,191,73,${envelope*.2})`,`rgba(255,219,132,${envelope*.65})`,`rgba(255,252,218,${envelope*.95})`][layer]!;
      ctx.lineWidth=[8,3,1][layer]!;
      ctx.beginPath();ctx.moveTo(...edge.a);ctx.lineTo(...edge.b);ctx.stroke();
    }
  }
  ctx.restore();
}
