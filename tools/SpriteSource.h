#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "render/SpriteFormat.h"

namespace ember {

// What the sprite cooker reads: sprite sets (one per character art folder), each a list of clips,
// each clip a list of source frame files. Stored as JSON (content/*.sprites.json):
//
//   { "version": 1, "sets": [ { "name": "neera", "clips": [
//       { "name": "idle", "pack": "field", "mode": "pingpong", "fps": 12,
//         "frames": { "pattern": "sprites/neera/idle-{}.png", "count": 36 } },
//       { "name": "walk-back", "pack": "field", "mode": "loop", "fps": 1, "frames": { "file": "sprites/kael/walk-back.png" } } ] } ] }
//
// Frame paths are relative to a source root given to the cooker, so art can live anywhere. A frame may name one
// cell of a grid sheet, "fx/fire-flipbook-4x4.png#4x4:5" (columns x rows : cell, row by row), and may end in
// "!black" for art painted on black (alpha from its brightest channel, like Engine2's deriveAlphaFromBlack but
// reaching full at a quarter of white, so solid art such as an arrow is opaque).

struct SpriteClipSource {
  std::string name;
  std::string pack;  // which pack loads it: "combat", "field", "cutscene/<name>"
  SpritePlayMode mode = SpritePlayMode::Loop;
  float fps = 12.0f;
  std::vector<std::string> frames;  // relative to the source root, in play order
};

struct SpriteSetSource {
  std::string name;  // "neera/neera-v2-001"; becomes the asset path prefix
  std::vector<SpriteClipSource> clips;
};

struct SpriteManifest {
  std::vector<SpriteSetSource> sets;
};

bool loadSpriteManifest(const std::filesystem::path& path, SpriteManifest& out);
bool saveSpriteManifest(const std::filesystem::path& path, const SpriteManifest& manifest);

// Builds a manifest from Engine2's sprite tables (src/units/manifest.json, tools/sprite-cleanup.json),
// with Engine2's pacing rules (src/units/visual.ts framePace) turned into fps and play modes.
// Frame paths are relative to <engine2>/public/game. Engine2 is only read.
bool importEngine2Sprites(const std::filesystem::path& engine2Root, SpriteManifest& out);

}  // namespace ember
