# The Interlude (step 2: the campaign side)

The historical chapter 25 years before the campaign, in the city the party later explores as the Ruins, played by a party of
its own: **Isolde** (Conjurer, persistent: her levels carry into the campaign when she joins later), **Salazar Jovem** (Cruzado,
a separate character from the present-day Salazar), **Selene** (Conjurer, Interlude only). The user's design (2026-10-25); the
maps, the party's final builds, the scenes and Salazar's quests come from the user. This document is the mechanism.

Files: `engine/world/Interlude.h/.cpp` (the rules), `content/interlude.json` (the data), `apps/sandbox/OverworldStage.h` (the
Ruins' pin and entry), `apps/sandbox/main.cpp` (the party of a fight, the chapter's bookkeeping after a mission),
`apps/sandbox/CampaignUi.h interludeMission` (a chapter map as a mission), `Cooker check-interlude`.

## The data: `content/interlude.json`

- `ruinsLocation`: the world-map location (overworld.json `locations`) whose first entry starts the chapter: `ruins`.
- `requiredQuests`: Salazar's quest ids (overworld.json `quests`) that must all be done (`questCompleted`) before the Ruins open.
  **Empty = shut**: an unconfigured gate never opens by accident. The quests are not written yet, so the Ruins are shut in
  every campaign today. Modo teste opens every location (as always); the developer flag `"interlude:ruins-open"` in
  `save.flags` opens them in a campaign for testing.
- `party`: exactly three heroes: name, classId, starting level, starter weapon, `persistent` (Isolde) / `temporary` (Selene).
- `leader`: who walks on free-roam maps (Isolde).
- `missions`: the chapter's maps in order; each id is a map id in `content/maps/ember` (the newest serial plays; the game
  makes its scene / battle files on first use like any mission). TEMPORARY: `interlude-placeholder` (= the Cruzado's test board
  with the three as its spawns and three foes) until the user's maps arrive.

## The states (`save.interlude.state`)

`NOT_STARTED` (no `interlude` object: every older save), `ACTIVE`, `COMPLETED`. Loading the first map never completes anything:
only `interludeAdvance` past the last mission does.

## The party-context switch

`interludeBegin`: the present party's fields (`interludePartyKeys()`: levels, xp, statPointAllocations, bags, promotions, weapons,
equipped, equipment, looseEquipment, spellUses, ember, unitHp, heroHunger, heroDiseases, heroPoisons, heroSkills, partyLeader,
partyFormation, hotbars, affinityScores, runes, unknown*, rations, hungerStreak, travelTrainingHours, pendingMission, battle)
are copied under `interlude.present` and replaced by the historical party's (its three heroes at their levels, XP 0, a starter
bag each, the starter weapon in hand, 20 rations, Isolde leading); the three get `recruited:<name>` flags while the chapter is
on (so the party rules count them); the first mission is pending. Campaign-wide fields (completed, flags, quests, the clock, the
world position) stay as they are.

