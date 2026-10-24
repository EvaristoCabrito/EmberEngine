/** Pre-baked tileable noise, built once on the CPU at startup instead of evaluated per-pixel
 * every frame (per-pixel fbm/voronoi loops are the expensive part of a shader like this — the
 * texture fetch that replaces them is nearly free).
 *
 * Channels of the resulting RGBA texture:
 *   R, G, B — three octaves of tileable value noise (low/mid/high frequency). Shaders combine
 *             them with a weighted sum to approximate fbm from a single texture fetch.
 *   A       — a Voronoi cell-edge factor (0 at cell centers, 1 on cracks) baked once here so
 *             the ice shader never runs the 3x3 neighbor search live.
 * The whole thing tiles seamlessly so any element can scroll/scale its UVs freely. */

function hash2(x: number, y: number, seed: number): number {
  const h = Math.sin(x * 127.1 + y * 311.7 + seed * 74.7) * 43758.5453123;
  return h - Math.floor(h);
}

/** Tileable bilinear value noise: lattice coordinates wrap mod `freq` so edges match up. */
function valueNoiseTileable(u: number, v: number, freq: number, seed: number): number {
  const xf = u * freq;
  const yf = v * freq;
  const x0 = Math.floor(xf);
  const y0 = Math.floor(yf);
  const tx = xf - x0;
  const ty = yf - y0;
  const wrap = (n: number) => ((n % freq) + freq) % freq;
  const h00 = hash2(wrap(x0), wrap(y0), seed);
  const h10 = hash2(wrap(x0 + 1), wrap(y0), seed);
  const h01 = hash2(wrap(x0), wrap(y0 + 1), seed);
  const h11 = hash2(wrap(x0 + 1), wrap(y0 + 1), seed);
  const sx = tx * tx * (3 - 2 * tx);
  const sy = ty * ty * (3 - 2 * ty);
  const a = h00 + (h10 - h00) * sx;
  const b = h01 + (h11 - h01) * sx;
  return a + (b - a) * sy;
}

/** Tileable Voronoi F2-F1 edge factor over an integer feature-point grid. */
function voronoiEdgeTileable(u: number, v: number, cells: number, seed: number): number {
  const xf = u * cells;
  const yf = v * cells;
  const xi = Math.floor(xf);
  const yi = Math.floor(yf);
  let f1 = 8;
  let f2 = 8;
  for (let oy = -1; oy <= 1; oy++) {
    for (let ox = -1; ox <= 1; ox++) {
      const cx = ((xi + ox) % cells + cells) % cells;
      const cy = ((yi + oy) % cells + cells) % cells;
      const jitterX = hash2(cx, cy, seed);
      const jitterY = hash2(cx, cy, seed + 91.7);
      const px = xi + ox + jitterX;
      const py = yi + oy + jitterY;
      const dx = px - xf;
      const dy = py - yf;
      const d = Math.sqrt(dx * dx + dy * dy);
      if (d < f1) {
        f2 = f1;
        f1 = d;
      } else if (d < f2) {
        f2 = d;
      }
    }
  }
  return Math.min(1, f2 - f1);
}

export function buildNoiseTexture(gl: WebGL2RenderingContext, size = 256): WebGLTexture {
  const data = new Uint8Array(size * size * 4);
  for (let y = 0; y < size; y++) {
    const v = y / size;
    for (let x = 0; x < size; x++) {
      const u = x / size;
      const r = valueNoiseTileable(u, v, 4, 1.7);
      const g = valueNoiseTileable(u, v, 9, 5.3);
      const b = valueNoiseTileable(u, v, 17, 11.1);
      const edge = voronoiEdgeTileable(u, v, 6, 3.9);
      const i = (y * size + x) * 4;
      data[i] = Math.round(r * 255);
      data[i + 1] = Math.round(g * 255);
      data[i + 2] = Math.round(b * 255);
      data[i + 3] = Math.round(edge * 255);
    }
  }
  const tex = gl.createTexture();
  if (!tex) throw new Error("gl.createTexture failed");
  gl.bindTexture(gl.TEXTURE_2D, tex);
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, size, size, 0, gl.RGBA, gl.UNSIGNED_BYTE, data);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.REPEAT);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.REPEAT);
  return tex;
}
