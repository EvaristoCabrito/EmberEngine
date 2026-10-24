// The legacy spell shaders (src/vfx/legacy/three) in TSL, for the WebGPU path (render/gpu.ts,
// render/shaderNode.ts). Ember's GLSL stays as it is; each function here is that GLSL translated
// line for line, named after the effect and shader it twins. A reversed smoothstep (edge0 > edge1)
// is written 1 - smoothstep(edge1, edge0, x): the same curve, defined in WGSL too.

import {
  Discard, Fn, If, abs, atan, attribute, cameraProjectionMatrix, clamp, cos, dot, exp, float, floor,
  fract, length, max, min, mix, mod, modelViewMatrix, normalLocal, positionGeometry, pow, sin,
  smoothstep, step, transformNormalToView, uv, vec2, vec3, vec4,
} from 'three/tsl';
import type { ShaderNodes, UniformNodes } from '../render/shaderNode';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
type N = any;
const P: N = positionGeometry;

/** `alpha` with fragments under `cut` discarded (GLSL: if (alpha < cut) discard;). */
const discardBelow = (alpha: N, cut: number): N => Fn(() => { If(alpha.lessThan(cut), () => { Discard(); }); return alpha; })();

// ---- Bless: the party-wide wave ring ----
export const blessWaveNodes = (u: UniformNodes): ShaderNodes => {
  const a = atan(P.y, P.x);
  const n = sin(a.mul(11.0).add(u.uTime.mul(7.0))).mul(0.48).add(sin(a.mul(23.0).sub(u.uTime.mul(4.0))).mul(0.22)).add(cos(a.mul(37.0).add(u.uTime.mul(3.0))).mul(0.12));
  const xy = P.xy.mul(u.uRadius.add(n.mul(u.uNoise)));
  const z = P.z.add(sin(a.mul(9.0).sub(u.uTime.mul(6.0))).mul(u.uNoise).mul(0.16)).add(sin(a.mul(7.0).add(u.uTime.mul(3.0))).mul(u.uHeight).mul(0.12));
  const energy = sin(a.mul(13.0).sub(u.uTime.mul(8.0)).add(n.mul(4.0))).mul(0.48).add(0.52).toVarying('vEnergy');
  const streak = smoothstep(0.26, 0.88, energy);
  const c0 = mix(vec3(1.0, 0.28, 0.025), vec3(1.0, 0.67, 0.16), streak);
  const c = mix(c0, vec3(1.0, 0.91, 0.62), pow(streak, 5.0));
  const flicker = sin(u.uTime.mul(15.0).add(energy.mul(8.0))).mul(0.24).add(0.76);
  return { positionNode: vec3(xy, z), colorNode: c.mul(u.uEmissive).mul(flicker), opacityNode: u.uOpacity.mul(streak.mul(0.65).add(0.35)) };
};

// ---- Caustic Venom: procedural smoke lobes ----
const hash31 = Fn(([pIn]: [N]): N => {
  const p: N = fract(pIn.mul(0.1031)).toVar();
  p.addAssign(dot(p, p.yzx.add(33.33)));
  return fract(p.x.add(p.y).mul(p.z));
});
const noise3 = Fn(([p]: [N]): N => {
  const i = floor(p), f0 = fract(p);
  const f: N = f0.mul(f0).mul(vec3(3.0).sub(f0.mul(2.0)));
  const h = (x: number, y: number, z: number) => hash31(i.add(vec3(x, y, z)));
  return mix(
    mix(mix(h(0, 0, 0), h(1, 0, 0), f.x), mix(h(0, 1, 0), h(1, 1, 0), f.x), f.y),
    mix(mix(h(0, 0, 1), h(1, 0, 1), f.x), mix(h(0, 1, 1), h(1, 1, 1), f.x), f.y), f.z);
});

