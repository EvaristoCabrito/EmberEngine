// The GPU-driven mesh scene: layouts shared with engine/render/MeshFormat.h, MeshGpu.h and the
// renderer's MeshFrame (scalar packing; every record is read through its device address).
#ifndef EMBER_SCENE_HLSLI
#define EMBER_SCENE_HLSLI

#include "common.hlsli"

struct MeshVertex {  // 32 bytes
  float3 position;
  uint normal;   // octahedral, 2 x snorm16
  uint tangent;  // octahedral, 2 x snorm16; lowest bit of y = bitangent sign (1 = negative)
  float2 uv;
  uint color;    // RGBA8, RGB sRGB-encoded
};

struct MeshCluster {  // 48 bytes
  float3 center;
  float radius;
  uint cone;            // axis x, y, z, cutoff: 4 x snorm8
  uint vertexOffset;    // into clusterVertices
  uint triangleOffset;  // bytes into triangles
  uint countsMaterial;  // vertexCount (8) | triangleCount (8) | material (16)
  int refinedGroup;
  uint group;
  uint2 reserved;
};

struct MeshGroup {  // 32 bytes
  float3 center;
  float radius;
  float error;
  uint depth;
  uint firstCluster;
  uint clusterCount;
};

struct MeshNode {  // 32 bytes
  float3 center;
  float radius;
  float error;
  int group;
  uint childOffset;
  uint childCount;
};

struct MeshGpu {  // 80 bytes
  uint64_t vertices;
  uint64_t clusterVertices;
  uint64_t triangles;
  uint64_t clusters;
  uint64_t groups;
  uint64_t nodes;
  uint levelCount;
  uint materialBase;
  uint clusterCount;
  uint ready;
  float4 sphere;
};

struct InstanceGpu {  // 64 bytes
  float4 rows[3];
  uint mesh;
  float scale;
  uint flags;
  uint pad;
};

struct MaterialGpu {  // 80 bytes
  float4 baseColor;
  float3 emissive;
  float metallic;
  float roughness;
  float normalScale;
  float occlusionStrength;
  float alphaCutoff;
  uint flags;
  uint baseColorTexture;
  uint normalTexture;
  uint ormTexture;
  uint emissiveTexture;
  uint3 pad;
};

static const uint kNoTexture = 0xFFFFFFFFu;
static const uint kMaterialAlphaMask = 1u;
static const uint kMaterialDoubleSided = 2u;

