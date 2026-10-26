import * as THREE from "three";
import { GLTFLoader } from "three/addons/loaders/GLTFLoader.js";
import { mergeGeometries } from "three/addons/utils/BufferGeometryUtils.js";

/** Renderer-owned cache: each placed tree has one draw and shared immutable geometry. */
export class ThreeTrees {
  private templates = new Map<string, THREE.BufferGeometry>();
  private importedProps = new Map<string, THREE.Object3D>();
  private maps = new Map<string, THREE.Texture>();
  private loading = new Set<string>();
  private disposed = false;
  revision = 0;

  create(kind: "broadleaf" | "snowy-pine" | "dead-oak" | "dead-snag" | "twisted-stump" | "watchtower-leafy-oak-v01" | "watchtower-leafy-willow-v01" | "watchtower-leafy-birch-v01" | "grey-outcrop" | "tavern-barrel" | "tavern-chair" | "tavern-candlestick" | "tavern-mug" | "tavern-table" | "tavern-table-small-v01" | "tavern-table-messhall-v01" | "tavern-chair-v04" | "vau-gothic-cottage-v01" | "vau-well-v01" | "watchtower-signal-brazier-v01" | "watchtower-arms-rack-v01" | "watchtower-command-table-v01" | "watchtower-signal-brazier-v02" | "watchtower-arms-rack-v02" | "watchtower-command-table-v02", tile: number): THREE.Object3D | null {
    if (kind === "vau-gothic-cottage-v01" || kind === "vau-well-v01") return this.createImportedProp(kind, tile);
    const template = this.templates.get(kind);
    if (!template) {
      if (!this.loading.has(kind)) {
        this.loading.add(kind);
        const watchtowerProp = kind.startsWith("watchtower-");
        const watchtowerLeafyTree = kind.startsWith("watchtower-leafy-");
        const modelName = kind.startsWith("tavern-") || watchtowerProp || watchtowerLeafyTree ? kind : `${kind}-photo`;
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
    mesh.scale.setScalar(tile * (kind === "grey-outcrop" ? 2.3 : kind === "tavern-mug" ? 0.7 : kind === "tavern-candlestick" ? 1.1 : kind === "tavern-table-small-v01" ? 1.12 : kind === "tavern-table-messhall-v01" ? 1.15 : kind === "tavern-chair-v04" ? 1.47 : kind === "watchtower-signal-brazier-v01" || kind === "watchtower-signal-brazier-v02" ? 1.65 : kind === "watchtower-arms-rack-v01" || kind === "watchtower-arms-rack-v02" ? 2.0 : kind === "watchtower-command-table-v01" || kind === "watchtower-command-table-v02" ? 1.1 : kind.startsWith("tavern-") ? 2 : 4.8));
    mesh.castShadow = mesh.receiveShadow = true;
    mesh.userData.importedTree = true;
    return mesh;
  }

  private createImportedProp(kind: "vau-gothic-cottage-v01" | "vau-well-v01", tile: number): THREE.Object3D | null {
    const template = this.importedProps.get(kind);
    if (!template) {
      if (!this.loading.has(kind)) {
        this.loading.add(kind);
        const path = kind === "vau-gothic-cottage-v01"
          ? "/game/props/gothic-cottage/v04/gothic-cottage-v04.glb"
          : "/game/models/props/vau-well-v01.glb";
        const assetUrl = import.meta.env.DEV ? `/public${path}` : path;
        new GLTFLoader().load(assetUrl, gltf => {
          const root = new THREE.Group();
          if (kind === "vau-gothic-cottage-v01") {
            const high = gltf.scene.getObjectByName("GothicCottage_LOD0");
            const low = gltf.scene.getObjectByName("GothicCottage_LOD1");
            if (!high || !low) { console.error("Cottage asset is missing a named LOD"); return; }
            const lod = new THREE.LOD();
            lod.addLevel(high.clone(true), 0);
            lod.addLevel(low.clone(true), 24);
            root.add(lod);
            const glass = gltf.scene.getObjectByName("GothicCottage_Glass");
            if (glass) root.add(glass.clone(true));
          } else {
            root.add(gltf.scene.clone(true));
          }
          // Blender's glTF export uses Y-up; the game board uses Z-up.
          root.rotation.x = Math.PI / 2;
          root.updateMatrixWorld(true);
          const bounds = new THREE.Box3().setFromObject(root);
          const size = bounds.getSize(new THREE.Vector3());
          const height = Math.max(size.z, 0.001);
          const center = bounds.getCenter(new THREE.Vector3());
          root.scale.setScalar(1 / height);
          root.position.set(-center.x / height, -center.y / height, -bounds.min.z / height);
          root.traverse(object => {
            if (!(object instanceof THREE.Mesh)) return;
            object.castShadow = object.name !== "GothicCottage_Glass";
            object.receiveShadow = true;
          });
          if (this.disposed) return;
          this.importedProps.set(kind, root);
          this.revision++;
        }, undefined, error => { console.error(`Could not load ${kind}`, error); });
      }
      return null;
    }
    const object = new THREE.Group();
    object.add(template.clone(true));
    object.scale.setScalar(tile * (kind === "vau-gothic-cottage-v01" ? 5.8 : 2.4));
    object.traverse(child => {
      if (child instanceof THREE.Mesh) child.castShadow = child.receiveShadow = true;
    });
    object.userData.importedTree = true;
    return object;
  }

  dispose(): void {
    this.disposed = true;
    for (const geometry of this.templates.values()) geometry.dispose();
    this.templates.clear();
    this.importedProps.clear();
    for (const map of this.maps.values()) map.dispose();
    this.maps.clear();
  }
}
