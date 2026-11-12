# Fire kit v01: textures for the artist

The 3D fire props (`content/models/fire/*.glb`) were built with textures already in the project, as stand-ins. The list below is
what would finish the paint job. Paste the block under "Brief" to the artist agent as it is.

## Brief (paste this)

```
Paint 9 seamless material textures for the Ember Ashes fire props (candlesticks, torches, braziers, campfires, lanterns, a shrine,
forges). World: dark fantasy, a burnt-out land under ashfall; photoreal, weathered, soot-stained, never clean or new.

Rules for every file:
- 1024 x 1024 px, JPG, tiles seamlessly on all four edges.
- A flat material photographed straight on: even soft light, no cast shadows, no highlights baked in, no objects, no text, no frame.
- The texel scale noted per file is how much real surface the square covers.
- Save into: content/models/textures/  with exactly the file name given.

1. fire-iron-v01.jpg       Hand-forged wrought iron, smoke-blackened, hammer marks, rust blooming in the pits, a little orange scale.
                           Covers 40 cm. (candlesticks, torch baskets, brazier bowls, lantern cages, chains)
2. fire-wax-v01.jpg        Tallow candle wax, ivory to yellowed cream, faint vertical drip runs, a little soot and dust.
                           Covers 10 cm. (every candle)
3. fire-pitch-v01.jpg      Torch head wrapping: coarse cloth soaked in black pitch, tarry, cracked, a few charred frayed threads.
                           Covers 20 cm. (torch heads, wicks, the smithy's canopy)
4. fire-charred-log-v01.jpg  Burnt firewood bark: black alligator-cracked char with grey ash in the cracks, no glow.
                           Covers 30 cm, grain running up. (campfire logs, tripod poles, stumps)
5. fire-embers-v01.jpg     A bed of live coals seen from above: black and grey lumps, the gaps glowing deep orange to yellow.
                           Covers 40 cm. (brazier bowls, campfire beds, forge hearths)
6. fire-embers-glow-v01.jpg  The same picture as 5, pixel for pixel, but only its light: the glowing gaps as they are, everything else
                           pure black. (the emission map of 5)
7. fire-hearth-stone-v01.jpg  Soot-stained fieldstone, rounded and heat-cracked, darker toward one side.
                           Covers 50 cm. (campfire ring stones, forge blocks)
8. fire-bronze-v01.jpg     Old cast bronze, dark brown with green verdigris in the hollows and worn golden high points.
                           Covers 30 cm. (the incense burner, the hand lantern's burner)
9. fire-grave-stone-v01.jpg  Carved cemetery limestone, grey, pitted, lichen and moss in patches, rain streaks.
                           Covers 60 cm. (the lantern pillar, the candle shrine, the menhir)
```

## Where each one goes

Once the files exist, the swap is one line each in `tools/scripts/models/create_fire_kit_v01.py` (`materials()`) and
`create_fire_kit_v01_scenes.py` (`materials()`), then rebuild the models (remove the `.glb` first: the scripts never overwrite),
re-run `register_fire_kit_v01.py`, and cook the next `props-vNN.pak`.

| New file | Material key | Stand-in used today |
|---|---|---|
| fire-iron-v01.jpg | `iron` | `wt-iron.jpg` |
| fire-wax-v01.jpg | `wax` | `wt-limestone.jpg`, tinted cream |
| fire-pitch-v01.jpg | `pitch` | `wt-burlap.jpg`, tinted black |
| fire-charred-log-v01.jpg | `bark` | `wisp-dead-bark-v01.jpg`, darkened |
| fire-embers-v01.jpg + fire-embers-glow-v01.jpg | `coal` | ambientCG `lava-001` colour + emission |
| fire-hearth-stone-v01.jpg | `stone` | ambientCG `rock-030` |
| fire-bronze-v01.jpg | `brass` | `brass-detail-v01.jpg` |
| fire-grave-stone-v01.jpg | `grave` | `wt-gravestone.jpg` |

Not needed: oak (`wt-dark-oak.jpg`), planks, masonry: the project's own are final.
