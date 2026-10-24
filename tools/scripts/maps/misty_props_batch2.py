"""The user's second late batch of Misty Cave decorations (Attachments/Underworld, 2026-10-23 night; docs/MISTY_MISSING_ART.md): one new
version of each map it touches (serial + 1; the old versions are only read), everything else as it was. Uses misty_add_props.py's
placement and checks.

    python -I tools/scripts/maps/misty_props_batch2.py

- misty-blackwater-shelf (As Margens do Rio Negro): the wardens' wrecked ferries on the silt banks, gypsum boulders as cover on the
  terraces, rimstone pools of cave pearls, a wardens' cairn beside each landing that leads on (down to the Garganta, to the Micélio),
  and its gypsum column drawn whole (v02) in place of v01;
- mordavia-underdark (O Underdark de Mordávia): the rising bough drawn whole, forking to bare tips (v02), in place of v01;
- misty-cave-dungeon (As Profundezas Enevoadas) and misty-root-descent (A Garganta das Raízes): the columns drawn whole (v02, their tops
  broken where they met the roof) in place of the v01 pieces whose tops ran off the photo, on the same cells.
"""
import json
import math
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import misty_add_props as base  # noqa: E402

base.BLOCKING |= {"ember-river-gypsum-boulder", "ember-river-ferry-wreck"}
SWAP = {"ember-cave-column": "ember-cave-column-v2", "ember-cave-column-slender": "ember-cave-column-slender-v2",
        "ember-cave2-root-column": "ember-cave2-root-column-v2", "ember-river-column": "ember-river-column-v2",
        "ember-cave4-bough": "ember-cave4-bough-v2"}


def cairns(d):
    """A cairn on the floor beside each connector that leads on (not the way back)."""
    out = []
    cols, rows = d["cols"], d["rows"]
    taken = {(x["x"], x["y"]) for x in d["decorations"]}
    for x in d["decorations"]:
        if not x.get("targetMapId") or x.get("returnConnector"):
            continue
        best = None
        for (c, r) in base.neighbors(x["x"], x["y"], cols, rows):
            for (c2, r2) in [(c, r), *base.neighbors(c, r, cols, rows)]:
                if (c2, r2) in taken or d["tiles"][r2 * cols + c2] not in ("plains", "nave", "snow"):
                    continue
                dd = math.dist(base.pos(c2, r2), base.pos(x["x"], x["y"]))
                if 1.5 <= dd and (best is None or dd < best[0]):
                    best = (dd, (c2, r2))
        if best:
            taken.add(best[1])
            out.append({"id": "ember-river-cairn", "x": best[1][0], "y": best[1][1]})
    return out


def swap_columns(d):
    n = 0
    for x in d["decorations"]:
        if x["id"] in SWAP:
            x["id"] = SWAP[x["id"]]
            n += 1
    if not n:
        raise SystemExit(f"{d['id']}: no v01 column to swap")
    return []


def river(d):
    swap_columns(d)
    return cairns(d) + base.place(d, [
        ("ember-river-ferry-wreck", ("snow",), 2, 9.0, 2.4),
        ("ember-river-gypsum-boulder", ("plains",), 4, 7.0, 2.4),
        ("ember-river-pearl-pool", ("plains", "snow"), 5, 6.0, 1.8)])


def main():
    work = {
        "misty-blackwater-shelf": river,
        "misty-cave-dungeon": swap_columns,
        "misty-root-descent": swap_columns,
        "mordavia-underdark": swap_columns,
    }
    for map_id, add in work.items():
        serial, name = base.newest(map_id)
        doc = json.load(open(os.path.join(base.MAPS, name), encoding="utf-8"))
        d = doc["draft"]
        new = add(d)
        d["decorations"] = d["decorations"] + new
        lost = base.check(d)
        out = os.path.join(base.MAPS, f"{map_id}{serial + 1:03d}.json")
        if os.path.exists(out):
            raise SystemExit(f"{out} exists")
        doc["serial"] = serial + 1
        doc["savedAt"] = int(time.time() * 1000)
        open(out, "w", encoding="utf-8", newline="\n").write(json.dumps(doc, ensure_ascii=False, indent=2))
        swapped = sum(1 for x in d["decorations"] if x["id"] in SWAP.values())
        print(f"{name} -> {os.path.basename(out)}: +{len(new)} ({', '.join(sorted({x['id'] for x in new}))}), {swapped} v02 columns; "
              f"{lost} floor cells only behind doors or pieces")


if __name__ == "__main__":
    main()
