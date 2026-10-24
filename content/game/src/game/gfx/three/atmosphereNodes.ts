// ThreeAtmosphere's shaders in TSL, for the WebGPU path (render/gpu.ts, render/shaderNode.ts).
// Each one is the GLSL in ThreeAtmosphere.ts translated line for line; the names match its
// uniforms. A reversed smoothstep(a, b, x) with a > b is written 1 - smoothstep(b, a, x): the same
// curve, but defined in WGSL too.

import {
  Fn, If, abs, clamp, cos, dot, float, floor, fract, max, min, mix, mod, pow, sin, smoothstep,
  positionWorld, positionGeometry, uv, vec2, vec3, vec4, attribute, cameraProjectionMatrix,
  modelViewMatrix, modelWorldMatrix, distance,
} from 'three/tsl';
import type { ShaderNodes, UniformNodes } from '../../../render/shaderNode';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
type N = any;

const worldXY = positionWorld.xy;

/** Mist 2 / Fog 01's three-octave texture fbm. */
const textureFbm = (noise: N, p: N): N =>
  noise.sample(p).r.mul(0.55)
    .add(noise.sample(p.mul(2.3).add(3.1)).g.mul(0.3))
    .add(noise.sample(p.mul(4.7).add(9.4)).b.mul(0.15));

export const mist2Nodes = (u: UniformNodes): ShaderNodes => {
  const p = worldXY.mul(u.uScale).add(u.uDrift.mul(u.uTime));
  const n = clamp(textureFbm(u.uNoiseTex, p), 0, 1);
  return { colorNode: mix(u.uColor, u.uSunColor, 0.35), opacityNode: n.mul(u.uAlpha) };
};

// Mist 3's simplex noise.
const hash22 = Fn(([pIn]: [N]) => {
  const p = vec2(dot(pIn, vec2(127.1, 311.7)), dot(pIn, vec2(269.5, 183.3)));
  return fract(sin(p).mul(43758.5453123)).mul(2.0).sub(1.0);
});
const simplex = Fn(([p]: [N]): N => {
  const K1 = 0.366025404, K2 = 0.211324865;
  const i: N = floor(p.add(p.x.add(p.y).mul(K1)));
  const a: N = p.sub(i).add(i.x.add(i.y).mul(K2));
  const o = a.x.greaterThan(a.y).select(vec2(1, 0), vec2(0, 1));
  const b = a.sub(o).add(K2);
  const c = a.sub(1.0).add(2.0 * K2);
  const h = max(vec3(0.5).sub(vec3(dot(a, a), dot(b, b), dot(c, c))), 0.0);
  const n = h.mul(h).mul(h).mul(h).mul(vec3(dot(a, hash22(i)), dot(b, hash22(i.add(o))), dot(c, hash22(i.add(1.0)))));
  return dot(n, vec3(70.0));
});

export const mist3Nodes = (u: UniformNodes): ShaderNodes => {
  const p = worldXY.mul(0.012).add(u.uDrift.mul(u.uTime));
  const raw = simplex(p).mul(0.6).add(simplex(p.mul(2.03).add(11.0)).mul(0.3)).add(simplex(p.mul(4.1).add(71.0)).mul(0.1));
  const n = smoothstep(0.45, 0.88, clamp(raw.mul(0.5).add(0.5), 0, 1));
  return { colorNode: mix(u.uColor, u.uSunColor, 0.35), opacityNode: n.mul(u.uAlpha) };
};

export const mist4Nodes = (u: UniformNodes): ShaderNodes => {
  const distFromEdge = u.uBoardHalfSize.sub(abs(worldXY.sub(u.uBoardCenter)));
  const minDist = min(distFromEdge.x, distFromEdge.y);
  const rawMask = smoothstep(0.0, u.uBorderThickness, minDist).oneMinus();
  const borderMask = pow(rawMask, 2.2).mul(0.55);
  const edgeT = clamp(minDist.div(u.uBorderThickness).oneMinus(), 0, 1);
  const angle = edgeT.mul(2.2).mul(sin(u.uTime.mul(0.15).sub(edgeT.mul(3.0))));
  const s = sin(angle), c = cos(angle);
  const p = worldXY.mul(u.uScale);
  const swirled = vec2(p.x.mul(c).sub(p.y.mul(s)), p.x.mul(s).add(p.y.mul(c))).add(vec2(0.02, -0.015).mul(u.uTime));
  const n = clamp(textureFbm(u.uNoiseTex, swirled), 0, 1);
  return { colorNode: mix(u.uColor, u.uSunColor, 0.35), opacityNode: n.mul(u.uAlpha).mul(borderMask) };
};

