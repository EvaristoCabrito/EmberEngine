// 2D cards in the 3D scene: characters and decorations (photoreal 2D art, ARCHITECTURE §7.1). Each clip is
// a texture array (one layer per frame), straight alpha in sRGB BC7; colour is premultiplied after sampling
// and blended premultiplied, so edges never fringe.
//
// A card stands on its feet: either turning about the vertical axis to face the camera (units) or along a fixed
// axis (decorations stand square to the hex rows, as in Engine2); by default it leans back with the camera's tilt so
// it projects upright anywhere on screen (Push.lean; panel "Cards"). Lit cards go through the lighting contract
// (lighting.hlsli) with Engine2's card model as the baseline (src/units/spriteMaterial.ts): the shading
// normal faces out of the card tilted upward (y 0.55 against z 1), diffuse light wraps around (0.85), no
// specular, and a lit card never exceeds twice its painted colour. Painted flames glow (Engine2's emit
// mask). Cards receive the sun's shadow from the ground and 3D pieces, but not from other cards (their
// shadow map holds the card itself, which would shadow itself; ARCHITECTURE §7.1).
#include "lighting.hlsli"

// Matches ember::SpriteInstance (render/SpriteDraw.h). 80 bytes.
struct SpriteInstance {
  float3 origin;
  float width;
  float height;
  uint textureIndex;
  uint layer;
  uint flags;      // 1 mirrored, 2 lit, 4 unit drawn nearer in depth
  float2 uvScale;  // the trimmed picture's share of the texture (the rest is block padding)
  float2 right;    // horizontal axis (x, z); zero = face the camera
  float3 emitColor;
  float emitLevel;
  float3 emitAt;   // where the light sits (texture u, v); z, then emitScaleV: texture uv -> card heights
  float emitScaleV;
};

struct Push {
  float4x4 viewProjection;
  float3 cameraRight;
  uint samplerIndex;
  uint64_t instances;  // device address of SpriteInstance[]
  uint64_t frame;      // MeshFrame (camera, lights); 0 = no scene lighting
  float3 cameraForward;
  uint lean;           // 1: a card's up is the world's vertical projected onto the image plane (see VSMain)
};
[[vk::push_constant]] Push push;

static const float kTilt = 0.55;      // Engine2 TILT
static const float kWrap = 0.85;      // Engine2 WRAP
static const float kMaxLit = 2.0;     // Engine2 MAX_LIT
static const float kUnitFront = 1.2;  // Engine2 UNIT_FRONT
static const float kGlowRadius = 0.1; // Engine2 GLOW_RADIUS

struct VSOut {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
  nointerpolation uint instance : TEXCOORD1;
  float3 world : TEXCOORD2;
  float3 normal : TEXCOORD3;
};

SpriteInstance loadSprite(uint i) { return vk::RawBufferLoad<SpriteInstance>(push.instances + uint64_t(i) * 80); }

VSOut VSMain(uint vertex : SV_VertexID, uint instance : SV_InstanceID) {
  const float2 corners[6] = { float2(0, 0), float2(1, 0), float2(1, 1), float2(0, 0), float2(1, 1), float2(0, 1) };
  const SpriteInstance s = loadSprite(instance);
  const float2 c = corners[vertex];
  const float3 right = dot(s.right, s.right) > 0.0 ? float3(s.right.x, 0.0, s.right.y) : push.cameraRight;
  // A flat card (the 2D view) lies facing up with its top toward -z, the top of the screen. Otherwise, with
  // push.lean, the card's up is the world's vertical projected onto the image plane: the card leans back by the
  // camera's tilt with its feet in place, so it projects upright anywhere on screen (vertical cards converge
  // under a tilted camera: the user saw every decoration "crooked" at the screen's sides). Without it, vertical.
  const bool flat = (s.flags & 16u) != 0;
  float3 up = float3(0, 1, 0);
  if (flat) up = float3(0, 0, -1);
  else if (push.lean != 0) {
    const float3 v = float3(0, 1, 0) - push.cameraForward * push.cameraForward.y;
    if (dot(v, v) > 1e-6) up = normalize(v);
  }
  const float3 world = s.origin + right * ((c.x - 0.5) * s.width) + up * (c.y * s.height);
  // The card's facing (three.js: a plane turned by yaw faces (sin yaw, 0, cos yaw)), tilted upward.
  const float3 facing = float3(-right.z, 0.0, right.x);
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
  o.normal = flat ? float3(0.0, 1.0, 0.0) : normalize(facing + float3(0.0, kTilt, 0.0));
  return o;
}

