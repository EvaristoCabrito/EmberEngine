import * as THREE from "three";

const SQRT3 = Math.sqrt(3);
const PAD = 2.4;
const STEP = 0.1;
export type WaterPatch = { x: number; y: number; level: number; size: number; shape: "round" | "square" };
type WaterFootprint = { size: number; shape: "round" | "square" } | null;

/** Rounded overlapping brush footprints sampled on a rectangular mesh, independent of tile art. */
export function sampleWater(x: number, y: number, cols: number, rows: number, levels: readonly (number | null)[], footprints: readonly WaterFootprint[] = [], extendEdges = false): { coverage: number; level: number } {
  const row0 = Math.round((y - PAD - 1) / 1.5);
  let coverage = 0, weighted = 0, total = 0;
  for (let row = row0 - 1; row <= row0 + 1; row++) {
    if (!extendEdges && (row < 0 || row >= rows)) continue;
    // Straight-border maps: a hex just outside the board repeats its nearest edge cell, so
    // edge water reaches all the way to the straight cut instead of leaving half-hex notches.
    const sourceRow = Math.max(0, Math.min(rows - 1, row));
    const col0 = Math.round(x / SQRT3 - 0.5 * (row & 1) - 0.5);
    for (let col = col0 - 1; col <= col0 + 1; col++) {
      if (!extendEdges && (col < 0 || col >= cols)) continue;
      const sourceCol = Math.max(0, Math.min(cols - 1, col));
      const level = levels[sourceRow * cols + sourceCol];
      if (level == null || !Number.isFinite(level)) continue;
      const cx = SQRT3 * (col + 0.5 * (row & 1) + 0.5), cy = PAD + 1.5 * row + 1;
      const footprint = footprints[sourceRow * cols + sourceCol];
      const size = Math.max(0.25, Math.min(1, footprint?.size ?? 1));
      const distance = (footprint?.shape === "square" ? Math.max(Math.abs(x - cx), Math.abs(y - cy)) : Math.hypot(x - cx, y - cy)) / size;
      // Keep the level defined beyond the visible edge so boundary triangles stay flat.
      const weight = Math.max(0, 1 - distance / 2);
      if (!weight) continue;
      coverage = Math.max(coverage, Math.min(1, (1.25 - distance) / 0.2));
      weighted += Math.max(0, Math.min(12, level)) * weight;
      total += weight;
    }
  }
  return { coverage: Math.max(0, coverage), level: total ? weighted / total : 0 };
}

export function sampleWaterPatches(x: number, y: number, patches: readonly WaterPatch[]): { coverage: number; level: number } {
  let coverage = 0, weighted = 0, total = 0;
  for (const patch of patches) {
    if (![patch.x, patch.y, patch.level, patch.size].every(Number.isFinite)) continue;
    const size = Math.max(0.25, Math.min(1, patch.size));
    const distance = (patch.shape === "square" ? Math.max(Math.abs(x-patch.x), Math.abs(y-patch.y)) : Math.hypot(x-patch.x,y-patch.y)) / size;
    const weight = Math.max(0, 1-distance/2);
    coverage = Math.max(coverage, Math.min(1, Math.max(0, (1.25-distance)/0.2)));
    weighted += Math.max(0, Math.min(12, patch.level))*weight; total += weight;
  }
  return { coverage, level: total ? weighted/total : 0 };
}

/** 3D Water V3 texture scale: below 1 stretches one tile over more of the board. */
const V3_TEXTURE_REPEAT = 0.5;
/** 3D Water V4 texture scale, same convention as V3. */
const V4_TEXTURE_REPEAT = 0.75;

export class ThreeWater {
  readonly time = { value: 0 };
  readonly flat = { value: 0 };
  private version: "v1" | "v2" | "v3" | "v4" | undefined;
  private modern = { value: 1 };
  // V4 only: glossy reflective water (see setVersion). 0 leaves V1-V3 exactly as they were.
  private v4 = { value: 0 };
  private textureSets = new Map<string, THREE.Texture[]>();
  private scale = { value: 1 };
  private material = new THREE.MeshStandardMaterial({ color: 0xc5d9db, roughness: 0.6, metalness: 0,
    transparent: true, opacity: 0.92, side: THREE.DoubleSide, depthWrite: false });
  readonly mesh = new THREE.Mesh(new THREE.BufferGeometry(), this.material);

