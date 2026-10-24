import * as THREE from "three";

/**
 * Environmental ambient occlusion for the battle ground — the "how enclosed is this point"
 * cue, built from map data instead of the depth buffer.
 *
 * Why not screen-space GTAO: this renderer is an orthographic camera looking straight down at
 * flat layers. Terrain is one plane at z=0; props and units are flat, non-depth-writing
 * billboards. Three's own GTAOPass was measured on a battle scene (wall, rock, tree, hill
 * block, Horror, a character): the depth buffer is uniform, the AO output was identical at
 * radius 0.25 / 4 / 20, and the only occlusion it produced was a solid black rectangle from
 * the invisible standing shadow-caster planes. There is no geometry for it to read.
 *
 * Instead: a low-res field over the whole board, stamped from what actually stands on it —
 * decoration footprints and raised/blocking terrain (hill, column, barricade, door) — then
 * blurred so it describes nearby geometry relationships (a prop's base, a wall foot, the ground
 * at the foot of a hill, the pocket between clustered props). Units are deliberately NOT
 * occluders: they already get contact decals, and AO here is environmental, not a second
 * character-shadow system.
 *
 * Applied inside the terrain's lighting, never on top of the final image, so it cannot paint
 * over sprites/overlays or tint colors — it only scales light the ground already receives.
 * Indirect (hemi fill) takes the full amount. Direct sunlight takes only GROUND_AO_DIRECT of
 * it: measured, the hemi fill is ~7% of sunlit ground brightness (sun 5 / ambient 2, no tone
 * mapping), so indirect-only AO topped out at 1–3 luminance — invisible. The small direct share
 * is a stylized stand-in for the bounce light this lighting model doesn't have.
 */

/** Field texels per hex radius (`tile`). The field is sized in hex units, so zooming (which
 * changes `tile`) only rescales the lookup, never rebuilds the field. */
const RES = 6;
/** Box-blur radius (texels) x 3 passes ≈ gaussian sigma of sqrt(r(r+1)) texels — r=3 is
 * ~0.6 hex radius: local, a prop's immediate surroundings, not a dark cloud around it. */
const BLUR_R = 3;
const BLUR_PASSES = 3;
/** Stamp radius around each occluder cell center, in hex radii (~ the hex's inscribed disc). */
const STAMP_R = 0.82;
/** Peak fraction of indirect light removed at full occlusion. Balanced against the contact
 * decals (Phase 3): in a cast shadow only indirect light is left, so this is AO's full effect
 * there — 0.45 stacked with contact took already-shadowed grass from 16 to 9 luminance. 0.30
 * with a larger direct share keeps the sunlit result (~0.17 x occlusion) and cuts shade by 1/3. */
export const GROUND_AO_STRENGTH = 0.3;
/** Fraction of GROUND_AO_STRENGTH also applied to direct sunlight (see module comment). */
export const GROUND_AO_DIRECT = 0.55;

export interface AoOccluder {
  /** Cell center in tile-normalized world units (hexWorld(x, y, 1), y-down). */
  x: number;
  y: number;
  /** 0..1 — how much this cell counts as surrounding geometry. */
  weight: number;
}

export class GroundAO {
  readonly uniforms = {
    groundAoMap: { value: null as THREE.DataTexture | null },
    /** World size (Three units) the field spans, x and y-down. */
    groundAoSize: { value: new THREE.Vector2(1, 1) },
    groundAoStrength: { value: GROUND_AO_STRENGTH },
    groundAoDirect: { value: GROUND_AO_DIRECT },
    /** Prevent local point lights from making individual hexes read as bright decals. */
    groundLightCap: { value: 10 },
    /** Ground-only close-contact comparison. Never changes bias on the lights/model surfaces. */
    groundContactStrength: { value: 1 },
  };
  private texture: THREE.DataTexture | null = null;
  private key = "";
  private gw = 1;
  private gh = 1;