float4 PSMain(VSOut i) : SV_Target0 {
  const SpriteInstance s = loadSprite(i.instance);
  float4 c = gTextureArrays[NonUniformResourceIndex(s.textureIndex)].Sample(gSamplers[push.samplerIndex], float3(i.uv, s.layer));
  clip(c.a - 1.0 / 255.0);
  float3 color = c.rgb;
  if ((s.flags & 2u) && push.frame != 0) {
    const MeshFrame f = loadFrame(push.frame);
    Surface surface = defaultSurface(i.world, i.normal, c.rgb);
    surface.wrap = kWrap;
    surface.specular = 0.0;
    // Shadows from the ground and 3D pieces, not from the cards' own map (a flat card would shadow itself).
    surface.receivesCardShadow = 0.0;
    if (s.flags & 4u) surface.twoSidedLocal = 1.0;  // units (Engine2: pointLightsOnBothSides)
    // The whole card takes the fog of war where it stands (Engine2's DecorLayer.shade).
    const Sight sight = tacticalSight(f, s.origin);
    if (f.debugView == kDebugFog) return float4(float3(1.0 - sight.color, sight.local, 1.0) * c.a, c.a);
    if (f.debugView == kDebugFogStates) return float4(fogStateColor(fogAlpha(f, s.origin)) * c.a, c.a);
    color = min(shadeInSight(f, surface, sight), c.rgb * kMaxLit * f.litCap);
    if (s.emitLevel > 0.0) {
      // Engine2's flame mask: bright, saturated pixels close to the light's colour glow, and so does the lit
      // glass just around where the light sits on the card.
      // A unit painted as glowing (emitAt.z < 0: the Cobalt Blue Deer): its whole body glows, antlers and white coat alike (the
      // user, 2026-10-18: "o corpo dele tem que brilhar"); only its dark pixels (eyes, hooves' shade) stay unlit. No glass.
      const bool unitGlow = s.emitAt.z < 0.0;
      const float3 lo = float3(0.45, 0.4, 0.9), hi = float3(0.8, 0.7, 0.97);
      const float eMax = max(c.r, max(c.g, c.b));
      const float eSat = (eMax - min(c.r, min(c.g, c.b))) / max(eMax, 1e-4);
      const float eSim = dot(normalize(c.rgb + 1e-4), normalize(s.emitColor));
      float mask = unitGlow ? smoothstep(0.15, 0.45, eMax)
                            : smoothstep(lo.x, hi.x, eMax) * smoothstep(lo.y, hi.y, eSat) * smoothstep(lo.z, hi.z, eSim);
      if (!unitGlow) {
        const float near = 1.0 - smoothstep(kGlowRadius * 0.6, kGlowRadius, length((i.uv - s.emitAt.xy) * float2(s.emitAt.z, s.emitScaleV)));
        mask = max(mask, smoothstep(0.12, 0.4, eMax) * smoothstep(0.08, 0.3, eSat) * smoothstep(0.75, 0.9, eSim) * near * 0.75);
      }
      color += c.rgb * mask * s.emitLevel * f.exposure;
    }
    color = applyVolume(f, color * sight.color, i.world);
  }
  return float4(color * c.a, c.a);
}

// The cards' depth, written after every card is drawn (so it never changes how cards blend over each other):
// alpha-tested at half coverage like the card shadows. Passes after the cards (the fog bank, effects) then stop at
// a character instead of reaching the ground behind it: the fog of war behind a unit no longer paints over it.
// Writes no colour (premultiplied blend of zero).
float4 PSDepth(VSOut i) : SV_Target0 {
  const SpriteInstance s = loadSprite(i.instance);
  const float a = gTextureArrays[NonUniformResourceIndex(s.textureIndex)].Sample(gSamplers[push.samplerIndex], float3(i.uv, s.layer)).a;
  clip(a - 0.5);
  return float4(0.0, 0.0, 0.0, 0.0);
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
  // push.lean bit 2: the spell light's pass (Renderer setLocalShadow): a camera-facing card turns to face that light instead.
  const bool local = (push.lean & 2u) != 0;
  float3 right = float3(s.right.x, 0.0, s.right.y);
  if (dot(s.right, s.right) == 0.0) {
    const float2 toward = local ? (f.localShadowPosition - s.origin).xz : f.sunDirection.xz;
    right = dot(toward, toward) > 1e-6 ? normalize(float3(toward.y, 0.0, -toward.x)) : float3(1, 0, 0);
  }
  const float3 world = s.origin + right * ((c.x - 0.5) * s.width) + ((s.flags & 16u) ? float3(0, 0, -1) : float3(0, 1, 0)) * (c.y * s.height);
  ShadowOut o;
  // Non-casters fall off the map; so does a card standing on never-seen ground (R7: it reveals nothing).
  const bool casts = (s.flags & 8u) && !neverSeen(f, s.origin);
  o.position = casts ? mul(float4(world, 1.0), local ? f.localShadowMatrix : f.shadowMatrix) : float4(0, 0, -1, 1);
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
