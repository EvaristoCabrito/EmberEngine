import * as THREE from "three";

export interface LandscapeSurface {
  geometry: THREE.BufferGeometry;
  minX: number; minY: number; maxX: number; maxY: number;
  heightAt(x: number, y: number): number;
}

/** Joined hex ground with exact six-sided perimeter and skirts only on exposed edges. */
export function buildHexLandscape(
  bounds: { minX: number; minY: number; maxX: number; maxY: number },
  centers: { x: number; y: number }[], radius: number, baseDepth: number,
  elevation: (x: number, y: number) => number,
): LandscapeSurface {
  const vertices: number[] = [], uv: number[] = [], indices: number[] = [];
  const shared = new Map<string, number>();
  const edges = new Map<string, { a: number; b: number; count: number }>();
  const vertex = (x: number, y: number) => {
    const key = `${Math.round(x * 1e6)},${Math.round(y * 1e6)}`;
    const existing = shared.get(key);
    if (existing !== undefined) return existing;
    const index = vertices.length / 3;
    vertices.push(x, y, elevation(x, y));
    uv.push((x - bounds.minX) / (bounds.maxX - bounds.minX), (y - bounds.minY) / (bounds.maxY - bounds.minY));
    shared.set(key, index); return index;
  };
  for (const center of centers) {
    const middle = vertex(center.x, center.y);
    const ring = Array.from({ length: 6 }, (_, i) => {
      const angle = (60 * i - 30) * Math.PI / 180;
      return vertex(center.x + Math.cos(angle) * radius, center.y + Math.sin(angle) * radius);
    });
    for (let i = 0; i < 6; i++) {
      const a = ring[i], b = ring[(i + 1) % 6];
      indices.push(middle, a, b);
      const key = `${Math.min(a, b)}:${Math.max(a, b)}`;
      const edge = edges.get(key);
      if (edge) edge.count++; else edges.set(key, { a, b, count: 1 });
    }
  }
  const topCount = indices.length;
  for (const { a, b, count } of edges.values()) {
    if (count !== 1) continue;
    const i = vertices.length / 3;
    vertices.push(...vertices.slice(a * 3, a * 3 + 3), ...vertices.slice(b * 3, b * 3 + 3),
      vertices[a * 3], vertices[a * 3 + 1], -baseDepth, vertices[b * 3], vertices[b * 3 + 1], -baseDepth);
    uv.push(0, 1, 1, 1, 0, 0, 1, 0);
    indices.push(i, i + 2, i + 1, i + 1, i + 2, i + 3);
  }
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute("position", new THREE.Float32BufferAttribute(vertices, 3));
  geometry.setAttribute("uv", new THREE.Float32BufferAttribute(uv, 2));
  geometry.setIndex(indices);
  geometry.addGroup(0, topCount, 0);
  geometry.addGroup(topCount, indices.length - topCount, 1);
  geometry.computeVertexNormals(); geometry.computeBoundingSphere();
  return { geometry, heightAt: elevation, ...bounds };
}

