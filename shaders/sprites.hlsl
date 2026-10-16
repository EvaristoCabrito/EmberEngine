// 2D cards in the 3D scene: characters and decorations (photoreal 2D art, ARCHITECTURE §7.1). Each clip is
// a texture array (one layer per frame), straight alpha in sRGB BC7; colour is premultiplied after sampling
// and blended premultiplied, so edges never fringe.
//
// A card stands upright: either turning about the vertical axis to face the camera (units) or along a fixed
// axis (decorations stand square to the hex rows, as in Engine2). Lit cards go through the lighting contract
// (lighting.hlsli) with Engine2's card model as the baseline (src/units/spriteMaterial.ts): the shading
// normal faces out of the card tilted upward (y 0.55 against z 1), diffuse light wraps around (0.85), no
// specular, and a lit card never exceeds twice its painted colour. Painted flames glow (Engine2's emit
// mask). Cards receive the sun's shadow from the ground and 3D pieces, but not from other cards (their
// shadow map holds the card itself, which would shadow itself; ARCHITECTURE §7.1).
//
// Relief (SpriteFormat.h: a second array per clip, R G = tangent-space normal, B = occlusion, A = height, 1 = nearest;
// tools/scripts/sprites/relief.py): with it a card is a body of thickness Push.thickness x its width standing in
// front of its plane. Each feature is a switch (SpriteDraw.h kSpriteFx*, Dev Controls "Sprites 3D"):
//   normals      the relief's normal lights every pixel on its own (shoulders toward the sun, lamps wrapping)
//   depth        each pixel writes its own depth (PSMainDepth) and is lit where it stands: the body intersects
//                fences, grass and other cards instead of the plane doing it
//   parallax     parallax occlusion mapping along the view: nearer parts shift with the camera
//   self-shadow  the sun marched through the height field: a raised arm shadows the chest
//   occlusion    the baked occlusion dims the indirect light
//   specular     a specular lobe (roughness 0.5) on the relief
// The plane is the body's back (height 0); the height field reaches out toward the viewer, so feet stay put.
#include "lighting.hlsli"

// Matches ember::SpriteInstance (render/SpriteDraw.h). 96 bytes.
struct SpriteInstance {
  float3 origin;
  float width;
  float height;
  uint textureIndex;
  uint layer;
  uint flags;      // 1 mirrored, 2 lit, 4 unit drawn nearer in depth, 8 casts, 16 flat
  float2 uvScale;  // the trimmed picture's share of the texture (the rest is block padding)
  float2 right;    // horizontal axis (x, z); zero = face the camera
  float3 emitColor;
  float emitLevel;
  float3 emitAt;   // where the light sits (texture u, v); z, then emitScaleV: texture uv -> card heights
  float emitScaleV;
  uint reliefIndex;  // the relief array; kNoTexture = none
  uint3 reserved;
};

struct Push {
  float4x4 viewProjection;
  float3 cameraRight;
  uint samplerIndex;
  uint64_t instances;  // device address of SpriteInstance[]
  uint64_t frame;      // MeshFrame (camera, lights); 0 = no scene lighting
  uint features;       // kSpriteFx* switched on
  float thickness;     // the body's thickness as a share of the card's width
  float parallax;      // reserved (1)
  uint pad;
};
[[vk::push_constant]] Push push;

static const float kTilt = 0.55;      // Engine2 TILT
static const float kWrap = 0.85;      // Engine2 WRAP
static const float kMaxLit = 2.0;     // Engine2 MAX_LIT
static const float kUnitFront = 1.2;  // Engine2 UNIT_FRONT
static const float kGlowRadius = 0.1; // Engine2 GLOW_RADIUS

static const uint kFxNormals = 1u, kFxDepth = 2u, kFxParallax = 4u, kFxSelfShadow = 8u, kFxOcclusion = 16u, kFxSpecular = 32u,
                  kFxDecor = 64u;

struct VSOut {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
  nointerpolation uint instance : TEXCOORD1;
  float3 world : TEXCOORD2;
  float3 normal : TEXCOORD3;
  float3 drawn : TEXCOORD4;  // where the vertex was rasterised (a unit slides toward the camera in depth)
};

SpriteInstance loadSprite(uint i) { return vk::RawBufferLoad<SpriteInstance>(push.instances + uint64_t(i) * 96); }

