#include "render/vfx/ElementSprites.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace ember {

namespace {

// createElementV2Sprites profiles: main width / height, ground width / height, main y, motes (x, y, size, phase).
struct Profile {
  float width, height, groundWidth, groundHeight, mainY;
  std::array<std::array<float, 4>, 4> motes;
};
const Profile& profile(VfxElement e) {
  static const Profile k[8] = {
      {0.98f, 1.22f, 0.96f, 0.3f, -0.02f, {{{-0.3f, 0.32f, 0.15f, 1}, {0.28f, 0.49f, 0.14f, 5}, {-0.2f, 0.72f, 0.12f, 9}, {0.22f, 0.91f, 0.1f, 13}}}},
      {0.92f, 1.2f, 1.02f, 0.32f, -0.08f, {{{-0.3f, 0.33f, 0.16f, 1}, {0.29f, 0.49f, 0.14f, 5}, {-0.2f, 0.72f, 0.12f, 9}, {0.22f, 0.91f, 0.1f, 13}}}},
      {0.82f, 1.32f, 0.9f, 0.28f, -0.02f, {{{-0.26f, 0.4f, 0.14f, 1}, {0.24f, 0.58f, 0.12f, 5}, {-0.18f, 0.8f, 0.1f, 9}, {0.2f, 1.02f, 0.1f, 13}}}},
      {1.02f, 1.16f, 1.08f, 0.36f, -0.05f, {{{-0.32f, 0.3f, 0.15f, 1}, {0.3f, 0.45f, 0.15f, 5}, {-0.2f, 0.67f, 0.13f, 9}, {0.23f, 0.88f, 0.12f, 13}}}},
      {1.04f, 1.22f, 1.06f, 0.34f, 0.0f, {{{-0.3f, 0.34f, 0.14f, 1}, {0.29f, 0.52f, 0.13f, 5}, {-0.2f, 0.75f, 0.12f, 9}, {0.22f, 0.96f, 0.11f, 13}}}},
      {1.0f, 1.28f, 0.96f, 0.34f, -0.23f, {{{-0.28f, 0.32f, 0.15f, 1}, {0.28f, 0.48f, 0.13f, 5}, {-0.2f, 0.73f, 0.12f, 9}, {0.23f, 0.88f, 0.1f, 13}}}},
      {1.08f, 1.15f, 1.08f, 0.38f, 0.0f, {{{-0.31f, 0.28f, 0.15f, 1}, {0.29f, 0.45f, 0.13f, 5}, {-0.21f, 0.69f, 0.12f, 9}, {0.22f, 0.88f, 0.1f, 13}}}},
      {1.08f, 1.18f, 0.98f, 0.3f, -0.12f, {{{-0.3f, 0.34f, 0.15f, 1}, {0.3f, 0.5f, 0.14f, 5}, {-0.2f, 0.74f, 0.12f, 9}, {0.22f, 0.94f, 0.1f, 13}}}},
  };
  return k[static_cast<int>(e)];
}

}  // namespace

const char* vfxElementName(VfxElement e) {
  static const char* k[8] = {"fire", "frost", "lightning", "poison", "arcane", "holy", "shadow", "ember"};
  return k[static_cast<int>(e)];
}

ElementSpritesV2 ElementSpritesV2::defaults(VfxElement element) {
  // DEFAULT_PIXEL_SETTINGS + PIXEL_DEFAULTS[element] + PIXEL_V2_DEFAULTS[element]: only emissive, opacity and
  // animationSpeed reach the V2 sprites.
  static const float kEmissive[8] = {1.15f, 1.35f, 1.9f, 1.2f, 1.65f, 1.5f, 0.92f, 1.25f};
  ElementSpritesV2 s;
  s.element = element;
  s.emissive = kEmissive[static_cast<int>(element)];
  if (element == VfxElement::Holy) s.emissive = 1.05f, s.opacity = 0.7f, s.animationSpeed = 0.8f;
  if (element == VfxElement::Fire) s.emissive = 1.15f, s.opacity = 0.9f, s.animationSpeed = 0.95f;
  return s;
}

