# EmberEngine — Architecture

The rule for every system in this engine: **size it for the finished game on day one.** No fixed caps,
no "good enough for now" formats, no system that has to be rewritten when the content arrives.
Every number below was measured from the game's real data, not guessed.

---

## 1. Where Engine2 broke, measured

| What | Measured (Engine2, 2026-10-14) | Why it broke |
|---|---|---|
| One character, `neera` | 289 PNG frames, 434×896 px | Decoded to RGBA8 for the GPU: **1.56 MB per frame → ~449 MB** for one character, with about half its animations made |
| All character sprites | `sprites/` 1.7 GB + `sprites-e2/` 1.9 GB of PNG on disk, 97 characters | Everything loaded up front, decoded on the CPU, uploaded raw |
| Largest map | 64×52 hexes (`frozen-tundra-crossing003`), median 26×20 | Whole map lived in memory as one blob; no streaming |
| Ground | one blend weight per tile per vertex | Hard cap on terrain types per area |
| Spell effects (the user, 2026-10-14) | stutters on any spell effect | |
| Sprite animation (the user, 2026-10-14) | mostly subpar | |

The planned game is far past this. **Finished-game sprite workload (the user's figures, 2026-10-14):**

- **A. Each main character (MC) carries 2× today's sprite data** (remaining animations, layered
  expressions, cutscene-exclusive sprites), at full source resolution.
- **B. Today's roster is ~10% of the finished game.** The ×10 test pack (today's roster cooked 10 times
  under distinct names) already includes A for every MC represented today. It must not be doubled again.
- **C. MCs come and go from the group: at most 8 in the party at once,** varied characters rotating
  through. Asked for a count of MCs beyond today's roster, the user's answer was "keep 10": the ×10 pack is
  the finished-game capacity target. What a battle or cutscene needs at one moment is set by the 8-MC
  party plus enemies on screen, not by the total roster.