export const venomSmokeNodes = (u: UniformNodes): ShaderNodes => {
  const pos: N = P.toVarying('vSmokePosition');
  const p = pos.add(vec3(u.uSeed.mul(2.7), u.uSeed.mul(0.83), u.uTime.mul(0.18)));
  const field = noise3(p.mul(2.2)).mul(0.55)
    .add(noise3(p.mul(4.6).add(vec3(0, 0, u.uTime.mul(0.2)))).mul(0.3))
    .add(noise3(p.mul(8.0).sub(vec3(u.uTime.mul(0.25), 0, 0))).mul(0.15));
  const ragged = field.mul(0.44).add(0.72);
  const edge = smoothstep(ragged, ragged.add(0.24), length(pos)).oneMinus();
  const alpha = discardBelow(edge.mul(smoothstep(0.28, 0.62, field)).mul(u.uOpacity), 0.018);
  const color = mix(vec3(0.04, 0.2, 0.03), vec3(0.2, 0.44, 0.075), smoothstep(0.28, 0.8, field));
  return { colorNode: color, opacityNode: alpha };
};

// ---- Fireball: the impact flipbook particles and flash ----
const hash21 = Fn(([p]: [N]): N => fract(sin(dot(p, vec2(127.1, 311.7))).mul(43758.5453)));
const noise2 = Fn(([p]: [N]): N => {
  const i = floor(p), f0 = fract(p);
  const f: N = f0.mul(f0).mul(vec2(3.0).sub(f0.mul(2.0)));
  return mix(mix(hash21(i), hash21(i.add(vec2(1, 0))), f.x), mix(hash21(i.add(vec2(0, 1))), hash21(i.add(vec2(1, 1))), f.x), f.y);
});

/** Camera-facing quad of `size`, rotated by `rotation`, centred on `center` (view-space offset). */
const billboard = (center: N, rotation: N, size: N): N => {
  const c: N = modelViewMatrix.mul(vec4(center, 1.0));
  const cs = cos(rotation), sn = sin(rotation);
  const p = vec2(P.x.mul(cs).sub(P.y.mul(sn)), P.x.mul(sn).add(P.y.mul(cs))).mul(size);
  return cameraProjectionMatrix.mul(vec4(c.xy.add(p), c.zw));
};

/** The impact's soft-particle depth fade is skipped: the game always builds it without a scene
 * depth texture (uSoftParticles = 0), so that branch never runs. */
export const fireballImpactNodes = (u: UniformNodes): ShaderNodes => {
  const vertexNode = billboard(attribute('aCenter', 'vec3'), attribute('aRotation', 'float'), attribute('aSize', 'float'));
  const frameV: N = attribute('aFrame', 'float'), alphaV: N = attribute('aAlpha', 'float'), kind: N = attribute('aKind', 'float');
  const tint: N = attribute('aTint', 'vec3'), intensity: N = attribute('aIntensity', 'float');
  const out = Fn(() => {
    const coord: N = uv();
    const alpha = float(0).toVar(), color: N = vec3(tint).toVar();
    If(kind.lessThan(0.5), () => {
      const frame = floor(mod(frameV, 16.0));
      const tile = vec2(mod(frame, 4.0), floor(frame.div(4.0)).oneMinus().add(2.0));
      const texel: N = u.uFlipbook.sample(clamp(coord, vec2(0.015), vec2(0.985)).add(tile).mul(0.25));
      alpha.assign(texel.a.mul(alphaV));
      color.mulAssign(texel.rgb);
    }).ElseIf(kind.lessThan(1.5), () => {
      const d = length(coord.sub(0.5).mul(vec2(1.0, 1.8)));
      alpha.assign(smoothstep(0.02, 0.5, d).oneMinus().mul(alphaV));
      color.mulAssign(mix(vec3(1.0, 0.98, 0.78), vec3(1.0, 0.18, 0.015), smoothstep(0.05, 0.42, d)));
    }).Else(() => {
      const q = coord.sub(0.5).mul(vec2(1.0, 1.35));
      const n = noise2(coord.mul(5.3).add(vec2(frameV.mul(0.008), frameV.mul(-0.013)))).mul(0.55).add(noise2(coord.mul(11.0).add(frameV.mul(0.004))).mul(0.25));
      const d = length(q);
      alpha.assign(smoothstep(n.mul(0.12).add(0.18), n.mul(0.2).add(0.58), d).oneMinus().mul(alphaV));
      color.mulAssign(n.mul(0.55).add(0.62));
    });
    If(alpha.lessThan(0.004), () => { Discard(); });
    return vec4(color.mul(intensity), alpha);
  })();
  return { vertexNode, colorNode: out.rgb, opacityNode: out.a };
};

