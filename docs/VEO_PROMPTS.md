# Veo prompts for character sprites

How to ask Veo for a character's moves so the video → sprite pipeline (`tools/scripts/sprites/video_matte.py` →
`video_sprite.py`) uses every second of them. Written 2026-10-25 from what went wrong with Voss's and Isolde's videos; each
rule says which problem it prevents.

## The same for every video

Put this block in every prompt of a character, word for word, so all their moves match:

```
Full-body shot of [CHARACTER DESCRIPTION, identical in every video], standing on a flat, plain medium-grey studio floor in front of a
seamless plain medium-grey studio backdrop. Soft, even, neutral studio lighting from the front and both sides, no colored light, no
shadows on the wall. Locked-off static camera at chest height, no camera movement, no zoom, no cuts. Vertical 9:16 frame. The whole
body, the head, the feet and the entire [STAFF / WEAPON] stay inside the frame at all times, with a clear margin of empty backdrop
above the head, below the feet and on both sides. The character fills about 80% of the frame's height. Realistic, photographic,
sharp focus, natural colors.
```

| Rule | Why (what happened) |
|---|---|
| Plain medium-grey backdrop and floor, every video | Isolde's hit was made on a dark stone wall: the cut-out was harder and her light had to be matched by hand. |
| Even neutral light, no colored light | The stone-wall set lit her half a stop darker; every move must look like the same person in the same light. |
| No camera movement, no cuts | Isolde's right walk opens on another shot and cuts at 1 s: that second is lost. |
| Margin on every side, staff included | Isolde's hit sways off the frame's edge, and Voss's fallen staff runs off the left edge: those pixels can't be recovered. |
| ~80% of the frame's height, the same in every move | Voss is ~1100 px tall in his idle but only ~610 px in his cast, hit and death (filmed from farther away), so those clips can't be sharper. |
| The same description in every video | The costume and the staff must not change between moves. |

Settings: 720 × 1280, 24 fps, 8 s (the longest clip Veo gives; more seconds = more frames to pick the best loop from).

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
frame while the legs and the [STAFF] move in a steady, natural walking rhythm at a constant pace, for the whole video. The character is
already walking in the first frame and keeps walking until the last frame, never slowing down, never stopping, never turning toward
the camera.
```
Why: Voss turns toward the camera in the last second of his right walk and stops in the last 2 s of his left walk; Isolde's left walk
spends its first second turning. Only the steady part can be looped.

Always add which hand holds the staff and where that puts it in this direction (the user, 2026-10-26, Isolde: her right walk came out
with the staff in her LEFT hand while every other clip has it in her RIGHT, so her two walks looked like mirror copies):
```
The character holds the [STAFF] in the [RIGHT] hand only, the same hand as in every other video. Walking to the [RIGHT], that hand is on
the far side of the body: the [STAFF] is seen partly behind the body, never crossing in front of it. The other hand swings freely.
```
(Walking toward the side of the staff hand puts it on the far side; walking the other way puts it on the near side, in front.)

Isolde's walk right (right-handed), the whole prompt:
```
Full-body shot of a young woman with long wavy copper-red hair and freckles, wearing a black studded leather corset with dark leather
shoulder guards and bracers, a long tattered black cloak, a tattered dark burgundy skirt and tall black leather boots, carrying a tall
ornate black staff whose head is a dark crescent of curved spikes cradling a red gem, standing on a flat, plain medium-grey studio floor
in front of a seamless plain medium-grey studio backdrop. Soft, even, neutral studio lighting from the front and both sides, no colored
light, no shadows on the wall. Locked-off static camera at chest height, no camera movement, no zoom, no cuts. Vertical 9:16 frame. The
whole body, the head, the feet and the entire staff stay inside the frame at all times, with a clear margin of empty backdrop above the
staff's head, below the feet and on both sides. The character fills about 80% of the frame's height. Realistic, photographic, sharp
focus, natural colors.
She walks in place to the RIGHT in exact profile view, as if on a treadmill: the body stays in the center of the frame while the legs
and the cloak move in a steady, natural walking rhythm at a constant pace, for the whole video. She holds the staff in her RIGHT hand
only, the same hand as in every other video: walking to the right, that hand is on the far side of her body, so the staff is seen
partly behind her body, upright, planted lightly with each step, never crossing in front of her. Her left arm, nearest the camera,
swings freely and empty. She is already walking in the first frame and keeps walking until the last frame, never slowing down, never
stopping, never turning toward the camera.
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
small and inside the frame: bright effects crossing the figure confuse the cut-out (Voss's cast lost half its staff), and the game
draws its own 3D spell effects anyway.

**Hit (played once)**
```
The character starts in the relaxed idle pose, is struck hard in the chest by an unseen blow from the RIGHT, staggers back one step
without leaving the frame, recovers and returns to exactly the relaxed idle pose, holding still for the last second.
```

**Death / fall (played once, held on the last frame)**
```
The character starts in the relaxed idle pose, is struck by an unseen blow, collapses and falls to the floor, and lies completely still
for the last two seconds. The whole body and the [STAFF] stay inside the frame while falling and lying down.
```
Why: Voss's death is 1204 px wide because he ends up lying across the frame; with the margin rule the fallen body and staff stay whole.

## After the video

Tell me the move and, if the action does not start at once, the second it starts. Everything else (cut-out, loop point, feet line,
size, the Média / Leve copies) is the pipeline's.
