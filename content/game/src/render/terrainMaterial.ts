// One material paints the whole ground. Each vertex carries surface weights (splatA/splatB,
// see SURFACE in board.ts); flat ground blends the surface textures in world space (no per-hex
// tiling), steep faces take a cliff texture projected from the side. Weights summing to ~0 are void.

import * as THREE from 'three';
import { MeshStandardNodeMaterial } from 'three/webgpu';
import {
  Fn, If, attribute, positionWorld, normalWorldGeometry, normalViewGeometry, cameraViewMatrix,
  texture, uniform, uniformArray, float, vec3, vec4, mix, smoothstep, clamp, max,
} from 'three/tsl';
import type { GroundTextures } from './textures';
import { groundSetUniforms } from './groundSets';
import { GROUND_LAYER_COUNT, GROUND_WEIGHT_ATTRIBUTES } from '../map/board';
import { WEBGPU } from './gpu';

export function makeTerrainMaterial(tex: GroundTextures): THREE.MeshStandardMaterial {
  if (WEBGPU) return makeTerrainNodeMaterial(tex) as unknown as THREE.MeshStandardMaterial;
  const mat = new THREE.MeshStandardMaterial({ roughness: 0.92 });
  const varying = GROUND_WEIGHT_ATTRIBUTES.map((_, i) => `vG${String.fromCharCode(65 + i)}`);
  const declarations = varying.map(name => `varying vec4 ${name};`).join('\n');
  mat.onBeforeCompile = (sh) => {
    Object.assign(sh.uniforms, {
      // grass, forest, pavers, snow, rock, gravel, ash, earth, cliff in one texture array
      // (textures.ts PROTO_LAYERS): one texture unit instead of nine, under the GPU's limit of 16
      tProto: { value: tex.proto },
      tPlains049Color: { value: tex.plains049Color }, tPlains049Normal: { value: tex.plains049Normal }, tPlains049Rough: { value: tex.plains049Rough },
    }, groundSetUniforms);
    sh.vertexShader = sh.vertexShader
      .replace('#include <common>', `#include <common>
        attribute vec4 splatA; attribute vec4 splatB;
        ${GROUND_WEIGHT_ATTRIBUTES.map(name => `attribute vec4 ${name};`).join('\n')}
        varying vec4 vSA; varying vec4 vSB; varying vec3 vWP; varying vec3 vWN;
        ${declarations}`)
      .replace('#include <begin_vertex>', `#include <begin_vertex>
        vSA = splatA; vSB = splatB; ${GROUND_WEIGHT_ATTRIBUTES.map((name, i) => `${varying[i]} = ${name};`).join(' ')}
        vWP = (modelMatrix * vec4(position, 1.0)).xyz;
        vWN = normalize(mat3(modelMatrix) * normal);`);
    sh.fragmentShader = sh.fragmentShader
      .replace('#include <common>', `#include <common>
        uniform highp sampler2DArray tProto; // 0 grass 1 forest 2 pavers 3 snow 4 rock 5 gravel 6 ash 7 earth 8 cliff
        uniform sampler2D tPlains049Color, tPlains049Normal, tPlains049Rough;
        varying vec4 vSA; varying vec4 vSB; varying vec3 vWP; varying vec3 vWN;
        // baked tile set: 12 terrain layers plus V3 City, shallow-water, and river-rock layers
        uniform float uGroundSet; uniform float uSetHas[${GROUND_LAYER_COUNT}];
        uniform highp sampler2DArray tSetColor, tSetNormal, tSetRough;
        ${declarations}
        float setW = 0.0;
        float setLayerW(int k) {
          float w = ${varying.map((name, i) => i === varying.length - 1 ? `${name}[k - ${i * 4}]` : `k < ${(i + 1) * 4} ? ${name}[k - ${i * 4}] : `).join('')};
          return uSetHas[k] > 0.5 ? w : 0.0;
        }
        vec4 setSample(highp sampler2DArray t, vec2 uv) {
          vec4 acc = vec4(0.0); float sum = 0.0;
          for (int k = 0; k < ${GROUND_LAYER_COUNT}; k++) {
            float w = setLayerW(k);
            if (w > 0.001) { acc += texture(t, vec3(uv, float(k))) * w; sum += w; }
          }
          return acc / max(sum, 1e-4);
        }
        vec3 proto(float layer, vec2 uv) { return texture(tProto, vec3(uv, layer)).rgb; }
        // Grass with no visible repeat (Quilez's "texture without tiling"): every 4-unit patch
        // samples the same prototype grass at its own random offset, the 3x3 neighbours blended
        // smoothly. Up close it is the same grass; from afar the lattice of the 4-unit tile is gone.
        vec4 tileHash(vec2 p) {
          return fract(sin(vec4(1.0 + dot(p, vec2(37.0, 17.0)), 2.0 + dot(p, vec2(11.0, 47.0)),
                                3.0 + dot(p, vec2(41.0, 29.0)), 4.0 + dot(p, vec2(23.0, 31.0)))) * 103.0);
        }
        vec3 grassNoTile(vec2 uv) {
          vec2 p = floor(uv), f = fract(uv), dx = dFdx(uv), dy = dFdy(uv);
          vec3 acc = vec3(0.0); float wsum = 0.0;
          for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) {
            vec2 g = vec2(float(i), float(j));
            vec4 o = tileHash(p + g);
            vec2 r = g - f + o.xy;
            float w = exp(-5.0 * dot(r, r));
            acc += w * textureGrad(tProto, vec3(uv + o.zw, 0.0), dx, dy).rgb;
            wsum += w;
          }
          return acc / wsum;
        }
        vec3 sideTex(float layer, vec3 n) {
          float wx = n.x * n.x, wz = n.z * n.z;
          return (proto(layer, vWP.zy / 2.0) * wx + proto(layer, vWP.xy / 2.0) * wz) / max(wx + wz, 1e-4);
        }`)
      .replace('#include <map_fragment>', `
        vec3 n = normalize(vWN);
        vec2 tuv = vWP.xz / 4.0;
        float total = vSA.x + vSA.y + vSA.z + vSA.w + vSB.x + vSB.y + vSB.z + vSB.w;
        vec3 topC = grassNoTile(tuv) * vSA.x + proto(1.0, tuv) * vSA.y + proto(2.0, tuv) * vSA.z
                  + proto(3.0, tuv) * vSA.w + proto(4.0, tuv) * vSB.x + proto(5.0, tuv) * vSB.y
                  + proto(6.0, tuv) * vSB.z + texture2D(tPlains049Color, tuv).rgb * vSB.w;
        topC /= max(total, 1e-4);
        if (uGroundSet > 0.5) {
          float gTotal = ${varying.map(name => `dot(${name}, vec4(1.0))`).join(' + ')}, have = 0.0;
          for (int k = 0; k < ${GROUND_LAYER_COUNT}; k++) have += setLayerW(k);
          setW = clamp(have / max(gTotal, 1e-4), 0.0, 1.0);
          if (setW > 0.0) topC = mix(topC, setSample(tSetColor, tuv).rgb, setW);
        }
        float ew = vSA.x + vSA.y + vSB.y + vSB.z + vSB.w, rw = vSA.z + vSA.w + vSB.x;
        vec3 cliffC = (sideTex(7.0, n) * ew + sideTex(8.0, n) * rw) / max(ew + rw, 1e-4);
        float flatness = smoothstep(0.45, 0.8, n.y);
        vec3 ground = mix(cliffC, topC, flatness);
        diffuseColor.rgb *= mix(vec3(0.035, 0.03, 0.03), ground, smoothstep(0.02, 0.2, total));`);
    sh.fragmentShader = sh.fragmentShader
      .replace('#include <roughnessmap_fragment>', `#include <roughnessmap_fragment>
        roughnessFactor = mix(roughnessFactor, texture2D(tPlains049Rough, vWP.xz / 4.0).r, clamp(vSB.w, 0.0, 1.0));
        if (setW > 0.0) roughnessFactor = mix(roughnessFactor, setSample(tSetRough, vWP.xz / 4.0).r, setW);`)
      // the baked tiles' normal maps, on flat tops (seen from above: +X right, image up = world -Z)
      .replace('#include <normal_fragment_maps>', `#include <normal_fragment_maps>
        if (setW > 0.0) {
          vec3 tn = setSample(tSetNormal, vWP.xz / 4.0).xyz * 2.0 - 1.0;
          vec3 wn = normalize(vec3(tn.x, tn.z, -tn.y));
          float flatTop = smoothstep(0.45, 0.8, normalize(vWN).y);
          normal = normalize(mix(normal, normalize((viewMatrix * vec4(wn, 0.0)).xyz), setW * flatTop));
        }`);
  };
  return mat;
}

