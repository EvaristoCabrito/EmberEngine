"""Monster Testing 1 a 4: the unit test series (the user, 2026-11-25: "você consegue entender o conceito de um mapa de teste de unidades?
porque insiste em repetir unidades várias vezes no mesmo mapa e continuar usando as mesmas nos outros mapas da série, porque insiste em
utilizar as unidades placeholder low res ao invés das novas acima de 36 frames?").

The concept, for every agent:
- the series covers every new unit, each one exactly ONCE in the whole series: never twice on one map, never again on another map of the
  series, never two classes of the same art (Slime / Slime Grande, the Zombie copies);
- only the new units: a sprite set with Alta + 64 + 36 and more than 36 frames (build/scenes/roster.json: the pose's `medium` and
  `light` filled). The old low-res placeholders (4, 8, 12, 36 frames) never go on these maps;
- at most eight enemies a map (24 on one map ran out of the user's video memory);
- the party (Aldric, Kael, Isolde, Neera, Salazar, Voss) in front, the new summons with them as allies, each summon on one map only.
A new unit goes into the map of the series with the fewest units (a fifth map when all have eight), never onto a second map.
The script checks all this (no class twice, no art twice, only new sets) before it writes anything.

Each map is the open plains board of content/maps/ember/monstros-novos-c002.json, written as content/maps/ember/<id>002.json, set active
in content/maps/active.json and listed in the test mode's Cenários (content/data/campaign.json missions, place "Teste"). Todas as Unidades,
which held every class twice over with the placeholders, leaves the list.

    python -I tools/scripts/maps/make_unit_tests.py
"""
import copy
import json
import os
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
MAPS = os.path.join(ROOT, "content", "maps", "ember")
TEMPLATE = os.path.join(MAPS, "monstros-novos-c002.json")
ACTIVE = os.path.join(ROOT, "content", "maps", "active.json")
CAMPAIGN = os.path.join(ROOT, "content", "data", "campaign.json")
RULES = os.path.join(ROOT, "content", "data", "battle-rules.json")
EMBER = os.path.join(ROOT, "content", "ember-classes.json")
ROSTER = os.path.join(ROOT, "build", "scenes", "roster.json")
VERSION = 2
LEAVE_LIST = ["unidades-novas"]  # Todas as Unidades: every class, the placeholders and the copies

# (map id, title, enemies, summons on the party's side); the big ones (size 3-4) one or two a map, in the back row
SERIES = [
    ("monster-testing-a", "Monster Testing 1",
     ["stag", "boar", "swampBlueDeer", "horse", "nekomata", "sickRat", "troll"], ["zombieDog"]),
    ("monster-testing-b", "Monster Testing 2",
     ["omnieye", "sahagin", "vampireMosquito", "screechingBat", "witch", "hypostasis", "birolhoLegs"], ["telesm"]),
    ("monster-testing-c", "Monster Testing 3",
     ["giantBeetle", "deepgroundBeetle", "mushroomMan", "emberMushroom", "corrosiveSlime", "giantSpider", "birolhoLegs2"], ["familiar4"]),
    ("monster-testing-d", "Monster Testing 4",
     ["emberedVanguard", "emberedZombie", "banditHorseman", "bigBlueCalf", "ashesCrawler", "roccoTheBird"], []),
]
ENEMY_AT = [(12, 36), (16, 36), (20, 36), (24, 36), (12, 31), (16, 31), (20, 31), (24, 31)]
SUMMON_AT = [(26, 44), (24, 44)]
PARTY = ("aldric", "kaelFinal", "conjurer", "neera", "salazar", "voss")


def new_set(entry):
    """Alta + 64 + 36 and more than 36 frames: the new art."""
    poses = entry["poses"]
    idle = next((p for p in poses if p["pose"] == "idle"), poses[0])
    return bool(idle.get("medium") and idle.get("light")) and max(p.get("frames", 0) for p in poses) > 36


