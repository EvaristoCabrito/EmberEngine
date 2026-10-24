# Permanent game visual protections

- The game's atmosphere and all visual effects are locked and final. Never modify the atmospheric sky, the real sun or moon, lighting, shadows, renderer/shader behavior, or game VFX to address a request about an apparent extra sun. The user will never ask to change the atmosphere; interpreting a sun-removal request that way is always a mistake.
- The permitted fix for a sun painted into a backdrop is to change only that specific image's alpha/transparency so the baked-in sun becomes transparent and the real in-game sky shows through. Preserve the original asset and use a new versioned filename; update only the references needed to use that version.
- If the image containing the painted sun is not certain, identify the scene's active backdrop before editing. Do not touch the sky lock or visual-effect assets for this issue.
- Preserve every other existing asset and all unrelated user work.
