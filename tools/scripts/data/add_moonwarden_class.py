# Garran Holt's class (docs/SKILLS_MOONWARDEN.md, 2026-11-13): the Guardião da Lua (Moonwarden), sword and shield.
# One run writes it into the data files the game reads; running it again changes nothing.
#   content/ember-classes.json  heroClasses.moonwarden (merged into the battle rules' classes): the Guerreiro's stats, the Conjuração
#                               Média tiers (the Arqueira's table, as the Vanguarda's), no Guerreiro skill (his techniques come from
#                               engine/world/SpellsMoonwarden.inc), one-handed swords in the main hand, a shield in the off hand
#   content/data/class-stats.json, classes.json  the Guerreiro's stats and movement
#   content/data/weapons.json, inn.json  every one-handed sword, every shield and every piece of armour the Guerreiro uses; the tier table;
#                                        the class name
#   content/data/overworld.json  the class's HP and weapon types
#   content/data/action-points.json  each technique's AP (the design's), the class's weapon category
#   content/progression.json  Garran Holt (the Guerreiro's gains, as Kael's)
# Usage: python -I tools/scripts/data/add_moonwarden_class.py   (from the project root)
import json
import sys

CID = "moonwarden"
PT, EN = "Guardião da Lua", "Moonwarden"
HERO = "Garran Holt"
# The design's AP per technique ("Moonwarden — Permanent Skills, T1–T10"; a normal sword blow is 6, a turn 12; Vigiar a Brecha's
# reaction blow is the sword's own 6)
AP = {"steadyStrike": 6, "digIn": 4, "flankStep": 7, "closeRanks": 5, "breakStride": 6, "watchTheGap": 4, "followThrough": 9,
      "pullClear": 5, "takeTheWeight": 6, "reckoning": 9}
# the first draft's techniques (2026-11-13, the moon-phase one), replaced by the T1-T10 set: their AP rows go
OLD = ["guardedCut", "brace", "wardenBash", "interpose", "gapThrust", "affront", "crushingBlow", "holdTheLine", "unbreakable"]


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
    write_text(path, original, out)


def write_text(path, original, text):
    json.loads(text)  # still JSON
    if text != original:
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print("wrote", path)
    else:
        print("unchanged", path)


def add(lst, x):
    if x not in lst:
        lst.append(x)
        return True
    return False


def rows(lst, per, pad):
    chunks = [", ".join(json.dumps(x, ensure_ascii=False) for x in lst[i:i + per]) for i in range(0, len(lst), per)]
    return (",\n" + " " * pad).join(chunks)


def usable_line(text):  # "Usável: a, b" + the class name
    lines = text.split("\n")
    for i, line in enumerate(lines):
        if line.startswith("Usável: ") and PT not in line[len("Usável: "):].split(", "):
            lines[i] = line + ", " + PT
    return "\n".join(lines)


rules, _ = load("content/data/battle-rules.json")  # read only (generated): the Guerreiro's stats, the Arqueira's Medium tiers
sword = rules["classes"]["swordsman"]
casters = rules["classes"]["mage"]["tiers"]  # T1-T10 open at 1, 3, 6, 8, 11, 13, 16, 18, 21, 23

# --- content/ember-classes.json (hand laid out: the entry is inserted as text) ---
ec, ec_text = load("content/ember-classes.json")
if CID not in ec.get("heroClasses", {}):
    block = f'''  "{CID}": {{
   "name": "{PT}", "nameEn": "{EN}", "role": "Linha de frente — escudo", "sprite": "{sword['sprite']}", "size": 1,
   "footprintW": null, "footprintH": null, "footprintOffsets": null, "resistances": {{}},
   "minRange": 1, "maxRange": 1, "mov": {sword['mov']}, "boss": false, "undead": false, "summon": false,
   "initiativeBonus": {sword['initiativeBonus']}, "shockCharges": 0, "fantomForceCharges": 0,
   "starterWeapon": "espada-larga", "weaponTypes": ["sword"],
   "stats": [{rows(sword['stats'], 5, 13)}],
   "tiers": [{rows(casters, 4, 13)}],
   "spells": [{rows([[]] * len(sword['stats']), 15, 13)}]
  }}'''
    anchor = '\n  }\n },\n "aboutHeroBaseClasses"'
    if anchor not in ec_text:
        sys.exit("content/ember-classes.json: the end of heroClasses was not where expected")
    text = ec_text.replace(anchor, '\n  },\n' + block + '\n },\n "aboutHeroBaseClasses"', 1)
    write_text("content/ember-classes.json", ec_text, text)
