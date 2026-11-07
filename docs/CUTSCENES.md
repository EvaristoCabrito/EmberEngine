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

Validation (`world/CutsceneScript.cpp`): wrong `format`, no `id`, no actors, an empty step list, an unknown action, an
undeclared actor, a malformed coordinate or duration refuse the file whole, naming the step.

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
