/** Classic top-down relief stays inside each cell, preserving picking and overlays. */
export function elevationFaces(level: number, neighbors: readonly number[]): { edge: number; depth: number; steps: number }[] {
  if (!Number.isFinite(level) || level <= 0) return [];
  return Array.from({ length: 6 }, (_, edge) => {
    const delta = Math.max(0, level - (neighbors[edge] ?? 0));
    return { edge, depth: Math.min(0.3, delta * 0.075), steps: Math.min(6, Math.ceil(delta)) };
  }).filter(face => face.depth > 0);
}

export function drawElevationSteps(ctx: any, cx: number, cy: number, radius: number, level: number, neighbors: readonly number[]): void {
  const point = (edge: number, scale = 1) => {
    const angle = (edge * 60 - 30) * Math.PI / 180;
    return { x: cx + Math.cos(angle) * radius * scale, y: cy + Math.sin(angle) * radius * scale };
  };
  for (const face of elevationFaces(level, neighbors)) {
    const a = point(face.edge), b = point((face.edge + 1) % 6);
    const innerA = point(face.edge, 1 - face.depth), innerB = point((face.edge + 1) % 6, 1 - face.depth);
    const front = face.edge <= 2;
    ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(b.x, b.y);
    ctx.lineTo(innerB.x, innerB.y); ctx.lineTo(innerA.x, innerA.y); ctx.closePath();
    ctx.fillStyle = front ? (face.edge === 1 ? "rgba(48,38,27,0.92)" : "rgba(72,57,39,0.86)") : "rgba(65,60,42,0.55)";
    ctx.fill();
    ctx.lineWidth = Math.max(0.7, radius * 0.018);
    for (let step = 1; step <= face.steps; step++) {
      const fraction = step / face.steps;
      const scale = 1 - face.depth * fraction;
      const p = point(face.edge, scale), q = point((face.edge + 1) % 6, scale);
      ctx.beginPath(); ctx.moveTo(p.x, p.y); ctx.lineTo(q.x, q.y);
      ctx.strokeStyle = step === face.steps ? "rgba(215,198,143,0.65)" : "rgba(12,16,13,0.55)";
      ctx.stroke();
    }
  }
}
