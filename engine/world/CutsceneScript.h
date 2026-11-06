#pragma once

// An on-map cutscene's script: a content file (content/cutscenes/<id>.json) that directs the units already standing on
// the loaded map, read and checked here, played by the map (MapStage's cutscene controller). The format is
// "ember-cutscene-1":
//
//   {
//     "format": "ember-cutscene-1",
//     "id": "test-aldeia",
//     "actors": { "A": { "unit": "Kael" }, "B": { "unit": "Neera" } },   // stable names -> the map's units, by their name
//     "steps": [
//       { "do": "face", "actor": "A", "direction": "e" },                  // one of the six hex directions: e ne nw w sw se
//       { "do": "face", "actor": "B", "toward": "A" },                     // ... or the direction of another actor
//       { "do": "move", "actor": "A", "to": [14, 20] },                    // walks there (the map's own pathfinding and poses)
//       { "do": "say",  "actor": "A", "text": "...", "translations": { "en": "..." } },  // the game's dialogue box
//       { "do": "wait", "seconds": 1.0 },
//       { "do": "enter", "actor": "B", "to": [12, 9] }                     // an offstage actor appears there (below)
//     ]
//   }
//
// An actor declared { "unit": "Neera", "offstage": true } waits off the board from the map's opening until its "enter" step
// (Estranhos na Costa, 2026-11-06: Neera and Voss come only after Kael's first fight): a unit of that name not of the party is
// kept out of the fight (not drawn, no turn, blocks nothing) and appears on the step's hex, fading in. A party hero of that
// name (Modo teste seats everyone) is never sent off: its "enter" walks it to the hex like "move".
// Only these five actions exist (the Interlude spec's step 1 + enter). The file is refused whole when a step is unknown, an actor
// is not declared, a coordinate or a duration is malformed: nothing of a bad script plays. Whether an actor stands on the
// map, and whether a destination is free and reachable, is the map's to check when the scene starts / the step plays.

#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "world/Campaign.h"  // ember::Json

namespace ember {

struct CutsceneStep {
  enum class Kind { Move, Face, Say, Wait, Enter };
  Kind kind = Kind::Wait;
  std::string actor;        // the actor's stable name (move / face / say)
  int x = 0, y = 0;         // move / enter: the destination hex
  int direction = -1;       // face: 0 e, 1 ne, 2 nw, 3 w, 4 sw, 5 se (Battle::neighbors' order); -1 = `toward`
  std::string toward;       // face: the actor faced instead
  std::string text;         // say: the line (the dialogue's own language)
  Json translations;        // say: {"en": ...}, the dialogue box's translations field
  std::string portrait;     // say: a sprite name for the portrait, when the speaker's own is not wanted
  float seconds = 0.0f;     // wait
};

struct CutsceneScript {
  std::string id;
  std::map<std::string, std::string> actors;  // stable name -> the unit's name on the map
  std::set<std::string> offstage;             // the actors who wait off the board until their "enter" step
  bool cameraOnActors = false;                // "camera": "actors": the camera's pivot glides to the middle of the actors on the board
  std::vector<CutsceneStep> steps;

  static constexpr const char* kFormat = "ember-cutscene-1";
  static constexpr std::array<const char*, 6> kDirections{"e", "ne", "nw", "w", "sw", "se"};

  // The script read from `file`; `why` tells what is wrong when it is refused (the step's index when one is at fault).
  static std::optional<CutsceneScript> load(const std::string& file, std::string* why);
  // The same from a parsed document.
  static std::optional<CutsceneScript> parse(const Json& j, std::string* why);
};

}  // namespace ember