// Matches ember::MeshFrame (Renderer.cpp): everything the mesh passes know about this frame.
struct MeshFrame {
  float4x4 viewProjection;
  float4 frustum[5];        // world-space planes (xyz normal pointing inside, w distance); no far plane
  float3 cameraPosition;
  float lodScale;           // projection[1][1] / 2 * screen height / pixel error threshold
  float2 screenSize;
  float zNear;
  uint instanceCount;
  uint meshCount;
  uint maxQueue;
  uint maxClusters;
  uint materialSampler;
  uint workBuffer;          // gBuffers index: counters, indirect arguments, queues, cluster list
  uint64_t meshes;
  uint64_t materials;
  uint64_t instances;
  float3 sunDirection;      // towards the sun
  float sunIntensity;
  float3 sunColor;
  float ambientIntensity;
  float3 skyColor;
  float exposure;           // already multiplied into sun and ambient; emissive surfaces multiply it in
  float3 groundColor;
  uint pad1;
  // Occlusion culling (two-phase): last frame's verdict per instance, this frame's depth pyramid.
  float4x4 view;
  float projectionX;        // projection[0][0]
  float projectionY;        // |projection[1][1]|
  uint hizTexture;          // R32F pyramid of the early pass's depth (farthest depth per texel)
  uint hizSampler;          // min-reduction sampler
  float2 hizSize;           // pyramid level 0 size in texels
  uint instanceVisibility;  // gBuffers index: one uint per instance, 1 = visible last frame
  uint pad2;
  // Early-pass cluster occlusion: last frame's camera and pyramid (still in the pyramid image when
  // the early pass runs). Clusters it hides are re-tested in the late pass against this frame's.
  float4x4 previousView;
  float previousProjectionX;
  float previousProjectionY;
  uint earlyOcclusion;  // 0 when last frame's pyramid is not usable (first frame, resize)
  uint costSplit;       // measurement only: 0 normal, 1 task shaders only (no mesh work), 2 mesh shaders
                        // run but every triangle is culled (no rasterisation)
  uint debugView;       // 0 shaded, 1 geometric normal, 2 shading normal (normal map applied): xyz * 0.5 + 0.5
  uint shadowPass;      // 1: this frame record drives the meshes' pass from the sun (frustum only, no occlusion)
  uint cardShadowMap;   // bindless depth texture: the cards' shadows this frame; kNoTexture = none
  uint pad6;
  // Sun shadow (lighting.hlsli sunShadow): one orthographic map fitted to the board.
  float4x4 shadowMatrix;     // world -> light clip: x, y in -1..1, z in 0..1 (0 = nearest the sun)
  uint shadowMap;            // bindless depth texture; kNoTexture = no shadows
  uint shadowSampler;        // comparison sampler (lit when depth <= stored)
  float shadowSize;          // texels per side
  float shadowTexelWorld;    // world size of one texel
  float shadowFilterTexels;  // radius of the soft filter, in texels
  uint lightingLayers;       // lighting.hlsli kLayer*: layers switched on
  float time;                // seconds (animated materials: water ripples)
  uint pad8;
  // Local lights (lights.hlsl): the list and its clusters.
  uint64_t lights;           // LocalLight[lightCount] (device address)
  uint lightCount;
  uint clusterBuffer;        // gBuffers index: per cluster, a count then up to kClusterMaxLights indices
  float clusterNear;         // view depth of the first slice's far side... see clusterSliceDepth
  float clusterFar;
  uint64_t clusters;         // the same cluster buffer by device address: read-only (fragment shaders may not
                             // bind a writable storage buffer without fragmentStoresAndAtomics)
  // Fog of war (R7, world/FogOfWar.h): the darkness mask over the board.
  float4 fogRect;            // world x0, z0, 1 / width, 1 / depth
  uint fogMask;              // RGBA16F: r darkness (0 in sight, 0.6 remembered, 1 never seen), then the volumetric
                             // fog's boundary, ground, valley (FogMask); kNoTexture = no fog of war
  uint fogSampler;
  uint2 pad9;
  // Volumetric fog (volume.hlsl): the integrated froxel volume surfaces read at their depth.
  uint volume;               // sampled 3D texture (scattered light, transmittance); kNoTexture = none
  uint volumeSampler;        // linear, clamped
  float volumeNear;          // view depth of the first slice boundary
  float volumeFar;           // ... and of the last
  // The atmosphere (atmosphere.hlsli): sky, ambient light, aerial perspective. 0 = none (the hemisphere).
  uint64_t atmosphere;       // AtmosphereFrame
  uint2 pad10;
};

// Matches ember::AtmosphereFrame (render/Atmosphere.h). 240 bytes.
struct AtmosphereFrame {
  float4x4 inverseViewProjection;
  float3 rayleighScattering;  // per km at sea level
  float rayleighScaleHeight;  // km
  float3 mieScattering;
  float mieScaleHeight;
  float3 mieExtinction;
  float mieG;                 // Cornette-Shanks asymmetry
  float3 ozoneAbsorption;
  float ozoneCenter;          // km: a tent of ozone peaking here ...
  float3 groundAlbedo;
  float ozoneWidth;           // ... this wide
  float3 sunIlluminance;      // the key light outside the air (colour x strength, before exposure)
  float bottomRadius;         // km
  float3 sunDirection;        // towards the sun or moon
  float topRadius;
  float viewHeight;           // km from the planet's centre (the board's ground)
  float kmPerUnit;            // aerial perspective: kilometres per world unit (artistic scale)
  float aerialMaxKm;          // the aerial-perspective volume reaches this far
  float ambientTarget;        // luminance of the ambient light falling on an upward surface (exposure in)
  uint transmittance;         // sampled / storage indices of the tables
  uint transmittanceStorage;
  uint multiScattering;
  uint multiScatteringStorage;
  uint skyView;
  uint skyViewStorage;
  uint aerial;
  uint aerialStorage;
  uint64_t ambient;           // SH9 irradiance (9 x float4), written by CSAmbient
  uint linearSampler;
  float skyScale;             // the sky behind the board: brightness relative to the lit air (dark at night)
  float3 moonDirection;       // the moon's disc (its phase art) in the sky, towards it
  uint moonTexture;           // the phase art (sRGB, alpha); kNoTextureIndex = no disc
  float moonTanRadius;        // tan of the disc's angular radius
  float moonGlow;             // the disc's radiance: the art x this (before exposure)
  float stars;                // star field strength (1 at night)
  uint sunDisc;               // 1: the key light is the sun (its small disc is drawn; stars come out as it sets)
  uint starLook;              // the star field's saved look (looks/stars/README.md)
  uint pad0;
};

