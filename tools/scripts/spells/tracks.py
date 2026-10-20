"""Each spell's scaling track (the user, 2026-10-20): "cada nível é uma pequena melhoria, apenas uma pequena"; "são 15 níveis de
escalonamento"; "só irei abrir além do 30 no Ember Ashes II, mas tem que programar certo de primeira".

A spell is learned on its tier's first level for the class; the 14 levels after it bring one small improvement each (15 levels of
scaling with the learning one), never two on one level. A spell learned at 25 keeps scaling to 39: those levels open in Ember Ashes II.

Writes kSpellTracks into engine/world/Spells.cpp (between TRACKS-BEGIN / TRACKS-END), the scaling sentences into Como jogar
(content/engine2/help.json) and docs/SPELLS_SCALING.md. Run from the repository root: python -I tools/scripts/spells/tracks.py"""
import io, json, re, sys

sys.stdout.reconfigure(encoding="utf-8")
SLOTS = 14
SRC = "engine/world/Spells.cpp"
src = io.open(SRC, encoding="utf-8", newline="").read()
ours_learn = {k: int(l) for k, l in re.findall(r'\{\{"(\w+)", "[^"]+", \d+, "[^"]*", (?:true|false)\}, "\w+", (\d+)\}', src)}

# kind: (learn level or None = ours (kEmberSpells), base values for the text, [(letter, step, count)])
# letters: r reach (hexes; the bow shots: hexes past the bow), a area ring (from 3 hexes), l length, d dice, f faces, b bonus,
# m weight on MAG (bow / lance skills: on the weapon's roll), c count, p percent, t turns, R rounds, s spread, j jumps, e decay,
# P push, k / K tick dice / faces, T tick weight, h / H the burning ground's dice / faces, g / G / M the ring's dice / faces / weight,
# u duration dice, v poison tier, x chance
S = {
    # --- Mago Negro --------------------------------------------------------------------------------------------------
    "magicMissile": (1, dict(r=5, c=1, m=1.15), [("r", 1, 7), ("c", 1, 4), ("m", 0.05, 3)]),
    "lightning": (3, dict(r=5, d=2, f=8, m=2.0), [("r", 1, 7), ("d", 1, 3), ("m", 0.1, 4)]),
    "frost": (5, dict(l=2, d=1, f=6, m=1.1), [("l", 1, 6), ("d", 1, 3), ("m", 0.05, 5)]),
    "poisonBreath": (3, dict(l=1, d=1, f=4, m=1.05), [("l", 1, 2), ("d", 1, 2), ("f", 2, 2), ("m", 0.04, 8)]),
    "fireball": (6, dict(r=5, a=0, d=2, f=6), [("r", 1, 7), ("a", 1, 3), ("d", 1, 4)]),
    "causticVenom": (11, dict(r=7, a=0, d=1, f=10, m=1.5), [("r", 1, 5), ("a", 1, 4), ("d", 1, 2), ("m", 0.1, 3)]),
    "lightningTier3": (15, dict(r=7, d=3, f=12, m=3.0), [("r", 1, 5), ("d", 1, 3), ("m", 0.1, 6)]),
    # --- Curandeiro / Paladino -----------------------------------------------------------------------------------------
    "cureMinor": (1, dict(r=2, d=1, f=6, m=1.0), [("r", 1, 4), ("d", 1, 3), ("m", 0.05, 7)]),
    "bless": (3, dict(a=0, p=1, t=3), [("a", 1, 2), ("p", 1, 7), ("t", 1, 5)]),
    "cureWounds": (3, dict(r=3, d=2, f=6, m=1.6), [("r", 1, 2), ("d", 1, 3), ("m", 0.05, 9)]),
    "burningHands": (5, dict(l=1, d=1, f=4, m=1.05), [("l", 1, 2), ("d", 1, 3), ("f", 2, 2), ("m", 0.05, 7)]),
    "turnUndead": (6, dict(a=0, d=4, f=6, m=1.6), [("a", 1, 2), ("d", 1, 5), ("m", 0.1, 7)]),
    "cureDisease": (6, dict(r=2), [("r", 1, 2)]),
    "divineBolt": (8, dict(r=6, a=0, d=2, f=4, m=1.4), [("r", 1, 6), ("a", 1, 2), ("d", 1, 3), ("m", 0.1, 3)]),
    "circleOfHealing": (None, dict(r=4, a=0, d=1, f=8, m=1.2), [("r", 1, 6), ("a", 1, 2), ("d", 1, 3), ("m", 0.1, 3)]),
    "resurrection": (None, dict(r=1, d=2, f=8, m=1.5), [("r", 1, 3), ("d", 1, 3), ("f", 2, 1), ("m", 0.1, 7)]),
    "judgment": (None, dict(r=6, a=0, d=2, f=8, m=1.8, T=1.5), [("r", 1, 4), ("a", 1, 2), ("d", 1, 3), ("m", 0.15, 4), ("T", 0.25, 1)]),
    "cureLight": (11, dict(r=1, d=1, f=8, m=1.2), [("r", 1, 2), ("d", 1, 3), ("m", 0.05, 9)]),
    "auraOfProtection": (15, dict(a=0, p=20, t=3), [("a", 1, 2), ("p", 2, 8), ("t", 1, 4)]),
    "intimidatingPresence": (15, dict(a=0, p=20, t=3), [("a", 1, 2), ("p", 2, 9), ("t", 1, 3)]),
    "divineWrath": (18, dict(r=4, d=1, f=10), [("r", 1, 8), ("d", 1, 5), ("f", 2, 1)]),
    # --- Conjurador / Necromante ---------------------------------------------------------------------------------------
    "summonFamiliar": (1, dict(r=2), [("r", 1, 6)]),
    "phantasmalForce": (2, dict(r=6, d=1, f=4, m=1.0), [("r", 1, 6), ("f", 2, 4), ("d", 1, 2), ("m", 0.1, 2)]),
    "webOfDreams": (3, dict(r=7, a=0, x=25), [("r", 1, 5), ("a", 1, 2), ("x", 5, 7)]),
    "summonFamiliar2": (3, dict(r=2), [("r", 1, 6)]),
    "placeSwap": (None, dict(r=6), [("r", 1, 4)]),
    "boneLance": (None, dict(l=3, d=1, f=8, m=1.3), [("l", 1, 4), ("d", 1, 3), ("f", 2, 2), ("m", 0.1, 5)]),
    "summonFamiliar4": (6, dict(r=2), [("r", 1, 6)]),
    "mirrorImage": (None, dict(r=6, c=2, R=3, p=33), [("r", 1, 4), ("c", 1, 2), ("R", 1, 3), ("p", 3, 5)]),
    "detonateSummon": (None, dict(r=6, a=0, d=2, f=8, m=1.8), [("r", 1, 4), ("a", 1, 2), ("d", 1, 3), ("m", 0.12, 5)]),
    "summonFamiliar3": (11, dict(r=2), [("r", 1, 6)]),  # Familiar Titã: T5 (the user, 2026-10-20)
    "sacrificeBond": (None, dict(p=100), [("p", -3, 14)]),
    "nightmare": (None, dict(r=6, a=0, d=2, f=8, m=1.5, p=25, t=1), [("r", 1, 6), ("a", 1, 2), ("d", 1, 2), ("m", 0.15, 2), ("p", 10, 1), ("t", 1, 1)]),
    "summonZombieDog": (15, dict(r=2), [("r", 1, 6)]),  # Cão Zumbi: the Necromante's T6 (the user, 2026-10-20)
    "empowerSummons": (None, dict(p=25, R=3), [("p", 1, 12), ("R", 1, 2)]),
    # --- Bruxo -----------------------------------------------------------------------------------------------------------
    "markOfRuin": (None, dict(r=6, d=1, f=6, m=1.15, p=20, t=3), [("r", 1, 5), ("p", 5, 3), ("m", 0.05, 3), ("t", 1, 1), ("d", 1, 1), ("f", 2, 1)]),
    "creepingPlague": (None, dict(r=6, d=1, f=8, m=1.3, v="greater", s=1), [("r", 1, 5), ("m", 0.05, 4), ("d", 1, 2), ("v", 1, 1), ("s", 1, 1), ("f", 2, 1)]),
    "lifeDrain": (None, dict(r=5, d=2, f=6, m=1.6, p=50), [("m", 0.05, 7), ("r", 1, 2), ("f", 2, 2), ("p", 25, 2), ("d", 1, 1)]),
    "shroudOfShadows": (None, dict(r=6, a=0, k=1, K=6, R=2, p=20), [("r", 1, 6), ("a", 1, 2), ("p", 5, 3), ("R", 1, 2), ("k", 1, 1)]),
    "hemorrhage": (None, dict(r=5, d=2, f=6, m=1.4, k=2, K=6, u=1), [("m", 0.05, 6), ("r", 1, 4), ("k", 1, 1), ("K", 2, 1), ("u", 1, 1), ("d", 1, 1)]),
    "profaneSilence": (None, dict(r=6, a=0, d=1, f=8, m=1.1, t=2), [("r", 1, 4), ("m", 0.05, 4), ("a", 1, 2), ("t", 1, 2), ("d", 1, 1), ("f", 2, 1)]),
    "blackPestilence": (None, dict(r=7, a=0, d=2, f=6, m=1.2, v="deadly"), [("m", 0.05, 5), ("r", 1, 2), ("a", 1, 2), ("d", 1, 2), ("f", 2, 2), ("v", 1, 1)]),
    "curseOfDecay": (None, dict(r=6, d=2, f=8, m=1.5, p=30, t=3, T=0.5, k=1, K=8), [("m", 0.05, 4), ("r", 1, 2), ("p", 10, 2), ("T", 0.1, 2), ("t", 1, 1), ("d", 1, 1), ("k", 1, 1), ("K", 2, 1)]),
    "soulHarvest": (None, dict(r=6, a=0, d=3, f=6, m=1.7, p=10), [("r", 1, 6), ("a", 1, 2), ("m", 0.1, 2), ("p", 2, 2), ("d", 1, 1), ("f", 2, 1)]),
    "devouringVoid": (None, dict(r=7, d=3, f=8, m=2.2, p=15, s=30), [("r", 1, 4), ("m", 0.05, 4), ("p", 5, 2), ("s", 5, 2), ("d", 1, 1), ("f", 2, 1)]),
    # --- Elementalista ---------------------------------------------------------------------------------------------------
    "pillarOfFlame": (None, dict(r=6, a=0, d=2, f=8, m=2.0, h=1, R=1), [("r", 1, 5), ("m", 0.1, 3), ("a", 1, 2), ("d", 1, 1), ("R", 1, 1), ("h", 1, 1), ("f", 2, 1)]),
    "iceLance": (None, dict(r=7, d=2, f=10, m=2.4, p=50), [("r", 1, 5), ("m", 0.05, 5), ("p", 25, 2), ("d", 1, 1), ("f", 2, 1)]),
    "thunderstorm": (None, dict(r=6, a=0, d=2, f=12, m=1.8, k=1, b=4), [("r", 1, 6), ("m", 0.1, 3), ("a", 1, 2), ("d", 1, 1), ("k", 1, 1), ("b", 2, 1)]),
    "meteor": (None, dict(r=8, a=0, d=4, f=6, m=3.0, g=2, M=2.0), [("r", 1, 4), ("m", 0.1, 3), ("a", 1, 2), ("M", 0.2, 2), ("d", 1, 1), ("g", 1, 1), ("f", 2, 1)]),
    "absoluteZero": (None, dict(r=6, a=0, d=3, f=8, m=2.2), [("m", 0.05, 6), ("r", 1, 4), ("a", 1, 2), ("d", 1, 1), ("f", 2, 1)]),
    "chainLightning": (None, dict(r=7, d=3, f=10, m=2.8, j=3, e=0.15, l=3), [("r", 1, 5), ("m", 0.05, 3), ("j", 1, 2), ("e", -0.025, 2), ("d", 1, 1), ("l", 1, 1)]),
    "eruption": (None, dict(a=0, d=4, f=8, m=2.6, h=1, H=8, R=2), [("m", 0.05, 6), ("a", 1, 2), ("R", 1, 2), ("d", 1, 1), ("h", 1, 1), ("f", 2, 1), ("H", 2, 1)]),
    "glacialGale": (None, dict(l=5, d=4, f=6, m=2.4, P=1), [("m", 0.05, 7), ("l", 1, 3), ("f", 2, 2), ("d", 1, 1), ("P", 1, 1)]),
    "wrathOfTheSkies": (None, dict(r=8, d=3, f=12, m=2.6, c=4, b=0), [("r", 1, 4), ("m", 0.1, 4), ("b", 1, 4), ("c", 1, 1), ("d", 1, 1)]),
    "primordialEmber": (None, dict(r=7, a=0, d=4, f=10, m=3.4, h=2, H=8, R=2), [("r", 1, 3), ("m", 0.1, 3), ("a", 1, 2), ("d", 1, 1), ("h", 1, 1), ("R", 1, 1), ("f", 2, 1), ("H", 2, 1), ("b", 1, 1)]),
    # --- Arqueira (bow skills: the dice are a bonus on the bow's roll; r on the shots = hexes past the bow, at most 4) -----------
    "longShot": (1, dict(r=7, d=1, f=8, b=0), [("r", 1, 5), ("d", 1, 3), ("b", 1, 4), ("f", 2, 2)]),
    "venomArrow": (None, dict(r=0, d=1, f=4, m=1.0, v="lesser"), [("r", 1, 4), ("v", 1, 3), ("m", 0.05, 3), ("d", 1, 2), ("f", 2, 2)]),
    "bloodyShot": (5, dict(r=6, m=1.5, k=1, K=6), [("r", 1, 6), ("m", 0.15, 6), ("k", 1, 1), ("K", 2, 1)]),
    "multiShot": (7, dict(r=6, c=2, d=0, f=4), [("r", 1, 6), ("d", 1, 3), ("f", 2, 3), ("c", 1, 2)]),
    "arrowRain": (None, dict(r=6, a=0, d=1, f=4, m=0.75), [("r", 1, 6), ("m", 0.05, 4), ("a", 1, 2), ("d", 1, 1), ("f", 2, 1)]),
    "explosiveArrow": (None, dict(r=0, d=2, f=6, m=1.3, p=50), [("r", 1, 4), ("m", 0.15, 3), ("p", 5, 3), ("d", 1, 2), ("f", 2, 2)]),
    "cripplingShot": (None, dict(r=0, d=1, f=8, m=1.0, p=50, t=2), [("p", 5, 5), ("r", 1, 4), ("m", 0.1, 3), ("d", 1, 1), ("t", 1, 1)]),
    "arrowStorm": (None, dict(r=7, a=0, d=1, f=6, m=0.9), [("r", 1, 5), ("m", 0.1, 4), ("a", 1, 2), ("f", 2, 2), ("d", 1, 1)]),
    "deathShot": (None, dict(r=2, d=2, f=8, m=2.0, p=25), [("r", 1, 4), ("m", 0.15, 4), ("p", 2, 3), ("f", 2, 2), ("d", 1, 1)]),
    # --- the plants' venom (below the Veneno Cáustico in every number) ------------------------------------------------------
    "minorVenom": (8, dict(r=5, a=0), [("r", 1, 2), ("a", 1, 2)]),
}
ORDER = "racljdfbmpPtRsekKThHgGMuvx"


