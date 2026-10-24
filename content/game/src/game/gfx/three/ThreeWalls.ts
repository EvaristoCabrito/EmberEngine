import * as THREE from "three";
import { mergeGeometries } from "three/examples/jsm/utils/BufferGeometryUtils.js";
import type { DecorationDef } from "../../types";
import { createCastleGeometry } from "./ThreeCastle";
import { createTempleGeometry } from "./ThreeTemple";
import { createRockGeometry } from "./ThreeRocks";
import { createReferenceDoorGeometry } from "./ThreeReferenceDoors";

/** Match sprite ground-line depth without changing the physical shadow geometry. */
export function configureWallDepth(material: THREE.MeshStandardMaterial, tile: number, base: number, perTile: number): void {
  material.onBeforeCompile = shader => {
    shader.uniforms.wallTile = { value: tile };
    shader.uniforms.wallDepthBase = { value: base };
    shader.uniforms.wallDepthPerTile = { value: perTile };
    shader.vertexShader = `uniform float wallTile;\nuniform float wallDepthBase;\nuniform float wallDepthPerTile;\nvarying float wallGroundDepth;\n${shader.vertexShader}`
      .replace("#include <project_vertex>", `#include <project_vertex>
        vec4 wallGround = modelMatrix * vec4(transformed.x, transformed.y - transformed.z * 3.5, 0.0, 1.0);
        float wallZ = wallDepthBase + (-wallGround.y / wallTile) * wallDepthPerTile;
        vec4 wallDepthClip = projectionMatrix * viewMatrix * vec4(wallGround.xy, wallZ, 1.0);
        wallGroundDepth = wallDepthClip.z / wallDepthClip.w * 0.5 + 0.5;
      `)
      .replace("vViewPosition = - mvPosition.xyz;", `
        vec4 physicalPosition = modelMatrix * vec4(transformed.x, transformed.y - transformed.z * 3.5, transformed.z, 1.0);
        physicalPosition.z -= 1.0;
        vViewPosition = -(viewMatrix * physicalPosition).xyz;
      `)
      .replace("#include <worldpos_vertex>", `#include <worldpos_vertex>
        #ifdef USE_SHADOWMAP
          worldPosition.y -= transformed.z * 3.5;
          worldPosition.z -= 1.0;
        #endif
      `);
    shader.fragmentShader = `varying float wallGroundDepth;\n${shader.fragmentShader}`
      // Architecture stands on Z-up ground. Keep its ambient hemisphere aligned to
      // that physical up axis instead of treating south-facing walls as underground.
      .replace("#include <lights_pars_begin>", THREE.ShaderChunk.lights_pars_begin.replace(
        "float dotNL = dot( normal, hemiLight.direction );",
        "float dotNL = dot( normal, (viewMatrix * vec4(0.0, 0.0, 1.0, 0.0)).xyz );",
      ))
      .replace("#include <dithering_fragment>", "#include <dithering_fragment>\n gl_FragDepth = wallGroundDepth;");
  };
  material.customProgramCacheKey = () => "architecture-physical-shadows-v4";
}

/** Remove internal caps at joins so they cannot cast a seam onto a neighboring wall. */
function omitBoxFaces(box: THREE.BoxGeometry, faces: number[]): THREE.BoxGeometry {
  const indices = Array.from(box.index!.array).filter((_, i) => !faces.includes(Math.floor(i / 6)));
  box.setIndex(indices);
  box.clearGroups();
  return box;
}