AtmosphereFrame loadAtmosphere(MeshFrame f) { return vk::RawBufferLoad<AtmosphereFrame>(f.atmosphere); }


// One local light (Engine2's CarriedLight: a three.js point light). 32 bytes.
struct LocalLight {
  float3 position;
  float range;     // influence ends here (three.js cutoff distance)
  float3 color;    // linear radiometric colour x intensity (before exposure)
  float decay;     // distance falloff exponent (Engine2: 1.25)
};

// Darkness of the fog of war over a world point (0 in sight .. 0.6 remembered .. 1 never seen); outside the
// mask = never seen.
float fogAlpha(MeshFrame f, float3 position) {
  if (f.fogMask == kNoTexture) return 0.0;
  const float2 uv = (position.xz - f.fogRect.xy) * f.fogRect.zw;
  if (any(uv < 0.0) || any(uv > 1.0)) return 1.0;
  return gTextures[NonUniformResourceIndex(f.fogMask)].SampleLevel(gSamplers[f.fogSampler], uv, 0).r;
}
// Never seen: nothing there is drawn lit, and nothing there casts a shadow.
bool neverSeen(MeshFrame f, float3 position) { return fogAlpha(f, position) >= 254.5 / 255.0; }

// Froxels (volume.hlsl): screen tiles x logarithmic depth slices.
static const uint kFroxelX = 160, kFroxelY = 90, kFroxelZ = 64;
// View depth at slice coordinate w (0..1), and back.
float froxelDepth(MeshFrame f, float w) { return f.volumeNear * pow(f.volumeFar / f.volumeNear, w); }
float froxelSlice(MeshFrame f, float depth) { return log(max(depth, 1e-4) / f.volumeNear) / log(f.volumeFar / f.volumeNear); }

static const uint kClusterX = 16, kClusterY = 9, kClusterZ = 24;
static const uint kClusterCount = kClusterX * kClusterY * kClusterZ;
static const uint kClusterMaxLights = 63;

LocalLight loadLight(MeshFrame f, uint i) { return vk::RawBufferLoad<LocalLight>(f.lights + uint64_t(i) * 32); }
uint clusterOffset(uint cluster) { return cluster * (kClusterMaxLights + 1) * 4; }
// View depth where slice `i` begins: logarithmic between clusterNear and clusterFar (slice 0 starts at 0).
float clusterSliceDepth(MeshFrame f, uint i) {
  if (i == 0) return 0.0;
  return f.clusterNear * pow(f.clusterFar / f.clusterNear, float(i - 1) / float(kClusterZ - 1));
}
uint clusterSlice(MeshFrame f, float depth) {
  if (depth < f.clusterNear) return 0;
  return min(kClusterZ - 1, 1 + uint(log(depth / f.clusterNear) / log(f.clusterFar / f.clusterNear) * float(kClusterZ - 1)));
}

