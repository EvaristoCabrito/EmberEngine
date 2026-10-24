import * as THREE from 'three';
import type { Board } from '../map/board';
import { hexAt } from '../core/hex';
import { warp } from './terrainMesh';

export const MEADOW_GRASS_VARIANT = 59;
export const TALL_GRASS_VARIANT = 60;

/** Actual tapered leaf geometry, only on the separately painted meadow version. */
export function buildMeadowGrass(board: Board, heightAt: (x: number, z: number) => number): THREE.BufferGeometry {
  const cells = board.cells.filter(c => c.type === 'plains' && (board.draft.tileVariants?.[c.i] === MEADOW_GRASS_VARIANT || board.draft.tileVariants?.[c.i] === TALL_GRASS_VARIANT));
  const positions: number[] = [], colors: number[] = [], indices: number[] = [];
  const bounds = board.bounds();
  const supported = (x: number, z: number, groundY: number): boolean => {
    // hexAt clamps outside points to a border cell; check the actual mesh bounds first.
    if (x <= bounds.x0 + 0.04 || x >= bounds.x1 - 0.04 || z <= bounds.z0 + 0.04 || z >= bounds.z1 - 0.04) return false;
    const [wx, wz] = warp(x, z), [c, r] = hexAt(board.layout, wx, wz);
    const under = board.cell(c, r);
    return under.surface >= 0 && Math.abs(under.groundY - groundY) < 0.01;
  };
  const perCell = Math.min(180, Math.max(24, Math.floor(90000 / Math.max(1, cells.length))));
  for (const cell of cells) {
    const tall = board.draft.tileVariants?.[cell.i] === TALL_GRASS_VARIANT;
    let seed = (cell.i + 1) * 1664525;
    const random = () => { seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0; return seed / 4294967296; };
    for (let k = 0; k < perCell; k++) {
      const x = cell.x + (random() - 0.5) * 1.7, z = cell.z + (random() - 0.5) * 1.85;
      const [wx,wz] = warp(x,z), [c,r] = hexAt(board.layout,wx,wz);
      if (board.cell(c,r).i !== cell.i) continue;
      const y = heightAt(x,z) + 0.005, angle = random() * Math.PI * 2;
      const height = tall ? 0.9 + random() * 0.55 : 0.30 + random() * 0.42, width = 0.012 + random() * 0.014;
      const lean = 0.08 + random() * 0.16, leanAngle = angle + (random() - 0.5) * 1.6;
      const lx = Math.cos(leanAngle) * lean, lz = Math.sin(leanAngle) * lean;
      const dx = Math.cos(angle) * width, dz = Math.sin(angle) * width;
      // Keep the whole leaning leaf supported, including its tip and edges, not just its root.
      let contained = supported(x, z, cell.groundY);
      for (let segment = 0; segment <= 3 && contained; segment++) {
        const t = segment / 3, taper = 1 - t, bend = t * t;
        for (const sign of [-1, 0, 1]) {
          if (!supported(x + lx * bend + sign * dx * taper, z + lz * bend + sign * dz * taper, cell.groundY)) contained = false;
        }
      }
      if (!contained) continue;
      const base = positions.length / 3, shade = 0.85 + random() * 0.3;
      for (let segment = 0; segment <= 3; segment++) {
        const t = segment / 3, taper = 1 - t, bend = t * t;
        for (const sign of [-1,1]) {
          positions.push(x + lx * bend + sign * dx * taper, y + height * t, z + lz * bend + sign * dz * taper);
          colors.push((0.09 + t * 0.08) * shade, (0.23 + t * 0.18) * shade, (0.035 + t * 0.025) * shade);
        }
        if (segment < 3) { const a = base + segment * 2; indices.push(a,a+2,a+1,a+1,a+2,a+3); }
      }
    }
  }
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute('position', new THREE.Float32BufferAttribute(positions,3));
  geometry.setAttribute('color', new THREE.Float32BufferAttribute(colors,3));
  geometry.setIndex(indices); geometry.computeVertexNormals(); geometry.computeBoundingSphere();
  return geometry;
}

export class MeadowGrass {
  readonly group = new THREE.Group();
  private readonly material = new THREE.MeshStandardMaterial({ vertexColors: true, side: THREE.DoubleSide, roughness: 0.95, metalness: 0 });
  private mesh: THREE.Mesh | null = null;
  rebuild(board: Board, heightAt: (x: number, z: number) => number): void {
    if (this.mesh) { this.group.remove(this.mesh); this.mesh.geometry.dispose(); this.mesh = null; }
    const geometry = buildMeadowGrass(board,heightAt);
    if (!geometry.getAttribute('position').count) { geometry.dispose(); return; }
    this.mesh = new THREE.Mesh(geometry,this.material);
    this.mesh.receiveShadow = true;
    this.group.add(this.mesh);
  }
  dispose(): void { this.mesh?.geometry.dispose(); this.group.clear(); this.material.dispose(); }
}