export const fireballFlashNodes = (u: UniformNodes): ShaderNodes => {
  const d = length(uv().sub(0.5).mul(vec2(1.0, 1.15)));
  const core = exp(d.mul(-18.0)), halo = exp(d.mul(-5.0));
  const a = discardBelow(core.add(halo.mul(0.36)).mul(u.uOpacity), 0.005);
  const c = mix(vec3(1.0, 0.46, 0.04), u.uTint, core);
  return { colorNode: c.mul(a).mul(u.uIntensity), opacityNode: a };
};

// Ashima's 3D simplex noise (OldFireBall's NOISE_GLSL), as TSL.
const mod289 = (x: N): N => x.sub(floor(x.mul(1.0 / 289.0)).mul(289.0));
const permute = (x: N): N => mod289(x.mul(34.0).add(1.0).mul(x));
const snoise = Fn(([v]: [N]): N => {
  const C = vec2(1.0 / 6.0, 1.0 / 3.0), D = vec4(0.0, 0.5, 1.0, 2.0);
  const i0: N = floor(v.add(dot(v, C.yyy)));
  const x0: N = v.sub(i0).add(dot(i0, C.xxx));
  const g: N = step(x0.yzx, x0.xyz), l: N = g.oneMinus();
  const i1: N = min(g.xyz, l.zxy), i2: N = max(g.xyz, l.zxy);
  const x1 = x0.sub(i1).add(C.xxx), x2 = x0.sub(i2).add(C.yyy), x3 = x0.sub(D.yyy);
  const i: N = mod289(i0);
  const p: N = permute(permute(permute(i.z.add(vec4(0.0, i1.z, i2.z, 1.0))).add(i.y).add(vec4(0.0, i1.y, i2.y, 1.0))).add(i.x).add(vec4(0.0, i1.x, i2.x, 1.0)));
  const ns: N = D.wyz.mul(0.142857142857).sub(D.xzx);
  const j = p.sub(floor(p.mul(ns.z).mul(ns.z)).mul(49.0));
  const x_ = floor(j.mul(ns.z)), y_ = floor(j.sub(x_.mul(7.0)));
  const x: N = x_.mul(ns.x).add(ns.yyyy), y: N = y_.mul(ns.x).add(ns.yyyy);
  const h: N = vec4(1.0).sub(abs(x)).sub(abs(y));
  const b0: N = vec4(x.xy, y.xy), b1: N = vec4(x.zw, y.zw);
  const s0: N = floor(b0).mul(2.0).add(1.0), s1: N = floor(b1).mul(2.0).add(1.0);
  const sh: N = step(h, vec4(0.0)).negate();
  const a0: N = b0.xzyw.add(s0.xzyw.mul(sh.xxyy)), a1: N = b1.xzyw.add(s1.xzyw.mul(sh.zzww));
  const p0 = vec3(a0.xy, h.x), p1 = vec3(a0.zw, h.y), p2 = vec3(a1.xy, h.z), p3 = vec3(a1.zw, h.w);
  const norm: N = vec4(1.79284291400159).sub(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)).mul(0.85373472095314));
  const m0: N = max(vec4(0.6).sub(vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3))), 0.0);
  const m = m0.mul(m0);
  return dot(m.mul(m), vec4(dot(p0.mul(norm.x), x0), dot(p1.mul(norm.y), x1), dot(p2.mul(norm.z), x2), dot(p3.mul(norm.w), x3))).mul(42.0);
});
const snoiseFbm = Fn(([pIn]: [N]): N => {
  const p: N = vec3(pIn).toVar(), f = float(0).toVar(), a = float(0.5).toVar();
  for (let k = 0; k < 4; k++) { f.addAssign(a.mul(snoise(p))); p.mulAssign(2.03); a.mulAssign(0.5); }
  return f;
});

