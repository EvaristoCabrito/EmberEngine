# On-map cutscenes (Interlude step 1)

The cutscene player directs the units already standing on the loaded map: the sprites walk, turn, speak through the
game's own dialogue box and wait, one step after another, each finished before the next begins. It lives in MapStage
(`playCutscene` / `updateCutscene` / `stopCutscene`, the section "On-map cutscenes") and reads content files; no scene
is written in C++. The full-screen film player (`CutsceneScreen.h`) and the battle's conversations (`BattleTalk.h`,
`NpcTalk.h`) are untouched and keep working beside it.

Two ways of writing scenes are planned (the user, 2026-10-25): the JSON below, written by hand or by an AI, now; a visual
command-list editor over the same format (RPG Maker's event list, with hex picking on the preview) later. The format is
the contract both share.

## The script: `content/cutscenes/<id>.json`

```json
{
  "format": "ember-cutscene-1",
  "id": "test-aldeia",
  "actors": { "A": { "unit": "Kael" }, "B": { "unit": "Neera" } },
  "steps": [
    { "do": "face", "actor": "A", "direction": "w" },
    { "do": "move", "actor": "A", "to": [14, 19] },
    { "do": "face", "actor": "B", "toward": "A" },
    { "do": "say",  "actor": "A", "text": "A vila está em silêncio.", "translations": { "en": "The village is silent." } },
    { "do": "wait", "seconds": 1.0 }
  ]
}
```

- `actors`: stable names used by the steps, each naming a unit by its name on the map (the battle file's `name`). They are
  resolved when the scene starts; a name not on the map refuses the whole scene before anything plays.
- `move`: the actor walks to the hex `[x, y]` on the map's own pathfinding (the whole floor in reach, as in free roam), with
  its walk poses and walk cue; the step is over when the card arrives. The hex must be on the board, walkable, free (or the
  actor's own) and reachable, else the scene fails there. The actor ends facing its last step.
- `face`: `direction` is one of `e ne nw w sw se` (n = towards smaller y, the order of `Battle::neighbors`), or `toward`
  names another actor. The board heading and the card's side (left / right on screen, the mirroring rule of a strike)
  are set at once.
- `say`: one dialogue line in the game's box (DialogScreen: speaker kicker, portrait by the speaker's name, Ok button).
  `translations` follows a dialogue line's convention (`{"en": ...}`, read in Opções' dialogue language); `portrait` names a
  sprite when the speaker's own art is not wanted. The scene goes on when the player dismisses the line.
- `wait`: seconds on the scene clock; everything keeps animating.
- `enter` (2026-11-06, Estranhos na Costa): `{"do": "enter", "actor": "B", "to": [x, y]}`. An actor declared
  `{"unit": "Neera", "offstage": true}` waits off the board from the map's opening: a unit of that name that is not of the party is
  kept out of the fight (not drawn, no turn, blocks nothing, nothing in the save; MapStage `sendOffstage` reads the map's onOpen /
  onVictory scenes) and `enter` puts it on the hex (the nearest free one within 3 if taken), fading in over 0.8 s. A party hero of
  that name is sent off too (2026-11-06) and comes in with its own hit points; if no scene brings it in (a scene refused or
  stopped), `returnOffstageParty` puts it back on its spawn hex once the opening scenes are over, so the fight never loses a
  hero. The map's intro conversation (BattleTalk) waits for the opening scenes. Map Editor: "Entrar" and the actor's "fora do
  tabuleiro".
- `exit` (2026-11-06, Bosque Morto): `{"do": "exit", "actor": "B"}`. A non-party actor leaves the board as a fallen unit does (its
  death pose, then the fade; 1.6 s): no kill, no loot; it waits off the board, so a later `enter` could bring it back. A party
  hero cannot leave by a scene. Map Editor: "Sair".
- `say` with `"speaker": "???"` (2026-11-06): the name shown instead of the unit's; with it the actor may still be off the board (a
  voice before its `enter`). A speaker with no art of its own shows no portrait.