/** Solid joining walls, following the centers of neighboring architecture cells. */
export function createWallGeometry(def: DecorationDef, tile: number, rotation: number, connections: { x: number; y: number }[] = [], origin = { x: 0, y: 0 }, projectHeight = true, textureHeightScale = 1): THREE.BufferGeometry {
  const height = tile * 0.75 * (def.heightScale ?? 1);
  let geometry: THREE.BufferGeometry;
  const referenceDoor = def.doorStyle === "stoneOak" || def.doorStyle === "dungeonOak";
  if (def.id === "watchtower-stone-open-door-2hex") {
    // Cut an opening out of the same wall volume: identical top, base and depth.
    const width = tile * ((rotation & 1) ? 1.5 : Math.sqrt(3)) * 2;
    const depth = tile * 0.32 * (def.wallThicknessScale ?? 1);
    const post = depth, lintel = height * 0.16;
    const parts = [
      new THREE.BoxGeometry(width, depth, lintel).translate(0, 0, height - lintel / 2),
      ...[-1, 1].map(side => new THREE.BoxGeometry(post, depth, height - lintel)
        .translate(side * (width - post) / 2, 0, (height - lintel) / 2)),
    ];
    geometry = mergeGeometries(parts)!;
    parts.forEach(part => part.dispose());
    geometry.rotateZ(-rotation * Math.PI / 2);
  } else if (referenceDoor) {
    geometry = createReferenceDoorGeometry(def, tile, height, rotation);
  } else if (def.rockStyle) {
    geometry = createRockGeometry(def, tile, height, rotation);
  } else if (def.templeStyle) {
    geometry = createTempleGeometry(def, tile, height, rotation, connections);
  } else if (def.castleStyle) {
    geometry = createCastleGeometry(def, tile, height, rotation, connections);
  } else if (def.model3d === "wall" || def.model3d === "secretDoor") {
    const thickness = tile * 0.32 * (def.wallThicknessScale ?? 1);
    const parts: THREE.BufferGeometry[] = [];
    if (connections.length === 0 || def.architectureSpan) {
      const length = tile * ((rotation & 1) ? 1.5 : Math.sqrt(3)) * (def.architectureSpan ?? 1);
      parts.push(new THREE.BoxGeometry(length, thickness, height).rotateZ(-rotation * Math.PI / 2).translate(0, 0, height / 2));
    } else {
      const openFaces = connections.map(({ x, y }) => Math.abs(x) > Math.abs(y) ? (x > 0 ? 0 : 1) : (y > 0 ? 2 : 3));
      parts.push(omitBoxFaces(new THREE.BoxGeometry(thickness, thickness, height), openFaces).translate(0, 0, height / 2));
      for (const connection of connections) {
        // Each neighbor contributes half a segment. A tiny overlap prevents raster seams.
        const length = Math.hypot(connection.x, connection.y) / 2 + tile * 0.01;
        parts.push(omitBoxFaces(new THREE.BoxGeometry(length, thickness, height), [0, 1])
          .rotateZ(Math.atan2(connection.y, connection.x))
          .translate(connection.x / 4, connection.y / 4, height / 2));
      }
    }
    geometry = mergeGeometries(parts)!;
    parts.forEach(part => part.dispose());
  } else {
    const width = tile * ((rotation & 1) ? 1.5 : Math.sqrt(3)) * (def.architectureSpan ?? 1), post = tile * 0.18, depth = tile * 0.32 * (def.wallThicknessScale ?? 1);
    const box = (w: number, h: number, z: number, x: number, elevation: number) =>
      new THREE.BoxGeometry(w, h, z).translate(x, 0, elevation);
    const radius = (width - post * 2) / 2;
    const archRise = Math.min(radius, height * 0.4);
    // The normal camera projects physical height upward by 3.5x. A full-size lintel
    // therefore reads much thicker there and makes the usable doorway look squat.
    // Reduce its physical fraction so its projected thickness matches tactics mode.
    const header = height * (projectHeight ? 0.1 : 0.18);
    const spring = height - header - archRise;
    const frame = new THREE.Shape();
    frame.moveTo(-width / 2, 0);
    frame.lineTo(-width / 2, height);
    frame.lineTo(width / 2, height);
    frame.lineTo(width / 2, 0);
    frame.lineTo(radius, 0);
    frame.lineTo(radius, spring);
    frame.absellipse(0, spring, radius, archRise, 0, Math.PI, false, 0);
    frame.lineTo(-radius, 0);
    frame.closePath();
    const extrude = (shape: THREE.Shape, thickness: number) => new THREE.ExtrudeGeometry(shape,
      { depth: thickness, bevelEnabled: false, curveSegments: 16 }).rotateX(Math.PI / 2).translate(0, thickness / 2, 0);
    const parts: THREE.BufferGeometry[] = [extrude(frame, depth)];
    if (def.model3d === "door") {
      const leafRadius = radius - tile * 0.025;
      const leaf = new THREE.Shape();
      leaf.moveTo(-leafRadius, 0);
      leaf.lineTo(leafRadius, 0);
      leaf.lineTo(leafRadius, spring);
      leaf.absellipse(0, spring, leafRadius, archRise - tile * 0.025, 0, Math.PI, false, 0);
      leaf.lineTo(-leafRadius, 0);
      leaf.closePath();
      parts.push(extrude(leaf, tile * 0.12));
      const metal = def.doorStyle === "iron" || def.doorStyle === "steel";
      const strapHeight = height * (def.doorStyle === "reinforced" ? 0.09 : 0.06);
      parts.push(
        box(leafRadius * 2, tile * 0.17, strapHeight, 0, spring * 0.25),
        box(leafRadius * 2, tile * 0.17, strapHeight, 0, spring * 0.8),
        box(tile * 0.08, tile * 0.23, height * 0.07, leafRadius * 0.65, spring * 0.5),
      );
      if (def.doorStyle === "steel") {
        // Raised panel ribs distinguish the steel leaf from riveted iron.
        for (const x of [-leafRadius * 0.5, 0, leafRadius * 0.5])
          parts.push(box(tile * 0.055, tile * 0.17, spring * 0.85, x, spring * 0.46));
      }
      if (metal || def.doorStyle === "reinforced") for (const x of [-leafRadius * 0.83, leafRadius * 0.83])
        for (const z of [spring * 0.25, spring * 0.8]) for (const side of [-1, 1])
          parts.push(new THREE.SphereGeometry(tile * 0.035, 6, 4).translate(x, side * tile * 0.095, z));
      if (def.doorStyle === "reinforced") parts.push(box(tile * 0.09, tile * 0.19, spring * 0.92, 0, spring * 0.46));
    }
    const compatible = parts.map(part => part.index ? part.toNonIndexed() : part);
    geometry = mergeGeometries(compatible)!;
    compatible.forEach((part, i) => { if (part !== parts[i]) part.dispose(); });
    parts.forEach(part => part.dispose());
    geometry.rotateZ(-rotation * Math.PI / 2);
  }
  // The battle camera looks down Z. Shear the vertical geometry toward screen-up to
  // present its height in the existing isometric view while preserving the ground grid.
  const positions = geometry.getAttribute("position");
  // Project onto each original face before the camera shear. World coordinates keep
  // the masonry pattern continuous across adjoining segments and corner cores.
  const normals = geometry.getAttribute("normal");
  const uv = geometry.getAttribute("uv");
  const repeatSize = tile;
  if (def.dungeonReference) geometry.computeBoundingBox();
  for (let i = 0; !referenceDoor && !def.castleStyle && !def.templeStyle && !def.rockStyle && i < positions.count; i++) {
    const x = positions.getX(i) + origin.x, y = positions.getY(i) + origin.y, z = positions.getZ(i);
    if (def.dungeonReference) {
      const bounds = geometry.boundingBox!;
      const vertical = (rotation & 1) !== 0;
      const front = Math.abs(vertical ? normals.getX(i) : normals.getY(i)) > 0.5;
      const useX = front !== vertical;
      const local = useX ? positions.getX(i) : positions.getY(i);
      const low = useX ? bounds.min.x : bounds.min.y;
      const span = useX ? bounds.max.x - low : bounds.max.y - low;
      const u = (local - low) / Math.max(0.001, span);
      if (Math.abs(normals.getZ(i)) > 0.5) {
        const capU = (positions.getX(i) - bounds.min.x) / Math.max(0.001, bounds.max.x - bounds.min.x);
        const capV = (positions.getY(i) - bounds.min.y) / Math.max(0.001, bounds.max.y - bounds.min.y);
        uv.setXY(i, (900 + capU * 165) / 1168, 1 - (480 - capV * 260) / 784);
        continue;
      }
      const v = z / height;
      // Original front panel; sides and caps sample plain masonry away from
      // its iron rings and grate, and never include the photograph's backdrop.
      uv.setXY(i, front ? (90 + u * 990) / 1168 : (900 + u * 165) / 1168,
        1 - (655 - v * 535) / 784);
      continue;
    }
    if (Math.abs(normals.getZ(i)) > 0.5) uv.setXY(i, x / repeatSize, y / repeatSize);
    // Tactical geometry expands physical height to match the normal view's
    // projected height. Keep the authored masonry rows independent of that lift.
    else uv.setXY(i, (Math.abs(normals.getX(i)) > 0.5 ? y : x) / repeatSize, z / (repeatSize * textureHeightScale));
  }
  uv.needsUpdate = true;
  if (projectHeight) for (let i = 0; i < positions.count; i++) positions.setY(i, positions.getY(i) + positions.getZ(i) * 3.5);
  positions.needsUpdate = true;
  // Preserve upright surface normals: the camera shear must not turn a vertical
  // wall face into a sun-facing slope or paint ground shadows across that face.
  geometry.computeBoundingBox();
  geometry.computeBoundingSphere();
  return geometry;
}
