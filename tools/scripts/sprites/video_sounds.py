"""A unit's sounds cut from its own videos (the user, 2026-11-14: "are you also taking the good sound FX on these videos ?"): each
clip's audio, the very frames its picture was cut from, as content/assets/game/SoundFX/<Name><Kind>001.mp3, and the unit's row in
content/data/battle-rules.json sounds.monster (by sprite; engine/world/BattleExport.cpp Cues::monster reads attack, cast, walk, hit,
death from it, as the Nekomata's, the Telesm's and the Sahagin's).

- attack / cast / hit / death: the clip's frames first..last of its video (24 fps), a 30 ms fade in, 120 ms out;
- walk: the walk loop's own frames (cut-result loopFirst..loopLast), so the steps line up with the feet;
- `attackFrom`: a unit whose cast plays as its blow (no attack clip) gets its cast's sound for the attack too;
- every sound brought to one loudness (EBU R128, -18 LUFS, the -2 dBTP ceiling), the levels set as the other units' (0.55, the walk 0.45);
- a clip whose audio is silence (under -55 dB mean) is skipped, and a file of that name already there is never overwritten (the next
  number is used);
- `--borrow sprite:kind,kind`: those kinds take another unit's sounds (its row in sounds.monster) instead of the clips' own audio
  (the Ember Mushroom, the user 2026-11-17: its hit / fall video's sound not to be used, the Mush's instead).

    python -I tools/scripts/sprites/video_sounds.py <cut.json> <Name> [--attack-from cast] [--borrow sprite:kind,kind]
"""
import argparse
import json
import os
import re
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
SFX = os.path.join(ROOT, "content", "assets", "game", "SoundFX")
RULES = os.path.join(ROOT, "content", "data", "battle-rules.json")
FPS = 24.0
KINDS = {"attack": "ATT", "cast": "Cast", "walk": "Walk", "hit": "Hit", "death": "Death"}


def mean_db(video, t0, dur):
    r = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-ss", f"{t0:.3f}", "-t", f"{dur:.3f}", "-i", video, "-vn",
                        "-af", "volumedetect", "-f", "null", "-"], capture_output=True, text=True)
    m = re.search(r"mean_volume: (-?[\d.]+) dB", r.stderr)
    return float(m.group(1)) if m else -99.0


def cut(video, t0, dur, out):
    fade_out = max(0.0, dur - 0.12)
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-ss", f"{t0:.3f}", "-t", f"{dur:.3f}", "-i", video, "-vn",
                    "-af", f"afade=t=in:d=0.03,afade=t=out:st={fade_out:.3f}:d=0.12,loudnorm=I=-18:TP=-2:LRA=11",
                    "-ar", "44100", "-ac", "2", "-b:a", "160k", out], check=True)


def free_name(name, tag):
    n = 1
    while os.path.exists(os.path.join(SFX, f"{name}{tag}{n:03d}.mp3")):
        n += 1
    return f"{name}{tag}{n:03d}.mp3"


def set_monster(sprite, sounds):
    """The sprite's row in battle-rules.json sounds.monster, written as text (the file's own 2-space layout, nothing else touched)."""
    t = open(RULES, encoding="utf-8", newline="").read()
    rules = json.loads(t)
    if sprite in rules["sounds"]["monster"]:
        print(f"battle-rules: {sprite} already has sounds, left as it is")
        return
    nl = "\r\n" if "\r\n" in t else "\n"
    head = '    "monster": {' + nl
    i = t.index(head) + len(head)
    body = json.dumps({sprite: sounds}, indent=2, ensure_ascii=False)[1:-1].strip("\n")
    block = nl.join("    " + line for line in body.split("\n")) + "," + nl
    t = t[:i] + block + t[i:]
    assert json.loads(t)["sounds"]["monster"][sprite] == sounds
    open(RULES, "w", encoding="utf-8", newline="").write(t)
    print(f"battle-rules: {sprite} {sorted(sounds)}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("spec")
    ap.add_argument("name")
    ap.add_argument("--attack-from", default="")
    ap.add_argument("--borrow", default="")
    a = ap.parse_args()
    spec = json.load(open(a.spec, encoding="utf-8"))
    res = json.load(open(os.path.join(os.path.dirname(a.spec), spec.get("result", "cut-result.json")), encoding="utf-8"))["clips"]
    sprite = spec["set"]
    sounds = {}
    if a.borrow:
        donor, kinds = a.borrow.split(":", 1)
        row = json.load(open(RULES, encoding="utf-8"))["sounds"]["monster"][donor]
        for kind in kinds.split(","):
            if kind in row:
                sounds[kind] = row[kind]
                print(f"{sprite} {kind}: borrowed from {donor} ({', '.join(s['file'] for s in row[kind])})")
    for c in spec["clips"]:
        kind = c.get("pose") if c.get("pose") in KINDS else None
        if kind is None or kind in sounds:
            continue
        r = res.get(c["clip"], {})
        first, last = (r["loopFirst"], r["loopLast"]) if kind == "walk" and "loopFirst" in r else (c["first"], c["last"])
        video = os.path.join(ROOT, c["video"])
        t0, dur = first / FPS, (last - first + 1) / FPS
        db = mean_db(video, t0, dur)
        if db < -55.0:
            print(f"{sprite} {kind}: frames {first}-{last} silent ({db:.0f} dB), skipped")
            continue
        fname = free_name(a.name, KINDS[kind])
        cut(video, t0, dur, os.path.join(SFX, fname))
        sounds[kind] = [{"file": fname, "volume": 0.45 if kind == "walk" else 0.55, "offset": 0, "delay": 0, "exclusive": True}]
        print(f"{sprite} {kind}: {fname} (frames {first}-{last}, {dur:.2f} s, {db:.0f} dB before)")
    if a.attack_from and "attack" not in sounds and a.attack_from in sounds:
        sounds["attack"] = sounds[a.attack_from]
    if sounds:
        set_monster(sprite, sounds)


if __name__ == "__main__":
    main()
