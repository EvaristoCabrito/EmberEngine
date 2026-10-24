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
  static const std::pair<std::string_view, std::string_view> kOwn[] = {
      {"ashen-forest-crossing", "ashen-forest-bg-v03.jpg"}, {"estalagem", "inn-panorama-v01.jpg"}};
  for (const auto& [id, file] : kOwn)
    if (draftId == id) return std::string(file);
  return {};
}
// EmberEngine's own ground tiles (content/ground, made from the user's art by tools/scripts/ground/make_tile.py): a map's
// grass and forest hexes wear its tile in place of the procedural surfaces, that map only (BoardData::load appends
// each as a ground layer, Board lays it). `tile` = world units per repeat.
// `map` = the set: a map id, or the name a draft picks with its "emberTiles" field (MapDraft::emberTiles). `terrain` = the
// terrain id whose hexes wear it; empty = the set's grass and forest surfaces.
// Never put a set on a map that already has its own ground (the user, 2026-10-20: O Vau's grass tile replaced the map's own
// grass and was removed; a new ground is only ever a new map version that picks it).
// The cemetery's set (cemiterio-v01): the user's six photos (Attachments/texturas/cemi01-06.jpg, 2026-10-20) cut by
// make_tile.py; each photo shows about 4 m of ground across its 1168 px, so its 784 px tile repeats every ~3.5-5 units.
struct EmberGroundTile {
  std::string_view map, terrain, key, file;
  float tile;
  bool overBaked = false;  // also over the terrain's baked art (by default a tile takes only the hexes the procedural surfaces had)
};
inline constexpr EmberGroundTile kEmberGroundTiles[] = {
    {"cemiterio-v01", "plains", "ember-cemiterio-relva-neve-v01", "content/ground/cemiterio-relva-neve-v01.png", 4.5f},  // cemi01: snow over dead grass
    {"cemiterio-v01", "snow", "ember-cemiterio-trilha-v01", "content/ground/cemiterio-trilha-v01.png", 5.0f},        // cemi02: trodden slush, the paths
    {"cemiterio-v01", "nave", "ember-cemiterio-lajes-v01", "content/ground/cemiterio-lajes-v01.png", 3.6f},          // cemi03: setts, frost in the joints
    {"cemiterio-v01", "ruins", "ember-cemiterio-entulho-v01", "content/ground/cemiterio-entulho-v01.png", 4.0f},     // cemi04: rubble
    {"cemiterio-v01", "woods", "ember-cemiterio-bosque-v01", "content/ground/cemiterio-bosque-v01.png", 4.5f},       // cemi05: leaf litter, snow
    {"cemiterio-v01", "hill", "ember-cemiterio-encosta-v01", "content/ground/cemiterio-encosta-v01.png", 4.5f},      // cemi06: stones, frosted grass
    // A Ponte de Pedra 023's deck (ponte-v01): the user's BridgeTiles photo (Attachments/BridgeTiles.jpg, 2026-10-21; ~9 setts across).
    {"ponte-v01", "ruins", "ember-ponte-calcamento-v01", "content/ground/ponte-calcamento-v01.png", 3.2f, true},
    // The watchtower's floors (the user's photos in Attachments/watchtower, 2026-10-21; about 16 setts across each): torre-v01 the
    // tower's own floors, prisao-v01 the prison under it (its photo's vignette evened out by make_tile.py --flatten).
    {"torre-v01", "nave", "ember-torre-piso-v01", "content/ground/torre-piso-v01.png", 5.6f},
    {"prisao-v01", "nave", "ember-prisao-piso-v01", "content/ground/prisao-piso-v01.png", 5.2f},
    // Deep Ashen Forest: charred forest litter, loose pale ash on clearings, and ash-darkened ridges.
    {"deep-ashen-v01", "woods", "ember-deep-ashen-charred-v01", "content/ground/deep-ashen-forest-ash-tile-v01.png", 4.5f},
    {"deep-ashen-v01", "hill", "ember-deep-ashen-slope-v01", "content/ground/deep-ashen-forest-ash-tile-v01.png", 4.5f},
    {"deep-ashen-v01", "plains", "ember-deep-ashen-loose-ash-v01", "content/ground/deep-ashen-forest-loose-ash-tile-v01.png", 4.0f},
    // The Misty Cave (cave-v01, the user's photos in Attachments/MistyCave, 2026-10-23; docs/MISTY_CAVE_PROMPTS.md T1-T5, W1; each ~4 m
    // of ground across its 784 px): bedrock floor, the old worked passage, rockfall, calcite ledges, the wet margin by the water; every
    // one over its terrain's baked art too. "side" is laid on no hex: BoardGround puts it on the board's sides and cliffs (the rock wall).
    {"cave-v01", "plains", "ember-cave-chao-v01", "content/ground/cave-chao-v01.png", 4.5f, true},
    {"cave-v01", "nave", "ember-cave-piso-antigo-v01", "content/ground/cave-piso-antigo-v01.png", 4.0f, true},
    {"cave-v01", "ruins", "ember-cave-desmoronamento-v01", "content/ground/cave-desmoronamento-v01.png", 4.0f, true},
    {"cave-v01", "hill", "ember-cave-calcita-v01", "content/ground/cave-calcita-v01.png", 4.0f, true},
    {"cave-v01", "snow", "ember-cave-margem-v01", "content/ground/cave-margem-v01.png", 4.5f, true},
    {"cave-v01", "side", "ember-cave-parede-v01", "content/ground/cave-parede-v01.png", 3.0f},
    // The Misty Cave's level 2, A Garganta das Raízes (cave2-v01, Attachments/MistyCave/2nd Floor, 2026-10-23; docs/MISTY_DEPTH2_PROMPTS.md):
    // the root mat on plains, the carved slate ledges on nave, slate scree and snapped roots on ruins, the colossal roots' backs on hill,
    // the sap seeps on snow; the gorge wall (slate threaded with roots) on the sides.
    {"cave2-v01", "plains", "ember-cave2-raizes-v01", "content/ground/cave2-raizes-v01.png", 4.5f, true},
    {"cave2-v01", "nave", "ember-cave2-ardosia-v01", "content/ground/cave2-ardosia-v01.png", 4.0f, true},
    {"cave2-v01", "ruins", "ember-cave2-lascas-v01", "content/ground/cave2-lascas-v01.png", 4.0f, true},
    {"cave2-v01", "hill", "ember-cave2-dorso-v01", "content/ground/cave2-dorso-v01.png", 4.0f, true},
    {"cave2-v01", "snow", "ember-cave2-seiva-v01", "content/ground/cave2-seiva-v01.png", 4.0f, true},
    {"cave2-v01", "side", "ember-cave2-parede-v01", "content/ground/cave2-parede-v01.png", 3.0f},
    // The Misty Cave's level 3, A Bacia dos Esporos (cave3-v01, Attachments/MistyCave/3rdfloor, 2026-10-23; docs/MISTY_DEPTH3_PROMPTS.md):
    // the mycelium loam on plains, the mushroom-cap walkway on nave, the fallen giant mushrooms on ruins, the shelf-fungus tops on hill,
    // the spore mire on snow; the mycelium wall W1 (Attachments/Underworld, delivered later the same day) on the sides.
    {"cave3-v01", "plains", "ember-cave3-micelio-v01", "content/ground/cave3-micelio-v01.png", 4.5f, true},
    {"cave3-v01", "nave", "ember-cave3-calcada-v01", "content/ground/cave3-calcada-v01.png", 4.0f, true},
    {"cave3-v01", "ruins", "ember-cave3-queda-v01", "content/ground/cave3-queda-v01.png", 4.0f, true},
    {"cave3-v01", "hill", "ember-cave3-orelha-v01", "content/ground/cave3-orelha-v01.png", 4.5f, true},
    {"cave3-v01", "snow", "ember-cave3-lodacal-v01", "content/ground/cave3-lodacal-v01.png", 4.0f, true},
    {"cave3-v01", "side", "ember-cave3-parede-v01", "content/ground/cave3-parede-v01.png", 3.0f},
    // Esporabrigo, the fungus-folk's village in the Bacia (sporehaven-v01, Attachments/Underworld, 2026-10-23;
    // docs/MISTY_MISSING_GROUNDS.md): the tilled beds on plains, the felted mycelium lanes on nave, the harvest refuse on ruins, the
    // stalk-round terraces on hill, the spore-washing pool's edge on snow; the village's palisade of stalk rounds on the sides.
    {"sporehaven-v01", "plains", "ember-sporehaven-canteiros-v01", "content/ground/sporehaven-canteiros-v01.png", 4.5f, true},
    {"sporehaven-v01", "nave", "ember-sporehaven-feltro-v01", "content/ground/sporehaven-feltro-v01.png", 4.0f, true},
    {"sporehaven-v01", "ruins", "ember-sporehaven-colheita-v01", "content/ground/sporehaven-colheita-v01.png", 4.0f, true},
    {"sporehaven-v01", "hill", "ember-sporehaven-talos-v01", "content/ground/sporehaven-talos-v01.png", 4.0f, true},
    {"sporehaven-v01", "snow", "ember-sporehaven-lavagem-v01", "content/ground/sporehaven-lavagem-v01.png", 4.0f, true},
    {"sporehaven-v01", "side", "ember-sporehaven-palicada-v01", "content/ground/sporehaven-palicada-v01.png", 3.0f},
    // O Micélio Profundo, the fungus-folk's colony by the black river (deep-mycelium-v01, Attachments/Underworld, 2026-10-23;
    // docs/MISTY_MISSING_GROUNDS.md): the lilac mycelium lace over shale on plains, the corded shale causeways on nave, the river refuse on
    // ruins, the coral-fungus mounds on hill, the sown river silt on snow; the shale wall wrapped in mycelium on the sides.
    {"deep-mycelium-v01", "plains", "ember-micelio-renda-v01", "content/ground/micelio-renda-v01.png", 4.5f, true},
    {"deep-mycelium-v01", "nave", "ember-micelio-calcada-v01", "content/ground/micelio-calcada-v01.png", 4.0f, true},
    {"deep-mycelium-v01", "ruins", "ember-micelio-refugo-v01", "content/ground/micelio-refugo-v01.png", 4.0f, true},
    {"deep-mycelium-v01", "hill", "ember-micelio-coral-v01", "content/ground/micelio-coral-v01.png", 4.0f, true},
    {"deep-mycelium-v01", "snow", "ember-micelio-lodo-v01", "content/ground/micelio-lodo-v01.png", 4.0f, true},
    {"deep-mycelium-v01", "side", "ember-micelio-parede-v01", "content/ground/micelio-parede-v01.png", 3.0f},
    // The Misty Cave's map 4, O Underdark de Mordávia (cave4-v01, Attachments/MistyCave/4rdFloor, 2026-10-23; docs/MISTY_DEPTH4_PROMPTS.md):
    // the Root-Crown's silver bark on plains, the Enclave's plank decks on nave, broken branches and crystal shards on ruins, the crown's
    // knotted burls on hill, the Glasswater Sea's shore on snow; the boughs' flanks on the sides.
    {"cave4-v01", "plains", "ember-cave4-casca-v01", "content/ground/cave4-casca-v01.png", 4.5f, true},
    {"cave4-v01", "nave", "ember-cave4-plataforma-v01", "content/ground/cave4-plataforma-v01.png", 4.0f, true},
    {"cave4-v01", "ruins", "ember-cave4-galhos-v01", "content/ground/cave4-galhos-v01.png", 4.0f, true},
    {"cave4-v01", "hill", "ember-cave4-no-v01", "content/ground/cave4-no-v01.png", 4.5f, true},
    {"cave4-v01", "snow", "ember-cave4-margem-v01", "content/ground/cave4-margem-v01.png", 4.0f, true},
    {"cave4-v01", "side", "ember-cave4-flanco-v01", "content/ground/cave4-flanco-v01.png", 3.0f},
};

inline std::string skyMoonAsset(std::string_view phase) { return "sky/moon/" + std::string(phase) + ".tex"; }

}  // namespace ember::e2