  constructor() {
    this.setVersion("v2");
    this.mesh.receiveShadow = true;
    this.material.onBeforeCompile = shader => {
      shader.uniforms.waterTime = this.time;
      shader.uniforms.waterModern = this.modern;
      shader.uniforms.waterV4 = this.v4;
      shader.uniforms.waterScale = this.scale;
      shader.uniforms.waterFlat = this.flat;
      shader.vertexShader = `uniform float waterTime; uniform float waterScale; uniform float waterFlat;
        attribute float coverage; attribute float waterDepth;
        varying float vWaterCoverage; varying float vWaterDepth; varying vec2 vWaterPoint;
        ${shader.vertexShader}`;
      shader.vertexShader = shader.vertexShader.replace("#include <beginnormal_vertex>", `
        vec2 p = position.xy / waterScale;
        float phaseA = dot(p, vec2(2.2, 1.5)) + waterTime * 1.4;
        float phaseB = dot(p, vec2(-1.2, 3.1)) - waterTime * 0.9;
        vec2 slope = 0.014 * cos(phaseA) * vec2(2.2, 1.5) + 0.009 * cos(phaseB) * vec2(-1.2, 3.1);
        slope += 0.08 * cos(dot(p, vec2(8.0, 5.0)) + waterTime * 1.8) * vec2(0.8, 0.5);
        vec3 objectNormal = normalize(vec3(-slope, 1.0));
        #ifdef USE_TANGENT
          vec3 objectTangent = vec3(tangent.xyz);
        #endif`);
      shader.vertexShader = shader.vertexShader.replace("#include <begin_vertex>", `
        vec3 transformed = vec3(position);
        transformed.z += waterScale * (0.014 * sin(phaseA) + 0.009 * sin(phaseB));
        transformed.z = mix(transformed.z, 0.8, waterFlat);
        vWaterCoverage = coverage; vWaterDepth = waterDepth; vWaterPoint = p;`);
      shader.fragmentShader = `varying float vWaterCoverage; varying float vWaterDepth; varying vec2 vWaterPoint;
        uniform float waterTime; uniform float waterModern; uniform float waterFlat; uniform float waterV4;
        float waterHash(vec2 p) {
          return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
        }
        float waterNoise(vec2 p) {
          vec2 cell = floor(p), f = fract(p);
          vec2 u = f * f * (3.0 - 2.0 * f);
          return mix(mix(waterHash(cell), waterHash(cell + vec2(1.0, 0.0)), u.x),
            mix(waterHash(cell + vec2(0.0, 1.0)), waterHash(cell + vec2(1.0, 1.0)), u.x), u.y);
        }
        float waterCloud(vec2 p) {
          return waterNoise(p) * 0.57 + waterNoise(p * 2.03 + 13.4) * 0.29
            + waterNoise(p * 4.11 + 7.2) * 0.14;
        }
        ${shader.fragmentShader}`;
      // Two slow, crossing normal layers avoid a static painted appearance.
      shader.fragmentShader = shader.fragmentShader.replace("#include <normal_fragment_maps>", `
        #ifdef USE_NORMALMAP_TANGENTSPACE
          vec2 flow = vec2(waterTime * 0.009, waterTime * 0.004) * waterModern * (1.0 + 1.2 * waterV4);
          vec3 rippleA = texture2D(normalMap, vNormalMapUv + flow).xyz * 2.0 - 1.0;
          vec3 rippleB = texture2D(normalMap, vNormalMapUv * 1.73 - flow * 0.65).xyz * 2.0 - 1.0;
          // V4: a third, finer and faster layer crossing the other two keeps glints moving.
          vec3 rippleC = texture2D(normalMap, vec2(vNormalMapUv.y, -vNormalMapUv.x) * 2.61 + flow * 1.9).xyz * 2.0 - 1.0;
          vec3 mapN = normalize(vec3((rippleA.xy + rippleB.xy * 0.38 * waterModern + rippleC.xy * 0.45 * waterV4) * normalScale, rippleA.z));
          normal = normalize(tbn * mapN);
        #endif`);
      shader.fragmentShader = shader.fragmentShader.replace("#include <map_fragment>", `
        #ifdef USE_MAP
          vec2 colorFlow = vec2(waterTime * 0.009, waterTime * 0.004) * waterModern;
          diffuseColor *= texture2D(map, vMapUv + colorFlow);
        #endif`);
      shader.fragmentShader = shader.fragmentShader.replace("#include <color_fragment>", `
        #include <color_fragment>
        if (vWaterCoverage < 0.05) discard;
        // World-space texture joins across every brush and stays fixed while the camera moves.
        vec2 drift = vec2(waterTime * 0.055, -waterTime * 0.035);
        float cloud = waterCloud(vWaterPoint * 2.2 + drift);
        float detail = waterNoise(vWaterPoint * 8.0 - drift * 1.7);
        vec2 warp = vec2(cloud, waterCloud(vWaterPoint * 2.2 + drift + 29.0));
        float phase = dot(vWaterPoint, vec2(12.0, 5.0)) + warp.x * 7.0 + waterTime * 0.7;
        float wave = 0.5 + 0.5 * sin(phase);
        float aa = max(fwidth(wave), 0.04);
        float crest = smoothstep(0.76 - aa, 0.96 + aa, wave);
        float broken = smoothstep(0.35, 0.7, waterCloud(vWaterPoint * 3.7 - drift + 51.0));
        float crossWave = sin(dot(vWaterPoint, vec2(-6.0, 15.0)) + warp.y * 5.0 - waterTime * 0.5);
        // V4 skips the procedural cloud/crest tinting: its look comes from real reflections.
        diffuseColor.rgb *= mix(mix(0.74 + 0.4 * cloud + 0.08 * detail, 0.92 + 0.13 * cloud + 0.035 * detail, waterModern), 1.0, waterV4);
        diffuseColor.rgb = mix(diffuseColor.rgb, mix(vec3(0.20, 0.48, 0.49), vec3(0.09, 0.17, 0.20), waterModern), crest * broken * mix(0.24, 0.08, waterModern) * (1.0 - waterV4));
        diffuseColor.rgb += mix(vec3(0.018, 0.027, 0.025), vec3(0.003, 0.005, 0.007), waterModern) * max(0.0, crossWave) * broken * (1.0 - waterV4);
        float shore = 1.0 - smoothstep(0.015, 0.16, vWaterDepth);
        float foam = shore * (mix(0.035 + 0.09 * crest * broken, 0.015 + 0.045 * crest * broken, waterModern));
        diffuseColor.rgb = mix(diffuseColor.rgb, vec3(0.53, 0.67, 0.62), foam);
        diffuseColor.a *= smoothstep(0.05, 0.55, vWaterCoverage);
        // V4: only the shoreline rim turns clear enough to show the ground beneath. (Depth
        // can't drive this: a level-0 pond is ~0.035 tile deep everywhere, which made the
        // whole pond half-transparent and milky.)
        diffuseColor.a *= mix(1.0, mix(0.55, 1.0, smoothstep(0.45, 1.0, vWaterCoverage)), waterV4);`);
      // Texture roughness multiplies the base value; keep tactical water from becoming mirror-like.
      shader.fragmentShader = shader.fragmentShader.replace("#include <roughnessmap_fragment>", `
        #include <roughnessmap_fragment>
        roughnessFactor = max(roughnessFactor, mix(0.68, 0.4, waterFlat));`);
      // Keep water in the game's muted palette even under strong sun and full-scene bloom.
      shader.fragmentShader = shader.fragmentShader.replace("#include <opaque_fragment>", `
        // Suppress the white sun hotspot in the tactical camera, keeping diffuse ripple detail.
        // All versions use the tactical highlight suppression. V4 retains subtle glints
        // in the flat view without bypassing the tactical camera's protection.
        outgoingLight = totalDiffuse + totalSpecular * mix(vec3(0.035, 0.055, 0.065), mix(vec3(1.0), vec3(0.12), waterV4), waterFlat) + totalEmissiveRadiance;
        // Soft sky tint follows the animated normal, with stronger reflection at grazing angles.
        float waterFacing = clamp(dot(normal, normalize(vViewPosition)), 0.0, 1.0);
        float waterFresnel = mix(0.02 + 0.30 * pow(1.0 - waterFacing, 4.0), 0.03 + 0.40 * pow(1.0 - waterFacing, 3.0), waterV4);
        vec3 waterSky = mix(vec3(0.035, 0.065, 0.08), mix(vec3(0.22, 0.29, 0.33), vec3(0.30, 0.38, 0.43), waterV4), smoothstep(-0.3, 0.8, normal.y));
        outgoingLight = mix(outgoingLight, waterSky, waterFresnel * waterModern);
        float waterPeak = max(max(outgoingLight.r, outgoingLight.g), outgoingLight.b);
        float waterKnee = 0.7;
        outgoingLight *= waterKnee / (waterKnee + waterPeak);
        #include <opaque_fragment>`);
    };
  }