// Fog 5's value noise and rotated fbm.
const hash12 = Fn(([pIn]: [N]) => {
  const p: N = fract(pIn.mul(vec2(123.34, 456.21))).toVar();
  p.addAssign(dot(p, p.add(45.32)));
  return fract(p.x.mul(p.y));
});
const vnoise = Fn(([p]: [N]) => {
  const i = floor(p);
  const f0 = fract(p);
  const f = f0.mul(f0).mul(vec2(3.0).sub(f0.mul(2.0)));
  return mix(mix(hash12(i), hash12(i.add(vec2(1, 0))), f.x), mix(hash12(i.add(vec2(0, 1))), hash12(i.add(vec2(1, 1))), f.x), f.y);
});
const fbm4 = Fn(([pIn]: [N]) => {
  const p: N = vec2(pIn).toVar(), v = float(0).toVar(), a = float(0.5).toVar();
  for (let k = 0; k < 4; k++) {
    v.addAssign(a.mul(vnoise(p)));
    // mat2(0.8, -0.6, 0.6, 0.8) * p, column-major as in GLSL
    p.assign(vec2(p.x.mul(0.8).add(p.y.mul(0.6)), p.x.mul(-0.6).add(p.y.mul(0.8))).mul(2.05).add(11.7));
    a.mulAssign(0.5);
  }
  return v;
});

export const fog5Nodes = (u: UniformNodes): ShaderNodes => {
  const t = u.uTime;
  const opacityNode = Fn(() => {
    const world = worldXY.add(u.uCam.mul(u.uParallax));
    const p0 = world.mul(u.uScale).add(u.uSeed);
    const ca = cos(u.uAngle), sa = sin(u.uAngle);
    const p = vec2(p0.x.mul(ca).sub(p0.y.mul(sa)), p0.x.mul(sa).add(p0.y.mul(ca)));
    const q: N = p.add(u.uDrift.mul(t)).toVar();
    q.y.mulAssign(u.uStretch.mul(0.3));
    const w1 = vec2(fbm4(q.mul(0.8).add(vec2(0, t.mul(0.03)))), fbm4(q.mul(0.8).add(vec2(5.2, t.mul(-0.026)))));
    const qw = q.add(w1.mul(u.uWarp).mul(1.6));
    const w2 = vec2(fbm4(qw.add(vec2(1.7, 9.2)).add(t.mul(0.02))), fbm4(qw.add(vec2(8.3, 2.8)).sub(t.mul(0.018))));
    const field = fbm4(q.mul(1.3).add(w2.mul(u.uWarp).mul(2.2)));
    const c1 = smoothstep(0.0, 0.085, abs(field.sub(0.5))).oneMinus();
    const c2 = smoothstep(0.0, 0.06, abs(field.sub(0.62))).oneMinus();
    const strand = clamp(c1.mul(0.75).add(c2.mul(0.5)).mul(fbm4(q.mul(5.0).add(w2.mul(3.0))).mul(0.85).add(0.45)), 0, 1);
    const veil = smoothstep(0.0, 0.3, abs(field.sub(0.55))).oneMinus().mul(0.08);
    const clumps = fbm4(p.mul(0.28).add(vec2(t.mul(0.005), t.mul(-0.004))).add(21.0));
    const density = float(0.5).toVar();
    If(u.uHasDensity.greaterThan(0.5), () => {
      const duv = vec2(
        worldXY.x.sub(u.uBoardCenter.x.sub(u.uBoardHalfSize.x)).div(u.uBoardHalfSize.x.mul(2.0)),
        worldXY.y.negate().div(u.uBoardHalfSize.y.mul(2.0)),
      );
      density.assign(u.uDensity.sample(clamp(duv, 0, 1)).r);
    });
    const thresh = mix(0.58, 0.36, density);
    const pm = smoothstep(thresh, thresh.add(0.14), clumps);
    const a = clamp(strand.add(veil), 0, 1).mul(pm).toVar();
    If(u.uFootPass.greaterThan(0.5), () => {
      // mist rises only a little way up each unit: full at the soles, gone by the knees
      const foot = float(0).toVar();
      for (let k = 0; k < 24; k++) {
        const unit = u.uUnits.element(k);
        const d = worldXY.sub(unit.xy);
        const hx = abs(d.x).div(unit.z), up = d.y.div(unit.w);
        const m = smoothstep(0.7, 1.2, hx).oneMinus().mul(smoothstep(0.0, 1.0, up).oneMinus()).mul(smoothstep(-0.5, 0.0, up));
        foot.assign(max(foot, float(k).lessThan(u.uUnitCount).select(m, 0.0)));
      }
      a.mulAssign(foot.mul(0.85));
    });
    return a.mul(u.uAlpha);
  })();
  return { colorNode: mix(u.uColor, u.uLight, 0.18), opacityNode };
};