| What | Today (measured) | Finished game = ×10 (cooked test pack) |
|---|---|---|
| Sprite clips | 375 | 3,750 |
| GPU data, full mip chains (BC7) | 2.63 GB | 26.3 GB (streamed, never all resident) |
| Always-resident tails | 19 MB | 192 MB |
| Cooked on disk | 934 MB | 9.34 GB |
| `neera` raw RGBA8 (Engine2's way) | ~449 MB | |

Plus sprites at full source resolution, and maps/dungeons many times the largest Ember map.
No GPU holds that all at once, so streaming is not an optimisation here; it is the only way the game runs.
The engine has to treat **memory as something it manages**, not something it hopes is big enough.

## 2. Core principles

1. **Nothing raw at runtime.** PNG, GLB, WAV are *source* formats. An offline **cooker** turns them into
   GPU-ready formats (BC7/BC5/BC6H textures, meshlet buffers, compressed audio) packed into archives.
   The game never decodes a PNG.
2. **Everything streams.** Every asset has a handle, a residency state and a priority. A residency manager
   keeps GPU and CPU memory under a budget, evicting least-recently-used data. Loading a map or cutscene
   is just "raise the priority of these assets".
3. **Always something to show.** Each texture keeps a tiny low-resolution version permanently resident, so
   a missing asset shows blurred for a few frames instead of a hitch or a hole.
4. **GPU-driven rendering.** The CPU does not issue one draw per object. Object lists live on the GPU; compute
   shaders cull and build the draw lists (already enabled: bindless descriptors, multi-draw-indirect,
   draw-indirect-count, buffer device address).
5. **Budgets are enforced by tools, not discovered by players.** The cooker reports size per asset; the
   engine has an on-screen memory/frame-time overlay from the first milestone that has content.
6. **Data-oriented core.** Contiguous arrays, handles instead of pointers, job system across all CPU cores.
7. **Libraries, never an engine.** Open-source libraries for solved problems (window, physics, mesh
   optimisation, texture compression, UI); the engine itself is ours. Every version is checked against the
   project's real latest release when it is added.
9. **The vision is the standard** (the user, 2026-10-14). Build toward the 2.75D image the user already
   sees, never shrink it to fit a first implementation. Prototypes test the actual vision (a lighting
   prototype shows sprites, depth and environment interacting, not a light on a flat sprite); performance
   tests measure the real game's workload; when an approach reaches its limits, a better architecture is
   researched before the art is reduced.
8. **Simple tools on top.** The editor hides the complexity: drag a character in, paint terrain, place a
   house. The person using it never sees a mip level or a meshlet.

## 3. Layers

```
apps/        Sandbox (test bed) · Editor · Game · Cooker (command-line)
gameplay/    ECS world, battle rules, campaign, cutscene timeline, AI
world/       world cells, streaming, terrain, hex gameplay grid, navigation
render/      frame graph, GPU scene, passes (geometry, sprites, lighting, sky, fog, post)
assets/      handles, residency manager, async file IO, archive (.pak) reader
rhi/         Vulkan 1.3: device, bindless, pipelines, memory, upload queues        ← exists (M0)
platform/    window, input, gamepad, file system, timers                           ← exists (M0)
core/        logging, allocators, containers, job system, math, profiling          ← partly exists
tools/       cooker library (shared by Cooker app and Editor hot-reload)
```

Dependencies only point downward. `rhi` knows nothing about sprites; `render` knows nothing about battles.

## 4. Assets and streaming

- **Archive format:** cooked assets in large `.pak` files, each asset a compressed block with a header
  (type, size, dependencies, mip/clip table). Read with asynchronous Windows I/O (IoRing / overlapped
  reads), many requests in flight, decompressed on job threads.
- **Handles:** 64-bit asset ids from the source path; the game references assets by id, never by file.
- **Disk order (built in M1):** within one priority, loads are read in file order (an elevator sweep
  that wraps around), and neighbouring assets merge into one read of up to 1 MB. Measured: 10,000
  small assets arrive in 211 reads instead of 10,000. Sending the drive thousands of small random
  reads instead made throughput swing between 250 and 1,100 MB/s from run to run on the same NVMe.
- **Streaming memory (built in M1):** read buffers come from a pool of fixed blocks taken from the OS
  once and recycled; loaded assets live in their own private heap. Before this, frames spiked to 8 ms
  while 31 threads fought the general allocator's locks.
- **Residency manager:** per-pool budgets (GPU textures, GPU geometry, CPU audio), priority from distance /
  "this cutscene is next" hints, LRU eviction, uploads through a dedicated transfer queue with a per-frame
  upload cap so streaming never stalls a frame.
- **GPU texture pools (built in M2):** streamed textures are created inside a pool allocated once at
  load (budget + one 64 MB block of slack), never with fresh driver memory: a fresh allocation took up
  to 12 ms on the main thread. When the pool has the bytes but no hole big enough, eviction frees a run
  of neighbouring textures (least recently drawn first) instead of scattered ones. Measured with a
  512 MB budget: blind LRU eviction in the same pool left 150 frames drawing a tail late; hole-making
  eviction left none, matching a pool 50% over budget. Uploads are capped at 8 MB per frame
  (480 MB/s at 60 Hz); 24 MB per frame cost up to 4.5 ms of staging copies per frame.
- **Hot reload (dev):** the editor watches source folders, re-cooks a changed file and swaps it in live.

## 5. Character sprites (the system Engine2 could not carry)

Characters are 2D (photoreal, from video), and so are some props (painted or photographic), mixed with real 3D objects (user, 2026-10-14). 2D art gets first-class support: the same lit-sprite path serves characters and 2D props.

**Source = the animation videos (user, 2026-10-14).** The user's videos have the motion they want;
Engine2's sprites lost it in three ways: each pose keeps a fixed 36 frames whatever its length (a
4.75 s idle becomes ~7.6 fps), each frame is cut out of its background separately (edges shimmer), and
the body is recentred every frame while the game slides the unit at an unrelated speed (feet glide).
EmberEngine's video-to-sprite pipeline therefore:
- keeps **every frame** at the video's own rate, so playback matches the video exactly;
- cuts the character out with a **temporally consistent video matting** model (chosen after research);
- finds **loop points** automatically and blends across them;
- measures **root motion**: the planted foot must not move relative to the ground, so its slide gives
  the true travel per frame. Each clip stores that track, and the game moves units by it. Feet stay
  locked to the ground; a different travel speed changes the playback rate, never the slide;
- optionally interpolates frames (48/60 fps), judged side by side against the video before it is used.
Videos live in `content/video/<Character>/<move>.mp4`.