  /** Adds the lookup to a terrain material. Every patched material shares `uniforms`, so the
   * field / strength / zoom scale are updated once for all of them. */
  patch(mat: THREE.MeshLambertMaterial): void {
    const uniforms = this.uniforms;
    mat.onBeforeCompile = (shader) => {
      Object.assign(shader.uniforms, uniforms);
      shader.vertexShader = shader.vertexShader
        .replace("#include <common>", "#include <common>\nvarying vec2 vGroundAoWorld;")
        .replace(
          "#include <project_vertex>",
          "#include <project_vertex>\nvGroundAoWorld = (modelMatrix * vec4(transformed, 1.0)).xy;",
        );
      // Keep complete shadow-map silhouettes. A receiver-side comparison closes the
      // large bias gap without clipping caster triangles or drawing a second dark shape.
      const lighting = THREE.ShaderChunk.lights_fragment_begin
        .replaceAll("directionalLightShadow.shadowBias", "mix(directionalLightShadow.shadowBias, max(-0.00005, directionalLightShadow.shadowBias * 0.03), groundContactStrength)")
        .replaceAll("pointLightShadow.shadowBias", "mix(pointLightShadow.shadowBias, max(-0.00005, pointLightShadow.shadowBias * 0.03), groundContactStrength)");
      shader.fragmentShader = shader.fragmentShader
        .replace("#include <lights_fragment_begin>", lighting)
        .replace(
          "#include <common>",
          "#include <common>\nvarying vec2 vGroundAoWorld;\nuniform sampler2D groundAoMap;\nuniform vec2 groundAoSize;\nuniform float groundAoStrength;\nuniform float groundAoDirect;\nuniform float groundLightCap;\nuniform float groundContactStrength;",
        )
        .replace(
          "#include <output_fragment>",
          // Soft shoulder instead of a hard min(): unchanged up to half the cap, then eases
          // toward the cap, so a lamp's bright center keeps a gradient instead of flattening
          // into a uniform disc with a hard edge.
          [
            "vec3 gLim = sampledDiffuseColor.rgb * groundLightCap;",
            "vec3 gKnee = gLim * 0.5;",
            "vec3 gSpan = max(gLim - gKnee, vec3(1e-4));",
            "outgoingLight = min(outgoingLight, gKnee) + gSpan * (1.0 - exp(-max(outgoingLight - gKnee, vec3(0.0)) / gSpan));",
            "#include <output_fragment>",
          ].join("\n"),
        )
        .replace(
          "#include <aomap_fragment>",
          [
            "#include <aomap_fragment>",
            "float groundOcc = groundAoStrength * texture2D(groundAoMap, vec2(vGroundAoWorld.x, -vGroundAoWorld.y) / groundAoSize).r;",
            "reflectedLight.indirectDiffuse *= 1.0 - groundOcc;",
            "reflectedLight.directDiffuse *= 1.0 - groundOcc * groundAoDirect;",
          ].join("\n"),
        );
    };
    mat.customProgramCacheKey = () => "groundAO-groundContact-v1";
    mat.needsUpdate = true;
  }

  /** Rebuilds the field when `key` (map identity + occluder set) changes; always refreshes the
   * zoom scale. `cols`/`rows`/`padMul` describe the board extent in hex units. `openAt` (tile-
   * normalized world point → bool) marks surfaces open to the sky — raised terrain tops — whose
   * texels are cleared after blurring, so only the ground at their foot is occluded, never the
   * hilltop itself (tested per texel against the exact hex cell, not a disc, or the gaps between
   * adjacent raised hexes keep occlusion and draw a honeycomb grid on the hilltop). */
  update(
    key: string,
    cols: number,
    rows: number,
    padMul: number,
    tile: number,
    occluders: () => AoOccluder[],
    openAt: (x: number, y: number) => boolean,
  ): void {
    if (key !== this.key) {
      this.key = key;
      this.build(cols, rows, padMul, occluders(), openAt);
    }
    this.uniforms.groundAoSize.value.set((tile * this.gw) / RES, (tile * this.gh) / RES);
  }

