import * as THREE from "three";
import { mergeGeometries } from "three/examples/jsm/utils/BufferGeometryUtils.js";
import type { DecorationDef } from "../../types";

/** Deterministic irregular stone solids, using a rock-only region of the reference. */
export function createRockGeometry(def: DecorationDef, tile: number, height: number, rotation: number): THREE.BufferGeometry {
  const parts: THREE.BufferGeometry[] = [];
  let stoneIndex = 0;
  const stone = (x: number, y: number, z: number, w: number, d: number, h: number, angle = 0) => {
    const geometry = new THREE.IcosahedronGeometry(1, 1);
    const p = geometry.getAttribute("position"), uv = geometry.getAttribute("uv");
    const seed = ++stoneIndex;
    for (let i = 0; i < p.count; i++) {
      const px = p.getX(i), py = p.getY(i), pz = p.getZ(i);
      // Coordinate-based noise keeps duplicate face vertices joined.
      const rough = 1 + 0.11 * Math.sin(px * 17 + py * 23 + pz * 13 + seed * 3.1);
      p.setXYZ(i, px * rough, py * rough, pz * rough);
      uv.setXY(i, (242 + (px + 1) * 50) / 1168, 1 - (293 + (pz + 1) * 16) / 784);
    }
    geometry.scale(w / 2, d / 2, h / 2).rotateY(angle).translate(x, y, z);
    geometry.computeVertexNormals();
    parts.push(geometry);
  };
  if (def.rockStyle === "arch") {
    const spring = height * 0.48;
    for (const side of [-1, 1]) for (let row = 0; row < 4; row++)
      stone(side * tile * 0.56, 0, height * (0.075 + row * 0.12), tile * 0.45,
        tile * 0.55, height * 0.22);
    for (let i = 0; i <= 10; i++) {
      const angle = i / 10 * Math.PI;
      stone(Math.cos(angle) * tile * 0.56, 0, spring + Math.sin(angle) * height * 0.36,
        tile * 0.35, tile * 0.52, height * 0.25, -angle);
    }
  } else if (def.rockStyle === "layered") {
    for (let row = 0; row < 5; row++) for (let i = 0; i < 3; i++)
      stone((i - 1) * tile * 0.48 + Math.sin(row * 2) * tile * 0.05,
        Math.cos(row) * tile * 0.04, height * (0.1 + row * 0.185),
        tile * 0.66, tile * (0.65 - row * 0.04), height * 0.24, Math.sin(i + row) * 0.06);
  } else {
    for (let i = 0; i < 5; i++)
      stone((i - 2) * tile * 0.28, tile * 0.1, height * 0.48,
        tile * 0.38, tile * 0.45, height * (0.86 + Math.sin(i * 4) * 0.1));
    for (let i = 0; i < 7; i++)
      stone((i % 3 - 1) * tile * 0.38, -tile * (0.16 + Math.floor(i / 3) * 0.16),
        height * 0.13, tile * 0.5, tile * 0.38, height * 0.22, i * 0.19);
    stone(0, -tile * 0.24, height * 0.29, tile * 1.1, tile * 0.6, height * 0.19, -0.25);
  }
  const geometry = mergeGeometries(parts)!;
  parts.forEach(part => part.dispose());
  geometry.rotateZ(-rotation * Math.PI / 2);
  return geometry;
}
