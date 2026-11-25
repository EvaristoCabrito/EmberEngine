"""Monster Testing maps (the user, 2026-11-25: "eu quero novos mapas de Monster Testing com outras unidades que nunca aparecem").

The units of content/data/battle-rules.json that stand on no test map (Todas as Unidades, Monstros Novos 3), split into small maps of
at most eight, each the layout of Monstros Novos 3 (content/maps/ember/monstros-novos-c002.json: the open plains board, the enemies in
two rows of four in front of the party): the party (Aldric, Kael, Isolde, Neera, Salazar, Voss) in front, the summons with them as allies.
Each map is written as content/maps/ember/<id>001.json, added to content/maps/active.json and to the scenario list
(content/data/campaign.json missions, place "Teste", as Todas as Unidades), so the test mode's Cenários shows it.

    python -I tools/scripts/maps/make_monster_testing.py
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

GROUPS = [
    ("monster-testing-a", "Monster Testing 1 · Feras", "As feras que não estão em nenhum mapa de teste: cães, lobos, o cervo azul, o Horror e a Asherah; os três Familiares no seu grupo.",
     ["wardog", "mordavianWolf", "morvenianWolf", "swampBlueCalf", "horror", "asherah"], ["familiar", "familiar2", "familiar3"]),
    ("monster-testing-b", "Monster Testing 2 · Soldados", "Os soldados e combatentes de linha que não estão em nenhum mapa de teste.",
     ["swordsman", "archer", "soldier", "miliciaV2", "brigand", "captain", "lancer", "pikeman"], []),
    ("monster-testing-c", "Monster Testing 3 · Bandidos e magos", "Os bandidos, cultistas e magos que não estão em nenhum mapa de teste.",
     ["cultist", "cultistV2", "assassin", "rogue", "warlock", "sorcerer", "necromancer", "ranger"], []),
    ("monster-testing-d", "Monster Testing 4 · Guardas e clérigos", "Os guardas, cavaleiros e clérigos que não estão em nenhum mapa de teste.",
     ["sentinel", "templar", "heavyKnight", "paladin", "healer", "cleric", "bishop", "elementalist"], []),
]
ENEMY_AT = [(12, 36), (16, 36), (20, 36), (24, 36), (12, 31), (16, 31), (20, 31), (24, 31)]
SUMMON_AT = [(26, 44), (28, 44), (30, 44)]


def main():
    classes = json.load(open(RULES, encoding="utf-8"))["classes"]
    tpl = json.load(open(TEMPLATE, encoding="utf-8"))
    party = [u for u in tpl["draft"]["playerSpawns"] if u["classId"] in ("aldric", "kaelFinal", "conjurer", "neera", "salazar", "voss")]
    level = tpl["draft"]["enemySpawns"][0].get("level", 15)
    active_text = open(ACTIVE, encoding="utf-8").read()
    active = json.loads(active_text)
    camp_text = open(CAMPAIGN, encoding="utf-8").read()
    camp = json.loads(camp_text)
    for mid, title, briefing, enemies, summons in GROUPS:
        for c in enemies + summons:
            assert c in classes, f"{c} is not a class in battle-rules.json"
        m = copy.deepcopy(tpl)
        d = m["draft"]
        m["serial"], m["savedAt"] = 1, int(time.time() * 1000)
        d.update(id=mid, title=title, briefing=briefing, objective="Ver cada unidade andar, atacar, levar dano e cair")
        d["playerSpawns"] = copy.deepcopy(party) + [
            {"name": classes[c].get("name", c), "classId": c, "x": x, "y": y, "level": level} for c, (x, y) in zip(summons, SUMMON_AT)]
        d["enemySpawns"] = [{"name": classes[c].get("name", c), "classId": c, "x": x, "y": y, "level": level, "useClassSprite": True}
                            for c, (x, y) in zip(enemies, ENEMY_AT)]
        d["neutralSpawns"] = []
        path = os.path.join(MAPS, f"{mid}001.json")
        open(path, "w", encoding="utf-8").write(json.dumps(m, indent=2, ensure_ascii=False) + "\n")
        active[mid] = 1
        if not any(x.get("id") == mid for x in camp["missions"]):
            i = next(k for k, x in enumerate(camp["missions"]) if x.get("id") == "unidades-novas")
            n = sum(1 for x in camp["missions"] if str(x.get("id", "")).startswith("monster-testing"))
            camp["missions"].insert(i + 1 + n, {"id": mid, "index": 0, "title": title, "place": "Teste", "briefing": briefing,
                                                "objective": d["objective"], "win": "rout", "mapFile": f"{mid}000.json"})
        print(path, len(d["enemySpawns"]), "enemies,", len(d["playerSpawns"]), "party")
    # the files' own formats: active.json sorted, 2-space indent; campaign.json one compact line
    open(ACTIVE, "w", encoding="utf-8").write(json.dumps(dict(sorted(active.items())), indent=2, ensure_ascii=False) + "\n")
    compact = json.dumps(camp, ensure_ascii=False, separators=(",", ":"))
    open(CAMPAIGN, "w", encoding="utf-8").write(compact + ("\n" if camp_text.endswith("\n") else ""))
    print("active.json and campaign.json updated")


if __name__ == "__main__":
    main()
