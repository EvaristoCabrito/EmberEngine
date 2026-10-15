#pragma once

#include <string_view>

#include "TextureCooker.h"

namespace ember {

// Engine2's procedural ground surfaces (C:/Engine2/src/render/textures.ts), generated with the same noise
// and colours, pixel for pixel: the ground a map shows where it has no baked photo layer.
// Layers, in Engine2's PROTO_LAYERS order: grass, forest, pavers, snow, rock, gravel, ash, earth, cliff
// (256 x 256, tileable, sRGB). "smooth-plains" is the 1024 x 1024 flat grass of the
// variant-plains-smooth layer (smoothPlainsCanvas).
inline constexpr std::string_view kEngine2ProtoLayers[] = {"grass", "forest", "pavers", "snow", "rock", "gravel", "ash", "earth", "cliff"};

bool engine2ProceduralTexture(std::string_view name, Rgba8& out);

}  // namespace ember
