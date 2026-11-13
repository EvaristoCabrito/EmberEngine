# Kael's three classes (docs/SKILLS_VANGUARD.md, 2026-11-13): Vanguarda (base), Mestre da Espada and Arauto da Guerra (prestige at 15).
# One run writes them into the data files the game reads; running it again changes nothing.
#   content/ember-classes.json  heroClasses (merged into the battle rules' classes), heroBaseClasses (Kael is a Vanguarda now)
#   content/data/class-stats.json, classes.json  the classes' stats and movement (the Guerreiro's numbers)
#   content/data/weapons.json, inn.json  every sword usable by the three (one-handed in the main hand or two-handed), the off hand locked,
#                                        the Medium (Conjuração Média) tier table, the class names
#   content/data/overworld.json  Kael's base class, the classes' HP and weapon types
#   content/data/action-points.json  each technique's AP (the design's), the classes' weapon category
#   content/progression.json  Kael's classes
# Usage: python -I tools/scripts/data/add_vanguard_classes.py   (from the project root)
import json
import sys

CLASSES = ["vanguard", "swordmaster", "warbringer"]
NAMES = {"vanguard": ("Vanguarda", "Vanguard"), "swordmaster": ("Mestre da Espada", "Swordmaster"), "warbringer": ("Arauto da Guerra", "Warbringer")}
ROLES = {"vanguard": "Linha de frente", "swordmaster": "Promovido — Vanguarda", "warbringer": "Promovido — Vanguarda"}
# The design's AP per technique (Riposta is a counter: the sword's own 6).
AP = {
    "measuredCut": 6, "longstepSlash": 7, "crossguard": 4, "pommelCheck": 5, "lowCut": 6, "partingCut": 7, "overheadHew": 8,
    "steadyFooting": 4, "challenge": 5, "breakGuard": 8,
    "feint": 4, "passingStep": 7, "needlePoint": 7, "secondIntention": 9, "readTheBlade": 4, "severTheRhythm": 8, "perfectCut": 9,
    "deadMeasure": 5, "masterstroke": 11,
    "drivingCut": 8, "wideCleave": 8, "sidecast": 7, "plantTheLine": 5, "shoulderThrough": 8, "crushAgainstStone": 9, "splitTheRank": 9,
    "reapTheFlanks": 9, "rollingAdvance": 10, "shatterTheLine": 11,
}


def load(path):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    return json.loads(text), text


def save(path, data, original):
    # the file's own layout: one line (compact) or indented by its first key's indent
    if "\n" not in original.strip():
        out = json.dumps(data, ensure_ascii=False, separators=(",", ":"))
    else:
        second = original.split("\n", 2)[1]
        indent = len(second) - len(second.lstrip(" "))
        out = json.dumps(data, ensure_ascii=False, indent=indent)
    if original.endswith("\n"):
        out += "\n"
    if out != original:
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(out)
        print("wrote", path)
    else:
        print("unchanged", path)


def add_all(lst, items):
    for x in items:
        if x not in lst:
            lst.append(x)


rules, _ = load("content/data/battle-rules.json")  # read only (generated): the Guerreiro's stats, the Arqueira's Medium tiers
sword = rules["classes"]["swordsman"]
medium = rules["classes"]["archer"]["tiers"]

def write_text(path, original, text):
    json.loads(text)  # still JSON
    if text != original:
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print("wrote", path)
    else:
        print("unchanged", path)


def rows(lst, per, pad):
    chunks = [", ".join(json.dumps(x, ensure_ascii=False) for x in lst[i:i + per]) for i in range(0, len(lst), per)]
    return (",\n" + " " * pad).join(chunks)


# --- content/ember-classes.json (hand laid out: the entries are inserted as text) ---
ec, ec_text = load("content/ember-classes.json")
if "vanguard" not in ec.get("heroClasses", {}):
    blocks = []
    for cid in CLASSES:
        pt, en = NAMES[cid]
        blocks.append(f'''  "{cid}": {{
   "name": "{pt}", "nameEn": "{en}", "role": "{ROLES[cid]}", "sprite": "{sword['sprite']}", "size": 1,
   "footprintW": null, "footprintH": null, "footprintOffsets": null, "resistances": {{}},
   "minRange": 1, "maxRange": 1, "mov": {sword['mov']}, "boss": false, "undead": false, "summon": false,
   "initiativeBonus": {sword['initiativeBonus']}, "shockCharges": 0, "fantomForceCharges": 0,
   "starterWeapon": "vanguard-espadao-t01-v01", "weaponTypes": ["sword"], "twoHandedSwords": true, "offHandLocked": true,
   "stats": [{rows(sword['stats'], 5, 13)}],
   "tiers": [{rows(medium, 4, 13)}],
   "spells": [{rows([["doubleStrike"]] * len(sword['spells']), 10, 13)}]
  }}''')
    about = ("Kael's classes (the user, 2026-11-13; docs/SKILLS_VANGUARD.md): Vanguarda, then Mestre da Espada or Arauto da Guerra at 15. The "
             "Guerreiro's stats, the Conjuração Média tiers (the Arqueira's table), Golpe Duplo the one Guerreiro skill kept (his techniques come "
             "from engine/world/SpellsVanguard.inc), swords only: one-handed in the main hand or two-handed (twoHandedSwords), the off hand locked "
             "(offHandLocked). heroBaseClasses: the class a hero plays as when the battle file still names one it replaced (Kael's maps say swordsman).")
    end = ec_text.rstrip().rfind("\n  }\n }\n}")
    if end < 0:
        sys.exit("content/ember-classes.json: the end of heroClasses was not where expected")
    text = (ec_text[:end] + "\n  },\n" + ",\n".join(blocks) + "\n },\n \"aboutHeroBaseClasses\": " + json.dumps(about, ensure_ascii=False) +
            ",\n \"heroBaseClasses\": {\"Kael\": {\"class\": \"vanguard\", \"replaces\": [\"swordsman\", \"kaelFinal\", \"kaelEarly\"]}}\n}\n")
    write_text("content/ember-classes.json", ec_text, text)
