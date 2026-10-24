import * as THREE from "three";
import { mergeGeometries } from "three/examples/jsm/utils/BufferGeometryUtils.js";
import type { DecorationDef } from "../../types";

/** Rectangular timber door and thick chamfered dungeon door with barred window. */
export function createReferenceDoorGeometry(def: DecorationDef, tile: number, height: number, rotation: number): THREE.BufferGeometry {
  const thick = def.doorStyle === "dungeonOak";
  const width = tile * ((rotation & 1) ? 1.5 : Math.sqrt(3)) * (def.architectureSpan ?? 1);
  const depth = tile * (thick ? 0.752 : 0.32);
  const parts: THREE.BufferGeometry[] = [];
  const stone = thick ? [537, 265, 575, 370] : [776, 224, 850, 358];
  const wood = thick ? [748, 292, 806, 458] : [587, 323, 659, 435];
  const metal = thick ? [761, 267, 780, 276] : [589, 300, 613, 307];
  const add = (g: THREE.BufferGeometry, rect: number[]) => {
    g.computeBoundingBox();
    const b = g.boundingBox!, p = g.getAttribute("position"), n = g.getAttribute("normal"), uv = g.getAttribute("uv");
    for (let i = 0; i < p.count; i++) {
      const useX = Math.abs(n.getX(i)) < 0.5;
      const lo = useX ? b.min.x : b.min.y, hi = useX ? b.max.x : b.max.y;
      const u = ((useX ? p.getX(i) : p.getY(i)) - lo) / (hi - lo || 1);
      // Horizontal surfaces need two ground-plane coordinates. Z is constant
      // across a cap and collapses its image into a stretched single-pixel row.
      const v = Math.abs(n.getZ(i)) > 0.5
        ? (p.getY(i) - b.min.y) / (b.max.y - b.min.y || 1)
        : (p.getZ(i) - b.min.z) / (b.max.z - b.min.z || 1);
      uv.setXY(i, (rect[0] + u * (rect[2] - rect[0])) / 1168,
        1 - (rect[3] - v * (rect[3] - rect[1])) / 784);
    }
    parts.push(g.index ? g.toNonIndexed() : g);
    if (g.index) g.dispose();
  };
  const box = (w: number, d: number, h: number, x: number, y: number, z: number, rect = stone) =>
    add(new THREE.BoxGeometry(w, d, h).translate(x, y, z), rect);
  const cx = thick ? width * 0.23 : 0, radius = width * (thick ? 0.17 : 0.23);
  const top = height * 0.87, bevel = thick ? height * 0.08 : 0;
  const frame = new THREE.Shape();
  frame.moveTo(-width / 2, 0); frame.lineTo(-width / 2, height);
  frame.lineTo(width / 2, height); frame.lineTo(width / 2, 0);
  frame.lineTo(cx + radius, 0); frame.lineTo(cx + radius, top - bevel);
  frame.lineTo(cx + radius * 0.8, top); frame.lineTo(cx - radius * 0.8, top);
  frame.lineTo(cx - radius, top - bevel); frame.lineTo(cx - radius, 0); frame.closePath();
  if (thick) {
    const hole = new THREE.Path();
    hole.moveTo(-width * 0.36, height * 0.26); hole.lineTo(-width * 0.36, height * 0.68);
    hole.lineTo(-width * 0.1, height * 0.68); hole.lineTo(-width * 0.1, height * 0.26); hole.closePath();
    frame.holes.push(hole);
    for (let i = 0; i < 4; i++) box(width * 0.017, tile * 0.055, height * 0.42,
      -width * 0.33 + i * width * 0.067, 0, height * 0.47, metal);
    for (const z of [0.38, 0.55]) box(width * 0.26, tile * 0.065, height * 0.018,
      -width * 0.23, 0, height * z, metal);
  }
  const extrude = (shape: THREE.Shape, d: number) => new THREE.ExtrudeGeometry(shape,
    { depth: d, bevelEnabled: false }).rotateX(Math.PI / 2).translate(0, d / 2, 0);
  add(extrude(frame, depth), stone);
  box(width, depth * 1.1, height * 0.065, 0, 0, height * 0.9675);
  if (!thick) {
    for (const sign of [-1, 1]) box(tile * 0.085, depth * 1.06, top,
      cx + sign * radius, 0, top / 2, wood);
    box(radius * 2 + tile * 0.12, depth * 1.1, height * 0.065, cx, 0, top, wood);
  }
  const swungOpen = !thick && def.model3d === "doorway";
  if (def.model3d === "door" || swungOpen) {
    const leafStart = parts.length;
    const leaf = new THREE.Shape();
    leaf.moveTo(cx - radius, 0); leaf.lineTo(cx + radius, 0);
    leaf.lineTo(cx + radius, top - bevel); leaf.lineTo(cx + radius * 0.8, top);
    leaf.lineTo(cx - radius * 0.8, top); leaf.lineTo(cx - radius, top - bevel); leaf.closePath();
    add(extrude(leaf, tile * 0.12), wood);
    for (const side of [-1, 1]) {
      for (const z of [0.19, 0.43, 0.73]) box(radius * 1.8, tile * 0.025, height * 0.035,
        cx, side * tile * 0.073, height * z, metal);
      add(new THREE.TorusGeometry(tile * 0.055, tile * 0.012, 6, 16)
        .rotateX(Math.PI / 2).translate(cx + radius * 0.58, side * tile * 0.092, height * 0.48), metal);
    }
    if (swungOpen) {
      // Swing the whole leaf, straps and handle around the right jamb. Keep
      // the opening genuinely empty rather than replacing it with dark artwork.
      const hingeX = cx + radius, hingeY = -depth / 2;
      for (let i = leafStart; i < parts.length; i++) parts[i]
        .translate(-hingeX, 0, 0)
        .rotateZ(Math.PI / 2)
        .translate(hingeX, hingeY, 0);
    }
  }
  const geometry = mergeGeometries(parts)!;
  parts.forEach(part => part.dispose());
  geometry.rotateZ(-rotation * Math.PI / 2);
  return geometry;
}