/** Continuous triangular landscape; gameplay hexes supply heights, never mesh topology. */
export function buildLandscape(
  bounds: { minX: number; minY: number; maxX: number; maxY: number },
  spacing: number | { x: number; y: number }, baseDepth: number,
  elevation: (x: number, y: number) => number,
  land: (x: number, y: number) => boolean,
  buildEdgeWalls = true,
  cuts: { x: number[]; y: number[] } = { x: [], y: [] },
): LandscapeSurface {
  const { minX, minY, maxX, maxY } = bounds;
  const spacingX = typeof spacing === "number" ? spacing : spacing.x;
  const spacingY = typeof spacing === "number" ? spacing : spacing.y;
  const axis = (min: number, max: number, step: number, cuts: number[]) => {
    const count = Math.max(1, Math.ceil((max - min) / step - 1e-9));
    const values = [...Array.from({ length: count + 1 }, (_, i) => min + i * (max - min) / count), ...cuts.filter(v => v > min && v < max)].sort((a, b) => a - b);
    return values.filter((v, i) => i === 0 || v - values[i - 1] > 1e-6);
  };
  const xs = axis(minX, maxX, spacingX, cuts.x), ys = axis(minY, maxY, spacingY, cuts.y);
  const cols = xs.length - 1, rows = ys.length - 1;
  const vertices: number[] = [], uv: number[] = [], heights: number[] = [], indices: number[] = [];
  const edgeCounts = new Map<string, { a: number; b: number; count: number }>();
  for (let row = 0; row <= rows; row++) for (let col = 0; col <= cols; col++) {
    const x = xs[col], y = ys[row];
    const z = elevation(x, y);
    heights.push(z); vertices.push(x, y, z); uv.push((x-minX)/(maxX-minX), (y-minY)/(maxY-minY));
  }
  const triangle = (a: number, b: number, c: number) => {
    const x = (vertices[a * 3] + vertices[b * 3] + vertices[c * 3]) / 3;
    const y = (vertices[a * 3 + 1] + vertices[b * 3 + 1] + vertices[c * 3 + 1]) / 3;
    if (!land(x, y)) return;
    indices.push(a, b, c);
    for (const [u, v] of [[a, b], [b, c], [c, a]]) {
      const key = `${Math.min(u, v)}:${Math.max(u, v)}`;
      const edge = edgeCounts.get(key);
      if (edge) edge.count++;
      else edgeCounts.set(key, { a: u, b: v, count: 1 });
    }
  };
  for (let row = 0; row < rows; row++) for (let col = 0; col < cols; col++) {
    const a = row * (cols + 1) + col, b = a + 1, d = a + cols + 1, c = d + 1;
    triangle(a, b, d); triangle(b, c, d);
  }
  const topCount = indices.length;
  // Optional terrain skirts are for exposed outdoor ground; authored architecture is
  // rendered from placed wall pieces so indoor edges never become cave-textured boxes.
  if (buildEdgeWalls) for (const edge of edgeCounts.values()) {
    if (edge.count !== 1) continue;
    const { a, b } = edge;
    const i = vertices.length / 3;
    vertices.push(vertices[a*3], vertices[a*3+1], heights[a], vertices[b*3], vertices[b*3+1], heights[b],
      vertices[a*3], vertices[a*3+1], -baseDepth, vertices[b*3], vertices[b*3+1], -baseDepth);
    const length = Math.hypot(vertices[a*3]-vertices[b*3], vertices[a*3+1]-vertices[b*3+1]) / spacingX;
    uv.push(0, heights[a]/spacingX, length, heights[b]/spacingX, 0, -baseDepth/spacingX, length, -baseDepth/spacingX);
    indices.push(i, i+2, i+1, i+1, i+2, i+3);
  }
  // A matching bottom cap closes each top triangle. It shares no vertices with the top.
  const bottomStart = vertices.length / 3;
  for (let i = 0; i < heights.length; i++) {
    vertices.push(vertices[i*3], vertices[i*3+1], -baseDepth); uv.push(0, 0);
  }
  for (let i = 0; i < topCount; i += 3) indices.push(bottomStart+indices[i], bottomStart+indices[i+2], bottomStart+indices[i+1]);
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute("position", new THREE.Float32BufferAttribute(vertices, 3));
  geometry.setAttribute("uv", new THREE.Float32BufferAttribute(uv, 2));
  geometry.setIndex(indices);
  geometry.addGroup(0, topCount, 0);
  geometry.addGroup(topCount, indices.length - topCount, 1);
  geometry.computeVertexNormals(); geometry.computeBoundingSphere();
  const heightAt = (x: number, y: number) => {
    const interval = (values: number[], point: number) => {
      let low = 0, high = values.length - 1;
      while (high - low > 1) { const mid = (low + high) >> 1; if (values[mid] <= point) low = mid; else high = mid; }
      return low;
    };
    const col = interval(xs, x), row = interval(ys, y);
    const u = THREE.MathUtils.clamp((x-xs[col])/(xs[col+1]-xs[col]), 0, 1);
    const v = THREE.MathUtils.clamp((y-ys[row])/(ys[row+1]-ys[row]), 0, 1);
    const a = row*(cols+1)+col, b = a+1, d = a+cols+1, c = d+1;
    // Match the actual triangle, so feet and cursor outlines stay on the rendered surface.
    return u+v <= 1 ? heights[a]+(heights[b]-heights[a])*u+(heights[d]-heights[a])*v
      : heights[c]+(heights[d]-heights[c])*(1-u)+(heights[b]-heights[c])*(1-v);
  };
  return { geometry, heightAt, ...bounds };
}