/** OldFireBall's flame (core and shell); `edgeFade` is the shell's grazing-angle fade. */
export const oldFireballFlameNodes = (edgeFade: boolean) => (u: UniformNodes): ShaderNodes => {
  const p = P.mul(1.6).add(vec3(0.0, 0.0, u.uTime.mul(-1.4)));
  const n = snoiseFbm(p.add(vec3(u.uTime.mul(0.35), u.uTime.mul(0.2), 0.0)));
  const displaced = P.add(normalLocal.mul(n).mul(u.uAmp));
  const heatV: N = n.toVarying('vHeat');
  const normalV: N = transformNormalToView(normalLocal).toVarying('vNormalV');
  const viewDir: N = modelViewMatrix.mul(vec4(displaced, 1.0)).xyz.negate().normalize().toVarying('vViewDir');
  const facing = clamp(dot(normalV.normalize(), viewDir.normalize()), 0.0, 1.0);
  const heat = clamp(facing.mul(0.85).add(heatV.mul(0.6)).add(0.15), 0.0, 1.0);
  const c1 = mix(vec3(0.55, 0.06, 0.01), vec3(1.0, 0.42, 0.05), smoothstep(0.0, 0.45, heat));
  const c2 = mix(c1, vec3(1.0, 0.82, 0.35), smoothstep(0.45, 0.75, heat));
  const col = mix(c2, vec3(1.0, 0.97, 0.88), smoothstep(0.8, 1.0, heat));
  const alpha = edgeFade ? u.uOpacity.mul(smoothstep(0.0, 0.55, facing)) : u.uOpacity;
  return { positionNode: displaced, colorNode: col.mul(alpha), opacityNode: alpha };
};

// ---- Magic Missile V2: bolt core and shell, trail ----
const mmBoltPosition = (u: UniformNodes): N => {
  const n = sin(P.x.mul(14.0).add(u.uTime.mul(8.0))).mul(cos(P.y.mul(17.0).sub(u.uTime.mul(6.0)))).add(sin(P.z.mul(23.0).add(u.uTime.mul(4.0))).mul(0.35));
  const p = P.add(P.add(vec3(0.001)).normalize().mul(n).mul(u.uDistort));
  return vec3(p.x.mul(0.55), p.y, p.z).mul(u.uScale);
};

export const mmCoreNodes = (u: UniformNodes): ShaderNodes => {
  const positionNode = mmBoltPosition(u);
  const vP: N = positionNode.toVarying('vP');
  const center = smoothstep(0.015, 0.16, length(vP.xy)).oneMinus();
  const pulse = sin(u.uTime.mul(19.0).add(vP.z.mul(20.0))).mul(0.16).add(0.84);
  const c0 = mix(vec3(0.17, 0.045, 0.55), vec3(0.72, 0.46, 1.0), center);
  const c = mix(c0, vec3(0.97, 0.91, 1.0), pow(center, 3.0));
  return { positionNode, colorNode: c.mul(u.uGlow).mul(pulse), opacityNode: float(0.98) };
};

export const mmShellNodes = (u: UniformNodes): ShaderNodes => {
  const positionNode = mmBoltPosition(u);
  const vP: N = positionNode.toVarying('vP');
  const n = sin(vP.x.mul(39.0).add(u.uTime.mul(11.0))).mul(sin(vP.y.mul(31.0).sub(u.uTime.mul(9.0)))).mul(sin(vP.z.mul(27.0).add(u.uTime.mul(8.0))));
  const bands = abs(sin(atan(vP.y, vP.x).mul(6.0).add(u.uTime.mul(4.0)))).mul(0.56).add(0.44);
  const a = clamp(n.mul(0.3).add(0.48).mul(bands).mul(u.uOpacity), 0.0, 0.82);
  const c = mix(vec3(0.13, 0.025, 0.43), vec3(0.65, 0.35, 0.98), bands);
  return { positionNode, colorNode: c.mul(u.uGlow), opacityNode: a };
};

export const mmTrailNodes = (u: UniformNodes): ShaderNodes => {
  const t: N = uv();
  const tip = smoothstep(0.0, 0.25, t.x);
  const fade = pow(t.x, 1.7);
  const noise = sin(t.x.mul(72.0).add(u.uTime.mul(8.0))).mul(sin(t.x.mul(31.0).sub(u.uTime.mul(5.0))));
  const cuts = smoothstep(u.uBreakup.mul(0.5).sub(0.4), u.uBreakup.mul(0.5).add(0.48), noise);
  const core = smoothstep(0.22, 0.5, abs(t.y.sub(0.5))).oneMinus();
  const c0 = mix(vec3(0.12, 0.025, 0.46), vec3(0.75, 0.52, 1.0), tip);
  const c = mix(c0, vec3(0.97, 0.92, 1.0), core.mul(tip));
  return { colorNode: c, opacityNode: u.uOpacity.mul(fade).mul(cuts).mul(core.mul(0.65).add(0.35)) };
};