def lay(grow):
    """The improvements one per level: each kind spread over the window (largest share still owed first)."""
    total = sum(n for _, _, n in grow)
    assert total <= SLOTS, grow
    seq, given = [], [0] * len(grow)
    for i in range(total):
        j = max((j for j in range(len(grow)) if given[j] < grow[j][2]),
                key=lambda j: ((i + 1) * grow[j][2] / total - given[j], -j))
        seq.append(j)
        given[j] += 1
    slots = ["-"] * SLOTS
    pos = [min(SLOTS - 1, int((i + 0.5) * SLOTS / total)) for i in range(total)] if total < SLOTS else list(range(SLOTS))
    for p, j in zip(pos, seq):
        letter, step, _ = grow[j]
        slots[p] = f"{letter}{step:g}"
    return slots


tracks = {}
for kind, (learn, base, grow) in S.items():
    L = learn if learn is not None else ours_learn[kind]
    tracks[kind] = (L, lay(grow))

# --- C++ table -------------------------------------------------------------------------------------------------------------
lines = ["// TRACKS-BEGIN (written by tools/scripts/spells/tracks.py; the level each token lands on = learn + its place, 1 to 14)",
         "constexpr SpellTrack kSpellTracks[] = {"]
for kind, (L, slots) in tracks.items():
    lines.append(f'    {{"{kind}", {L}, "{" ".join(slots)}"}},')