else:  # there already: its tier table set to the casters' (the T1-T10 set), the rest as it is
    start = ec_text.index(f'"{CID}": {{', ec_text.index('"heroClasses"'))
    at = ec_text.index('"tiers": [', start) + len('"tiers": [')
    depth, end = 1, at
    while depth:
        depth += {"[": 1, "]": -1}.get(ec_text[end], 0)
        end += 1
    text = ec_text[:at] + rows(casters, 4, 13) + ec_text[end - 1:]
    write_text("content/ember-classes.json", ec_text, text)

# --- content/data/class-stats.json, classes.json ---
cs, cs_text = load("content/data/class-stats.json")
cs["classes"][CID] = cs["classes"]["swordsman"]
save("content/data/class-stats.json", cs, cs_text)
cl, cl_text = load("content/data/classes.json")
cl["classes"][CID] = dict(cl["classes"].get("swordsman", {"mov": 5}))
save("content/data/classes.json", cl, cl_text)

# --- content/data/weapons.json: one-handed swords, the Guerreiro's shields ---
wj, wj_text = load("content/data/weapons.json")
one_handed = set()
for wid, w in wj["weapons"].items():
    if w.get("weaponType") == "sword" and not w.get("twoHanded"):
        add(w["usableBy"], CID)
        one_handed.add(wid)
shields = set()
for oid, o in wj["offHand"].items():
    if o.get("kind") == "shield" and "swordsman" in (o.get("usableBy") or []):
        add(o["usableBy"], CID)
        shields.add(oid)
wj["classes"][CID] = {"starter": "espada-larga", "weaponTypes": ["sword"], "minRange": 1, "maxRange": 1}
save("content/data/weapons.json", wj, wj_text)

# --- content/data/inn.json: the same swords, shields and armour, the tier table, the name ---
inn, inn_text = load("content/data/inn.json")
for w in inn["weapons"]:
    if w.get("id") in one_handed and add(w.setdefault("usableBy", []), CID) and isinstance(w.get("tooltip"), list):
        w["tooltip"] = [usable_line(t) for t in w["tooltip"]]
for e in inn["equipment"]:
    if (e.get("id") in shields or "swordsman" in (e.get("usableBy") or [])) and add(e.setdefault("usableBy", []), CID):
        if isinstance(e.get("tooltip"), str):
            e["tooltip"] = usable_line(e["tooltip"])
        elif isinstance(e.get("tooltip"), list):
            e["tooltip"] = [usable_line(t) for t in e["tooltip"]]
inn["tierUses"][CID] = casters
inn["classNames"][CID] = PT
save("content/data/inn.json", inn, inn_text)

# --- content/data/overworld.json ---
ow, ow_text = load("content/data/overworld.json")
t = ow["tables"]
t["classes"][CID] = t["classes"]["swordsman"]
t["classWeaponTypes"][CID] = ["sword"]
save("content/data/overworld.json", ow, ow_text)

# --- content/data/action-points.json (hand laid out: added to byKind's line) ---
ap, ap_text = load("content/data/action-points.json")
text = ap_text
for k in OLD:  # the replaced techniques' rows
    text = text.replace(f', "{k}": {ap["spells"]["byKind"][k]}', "", 1) if k in ap["spells"]["byKind"] else text
missing = {k: v for k, v in AP.items() if ap["spells"]["byKind"].get(k) != v}
for k in list(missing):  # a row there with another value: its value changed in place
    if k in ap["spells"]["byKind"]:
        text = text.replace(f'"{k}": {ap["spells"]["byKind"][k]}', f'"{k}": {missing.pop(k)}', 1)
if missing:
    last = list(json.loads(text)["spells"]["byKind"].items())[-1]  # the line as it is now (the replaced rows gone)
    old = f'"{last[0]}": {last[1]}}}'
    if old not in text:
        sys.exit("content/action-points.json: the end of byKind was not where expected")
    text = text.replace(old, f'"{last[0]}": {last[1]}, ' + ", ".join(f'"{k}": {v}' for k, v in missing.items()) + "}", 1)
if CID not in ap["weapons"]["classWeapons"]:
    text = text.replace('"warbringer": "sword",', f'"warbringer": "sword", "{CID}": "sword",', 1)
write_text("content/data/action-points.json", ap_text, text)

# --- content/progression.json (hand laid out) ---
pj, pj_text = load("content/progression.json")
text = pj_text
if HERO not in pj["heroes"]:
    anchor = '  "Aldric":'
    if anchor not in text:
        sys.exit("content/progression.json: Aldric's line was not where expected")
    text = text.replace(anchor, f'  "{HERO}": {{ "gain": [5, 2, 1, 3, 2], "classes": ["{CID}"] }},\n' + anchor, 1)
write_text("content/progression.json", pj_text, text)