// The work buffer: counters, indirect arguments, then the queues.
static const uint kCounterQueue0 = 0;
static const uint kCounterQueue1 = 1;
static const uint kCounterClusters = 2;
static const uint kCounterQueueOverflow = 3;
static const uint kCounterClusterOverflow = 4;
static const uint kCounterInstances = 5;
static const uint kCounterEarlyClusters = 6;
static const uint kCounterOccludedInstances = 7;
static const uint kCounterRetest = 8;     // early-pass clusters hidden by last frame's pyramid
static const uint kCounterTriangles = 9;  // triangles sent to the rasteriser (statistics)
static const uint kTraverseArgs = 64;   // byte offsets
static const uint kDrawArgs = 80;
static const uint kQueuesOffset = 128;

// Byte offsets in the work buffer.
uint queueOffset(MeshFrame f, uint queue) { return kQueuesOffset + queue * f.maxQueue * 8; }
uint clusterListOffset(MeshFrame f) { return kQueuesOffset + 2 * f.maxQueue * 8; }
uint retestListOffset(MeshFrame f) { return clusterListOffset(f) + f.maxClusters * 8; }
RWByteAddressBuffer work(MeshFrame f) { return gBuffers[NonUniformResourceIndex(f.workBuffer)]; }

MeshFrame loadFrame(uint64_t address) { return vk::RawBufferLoad<MeshFrame>(address); }
InstanceGpu loadInstance(MeshFrame f, uint i) { return vk::RawBufferLoad<InstanceGpu>(f.instances + uint64_t(i) * 64); }
MeshGpu loadMesh(MeshFrame f, uint i) { return vk::RawBufferLoad<MeshGpu>(f.meshes + uint64_t(i) * 80); }
MaterialGpu loadMaterial(MeshFrame f, uint i) { return vk::RawBufferLoad<MaterialGpu>(f.materials + uint64_t(i) * 80); }
MeshCluster loadCluster(MeshGpu m, uint i) { return vk::RawBufferLoad<MeshCluster>(m.clusters + uint64_t(i) * 48); }
MeshGroup loadGroup(MeshGpu m, uint i) { return vk::RawBufferLoad<MeshGroup>(m.groups + uint64_t(i) * 32); }
MeshNode loadNode(MeshGpu m, uint i) { return vk::RawBufferLoad<MeshNode>(m.nodes + uint64_t(i) * 32); }
MeshVertex loadVertex(MeshGpu m, uint i) { return vk::RawBufferLoad<MeshVertex>(m.vertices + uint64_t(i) * 32); }
uint loadClusterVertex(MeshGpu m, uint i) { return vk::RawBufferLoad<uint>(m.clusterVertices + uint64_t(i) * 4); }

// Local indices of one cluster triangle (3 bytes at triangleOffset + 3 * t).
uint3 loadTriangle(MeshGpu m, MeshCluster c, uint t) {
  const uint64_t byteAddress = m.triangles + c.triangleOffset + t * 3;
  const uint64_t aligned = byteAddress & ~uint64_t(3);
  const uint shift = uint(byteAddress - aligned) * 8;
  const uint2 words = vk::RawBufferLoad<uint2>(aligned, 4);
  const uint packed = shift == 0 ? words.x : (words.x >> shift) | (words.y << (32 - shift));
  return uint3(packed & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF);
}

uint clusterVertexCount(MeshCluster c) { return c.countsMaterial & 0xFF; }
uint clusterTriangleCount(MeshCluster c) { return (c.countsMaterial >> 8) & 0xFF; }
uint clusterMaterial(MeshCluster c) { return c.countsMaterial >> 16; }

float3 transformPoint(InstanceGpu inst, float3 p) {
  const float4 h = float4(p, 1.0);
  return float3(dot(inst.rows[0], h), dot(inst.rows[1], h), dot(inst.rows[2], h));
}
// Where the instance stands (its transform's translation): the fog of war reads it there.
float3 instanceAnchor(InstanceGpu inst) { return float3(inst.rows[0].w, inst.rows[1].w, inst.rows[2].w); }
float3 transformDirection(InstanceGpu inst, float3 v) {
  return float3(dot(inst.rows[0].xyz, v), dot(inst.rows[1].xyz, v), dot(inst.rows[2].xyz, v));
}