lines += ["};", "// TRACKS-END"]
nl = "\r\n" if "\r\n" in src else "\n"
a, b = src.find("// TRACKS-BEGIN"), src.find("// TRACKS-END")
assert a >= 0 and b >= 0, "markers missing"
src = src[:a] + nl.join(lines) + src[b + len("// TRACKS-END"):]
io.open(SRC, "w", encoding="utf-8", newline="").write(src)

# --- text ------------------------------------------------------------------------------------------------------------------
TIERS = ["lesser", "poison", "greater", "deadly", "lethal"]
TIER_PT = {"lesser": "Veneno Menor", "poison": "Veneno", "greater": "Veneno Maior", "deadly": "Veneno Mortal", "lethal": "Veneno Letal"}
TIER_EN = {"lesser": "Lesser Poison", "poison": "Poison", "greater": "Greater Poison", "deadly": "Deadly Poison", "lethal": "Lethal Poison"}
LABEL = {  # letter: (pt, en)
    "r": ("alcance", "range"), "l": ("comprimento", "length"), "c": ("quantidade", "count"), "m": ("peso do MAG", "MAG weight"),
    "p": ("porcentagem", "percent"), "t": ("turnos", "turns"), "R": ("rodadas", "rounds"), "s": ("propagação", "spread"),
    "j": ("saltos", "jumps"), "e": ("perda por salto", "loss per jump"), "P": ("empurrão", "push"), "T": ("peso do efeito", "effect weight"),
    "b": ("bônus", "bonus"), "M": ("peso do anel", "ring weight"), "x": ("chance de sono %", "sleep chance %"),
}
OWN = {  # spell: {letter: (pt, en)}: what its numbers mean
    "magicMissile": {"c": ("mísseis", "missiles")}, "mirrorImage": {"c": ("cópias", "copies"), "p": ("chance de errar %", "miss chance %")},
    "multiShot": {"c": ("alvos", "targets")}, "wrathOfTheSkies": {"c": ("raios", "bolts")},
    "markOfRuin": {"p": ("dano extra %", "extra damage %")}, "lifeDrain": {"p": ("cura %", "healing %")},
    "shroudOfShadows": {"p": ("redução %", "reduction %")}, "curseOfDecay": {"p": ("teto %", "cap %"), "T": ("peso por turno", "weight per turn")},
    "soulHarvest": {"p": ("alma %", "soul %")}, "devouringVoid": {"p": ("dano extra %", "extra damage %"), "s": ("resistência retirada", "resistance shred")},
    "iceLance": {"p": ("dano após o 1º %", "damage past the 1st %")}, "explosiveArrow": {"p": ("dano nos vizinhos %", "neighbours' damage %")},
    "cripplingShot": {"p": ("MOV retirado %", "MOV off %")}, "deathShot": {"p": ("executa abaixo de % do HP", "executes under % HP")},
    "bless": {"p": ("acerto %", "hit %"), "t": ("rodadas", "rounds")}, "auraOfProtection": {"p": ("proteção %", "protection %"), "t": ("rodadas", "rounds")},
    "intimidatingPresence": {"p": ("intimidação %", "intimidation %"), "t": ("rodadas", "rounds")},
    "sacrificeBond": {"p": ("dano que a invocação recebe %", "damage the summon takes %")}, "empowerSummons": {"p": ("atributos %", "stats %")},
    "nightmare": {"p": ("chance de medo %", "fear chance %"), "t": ("turnos de medo", "fear turns")},
    "judgment": {"T": ("peso contra mortos-vivos", "weight on the undead")}, "creepingPlague": {"s": ("espalha", "spreads")},
    "longShot": {"b": ("bônus no arco", "bow bonus")}, "bloodyShot": {"m": ("peso do arco", "bow weight")},
    "thunderstorm": {"b": ("bônus do eco", "echo bonus")}, "chainLightning": {"l": ("alcance do salto", "jump reach")},
    "glacialGale": {"P": ("empurrão", "push")},
}
for k in ("venomArrow", "explosiveArrow", "cripplingShot", "arrowRain", "arrowStorm", "deathShot", "bloodyShot"):
    OWN.setdefault(k, {}).setdefault("m", ("peso do arco", "bow weight"))
