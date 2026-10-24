import * as THREE from "three";
import { groundDepthLayer } from "./groundDepthLayer";

/** Raised faces for the standard 2D battle camera. Terrain tops are lifted by the renderer;
 * this mesh fills the exposed sides so high hexes read as raised ground instead of inset discs. */
export class ThreeElevationSteps {
  readonly group = new THREE.Group();
  private material = new THREE.MeshBasicMaterial({
    vertexColors: true,
    side: THREE.DoubleSide,
    toneMapped: false,
  });

  constructor(depthLayer?: { value: number }) {
    if (depthLayer) groundDepthLayer(this.material, depthLayer);
  }

  rebuild(cols: number, rows: number, tile: number, height: (col: number, row: number) => number, stepHeight: number): void {
    for (const child of this.group.children) {
      if (child instanceof THREE.Mesh) child.geometry.dispose();
    }
    this.group.clear();
    const positions: number[] = [];
    const colors: number[] = [];
    const indices: number[] = [];
    const levels = (col: number, row: number) => col < 0 || row < 0 || col >= cols || row >= rows ? 0 : height(col, row);
    const shades = [0x493d2f, 0x30261b, 0x493d2f, 0x554832, 0x554832, 0x493d2f];

    for (let row = 0; row < rows; row++) for (let col = 0; col < cols; col++) {
      const level = levels(col, row);
      if (level <= 0) continue;
      const right = row & 1;
      const neighbors = [
        [col + 1, row], [col + right, row + 1], [col + right - 1, row + 1],
        [col - 1, row], [col + right - 1, row - 1], [col + right, row - 1],
      ].map(([c, r]) => levels(c!, r!));
      const centerX = tile * Math.sqrt(3) * (col + 0.5 * right + 0.5);
      const centerY = -tile * (2.4 + 1.5 * row + 1);
      const point = (edge: number) => {
        const angle = (edge * 60 - 30) * Math.PI / 180;
        return [centerX + Math.cos(angle) * tile, centerY + Math.sin(angle) * tile] as const;
      };

      for (let edge = 0; edge < 6; edge++) {
        const lower = neighbors[edge] ?? 0;
        if (level <= lower) continue;
        const a = point(edge), b = point((edge + 1) % 6);
        const bottomZ = lower * stepHeight, topZ = level * stepHeight;
        const start = positions.length / 3;
        positions.push(
          a[0], a[1], bottomZ, b[0], b[1], bottomZ,
          b[0], b[1], topZ, a[0], a[1], topZ,
        );
        const color = new THREE.Color(shades[edge]!);
        for (let i = 0; i < 4; i++) colors.push(color.r, color.g, color.b);
        indices.push(start, start + 1, start + 2, start, start + 2, start + 3);
      }
    }

    if (!positions.length) return;
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute("position", new THREE.Float32BufferAttribute(positions, 3));
    geometry.setAttribute("color", new THREE.Float32BufferAttribute(colors, 3));
    geometry.setIndex(indices);
    geometry.computeVertexNormals();
    geometry.computeBoundingSphere();
    this.group.add(new THREE.Mesh(geometry, this.material));
  }

  dispose(): void {
    this.group.removeFromParent();
    for (const child of this.group.children) {
      if (child instanceof THREE.Mesh) child.geometry.dispose();
    }
    this.group.clear();
    this.material.dispose();
  }
}