// ---- Phantasmal Force: tendrils, core, shell ----
export const pfTendrilNodes = (u: UniformNodes): ShaderNodes => {
  const along: N = attribute('aAlong', 'float');
  const wave = sin(u.uTime.mul(3.4).add(along.mul(18.0)).add(P.z.mul(4.0)));
  const wave2 = cos(u.uTime.mul(2.1).sub(along.mul(13.0)).add(P.x.mul(5.0)));
  const noiseV: N = wave.mul(0.5).add(wave2.mul(0.5)).toVarying('vNoise');
  const xy = P.xy.add(vec2(wave, wave2).mul(u.uTurbulence).mul(along.add(0.25)));
  const xz = vec2(xy.x, P.z).add(vec2(wave2, wave).mul(u.uSplineNoise).mul(sin(along.mul(3.14159))));
  const positionNode = vec3(xz.x, xy.y, xz.y).mul(mix(1.0, 0.07, u.uCollapse));
  const aV: N = along.toVarying('vAlong');
  const ph = fract(u.uTime.mul(0.9).add(aV));
  const pulse = smoothstep(0.0, 0.22, ph).mul(smoothstep(0.28, 0.55, ph).oneMinus());
  const c0 = mix(vec3(0.12, 0.08, 0.72), vec3(0.42, 0.22, 1.0), smoothstep(0.0, 0.65, aV));
  const color = mix(c0, vec3(0.77, 0.63, 1.0), clamp(pulse.mul(0.82).add(smoothstep(0.78, 1.0, aV).mul(0.35)), 0.0, 1.0));
  const flicker = sin(u.uTime.mul(11.0).add(noiseV.mul(4.0))).mul(0.28).add(0.72);
  return { positionNode, colorNode: color.mul(pulse.mul(2.5).add(1.5)), opacityNode: u.uOpacity.mul(flicker).mul(pulse.mul(0.52).add(0.48)) };
};

export const pfCoreNodes = (u: UniformNodes): ShaderNodes => {
  const n = sin(P.x.mul(18.0).add(u.uTime.mul(9.0))).mul(cos(P.y.mul(15.0).sub(u.uTime.mul(7.0)))).mul(sin(P.z.mul(17.0).add(u.uTime.mul(5.0))));
  const positionNode = P.add(normalLocal.mul(n).mul(u.uDistortion));
  const vP: N = positionNode.toVarying('vP');
  const turbulence = sin(vP.x.mul(24.0).add(u.uTime.mul(11.0))).mul(cos(vP.y.mul(22.0).sub(u.uTime.mul(8.0)))).mul(sin(vP.z.mul(25.0).add(u.uTime.mul(6.0)))).mul(0.5).add(0.5);
  const c0 = mix(vec3(0.33, 0.13, 1.0), vec3(0.76, 0.61, 1.0), turbulence);
  const color = mix(c0, vec3(1.0, 0.96, 1.0), smoothstep(0.75, 1.0, turbulence));
  return { positionNode, colorNode: color.mul(u.uEmission), opacityNode: float(0.98) };
};

export const pfShellNodes = (u: UniformNodes): ShaderNodes => {
  const n = sin(P.x.mul(9.0).add(u.uTime.mul(13.0))).mul(cos(P.y.mul(8.0).sub(u.uTime.mul(9.0)))).mul(sin(P.z.mul(10.0).add(u.uTime.mul(7.0))));
  const positionNode = P.add(normalLocal.mul(n).mul(u.uDistortion));
  const normalV: N = transformNormalToView(normalLocal).toVarying('vNormal');
  const warp: N = n.toVarying('vWarp');
  const edge = pow(abs(normalV.normalize().z).oneMinus(), 1.5);
  const color = mix(vec3(0.28, 0.16, 0.96), vec3(0.86, 0.75, 1.0), clamp(edge.add(abs(warp).mul(u.uDistortion)), 0.0, 1.0));
  return { positionNode, colorNode: color.mul(2.3), opacityNode: u.uOpacity.mul(edge.mul(0.8).add(0.2)) };
};