SHOT = {"venomArrow", "explosiveArrow", "cripplingShot", "deathShot"}
DICE = [("d", "f", ("dano", "damage")), ("k", "K", ("dano por turno", "damage per turn")), ("g", "G", ("dano do anel", "ring damage")),
        ("h", "H", ("chão em chamas", "burning ground"))]


def num(v, pt):
    s = f"{v:.2f}".rstrip("0").rstrip(".") if isinstance(v, float) else str(v)
    return s.replace(".", ",") if pt else s


def sentence(kind, pt):
    L, slots = tracks[kind]
    base = S[kind][1]
    gain = {}
    for tok in slots:
        if tok != "-":
            gain[tok[0]] = gain.get(tok[0], 0) + float(tok[1:])
    parts = []
    used = set()
    for dl, fl, lab in DICE:
        if dl in base and (dl in gain or fl in gain):
            d0, f0 = base[dl], base.get(fl, 6)
            d1, f1 = d0 + round(gain.get(dl, 0)), f0 + round(gain.get(fl, 0))
            parts.append(f"{lab[0 if pt else 1]} {d0}D{f0} → {d1}D{f1}" if d0 else f"{lab[0 if pt else 1]} 0 → {d1}D{f1}")
            used |= {dl, fl}
    for letter in ORDER:
        if letter in used or letter not in gain:
            continue
        g = gain[letter]
        if letter == "a":
            rings = round(g)
            parts.append(("área 3 hexes → " if pt else "area 3 hexes → ") + (f"{rings} anéis" if pt else f"{rings} rings") if rings > 1 else
                         ("área 3 hexes → 1 anel" if pt else "area 3 hexes → 1 ring"))
            continue
        if letter == "v":
            t0 = base["v"]
            t1 = TIERS[TIERS.index(t0) + round(g)]
            parts.append(f"{(TIER_PT if pt else TIER_EN)[t0]} → {(TIER_PT if pt else TIER_EN)[t1]}")
            continue
        if letter == "u":
            parts.append("duração 1D6 → 2D6" if pt else "duration 1D6 → 2D6")
            continue
        lab = OWN.get(kind, {}).get(letter) or LABEL[letter]
        lab = lab[0 if pt else 1]
        if letter == "r" and kind in SHOT:
            lab = "hexes além do arco" if pt else "hexes past the bow"
        b0 = base.get(letter, 0)
        b1 = b0 + g
        if isinstance(b0, int) and float(g).is_integer():
            b1 = int(round(b1))
        else:
            b0, b1 = float(b0), round(b1, 3)
        parts.append(f"{lab} {num(b0, pt)} → {num(b1, pt)}")
    if pt:
        head = f"Escala do nível {L} ao {L + SLOTS}, uma melhoria pequena por nível"
    else:
        head = f"Scales from level {L} to {L + SLOTS}, one small improvement a level"
    return head + (": " + "; ".join(parts) + "." if parts else ".")