  setEnabled(on: boolean): void {
    this.uniforms.groundAoStrength.value = on ? GROUND_AO_STRENGTH : 0;
  }

  private build(
    cols: number,
    rows: number,
    padMul: number,
    occluders: AoOccluder[],
    openAt: (x: number, y: number) => boolean,
  ): void {
    const gw = Math.ceil(Math.sqrt(3) * (cols + 1.5) * RES);
    const gh = Math.ceil((2 * padMul + 1.5 * rows + 2) * RES);
    this.gw = gw;
    this.gh = gh;
    let field = new Float32Array(gw * gh);
    const r = STAMP_R * RES;
    for (const o of occluders) {
      const cx = o.x * RES;
      const cy = o.y * RES;
      for (let y = Math.max(0, Math.floor(cy - r)); y <= Math.min(gh - 1, Math.ceil(cy + r)); y++) {
        for (
          let x = Math.max(0, Math.floor(cx - r));
          x <= Math.min(gw - 1, Math.ceil(cx + r));
          x++
        ) {
          const dx = x + 0.5 - cx;
          const dy = y + 0.5 - cy;
          if (dx * dx + dy * dy > r * r) continue;
          const i = y * gw + x;
          if (o.weight > field[i]!) field[i] = o.weight;
        }
      }
    }
    let tmp = new Float32Array(gw * gh);
    for (let pass = 0; pass < BLUR_PASSES; pass++) {
      boxBlur(field, tmp, gw, gh, BLUR_R, true);
      boxBlur(tmp, field, gw, gh, BLUR_R, false);
    }
    const data = new Uint8Array(gw * gh);
    for (let y = 0; y < gh; y++) {
      for (let x = 0; x < gw; x++) {
        const i = y * gw + x;
        data[i] =
          field[i]! <= 0 || openAt((x + 0.5) / RES, (y + 0.5) / RES)
            ? 0
            : Math.round(255 * Math.min(1, field[i]!));
      }
    }
    field = tmp = new Float32Array(0);

    this.texture?.dispose();
    const tex = new THREE.DataTexture(data, gw, gh, THREE.RedFormat, THREE.UnsignedByteType);
    tex.minFilter = THREE.LinearFilter;
    tex.magFilter = THREE.LinearFilter;
    tex.wrapS = THREE.ClampToEdgeWrapping;
    tex.wrapT = THREE.ClampToEdgeWrapping;
    // One byte per texel and a width that is rarely a multiple of 4: the default row alignment
    // of 4 makes WebGL expect a larger buffer, the upload fails, and every lookup reads 0.
    tex.unpackAlignment = 1;
    tex.needsUpdate = true;
    this.texture = tex;
    this.uniforms.groundAoMap.value = tex;
  }

  dispose(): void {
    this.texture?.dispose();
  }
}

/** One horizontal or vertical box-blur pass (edge-clamped running sum). */
export function boxBlur(
  src: Float32Array,
  dst: Float32Array,
  w: number,
  h: number,
  r: number,
  horizontal: boolean,
): void {
  const len = horizontal ? w : h;
  const lines = horizontal ? h : w;
  const step = horizontal ? 1 : w;
  const norm = 1 / (2 * r + 1);
  for (let line = 0; line < lines; line++) {
    const base = horizontal ? line * w : line;
    let sum = 0;
    for (let k = -r; k <= r; k++) sum += src[base + Math.min(len - 1, Math.max(0, k)) * step]!;
    for (let i = 0; i < len; i++) {
      dst[base + i * step] = sum * norm;
      sum +=
        src[base + Math.min(len - 1, i + r + 1) * step]! - src[base + Math.max(0, i - r) * step]!;
    }
  }
}