/** The same ground in TSL for the WebGPU path (render/gpu.ts): line for line the GLSL above. */
function makeTerrainNodeMaterial(tex: GroundTextures): MeshStandardNodeMaterial {
  const mat = new MeshStandardNodeMaterial({ roughness: 0.92 });
  const sA = attribute<'vec4'>('splatA', 'vec4'), sB = attribute<'vec4'>('splatB', 'vec4');
  const gAttr = GROUND_WEIGHT_ATTRIBUTES.map(name => attribute<'vec4'>(name, 'vec4'));
  const wp = positionWorld, wn = normalWorldGeometry.normalize();
  const tuv = wp.xz.div(4.0);

  // grass, forest, pavers, snow, rock, gravel, ash, earth, cliff (textures.ts PROTO_LAYERS)
  const tProto = texture(tex.proto);
  const proto = (layer: number, uv: ReturnType<typeof vec3>['xy']) => tProto.sample(uv).depth(float(layer)).rgb;
  const sideTex = (layer: number) => {
    const wx = wn.x.mul(wn.x), wz = wn.z.mul(wn.z);
    return proto(layer, wp.zy.div(2.0)).mul(wx).add(proto(layer, wp.xy.div(2.0)).mul(wz)).div(max(wx.add(wz), 1e-4));
  };

  // the baked tile set, kept current from groundSets.ts's shared uniforms
  const uGroundSet = uniform(0).onRenderUpdate(() => groundSetUniforms.uGroundSet.value);
  const uSetHas = uniformArray(new Array<number>(GROUND_LAYER_COUNT).fill(0), 'float');
  uGroundSet.onRenderUpdate(() => {
    const has = groundSetUniforms.uSetHas.value;
    for (let k = 0; k < GROUND_LAYER_COUNT; k++) uSetHas.array[k] = has[k] ?? 0;
    return groundSetUniforms.uGroundSet.value;
  });
  const tSetColor = texture(groundSetUniforms.tSetColor.value).onRenderUpdate(() => groundSetUniforms.tSetColor.value);
  const tSetNormal = texture(groundSetUniforms.tSetNormal.value).onRenderUpdate(() => groundSetUniforms.tSetNormal.value);
  const tSetRough = texture(groundSetUniforms.tSetRough.value).onRenderUpdate(() => groundSetUniforms.tSetRough.value);
  const layerW = (k: number) => {
    const g = gAttr[k >> 2]!, c = (['x', 'y', 'z', 'w'] as const)[k & 3];
    const has = uSetHas.element(k) as unknown as ReturnType<typeof float>; // the typings leave the element untyped
    return g[c].mul(has.greaterThan(0.5).select(1.0, 0.0));
  };
  const setSample = (t: typeof tSetColor) => Fn(() => {
    const acc = vec4(0).toVar(), sum = float(0).toVar();
    for (let k = 0; k < GROUND_LAYER_COUNT; k++) {
      const w = layerW(k);
      If(w.greaterThan(0.001), () => { acc.addAssign(t.sample(tuv).depth(float(k)).mul(w)); sum.addAssign(w); });
    }
    return acc.div(max(sum, 1e-4));
  })();
  const setW = Fn(() => {
    const gTotal = float(0).toVar(), have = float(0).toVar();
    for (const g of gAttr) gTotal.addAssign(g.dot(vec4(1)));
    for (let k = 0; k < GROUND_LAYER_COUNT; k++) have.addAssign(layerW(k));
    return uGroundSet.greaterThan(0.5).select(clamp(have.div(max(gTotal, 1e-4)), 0.0, 1.0), 0.0);
  })().toVar('setW');

  const total = sA.x.add(sA.y).add(sA.z).add(sA.w).add(sB.x).add(sB.y).add(sB.z).add(sB.w);
  mat.colorNode = Fn(() => {
    const topC = proto(0, tuv).mul(sA.x).add(proto(1, tuv).mul(sA.y)).add(proto(2, tuv).mul(sA.z))
      .add(proto(3, tuv).mul(sA.w)).add(proto(4, tuv).mul(sB.x)).add(proto(5, tuv).mul(sB.y))
      .add(proto(6, tuv).mul(sB.z)).add(texture(tex.plains049Color, tuv).rgb.mul(sB.w))
      .div(max(total, 1e-4)).toVar();
    If(setW.greaterThan(0.0), () => { topC.assign(mix(topC, setSample(tSetColor).rgb, setW)); });
    const ew = sA.x.add(sA.y).add(sB.y).add(sB.z).add(sB.w), rw = sA.z.add(sA.w).add(sB.x);
    const cliffC = sideTex(7).mul(ew).add(sideTex(8).mul(rw)).div(max(ew.add(rw), 1e-4));
    const ground = mix(cliffC, topC, smoothstep(0.45, 0.8, wn.y));
    return vec4(mix(vec3(0.035, 0.03, 0.03), ground, smoothstep(0.02, 0.2, total)), 1);
  })();
  mat.roughnessNode = Fn(() => {
    const r = mix(float(0.92), texture(tex.plains049Rough, tuv).r, clamp(sB.w, 0.0, 1.0)).toVar();
    If(setW.greaterThan(0.0), () => { r.assign(mix(r, setSample(tSetRough).r, setW)); });
    return r;
  })();
  // the baked tiles' normal maps, on flat tops (seen from above: +X right, image up = world -Z)
  mat.normalNode = Fn(() => {
    const n = normalViewGeometry.toVar();
    If(setW.greaterThan(0.0), () => {
      const tn = setSample(tSetNormal).xyz.mul(2.0).sub(1.0);
      const wnMap = vec3(tn.x, tn.z, tn.y.negate()).normalize();
      const flatTop = smoothstep(0.45, 0.8, wn.y);
      n.assign(mix(n, cameraViewMatrix.mul(vec4(wnMap, 0)).xyz.normalize(), setW.mul(flatTop)).normalize());
    });
    return n;
  })();
  return mat;
}
