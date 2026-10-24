import * as THREE from "three";
import { GLTFLoader } from "three/addons/loaders/GLTFLoader.js";
import { mergeGeometries } from "three/addons/utils/BufferGeometryUtils.js";

/** Renderer-owned cache: each placed tree has one draw and shared immutable geometry. */
export class ThreeTrees {
  private templates = new Map<string, THREE.BufferGeometry>();
  private maps = new Map<string, THREE.Texture>();
  private loading = new Set<string>();
  private disposed = false;
  revision = 0;

  create(kind: "broadleaf" | "snowy-pine" | "dead-oak" | "dead-snag" | "twisted-stump" | "grey-outcrop" | "tavern-barrel" | "tavern-chair" | "tavern-candlestick" | "tavern-mug" | "tavern-table" | "watchtower-signal-brazier-v01" | "watchtower-arms-rack-v01" | "watchtower-command-table-v01" | "watchtower-signal-brazier-v02" | "watchtower-arms-rack-v02" | "watchtower-command-table-v02", tile: number): THREE.Mesh | null {
    const template = this.templates.get(kind);
    if (!template) {
      if (!this.loading.has(kind)) {
        this.loading.add(kind);
        const watchtowerProp = kind.startsWith("watchtower-");
        const modelName = kind.startsWith("tavern-") || watchtowerProp ? kind : `${kind}-photo`;
        const folder = kind.startsWith("tavern-") || watchtowerProp ? "props" : kind === "grey-outcrop" ? "rocks" : "trees";
        new GLTFLoader().load(`/game/models/${folder}/${modelName}.glb`, gltf => {
          const parts: THREE.BufferGeometry[] = [];
          gltf.scene.updateMatrixWorld(true);
          gltf.scene.traverse(object => {
            if (!(object instanceof THREE.Mesh)) return;
            const source = object.geometry;
            const map = (object.material as THREE.MeshStandardMaterial).map;
            if (map) this.maps.set(kind, map);
            const geometry = source.index ? source.toNonIndexed() : source.clone();
            geometry.applyMatrix4(object.matrixWorld);
            geometry.rotateX(Math.PI / 2); // glTF Y-up -> battle Z-up.
            const position = geometry.getAttribute("position");
            const original = geometry.getAttribute("color");
            const color = (object.material as THREE.MeshStandardMaterial).color;
            const colors = new Float32Array(position.count * 3);
            for (let i = 0; i < position.count; i++) {
              colors[i * 3] = color.r * (original?.getX(i) ?? 1);
              colors[i * 3 + 1] = color.g * (original?.getY(i) ?? 1);
              colors[i * 3 + 2] = color.b * (original?.getZ(i) ?? 1);
            }
            for (const name of Object.keys(geometry.attributes)) if (name !== "position" && name !== "normal" && name !== "uv") geometry.deleteAttribute(name);
            if (!geometry.getAttribute("normal")) geometry.computeVertexNormals();
            geometry.setAttribute("color", new THREE.BufferAttribute(colors, 3));
            parts.push(geometry);
            source.dispose();
            const materials = Array.isArray(object.material) ? object.material : [object.material];
            materials.forEach(material => material.dispose());
          });
          const merged = mergeGeometries(parts);
          parts.forEach(part => part.dispose());
          if (!merged) return;
          if (this.disposed) { merged.dispose(); return; }
          merged.computeBoundingBox();
          const box = merged.boundingBox!;
          merged.translate(0, 0, -box.min.z);
          merged.scale(1 / (box.max.z - box.min.z), 1 / (box.max.z - box.min.z), 1 / (box.max.z - box.min.z));
          merged.computeBoundingSphere();
          this.templates.set(kind, merged);
          this.revision++;
        }, undefined, error => { console.error(`Could not load ${kind} tree`, error); });
      }
      return null;
    }
    const mesh = new THREE.Mesh(template, new THREE.MeshStandardMaterial({ vertexColors: true, map: this.maps.get(kind), metalness: kind === "tavern-mug" ? 0.8 : 0, roughness: kind === "tavern-mug" ? 0.5 : 0.9, side: THREE.DoubleSide }));
    mesh.scale.setScalar(tile * (kind === "grey-outcrop" ? 2.3 : kind === "tavern-mug" ? 0.7 : kind === "tavern-candlestick" ? 1.1 : kind === "watchtower-signal-brazier-v01" || kind === "watchtower-signal-brazier-v02" ? 1.65 : kind === "watchtower-arms-rack-v01" || kind === "watchtower-arms-rack-v02" ? 2.0 : kind === "watchtower-command-table-v01" || kind === "watchtower-command-table-v02" ? 1.1 : kind.startsWith("tavern-") ? 2 : 4.8));
    mesh.castShadow = mesh.receiveShadow = true;
    mesh.userData.importedTree = true;
    return mesh;
  }

  dispose(): void {
    this.disposed = true;
    for (const geometry of this.templates.values()) geometry.dispose();
    this.templates.clear();
    for (const map of this.maps.values()) map.dispose();
    this.maps.clear();
  }
}