// ---- Web of Dreams: the thread web ----
export const webThreadNodes = (u: UniformNodes): ShaderNodes => {
  const along: N = attribute('aAlong', 'float'), delay: N = attribute('aDelay', 'float'), phase: N = attribute('aPhase', 'float');
  const grown = smoothstep(delay, delay.add(0.18), u.uFormation.sub(along.mul(0.2)));
  const wave = sin(u.uTime.mul(2.7).add(along.mul(17.0)).add(phase)).mul(u.uDisplacement);
  const wave2 = cos(u.uTime.mul(1.9).sub(along.mul(12.0)).add(phase.mul(1.7))).mul(u.uDisplacement);
  const x = P.x.add(wave.mul(along.add(0.25)));
  const y = P.y.add(wave2.mul(along.add(0.35)));
  const z = P.z.add(sin(u.uTime.mul(2.1).add(along.mul(9.0)).add(phase)).mul(u.uDisplacement).mul(1.6));
  const tighten = mix(1.0, u.uTightening.oneMinus(), smoothstep(0.68, 0.92, u.uFormation));
  const positionNode = vec3(vec2(x, y).mul(tighten), z);
  const pulseT = fract(u.uTime.mul(u.uPulseSpeed).mul(0.27).add(along).add(phase));
  const pulse: N = smoothstep(0.0, 0.07, pulseT).mul(smoothstep(0.11, 0.19, pulseT).oneMinus()).toVarying('vPulse');
  const aV: N = along.toVarying('vAlong'), visible: N = grown.toVarying('vVisible');
  const normalV: N = transformNormalToView(normalLocal).toVarying('vNormal');
  const viewDir: N = modelViewMatrix.mul(vec4(positionNode, 1.0)).xyz.negate().normalize().toVarying('vViewDirection');
  const lavender = vec3(0.78, 0.66, 1.0);
  const c0 = mix(vec3(0.12, 0.075, 0.68), vec3(0.42, 0.18, 0.94), smoothstep(0.02, 0.72, aV));
  const c1 = mix(c0, lavender, clamp(pulse.mul(0.92).add(smoothstep(0.76, 1.0, aV).mul(0.16)), 0.0, 1.0));
  const c2 = mix(c1, vec3(0.57, 0.78, 1.0), pulse.mul(0.12));
  const bevel = abs(dot(normalV.normalize(), vec3(-0.36, 0.48, 0.8).normalize())).mul(0.58).add(0.42);
  const rim = pow(abs(dot(normalV.normalize(), viewDir.normalize())).oneMinus(), 2.0);
  const color = c2.mul(bevel).add(lavender.mul(rim).mul(0.62));
  return { positionNode, colorNode: color.mul(u.uEnergy).mul(pulse.mul(1.6).add(0.72)), opacityNode: u.uOpacity.mul(visible).mul(pulse.mul(0.5).add(0.48)) };
};

// ---- ThreeVfxSystem: flipbook flame particles (Burning Hands, element emitters) ----
/** Same as the impact: the soft-particle depth fade is skipped (built without a scene depth texture). */
export const flipbookParticleNodes = (u: UniformNodes): ShaderNodes => {
  const vertexNode = billboard(attribute('aCenter', 'vec3'), attribute('aRotation', 'float'), attribute('aSize', 'float'));
  const frameV: N = mod(attribute('aFrame', 'float'), 16.0).toVarying('vFrame');
  const alphaV: N = attribute('aOpacity', 'float'), emissive: N = attribute('aEmissive', 'float').mul(u.uIntensity), tint: N = attribute('aTint', 'vec3');
  const frame = floor(mod(frameV, 16.0));
  const tile = vec2(mod(frame, 4.0), floor(frame.div(4.0)).oneMinus().add(2.0));
  const local0: N = clamp(uv(), vec2(0.02), vec2(0.98));
  const turbulence = sin(local0.y.mul(13.0).add(frameV.mul(0.37))).mul(sin(local0.x.mul(9.0).sub(frameV.mul(0.29))));
  const local = local0.add(vec2(turbulence.mul(0.014), turbulence.mul(0.006)));
  const flame: N = u.uFlipbook.sample(clamp(local, vec2(0.02), vec2(0.98)).add(tile).mul(0.25));
  const alpha = discardBelow(flame.a.mul(alphaV), 0.006);
  return { vertexNode, colorNode: flame.rgb.mul(tint).mul(emissive), opacityNode: alpha };
};
