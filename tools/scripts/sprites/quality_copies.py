"""A hero's Média and Leve copies from their Alta cut spec, by the game's one rule (the user, 2026-10-25: "make them all like this
for medium, and low get 610, anything less is just nasty looking"; frame counts are only ever 36 or 64):

    Alta   every frame, the video's own size (the cut spec itself)
    Média  at most 64 frames, the figure ~916 px tall (Neera Hydra's height; a clip already shorter keeps its own size)
    Leve   at most 36 frames, the figure ~610 px tall (never drawn smaller: MapStage pins a copy without halving it)

Writes <spec dir>/cut-64-media.json and cut-36-media.json (sets <set>-64-media / <set>-36-media, results cut-result-64-media.json /
cut-result-36-media.json) as copies of every clip of the Alta spec (the same frames, keys, colour match, bodyAt), with `scaleTo` and
`frameCount`. A new `version` puts the frames in new folders, so the copies cut before stay on disk.

    python -I tools/scripts/sprites/quality_copies.py content/sprites/<set>/cut.json <version suffix, e.g. v02>
"""
import copy
import json
import os
import sys

RULE = (("64-media", "Média", 64, 916), ("36-media", "Leve", 36, 610))


def main():
    spec_path, suffix = sys.argv[1], sys.argv[2]
    here = os.path.dirname(spec_path)
    alta = json.load(open(spec_path, encoding="utf-8"))
    for tag, quality, frames, height in RULE:
        s = copy.deepcopy(alta)
        s["about"] = (f"{alta['set']}'s Personagens {quality} (tools/scripts/sprites/quality_copies.py, the rule of 2026-10-25): every clip of "
                      f"cut.json from the same matted frames, at most {frames} frames picked evenly (no blend, the clip's timing kept), the "
                      f"figure ~{height} px tall (a clip already shorter keeps its own size).")
        s["set"] = f"{alta['set']}-{tag}"
        s["version"] = f"media{frames}-{suffix}"
        s["result"] = f"cut-result-{tag}.json"
        s["frameCount"] = frames
        for c in s["clips"]:
            c["scaleTo"] = height
            c.pop("why", None)
            c.pop("version", None)  # a clip's own Alta version (e.g. Voss's cast v02) is not the copy's
        out = os.path.join(here, f"cut-{tag}.json")
        with open(out, "w", encoding="utf-8") as f:
            json.dump(s, f, indent=1, ensure_ascii=False)
            f.write("\n")
        print(f"{out}: set {s['set']} version {s['version']}, {len(s['clips'])} clips, {frames} frames, ~{height} px")


if __name__ == "__main__":
    main()