export const exteriorFogNodes = (u: UniformNodes): ShaderNodes => {
  const over = abs(worldXY.sub(u.uBoardCenter)).sub(u.uBoardHalfSize);
  const exterior = smoothstep(-14.0, 14.0, max(over.x, over.y));
  const drifted = uv().add(vec2(sin(u.uTime.mul(0.11)), cos(u.uTime.mul(0.08))).mul(0.006));
  const fog = u.uFog.sample(drifted);
  return { colorNode: fog.rgb, opacityNode: fog.a.mul(u.uOpacity).mul(exterior) };
};

export const revealFogNodes = (u: UniformNodes): ShaderNodes => {
  const p = worldXY.mul(u.uScale).add(u.uDrift.mul(u.uTime));
  const n1 = textureFbm(u.uNoiseTex, p);
  const n2 = textureFbm(u.uNoiseTex, p.mul(-1.6).add(7.0).sub(u.uDrift.mul(u.uTime).mul(0.35)));
  const n = clamp(n1.mul(0.6).add(n1.mul(n2).mul(0.9)), 0, 1);
  const over = abs(worldXY.sub(u.uBoardCenter)).sub(u.uBoardHalfSize);
  const exterior = smoothstep(-14.0, 14.0, max(over.x, over.y));
  const revealUv = vec2(
    worldXY.x.sub(u.uBoardCenter.x.sub(u.uBoardHalfSize.x)).div(u.uBoardHalfSize.x.mul(2.0)),
    worldXY.y.negate().div(u.uBoardHalfSize.y.mul(2.0)),
  );
  const unrevealed = u.uHasReveal.greaterThan(0.5).select(u.uReveal.sample(revealUv).r, 1.0);
  return { colorNode: mix(u.uColor, u.uSunColor, 0.35), opacityNode: n.mul(u.uAlpha).mul(max(exterior, unrevealed)) };
};

// Drift particles: per-instance anchor/seed/size, camera-facing quads offset in view space.
const particleColor = (u: UniformNodes, alpha: N): ShaderNodes['opacityNode'] => {
  const mask = smoothstep(0.15, 0.5, distance(uv(), vec2(0.5))).oneMinus();
  return mask.mul(alpha).mul(u.uOpacity);
};
// length of the model matrix's first column (GLSL: length(modelMatrix[0].xyz))
const worldScale = (): N => modelWorldMatrix.mul(vec4(1, 0, 0, 0)).xyz.length();

export const dustNodes = (u: UniformNodes): ShaderNodes => {
  const aBase = attribute('aBase', 'vec2'), aSeed = attribute('aSeed', 'float'), aSize = attribute('aSize', 'float');
  const t = u.uTime.add(aSeed.mul(37.0));
  const wander = vec2(sin(t.mul(0.35).add(aSeed.mul(6.0))), cos(t.mul(0.27).add(aSeed.mul(9.0)))).mul(9.0);
  const z = sin(t.mul(0.5).add(aSeed.mul(3.0))).mul(3.0).add(6.0);
  const zNorm = clamp(z.sub(3.0).div(6.0), 0, 1);
  const center: N = modelViewMatrix.mul(vec4(aBase.add(wander), z, 1.0));
  const offset: N = positionGeometry.xy.mul(aSize).mul(mix(0.55, 1.0, zNorm)).mul(worldScale());
  const vertexNode = cameraProjectionMatrix.mul(vec4(center.xy.add(offset), center.zw));
  const alpha = mix(0.3, 1.0, zNorm).mul(sin(t.mul(0.6).add(aSeed.mul(5.0))).mul(0.5).add(0.5)).toVarying('vAlpha');
  return { colorNode: u.uColor, opacityNode: particleColor(u, alpha), vertexNode };
};

export const emberNodes = (u: UniformNodes): ShaderNodes => {
  const aBase = attribute('aBase', 'vec2'), aSeed = attribute('aSeed', 'float'), aSize = attribute('aSize', 'float');
  const RISE_SPEED = 6.0;
  const cycle = u.uRiseHeight.div(RISE_SPEED);
  const t = mod(u.uTime.mul(RISE_SPEED).add(aSeed.mul(cycle)), cycle);
  const rise01 = t.div(cycle);
  const sway = vec2(sin(u.uTime.mul(0.4).add(aSeed.mul(10.0))), cos(u.uTime.mul(0.32).add(aSeed.mul(7.0)))).mul(6.0);
  const sizeMul = mix(0.5, 1.2, smoothstep(0.0, 0.55, rise01));
  const center: N = modelViewMatrix.mul(vec4(aBase.add(sway), t.add(4.0), 1.0));
  const offset: N = positionGeometry.xy.mul(aSize).mul(sizeMul).mul(worldScale());
  const vertexNode = cameraProjectionMatrix.mul(vec4(center.xy.add(offset), center.zw));
  const alpha = smoothstep(0.0, 0.08, rise01).mul(smoothstep(0.85, 1.0, rise01).oneMinus()).toVarying('vAlpha');
  return { colorNode: u.uColor, opacityNode: particleColor(u, alpha), vertexNode };
};