// The card's axes in the world: width along `right`, height along `up`, `facing` out of the picture toward the viewer.
struct CardBasis {
  float3 right, up, facing;
};
CardBasis cardBasis(SpriteInstance s) {
  CardBasis b;
  b.right = dot(s.right, s.right) > 0.0 ? float3(s.right.x, 0.0, s.right.y) : push.cameraRight;
  const bool flat = (s.flags & 16u) != 0;
  b.up = flat ? float3(0, 0, -1) : float3(0, 1, 0);
  // The card's facing (three.js: a plane turned by yaw faces (sin yaw, 0, cos yaw)); a flat card faces up.
  b.facing = flat ? float3(0, 1, 0) : float3(-b.right.z, 0.0, b.right.x);
  return b;
}

VSOut VSMain(uint vertex : SV_VertexID, uint instance : SV_InstanceID) {
  const float2 corners[6] = { float2(0, 0), float2(1, 0), float2(1, 1), float2(0, 0), float2(1, 1), float2(0, 1) };
  const SpriteInstance s = loadSprite(instance);
  const float2 c = corners[vertex];
  const CardBasis b = cardBasis(s);
  const bool flat = (s.flags & 16u) != 0;
  const float3 world = s.origin + b.right * ((c.x - 0.5) * s.width) + b.up * (c.y * s.height);
  VSOut o;
  float3 drawn = world;
  if ((s.flags & 4u) && push.frame != 0) {
    // A unit wins against a prop card standing through its own hex: each vertex slides toward the camera
    // along its own line of sight, landing on the same pixel with a nearer depth.
    const float3 eye = loadFrame(push.frame).cameraPosition;
    const float3 d = world - eye;
    drawn = eye + d * (1.0 - min(kUnitFront / max(length(d), 1e-3), 0.9));
  }
  o.position = mul(float4(drawn, 1.0), push.viewProjection);  // -Zpr: glm's column-major matrix reads as its transpose
  // A card standing on never-seen ground is not drawn: even black, its outline over lit ground would show (R7).
  if (push.frame != 0 && neverSeen(loadFrame(push.frame), s.origin)) o.position = float4(0, 0, -1, 1);
  const float u = (s.flags & 1u) ? 1.0 - c.x : c.x;
  o.uv = float2(u, 1.0 - c.y) * s.uvScale;
  o.instance = instance;
  o.world = world;
  o.drawn = drawn;
  o.normal = flat ? float3(0.0, 1.0, 0.0) : normalize(b.facing + float3(0.0, kTilt, 0.0));
  return o;
}

float4 sampleRelief(SpriteInstance s, float2 uv) {
  return gTextureArrays[NonUniformResourceIndex(s.reliefIndex)].Sample(gSamplers[push.samplerIndex], float3(uv, s.layer));
}

