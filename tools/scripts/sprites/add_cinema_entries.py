"""Cinemática copies in content/sprite-overrides.json (the user, 2026-11-21: "Cinematic Média and Cinematic Leve"): for every hero set
with a Média copy (a "mediumOf" entry whose cut dir holds the set's Alta cut.json), a "cinemaOf" entry <set>-cine-media right after
its Média entry, with the same packs: every frame of the set's clips at ~610 px (tools/scripts/sprites/quality_copies.py --tags
cine-media). apply_sprite_overrides.py skips an entry whose cut is not on disk yet (the game draws the Média copy there).

The file is hand-formatted, so the entries go in as text (one line each); nothing else in it is rewritten. Idempotent.

    python -I tools/scripts/sprites/add_cinema_entries.py
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
PATH = os.path.join(ROOT, "content", "sprite-overrides.json")


def main():
    text = open(PATH, encoding="utf-8").read()
    data = json.loads(text)
    have = {e["set"] for e in data["sets"]}
    dec = json.JSONDecoder()
    added = []
    for e in data["sets"]:
        of = e.get("mediumOf")
        if not of or not e["set"].endswith("-64-media"):
            continue
        cut_dir = os.path.dirname(e["cut"])
        alta_p = os.path.join(ROOT, cut_dir, "cut.json")
        if not os.path.exists(alta_p) or json.load(open(alta_p, encoding="utf-8"))["set"] != of["set"]:
            continue
        name = f"{of['set']}-cine-media"
        if name in have:
            continue
        entry = {"set": name, "version": "cine-v01", "cinemaOf": {"hero": of["hero"], "set": of["set"]},
                 "cut": f"{cut_dir}/cut-cine-media.json",
                 "why": "Cinemática Média / Leve (the user, 2026-11-21): every frame of the clips at ~610 px of figure (quality_copies.py cine-media)",
                 "packs": e.get("packs", {})}
        key = f'"set": "{e["set"]}"'
        at = text.index(key)
        start = text.rindex("{", 0, at)
        obj, end = dec.raw_decode(text, start)
        assert obj.get("set") == e["set"], (obj.get("set"), e["set"])
        text = text[:end] + ",\n  " + json.dumps(entry, ensure_ascii=False) + text[end:]
        added.append(name)
    check = json.loads(text)
    assert len(check["sets"]) == len(data["sets"]) + len(added)
    open(PATH + ".tmp", "w", encoding="utf-8").write(text)
    os.replace(PATH + ".tmp", PATH)
    print(f"{len(added)} entries:", ", ".join(added))


if __name__ == "__main__":
    main()
