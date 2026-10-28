#pragma once

// Ember's ProceduralElementEmitter, version 2 presets (src/vfx/legacy/three/ProceduralElementEmitter.ts
// createElementV2Sprites + update): an element's three authored 4x4 sheets (fx/<element>-v2-main / -secondary /
// -particles-flipbook-4x4) composed as one effect standing on a point: the main sheet's frames stand on the floor,
// the secondary sheet lies as a ground halo / seal / puddle / ring, four motes from the particle sheet float around
// the plume. Each is a three Sprite (screen-aligned, additive, alphaTest 0.02) playing at 12 frames a second from
// its own starting frame. The version 1 presets (particle emitters) are not here yet.

#include "render/VfxDraw.h"

namespace ember {

enum class VfxElement { Fire, Frost, Lightning, Poison, Arcane, Holy, Shadow, Ember };
const char* vfxElementName(VfxElement e);  // "fire", "frost", ... (the sheets' file names)
const char* vfxElementLabel(VfxElement e); // Portuguese names for the preview menus

struct ElementSpritesV2 {
  // The three sheets: main, secondary, particles (VfxFlipbook of each cooked clip).
  VfxFlipbook main, secondary, particles;
  VfxElement element = VfxElement::Poison;

  // PixelElementSettings as the caller sets them (scale, intensity, emissive, opacity, animationSpeed).
  float scale = 1.0f, intensity = 1.0f, emissive = 1.0f, opacity = 1.0f, animationSpeed = 1.0f;
  // pixelDefaults(element, 2): the element's own emissive / opacity / animationSpeed.
  static ElementSpritesV2 defaults(VfxElement element);

  // update(dt, tile, time, x, -y): the group at `at` (Ember frame), scaled by max(1, tile) x scale; `time` = seconds
  // since it started (x animationSpeed inside). Draws over the given model (Ember -> world), depth-tested.
  void build(glm::vec3 at, float time, float tile, VfxFrame& frame) const;
};

}  // namespace ember
