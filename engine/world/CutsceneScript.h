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
//       { "do": "move", "actor": "A", "near": "B" },                       // ... or to the free hex nearest another actor
//       { "do": "say",  "actor": "A", "text": "...", "translations": { "en": "..." } },  // the game's dialogue box
//       { "do": "wait", "seconds": 1.0 },
//       { "do": "enter", "actor": "B", "to": [12, 9] },                    // an offstage actor appears there (below)
//       { "do": "shot", "on": "A", "look": "B", "tilt": 12, "distance": "close", "lift": 0.38, "over": 2.0 }  // the camera (below)
//     ]
//   }
//
// An actor declared { "unit": "Neera", "offstage": true } waits off the board from the map's opening until its "enter" step
// (Estranhos na Costa, 2026-11-06: Neera and Voss come only after Kael's first fight): a unit of that name not of the party is
// kept out of the fight (not drawn, no turn, blocks nothing) and appears on the step's hex, fading in. A party hero of that
// name (Modo teste seats everyone) is never sent off: its "enter" walks it to the hex like "move".
// An actor declared { "unit": "Vora", "fallen": true } may be dead when the scene starts (the fight's dead, in an onVictory scene):
// its body stays on the board for the scene's shots instead of fading away (Stone Bridge, 2026-11-06).
//
// The film steps (Stone Bridge, 2026-11-06; docs/CUTSCENES.md): the camera and the screen, so a scene can be cut like a film
// instead of watched from the player's view.
//   "shot": the camera's goal. "on": an actor or [x, y] (the pivot), or "between": [A, B] (their middle); "turn": the compass
//     heading the camera looks along ("n" "ne" "e" "se" "s" "sw" "w" "nw", or degrees, 0 north and 90 east), or "look": an actor
//     the camera looks at from the pivot, or "behind": an actor over whose shoulder it looks at the pivot; "tilt": degrees above
//     the ground (0 level, 90 straight down, negative looks up at the sky); "distance": a number, or "close" (the subject's card
//     fills the frame), "medium" (1.8 x close), "wide" (3.2 x close); "lift": share of the subject's card height the eye and the
//     look point rise by (the chest at 0.38; world units when the pivot is a hex); "offset": [x, z] world units added to the pivot
//     (the camera may stand up to 4 hexes past the board's edge: over the river); "over": the seconds the move takes (0 = the
//     orbit's own glide); "cut": true jumps there; "async": true lets the scene go on while the camera moves. A field left out keeps
//     what the camera has. The player's view comes back at the scene's end unless the script says "cameraBack": false.
//   "shake": { "seconds", "strength" (world units, 0.15) }: the view trembles, fading out over its seconds.
//   "fade": { "to": "black" | "clear", "seconds" }: the screen to black and back (the scene waits its seconds).
//   "bars": { "on": true | false }: the cinema's letterbox bars.
//   "tint": { "percent", "over" }: the picture darkened by that much, over those seconds.
//   "flash": { "seconds" }: a white flash.
//   "sfx": { "file", "volume" }: an effect from game/SoundFX. "music": { "file", "volume" } or { "stop": true }: the map's track
//     replaced (or silenced) until the scene ends.
//   "caption": { "text", "translations", "style": "plain" | "red" | "blue", "seconds" }: words over the picture ("…mais tarde";
//     the red and blue boxes of a status change), the scene waiting their seconds.
//   "emote": { "actor", "text" }: a mark popping over the actor's head ("!", "!!", "?", "…").
// The file is refused whole when a step is unknown, an actor is not declared, a coordinate or a duration is malformed: nothing of
// a bad script plays. Whether an actor stands on the map, and whether a destination is free and reachable, is the map's to check
// when the scene starts / the step plays.

#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "world/Campaign.h"  // ember::Json

namespace ember {

struct CutsceneStep {
  enum class Kind { Move, Face, Say, Wait, Enter, Exit, Shot, Shake, Fade, Bars, Tint, Flash, Sfx, Music, Caption, Emote };
  Kind kind = Kind::Wait;
  std::string actor;        // the actor's stable name (move / face / say / emote)
  int x = 0, y = 0;         // move / enter: the destination hex; shot: the pivot hex (hasHex)
  std::string near;         // move: the destination is the free hex nearest this actor instead of `to`
  int direction = -1;       // face: 0 e, 1 ne, 2 nw, 3 w, 4 sw, 5 se (Battle::neighbors' order); -1 = `toward`
  std::string toward;       // face: the actor faced instead
  std::string text;         // say / caption: the line; emote: the mark; sfx / music: the file
  Json translations;        // say / caption: {"en": ...}, the dialogue box's translations field
  std::string portrait;     // say: a sprite name for the portrait, when the speaker's own is not wanted
  std::string speaker;      // say: the name shown instead of the unit's ("???": a voice); with it the actor may still be off the board
  float seconds = 0.0f;     // wait / fade / caption / shake / flash: the duration; shot: "over"; tint: "over"
  // shot: the camera's goal; a field not given keeps what the camera has (the header above).
  std::string on, on2;      // the pivot: an actor, or two actors (their middle, "between")
  bool hasHex = false;      // ... or the hex x, y
  bool hasTurn = false;     // "turn": the heading the camera looks along, radians (0 north, +pi/2 east)
  float turn = 0.0f;
  std::string look;         // "look": the camera looks from the pivot toward this actor
  std::string behind;       // "behind": the eye on this actor's side of the pivot, over its shoulder
  bool hasTilt = false;     // "tilt": radians above the ground (0 level, pi/2 straight down; negative looks up)
  float tilt = 0.0f;
  int distanceKind = 0;     // 0 kept, 1 the number, 2 "close", 3 "medium", 4 "wide"
  float distance = 0.0f;
  bool hasLift = false;     // "lift": share of the subject's card height (a hex: world units)
  float lift = 0.0f;
  bool hasOffset = false;   // "offset": world units added to the pivot (x, z): a shot past the board's edge, over the river
  float offsetX = 0.0f, offsetZ = 0.0f;
  bool cut = false;         // the view jumps there (no glide)
  bool async = false;       // the scene goes on while the camera moves
  // shake / sfx / music / fade / tint / bars / caption
  float strength = 0.0f;    // shake: world units; tint: 0..1; sfx / music: the volume
  bool flag = false;        // bars: on; fade: to black; music: stop
  int style = 0;            // caption: 0 plain, 1 red box, 2 blue box
};

struct CutsceneScript {
  std::string id;
  std::map<std::string, std::string> actors;  // stable name -> the unit's name on the map
  std::set<std::string> offstage;             // the actors who wait off the board until their "enter" step
  std::set<std::string> fallen;               // the actors who may be dead when the scene starts (their bodies kept for it)
  bool cameraOnActors = false;                // "camera": "actors": the camera's pivot glides to the middle of the actors on the board
  bool cameraBack = true;                     // "cameraBack": false keeps the last shot's view when the scene ends
  std::vector<CutsceneStep> steps;

  static constexpr const char* kFormat = "ember-cutscene-1";
  static constexpr std::array<const char*, 6> kDirections{"e", "ne", "nw", "w", "sw", "se"};
  static constexpr std::array<const char*, 8> kHeadings{"n", "ne", "e", "se", "s", "sw", "w", "nw"};  // 45 degrees apart, clockwise

  // The script read from `file`; `why` tells what is wrong when it is refused (the step's index when one is at fault).
  static std::optional<CutsceneScript> load(const std::string& file, std::string* why);
  // The same from a parsed document.
  static std::optional<CutsceneScript> parse(const Json& j, std::string* why);
};

}  // namespace ember
