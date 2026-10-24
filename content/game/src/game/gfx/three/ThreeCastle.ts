import * as THREE from "three";
import { mergeGeometries } from "three/examples/jsm/utils/BufferGeometryUtils.js";
import type { DecorationDef } from "../../types";

// Sample masonry inside the supplied photographs; their gray backgrounds never
// enter the UV rectangles. The silhouette is solid geometry, not an image card.
export function createCastleGeometry(def: DecorationDef, tile: number, height: number, rotation: number, connections: { x: number; y: number }[]): THREE.BufferGeometry {
  const parts: THREE.BufferGeometry[] = [];
  const depth = tile * 0.42 * (def.wallThicknessScale ?? 1);
  const width = tile * ((rotation & 1) ? 1.5 : Math.sqrt(3));
  const gate = def.castleStyle === "gate";
  const ruined = def.castleStyle === "ruined";
  const rect = def.thickWall ? [405 / 1168, 228 / 784, 550 / 1168, 354 / 784]
    : gate ? [210 / 1168, 190 / 784, 365 / 1168, 640 / 784]
    : ruined ? [55 / 1168, 255 / 784, 660 / 1168, 600 / 784]
    : [45 / 1712, 360 / 1152, 1665 / 1712, 915 / 1152];
  const texture = (part: THREE.BufferGeometry, frontGate = false) => {
    part.computeBoundingBox();
    const bounds = part.boundingBox!, p = part.getAttribute("position"), n = part.getAttribute("normal"), uv = part.getAttribute("uv");
    for (let i = 0; i < p.count; i++) {
      const front = Math.abs(n.getY(i)) > 0.5;
      const axis = front || Math.abs(n.getZ(i)) > 0.5 ? "x" : "y";
      const lo = bounds.min[axis], span = bounds.max[axis] - lo;
      const u = ((axis === "x" ? p.getX(i) : p.getY(i)) - lo) / (span || 1);
      const v = Math.abs(n.getZ(i)) > 0.5
        ? (p.getY(i) - bounds.min.y) / (bounds.max.y - bounds.min.y || 1)
        : (p.getZ(i) - bounds.min.z) / (bounds.max.z - bounds.min.z || 1);
      if (frontGate && front) {
        // Shared coordinates keep the photographed arch and wooden leaf aligned.
        uv.setXY(i, (197 + (p.getX(i) / width + 0.5) * 775) / 1168,
          1 - (708 - p.getZ(i) / height * 588) / 784);
      } else uv.setXY(i, rect[0] + u * (rect[2] - rect[0]), 1 - rect[3] + v * (rect[3] - rect[1]));
    }
    parts.push(part.index ? part.toNonIndexed() : part);
    if (part.index) part.dispose();
  };
  const box = (w: number, d: number, h: number, x: number, y: number, z: number, angle = 0) => {
    const part = new THREE.BoxGeometry(w, d, h);
    texture(part);
    parts[parts.length - 1].rotateZ(angle).translate(x, y, z);
  };
  const merlonHeight = height * 0.16;
  const battlements = (length: number, x: number, y: number, angle: number) => {
    const count = Math.max(1, Math.round(length / (tile * 0.42)));
    const spacing = length / count;
    for (let i = 0; i < count; i++) {
      const offset = -length / 2 + spacing * (i + 0.5);
      box(spacing * 0.56, depth * 1.08, merlonHeight,
        x + Math.cos(angle) * offset, y + Math.sin(angle) * offset, height + merlonHeight / 2, angle);
    }
  };
  const thickBody = (length: number, x: number, y: number, angle: number) => {
    const slit = Math.min(tile * 0.09, length * 0.12), sideWidth = (length - slit) / 2;
    for (const sign of [-1, 1]) {
      const offset = sign * (slit + sideWidth) / 2;
      box(sideWidth, depth, height, x + Math.cos(angle) * offset,
        y + Math.sin(angle) * offset, height / 2, angle);
    }
    box(slit, depth, height * 0.34, x, y, height * 0.17, angle);
    box(slit, depth, height * 0.32, x, y, height * 0.84, angle);
    box(length, depth * 1.18, height * 0.09, x, y, height * 0.045, angle);
    battlements(length, x, y, angle);
  };
  if (def.castleStyle === "tower") {
    // Match the rectangular architecture cell on each axis. The old 1.45-tile
    // square stopped short of horizontal neighbors, exposing grass at the joins.
    const towerWidth = tile * Math.sqrt(3) + tile * 0.01;
    const towerDepth = tile * 1.5 + tile * 0.01;
    for (let side = 0; side < 4; side++) {
      const angle = side * Math.PI / 2;
      const horizontal = side % 2 === 0;
      thickBody(horizontal ? towerWidth : towerDepth,
        -Math.sin(angle) * (towerWidth - depth) / 2,
        Math.cos(angle) * (towerDepth - depth) / 2, angle);
    }
  } else if (gate) {
    const radius = width * 0.195, spring = height * 0.53, rise = height * 0.245;
    const frame = new THREE.Shape();
    frame.moveTo(-width / 2, 0); frame.lineTo(-width / 2, height);
    frame.lineTo(width / 2, height); frame.lineTo(width / 2, 0);
    frame.lineTo(radius, 0); frame.lineTo(radius, spring);
    frame.absellipse(0, spring, radius, rise, 0, Math.PI, false, 0);
    frame.lineTo(-radius, 0); frame.closePath();
    const extrude = (shape: THREE.Shape, d: number) => new THREE.ExtrudeGeometry(shape,
      { depth: d, bevelEnabled: false, curveSegments: 24 }).rotateX(Math.PI / 2).translate(0, d / 2, 0);
    texture(extrude(frame, depth), true);
    if (def.model3d === "door") {
      const leaf = new THREE.Shape();
      leaf.moveTo(-radius, 0); leaf.lineTo(radius, 0); leaf.lineTo(radius, spring);
      leaf.absellipse(0, spring, radius, rise, 0, Math.PI, false, 0);
      leaf.lineTo(-radius, 0); leaf.closePath();
      texture(extrude(leaf, tile * 0.1), true);
    }
    battlements(width, 0, 0, 0);
  } else if (ruined) {
    if (def.thickWall) box(width, depth * 1.18, height * 0.09, 0, 0, height * 0.045);
    // Uneven courses form a broken end, with individual fallen stones below it.
    const courses = 7, step = height / courses;
    for (let row = 0; row < courses; row++) {
      const length = width * (1 - row * 0.055);
      box(length, depth, step, -width / 2 + length / 2, 0, step * (row + 0.5));
    }
    battlements(width * 0.55, -width * 0.225, 0, 0);
    for (let i = 0; i < 9; i++) {
      box(tile * 0.15, tile * 0.17, step * 0.55,
        width * (0.18 + (i % 3) * 0.12), ((i % 2) ? 1 : -1) * depth * 0.6,
        step * (0.28 + Math.floor(i / 3) * 0.2), i * 0.73);
    }
  } else if (connections.length) {
    box(depth, depth, height, 0, 0, height / 2);
    for (const c of connections) {
      const angle = Math.atan2(c.y, c.x), length = Math.hypot(c.x, c.y) / 2 + tile * 0.01;
      if (def.thickWall) thickBody(length, c.x / 4, c.y / 4, angle);
      else {
        box(length, depth, height, c.x / 4, c.y / 4, height / 2, angle);
        battlements(length, c.x / 4, c.y / 4, angle);
      }
    }
  } else {
    if (def.thickWall) thickBody(width, 0, 0, 0);
    else {
      box(width, depth, height, 0, 0, height / 2);
      battlements(width, 0, 0, 0);
    }
  }
  const geometry = mergeGeometries(parts)!;
  parts.forEach(part => part.dispose());
  // Towers already follow the world's unequal X/Y spacing, regardless of rotation.
  if (def.castleStyle !== "tower" && (gate || ruined || !connections.length)) geometry.rotateZ(-rotation * Math.PI / 2);
  return geometry;
}
