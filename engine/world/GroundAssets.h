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
// EmberEngine's own ground tiles (content/ground, made from the user's art by tools/scripts/ground/make_tile.py): a map's
// grass and forest hexes wear its tile in place of Engine2's procedural surfaces, that map only (BoardData::load appends
// each as a ground layer, Board lays it). `tile` = world units per repeat, the scale of the same art on the plane around
// the board (O Vau: backdrop-fit.json groundTile 36 for the photo's 1168 px, the tile is 784 of them).
// `map` = the set: a map id, or the name a draft picks with its "emberTiles" field (MapDraft::emberTiles). `terrain` = the
// terrain id whose hexes wear it; empty = the set's grass and forest surfaces (O Vau's rule).
// The cemetery's set (cemiterio-v01): the user's six photos (Attachments/texturas/cemi01-06.jpg, 2026-10-20) cut by
// make_tile.py; each photo shows about 4 m of ground across its 1168 px, so its 784 px tile repeats every ~3.5-5 units.
struct EmberGroundTile {
  std::string_view map, terrain, key, file;
  float tile;
};
inline constexpr EmberGroundTile kEmberGroundTiles[] = {
    {"vau", "", "ember-vau-grass-v01", "content/ground/vau-grass-v01.png", 36.0f * 784.0f / 1168.0f},
    {"cemiterio-v01", "plains", "ember-cemiterio-relva-neve-v01", "content/ground/cemiterio-relva-neve-v01.png", 4.5f},  // cemi01: snow over dead grass
    {"cemiterio-v01", "snow", "ember-cemiterio-trilha-v01", "content/ground/cemiterio-trilha-v01.png", 5.0f},        // cemi02: trodden slush, the paths
    {"cemiterio-v01", "nave", "ember-cemiterio-lajes-v01", "content/ground/cemiterio-lajes-v01.png", 3.6f},          // cemi03: setts, frost in the joints
    {"cemiterio-v01", "ruins", "ember-cemiterio-entulho-v01", "content/ground/cemiterio-entulho-v01.png", 4.0f},     // cemi04: rubble
    {"cemiterio-v01", "woods", "ember-cemiterio-bosque-v01", "content/ground/cemiterio-bosque-v01.png", 4.5f},       // cemi05: leaf litter, snow
    {"cemiterio-v01", "hill", "ember-cemiterio-encosta-v01", "content/ground/cemiterio-encosta-v01.png", 4.5f},      // cemi06: stones, frosted grass
};

inline std::string skyMoonAsset(std::string_view phase) { return "sky/moon/" + std::string(phase) + ".tex"; }

}  // namespace ember::e2
