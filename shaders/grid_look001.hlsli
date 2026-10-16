// Battle grid look 001 "luminous" (looks/grid/README.md; frozen: a change is a new look).
// Included by terrain.hlsl after hexLocate / hexRadial. Reads push.gridCols / gridRows / grid (one Renderer::GridCell
// per hex: flags, extra) and push.gridSeconds, and paints in display space (sRGB) like the ground marks, so the grid
// keeps its colours in any light. Everything is signed distance: region outlines with a glow on both sides, a faint
// lattice inside a region, the route's chevrons flowing towards the destination, the cursor's corner brackets, the
// picked unit's turning ring, the target locks, hazard stripes on enemy zones. Hex radius 1, pointy-top, odd rows
// shifted +x; p = offset from the hex centre (x east, y = +z south); directions k with edge normals at 60k degrees:
// 0 E, 1 SE, 2 SW, 3 W, 4 NW, 5 NE.

static const uint kGridReach = 1u, kGridReachFoe = 2u, kGridTarget = 4u, kGridSpellAim = 8u, kGridSpellArea = 16u,
                  kGridSpellChosen = 32u, kGridHover = 64u, kGridHoverBlocked = 128u, kGridActive = 256u,
                  kGridActiveFoe = 512u, kGridRouteEnd = 1024u;
static const float kApothem = 0.8660254;
static const float kTwoPi = 6.2831853;
static const float kNoEdge = 9.0;

uint2 gridCellAt(int2 cr) {
  if (cr.x < 0 || cr.y < 0 || cr.x >= int(push.gridCols) || cr.y >= int(push.gridRows)) return uint2(0, 0);
  return vk::RawBufferLoad<uint2>(push.grid + (uint64_t(cr.y) * push.gridCols + uint64_t(cr.x)) * 8);
}

int2 gridNeighbour(int2 cr, uint k) {
  const int odd = cr.y & 1;
  switch (k) {
    case 0: return int2(cr.x + 1, cr.y);
    case 1: return int2(cr.x + odd, cr.y + 1);
    case 2: return int2(cr.x + odd - 1, cr.y + 1);
    case 3: return int2(cr.x - 1, cr.y);
    case 4: return int2(cr.x + odd - 1, cr.y - 1);
    default: return int2(cr.x + odd, cr.y - 1);
  }
}

float2 gridNormal(uint k) {
  const float a = float(k) * (kTwoPi / 6.0);
  return float2(cos(a), sin(a));
}

// Painting: a fill or line sits over the ground; a glow lightens it (screen), so halos never darken.
void gridPaint(inout float3 display, inout bool any, float3 color, float alpha) {
  alpha = saturate(alpha);
  if (alpha <= 0.001) return;
  display = lerp(display, color, alpha);
  any = true;
}
void gridGlow(inout float3 display, inout bool any, float3 color, float alpha) {
  alpha = saturate(alpha);
  if (alpha <= 0.001) return;
  display = display + color * alpha * (1.0 - display);
  any = true;
}

// An anti-aliased line: 1 within `width` of the distance, fading over a pixel.
float gridLine(float dist, float width, float pix) { return 1.0 - smoothstep(width - pix, width + pix, dist); }

// Hazard stripes in world space, drifting slowly: 1 on a stripe, 0 between.
float gridStripes(float2 xz, float t, float pix) {
  const float u = (xz.x + xz.y) * 1.25 - t * 0.18;
  const float v = abs(frac(u) - 0.5) * 2.0;
  const float aa = 2.5 * pix;
  return smoothstep(0.5 - aa, 0.5 + aa, v);
}

// A region: hexes sharing a flag. `edge` = distance to the nearest edge where the region meets hexes outside it
// (0 on the edge), for the fragment inside or outside; kNoEdge when none of this hex's edges is one.
struct GridRegion {
  bool inside;
  float edge;
};
GridRegion gridRegion(uint flags, uint nb[6], uint flag, float2 p) {
  GridRegion r;
  r.inside = (flags & flag) != 0;
  r.edge = kNoEdge;
  [unroll] for (uint k = 0; k < 6; ++k) {
    const bool n = (nb[k] & flag) != 0;
    if (n == r.inside) continue;
    const float e = dot(p, gridNormal(k)) / kApothem;  // 1 on edge k
    r.edge = min(r.edge, abs(1.0 - e));
  }
  return r;
}

