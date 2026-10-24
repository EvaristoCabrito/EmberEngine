// Which GPU path draws the 3D world. The WebGPU path (three's WebGPURenderer + TSL node
// materials) is being built alongside the WebGL one and is chosen with ?renderer=webgpu; WebGL
// stays the default until the WebGPU path matches it. Read once: a material is built for one path.

/** The GPU's anisotropic filtering limit, on either renderer (WebGL keeps it in `capabilities`). */
export function maxAnisotropy(renderer: unknown): number {
  const r = renderer as { capabilities?: { getMaxAnisotropy(): number }; getMaxAnisotropy?: () => number };
  return r.capabilities?.getMaxAnisotropy() ?? r.getMaxAnisotropy?.() ?? 1;
}

export const WEBGPU =typeof location !== 'undefined' && new URLSearchParams(location.search).get('renderer') === 'webgpu';