// The card's colour at this pixel; `depth` is its own depth when the depth feature is on (else the plane's).
float4 cardColor(VSOut i, out float depth) {
  depth = i.position.z;
  const SpriteInstance s = loadSprite(i.instance);
  // Which relief features apply here: none without a relief array or a scene; decor cards only when asked.
  uint fx = (s.reliefIndex != kNoTexture && push.frame != 0) ? push.features : 0u;
  if ((s.flags & 4u) == 0 && (fx & kFxDecor) == 0) fx = 0u;
  const CardBasis b = cardBasis(s);
  const float mirrorSign = (s.flags & 1u) ? -1.0 : 1.0;
  // Texture uv per world unit along the card's axes (v runs down the picture).
  const float2 uvPerWorld = float2(mirrorSign * s.uvScale.x / max(s.width, 1e-4), -s.uvScale.y / max(s.height, 1e-4));
  const float thickness = push.thickness * s.width;

  float2 uv = i.uv;
  float3 view = 0.0;
  float3 viewT = 0.0;
  MeshFrame f;
  if (push.frame != 0) {
    f = loadFrame(push.frame);
    view = normalize(f.cameraPosition - i.world);
    viewT = float3(dot(view, b.right), dot(view, b.up), dot(view, b.facing));
  }
  if ((fx & kFxParallax) && viewT.z > 0.05) {
    // Parallax occlusion mapping: the eye's ray enters the body's front (height 1) and sinks until it meets the
    // height field; the plane (height 0) keeps its place, nearer parts slide along the view.
    const float2 slide = viewT.xy / viewT.z * thickness * uvPerWorld;  // uv shift per unit of height
    const uint steps = uint(lerp(14.0, 6.0, viewT.z));
    const float layer = 1.0 / float(steps);
    float h = 1.0;
    float prevH = 1.0, prevSurface = 0.0;
    float2 prevUv = uv + slide;
    float surface = sampleRelief(s, prevUv).a;
    bool hit = false;
    for (uint k = 0; k < steps; ++k) {
      prevH = h;
      prevSurface = surface;
      prevUv = uv + h * slide;
      h -= layer;
      const float2 at = uv + h * slide;
      surface = sampleRelief(s, at).a;
      if (surface >= h) {
        // Where the ray crossed between the two samples.
        const float a = prevH - prevSurface, c = surface - h;
        const float t = a / max(a + c, 1e-4);
        uv = lerp(prevUv, at, t);
        hit = true;
        break;
      }
    }
    if (!hit) uv = uv;  // reached the plane: the plane's own pixel
  }

  float4 c = gTextureArrays[NonUniformResourceIndex(s.textureIndex)].Sample(gSamplers[push.samplerIndex], float3(uv, s.layer));
  clip(c.a - 1.0 / 255.0);
  float4 relief = fx != 0u ? sampleRelief(s, uv) : float4(0.5, 0.5, 1.0, 0.0);
  const float height = relief.a;
  float3 position = i.world;
  if (fx & kFxDepth) {
    // The pixel stands height x thickness in front of the plane: its own depth, its own place for the lights.
    const float3 at = i.drawn + b.facing * (height * thickness);
    const float4 clipPos = mul(float4(at, 1.0), push.viewProjection);
    depth = clipPos.z / max(clipPos.w, 1e-6);
    position = i.world + b.facing * (height * thickness);
  }

  float3 color = c.rgb;
  if ((s.flags & 2u) && push.frame != 0) {
    float3 normal = i.normal;
    if (fx & kFxNormals) {
      float3 n = float3(relief.xy * 2.0 - 1.0, 0.0);
      n.x *= mirrorSign;
      n.z = sqrt(saturate(1.0 - dot(n.xy, n.xy)));
      normal = normalize(n.x * b.right + n.y * b.up + n.z * i.normal);
    }
    Surface surface = defaultSurface(position, normal, c.rgb);
    surface.geometricNormal = i.normal;
    surface.wrap = kWrap;
    surface.specular = 0.0;
    if (fx & kFxSpecular) {
      surface.specular = 0.5;
      surface.roughness = 0.5;
    }
    if (fx & kFxOcclusion) surface.occlusion = relief.b;
    // Shadows from the ground and 3D pieces, not from the cards' own map (a flat card would shadow itself).
    surface.receivesCardShadow = 0.0;
    if (s.flags & 4u) surface.twoSidedLocal = 1.0;  // units (Engine2: pointLightsOnBothSides)
    if (fx & kFxSelfShadow) {
      // The sun marched through the height field from this pixel: where the field rises over the ray, the sun is hidden.
      const float3 l = f.sunDirection;
      const float3 lt = float3(dot(l, b.right), dot(l, b.up), dot(l, b.facing));
      if (lt.z > 0.02) {
        const float maxWorld = min((1.0 - height) * thickness / lt.z, 0.75 * s.width);
        const uint steps = 8;
        float occluded = 0.0;
        [unroll] for (uint k = 1; k <= steps; ++k) {
          const float t = maxWorld * float(k) / float(steps);
          const float rayH = height + lt.z * t / max(thickness, 1e-4) + 0.02;
          const float2 at = uv + lt.xy * t * uvPerWorld;
          const float4 r = sampleRelief(s, at);
          const float fieldH = r.a;
          occluded = max(occluded, (fieldH - rayH) * 6.0);
        }
        surface.sunVisibility = 1.0 - saturate(occluded);
      }
    }
    // The whole card takes the fog of war where it stands (Engine2's DecorLayer.shade).
    const Sight sight = tacticalSight(f, s.origin);
    if (f.debugView == kDebugFog) return float4(float3(1.0 - sight.color, sight.local, 1.0) * c.a, c.a);
    if (f.debugView == kDebugFogStates) return float4(fogStateColor(fogAlpha(f, s.origin)) * c.a, c.a);
    if (f.debugView == 1u) return float4((i.normal * 0.5 + 0.5) * c.a, c.a);
    if (f.debugView == 2u) return float4((normal * 0.5 + 0.5) * c.a, c.a);
    color = min(shadeInSight(f, surface, sight), c.rgb * kMaxLit * f.litCap);
    if (s.emitLevel > 0.0) {
      // Engine2's flame mask: bright, saturated pixels close to the light's colour glow, and so does the lit
      // glass just around where the light sits on the card.
      const float3 lo = float3(0.45, 0.4, 0.9), hi = float3(0.8, 0.7, 0.97);
      const float eMax = max(c.r, max(c.g, c.b));
      const float eSat = (eMax - min(c.r, min(c.g, c.b))) / max(eMax, 1e-4);
      const float eSim = dot(normalize(c.rgb + 1e-4), normalize(s.emitColor));
      float mask = smoothstep(lo.x, hi.x, eMax) * smoothstep(lo.y, hi.y, eSat) * smoothstep(lo.z, hi.z, eSim);
      const float near = 1.0 - smoothstep(kGlowRadius * 0.6, kGlowRadius, length((uv - s.emitAt.xy) * float2(s.emitAt.z, s.emitScaleV)));
      mask = max(mask, smoothstep(0.12, 0.4, eMax) * smoothstep(0.08, 0.3, eSat) * smoothstep(0.75, 0.9, eSim) * near * 0.75);
      color += c.rgb * mask * s.emitLevel * f.exposure;
    }
    color = applyVolume(f, color * sight.color, position);
  }
  return float4(color * c.a, c.a);
}