def per_level(kind, pt):
    L, slots = tracks[kind]
    out = []
    for i, tok in enumerate(slots):
        if tok == "-":
            continue
        letter, step = tok[0], float(tok[1:])
        lab = OWN.get(kind, {}).get(letter) or LABEL.get(letter)
        name = {"a": ("+1 anel", "+1 ring"), "d": ("+1 dado", "+1 die"), "f": ("dado maior", "bigger die"), "k": ("+1 dado por turno", "+1 tick die"),
                "K": ("dado por turno maior", "bigger tick die"), "g": ("+1 dado no anel", "+1 ring die"), "h": ("+1 dado de fogo", "+1 fire die"),
                "H": ("dado de fogo maior", "bigger fire die"), "u": ("+1 dado de duração", "+1 duration die"), "v": ("veneno mais forte", "stronger poison")}.get(letter)
        if name:
            txt = name[0 if pt else 1]
        else:
            txt = f"{'+' if step > 0 else ''}{num(step if not step.is_integer() else int(step), pt)} {lab[0 if pt else 1]}"
            if letter == "r":
                txt = ("+1 hex de alcance" if pt else "+1 hex of range") if kind not in SHOT else ("+1 hex além do arco" if pt else "+1 hex past the bow")
        out.append(f"{L + 1 + i}: {txt}")
    return ", ".join(out)