**Measured roster (2026-10-14, Engine2's 305 poses, 8,127 frames in use):** 3.2 G pixels at native size,
1.86 G after trimming. Uploaded raw like Engine2 did: **9.25 GB** with mips. Cooked to BC7 with mips:
**2.31 GB**; the finished game is ×10 of the full cooked roster (2.63 GB, §1), **26.3 GB**. No consumer GPU holds the finished roster at
once, which is why residency is per clip. Largest single clip today: ~37 MB (BirolhoLegs2 cast, 36 frames
of 1249×655).

**Cooking** (per character, per animation clip):
1. **Trim** to the union of the clip's visible pixels (+4 px), the same rule Engine2 uses: every frame
   keeps its place and the pivot never moves. Dimensions round up to a multiple of 4 (BC7 blocks).
2. **One texture array per clip:** each frame is a layer. Measured alternative: trimming each frame
   tightly into an atlas would save a further 20% of pixels, but atlas pages pack at roughly 85–90%,
   BC7 needs 4-pixel alignment and mips need gutters, so most of that comes back. Atlases also let
   neighbouring frames bleed into each other at small mip sizes. Arrays: one bindless index per clip,
   frame = layer, no bleeding.
3. **Edges:** every alpha value is kept exactly as authored. Colour is dilated into the whole transparent
   area, so no mip or filter ever pulls in black or white fringes.
4. **Mips** are filtered in linear light, weighted by alpha (equivalent to filtering premultiplied
   colour), then stored with straight alpha. The shader premultiplies after sampling, and blending is
   premultiplied.
5. **BC7, sRGB** (`VK_FORMAT_BC7_SRGB_BLOCK`, 1 byte/pixel), encoded with **bc7e** (Binomial's
   high-quality BC7 encoder, Apache 2.0), using its pure C++ port `basisu_bc7e_scalar` from Basis
   Universal v2.50 (released 2026-08-03): no ISPC compiler needed. Perceptual error metric and alpha
   modes on; cooking runs on every core.
6. **Two assets per clip in the archive:** `tail` = every frame at small size (largest side ≤ 64 px),
   always resident (19 MB for today's roster as cooked, 192 MB at ×10, the finished game); `full` = the whole mip chain, streamed.
   A clip never shows a hole: it draws from the tail until the full version lands.
7. Later (M5): a normal/depth hint per frame so sprites are lit by the scene's lights and cast real shadows.

**Uploads:** a staging ring in host memory and the GPU's **dedicated transfer queue** (the copy engine),
tracked with a timeline semaphore. Rendering never waits: a clip switches from its tail to its full image
only once the transfer queue reports the copy done. A per-frame upload cap keeps the copy engine from
starving the frame.

Result for `neera`, all clips, BC7 trimmed:

| | Raw RGBA8 | Cooked BC7, trimmed |
|---|---|---|
| Today | ~449 MB | **~71 MB** |
| Finished (×2 animations) | ~900 MB | **~142 MB** |
| Finished, at 2× source resolution | ~3.6 GB | **~570 MB** |

That is why residency is **per clip, not per character**: only the clips on screen (idle + the current
action + whatever the cutscene timeline says is next) need full resolution in memory; everything else
sits on disk or as a tiny always-resident low mip.

**Sprite packs** (user, 2026-10-14). The look is old JRPG sprite cutscenes, but with real-life-looking
characters and perfectly smooth sprite motion. A character's clips are grouped into packs, each cooked
as its own block in the archive:
- `combat`: battle moves, hits, deaths.
- `field`: walk, idle, talk.
- `cutscene/<name>`: only the moves and expressions that one cutscene uses, including ones made only for it.

A cutscene loads its pack and nothing else, so no combat moves sit in memory during a story scene, and a
new expression is a new clip in a pack with no cost anywhere else. Expressions also live in packs and layer
over any body clip.

**Runtime:**
- A sprite is a lit, depth-tested, shadow-casting card in the 3D scene (real HD-2D, not a 2D overlay).
- **Layered animation:** body clip + face/expression layer + optional effect layers, each with its own clip
  and timing, composited per draw. Expressions are authored once and reused across body animations.
- **Cutscene timeline:** tracks for camera, characters (clip + expression + position), lights, VFX, audio,
  subtitles. The timeline also *announces* upcoming clips to the residency manager so they stream in
  before they are needed.

## 6. World

- **Gameplay grid ≠ render structure.** The hex grid is a gameplay layer (movement, targeting, footprints)
  laid over a real 3D world. Map size is not tied to hex count.
- **Two board modes from one map (R8):** the map stores per-hex gameplay data (height, terrain type) plus
  the natural ground; the terrain system renders it either as stepped hex columns or as seamless ground,
  switchable at runtime, with every hex centre at the same height in both.
- **World cells:** the world is divided into fixed-size cells streamed by distance; a map or dungeon is a
  set of cells. Positions are stored relative to cell origins so precision holds at any world size.
- **Terrain:** height via GPU clipmaps; materials by **index** (each texel stores material ids + blend, not
  one weight per type), so hundreds of terrain materials can exist with no per-area limit; texturing
  through a virtual texture cache so detail does not cost memory everywhere at once.
- **Props, houses, trees, water:** real 3D, made in Blender, exported as GLB, cooked into meshlets
  (meshoptimizer) with automatic LODs. Instanced through the GPU scene.

## 7. Renderer

Built as a **frame graph**: each pass declares what it reads and writes; the graph orders passes, places
barriers and reuses memory for transient targets.

- Geometry: GPU culling (frustum + occlusion against the previous frame's depth), meshlet rendering,
  visibility buffer.
- Lighting: physically based materials, clustered lights (hundreds of torches/spells per scene), cascaded
  or virtual shadow maps, probe-based global illumination, image-based sky lighting.
- Atmosphere: physically based sky, moon phases, volumetric fog and light shafts, weather.
- Post: HDR pipeline (exists), temporal anti-aliasing / upscaling (AMD FidelityFX, open source), bloom,
  depth of field (incl. the HD-2D tilt-shift look), colour grading, tonemapping (exists).
- Every pass shows up in a GPU timing overlay.

### 7.0 Visual target (the user, 2026-10-14)

**Not Unreal, not HD-2D, not conventional 2.5D.** The reference is the spirit of Square Enix's **Luminous
Engine** (Agni's Philosophy, Final Fantasy XV): real-time images that approach pre-rendered CGI, a renderer
built around one artistic vision rather than a general engine with a style applied afterwards. Square Enix
abandoned it as too hard to build and port; this project bets that AI-assisted development makes that
engineering affordable for one person. AI lowers the cost, not the complexity: GPU differences, shader
behaviour, memory and performance still get deliberate architecture and measurement.

What Square Enix published about FFXV's rendering (SIGGRAPH 2016 talk, "Rendering techniques of Final
Fantasy XV"): physically based rendering; dynamic weather and time of day; procedural sky and clouds wired
into lighting and weather; hybrid global illumination (dynamic + static) for bounced light. Ember takes that
direction into **2.75D as a first-class paradigm**: 2D characters, spatial depth and world-space lighting
designed as one system. The defining milestone is the first scene where they convincingly work as one
(Demonstration 1, §9). The papers are read in full before M5 is designed.

**The picture (the user, 2026-10-14): it does not exist yet in any game.** "Hexed Tactics Ogre with HD-2D
and Ember's style of photorealistic sprites and cinematic feel. Dramatic light and shadow work, incredible
spell effects, realistic ground textures, 2D props combined with real 3D objects."

**Visual requirements (draft, from that picture; each becomes a pass/fail check in D1–D3):**
- R1 **Tactics on hexes in a diorama world:** a hex battlefield under a free-orbit, zooming tactics camera
  (Tactics Ogre), framed like a miniature set (HD-2D depth of field / tilt-shift), cinematic in motion.
- R2 **Photoreal 2D characters** (from video) that belong in the 3D light: lit, shadowed, occluded, feet
  planted, never looking pasted on.
- R3 **Dramatic light and shadow:** strong key lights, deep shadows, torches, spell light, soft-to-hard
  shadow transitions, light shafts in fog. Shadows from sprites and 3D alike.
- R4 **Incredible spell effects:** effects that emit real light onto characters, ground and props, with
  particles, distortion, glow and volumetrics; the cinematic stress scene (D3) measures them.
- R5 **Realistic ground:** photographic-quality terrain materials (height-blended layers, wetness,
  detail at the tactics camera's distance), the hex grid laid over them, not painted into them.
- R6 **2D props and real 3D objects together:** photoreal 2D prop cards mixed with real 3D buildings
  in one consistent light (the 2D/3D split in §7.1).
- R7 **Fog of war (the user's specification, 2026-10-14): a rendering requirement and an acceptance test
  for the world renderer.** Three states per hex/area:
  1. *Unexplored:* absolute black. Dense, literal fog may gather along the boundary as an uncertain
     transition. Nothing beyond the boundary (terrain, units) is ever revealed by lights, spells or
     atmospheric effects; unexplored space stays black even where fog would otherwise be lit.
  2. *Explored, out of line of sight:* remembered territory, recognisable under strong darkening, shown in
     its last-known state; current enemies, moving units and other dynamic details are hidden until line of
     sight returns. Dense fog adds depth and obscures distant detail. Light from visible areas must not
     reveal hidden objects.
  3. *Visible:* full world rendering (dynamic lighting, environmental effects). Atmospheric fog may exist
     here but must never be confused with the tactical mask.
  The literal fog is **world-space volumetric fog constrained by tactical visibility**, not a translucent
  plane over hidden tiles: visible depth, soft density variation, subtle drift; it gathers in valleys,
  corridors and around ruins near the explored boundary. Discovery makes the black boundary recede and
  reveals the environment through the fog; losing line of sight fades terrain to its dark remembered state
  while dynamic entities disappear. **The separation is the rule: tactical visibility decides what the
  player may see; volumetric fog decides how the visible atmosphere looks. Lighting, shadows, VFX and fog
  scattering read the visibility mask; none bypasses it.** Acceptance: the next visual prototype (D1)
  shows all three states together with a moving light and a spell effect, and proves neither reveals
  unexplored terrain.
- R8 **Three board looks, the player's choice (the user, 2026-10-14), as Engine2 defines them**
  (`C:/Engine2/src/render/terrainMesh.ts` for the game's two, `C:/Engine2/diorama.html` keys 1/2/3 for all
  three; editor elevation brushes in `src/editor/terrainTools.ts`): **smooth** = one continuous heightfield in
  which terraces and shores wander naturally around the hex layout; **hard** = a crisp hex cliff at every
  height step, same-level ground flowing seamlessly (the stepped Tactics Ogre read); **tabletop tiles** =
  each hex a separate bevelled piece with a gap between neighbours and a dark seam where stacked levels
  meet (`buildHexTerrain(tabletop)` in diorama.html). All read the same cells (per-hex elevation + terrain),
  so one map authored once renders every way and gameplay is identical: same hexes, heights, ranges, line
  of sight. Switching is a player setting at runtime. Lighting, shadows, fog of war (R7), terrain materials,
  props and buildings work in all three. Engine2 is the reference for *what* the looks are, not for how
  they are rendered.
- **Colour reference (the user, 2026-10-14):** the saturated, warm, shadowed colour of Engine2's hex diorama
  prototype (`diorama.html`: sun #fff0d4 x3.0, hemisphere #d4e6ff / #5d4b33 x1.0, 4096 soft sun shadows,
  ACES filmic tonemapping, bloom, distance fog, optional tilt-shift) is the floor, not the ceiling: the user
  asked for better lighting than Engine2's, built with the modern techniques high-end engines publish
  (our own implementation; no outside engine). EmberEngine's first ground port looked washed out next to
  it: its provisional lighting skipped the 1/pi on the sky fill and had no shadows.

### 7.1 The 2.75D frame: 2D art in real light (researched 2026-10-14, to be validated)

**Status:** none of this exists yet. Today the engine draws unlit sprites on a flat ground. M5 is not
"add lights to sprites". It has to settle how world-space lighting meets 2D artwork, depth, terrain,
materials, shadows and volumetrics while keeping the look. A conventional 3D pipeline does not give that
for free. The design below is the hypothesis; Demonstration 1 (§9) proves or breaks it before M5 is built
on it.

**What shipped games do (public sources only; none of these studios published a full technical talk):**
- *Octopath Traveler / Dragon Quest III HD-2D* (Unreal): 2D sprites in 3D scenes; point lights added with
  effects make characters cast shadows on the environment; depth of field and tilt-shift give the
  diorama look. How the shadows are cast is not public (outside guesses: invisible 3D shapes).
- *Sea of Stars* (Unity, custom render pipeline): sprites dynamically lit with **normal maps**, **pixel
  depth writing**, shadow receiving, **shadow-casting sprites**, volumetric lighting, screen-space indirect
  light, height fog (the studio's own feature list).
- Painted-sprite tools (Sprite Lamp) derive a normal map and a depth map per sprite; a SIGGRAPH 2014
  poster self-shadows sprites from such a depth map and merges projected shadows first, self-shadows second.
- Known billboard failures: a camera-facing card casts stretched or vanishing shadows when the light is at
  a right angle to the view; the usual fix is a separate light-facing shadow proxy; alpha must be clipped
  in the shadow pass or the card casts a rectangle.

**What is 2D and what is 3D (taken from Engine2; only this logic carries over, the user, 2026-10-14).**
Engine2 was dropped, so its rendering is not a reference; its split between 2D and 3D is ("some things lose
too much richness in full 3D no matter the quality"):
- **Photoreal 2D cards:** characters, and props and decor (the more detailed objects), standing upright in
  the 3D world. Photoreal 2D reaches a realism 3D props did not.
- **Real 3D:** houses and buildings, and the terrain's shape and height.
- **Ground:** photoreal texture art on the real 3D terrain.
- **3D where it is not seen as 3D:** terrain shape and height, light, fog, shadows, collision.

**Camera (the user, 2026-10-14): free until the user settles the angles, then locked;** cinematic moves in
cutscenes and spells on top. The engine supports a free orbiting camera now; 2D art must look right at
the angles finally chosen.

**Design for the frame (hypothesis for D1):**
1. **Cards are lit by the world** through the same clustered light list and shadow maps as the 3D world.
   How a card responds to light (one shading normal per card, or per-pixel normal / depth layers estimated
   from the video frames with Meta's *Sapiens* / *Sapiens2*) is decided in D1 by comparing them side by
   side on real frames; the photoreal look must survive the lighting. Per-pixel layers cost sprite memory
   and streaming ×2.5 (×10 roster: 26.3 GB -> ~66 GB, peak demand 322 MB -> ~800 MB); temporally consistent
   human models (*From Frames to Sequences* 2026, *GeoMan* 2025) have no released weights yet, so flicker
   must be measured.
2. **Depth:** cards write depth, so 3D buildings and terrain occlude them correctly and they occlude each
   other; feet meet the ground.
3. **Shadows:** alpha-clipped casters into the same shadow maps; the caster faces the light, not the camera,
   so moving the free camera never moves a shadow. Contact shadows at the feet.
4. **Volumetric fog** lit by the same lights and shadow maps; spell effects emit real light.
5. **The look on top** (tilt-shift, bloom, grading, day/night): tuned after D2, never before.

**Open questions only a prototype answers:** shadow quality of light-facing casters at low sun angles;
how the cards read at the angles the user picks; whether 2D props need extra views for the cinematic camera.

**3D geometry (M3), chosen to feed this frame:** meshes cooked into meshlets with meshoptimizer v1.3
(cluster LOD via `clusterlod.h`), drawn by task + mesh shaders (`VK_EXT_mesh_shader`, supported on the dev
GPU, RX 6400: up to 256 vertices / 256 primitives; AMD recommends 64–128 per meshlet on RDNA 2), culled
on the GPU per instance and per meshlet against the frustum and the previous frame's depth pyramid
(two-phase occlusion), written to a visibility buffer. A compute-culling + multi-draw-indirect path is built
too and measured against it on the dev GPU; the faster one is kept.

### 7.1b Lighting design (researched 2026-10-14; built step by step for D1 on Engine2's maps)

**Sources read in full:** Square Enix, *Rendering Techniques of Final Fantasy XV* (SIGGRAPH 2016 talk
paper, Elcott, Chang, Miyamoto, Metaaphanon, https://www.jp.square-enix.com/tech/library/pdf/SEJ-Siggraph2016_Graphics_short.pdf)
and its 83-slide deck (https://www.jp.square-enix.com/tech/library/pdf/s16_final.pdf). The Luminous
Engine's lighting, as published:

| FFXV (Luminous) | What it does |
|---|---|
| Physically based shading: GGX, Schlick-Smith masking, Schlick Fresnel, Lambert diffuse; roughness/metallic | the BRDF |
| Linear workflow, real sky values (sun = moon x 400,000); exposure multiplied into every light, sky and fog, not applied in post (fp16 precision) | units and exposure |
| Procedural sky: precomputed scattering LUTs (fitted to photographed skies), Rayleigh + Mie phase, separate overcast model (CIE); procedural clouds (7 noise octaves, ray-marched, cloud shadows on the ground); stars, moon | the sky drives the light |
| Sky diffuse via PRT (sky SH x baked transfer, order 3) in probe grids placed on nav meshes / height maps; irradiance volumes for static local lights; probes blended in layers | indirect diffuse |
| Tiered specular: dynamic sky cubemap (filtered over frames, 6 faces x 8 mips), local cubemaps relit by time of day (sky/local split), half-res SSR | reflections |
| Cascaded sun shadows + height-map ray marching far away; local shadow maps in an atlas | shadows |
| Tiled + 32 logarithmic depth-slice light culling; IES lights | local lights |
| Volumetric fog in a 160x90x64 froxel grid (~100 m), lit by sun (blurred shadow -> light shafts), local lights and probes; noise and wind | atmosphere |
| Aerial perspective: inscatter + Beer transmittance, artist-tinted mid-ground | depth |
| Half-res SSAO (8 samples, temporal) + analytic AO spheres on foliage and heroes | contact |
| Weather: wetness (darker diffuse, lower roughness, more specular), puddles, ripples, rain particles with depth collision | weather |
| Artists calibrated every parameter against 24 hours of HDR photographs of a real sky | calibration |

**EmberEngine's plan.** The same structure, with the newer published techniques where they are better,
always our own implementation (no outside engine):
1. **BRDF and energy** (now): GGX / height-correlated Smith / Schlick, Lambert with (1 - F); indirect light
   divided by pi like direct light (the provisional lighting forgot this: the source of the washed-out ground).
   Light colours authored in sRGB and converted to linear. Exposure multiplied into the lights.
2. **Sun shadows** (now): one 4096 map fitted to the board (Engine2's setup) with soft filtering, then cascades
   when the camera can see past the board (M4). Cards cast alpha-tested shadows.
3. **Sky** (next): physically based atmosphere (Hillaire 2020: transmittance, multi-scattering and sky-view LUTs,
   aerial perspective volume), which has twilight and runs in well under a millisecond, calibrated to
   photographs as FFXV did; its SH projection is the ambient light, its filtered cubemap the reflections.
4. **Local lights**: clustered (FFXV's 32 log slices), spell and torch light, through the fog-of-war mask (R7).
5. **Volumetric fog** in froxels lit by sun shadow and local lights (R7's literal fog). 6. **AO** (GTAO).
7. **Probes / GI**, clouds and weather after D1.

**Hybrid architecture (the user, 2026-10-14: "a hybrid Luminous + Unreal lighting system ... our own
implementation in C++ and Vulkan").** Luminous's lesson: combine dynamic effects with cached lighting
(probes, PRT) instead of recomputing everything every frame. Unreal's published lesson (Lumen): update indirect
light selectively, account for occlusion, use several scene representations (screen traces + cached probes)
at lower resolution. Hardware ray tracing is never required (the dev GPU has it but it is slow); any tracing or
probe method is measured against Ember's geometry and camera before it is kept.

| Layer | Treatment |
|---|---|
| Direct: sun / moon, torches, moving spell and combat lights | rasterised, every frame where visible; shadow maps (cached or rate-limited where nothing moves) |
| Broad environment light | cached spatial irradiance probes, updated where the scene changes |
| Bounce from changing lights (a spell, a torch carried by) | selective probe updates; responsiveness measured (no visible lag or popping) |
| Contact detail, small-scale occlusion | screen-space (AO, contact shadows) |
| Fog and light scattering | GPU froxel pass, reading the visibility mask |
| Unexplored fog of war | strict mask (R7): no light, shadow, bounce, fog or effect reveals it |

**The lighting contract (fixed before more materials depend on it; `shaders/lighting.hlsli`).** Every lit
surface (terrain, 3D meshes, 2D cards, later water and effects) fills one `Surface` (world position, geometric
normal, shading normal, albedo, roughness, metallic, occlusion, emissive, and for cards their depth / normal /
material masks) and calls one `shade()`. Inside it: direct light (sun with its shadow, the local-light list),
then `indirectDiffuse()` and `indirectSpecular()` (today the hemisphere; later probes and the sky cubemap: the
interface does not change), then the visibility mask (R7) applied to light and to colour. Units: linear
radiometric RGB, sun and sky in the same scale as local lights, exposure multiplied into every light. Each
layer has its own GPU timer and an on/off switch for measuring it alone.

**Proof scene (D1 foundation):** an existing Engine2 map with a character (a real sprite), elevated terrain,
water, a building or wall, a moving light and a spell light, lit together with coherent shadows and depth,
before anything is scaled up.

**The floor (the user, 2026-10-14):** never less saturated, warm or shadowed than Engine2's prototype
(`diorama.html`: sun #fff0d4 x3.0 from (-30, 42, 20), hemisphere #d4e6ff / #5d4b33 x1.0, 4096 soft shadow, ACES
filmic (three.js's fit, exposure / 0.6), bloom 0.3 / 0.55 / 0.9, fog #9fc2dd 85-175). Until the sky exists,
those are the default lights, so every step is compared against the prototype under the same light.

### 7.2 M3 measurements so far (2026-10-14, RX 6400, 1600x900; M3 is not done)

Built: GPU-driven meshes (instance culling, cluster-LOD BVH traversal, task + mesh shaders, R32 visibility
buffer, analytic-derivative resolve), two-phase occlusion culling per instance and per cluster (early
clusters hidden by last frame's pyramid are re-tested late), the battle test (`Sandbox --battle`), GPU
cost split (`--content`, `--cost-split`), proxy quality images (`--compare`, `tools/scripts/compare_meshes.py`).

**How to read GPU times.** At 60 Hz an under-loaded GPU drops its clock (power saving), so a frame that
costs 4 ms at full clock is measured at up to ~16 ms, alternating frame to frame (every pass slows by the
same factor, even fixed-cost tonemapping). Cost at full clock is measured with `--unpaced`. Whether the
power governor ever makes a paced frame miss its vblank must be measured in a window with vsync (open).

**Battle test results (128x104 hexes, 26.5k 3D pieces, 28k cards, 72 units):**

| Meshes | GPU at full clock (median / worst) | Triangles drawn | Verdict |
|---|---|---|---|
| Cooked originals (Engine2 cottages) | 50-116 ms | 39-134 M, cluster list overflows | fails |
| Voxel-remesh proxies (position error only, `Cooker --remesh 128`) | 2.5-4.1 / 4.2 ms | 1.6-4.1 M | fast enough, but see quality |

Where the time goes (proxies, default view): drawing the visibility buffer ~75% (task shaders 13%, mesh
shaders 55%, rasterisation 32% of it), material resolve ~20%, culling + LOD + pyramids <10%, 2D cards
0.15-0.4 ms. CPU main thread worst 3.8-5.9 ms.

**Why the originals fail.** The cottage (Engine2, baked from a 215k-triangle source) is 1,170 separate
pieces (bricks, planks, tiles, ~30 cm). Cluster LOD can only remove whole pieces, so its levels report
0.25 / 0.41 / 0.72 m errors on a 6.5 m house, and at 1 px the LOD keeps ~25k of 34k triangles even when the
house is 20 px wide. Generated building models are likely to be built the same way.

**Proxy quality (`--compare`, cottage v01, vs the full-detail original):**

| Width | Silhouette IoU (proxy) | Shading-normal error, proxy: mean (95th pct) | Original at engine LOD |
|---|---|---|---|
| 400 px | 0.998 | 24.2 deg (83.9) | 0.0 deg |
| 200 px | 0.998 | 27.7 deg (83.4) | 0.0 deg |
| 100 px | 0.996 | 31.3 deg (82.1) | 1.6 deg |
| 50 px | 0.972 | 45.0 deg (104.3) | 5.9 deg |
| 25 px | 0.952 | 53.0 deg (108.9) | 23.9 deg |

Silhouettes hold; surface normals do not: the proxy averages the bricks' and tiles' real geometry into
a smooth voxel surface, and it would be used below ~260 px (where its 2.5 cm error is under a pixel), i.e.
in the 200-50 px rows. Lighting on those buildings would visibly change. **The position-only proxy is not
acceptable as is.** Its silhouette and speed are; its normals (and its missing colour) need to come from
the original: normal, colour and ORM maps baked from the original onto the proxy, then measured again
with the same tool against the same numbers.

**Paused (the user, 2026-10-14): none of these assets are final.** Whether proxies are needed at all
depends on how the final buildings are made: one continuous surface (typical of generated models)
simplifies under cluster LOD; kit-bashed pieces like this cottage do not. The question is settled with a
representative building (made in Blender, one continuous mesh) in the battle test, not by optimising
placeholders. The unfinished bake (xatlas atlas, ray-cast bake, proxy format; not compiled, not run) is
kept on the branch `wip/proxy-bake`.

## 8. Gameplay and tools

**Engine2's game design is sound (the user, 2026-10-14: "nothing else is wrong with its game design"); its
engine is what failed (spell effects stutter, sprite animation subpar, §1).** So its rules, maps, board looks
(R8), editor features and authored content are the reference and are ported faithfully; the technology
(renderer, card lighting, camera, VFX, animation) is built fresh here. Its maps and dialogues come across with
an importer, so nothing authored in the old editor is lost.

- **ECS** for game objects; battle rules, campaign and saves port from Engine2's logic (rules, not its renderer).
- **Physics/collision:** Jolt Physics.
- **Audio:** streamed music/voice, positional effects.
- **Editor:** Dear ImGui-based, runs the real renderer, hot reload, undo for everything, saves only when the
  user saves.

## 9. Build order and exit tests

Each milestone ends with a test on real game data, not a demo scene.

| # | Milestone | Done when |
|---|---|---|
| M0 | Vulkan 1.3 foundation: bindless, HDR target, tonemap, off-screen capture | ✅ triangle renders with zero validation errors |
| M1 | Core: job system, allocators, async file IO, `.pak` archive, asset handles, profiler overlay | ✅ 10,000 assets (332 MB, 172 MB compressed) all requested in one frame, streamed in ~0.1 s at ~1.6 GB/s from disk, every byte verified against its source; worst main-thread frame 3.4–8.6 ms against the 16.7 ms budget (release build, Ryzen 9 5950X, NVMe) |
| M2 | Sprite cooker + sprite residency | **every character in Engine2's manifest** cooked; a parade that plays every clip of every character stays under a fixed GPU budget smaller than the roster, with no frame over budget and no clip seen at tail quality when it was requested ahead — at the finished-game workload (×10, §1). **Measured (2026-10-14, RX 6400, 60 Hz, 512 MB budget, clips in a fixed-seed shuffled order):** today's roster, 375 clips (2.63 GB GPU): PASS, 0 late frames, worst main-thread frame 3.9 ms, peak demand 227 MB, 306 evictions; debug with Vulkan validation: PASS, 0 errors. **×10 = finished game, 3,750 clips (26.3 GB GPU): PASS, 0 late frames, worst frame 4.6 ms, peak demand 322 MB, 3,668 evictions, 0 requests without room.** Earlier runs, kept for the record: ×2 (a misreading of the workload) passed; ×10 in roster order failed (421 late frames) because the pack's 10 copies of one clip shared the stage, ~900 MB in one frame, more than the budget, which no real scene does. |
| M3 | 3D meshes: GLB → cluster LOD, GPU-driven culling, PBR; 2D cards at scale | **a large-battle scene with real assets at real density, every frame ≤ 16.7 ms (CPU and GPU) on this PC (RX 6400), quality never lowered to pass, 0 validation errors in debug** (the user, 2026-10-14). Measured from the 243 Ember maps (Engine2's copies): largest map 64×52 hexes; densest 3D architecture 0.20 pieces/hex (deep crypt: 392 on 1,920); densest 2D decor 0.21 cards/hex (wisp forest: 335 on 1,584); most units 33 (30 enemies). **Those maps were bare and Engine2 already stuttered on them (the user, 2026-10-14), so they are a floor, not the finished game's density.** The scene: a battlefield of 128×104 hexes (4× the largest map's area) dressed at **10× the measured densities, both at once**: ~27,000 real 3D building pieces (2.0/hex; the cooked GLBs until Meshy buildings exist) and ~28,000 2D decor cards (2.1/hex) from the 443 decoration images at full native resolution, plus 8 MCs and 64 enemies (2× the largest measured enemy count) animating from the M2 sprite pipeline; measured zoomed out with the whole field in view, at the default zoom, and during a free-orbit sweep. Scope numbers can be raised by the user, never lowered to pass. |
| M4 | World cells + terrain | a world 10× the largest Ember map streams while flying across it, no hitches, in both board modes (R8), switched at runtime without a reload |
| M5 | Lighting and atmosphere | sprites and 3D lit by the same lights and shadows, fog, sky, moon |
| M6 | Editor | build a map without touching code |
| M7 | Gameplay port + cutscene timeline | a battle and a cutscene playable end to end |

### Demonstrations (the user's progress gates, 2026-10-14)

Passing engineering tests (like M2's streaming) is foundation work, not evidence of the lighting or
environments the game needs. Visual progress is judged by three demonstrations. None exists yet; each is reached only
when it is measured and shown, never claimed.

| # | Demonstration | Shows | Built on |
|---|---|---|---|
| D1 | **Lit sprites in a spatial world** | a dynamic light visibly lights a character and the terrain near it, with coherent depth and occlusion (the character stands in the world, passes in front of and behind things, casts and receives shadow); and the fog of war (R7): unexplored, remembered and visible areas together, with a moving light and a spell effect, neither revealing unexplored terrain or hidden units | M3 + a test terrain + the §7.1 card lighting inside one clustered light pass + the tactical visibility mask and volumetric fog (R7); it validates §7.1 before M5 is built on it |
| D2 | **A complete environmental scene** | terrain, props, atmospheric depth, shadows and lighting working together | M4 + M5 |
| D3 | **A cinematic stress scene** | multiple dynamic lights, spell effects, particles and animated characters together, inside a measured frame budget on the dev GPU | M5 + effects/particles |
