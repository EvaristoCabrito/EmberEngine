# The default setup: only the user changes settings; agents only ADD

- The user, 2026-11-04: "ALL MAPS IN THE GAME ALL MAPS IN THE EDITOR ALL NEW MAPS I EVER CREATED START WITH THIS SETUP UNLESS I CHANGE IT MANUALLY ONLY MANUAL CHANGES WILL TAKE EFFECT NOW YOU OR ANY AGENT ARE FORBIDEN TO ALTER ANY SETTINGS ... ONLY ADD MORE ... NEVER REMOVE".
- The setup: clouds = look 004 (MapStage default `clouds_` 2 + `savedCloudLook_` 4; the Map Editor's Nuvens shows "Nuvens 004" for a map with no choice) and the sky = Andromeda (star look 5). A map's saved choice stays until the user changes it by hand.
- No agent changes a default, a picker's list, a saved setting or a map's choice on its own. A new look or option is only ADDED as one more choice, never in place of one. Nothing is removed without the user's explicit order.

# Permanent game visual protections

- The game's atmosphere and all visual effects are locked and final. Never modify the atmospheric sky, the real sun or moon, lighting, shadows, renderer/shader behavior, or game VFX to address a request about an apparent extra sun. The user will never ask to change the atmosphere; interpreting a sun-removal request that way is always a mistake.
- The permitted fix for a sun painted into a backdrop is to change only that specific image's alpha/transparency so the baked-in sun becomes transparent and the real in-game sky shows through. Preserve the original asset and use a new versioned filename; update only the references needed to use that version.
- If the image containing the painted sun is not certain, identify the scene's active backdrop before editing. Do not touch the sky lock or visual-effect assets for this issue.
- Preserve every other existing asset and all unrelated user work.
## Image rule number 1

- Strictly prohibit “ruído difusivo pixelado”: diffusion-like blotches or marks, grain/noise, pixelation, blocky squares, and smeared or scribbly edges. Visual work must be crisp, smooth, clean, and photorealistic, built from real 3D geometry or supplied original artwork; never use diffusion-generated imagery.
- Show every proposed game asset individually as a preview and wait for the user's explicit approval before adding or changing game assets or content. Never open the game or the user's browser.