// A region's look: fill (brighter towards its boundary), lattice between its hexes, the boundary line with a glow on
// both sides, breathing with `pulse`.
void gridPaintRegion(inout float3 display, inout bool any, GridRegion r, float3 fill, float fillAlpha, float3 edge, float edgeAlpha,
                     float glowReach, float pulse, float lattice, float d, float pix, float flat) {
  if (r.inside) {
    float a = fillAlpha;
    if (r.edge < kNoEdge) a += fillAlpha * 0.8 * exp(-r.edge / 0.3);
    gridPaint(display, any, fill, a * flat);
    if (lattice > 0.0) {
      const float lat = gridLine(1.0 - d, 0.014, pix) * lattice;
      gridPaint(display, any, edge, lat * flat);
    }
  }
  if (r.edge < kNoEdge) {
    const float core = gridLine(r.edge, 0.035, pix);
    const float halo = exp(-r.edge / glowReach) * (r.inside ? 0.5 : 0.3) * pulse;
    gridPaint(display, any, edge, core * edgeAlpha * flat);
    gridGlow(display, any, edge, halo * edgeAlpha * flat);
  }
}

float gridSegment(float2 p, float2 a, float2 b, out float t) {
  const float2 ab = b - a;
  t = saturate(dot(p - a, ab) / max(dot(ab, ab), 1e-6));
  return length(p - (a + ab * t));
}

// The route: through this hex from the previous step's shared edge to the next step's, with chevrons flowing towards
// the destination; a reticle on the destination. Returns true when something was painted.
void gridPaintRoute(inout float3 display, inout bool any, uint flags, uint extra, uint nb[6], float2 p, float d, float t, float pix, float flat) {
  const uint step = extra & 255u;
  if (step == 0) return;
  int kIn = -1, kOut = -1;
  [unroll] for (uint k = 0; k < 6; ++k) {
    const uint s = nb[k] & 255u;
    if (step > 1 && s == step - 1) kIn = int(k);
    if (s == step + 1) kOut = int(k);
  }
  const float3 color = float3(0.86, 0.95, 1.0);
  float dist = 9.0, along = 0.0, lateral = 0.0;
  if (kIn >= 0) {
    float tt;
    const float dd = gridSegment(p, gridNormal(uint(kIn)) * kApothem, float2(0.0, 0.0), tt);
    if (dd < dist) dist = dd, along = tt * kApothem, lateral = dd;
  }
  if (kOut >= 0) {
    float tt;
    const float dd = gridSegment(p, float2(0.0, 0.0), gridNormal(uint(kOut)) * kApothem, tt);
    if (dd < dist) dist = dd, along = kApothem + tt * kApothem, lateral = dd;
  }
  if (dist < 9.0) {
    along += float(step - 1) * 2.0 * kApothem;
    const float width = 0.085;
    const float body = gridLine(dist, width, pix);
    // chevrons: bands whose phase lags with the lateral distance, so each points along the route
    const float phase = frac((along - lateral * 1.4 - t * 1.5) / 0.5);
    const float chevron = smoothstep(0.5 - 3.0 * pix, 0.5 + 3.0 * pix, abs(phase - 0.5) * 2.0);
    gridPaint(display, any, float3(0.12, 0.2, 0.28), body * 0.55 * flat);
    gridPaint(display, any, color, body * (0.35 + 0.65 * chevron) * flat);
    gridGlow(display, any, float3(0.5, 0.8, 1.0), exp(-max(dist - width, 0.0) / 0.1) * 0.35 * flat);
  }
  if ((flags & kGridRouteEnd) != 0) {
    const float r = length(p);
    const float theta = atan2(p.y, p.x);
    // a ring with three turning gaps, four ticks, a breathing disc
    const float gap = frac(theta / kTwoPi * 3.0 - t * 0.15);
    const float ring = gridLine(abs(r - 0.6), 0.03, pix) * smoothstep(0.1, 0.16, gap);
    const float tickAngle = abs(frac(theta / kTwoPi * 4.0 + 0.5) - 0.5) * (kTwoPi / 4.0) * r;
    const float tick = gridLine(tickAngle, 0.03, pix) * (1.0 - smoothstep(0.78, 0.8 + pix, r)) * smoothstep(0.64 - pix, 0.66, r);
    const float breath = 0.5 + 0.5 * sin(t * 3.0);
    gridPaint(display, any, color, max(ring, tick) * 0.95 * flat);
    gridGlow(display, any, float3(0.5, 0.8, 1.0), (1.0 - smoothstep(0.0, 0.55, r)) * (0.08 + 0.1 * breath) * flat);
    gridGlow(display, any, float3(0.5, 0.8, 1.0), exp(-abs(r - 0.6) / 0.12) * 0.3 * flat);
  }
}