void ElementSpritesV2::build(glm::vec3 at, float time, float tile, VfxFrame& frame) const {
  const Profile& p = profile(element);
  const float groupScale = std::max(1.0f, tile) * scale;
  const float t = time * animationSpeed;
  const int base = static_cast<int>(std::floor(t * 12.0f)) % 16;
  const float colour = emissive * intensity;
  const glm::vec3 sight = vfxFromEmber(at);
  // One layer: a sprite at local (x, y, z) of the group, w x h, its starting frame, opacity, rotation, anchor.
  auto sprite = [&](float x, float y, float z, float w, float h, int phase, float layerOpacity, float rotation, bool floor) {
    VfxInstance inst;
    inst.a = glm::vec4(at + glm::vec3(x, y, z) * groupScale, 0.0f);
    inst.b = {rotation + std::sin(t * 1.8f + phase) * 0.035f, static_cast<float>((base + phase) % 16), layerOpacity * opacity, colour};
    inst.c = {w * groupScale, h * groupScale, floor ? 0.0f : 0.5f, 0.0f};
    return inst;
  };
  auto draw = [&](const VfxFlipbook& sheet, const std::vector<VfxInstance>& sprites) {
    if (!sheet || sprites.empty()) return;
    static const std::vector<VfxVertex> quad = [] {
      const VfxVertex a{{-0.5f, -0.5f, 0}, 0, {0, 0, 1}, 0}, b{{0.5f, -0.5f, 0}, 1, {0, 0, 1}, 0}, c{{0.5f, 0.5f, 0}, 1, {0, 0, 1}, 1},
          d{{-0.5f, 0.5f, 0}, 0, {0, 0, 1}, 1};
      return std::vector<VfxVertex>{a, b, c, a, c, d};
    }();
    const uint32_t firstInstance = static_cast<uint32_t>(frame.instances.size());
    frame.instances.insert(frame.instances.end(), sprites.begin(), sprites.end());
    VfxDraw d;
    d.gpu.model = vfxEmberToWorld();
    d.gpu.params1 = {sheet.cellWidth, sheet.cellHeight, sheet.cropX, sheet.cropY};
    d.gpu.color = {1.0f / sheet.textureWidth, 1.0f / sheet.textureHeight, 0, 0};
    d.gpu.firstVertex = frame.addVertices(quad);
    d.gpu.vertexCount = static_cast<uint32_t>(quad.size());
    d.gpu.firstInstance = firstInstance;
    d.gpu.material = static_cast<uint32_t>(VfxMaterial::SpriteSheet);
    d.gpu.sightAt = sight;
    d.gpu.texture = sheet.texture;
    d.instances = static_cast<uint32_t>(sprites.size());
    d.blend = VfxBlend::Additive;
    d.depthTest = true;
    frame.draws.push_back(d);
  };
  // The first sheet's complete frames stand on the floor; the second lies as the ground layer; four motes.
  draw(main, {sprite(0.0f, p.mainY, 0.035f, p.width, p.height, 0, 0.88f, 0.0f, true)});
  draw(secondary, {sprite(0.0f, 0.025f, 0.04f, p.groundWidth, p.groundHeight, 3, 0.74f, 0.0f, false)});
  std::vector<VfxInstance> motes;
  for (int i = 0; i < 4; ++i) {
    const auto& m = p.motes[static_cast<size_t>(i)];
    motes.push_back(sprite(m[0], m[1], 0.045f + i * 0.001f, m[2], m[2], static_cast<int>(m[3]), 0.9f - i * 0.055f, (i % 2 ? 1.0f : -1.0f) * 0.1f, false));
  }
  draw(particles, motes);
}

}  // namespace ember