- Scene walks spend no Action Points (2026-11-06): the AP system caps a turn's walk, never a scene's.
- `"camera": "actors"` (top level, optional): while the scene plays the camera's pivot glides to the middle of the actors standing on
  the board (zoom, turn and tilt stay the player's). Scenes without it leave the camera alone, as before.

The first scene that uses both: `estranhos-na-costa`, the Vau's opening (onOpen): Kael climbs the hill over the water on the
empty east half of the south bank, Neera steps out from behind the cottage, Voss stands up the hill; at the end the three head
west towards the ford and the fight begins. Neera and Voss are in the party from the start (overworld `joinIndex` 0).

Bosque Morto (bosque014): `bosque-morto-01-a-menina-no-tronco` (onOpen) and `bosque-morto-02-o-que-a-guardia-deixou` (onVictory).
Lio (`villagerF1`) and Vessa (`crazyLady`) are neutral NPCs of the map with the Inn's own sprites and portraits; both wait off the
board until the opening brings them in (Lio out of the hollow log by (15,14), Vessa at the giant dead oak's foot (7,10)). In the
fight they stand as neutrals (no turn, not targeted by the enemy); the victory scene ends with Vessa's `exit`.

## Film steps (Stone Bridge, 2026-11-06)

The scene can be cut like a film instead of watched from the player's view (the user: "deixar similar a um filme mesmo, não
apenas seguir os personagens por cima"). The camera is the action camera's own (MapStage `ShotKey`: pivot, turn, tilt, distance,
lift), written by the script; the screen effects are drawn over the board, under the dialogue box. All of it is the player's
camera again when the scene ends.

- `shot`: `{"do": "shot", "on": "kael" | [x, y], "between": ["kael", "butcher"], "turn": "e" | 90, "look": "butcher", "behind": "kael",
  "tilt": 12, "distance": 6 | "close" | "medium" | "wide", "lift": 0.38, "over": 2.0, "cut": true, "async": true}`. `on` / `between`
  set the pivot (an actor's feet, a hex, or the middle of two actors); `turn` is the compass heading the camera looks along (`n ne e se
  s sw w nw`, or degrees: 0 north = smaller y, 90 east); `look` turns the camera from the pivot toward that actor; `behind` puts the eye
  on that actor's side of the pivot, 35° off the line (over its shoulder, as the action camera's two-shot); `tilt` in degrees above
  the ground (0 level, 90 straight down, negative looks up at the sky); `distance` a number or `close` (the subject's card fills the
  frame, `closeDistance`), `medium` (1.8 x), `wide` (3.2 x); `lift` a share of the subject's card height (the chest at 0.38; world units
  when the pivot is a hex); `offset` `[x, z]` world units added to the pivot (up to 4 hexes past the board's edge: a shot over the
  river). A field left out keeps what the camera has, so `{"do": "shot", "distance": 3.5, "over": 8}` is a slow
  dolly-in. `over` is the move's seconds (a straight move; 0 = the orbit's own glide), and the step waits for it unless `async`;
  `cut` jumps there. The script's `"cameraBack": false` keeps the last view at the end. The eye never goes under the deck
  (`kMinEyePitch`): "from below" shots are level ones.
- `shake` `{"seconds", "strength"}` (world units, 0.15): the drawn view trembles, dying out over its seconds; the camera itself stays.
- `fade` `{"to": "black" | "clear", "seconds"}`: the scene waits its seconds. A scene that ends black is lifted by its end.
- `bars` `{"on": true | false}`: the cinema's letterbox (12% top and bottom), gliding in and out.
- `tint` `{"percent", "over"}`: the picture darkened. `flash` `{"seconds"}`: a white flash.
- `sfx` `{"file", "volume"}`: an effect from game/SoundFX. `music` `{"file", "volume"}` or `{"stop": true}`: the map's track replaced
  or silenced until the scene ends (then the map's own track again).
- `caption` `{"text", "translations", "style": "plain" | "red" | "blue", "seconds"}`: words over the picture ("…mais tarde" in the
  display face at the centre; the red / blue status boxes high on the screen), the scene waiting their seconds.
- `emote` `{"actor", "text"}`: a mark over the actor's head ("!", "!!", "?", "…"), the fight's own number popup.
- `move` with `"near": "<actor>"` instead of `"to"`: the free reachable hex nearest that actor (its body may be dead).
- An actor declared `{"unit": "Vora", "fallen": true}` may be dead when the scene starts (an onVictory scene): it is found among the
  fallen, and its body never fades while the map is open (`keptBodies_`, read from this map's scenes at the start), so the scene's
  shots have it.

The first scenes cut this way: `stone-bridge-01-o-pedagio` (onOpen) and `stone-bridge-02-a-promessa` (onVictory) on `thebridge`
(thebridge030: Vora the war dog, O Carniceiro as a neutral kept alive through the fight, Lio as a guest hero, the militia named, all but
Ulfred off the board until the opening's end). The shot list they follow is docs/STONE_BRIDGE.md.

The dialogue box (DialogScreen, 2026-11-06, the user's Paint mock-up): high on the screen, its text area keeps room for four lines
whatever the line's length, and a small Ok / Próximo sits in the area's bottom-right corner instead of a bar across the box.

Validation (`world/CutsceneScript.cpp`): wrong `format`, no `id`, no actors, an empty step list, an unknown action, an
undeclared actor, a malformed coordinate, duration, heading or distance refuse the file whole, naming the step.

## What a playing cutscene owns

- The board takes no input (`event()` returns, as under a conversation); the turn loop and free roam hold
  (`holdTurns`, `updateRoaming` skipped); `busy()` is true (a battle script waits for it).
- The grid, the battle bar and the pause menu are not drawn; the camera bar stays. The dialogue box is drawn by
  `drawCutsceneDialog`.
- The picked unit and the previous `holdTurns` are kept and given back at the end (`stopCutscene`); a stop mid-walk leaves
  each actor on the hex its card was stepping onto.
- It reports (`cutsceneDone()` / `cutsceneFailed()` / `cutsceneWhy()` and the log); what happens next is the caller's.

## Map events: `content/cutscenes/triggers.json`

`"onOpen": { "<map id>": "<script id>" | ["<script id>", ...] }`: the script (or the list, one after another in its order; one refused
is skipped with a log line) plays once the map is ready and the loading curtain is gone, every time the
map opens (MapStage `runCutsceneTriggers`; a reload replays it: the map's opening is the safe checkpoint). Not in the Map Editor's
preview, not under a `--cutscene` test run. A battle script waits for the opening scene; a capture with one ends after it. Other
events (a waypoint reached, a victory) are not wired yet: add them here when a scene needs them.

Map Editor, Cutscenes tab (2026-11-05): every scene in the list says where it plays ("Toca ao abrir: <map> (Nª)" or "Sem gatilho"),
the open map's opening order (▲ ▼ Tirar, + Cena), "Toca ao abrir o mapa" per scene, "Testar esta cena" (Testar with only that
scene played as the map opens: MissionEntry::cutscenes -> MapStageOptions::openingCutscenes) and "Testar a abertura deste mapa".

## Testing

```
Sandbox --map build/test-maps/maps/aldeia015.json --ground build/ground/ground-v42.pak --scene build/test-maps/scenes/aldeia015.scene.json
  --sprites build/sprites/battle.pak --meshes build/meshes/props-v8.pak --heroes build/scenes/roster.json
  --battle-rules build/test-maps/battles/aldeia015.battle.json --game-assets content/assets
  --cutscene test-aldeia,!test-bad-actor,!test-bad-command,!test-unreachable,test-aldeia --cutscene-auto 1.5 --capture out.png
```

`--cutscene` plays the listed scripts in turn; `!id` must be refused or fail; `--cutscene-auto S` dismisses each line by
itself after S seconds (a capture has nobody clicking). The run exits 3 if a verdict is wrong. A battle script can also
run one: step `cutscene:<id>` (checked when it is over). The `test-*.json` scripts are temporary test content.