// The picked unit's ring: three arcs turning on a circle, a steady hex outline, a soft pool of light under its feet.
void gridPaintActive(inout float3 display, inout bool any, bool foe, float2 p, float d, float t, float pix, float flat) {
  const float3 color = foe ? float3(1.0, 0.52, 0.36) : float3(1.0, 0.93, 0.76);
  const float r = length(p);
  const float theta = atan2(p.y, p.x);
  const float arcs = smoothstep(0.08, 0.14, frac(theta / kTwoPi * 3.0 + t * (foe ? -0.12 : 0.12)));
  const float ring = gridLine(abs(r - 0.7), 0.04, pix) * arcs;
  const float outline = gridLine(abs(d - 0.96), 0.022, pix);
  gridPaint(display, any, color, max(ring, outline * 0.6) * flat);
  gridGlow(display, any, color, exp(-max(r - 0.7, 0.0) / 0.12) * 0.25 * flat);
  gridGlow(display, any, color, (1.0 - smoothstep(0.0, 0.7, r)) * 0.12 * flat);
}

// A target lock on a foe the picked unit can strike: red hex, hazard stripes, four turning brackets; brighter and
// faster under the cursor.
void gridPaintTarget(inout float3 display, inout bool any, bool hovered, float2 p, float2 xz, float d, float t, float pix, float flat) {
  const float3 red = float3(1.0, 0.24, 0.18), lock = float3(1.0, 0.6, 0.5);
  const float r = length(p);
  const float theta = atan2(p.y, p.x);
  const float beat = 0.5 + 0.5 * sin(t * (hovered ? 7.0 : 3.5));
  const float stripes = gridStripes(xz, t, pix);
  const float fillA = (hovered ? 0.3 : 0.16) + 0.1 * beat;
  gridPaint(display, any, red, (fillA + stripes * 0.08) * flat);
  const float outline = gridLine(abs(d - 0.955), 0.03, pix);
  const float brackets = smoothstep(0.42, 0.5, abs(frac(theta / kTwoPi * 4.0 + t * (hovered ? 0.35 : 0.12)) - 0.5) * 2.0);
  const float ring = gridLine(abs(r - 0.62), 0.04, pix) * brackets;
  gridPaint(display, any, lock, max(outline, ring) * (hovered ? 1.0 : 0.85) * flat);
  gridGlow(display, any, red, exp(-abs(d - 0.955) / 0.1) * (0.25 + 0.2 * beat) * flat);
}

// The cursor: a hex outline with bright corner brackets, breathing; ember where nothing can stand.
void gridPaintHover(inout float3 display, inout bool any, bool blocked, float2 p, float d, float t, float pix, float flat) {
  const float3 color = blocked ? float3(1.0, 0.42, 0.34) : float3(0.95, 0.98, 1.0);
  const float radius = 0.925 + 0.012 * sin(t * 5.0);
  const float theta = atan2(p.y, p.x);
  const float corner = abs(frac((theta - kTwoPi / 12.0) / (kTwoPi / 6.0)) - 0.5);  // 0 mid-edge, 0.5 on a corner
  const float bracket = smoothstep(0.27, 0.4, corner);
  const float outline = gridLine(abs(d - radius), 0.028, pix);
  gridPaint(display, any, color, outline * (0.3 + 0.7 * bracket) * flat);
  gridGlow(display, any, color, exp(-abs(d - radius) / 0.1) * 0.3 * flat);
  if (blocked) {
    // a cross through the hex
    const float cross = min(gridLine(abs(p.x - p.y) / 1.4142, 0.03, pix), 1.0 - smoothstep(0.3, 0.34, hexRadial(p)));
    const float cross2 = min(gridLine(abs(p.x + p.y) / 1.4142, 0.03, pix), 1.0 - smoothstep(0.3, 0.34, hexRadial(p)));
    gridPaint(display, any, color, max(cross, cross2) * 0.9 * flat);
  }
}

// Spell targets chosen so far: a gold hex ring with ticks at the corners.
void gridPaintChosen(inout float3 display, inout bool any, float2 p, float d, float t, float pix, float flat) {
  const float3 gold = float3(1.0, 0.85, 0.42);
  const float breath = 0.5 + 0.5 * sin(t * 4.0);
  const float ring = gridLine(abs(d - 0.7), 0.035, pix);
  gridPaint(display, any, gold, ring * flat);
  gridGlow(display, any, gold, exp(-abs(d - 0.7) / 0.12) * (0.25 + 0.15 * breath) * flat);
  gridGlow(display, any, gold, (1.0 - smoothstep(0.0, 0.7, d)) * 0.1 * flat);
}

