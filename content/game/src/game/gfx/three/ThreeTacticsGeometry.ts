import * as THREE from "three";
import { mergeGeometries } from "three/examples/jsm/utils/BufferGeometryUtils.js";

/** These meshes use Z as up; Three's default hemisphere assumes Y is up. */
export function lightTacticsMaterial(material: THREE.MeshStandardMaterial): void {
  material.onBeforeCompile = shader => {
    shader.fragmentShader = shader.fragmentShader.replace("#include <lights_pars_begin>",
      THREE.ShaderChunk.lights_pars_begin.replace(
        "float dotNL = dot( normal, hemiLight.direction );",
        "float dotNL = dot( normal, (viewMatrix * vec4(0.0, 0.0, 1.0, 0.0)).xyz );"));
  };
  material.customProgramCacheKey = () => "tactics-z-up-lighting-v1";
}

const TREES = new Set(["dead-tree", "dead-tree-large", "wilds-twisted-tree", "wilds-gibbet-tree", "wilds-snowy-dead-tree", "wilds-ancestral-tree", "dense-forest"]);
const ROCKS = new Set(["spike-rocks", "spike-rocks-2", "rocky-outcrop", "boulder-pile", "large-boulder", "mossy-rocks", "twin-spires"]);
const HOUSES = new Set(["small-house", "stone-hut"]);