def main():
    classes = {**json.load(open(EMBER, encoding="utf-8"))["classes"], **json.load(open(RULES, encoding="utf-8"))["classes"]}
    roster = {e["key"]: e for e in json.load(open(ROSTER, encoding="utf-8"))["heroes"]}
    seen, art = {}, {}
    for mid, _, enemies, summons in SERIES:
        assert len(enemies) <= len(ENEMY_AT) and len(summons) <= len(SUMMON_AT), mid
        for c in enemies + summons:
            assert c in classes, f"{c}: not a class"
            assert c not in seen, f"{c} on {seen.get(c)} and {mid}: a unit goes on one map of the series only"
            seen[c] = mid
            e = roster.get(c)
            assert e and new_set(e), f"{c}: not a new set (Alta + 64 + 36, over 36 frames)"
            s = e["poses"][0].get("set")
            assert s not in art, f"{c} and {art.get(s)} are the same art ({s})"
            art[s] = c

    tpl = json.load(open(TEMPLATE, encoding="utf-8"))
    party = [u for u in tpl["draft"]["playerSpawns"] if u["classId"] in PARTY]
    assert [u["classId"] for u in party] == list(PARTY), "the template's party changed"
    level = tpl["draft"]["enemySpawns"][0].get("level", 15)
    active = json.loads(open(ACTIVE, encoding="utf-8").read())
    camp_text = open(CAMPAIGN, encoding="utf-8").read()
    camp = json.loads(camp_text)
    camp["missions"] = [x for x in camp["missions"] if x.get("id") not in LEAVE_LIST]
    for mid, title, enemies, summons in SERIES:
        names = ", ".join(classes[c].get("name", c) for c in enemies)
        briefing = f"Unidades novas (Alta, 64 e 36), cada uma só neste mapa da série: {names}" + (
            f"; no seu grupo: {', '.join(classes[c].get('name', c) for c in summons)}." if summons else ".")
        objective = "Ver cada unidade andar, atacar, levar dano e cair"
        m = copy.deepcopy(tpl)
        d = m["draft"]
        m["serial"], m["savedAt"] = VERSION, int(time.time() * 1000)
        d.update(id=mid, title=title, briefing=briefing, objective=objective)
        d["playerSpawns"] = copy.deepcopy(party) + [
            {"name": classes[c].get("name", c), "classId": c, "x": x, "y": y, "level": level, "useClassSprite": True}
            for c, (x, y) in zip(summons, SUMMON_AT)]
        d["enemySpawns"] = [{"name": classes[c].get("name", c), "classId": c, "x": x, "y": y, "level": level, "useClassSprite": True}
                            for c, (x, y) in zip(enemies, ENEMY_AT)]
        d["neutralSpawns"] = []
        path = os.path.join(MAPS, f"{mid}{VERSION:03d}.json")
        open(path, "w", encoding="utf-8").write(json.dumps(m, indent=2, ensure_ascii=False) + "\n")
        active[mid] = VERSION
        entry = {"id": mid, "index": 0, "title": title, "place": "Teste", "briefing": briefing, "objective": objective, "win": "rout",
                 "mapFile": f"{mid}000.json"}
        at = next((k for k, x in enumerate(camp["missions"]) if x.get("id") == mid), None)
        if at is None:
            at = max((k for k, x in enumerate(camp["missions"]) if str(x.get("id", "")).startswith("monster-testing")), default=-1) + 1 or len(camp["missions"])
            camp["missions"].insert(at, entry)
        else:
            camp["missions"][at] = entry
        print(path, len(d["enemySpawns"]), "enemies,", len(d["playerSpawns"]), "on the party's side")
    open(ACTIVE, "w", encoding="utf-8").write(json.dumps(dict(sorted(active.items())), indent=2, ensure_ascii=False) + "\n")
    compact = json.dumps(camp, ensure_ascii=False, separators=(",", ":"))
    open(CAMPAIGN, "w", encoding="utf-8").write(compact + ("\n" if camp_text.endswith("\n") else ""))
    print(len(seen), "units, each once; active.json and campaign.json updated")


if __name__ == "__main__":
    main()