# Como jogar
HELP = "content/engine2/help.json"
RAW = io.open(HELP, encoding="utf-8", newline="").read()
d = json.loads(RAW)
LAYOUT = next(((ind, end) for ind in (None, 1, 2, 4) for end in ("", "\n") if json.dumps(d, ensure_ascii=False, indent=ind) + end == RAW))
ROWS = {
    ("Míssil Mágico", "Magic Missile"): "magicMissile", ("Relâmpago", "Lightning"): "lightning", ("Relâmpago3", "Relâmpago3"): "lightningTier3",
    ("Bola De Fogo", "Fireball"): "fireball", ("Veneno Cáustico", "Caustic Venom"): "causticVenom", ("Divine Bolt", "Divine Bolt"): "divineBolt",
    ("Teia dos Sonhos", "Web of Dreams"): "webOfDreams", ("Cura Menor", "Minor Heal"): "cureMinor", ("Cura Média", "Medium Heal"): "cureWounds",
    ("Curar Doença Leve", "Cure Minor Disease"): "cureDisease", ("Healing Hands", "Healing Hands"): "cureLight",
    ("Tiro Longo", "Long Shot"): "longShot", ("Bloody Shot", "Bloody Shot"): "bloodyShot", ("Tiro Múltiplo", "Multi Shot"): "multiShot",
    ("Bless", "Bless"): "bless", ("Turn Undead", "Turn Undead"): "turnUndead", ("Aura de Proteção", "Aura of Protection"): "auraOfProtection",
    ("Presença Intimidante", "Intimidating Presence"): "intimidatingPresence", ("Frost", "Frost"): "frost",
    ("Mãos Flamejantes", "Burning Hands"): "burningHands", ("Sopro Venenoso", "Poison Breath"): "poisonBreath",
    ("Força Fantasmagórica", "Phantasmal Force"): "phantasmalForce", ("Ira Divina", "Divine Wrath"): "divineWrath",
}


