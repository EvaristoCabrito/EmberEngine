# Veo prompts for character sprites

## THE PROMPT FORMAT (the user's own, 2026-11-19: "this is a prompt, not what you're shitting out")

Every prompt for a character move is written in THIS format, as plain text, one prompt only, no options and no explanation around it.
It is the user's working template; the long generic block further down (written by Claude on 2026-11-19, never asked for) does NOT apply any more.

```
ZOOM OUT <Move> — <N> seconds: <One short paragraph of the body mechanics of the move, in the order it happens: the torso, the head, the face, the arms, the feet, the cloak or the weapon. It ends by returning to the exact opening stance (or holding the final pose still). "One single <move>, with no additional movements.">

CRITICAL CAMERA AND FRAMING — ABSOLUTE PRIORITY: The camera must remain COMPLETELY STATIONARY throughout the entire animation. ABSOLUTELY NO ZOOM IN, NO ZOOM OUT, NO CAMERA PUSH, NO CAMERA PULLBACK, NO TRACKING, NO CROPPING, AND NO REFRAMING. Maintain exactly the same camera distance, character size, and composition from the first frame to the last. The character must NEVER appear larger or smaller in the frame.

The ENTIRE character, including <head, both feet, hands, full cloak, the whole weapon from tip to end>, must remain COMPLETELY INSIDE THE FRAME AT ALL TIMES. Leave GENEROUS empty space on all four sides, including above the <weapon> and below the boots. Nothing may touch or cross the frame boundaries.

Preserve <his/her> exact design, anatomy, proportions, <armor, cloak, weapon, colors, textures> and original <left-facing profile / front-facing opening stance>. Plain, evenly lit LIGHT NEUTRAL-GRAY background with a matching floor. Fixed studio lighting.

Audio: <the move's sound effects only>. No music or voices.
```

Adapt per character and move (the weapon, the hand, the facing, the costume list). For a fall the wide picture is set in the generator (not in the text) and the
frame paragraph adds: the character stays at the exact CENTER of the picture the whole time, standing, falling and lying down, with generous empty space left and
right of the fallen body (Isolde's fall, 2026-11-20: a vertical picture does not work for a fall).

---

(Older notes below. Written 2026-10-25 from what went wrong with Voss's and Isolde's videos; where they disagree with the format above, the format above wins.)

How to ask Veo for a character's moves so the video → sprite pipeline (`tools/scripts/sprites/video_matte.py` →
`video_sprite.py`) uses every second of them. Written 2026-10-25 from what went wrong with Voss's and Isolde's videos; each
rule says which problem it prevents.

## The same for every video

Put this block in every prompt of a character, word for word, so all their moves match:

```
A single continuous full-screen shot of [CHARACTER DESCRIPTION, identical in every video], standing on a flat, plain medium-grey studio
floor in front of a seamless plain medium-grey studio backdrop that fills the whole picture from edge to edge. Soft, even, neutral studio
lighting from the front and both sides, no colored light, no shadows on the wall. Locked-off static camera at chest height, placed far
enough back that the whole character, from the top of the [STAFF / WEAPON] to the soles of the boots, is always visible, with plenty
of empty grey space above, below and on both sides. The character fills about 80% of the picture's height. One single plain picture:
no borders, no black bars, no letterbox, no inset, no picture-in-picture, no blurred or duplicated copy of the video behind or beside
it, no vignette, no text. No camera movement, no zoom, no cuts. Realistic, photographic, sharp focus, natural colors. Audio: only sound
effects of the character itself and its movement; no narration, no voice-over, no speech, no dialogue, no music, no background ambience.
```

**Never write the words "frame", "framed", "inside the frame" or an aspect ratio such as "9:16" in a prompt** (the user, 2026-11-19): the
generator takes them literally and draws a smaller picture inside the picture (IsoldeHitReacV4: a 1920 × 1080 video with the shot as a
panel in the middle and blurred copies of her at both sides). Say "picture", "screen", "visible", "empty space around her" instead, and
set the aspect ratio (vertical, 9:16) in the generator's own setting, never in the text.

| Rule | Why (what happened) |
|---|---|
| Plain medium-grey backdrop and floor, every video | Isolde's hit was made on a dark stone wall: the cut-out was harder and her light had to be matched by hand. |
| Even neutral light, no colored light | The stone-wall set lit her half a stop darker; every move must look like the same person in the same light. |
| No camera movement, no cuts | Isolde's right walk opens on another shot and cuts at 1 s: that second is lost. |
| Empty space on every side, staff included (said as "always visible", never as "inside the frame") | Isolde's hit sways off the picture's edge, and Voss's fallen staff runs off the left edge: those pixels can't be recovered. |
| No borders / inset / blurred copies, said in the prompt; no "frame" or "9:16" in the text | IsoldeHitReacV4 came out as a 1920 × 1080 video with the shot as a smaller panel and blurred copies of her at the sides: the cut-out would pick up the copies. |
| ~80% of the picture's height, the same in every move | Voss is ~1100 px tall in his idle but only ~610 px in his cast, hit and death (filmed from farther away), so those clips can't be sharper. |
| Audio line: sound effects only, no narration (the user, 2026-11-19) | The game's unit sounds are cut from the videos' own audio; a narrated video can't be used for them. |
| The same description in every video | The costume and the staff must not change between moves. |

