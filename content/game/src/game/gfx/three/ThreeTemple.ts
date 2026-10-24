import * as THREE from "three";
import { mergeGeometries } from "three/examples/jsm/utils/BufferGeometryUtils.js";
import type { DecorationDef } from "../../types";

/** Solid temple masonry with photographed facades and recessed, closed niches. */
export function createTempleGeometry(def: DecorationDef, tile: number, height: number, rotation: number, connections: { x: number; y: number }[]): THREE.BufferGeometry {
  const parts: THREE.BufferGeometry[] = [];
  const depth = tile * 0.42;
  const relief = def.templeStyle === "relief";
  // Bounds exclude the gray background. Normalized source dimensions retain
  // identical facade proportions through the tactical height conversion.
  const crop = relief ? [130 / 1168, 88 / 784, 1037 / 1168, 686 / 784]
    : [190 / 1712, 125 / 1152, 1520 / 1712, 1014 / 1152];
  const segment = (width: number, x: number, y: number, angle: number, niche: boolean) => {
    const add = (part: THREE.BufferGeometry) => {
      const p = part.getAttribute("position"), n = part.getAttribute("normal"), uv = part.getAttribute("uv");
      for (let i = 0; i < p.count; i++) {
        const front = Math.abs(n.getY(i)) > 0.5;
        const top = Math.abs(n.getZ(i)) > 0.5;
        // Facades share coordinates so the arch, columns and trim stay aligned.
        const u = top ? 0.02 + THREE.MathUtils.clamp(p.getX(i) / width + 0.5, 0, 1) * 0.16
          : front ? p.getX(i) / width + 0.5 : 0.04 + (p.getY(i) / depth + 0.5) * 0.1;
        // The left masonry pier is outside the arch in all three source images.
        // Caps must sample that stone-only rectangle, never the full facade.
        const v = top ? 0.18 + THREE.MathUtils.clamp(p.getY(i) / depth + 0.5, 0, 1) * 0.5
          : p.getZ(i) / height;
        uv.setXY(i, crop[0] + THREE.MathUtils.clamp(u, 0, 1) * (crop[2] - crop[0]),
          1 - crop[3] + THREE.MathUtils.clamp(v, 0, 1) * (crop[3] - crop[1]));
      }
      const compatible = part.index ? part.toNonIndexed() : part;
      if (compatible !== part) part.dispose();
      compatible.rotateZ(angle).translate(x, y, 0);
      parts.push(compatible);
    };
    const box = (w: number, d: number, h: number, cx: number, cy: number, z: number) =>
      add(new THREE.BoxGeometry(w, d, h).translate(cx, cy, z));
    if (niche) {
      const radius = width * 0.22, spring = height * 0.5, rise = height * 0.29;
      const frame = new THREE.Shape();
      frame.moveTo(-width / 2, 0); frame.lineTo(-width / 2, height);
      frame.lineTo(width / 2, height); frame.lineTo(width / 2, 0);
      frame.lineTo(radius, 0); frame.lineTo(radius, spring);
      frame.absellipse(0, spring, radius, rise, 0, Math.PI, false, 0);
      frame.lineTo(-radius, 0); frame.closePath();
      add(new THREE.ExtrudeGeometry(frame, { depth, bevelEnabled: false, curveSegments: 24 })
        .rotateX(Math.PI / 2).translate(0, depth / 2, 0));
      // Both sides remain closed walls. The inset image panel sits inside the
      // arch rather than painting a fake doorway over a flat cube.
      box(width, depth * 0.3, height, 0, 0, height / 2);
      for (const side of [-1, 1]) {
        for (const sign of [-1, 1]) {
          box(width * 0.073, depth * 0.12, height * 0.39,
            sign * width * 0.26, side * depth * 0.51, height * 0.3);
          box(width * 0.082, depth * 0.15, height * 0.052,
            sign * width * 0.26, side * depth * 0.51, height * 0.5);
        }
      }
    } else box(width, depth, height, 0, 0, height / 2);
    box(width, depth * 1.13, height * 0.063, 0, 0, height * 0.9685);
    box(width, depth * 1.08, height * 0.1, 0, 0, height * 0.05);
  };
  if (def.templeStyle === "plain" && connections.length) {
    segment(depth, 0, 0, 0, false);
    for (const c of connections) segment(Math.hypot(c.x, c.y) / 2 + tile * 0.01,
      c.x / 4, c.y / 4, Math.atan2(c.y, c.x), false);
  } else segment(tile * ((rotation & 1) ? 1.5 : Math.sqrt(3)), 0, 0,
    -rotation * Math.PI / 2, def.templeStyle !== "plain");
  const geometry = mergeGeometries(parts)!;
  parts.forEach(part => part.dispose());
  return geometry;
}