def txt(cell):
    return "".join(x.get("text", "") for line in cell for x in line)


def setc(cell, text):
    muted = any(x.get("muted") for line in cell for x in line)
    return [[{"text": text, "muted": True} if muted else {"text": text}]]


# the scaling sentences written before (bc1f175 / 1bc8cf0) and this script's own, all replaced by the new one
OLD = [r"Escala do nível \d+ ao \d+[^.]*\.", r"Scales from level \d+ to \d+[^.]*\.",
       r"Alcance \d+, (?:\+1 por nível|\+1 a cada \d+ níveis) desde o nível \d+, até \d+(?: no nível \d+)?\.",
       r"Range \d+, (?:\+1 per level|\+1 every \d+ levels) from level \d+, up to \d+(?: at level \d+)?\.",
       r"Área: 3 hexes[^.]*\.", r"Area: 3 hexes[^.]*\.", r"Dano: \+1D\d [^.]*\.", r"Damage: \+1D\d [^.]*\."]
changed = 0
seen = set()
for lang in ("pt", "en"):
    pt = lang == "pt"
    for tab in d[lang]["tabs"]:
        for block in tab.get("blocks", []):
            for row in block.get("rows", []) or []:
                if not isinstance(row, list) or len(row) < 5:
                    continue
                name = txt(row[0]).strip()
                kind = next((k for (a_, b_), k in ROWS.items() if name == (a_ if pt else b_)), None)
                if not kind:
                    continue
                seen.add(kind)
                notes = txt(row[4]).strip()
                new = notes
                for pat in OLD:
                    new = re.sub(pat, "", new).strip()
                new = new.replace("1 míssil, +1 a cada 3 níveis, até 5 no nível 13", "").replace("1 missile, +1 every 3 levels, up to 5 at level 13", "")
                new = re.sub(r"\s{2,}", " ", new).strip(" ;,")
                final = (sentence(kind, pt) + (" " + new if new and new != "—" else "")).strip()
                if final != notes:
                    row[4] = setc(row[4], final)
                    changed += 1
io.open(HELP, "w", encoding="utf-8", newline="").write(json.dumps(d, ensure_ascii=False, indent=LAYOUT[0]) + LAYOUT[1])
print("Como jogar rows changed:", changed, "| spells without a row there:", ", ".join(k for k in S if k not in seen))

