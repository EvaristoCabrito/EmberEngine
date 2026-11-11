# O Farol / the Light House (2026-11-11)

The user: "create a new dungeon the Light House complete with new 3D Objects, underground levels and a boss talk scene at the top of
the lighthouse. Use some of the texture archives we have been building over time." Picked the same day: the Embered Wraith as the
boss, a film cutscene and then the fight, five maps, three groups of 3D objects (lamp and lens, machinery, sea and dock pieces).
Later: "you can make me prompts for new tile sets and textures youre missing" and "add instructions for an artist agent to finish
applying the textures as he makes them".

World map: the pin `lighthouse` (hex 1, 3), open access, entry mission `farol`, the other four as floors.

| id | what | size | kind |
|---|---|---|---|
| `farol` | the headland: sea north and west, the dock, the keeper's cottage, the fog bell, the tower's ground floor (hall, stair up, LOCKED door to the cellar stair) | 48 x 42 | under the sky, rooms roofed (like the Mosteiro's ground floor) |
| `farol-andar-2` | Floor II: keeper's room, oil store (the apprentice), machine hall with the clockwork | 26 x 25 | Interna acima do solo |
| `farol-topo` | the lantern room: the great lamp, O Faroleiro and his drowned watch | 22 x 21 | battle map (`win: rout`, not explore), the two scenes |
| `farol-poroes` | level -1: stair hall, oil cellar, the wreckers' LOCKED store, cistern gallery, the hold (the castaway), the LOCKED descent | 36 x 26 | Subterrâneo |
| `farol-gruta` | level -2: the sea cave: inlet, the wreckers' landing, the beach chamber, the hoard and the troll | 42 x 30 | Subterrâneo, rock = `void`, sea = `water`, no built walls |

Generator: `python -I tools/scripts/maps/make_lighthouse.py [ground|2|top|cellars|cave] [--dry]` (next serial of each id, never an
overwrite; `--dry` runs the checks and prints the plan only). It runs on the Mosteiro's builder, `monastery_lib.py`, which now takes a
wall kit by name (`Floor(..., kit_name='farol')`) and refuses pieces and people on water / void. Location:
`python -I tools/scripts/data/add_lighthouse_location.py` (idempotent; re-run it after a new serial so `mapFile` follows).

The story the maps tell: the keeper, Anselmo, never let the light go out. When the oil ran out in the ash storm he brought a
glowing stone up from the cave and set it in the lantern. He died last winter; the light came back the same night, orange, and it
wanders. Ships follow it onto the rocks and the crews walk back up the beach. People who talk: Marta (the road), the Velho
Pescador (the cottage), the Aprendiz (Floor II, oil store), the Náufrago (the cellars' cage).

## The boss scenes (`content/cutscenes`, wired in `triggers.json`)

- `farol-01-a-luz-que-chama` (onOpen of `farol-topo`): letterbox, the lamp out of black, Kael walks (11, 14) -> (11, 12), the
  talk with O Faroleiro (he turns at "Enterraram o senhor"), the flash, then the fight. Actors: only Kael and O Faroleiro, so the
  scene never depends on who else is in the party. Every hero has a spawn off Kael's lane.
- `farol-02-a-ultima-vigilia` (onVictory): the body, the lens' last turn (a caption), Kael's three lines, fade.
- `battle-lines.json`: two lines for `farol-topo` (battleStart by O Faroleiro, allyBadlyWounded by Kael).

Limits: the Embered Wraith is an ordinary class (no `boss` flag, rank F by default in `content/enemy-ranks.json`), so the map is
won by rout and his strength is the class's own; the Cooker's export gives enemies the mission's level, not the spawn's. A boss
class or a higher rank for him is the user's call. The lamp cannot go dark when he falls (a model has one state).

## The 3D kit (`content/models/lighthouse`, section "Farol" in the editor)

`blender -b -t 4 -P tools/scripts/models/create_lighthouse_kit_v01.py`, then `register_lighthouse_kit_v01.py`. Review renders:
`work/lighthouse-kit-v01-preview/` (`render_lighthouse_kit_preview.py`). Cooked into `build/meshes/props-v45.pak` (JOGAR uses it).

| id | what | tris |
|---|---|---|
| `ember-lighthouse-lamp` | the lantern: cast-iron pedestal, brass gear table with hand crank, stacked glass prisms glowing, brass cage and vented crown; lights the room (orange, radius 9) | 14 296 |
| `ember-lighthouse-clockwork` | oak frame, rope drum and weight, brass wheels, escape wheel, pendulum, crank | 7 660 |
| `ember-lighthouse-oil-tank` | riveted copper tank on an iron cradle, feed pipe, tap, sight glass, oil can | 4 856 |
| `ember-lighthouse-fog-bell` | bronze bell with wall thickness in an oak frame, yoke, clapper, pull rope | 3 292 |
| `ember-lighthouse-rowboat` | clinker hull, ribs, thwarts, oars, oarlocks, painter, on chocks (2 hexes) | 6 324 |
| `ember-lighthouse-anchor` | iron anchor with a banded wooden stock, laid down, its chain | 9 332 |
| `ember-lighthouse-mooring-post` | timber bollard, iron bands and pin, hawser and coils | 4 796 |
| `ember-lighthouse-net-rack` | pole trestles, a diamond-mesh net with corks and leads, three glass floats (2 hexes) | 20 820 |
| `ember-lighthouse-lobster-pots` | three lath-and-hoop pots with netted ends, buoy line and cork | 7 928 |

Photographs used, all from the archives: ambientCG `metal-024`, `planks-037a`, `wood-060`, `lava-001`; the user's `wt-iron`,
`wt-dark-oak`, `brass-detail-v01`, `lamp-glass-v01` + `lamp-glass-glow-v01`, `dark-green-glass-photo-v01`, the hemp rope.

Wall kit: `ember-arch-farol-*` (15 pieces: copies of the Mosteiro's thirteen plus `-round-wall` and `-round-wall-torch`,
`register_lighthouse_architecture.py`) with the Watchtower's stone until `farol-wall-v01.jpg` arrives. `MapStage.h` cuts these
walls away like the Mosteiro's.

**Round walls above ground, square walls underground (the user, 2026-11-11: "you need to make round walls for this one, only
square walls go underground").** The tower on `farol`, `farol-andar-2` and `farol-topo` is a true disc of hexes
(`make_lighthouse.py` `disc()`: every hex whose centre is within the radius; its wall is the one-hex ring, each piece with exactly
two neighbours, checked). The ring is laid with the round pieces: `ArchitectureDef::round` (`Architecture.h`, read from
`architectureDefs.round`), and `emberGeometry` draws such a piece as one smooth curve from the midpoint towards one neighbour to
the midpoint towards the other, bent at the hex's centre, in place of the square post and two straight arms. A round piece with
one neighbour, three (where a partition meets the ring) or none is the plain wall. Windows are straight pieces, so they stand only
where the ring runs straight (the top and bottom rows); the lantern room's glazing is those two rows for now. Doors likewise
(the bottom row). The partitions inside the tower, the cottage and the cellars are the square kit. Maps: `farol002`,
`farol-andar-2002`, `farol-topo002` (the 001s are the octagon drafts, never tested).

## What is a stand-in today

| where | today | wanted (prompt below) |
|---|---|---|
| every `ember-arch-farol-*` wall | `walls/watchtower-v01.jpg` | 1 · `farol-wall-v01.jpg` |
| tower floors (`farol-andar-2`, `farol-topo`, the rooms of `farol`) | tile set `torre-v01` | 2 · `farol-piso-v01` |
| `farol-poroes` | tile set `watchtower-foundations-v01` | 3 · `farol-porao-v01` |
| `farol` outside: grass / shore rock / path | the stock plains / ruins / nave tiles | 4, 5, 6 · `farol-relva`, `farol-costa`, `farol-caminho` |
| `farol-gruta`: floor / wet margin / rock face | tile set `cave-v01` (the Misty Cave's) | 7, 8, 9 · `farol-gruta-chao`, `-margem`, `-parede` |
| `farol` backdrop | none | 10 · `farol-panorama-v01.jpg` |

## Prompts (deliver in `Attachments/Lighthouse/` under these names, JPG or PNG at full quality)

### 1 · Pedra da torre: `farol-wall-v01.jpg` (todas as paredes do kit; 2048 x 2048, emenda nos 4 lados)

```
A photograph of a square of the outer wall of an old sea lighthouse, photographed square-on with the lens axis perpendicular to the wall, the frame filled edge to edge by the stonework alone, the courses running horizontally. Large coursed blocks of dark grey granite, each a different length, laid in tight beds with narrow lime mortar joints gone grey-white, the faces rough-dressed with a point chisel and rounded by a century of salt wind. Photographed with a 100mm macro lens on a full-frame sensor at f/11, focus stacked so the whole square stays sharp, lit by a large diffused softbox from the upper left standing in for cold overcast daylight, with a faint warm orange fill from the lower right as if from a lantern below. Construction and history: white salt bloom crusted along the lower edge of every block and in every joint; old limewash surviving as thin pale ghosts on a third of the faces; rust streaks weeping in stains of different ages from two iron cramps set flush in the stone; a fine veil of grey ash settled into every tooling mark; one hairline crack crossing a block and stopping at the joint; grey-orange lichen in small flat rosettes on two blocks; the wear the same density across the whole square. Surface truth at close range: feldspar and quartz grain catching the key light, micro-pitting across every face, mortar crumbled to sand deep in the joints, salt crystals standing as a granular crust, the high points of the blocks rubbed faintly smooth. Natural sensor grain, true-to-life colour response, gentle highlight rolloff, raw file neutral grade, the frame reading as a photograph of a real wall. The image is a seamless tile: its left edge continues into its right edge and its top edge into its bottom edge, every block, joint and stain that reaches a border carrying on from the opposite border at the same place, the courses meeting exactly, the tone one even level across the whole square.
```

### 2 · Piso da torre: `farol-piso-v01.jpg` (nave dos andares da torre; 2048 x 2048, emenda nos 4 lados)

```
A photograph of a square of the stone floor inside an old sea lighthouse, photographed straight down with the lens axis perpendicular to the floor, the frame filled edge to edge by the paving alone. Wide flagstones of dark grey slate laid in straight courses, each slab a different length, the joints narrow and dark, the slabs dished by a century of boots along the middle of each course and their edges rounded. Photographed with a 100mm macro lens on a full-frame sensor at f/11, focus stacked so the whole square stays sharp, lit by a large diffused softbox from the upper left standing in for dim window light, with a faint warm orange fill from the right as if from a lantern. Construction and history: old lamp oil soaked into the stone as dark soft-edged stains in four places; fine grey ash swept into the joints and the hollows; a pale scuffed track where something heavy was dragged; two brass screws and a short length of frayed hemp trodden flat into a joint; rust rings left by the feet of iron drums; the wear the same density across the whole square. Surface truth at close range: the slate's cleavage lines and mica specks catching the key, grit in the joints, the worn tops faintly polished, the oil stains with a dull sheen, loose ash lying as a granular layer with individual particles readable. Natural sensor grain, true-to-life colour response, gentle highlight rolloff, raw file neutral grade, the frame reading as a photograph of a real floor. The image is a seamless tile: its left edge continues into its right edge and its top edge into its bottom edge, every slab and joint that reaches a border carrying on from the opposite border at the same place, the tone one even level across the whole square.
```

### 3 · Piso do porão: `farol-porao-v01.jpg` (nave dos porões; 2048 x 2048, emenda nos 4 lados)

```
A photograph of a square of the floor of a damp cellar cut under a sea lighthouse, photographed straight down with the lens axis perpendicular to the floor, the frame filled edge to edge by the floor alone. Rough-hewn bedrock levelled with a pick, patched with irregular slabs of dark stone and bands of old brick laid flat, everything wet. Photographed with a 100mm macro lens on a full-frame sensor at f/11, focus stacked so the whole square stays sharp, lit by a large diffused softbox from the upper left standing in for a single torch far away, with a faint orange fill from the right. Construction and history: shallow films of standing water in the lowest places reflecting the key as dull patches; white salt lines marking where old puddles dried; black oil stains spread wide and thin; green-black slime in the joints of the brick; rust bleeding from an iron ring bolt set in the rock; scattered barnacle shells and a strand of dried kelp carried in by the tide; fine grey ash floating as a skin on the water films; the wear the same density across the whole square. Surface truth at close range: pick marks in the rock, the brick's sandy face spalling, salt as a granular crust, the wet stone dark and glossy against the dry stone matte and pale. Natural sensor grain, true-to-life colour response, gentle highlight rolloff, raw file neutral grade, the frame reading as a photograph of a real floor. The image is a seamless tile: its left edge continues into its right edge and its top edge into its bottom edge, every slab, brick and stain that reaches a border carrying on from the opposite border at the same place, the tone one even level across the whole square.
```

### 4 · Relva do promontório: `farol-relva-v01.jpg` (plains do mapa externo; 2048 x 2048, emenda nos 4 lados)

```
A photograph of a square of ground on a windswept sea headland in the world of Ember Ashes, a burnt-out land under permanent ashfall, photographed straight down with the lens axis perpendicular to the ground, the frame filled edge to edge by the ground alone. Short dead coastal turf, bone-white and straw-grey, every blade combed flat in one direction by the wind, growing thin over dark stony soil that shows through in a third of the frame. Photographed with a 100mm macro lens on a full-frame sensor at f/11, focus stacked so the whole square stays sharp, lit by a large diffused softbox from the upper left standing in for cold overcast daylight. Construction and history: grey ash lying in the lee of every tussock as small drifts; angular chips of dark granite the size of a thumbnail scattered evenly; dried thrift flower heads gone brown; a few white fragments of broken shell dropped by gulls; salt crust on the bare soil; the same density of turf, stones and ash across the whole square. Surface truth at close range: individual blades with split tips, the soil's grit and fine roots, ash as a loose granular layer with individual particles readable at the edge of each drift, shell fragments with a pearly inner face. Natural sensor grain, true-to-life colour response, gentle highlight rolloff, raw file neutral grade, the frame reading as a photograph of real ground. The image is a seamless tile: its left edge continues into its right edge and its top edge into its bottom edge, every tussock and stone that reaches a border carrying on from the opposite border at the same place, the tone one even level across the whole square.
```

### 5 · Rocha da costa: `farol-costa-v01.jpg` (ruins do mapa externo: a faixa junto ao mar e o cais; 2048 x 2048, emenda nos 4 lados)

```
A photograph of a square of wave-worn shore rock at the foot of a sea cliff in the world of Ember Ashes, photographed straight down with the lens axis perpendicular to the rock, the frame filled edge to edge by the rock alone. Dark basalt bedrock planed nearly flat by the sea, broken by a net of shallow cracks into plates the size of a hand to the size of a table. Photographed with a 100mm macro lens on a full-frame sensor at f/11, focus stacked so the whole square stays sharp, lit by a large diffused softbox from the upper left standing in for cold overcast daylight. Construction and history: clusters of grey-white barnacles and limpets on a quarter of the plates; black dried seaweed pressed into the cracks; small pockets holding a film of water with grey ash floating on it; white salt drying in rings round each pocket; rust-orange iron staining along two cracks; coarse dark sand and shell grit collected in the low places; the same density of marks across the whole square. Surface truth at close range: the basalt's fine vesicles and micro-pitting, the barnacles' ribbed plates, wet rock glossy and dark beside dry rock matte and grey, sand grains individually readable. Natural sensor grain, true-to-life colour response, gentle highlight rolloff, raw file neutral grade, the frame reading as a photograph of real rock. The image is a seamless tile: its left edge continues into its right edge and its top edge into its bottom edge, every crack and plate that reaches a border carrying on from the opposite border at the same place, the tone one even level across the whole square.
```

### 6 · Caminho de pedra: `farol-caminho-v01.jpg` (nave do mapa externo: o caminho da estrada à torre; 2048 x 2048, emenda nos 4 lados)

```
A photograph of a square of an old cobbled path leading to a sea lighthouse, photographed straight down with the lens axis perpendicular to the ground, the frame filled edge to edge by the paving alone. Rounded beach cobbles of grey granite and dark basalt, fist-sized, set tight on edge in packed earth in rough rows, their tops worn flat. Photographed with a 100mm macro lens on a full-frame sensor at f/11, focus stacked so the whole square stays sharp, lit by a large diffused softbox from the upper left standing in for cold overcast daylight. Construction and history: grey ash packed into every gap between the stones; two shallow cart ruts crossing the square as slightly lower, more polished bands; dead bone-white grass stems lying flat in a few joints; a scatter of coarse sand blown over one side; salt bloom on the darker stones; the same density of stones and wear across the whole square. Surface truth at close range: quartz grain catching the key on the granite, the basalt's fine pits, the polished tops against the rough flanks, ash as a loose granular layer with individual particles readable. Natural sensor grain, true-to-life colour response, gentle highlight rolloff, raw file neutral grade, the frame reading as a photograph of a real path. The image is a seamless tile: its left edge continues into its right edge and its top edge into its bottom edge, every cobble that reaches a border carrying on from the opposite border at the same place, the tone one even level across the whole square.
```

### 7 · Chão da gruta: `farol-gruta-chao-v01.jpg` (plains da gruta; 2048 x 2048, emenda nos 4 lados)

```
A photograph of a square of the floor of a sea cave under a cliff, photographed straight down with the lens axis perpendicular to the floor, the frame filled edge to edge by the floor alone. Dark wet basalt bedrock, smoothed by tides into low rounded swells, its hollows filled with coarse black sand and fine shingle. Photographed with a 100mm macro lens on a full-frame sensor at f/11, focus stacked so the whole square stays sharp, lit by a large diffused softbox from the upper left standing in for weak daylight from the cave mouth, with a faint orange fill from the right as if from an ember lantern. Construction and history: strands of dark kelp left by the last tide; broken mussel and limpet shells in the sand; a few pale splinters of ship's timber and one corroded iron nail; white salt lines on the rock above the sand; a thin drift of grey ash carried in on the water and stranded at the sand's edge; the same density of rock, sand and wrack across the whole square. Surface truth at close range: the basalt glossy where wet, the sand's individual grains and shell flakes, the kelp's leathery blades, salt as a fine granular crust. Natural sensor grain, true-to-life colour response, gentle highlight rolloff, raw file neutral grade, the frame reading as a photograph of a real cave floor. The image is a seamless tile: its left edge continues into its right edge and its top edge into its bottom edge, every swell of rock and pocket of sand that reaches a border carrying on from the opposite border at the same place, the tone one even level across the whole square.
```

### 8 · Margem molhada: `farol-gruta-margem-v01.jpg` (snow da gruta: a areia em volta da enseada; 2048 x 2048, emenda nos 4 lados)

```
A photograph of a square of wet tidal sand at the water's edge inside a sea cave, photographed straight down with the lens axis perpendicular to the sand, the frame filled edge to edge by the sand alone. Fine dark grey sand saturated with water, its surface drawn into shallow parallel ripples by the retreating tide, a thin film of water lying in the troughs. Photographed with a 100mm macro lens on a full-frame sensor at f/11, focus stacked so the whole square stays sharp, lit by a large diffused softbox from the upper left standing in for weak daylight from the cave mouth, with a faint orange fill from the right. Construction and history: a wavering line of pale foam and grey ash left at the last wave's reach; small rounded pebbles half sunk in the sand; shell fragments and two whole mussel shells; a single boot print softened by water; a short piece of tarred rope; the same density of ripples and wrack across the whole square. Surface truth at close range: individual sand grains with mica flecks catching the key, the water film mirroring the light in the troughs, foam bubbles with dry ash caught on them, the pebbles glossy and dark. Natural sensor grain, true-to-life colour response, gentle highlight rolloff, raw file neutral grade, the frame reading as a photograph of real sand. The image is a seamless tile: its left edge continues into its right edge and its top edge into its bottom edge, every ripple that reaches a border carrying on from the opposite border at the same place, the tone one even level across the whole square.
```

### 9 · Parede da gruta: `farol-gruta-parede-v01.jpg` (side da gruta: as faces de rocha do tabuleiro; 2048 x 2048, emenda esquerda-direita)

```
A photograph of a square of the rock wall of a sea cave, photographed square-on with the lens axis perpendicular to the wall, the frame filled edge to edge by the rock alone. Dark columnar basalt in broken vertical ribs, wet from top to bottom, the lower third below the tide line. Photographed with a 100mm macro lens on a full-frame sensor at f/11, focus stacked so the whole square stays sharp, lit by a large diffused softbox from the upper left standing in for weak daylight from the cave mouth, with a faint orange fill from the lower right as if from an ember lantern on the floor. Construction and history: a dense band of barnacles and black mussels along the lower third; above it a band of green-black weed hanging straight down; above that bare rock streaked with white salt runs and rust-orange mineral seep from the joints; fine grey ash caught on every small ledge; water beading and running in thin lines. Surface truth at close range: the basalt's fine vesicles, the barnacles' ribbed plates, the weed's wet sheen, salt as a crystalline crust, droplets catching the key. Natural sensor grain, true-to-life colour response, gentle highlight rolloff, raw file neutral grade, the frame reading as a photograph of a real cave wall. The image tiles sideways: its left edge continues into its right edge, every rib and band meeting at the same height on both sides, the tide line level across the whole width.
```

### 10 · Panorama do mar: `farol-panorama-v01.jpg` (fundo do mapa externo; 8192 x 2048, a linha do horizonte a meia altura)

```
A photograph, very wide panoramic format 4:1, of the open sea at night seen from the top of a low headland in the world of Ember Ashes, a burnt-out land under permanent ashfall, the horizon exactly at the vertical centre of the frame, the lighthouse itself outside the frame. A heavy slow swell of black water running toward the camera, white foam breaking on a scatter of low black rocks in the middle distance, the masts and ribs of two wrecked ships standing among those rocks; far along the coast on the right a line of burnt cliffs fading into haze. The sky is low unbroken cloud, slate blue-grey, lit from below by a dull orange glow coming from behind the camera, with a paler band of cold light along the horizon where the cloud thins; fine ash falling through the whole depth and catching that orange light as slow bright specks. Photographed with a 35mm lens on a full-frame sensor at f/8 from a tripod, a long exposure of several seconds that smooths the far water into mist and leaves the near foam streaked, the exposure set for the sky so the sea sits a stop dark. Natural sensor grain, true-to-life colour response, gentle highlight rolloff, raw file neutral grade, the frame reading as a photograph of a real place. The foreground at the bottom of the frame is flat dark rock with ash on it, so it can sit under a game board; the left and right edges of the image continue into each other seamlessly.
```

## Instructions for the artist agent (apply each texture as it arrives)

Rules that hold for every step: read `AGENTS.md` first (the Bosque Morto ground is locked); never overwrite a file, a new look is a
new `-vNN`; never start the game or a capture (the user tests); one build at the end with 2 cores, never while the game is open;
show the user a picture of the result (a 2 x 2 tiling preview, a contact sheet) and say what was not checked.

**A. A seamless square arrives (prompts 1 to 9).** Check it tiles before using it:
`python -I tools/scripts/ground/make_tile.py "Attachments/Lighthouse/<file>" content/ground/<name>.png --preview work/lighthouse-tiles/<name>-2x2.png`
(the tool cuts the middle square and closes the seam; look at the 2 x 2 preview; `--flatten` when the light is uneven). For the
wall (prompt 1) the output goes to `content/textures/walls/farol-v01.jpg` instead (convert to JPG, quality 95).

**B. The wall (1).** In `tools/scripts/models/register_lighthouse_architecture.py` set `TEXTURE = '/ember/textures/walls/farol-v01.jpg'`
(and `REPEAT` so one image covers about 3.2 units: start at 3.2), add `"farol-v01"` to `textures` in
`content/models/architecture.json`, run the script, then cook the wall textures:
`build\release\bin\Cooker.exe architecture content\data\editor-catalog.json content build\meshes\architecture-new.pak --cache build\meshes\cook-cache-architecture`.
Keep the old pak beside it with the date (as the `architecture-2026....pak` files there) and put the new one in its place only
when the game is closed. No map changes: every piece already names the kit.

**C. The floors (2 to 9): three tile sets in `engine/world/GroundAssets.h` (`kEmberGroundTiles`).** Follow the `mosteiro-v01` rows
exactly (set name, terrain, asset id, project file, tile size in units, `true` = over the terrain's baked art):

```
{"farol-v01", "nave", "ember-farol-piso-v01", "content/ground/farol-piso-v01.png", 3.6f, true},
{"farol-v01", "plains", "ember-farol-relva-v01", "content/ground/farol-relva-v01.png", 4.5f, true},
{"farol-v01", "ruins", "ember-farol-costa-v01", "content/ground/farol-costa-v01.png", 4.5f, true},
{"farol-porao-v01", "nave", "ember-farol-porao-v01", "content/ground/farol-porao-v01.png", 4.2f, true},
{"farol-gruta-v01", "plains", "ember-farol-gruta-chao-v01", "content/ground/farol-gruta-chao-v01.png", 4.5f, true},
{"farol-gruta-v01", "snow", "ember-farol-gruta-margem-v01", "content/ground/farol-gruta-margem-v01.png", 4.0f, true},
{"farol-gruta-v01", "side", "ember-farol-gruta-parede-v01", "content/ground/farol-gruta-parede-v01.png", 3.0f},
```

A set may go in with only the tiles that exist; a terrain without a row keeps its stock art. The headland uses one `nave` for both
the tower's rooms and the path: until the path (6) has its own terrain, `farol-piso` covers both; to give the path its cobbles,
change the path's rects in `make_lighthouse.py` `ground()` from `'nave'` to `'hill'` only if the user agrees to the hill's rules
there (Mov 2), otherwise leave it and say so. Check how `mosteiro-v01` reached the game before assuming more is needed
(`git log -S"mosteiro-v01" -- engine/world/GroundAssets.h`): if that commit also cooked a ground pak, do the same with
`Cooker ground`; this was not verified when this document was written.

Then point the maps at the sets, in `tools/scripts/maps/make_lighthouse.py`: `INDOOR` `emberTiles='farol-v01'`; `ground()` add
`emberTiles='farol-v01'`; `poroes()` `emberTiles='farol-porao-v01'`; `gruta()` `emberTiles='farol-gruta-v01'`. Run the generator
for those floors (new serials), then `add_lighthouse_location.py`. If the user has saved any of these maps in the editor since
(a serial higher than the generator's last), do not regenerate: change `emberTiles` in a copy of their newest serial instead and
save it as the next one.

**D. The panorama (10).** Copy to `content/backdrops/farol-panorama-v01.jpg`, register it as a background in the editor catalogue
the way the other `scene-*` entries are (`editor.backgrounds`), and add the map's entry to `content/data/backdrop-fit.json`.
The panorama rule: an entry is locked once the user approves it; change it only when asked.

**E. Build and report.** One `build.bat` run limited to 2 cores, only with the game closed, only when a `.h` / `.cpp` changed
(step C). Then tell the user which textures are in, which are still
stand-ins (the table above), and that the look in the game is theirs to judge.

**F. The 3D kit's own textures.** If the user draws a photograph for a part (brass, lens glass, boat planks), put it in
`content/models/textures/` with a `-v01` name, point the matching `MATERIALS` entry in `create_lighthouse_kit_v01.py` at it, bump
`VERSION` to `v02` (new files beside v01), run the script, the preview render and `register_lighthouse_kit_v01.py` after pointing
its ids at the v02 files, stage the GLBs in a new `build/meshes/src-vNN/ember/lighthouse/` and cook the next `props-vNN.pak`.
