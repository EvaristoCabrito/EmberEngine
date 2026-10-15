// Provisional lighting shared by every lit surface (3D meshes, the board's ground): one sun (GGX
// specular + Lambert diffuse) and a sky/ground hemisphere. The frame's real lighting model (clustered
// lights, shadows, how 2D cards and 3D share light) comes with Demonstration 1 (docs/ARCHITECTURE.md §7.1);
// keeping it in one place keeps every surface lit the same way until then.
#ifndef EMBER_LIGHTING_HLSLI
#define EMBER_LIGHTING_HLSLI

#include "scene.hlsli"

static const float kPi = 3.14159265;

float3 fresnelSchlick(float3 f0, float vh) { return f0 + (1.0 - f0) * pow(1.0 - vh, 5.0); }
float ggx(float nh, float a) {
  const float a2 = a * a, d = nh * nh * (a2 - 1.0) + 1.0;
  return a2 / (kPi * d * d);
}
float smithVisibility(float nv, float nl, float a) {  // height-correlated, includes 1 / (4 nv nl)
  const float a2 = a * a;
  const float v = nl * sqrt(nv * nv * (1.0 - a2) + a2), l = nv * sqrt(nl * nl * (1.0 - a2) + a2);
  return 0.5 / max(v + l, 1e-5);
}

// Sun + hemisphere on a surface point (n: shading normal, unit length, facing the viewer).
float3 shadeSurface(MeshFrame f, float3 position, float3 n, float3 baseColor, float roughness, float metallic, float occlusion) {
  const float3 view = normalize(f.cameraPosition - position);
  const float a = max(roughness * roughness, 0.002);
  const float3 diffuseColor = baseColor * (1.0 - metallic);
  const float3 f0 = lerp(0.04, baseColor, metallic);
  const float3 l = f.sunDirection, h = normalize(l + view);
  const float nl = saturate(dot(n, l)), nv = max(dot(n, view), 1e-4), nh = saturate(dot(n, h)), vh = saturate(dot(view, h));
  const float3 F = fresnelSchlick(f0, vh);
  const float3 sun = (diffuseColor * (1.0 - F) / kPi + F * ggx(nh, a) * smithVisibility(nv, nl, a)) * nl * f.sunColor * f.sunIntensity;
  const float3 hemisphere = lerp(f.groundColor, f.skyColor, n.y * 0.5 + 0.5);
  const float3 ambient = (diffuseColor + f0 * (1.0 - roughness) * 0.25) * hemisphere * f.ambientIntensity * occlusion;
  return sun + ambient;
}

#endif