// The look: returns true when it painted anything on this fragment.
bool gridLook001(inout float3 display, float2 xz, float flat, float t) {
  int2 cr;
  float2 p;
  if (!hexLocate(xz, push.gridCols, push.gridRows, cr, p)) return false;
  const uint2 cell = gridCellAt(cr);
  uint nb[6];
  bool anyNear = cell.x != 0 || cell.y != 0;
  [unroll] for (uint k = 0; k < 6; ++k) {
    const uint2 n = gridCellAt(gridNeighbour(cr, k));
    nb[k] = n.x | ((n.y & 255u) << 16);  // flags, and the route step for the route
    anyNear = anyNear || n.x != 0 || n.y != 0;
  }
  if (!anyNear) return false;
  const uint flags = cell.x;
  const float d = hexRadial(p);
  const float pix = max(length(fwidth(xz)) * 0.7, 1e-4);  // about a pixel, in hex radii
  bool any = false;
  uint nbFlags[6];
  uint nbSteps[6];
  [unroll] for (uint k = 0; k < 6; ++k) nbFlags[k] = nb[k] & 0xFFFFu, nbSteps[k] = nb[k] >> 16;

  // Movement (the party): ice, breathing boundary, lattice; fainter towards the limit of the movement.
  {
    const GridRegion r = gridRegion(flags, nbFlags, kGridReach, p);
    if (r.inside || r.edge < kNoEdge) {
      const float share = float((cell.y >> 8) & 255u) / 255.0;
      const float pulse = 0.8 + 0.25 * sin(t * 2.2);
      gridPaintRegion(display, any, r, float3(0.42, 0.78, 1.0), 0.16 * (1.0 - 0.35 * share), float3(0.74, 0.93, 1.0), 0.95, 0.28, pulse, 0.22, d, pix, flat);
    }
  }
  // A foe's movement: ember, hazard stripes.
  {
    const GridRegion r = gridRegion(flags, nbFlags, kGridReachFoe, p);
    if (r.inside || r.edge < kNoEdge) {
      const float pulse = 0.8 + 0.25 * sin(t * 2.2 + 1.0);
      gridPaintRegion(display, any, r, float3(1.0, 0.45, 0.28), 0.12, float3(1.0, 0.6, 0.42), 0.9, 0.25, pulse, 0.12, d, pix, flat);
      if (r.inside) gridPaint(display, any, float3(1.0, 0.45, 0.28), gridStripes(xz, t, pix) * 0.08 * flat);
    }
  }
  // Spell aim: violet region; the hovered aim's area bright.
  {
    const GridRegion r = gridRegion(flags, nbFlags, kGridSpellAim, p);
    if (r.inside || r.edge < kNoEdge) {
      const float pulse = 0.8 + 0.25 * sin(t * 2.2 + 2.0);
      gridPaintRegion(display, any, r, float3(0.72, 0.52, 1.0), 0.12, float3(0.84, 0.7, 1.0), 0.9, 0.25, pulse, 0.18, d, pix, flat);
    }
    const GridRegion a = gridRegion(flags, nbFlags, kGridSpellArea, p);
    if (a.inside || a.edge < kNoEdge) {
      const float breath = 0.5 + 0.5 * sin(t * 4.0);
      gridPaintRegion(display, any, a, float3(0.86, 0.7, 1.0), 0.22 + 0.08 * breath, float3(0.95, 0.88, 1.0), 0.95, 0.2, 1.0, 0.0, d, pix, flat);
    }
  }
  if (flags & kGridTarget) gridPaintTarget(display, any, (flags & kGridHover) != 0, p, xz, d, t, pix, flat);
  gridPaintRoute(display, any, flags, cell.y | 0u, nbSteps, p, d, t, pix, flat);
  if (flags & kGridSpellChosen) gridPaintChosen(display, any, p, d, t, pix, flat);
  if (flags & (kGridActive | kGridActiveFoe)) gridPaintActive(display, any, (flags & kGridActiveFoe) != 0, p, d, t, pix, flat);
  if ((flags & kGridHover) && !(flags & kGridTarget)) gridPaintHover(display, any, (flags & kGridHoverBlocked) != 0, p, d, t, pix, flat);
  return any;
}
