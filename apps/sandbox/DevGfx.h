#pragma once

// How Engine2's Dev Controls switches (DevGfxSettings.h) act on a map the game opens (MapStage), the way Engine2's battle
// renderer read them every frame. A battle already open takes later changes through MapStage::applyGfx.

#include "DevGfxSettings.h"
#include "MapStage.h"
#include "render/Renderer.h"

namespace ember {

inline void DevGfx::apply(MapStageOptions& m, Renderer& renderer) const {
  if (!realShadows) m.layers &= ~2u;
  if (!localLights) m.layers &= ~8u;
  if (!fogOfWar) m.fog = 0;
  if (!atmosphericFx) m.volume = false, m.airParticles = false;
  if (sunMoved) {
    // Engine2's azimuth is where shadows fall on screen (+x right, +z down in the normal view); the sun stands opposite.
    m.sunAzimuth = sunAzimuth + 180.0f;
    m.sunElevation = sunElevation;
  }
  renderer.setShadowResolution(shadowResolution);
  renderer.setShadowFilter(softShadows ? 4.0f : 1.6f);
  renderer.setSpriteFeatures(spriteFeatures(), spriteThickness);
  if (fogDebug) renderer.setDebugView(4);
}

}  // namespace ember
