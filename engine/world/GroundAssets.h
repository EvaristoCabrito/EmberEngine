#pragma once

// Asset names of the cooked ground textures (Cooker ground), shared by the cooker and the runtime.
//   ground/proto/<surface>.tex         Engine2's 9 procedural surfaces (BC7 sRGB, 256 px)
//   ground/plains049/<role>.tex        the plains049 surface's photo maps (color, normal, rough)
//   ground/layer/<key>/<role>.tex      each baked ground layer present in the set (color, normal, rough)
//   sky/moon/<phase>.tex               the moon's phase art (Engine2's icons, BC7 sRGB with alpha, 256 px)
//   backdrop/<file>.tex                every painted map backdrop (public/game/map-backgrounds, BC7 sRGB)
// Roles: color = BC7 sRGB, normal = BC5 (X, Y of the stored normal), rough = BC7 linear (R used).

#include <string>
#include <string_view>

namespace ember::e2 {

inline std::string groundProtoAsset(std::string_view surface) { return "ground/proto/" + std::string(surface) + ".tex"; }
inline std::string groundPlains049Asset(std::string_view role) { return "ground/plains049/" + std::string(role) + ".tex"; }
inline std::string groundLayerAsset(std::string_view key, std::string_view role) {
  return "ground/layer/" + std::string(key) + "/" + std::string(role) + ".tex";
}

inline std::string backdropAsset(std::string_view file) { return "backdrop/" + std::string(file) + ".tex"; }
// EmberEngine's own painted backdrops (content/backdrops, the user's art; cooked beside Engine2's): the file for a map's
// draft id, used in place of the scene's own; "" = none.
inline std::string emberBackdrop(std::string_view draftId) {
  static const std::pair<std::string_view, std::string_view> kOwn[] = {{"ashen-forest-crossing", "ashen-forest-bg-v03.jpg"}};
  for (const auto& [id, file] : kOwn)
    if (draftId == id) return std::string(file);
  return {};
}
inline std::string skyMoonAsset(std::string_view phase) { return "sky/moon/" + std::string(phase) + ".tex"; }

}  // namespace ember::e2
