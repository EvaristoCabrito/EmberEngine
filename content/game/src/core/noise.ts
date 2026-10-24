// Deterministic noise used for terrain shaping and code-generated textures.

export function hash(x: number, y: number): number {
  let h = (Math.imul(x, 374761393) + Math.imul(y, 668265263)) | 0;
  h = Math.imul(h ^ (h >>> 13), 1274126177);
  h ^= h >>> 16;
  return (h >>> 0) / 4294967296;
}

export const sstep = (t: number): number => t * t * (3 - 2 * t);

export function smooth(a: number, b: number, x: number): number {
  const t = Math.min(1, Math.max(0, (x - a) / (b - a)));
  return t * t * (3 - 2 * t);
}

export const lerp = (a: number, b: number, t: number): number => a + (b - a) * t;

export function vnoise(x: number, y: number): number {
  const xi = Math.floor(x), yi = Math.floor(y), u = sstep(x - xi), v = sstep(y - yi);
  const a = hash(xi, yi), b = hash(xi + 1, yi), c = hash(xi, yi + 1), d = hash(xi + 1, yi + 1);
  return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}

export function fbm(x: number, y: number, oct = 4): number {
  let t = 0, a = 0.5, f = 1, n = 0;
  for (let i = 0; i < oct; i++) { t += a * vnoise(x * f, y * f); n += a; a *= 0.5; f *= 2; }
  return t / n;
}

/** Tileable value noise: repeats every (px, py) units, so textures wrap without seams. */
export function pnoise(x: number, y: number, px: number, py: number): number {
  const xi = Math.floor(x), yi = Math.floor(y), u = sstep(x - xi), v = sstep(y - yi);
  const H = (a: number, b: number) => hash(((a % px) + px) % px, ((b % py) + py) % py);
  const a = H(xi, yi), b = H(xi + 1, yi), c = H(xi, yi + 1), d = H(xi + 1, yi + 1);
  return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}

export function pfbm(x: number, y: number, px: number, py: number, oct = 5): number {
  let t = 0, a = 0.5, n = 0;
  for (let i = 0; i < oct; i++) { t += a * pnoise(x, y, px, py); n += a; x *= 2; y *= 2; px *= 2; py *= 2; a *= 0.5; }
  return t / n;
}

export function makeRand(seed: number): () => number {
  let s = seed >>> 0;
  return () => ((s = (Math.imul(s, 1664525) + 1013904223) >>> 0) / 4294967296);
}