else:
    print("unchanged content/ember-classes.json")

# --- content/data/class-stats.json, classes.json ---
cs, cs_text = load("content/data/class-stats.json")
for cid in CLASSES:
    cs["classes"][cid] = cs["classes"]["swordsman"]
save("content/data/class-stats.json", cs, cs_text)
cl, cl_text = load("content/data/classes.json")
for cid in CLASSES:
    cl["classes"][cid] = dict(cl["classes"].get("swordsman", {"mov": 5}))
save("content/data/classes.json", cl, cl_text)

# --- content/data/weapons.json ---
wj, wj_text = load("content/data/weapons.json")
for w in wj["weapons"].values():
    if w.get("weaponType") == "sword":
        add_all(w["usableBy"], CLASSES)
for cid in CLASSES:
    base = dict(wj["classes"].get("vanguard", {"starter": "vanguard-espadao-t01-v01", "weaponTypes": ["sword"], "minRange": 1, "maxRange": 1}))
    base["offHandLocked"] = True
    wj["classes"][cid] = base
save("content/data/weapons.json", wj, wj_text)

# --- content/data/inn.json ---
inn, inn_text = load("content/data/inn.json")
names_pt = {cid: NAMES[cid][0] for cid in CLASSES}
for w in inn["weapons"]:
    if w.get("weaponType", None) == "sword" or w.get("id") in wj["weapons"] and wj["weapons"][w["id"]].get("weaponType") == "sword":
        before = list(w.get("usableBy") or [])
        add_all(w.setdefault("usableBy", []), CLASSES)
        added = [names_pt[c] for c in CLASSES if c not in before]
        if added and isinstance(w.get("tooltip"), list):
            fixed = []
            for t in w["tooltip"]:
                lines = t.split("\n")
                for i, line in enumerate(lines):
                    if line.startswith("Usável: "):
                        have = line[len("Usável: "):].split(", ")
                        lines[i] = "Usável: " + ", ".join(have + [n for n in added if n not in have])
                fixed.append("\n".join(lines))
            w["tooltip"] = fixed
for cid in CLASSES:
    inn["tierUses"][cid] = inn["tierUses"]["archer"]
    inn["classNames"][cid] = NAMES[cid][0]
inn["offHandLockedClasses"] = CLASSES
save("content/data/inn.json", inn, inn_text)
if inn["tierUses"]["archer"] != medium:
    print("note: inn.json's Arqueira tier table differs from battle-rules.json's", file=sys.stderr)

# --- content/data/overworld.json ---
ow, ow_text = load("content/data/overworld.json")
t = ow["tables"]
t["baseClass"]["Kael"] = "vanguard"
for cid in CLASSES:
    t["classes"][cid] = t["classes"]["swordsman"]
    t["classWeaponTypes"][cid] = ["sword"]
save("content/data/overworld.json", ow, ow_text)

# --- content/data/action-points.json (hand laid out) ---
ap, ap_text = load("content/data/action-points.json")
text = ap_text
if not ap["spells"]["byKind"]:
    body = ", ".join(f'"{k}": {v}' for k, v in AP.items())
    text = text.replace('"byKind": {}', '"byKindNote": "Kael\'s techniques (docs/SKILLS_VANGUARD.md): the design\'s AP for the whole technique, its bundled step '
                                         'included",\n    "byKind": {' + body + '}', 1)
if "vanguard" not in ap["weapons"]["classWeapons"]:
    text = text.replace('"paladin": "sword", "heavyKnight": "sword",', '"paladin": "sword", "heavyKnight": "sword", "vanguard": "sword", "swordmaster": "sword", '
                                                                      '"warbringer": "sword",', 1)
write_text("content/data/action-points.json", ap_text, text)

# --- content/progression.json (hand laid out) ---
pj, pj_text = load("content/progression.json")
text = pj_text
if "vanguard" not in pj["heroes"]["Kael"]["classes"]:
    text = text.replace('"classes": ["swordsman"] }', '"classes": ["swordsman", "vanguard", "swordmaster", "warbringer"] }', 1)
write_text("content/progression.json", pj_text, text)