  setVersion(version: "v1" | "v2" | "v3" | "v4"): void {
    if (this.version === version) return;
    let textures = this.textureSets.get(version);
    if (!textures) {
      const loader = new THREE.TextureLoader();
      const directory = version === "v1" ? "water" : version === "v3" ? "water-v3" : version === "v4" ? "water-v4" : "water-v2";
      textures = ["color", "normal", "roughness"].map(kind => loader.load(`/game/textures/${directory}/water-${kind}.png`));
      textures[0]!.colorSpace = THREE.SRGBColorSpace;
      for (const texture of textures) {
        texture.wrapS = texture.wrapT = THREE.RepeatWrapping;
        texture.anisotropy = 8;
        // V3's maps are a 2048px FFT ocean tile; spread it wider so wavelets read at hex scale.
        if (version === "v3") texture.repeat.setScalar(V3_TEXTURE_REPEAT);
        if (version === "v4") texture.repeat.setScalar(V4_TEXTURE_REPEAT);
      }
      this.textureSets.set(version, textures);
    }
    this.material.map = textures[0]!;
    this.material.normalMap = textures[1]!;
    this.material.roughnessMap = textures[2]!;
    const modern = version !== "v1";
    const v3 = version === "v3";
    const v4 = version === "v4";
    this.material.color.setHex(v3 || v4 ? 0xffffff : modern ? 0xc5d9db : 0x278d9e);
    this.material.opacity = v4 ? 0.9 : v3 ? 0.94 : modern ? 0.92 : 0.8;
    this.material.normalScale.setScalar(v4 ? 1.0 : v3 ? 0.9 : modern ? 0.72 : 0.48);
    this.material.roughness = v4 ? 1.0 : v3 ? 0.9 : modern ? 0.85 : 0.42;
    this.v4.value = v4 ? 1 : 0;
    this.modern.value = modern ? 1 : 0;
    this.version = version;
    this.material.needsUpdate = true;
  }

