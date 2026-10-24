// One material, two GPU paths: a GLSL ShaderMaterial on the WebGL path, and on the WebGPU path
// (render/gpu.ts) a node material built from TSL with the same uniforms. The material's
// `uniforms` stays the very same { value } objects on both paths, so code that sets
// `material.uniforms.uTime.value` (or shares one uniform object between materials) is unchanged;
// the TSL nodes read those values every frame.

import * as THREE from 'three';
import { MeshBasicNodeMaterial } from 'three/webgpu';
import { texture, uniform, uniformArray } from 'three/tsl';
import { WEBGPU } from './gpu';

type UniformDefs = Record<string, { value: unknown }>;
/** TSL nodes, keyed like the uniforms (typed loosely: each shader knows its own types). */
// eslint-disable-next-line @typescript-eslint/no-explicit-any
export type UniformNodes = Record<string, any>;
// eslint-disable-next-line @typescript-eslint/no-explicit-any
export interface ShaderNodes { colorNode: any; opacityNode?: any; vertexNode?: any; positionNode?: any }

const EMPTY_TEXTURE = Object.assign(new THREE.DataTexture(new Uint8Array([0, 0, 0, 0]), 1, 1), { needsUpdate: true });

/** TSL uniform nodes that follow `defs[k].value` each frame. A texture (or a null that will be a
 * texture later — every null uniform in these shaders is a sampler) becomes a texture node. */
export function uniformNodes(defs: UniformDefs): UniformNodes {
  const nodes: UniformNodes = {};
  for (const [key, def] of Object.entries(defs)) {
    const v = def.value as { isTexture?: boolean } | null | number | unknown[];
    if (v === null || (v as { isTexture?: boolean }).isTexture) {
      nodes[key] = texture((v as THREE.Texture | null) ?? EMPTY_TEXTURE).onRenderUpdate(() => (def.value as THREE.Texture | null) ?? EMPTY_TEXTURE);
    } else if (Array.isArray(v)) {
      nodes[key] = uniformArray(v as THREE.Vector4[]); // the array itself is shared and edited in place
    } else {
      nodes[key] = uniform(v as number).onRenderUpdate(() => def.value as number);
    }
  }
  return nodes;
}

/** A ShaderMaterial (WebGL path) or its TSL twin (WebGPU path) — see the module comment. */
export function shaderMaterial(params: THREE.ShaderMaterialParameters & { uniforms: UniformDefs }, build: (u: UniformNodes) => ShaderNodes): THREE.ShaderMaterial {
  if (!WEBGPU) return new THREE.ShaderMaterial(params);
  const { colorNode, opacityNode, vertexNode, positionNode } = build(uniformNodes(params.uniforms));
  const m = new MeshBasicNodeMaterial();
  // only the render state the ShaderMaterial actually set; the rest keeps three's defaults
  for (const key of ['transparent', 'depthWrite', 'depthTest', 'depthFunc', 'blending', 'side'] as const) {
    if (params[key] !== undefined) (m as unknown as Record<string, unknown>)[key] = params[key];
  }
  m.fog = false; // a ShaderMaterial without fog chunks never took scene fog
  m.colorNode = colorNode;
  if (opacityNode) m.opacityNode = opacityNode;
  if (vertexNode) m.vertexNode = vertexNode;
  if (positionNode) m.positionNode = positionNode;
  (m as unknown as { uniforms: UniformDefs }).uniforms = params.uniforms;
  return m as unknown as THREE.ShaderMaterial;
}
