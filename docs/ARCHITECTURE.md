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

Characters are the only 2D thing in the game; everything else is real 3D. They get first-class support.

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

## 8. Gameplay and tools

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
| M3 | 3D meshes: GLB → meshlets, GPU-driven culling, PBR | 100,000 instanced Blender props in view at 60 fps on the dev GPU |
| M4 | World cells + terrain | a world 10× the largest Ember map streams while flying across it, no hitches |
| M5 | Lighting and atmosphere | sprites and 3D lit by the same lights and shadows, fog, sky, moon |
| M6 | Editor | build a map without touching code |
| M7 | Gameplay port + cutscene timeline | a battle and a cutscene playable end to end |