  /** `board` (tile units, Y down): set on straight-border maps only. Water then fills edge
   * hexes out to that rectangle and is cut exactly on it, like the ground's straight edge. */
  rebuild(cols: number, rows: number, tile: number, levels: readonly (number | null)[], allowed: (col: number, row: number) => boolean,
    ground: (x: number, y: number) => number = () => 0, footprints: readonly WaterFootprint[] = [], patches: readonly WaterPatch[] = [],
    board?: { minX: number; maxX: number; minY: number; maxY: number }): void {
    this.scale.value = tile;
    if (!patches.length && !levels.some(level => level != null && Number.isFinite(level))) {
      this.mesh.geometry.dispose(); this.mesh.geometry = new THREE.BufferGeometry();
      return;
    }
    const positions: number[] = [], uvs: number[] = [], coverage: number[] = [], depths: number[] = [], indices: number[] = [];
    let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
    // Edge cells extend a hex outward on a straight-border map, so reach one hex further.
    const reach = board ? 1.4 + SQRT3 : 1.4;
    for (let row = 0; row < rows; row++) for (let col = 0; col < cols; col++) {
      const level = levels[row * cols + col];
      if (level == null || !Number.isFinite(level) || !allowed(col, row)) continue;
      const x = SQRT3 * (col + 0.5 * (row & 1) + 0.5), y = PAD + 1.5 * row + 1;
      minX = Math.min(minX, x - reach); maxX = Math.max(maxX, x + reach);
      minY = Math.min(minY, y - reach); maxY = Math.max(maxY, y + reach);
    }
    const buckets = new Map<string, WaterPatch[]>();
    for (const patch of patches) {
      if (![patch.x, patch.y, patch.level, patch.size].every(Number.isFinite)) continue;
      minX = Math.min(minX, patch.x-1.4); maxX = Math.max(maxX, patch.x+1.4);
      minY = Math.min(minY, patch.y-1.4); maxY = Math.max(maxY, patch.y+1.4);
      const key = Math.floor(patch.x/2) + ":" + Math.floor(patch.y/2);
      const list = buckets.get(key) ?? []; list.push(patch); buckets.set(key, list);
    }
    if (board) {
      minX = Math.max(minX, board.minX); maxX = Math.min(maxX, board.maxX);
      minY = Math.max(minY, board.minY); maxY = Math.min(maxY, board.maxY);
    }
    if (!Number.isFinite(minX) || minX >= maxX || minY >= maxY) { this.mesh.geometry.dispose(); this.mesh.geometry = new THREE.BufferGeometry(); return; }
    const nx = Math.ceil((maxX - minX) / STEP), ny = Math.ceil((maxY - minY) / STEP);
    for (let iy = 0; iy <= ny; iy++) for (let ix = 0; ix <= nx; ix++) {
      // On a straight-border map the last row/column of vertices sits exactly on the cut.
      const x = board ? Math.min(maxX, minX + ix * STEP) : minX + ix * STEP;
      const y = board ? Math.min(maxY, minY + iy * STEP) : minY + iy * STEP;
      const sample = sampleWater(x, y, cols, rows, levels, footprints, !!board);
      const nearby: WaterPatch[] = [];
      const bx = Math.floor(x/2), by = Math.floor(y/2);
      for (let dy=-1; dy<=1; dy++) for (let dx=-1; dx<=1; dx++) nearby.push(...(buckets.get((bx+dx)+":"+(by+dy)) ?? []));
      const free = sampleWaterPatches(x, y, nearby);
      if (free.coverage > sample.coverage) { sample.coverage = free.coverage; sample.level = free.level; }
      let best = Infinity, bc = -1, br = -1;
      const row0 = Math.round((y - PAD - 1) / 1.5);
      for (let row = row0 - 1; row <= row0 + 1; row++) {
        const col0 = Math.round(x / SQRT3 - 0.5 * (row & 1) - 0.5);
        for (let col = col0 - 1; col <= col0 + 1; col++) {
          const distance = (x - SQRT3 * (col + 0.5 * (row & 1) + 0.5)) ** 2 + (y - PAD - 1.5 * row - 1) ** 2;
          if (distance < best) { best = distance; bc = col; br = row; }
        }
      }
      // Inside a straight-border board every point belongs to the board, even where its
      // nearest hex center lies just outside (the half-hex strips along the straight edges).
      const valid = board
        ? allowed(Math.max(0, Math.min(cols - 1, bc)), Math.max(0, Math.min(rows - 1, br)))
        : bc >= 0 && br >= 0 && bc < cols && br < rows && allowed(bc, br);
      const z = (sample.level * 0.65 + 0.035) * tile;
      positions.push(x * tile, -y * tile, z);
      // A texture repeat spans four world tiles; brush edges sample continuously.
      uvs.push(x * 0.25, -y * 0.25);
      coverage.push(valid ? sample.coverage : 0);
      depths.push(Math.max(0, (z - ground(x * tile, -y * tile)) / tile));
    }
    for (let iy = 0; iy < ny; iy++) for (let ix = 0; ix < nx; ix++) {
      const a = iy * (nx + 1) + ix, b = a + 1, c = a + nx + 1, d = c + 1;
      if (coverage[a]! + coverage[b]! + coverage[c]! + coverage[d]! > 0) indices.push(a, c, b, b, c, d);
    }
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute("position", new THREE.Float32BufferAttribute(positions, 3));
    geometry.setAttribute("uv", new THREE.Float32BufferAttribute(uvs, 2));
    geometry.setAttribute("coverage", new THREE.Float32BufferAttribute(coverage, 1));
    geometry.setAttribute("waterDepth", new THREE.Float32BufferAttribute(depths, 1));
    geometry.setIndex(indices); geometry.computeVertexNormals(); geometry.computeBoundingSphere();
    if (geometry.boundingSphere) geometry.boundingSphere.radius += tile * 0.03;
    this.mesh.geometry.dispose(); this.mesh.geometry = geometry;
  }

  dispose(): void {
    this.mesh.removeFromParent(); this.mesh.geometry.dispose();
    for (const textures of this.textureSets.values()) for (const texture of textures) texture.dispose();
    this.textureSets.clear();
    this.material.dispose();
  }
}