Settings (in the generator, not in the prompt): vertical 9:16, 1080 × 1920 when offered (720 × 1280 otherwise), 24 fps, 8 s for loops (the
longest clip Veo gives; more seconds = more frames to pick the best loop from), 4 s is enough for a hit.

## Each move

Add one of these after the block above.

**Idle (a loop)**
```
The character stands still in a relaxed ready pose, holding the [STAFF] upright, breathing slowly and naturally with only tiny
movements, eyes forward. The video ends in exactly the same pose, position and stillness it starts with, so it loops seamlessly.
No turning, no gestures, no steps.
```
Why: the idle is played as a loop. Voss's and Isolde's idles show a jump where the loop restarts because their videos never come back
to the first pose.

**Walk right / walk left (loops)**
```
The character walks in place to the [RIGHT / LEFT] in exact profile view, as if on a treadmill: the body stays in the center of the
picture while the legs and the [STAFF] move in a steady, natural walking rhythm at a constant pace, for the whole video. The character is
already walking at the very first moment and keeps walking until the very last moment, never slowing down, never stopping, never turning
toward the camera.
```
Why: Voss turns toward the camera in the last second of his right walk and stops in the last 2 s of his left walk; Isolde's left walk
spends its first second turning. Only the steady part can be looped.

Always add which hand holds the staff and where that puts it in this direction (the user, 2026-10-26, Isolde: her right walk came out
with the staff in her LEFT hand, held by the far arm while the near arm hung empty, while every other clip has it in her RIGHT, so her
two walks looked like mirror copies):
```
The character holds the [STAFF] in the [RIGHT] hand only, the same hand as in every other video. Walking to the [RIGHT], that hand is the
one nearest the camera: the [STAFF] is held by the near arm, in front of the body. The other arm, on the far side, swings freely and empty.
```
(A right-handed character walking to the right has the staff hand nearest the camera, the staff in front of the body; walking to the
left, the staff hand is on the far side, the staff partly behind the body. A left-handed one the other way round.)

Isolde's walk right (right-handed), the whole prompt:
```
A single continuous full-screen shot of a young woman with long wavy copper-red hair and freckles, wearing a black studded leather
corset with dark leather shoulder guards and bracers, a long tattered black cloak, a tattered dark burgundy skirt and tall black leather
boots, carrying a tall ornate black staff whose head is a dark crescent of curved spikes cradling a red gem, standing on a flat, plain
medium-grey studio floor in front of a seamless plain medium-grey studio backdrop that fills the whole picture from edge to edge. Soft,
even, neutral studio lighting from the front and both sides, no colored light, no shadows on the wall. Locked-off static camera at chest
height, placed far enough back that the whole woman, from the top of her staff to the soles of her boots, is always visible, with plenty
of empty grey space above, below and on both sides. She fills about 80% of the picture's height. One single plain picture: no borders,
no black bars, no letterbox, no inset, no picture-in-picture, no blurred or duplicated copy of the video behind or beside it, no
vignette, no text. No camera movement, no zoom, no cuts. Realistic, photographic, sharp focus, natural colors.
She walks in place to the RIGHT in exact profile view, as if on a treadmill: the body stays in the center of the picture while the legs
and the cloak move in a steady, natural walking rhythm at a constant pace, for the whole video. She holds the staff in her RIGHT hand
only, the same hand as in every other video: walking to the right, her right arm is the one nearest the camera, so she holds the staff
with that near arm, upright in front of her body, planted lightly with each step. Her left arm, on the far side of her body, swings
freely and empty. She is already walking at the very first moment and keeps walking until the very last moment, never slowing down,
never stopping, never turning toward the camera.
```

**Turn to walk (optional, played once before the walk loop)**
```
The character starts in the relaxed idle pose facing the camera, turns smoothly into profile view to the [RIGHT / LEFT] and begins
walking in place, then keeps walking steadily until the end.
```
Why: the turn in Isolde's left walk was the best part of it; as its own video it can start every walk (needs a "walk start" pose in
the game: not built yet).

**Cast, attack, staff counter (played once)**
```
The character starts in the relaxed idle pose, [ACTION: e.g. raises the staff and casts a spell / thrusts the staff forward at an enemy
on the RIGHT side], then returns to exactly the relaxed idle pose and holds still for the last second.
```
Why: a one-shot starts and ends on the idle pose, so the game passes from idle into it and back with no jump. Keep glows and magic
small and close to the character: bright effects crossing the figure confuse the cut-out (Voss's cast lost half its staff), and the game
draws its own 3D spell effects anyway.

**Hit (played once)**
```
The character starts in the relaxed idle pose, is struck hard in the chest by an unseen blow from the RIGHT, staggers back one small
step staying on the same spot of the floor, recovers and returns to exactly the relaxed idle pose, holding still for the last second.
```

**Death / fall (played once, held on the last pose)**
```
The character starts in the relaxed idle pose, is struck by an unseen blow, collapses and falls to the floor, and lies completely still
for the last two seconds. The whole body and the [STAFF] stay fully visible, with empty space around them, while falling and lying down;
the character falls toward the camera side, not sideways.
```
Why: Voss's death is 1204 px wide because he ends up lying across the picture, and Isolde's Hit2Fall lying pose runs off both sides
(2026-11-19); with the empty-space rule the fallen body and staff stay whole.

## After the video

Tell me the move and, if the action does not start at once, the second it starts. Everything else (cut-out, loop point, feet line,
size, the Média / Leve copies) is the pipeline's.