`interludeComplete`: Isolde's level, XP, trained skills, attribute points and promotion are kept under `interlude.isolde`; every
moved field is restored exactly as it was (the ones the present did not have are removed again); the three flags are removed;
`pendingMission` / `battle` cleared; state COMPLETED. Young Salazar's and Selene's historical numbers live only in the chapter's
fields and are gone with them (their names never touch `Salazar`'s keys). The whole switch is computed on a copy and assigned at
once: a failed step leaves the save as it was.

`recruitIsolde` (her later story event, not wired to any event yet): her kept progression into `levels / xp / heroSkills /
statPointAllocations / promotions` + the `recruited:Isolde` flag, once (`interlude.isoldeRecruited`). Equipment, consumables,
Ember and statuses do not transfer (the recruitment design decides those).

## In the game

- World map: the Ruins' pin shows locked while the gate is shut (`OverworldStage::locationStatus`); a click says why
  ("As Ruínas estão fechadas: termine antes as missões de Salazar."). Open + NOT_STARTED: `interludeBegin`, the save written,
  the first chapter map's briefing opens. ACTIVE: the chapter's pending map opens (a chapter left half-way, or a save loaded
  mid-chapter, resumes at its map: the pending mission is the checkpoint). COMPLETED: the Ruins open as any present-day
  location (their four missions).
- A fight: `setUpParty` makes the party exactly the chapter's three while ACTIVE (every present hero on the map leaves);
  `setUpExplore` makes Isolde the leader. Levels, XP, bags, gear come from the save's (historical) fields as for any fight.
- After a chapter mission is won (`finishCampaignFight`) or its free-roam map left by the waypoint (`markVisited`):
  `interludeAdvance`: the next map pending, or the chapter complete; then the world map opens with the Ruins' mission list and the
  note "Interlúdio concluído: o grupo voltou ao presente."

## Step 3: the vertical slice (placeholders, 2026-10-25: "cria qualquer placeholder depois crio em cima")

The whole chapter as one playable path. The maps are mine to make (the user, 2026-10-25: "eu não tenho mapas de cidade nenhuma, você
vai criar"); the panorama / ground photos of the city are theirs, later.
1. `interlude-city` (`content/maps/ember/interlude-city003.json`, `tools/scripts/maps/make_interlude_city.py`; 001 / 002 were the
   first cuts and stay unused): the walled city, 60 x 45: a battlement ring with towers and two open gates (south: the party's entry;
   north: the passage out), the ring road, the N-S and E-W avenues crossing at the plaza (fountain, benches, lamps, notice board),
   NW the market square (stalls, carts, well, forge, storehouses), NE the noble quarter (the Salazar family's house facing its garden
   and fountain, knight statues, trees, fine houses), SW the poor quarter (stone huts, firewood, troughs), SE the temple (twin spires
   in an enclosure of temple walls opening west, columns at the mouth, penitent statues, ritual braziers), lamps along the avenues,
   eight townsfolk as neutrals. `content/interlude/city-plan.json` holds the geography (walls, gates, streets, plaza, districts,
   landmarks) the present-day Ruins must follow. Opens with `interlude-city-opening` (Isolde walks up the avenue, the two turn to
   her, three lines). The north gate's passage (floor connector at (29, 2)) marks the map done for the chapter (main.cpp connector
   branch -> markVisited -> interludeAdvance) and opens the fight with the party inside the north gate (arrivedFrom).
2. `interlude-placeholder` (`interlude-placeholder003.json`, the same generator): the same city as a rout mission, the party inside
   the north gate, seven foes on the avenue, the plaza and the temple; opens with `interlude-placeholder-opening` (young Salazar turns,
   two lines); the turn loop holds until the scene ends; winning completes the chapter (finishCampaignFight -> interludeAdvance ->
   interludeComplete) and the world map opens on the present-day Ruins' list.
Blocking on these maps follows the rule of 2026-10-25: only barricades and architecture walls block; houses and props never.
A defeat keeps the fight pending ("Tentar de novo" / the briefing again); a save loaded mid-chapter resumes at its pending map.

## Checks

```
Cooker check-interlude content/interlude.json content/engine2/overworld.json content/engine2/campaign.json
```
runs the cycle on synthetic saves: an old save is NOT_STARTED; an unconfigured gate is shut, Modo teste and the flag open it,
a required quest not done keeps it shut and done opens it; begin puts exactly the three in (the present's numbers gone, `completed`
untouched, the flags set); a second begin is refused; a fight's gains; advance through the missions; complete restores every
party field byte for byte, keeps Isolde's level 4 / 120 XP / skill / points, leaves Salazar untouched and Selene out; begin is
refused for ever after; recruitment once, at her kept level; a save left ACTIVE resumes at its pending mission.

Captures: `--title --title-screen map --save-bank <copy> --map-ui enter:ruins` (the gate and the start on a copy of a bank),
`--title --title-screen mission:interlude-placeholder --save-bank <that copy>` (the chapter's fight with the three).
