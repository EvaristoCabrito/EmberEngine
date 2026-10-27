import * as THREE from 'three';
import type { Board } from '../map/board';
import { hexAt } from '../core/hex';
import { warp } from './terrainMesh';

const MEADOW_GRASS_VARIANTS = new Set([52, 59, 60, 66, 67, 68, 69]);

/** Sparse tapered leaves above the photographic plains grounds. */
export function buildMeadowGrass(board: Board, heightAt: (x: number, z: number) => number): THREE.BufferGeometry {
  const cells = board.cells.filter(c => c.type === 'plains' && MEADOW_GRASS_VARIANTS.has(board.draft.tileVariants?.[c.i] ?? -1));
  const positions: number[] = [], colors: number[] = [], indices: number[] = [], bladeT: number[] = [], bladeSeed: number[] = [];
  const bounds = board.bounds();
  const supported = (x: number, z: number, groundY: number): boolean => {
    // hexAt clamps outside points to a border cell; check the actual mesh bounds first.
    if (x <= bounds.x0 + 0.04 || x >= bounds.x1 - 0.04 || z <= bounds.z0 + 0.04 || z >= bounds.z1 - 0.04) return false;
    const [wx, wz] = warp(x, z), [c, r] = hexAt(board.layout, wx, wz);
    const under = board.cell(c, r);
    return under.surface >= 0 && Math.abs(under.groundY - groundY) < 0.01;
  };
  for (const cell of cells) {
    const variant = board.draft.tileVariants?.[cell.i] ?? -1;
    const legacyDenseGrass = variant === 59 || variant === 60;
    const perCell = legacyDenseGrass
      ? Math.min(180, Math.max(24, Math.floor(90000 / Math.max(1, cells.length))))
      : Math.min(18, Math.max(12, Math.floor(7000 / Math.max(1, cells.length))));
    let seed = (cell.i + 1) * 1664525;
    const random = () => { seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0; return seed / 4294967296; };
    for (let k = 0; k < perCell; k++) {
      const x = cell.x + (random() - 0.5) * 1.7, z = cell.z + (random() - 0.5) * 1.85;
      const [wx,wz] = warp(x,z), [c,r] = hexAt(board.layout,wx,wz);
      if (board.cell(c,r).i !== cell.i) continue;
      const y = heightAt(x,z) + 0.005, angle = random() * Math.PI * 2;
      const tall = variant === 60;
      const height = tall ? 0.9 + random() * 0.55 : 0.30 + random() * 0.42, width = 0.012 + random() * 0.014;
      const windSeed = random();
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
          bladeT.push(t);
          bladeSeed.push(windSeed);
        }
        if (segment < 3) { const a = base + segment * 2; indices.push(a,a+2,a+1,a+1,a+2,a+3); }
      }
    }
  }
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute('position', new THREE.Float32BufferAttribute(positions,3));
  geometry.setAttribute('color', new THREE.Float32BufferAttribute(colors,3));
  geometry.setAttribute('grassT', new THREE.Float32BufferAttribute(bladeT,1));
  geometry.setAttribute('grassSeed', new THREE.Float32BufferAttribute(bladeSeed,1));
  geometry.setIndex(indices); geometry.computeVertexNormals(); geometry.computeBoundingSphere();
  return geometry;
}

export class MeadowGrass {
  readonly group = new THREE.Group();
  private readonly windTime = { value: 0 };
  private readonly material: THREE.MeshStandardMaterial;
  private mesh: THREE.Mesh | null = null;
  private animationFrame: number | null = null;

  constructor() {
    this.material = new THREE.MeshStandardMaterial({ vertexColors: true, side: THREE.DoubleSide, roughness: 0.95, metalness: 0 });
    this.material.onBeforeCompile = shader => {
      shader.uniforms.uGrassWindTime = this.windTime;
      shader.vertexShader = shader.vertexShader.replace(
        '#include <common>',
        '#include <common>\nattribute float grassT;\nattribute float grassSeed;\nuniform float uGrassWindTime;',
      ).replace(
        '#include <begin_vertex>',
        '#include <begin_vertex>\nfloat grassPhase = uGrassWindTime * 1.15 + grassSeed * 6.2831853;\nfloat grassBend = sin(grassPhase) * grassT * grassT;\ntransformed.x += grassBend * 0.035;\ntransformed.z += cos(grassPhase * 0.83 + grassSeed) * grassT * grassT * 0.02;',
      );
    };
    this.material.customProgramCacheKey = () => 'plains-meadow-wind-v1';
  }

  private readonly animate = (now: number): void => {
    this.windTime.value = now * 0.001;
    this.animationFrame = requestAnimationFrame(this.animate);
  };

  rebuild(board: Board, heightAt: (x: number, z: number) => number): void {
    if (this.mesh) { this.group.remove(this.mesh); this.mesh.geometry.dispose(); this.mesh = null; }
    const geometry = buildMeadowGrass(board,heightAt);
    if (!geometry.getAttribute('position').count) {
      geometry.dispose();
      if (this.animationFrame !== null) cancelAnimationFrame(this.animationFrame);
      this.animationFrame = null;
      return;
    }
    this.mesh = new THREE.Mesh(geometry,this.material);
    this.mesh.receiveShadow = true;
    this.group.add(this.mesh);
    if (this.animationFrame === null) this.animationFrame = requestAnimationFrame(this.animate);
  }
  dispose(): void {
    if (this.animationFrame !== null) cancelAnimationFrame(this.animationFrame);
    this.animationFrame = null;
    this.mesh?.geometry.dispose(); this.group.clear(); this.material.dispose();
  }
}