# docs/SPELLS_SCALING.md
NAMES = {
    "magicMissile": "Míssil Mágico", "lightning": "Relâmpago", "frost": "Frost", "poisonBreath": "Sopro Venenoso", "fireball": "Bola de Fogo",
    "causticVenom": "Veneno Cáustico", "lightningTier3": "Relâmpago Maior", "cureMinor": "Cura Menor", "bless": "Bênção", "cureWounds": "Cura Média",
    "burningHands": "Mãos Flamejantes", "turnUndead": "Expulsar Mortos-Vivos", "cureDisease": "Curar Doença", "divineBolt": "Raio Divino",
    "circleOfHealing": "Círculo de Cura", "resurrection": "Ressurreição", "judgment": "Julgamento", "cureLight": "Mãos Curativas",
    "auraOfProtection": "Aura de Proteção", "intimidatingPresence": "Presença Intimidante", "divineWrath": "Ira Divina",
    "summonFamiliar": "Invocar Familiar", "phantasmalForce": "Força Fantasmagórica", "webOfDreams": "Teia dos Sonhos",
    "summonFamiliar2": "Familiar Maior", "placeSwap": "Troca de Lugar", "boneLance": "Lança de Osso", "summonFamiliar4": "Familiar Radiante",
    "mirrorImage": "Imagem Espelhada", "detonateSummon": "Detonar Invocação", "summonFamiliar3": "Familiar Titã (T5)",
    "sacrificeBond": "Elo de Sacrifício", "nightmare": "Pesadelo", "summonZombieDog": "Cão Zumbi (Necromante, T6)",
    "empowerSummons": "Fortalecer Invocações", "markOfRuin": "Marca da Ruína", "creepingPlague": "Praga Rastejante",
    "lifeDrain": "Dreno de Vida", "shroudOfShadows": "Mortalha de Sombras", "hemorrhage": "Hemorragia", "profaneSilence": "Silêncio Profano",
    "blackPestilence": "Peste Negra", "curseOfDecay": "Maldição da Decadência", "soulHarvest": "Colheita de Almas",
    "devouringVoid": "Vazio Devorador", "pillarOfFlame": "Coluna de Fogo", "iceLance": "Lança de Gelo", "thunderstorm": "Tempestade Elétrica",
    "meteor": "Meteoro", "absoluteZero": "Zero Absoluto", "chainLightning": "Cadeia de Raios", "eruption": "Erupção",
    "glacialGale": "Vendaval Glacial", "wrathOfTheSkies": "Fúria dos Céus", "primordialEmber": "Brasa Primordial", "longShot": "Tiro Longo",
    "venomArrow": "Flecha Envenenada", "bloodyShot": "Tiro Sangrento", "multiShot": "Tiro Múltiplo", "arrowRain": "Chuva de Flechas",
    "explosiveArrow": "Flecha Explosiva", "cripplingShot": "Tiro Paralisante", "arrowStorm": "Tempestade de Flechas", "deathShot": "Tiro Mortal",
    "minorVenom": "Veneno Menor (plantas)",
}
rows = []
for kind in S:
    rows.append(f"### {NAMES[kind]}\n\n{sentence(kind, True)}\n\nNível a nível: {per_level(kind, True)}.\n")
doc = f"""# Escala das magias

Gerado por `tools/scripts/spells/tracks.py` (a tabela `kSpellTracks` em `engine/world/Spells.cpp` é a fonte; o texto do Como jogar sai do mesmo script).

Regras do usuário (2026-10-20): baseado no AD&D 2ª edição, sem copiar os números (o Ember tem atributos, HP e teto de nível
próprios); cada magia tem função e números únicos. A magia é aprendida no primeiro nível do tier dela para a classe e escala por
15 níveis: o nível em que é aprendida e os 14 seguintes, **uma melhoria pequena por nível, nunca duas no mesmo nível**. O alcance
nunca passa de 12 hexes (a visão do grupo). A área existe só nas magias de área: começa em 3 hexes (um corpo de tamanho 3) e cresce
um anel por vez, duas ou três vezes; o Veneno Cáustico tem a maior área do jogo (4 anéis), a Bola de Fogo a seguinte (3 anéis).
O Ember Ashes I para no nível 30; uma magia aprendida depois do 16 continua a escala no Ember Ashes II (teto 60), que já está
programado: os níveis além do 30 só não se alcançam ainda.

Familiar Titã é Tier 5 (nível 11); o Cão Zumbi é Tier 6 do Necromante (nível 15, a promoção) (o usuário, 2026-10-20).

""" + "\n".join(rows)
io.open("docs/SPELLS_SCALING.md", "w", encoding="utf-8", newline="\n").write(doc)
for kind, (L, slots) in tracks.items():
    print(f"{kind:22s} {L:2d}  {' '.join(slots)}")