float2 snorm16x2(uint packed) {
  const int2 v = int2(int(packed << 16) >> 16, int(packed) >> 16);
  return max(float2(v) / 32767.0, -1.0);
}
float3 octDecode(float2 e) {
  float3 n = float3(e, 1.0 - abs(e.x) - abs(e.y));
  const float t = saturate(-n.z);
  n.xy += float2(n.x >= 0.0 ? -t : t, n.y >= 0.0 ? -t : t);
  return normalize(n);
}
float3 decodeNormal(uint packed) { return octDecode(snorm16x2(packed)); }
float4 decodeTangent(uint packed) { return float4(octDecode(snorm16x2(packed & 0xFFFEFFFFu)), (packed & 0x10000u) ? -1.0 : 1.0); }

float srgbToLinear(float c) { return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }
float3 srgbToLinear3(float3 c) { return float3(srgbToLinear(c.r), srgbToLinear(c.g), srgbToLinear(c.b)); }
float4 decodeColor(uint packed) {
  const float4 c = float4(packed & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF, packed >> 24) / 255.0;
  return float4(srgbToLinear(c.r), srgbToLinear(c.g), srgbToLinear(c.b), c.a);
}

// Screen-space error in pixels of bounds (mesh-space centre, radius, error) under an instance.
float screenError(MeshFrame f, InstanceGpu inst, float3 center, float radius, float error) {
  const float3 c = transformPoint(inst, center);
  const float d = length(c - f.cameraPosition) - radius * inst.scale;
  return error * inst.scale / max(d, f.zNear) * f.lodScale;
}

bool sphereInFrustum(MeshFrame f, float3 center, float radius) {
  [unroll] for (uint i = 0; i < 5; ++i)
    if (dot(f.frustum[i].xyz, center) + f.frustum[i].w < -radius) return false;
  return true;
}

// Screen rectangle (uv, 0..1, y down) of a view-space sphere (forward = +z); false when it crosses
// the near plane. "2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere" (Mara and
// McGuire 2013), in the form used by niagara (Kapoulkine).
bool projectSphere(float3 c, float r, float zNear, float p00, float p11, out float4 aabb) {
  aabb = 0.0;
  if (c.z < r + zNear) return false;
  const float3 cr = c * r;
  const float czr2 = c.z * c.z - r * r;
  const float vx = sqrt(c.x * c.x + czr2);
  const float minx = (vx * c.x - cr.z) / (vx * c.z + cr.x);
  const float maxx = (vx * c.x + cr.z) / (vx * c.z - cr.x);
  const float vy = sqrt(c.y * c.y + czr2);
  const float miny = (vy * c.y - cr.z) / (vy * c.z + cr.y);
  const float maxy = (vy * c.y + cr.z) / (vy * c.z - cr.y);
  aabb = float4(minx * p00, miny * p11, maxx * p00, maxy * p11);
  aabb = aabb.xwzy * float4(0.5, -0.5, 0.5, -0.5) + 0.5;  // NDC (y up) -> uv (y down)
  return true;
}

// Is a world-space sphere possibly visible against the depth pyramid, seen from a camera? Reversed-Z:
// larger is nearer.
bool sphereVisibleHiZ(MeshFrame f, float3 worldCenter, float radius, float4x4 view, float projectionX, float projectionY) {
  const float3 v = mul(float4(worldCenter, 1.0), view).xyz;
  const float3 c = float3(v.x, v.y, -v.z);  // forward = +z
  float4 aabb;
  if (!projectSphere(c, radius, f.zNear, projectionX, projectionY, aabb)) return true;
  const float width = (aabb.z - aabb.x) * f.hizSize.x, height = (aabb.w - aabb.y) * f.hizSize.y;
  const float level = ceil(log2(max(width, height)));
  const float farthest = gTextures[NonUniformResourceIndex(f.hizTexture)]
                             .SampleLevel(gSamplers[f.hizSampler], (aabb.xy + aabb.zw) * 0.5, level).r;
  const float nearest = f.zNear / (c.z - radius);
  return nearest >= farthest;
}
bool sphereVisibleHiZ(MeshFrame f, float3 worldCenter, float radius) {
  return sphereVisibleHiZ(f, worldCenter, radius, f.view, f.projectionX, f.projectionY);
}

#endif