float4 PSMain(VSOut i) : SV_Target0 {
  float depth;
  return cardColor(i, depth);
}

// The same card writing its own depth (the depth feature): its pipeline writes depth, LESS_OR_EQUAL in reversed Z.
float4 PSMainDepth(VSOut i, out float depth : SV_Depth) : SV_Target0 {
  return cardColor(i, depth);
}

// Cards in the sun's shadow (the card shadow map, redrawn every frame): alpha-tested at half coverage. A card
// that turns to face the camera (a unit) casts facing the sun instead, so moving the camera never moves its
// shadow (ARCHITECTURE §7.1); fixed cards cast as they stand.
struct ShadowOut {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
  nointerpolation uint instance : TEXCOORD1;
};

ShadowOut VSShadow(uint vertex : SV_VertexID, uint instance : SV_InstanceID) {
  const float2 corners[6] = { float2(0, 0), float2(1, 0), float2(1, 1), float2(0, 0), float2(1, 1), float2(0, 1) };
  const SpriteInstance s = loadSprite(instance);
  const MeshFrame f = loadFrame(push.frame);
  const float2 c = corners[vertex];
  float3 right = float3(s.right.x, 0.0, s.right.y);
  if (dot(s.right, s.right) == 0.0) {
    const float2 sun = f.sunDirection.xz;
    right = dot(sun, sun) > 1e-6 ? normalize(float3(sun.y, 0.0, -sun.x)) : float3(1, 0, 0);
  }
  const float3 world = s.origin + right * ((c.x - 0.5) * s.width) + ((s.flags & 16u) ? float3(0, 0, -1) : float3(0, 1, 0)) * (c.y * s.height);
  ShadowOut o;
  // Non-casters fall off the map; so does a card standing on never-seen ground (R7: it reveals nothing).
  const bool casts = (s.flags & 8u) && !neverSeen(f, s.origin);
  o.position = casts ? mul(float4(world, 1.0), f.shadowMatrix) : float4(0, 0, -1, 1);
  const float u = (s.flags & 1u) ? 1.0 - c.x : c.x;
  o.uv = float2(u, 1.0 - c.y) * s.uvScale;
  o.instance = instance;
  return o;
}

void PSShadow(ShadowOut i) {
  const SpriteInstance s = loadSprite(i.instance);
  const float a = gTextureArrays[NonUniformResourceIndex(s.textureIndex)].Sample(gSamplers[push.samplerIndex], float3(i.uv, s.layer)).a;
  clip(a - 0.5);
}