/** First tactics-view models; each owns its materials so occlusion fading is per prop. */
export function tacticsProp(id: string, w: number, h: number, stone: THREE.Texture, timber: THREE.Texture): THREE.Mesh | null {
  // Keep authored tree artwork; generic branch models do not match these maps.
  if (TREES.has(id)) return null;
  if (!TREES.has(id) && !ROCKS.has(id) && !HOUSES.has(id)) return null;
  const parts: THREE.BufferGeometry[][] = [];
  const materials: THREE.MeshStandardMaterial[] = [];
  const materialIndices = new Map<string, number>();
  const add = (geometry: THREE.BufferGeometry, color: number, map?: THREE.Texture) => {
    if (!geometry.getAttribute("uv")) {
      const position = geometry.getAttribute("position");
      const uv = new Float32Array(position.count * 2);
      for (let i = 0; i < position.count; i++) {
        uv[i * 2] = position.getX(i) / w + 0.5;
        uv[i * 2 + 1] = position.getZ(i) / h;
      }
      geometry.setAttribute("uv", new THREE.BufferAttribute(uv, 2));
    }
    const key = `${color}:${map?.uuid ?? "none"}`;
    let index = materialIndices.get(key);
    if (index === undefined) {
      index = materials.length;
      materialIndices.set(key, index);
      parts.push([]);
      const material = new THREE.MeshStandardMaterial({ color, ...(map ? { map } : {}), roughness: 1, flatShading: true, transparent: true });
      lightTacticsMaterial(material);
      materials.push(material);
    }
    parts[index]!.push(geometry.index ? geometry.toNonIndexed() : geometry);
    if (geometry.index) geometry.dispose();
  };
  if (TREES.has(id)) {
    const trunkH = h * 0.75;
    add(new THREE.CylinderGeometry(w * 0.035, w * 0.075, trunkH, 7).rotateX(Math.PI / 2).translate(0, 0, trunkH / 2), 0x80664d, timber);
    for (let i = 0; i < 7; i++) {
      const az = i * 2.399;
      const start = new THREE.Vector3(0, 0, trunkH * (0.3 + i * 0.08));
      const end = new THREE.Vector3(Math.cos(az) * w * 0.35, Math.sin(az) * w * 0.35, start.z + h * 0.24);
      const direction = end.clone().sub(start);
      const branch = new THREE.CylinderGeometry(w * 0.008, w * 0.023, direction.length(), 5);
      branch.applyQuaternion(new THREE.Quaternion().setFromUnitVectors(new THREE.Vector3(0, 1, 0), direction.normalize()));
      branch.translate(...start.clone().add(end).multiplyScalar(0.5).toArray());
      add(branch, 0x80664d, timber);
    }
    if (id === "dense-forest" || id === "wilds-ancestral-tree") {
      for (let i = 0; i < 3; i++) add(new THREE.IcosahedronGeometry(1, 1).scale(w * 0.3, w * 0.25, h * 0.24).translate((i - 1) * w * 0.2, 0, h * 0.72), 0x4e6035);
    }
  } else if (ROCKS.has(id)) {
    const jagged = id.includes("spike") || id.includes("spire");
    const count = id === "rocky-outcrop" ? 14 : 7;
    for (let i = 0; i < count; i++) {
      const main = i < 4;
      const angle = i * 2.399;
      const spread = w * (main ? 0.2 : 0.4);
      const radius = w * (main ? 0.23 : 0.07 + (i % 3) * 0.025);
      const height = h * (main ? (jagged ? 0.7 : i === 0 ? 0.66 : 0.37) : 0.12);
      const rock = new THREE.IcosahedronGeometry(1, 2);
      const positions = rock.getAttribute("position");
      for (let j = 0; j < positions.count; j++) {
        const x = positions.getX(j), y = positions.getY(j), z = positions.getZ(j);
        const irregularity = 1 + 0.16 * Math.sin(x * 13 + y * 7 + z * 19 + i * 3);
        positions.setXYZ(j, x * irregularity, y * irregularity, z * irregularity);
      }
      rock.scale(radius, radius * 0.85, height / 2).rotateZ(angle).translate(Math.cos(angle) * spread, Math.sin(angle) * spread, height / 2);
      rock.computeVertexNormals();
      add(rock, id === "mossy-rocks" ? 0x68745b : 0x77776d, stone);
    }
  } else {
    const depth = w * 0.78, wallH = h * 0.48, roofH = h * 0.38;
    const beam = w * 0.035;
    // Weathered timber cottage: steep gable roof, exposed frame, planks and dark openings.
    add(new THREE.BoxGeometry(w, depth, wallH).translate(0, 0, wallH / 2), 0x8c8065, timber);
    const gable = new THREE.BufferGeometry();
    gable.setAttribute("position", new THREE.Float32BufferAttribute([
      -w/2,-depth/2,wallH, w/2,-depth/2,wallH, 0,-depth/2,wallH+roofH,
      w/2,depth/2,wallH, -w/2,depth/2,wallH, 0,depth/2,wallH+roofH,
    ],3));
    gable.computeVertexNormals();
    add(gable, 0x8c8065, timber);
    const slope = Math.atan2(roofH, w * 0.56);
    const roofLength = Math.hypot(w * 0.56, roofH);
    for (const sign of [-1, 1]) {
      for (let row = 0; row < 13; row++) {
        const t = (row + 0.5) / 13;
        for (let col = 0; col < 10; col++) {
          const shingle = new THREE.BoxGeometry(roofLength/13*1.1, depth/10*1.12, beam*.28);
          shingle.rotateY(sign*slope).translate(sign*w*.56*(1-t), (col-4.5)*depth/10, wallH+roofH*t);
          add(shingle, (row+col)%3 === 0 ? 0x48453c : 0x5e584a, timber);
        }
      }
    }
    for (const x of [-w/2,0,w/2]) for (const y of [-depth/2,depth/2])
      add(new THREE.BoxGeometry(beam,beam,wallH).translate(x,y,wallH/2),0x4c3b27,timber);
    for (const z of [beam,wallH*.63,wallH]) for (const y of [-depth/2,depth/2])
      add(new THREE.BoxGeometry(w+beam,beam,beam).translate(0,y,z),0x4c3b27,timber);
    for (const x of [-w/2,w/2])
      add(new THREE.BoxGeometry(beam,depth,beam).translate(x,0,wallH*.6),0x4c3b27,timber);
    add(new THREE.BoxGeometry(w*.19,beam*.6,wallH*.77).translate(-w*.19,-depth/2-beam*.6,wallH*.385),0x30281e,timber);
    for (const x of [w*.23,-w*.22]) {
      const z = x < 0 ? wallH*.87 : wallH*.47;
      add(new THREE.BoxGeometry(w*.17,beam*.4,wallH*.22).translate(x,-depth/2-beam,z),0x151912);
      add(new THREE.BoxGeometry(beam*.5,beam*.6,wallH*.25).translate(x,-depth/2-beam*1.3,z),0x54422c,timber);
      add(new THREE.BoxGeometry(w*.19,beam*.6,beam*.5).translate(x,-depth/2-beam*1.3,z),0x54422c,timber);
    }
  }
  // Indexed primitives must all have the same attribute set for merging.
  // A detailed shingle roof still draws once per material, rather than once per shingle.
  const batches = parts.map(batch => mergeGeometries(batch)!);
  const geometry = mergeGeometries(batches, true)!;
  parts.flat().forEach(part => part.dispose());
  batches.forEach(part => part.dispose());
  const mesh = new THREE.Mesh(geometry, materials);
  mesh.castShadow = mesh.receiveShadow = true;
  mesh.userData.tacticsModel = true;
  mesh.userData.tacticsArchitecture = HOUSES.has(id);
  return mesh;
}
