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
    # Reflexo Arcano in the Lança's place (the user, 2026-10-20: "começa com um reflexo mais fraco e vai escalando", "um talento que começa
    # em menor percentagem a absorver o dano como cura", "escalar a duração ... começando com apenas 2 turnos indo até 6")
    # (then: "não reflete e não absorve 100%: chega até 90% de reflexo e 50% de absorção"; before the talent he takes what is not reflected)
    "arcaneReflection": (None, dict(t=2, p=40, x=10), [("p", 10, 5), ("t", 1, 4), ("Z", 1, 1), ("x", 10, 4)]),
    "summonFamiliar4": (6, dict(r=2), [("r", 1, 6)]),
    "mirrorImage": (None, dict(r=6, c=2, R=3, p=33), [("r", 1, 4), ("c", 1, 2), ("R", 1, 3), ("p", 3, 5)]),
    "detonateSummon": (None, dict(r=6, a=0, d=2, f=8, m=1.8), [("r", 1, 4), ("a", 1, 2), ("d", 1, 3), ("m", 0.12, 5)]),
    "summonFamiliar3": (11, dict(r=2), [("r", 1, 6)]),  # Familiar Titã: T5 (the user, 2026-10-20)
    "sacrificeBond": (None, dict(p=100), [("p", -3, 14)]),
    "nightmare": (None, dict(r=6, a=0, d=2, f=8, m=1.5, p=25, t=1), [("r", 1, 6), ("a", 1, 2), ("d", 1, 2), ("m", 0.15, 2), ("p", 10, 1), ("t", 1, 1)]),
    "summonZombieDog": (15, dict(r=2), [("r", 1, 6)]),  # Cão Zumbi: the Necromante's T6 (the user, 2026-10-20)
    "empowerSummons": (None, dict(p=25, R=3), [("p", 1, 12), ("R", 1, 2)]),
    # the Necromante's T6 (the user, 2026-10-20: "deixar a magia Lança de Ossos como T6 para necromancer, e adaptar para algo mais forte e mais único")
    "boneLance": (None, dict(l=4, d=2, f=8, m=1.8, t=1, p=50), [("m", 0.1, 4), ("l", 1, 2), ("d", 1, 2), ("f", 2, 2), ("t", 1, 1), ("Z", 1, 1), ("p", 10, 2)]),
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
    # --- Guerreiro (weapon skills: the dice are a bonus on the weapon's roll, m = the weight on it; Z = its talent) ----------
    # the user (2026-10-20): "eles não tem muito o que escalar além de dano, mas pode dar um talento nelas sim"
    "doubleStrike": (1, dict(d=0, f=4, m=1.0), [("d", 1, 2), ("f", 2, 2), ("m", 0.05, 9), ("Z", 1, 1)]),
    "provoke": (3, dict(r=4, a=0, p=0), [("r", 1, 6), ("a", 1, 1), ("p", 10, 6), ("Z", 1, 1)]),
    "cleave": (4, dict(d=1, f=8, m=1.0), [("d", 1, 1), ("f", 2, 2), ("m", 0.05, 10), ("Z", 1, 1)]),
    "shieldBash": (4, dict(d=1, f=4, m=1.0, t=1), [("f", 2, 4), ("d", 1, 1), ("t", 1, 1), ("m", 0.05, 7), ("Z", 1, 1)]),
    "bullRush": (8, dict(r=4, d=2, f=8, k=1, K=8, m=1.0), [("r", 1, 2), ("d", 1, 1), ("f", 2, 2), ("k", 1, 1), ("K", 2, 2), ("m", 0.05, 5), ("Z", 1, 1)]),
    "executionerStrike": (8, dict(d=1, f=8, p=30, M=1.5, m=1.0), [("M", 0.15, 5), ("d", 1, 1), ("f", 2, 2), ("p", 5, 2), ("m", 0.05, 3), ("Z", 1, 1)]),
    "steelWhirlwind": (None, dict(d=1, f=6, m=1.0), [("d", 1, 2), ("f", 2, 2), ("m", 0.05, 9), ("Z", 1, 1)]),
    "guardBreak": (None, dict(d=1, f=8, p=20, t=2, m=1.0), [("m", 0.05, 8), ("p", 5, 3), ("d", 1, 1), ("t", 1, 1), ("Z", 1, 1)]),
    "warCry": (None, dict(a=2, p=15, t=2), [("p", 1, 10), ("t", 1, 2), ("a", 1, 1), ("Z", 1, 1)]),
    "leapingStrike": (None, dict(r=3, d=2, f=6, m=1.5, b=0), [("m", 0.1, 6), ("r", 1, 2), ("f", 2, 2), ("b", 1, 2), ("d", 1, 1), ("Z", 1, 1)]),
    # --- Cavaleiro Pesado ------------------------------------------------------------------------------------------------
    "shoulderSmash": (11, dict(d=1, f=8, c=1, m=1.0), [("m", 0.05, 8), ("c", 1, 2), ("f", 2, 2), ("d", 1, 1), ("Z", 1, 1)]),
    "stampede": (18, dict(r=4, d=1, f=10, m=1.0), [("m", 0.05, 6), ("r", 1, 4), ("d", 1, 2), ("f", 2, 1), ("Z", 1, 1)]),
    # --- Lanceiro ----------------------------------------------------------------------------------------------------------
    "piercingThrust": (1, dict(l=0, x=20, m=1.0), [("m", 0.05, 7), ("x", 5, 4), ("l", 1, 2), ("Z", 1, 1)]),
    "spearToss": (None, dict(r=2, d=0, f=4, m=0.9, k=1, K=6, b=0), [("m", 0.05, 4), ("d", 1, 2), ("f", 2, 2), ("b", 1, 2), ("r", 1, 1), ("k", 1, 1), ("K", 2, 1), ("Z", 1, 1)]),
    "sweep": (4, dict(d=0, f=4, m=1.0), [("m", 0.05, 9), ("f", 2, 3), ("d", 1, 1), ("Z", 1, 1)]),
    "hookPull": (None, dict(r=3, d=1, f=4, m=0.8, t=0), [("m", 0.05, 7), ("r", 1, 2), ("f", 2, 2), ("d", 1, 1), ("t", 1, 1), ("Z", 1, 1)]),
    "trip": (8, dict(d=1, f=8, m=1.0), [("m", 0.05, 10), ("d", 1, 2), ("f", 2, 1), ("Z", 1, 1)]),
    "poleVault": (None, dict(d=1, f=6, m=1.2, b=0), [("m", 0.1, 6), ("b", 1, 4), ("f", 2, 2), ("d", 1, 1), ("Z", 1, 1)]),
    "disarm": (None, dict(d=1, f=4, p=40, t=2, m=1.0), [("m", 0.05, 5), ("p", 5, 4), ("f", 2, 2), ("d", 1, 1), ("t", 1, 1), ("Z", 1, 1)]),
    "swapPost": (None, dict(r=3, d=1, f=6, m=1.0, b=0), [("m", 0.05, 6), ("r", 1, 2), ("f", 2, 2), ("b", 1, 2), ("d", 1, 1), ("Z", 1, 1)]),
    "impaleDrag": (None, dict(l=3, d=1, f=8, m=1.0, e=0.25), [("e", 0.01, 5), ("m", 0.05, 4), ("f", 2, 2), ("l", 1, 1), ("d", 1, 1), ("Z", 1, 1)]),
    "sentinelVigil": (None, dict(a=1, d=0, f=6, m=1.0, b=0), [("m", 0.1, 6), ("b", 1, 3), ("f", 2, 2), ("d", 1, 1), ("a", 1, 1), ("Z", 1, 1)]),
}
# Each skill's talent (Z on its track): (name pt, what it does pt, name en, what it does en)
TALENTS = {
    "doubleStrike": ("Terceiro Golpe", "um terceiro golpe com metade do dano, sem contra-ataque", "Third Blow", "a third blow at half damage, no counter"),
    "provoke": ("Fúria Contida", "o guerreiro ganha +10% de ATK por 2 turnos", "Held Fury", "the warrior gains +10% ATK for 2 turns"),
    "cleave": ("Corte Baixo", "os atingidos ficam Paralisados (-25% de MOV) por 1 turno", "Low Cut", "the struck are Crippled (-25% MOV) for 1 turn"),
    "shieldBash": ("Empurrão de Escudo", "o alvo é empurrado 1 hex para longe", "Shield Shove", "the target is pushed 1 hex away"),
    "bullRush": ("Contra a Parede", "se o empurrão bate em algo, o alvo fica atordoado 1 turno", "Into the Wall", "if the shove hits something, the target is stunned 1 turn"),
    "executionerStrike": ("Sentença", "se o golpe mata, o uso da habilidade volta", "Sentence", "if the blow kills, the skill's use comes back"),
    "steelWhirlwind": ("Lâminas Abertas", "os atingidos sangram (1D6 por turno, por 1D3 turnos)", "Open Wounds", "the struck bleed (1D6 a turn, for 1D3 turns)"),
    "guardBreak": ("Sem Resposta", "o alvo não contra-ataca este golpe", "No Answer", "the target cannot counter this blow"),
    "warCry": ("Terror", "os inimigos no raio ficam com medo por 1 turno", "Terror", "the foes in the radius are afraid for 1 turn"),
    "leapingStrike": ("Onda de Choque", "os outros inimigos ao lado do pouso levam metade de um golpe", "Shockwave", "the other foes beside the landing take half a blow"),
    "shoulderSmash": ("Derrubar", "os atingidos ficam Desequilibrados (sem contra-ataque) até o próximo turno deles", "Knock Down", "the struck are Off balance (no counter) until their next turn"),
    "stampede": ("Pisoteio", "os atingidos são empurrados 1 hex", "Trample", "the struck are pushed 1 hex"),
    "piercingThrust": ("Atravessar", "os alvos depois do primeiro levam 75% do dano, não 50%", "Run Through", "the targets after the first take 75% of the damage, not 50%"),
    "spearToss": ("Colisão Atordoante", "quando o corpo bate em outro, os dois ficam atordoados 1 turno", "Stunning Crash", "when the body hits another, both are stunned 1 turn"),
    "sweep": ("Varrer Longe", "empurra 2 hexes em vez de 1", "Sweep Away", "pushes 2 hexes instead of 1"),
    "hookPull": ("Gancho Farpado", "o puxado fica Paralisado (-30% de MOV) por 1 turno", "Barbed Hook", "the pulled foe is Crippled (-30% MOV) for 1 turn"),
    "trip": ("Tendão Cortado", "a Rasteira tira 20% de todos os atributos, não 10%", "Cut Tendon", "the Trip takes 20% off every stat, not 10%"),
    "poleVault": ("Queda do Alto", "50% de chance de atordoar o alvo por 1 turno", "From Above", "a 50% chance to stun the target for 1 turn"),
    "disarm": ("Mão Ferida", "o desarmado perde também 1 uso de habilidade", "Wounded Hand", "the disarmed foe also loses 1 use of a skill"),
    "swapPost": ("Coragem do Posto", "o aliado trocado ganha +10% de ATK por 1 turno", "Post's Courage", "the swapped ally gains +10% ATK for 1 turn"),
    "impaleDrag": ("Ferida Aberta", "o arrastado sangra (1D8 por turno, por 1D3 turnos)", "Open Wound", "the dragged foe bleeds (1D8 a turn, for 1D3 turns)"),
    "arcaneReflection": ("Absorção", "do dano que não volta, uma parte vira cura em vez de ferir, de 10% a 50%", "Absorption",
                         "of the damage that does not go back, a share heals instead of hurting, from 10% to 50%"),
    "boneLance": ("Estilhaços", "os ossos explodem na ponta da reta: os outros vizinhos dela levam de 50% a 70% do dano", "Shards",
                  "the bones burst at the line's far end: its other neighbours take 50% to 70% of the damage"),
    "sentinelVigil": ("Contra-Golpe Firme", "quem a Vigília interrompe fica Desequilibrado (sem contra-ataque)", "Firm Riposte", "whoever the Vigil interrupts is Off balance (no counter)"),
}
ORDER = "racljdfbmpPtRsekKThHgGMuvxZ"


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


# letters that only mean something once the talent is open: their steps come after it on the track
AFTER_TALENT = {"arcaneReflection": "x", "boneLance": "p"}
tracks = {}
for kind, (learn, base, grow) in S.items():
    L = learn if learn is not None else ours_learn[kind]
    slots = lay(grow)
    if kind in AFTER_TALENT:
        letter = AFTER_TALENT[kind]
        while True:
            z = next(i for i, t in enumerate(slots) if t.startswith("Z"))
            early = next((i for i, t in enumerate(slots[:z]) if t.startswith(letter)), None)
            if early is None:
                break
            slots[early], slots[z] = slots[z], slots[early]
    tracks[kind] = (L, slots)

# --- C++ table -------------------------------------------------------------------------------------------------------------
lines = ["// TRACKS-BEGIN (written by tools/scripts/spells/tracks.py; the level each token lands on = learn + its place, 1 to 14)",
         "constexpr SpellTrack kSpellTracks[] = {"]
for kind, (L, slots) in tracks.items():
    lines.append(f'    {{"{kind}", {L}, "{" ".join(slots)}"}},')
lines += ["};", "// The talents (Z): name and what it does, PT / EN", "struct SpellTalent {", "  const char *kind, *pt, *ptAbout, *en, *enAbout;", "};",
          "constexpr SpellTalent kSpellTalents[] = {"]
for kind, (pt, pta, en, ena) in TALENTS.items():
    lines.append(f'    {{"{kind}", "{pt}", "{pta}", "{en}", "{ena}"}},')
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
    "executionerStrike": {"p": ("executa abaixo de % do HP", "executes under % HP"), "M": ("multiplicador da execução", "execution multiplier"), "m": ("peso da arma", "weapon weight")},
    "piercingThrust": {"l": ("hexes além da arma", "hexes past the weapon"), "x": ("DEF ignorada %", "DEF ignored %"), "m": ("peso da arma", "weapon weight")},
    "shieldBash": {"t": ("turnos atordoado", "stun turns")}, "shoulderSmash": {"c": ("hexes de largura", "hexes wide")},
    "provoke": {"p": ("inimizade extra %", "extra enmity %"), "a": ("raio", "radius")}, "warCry": {"p": ("ATK dos aliados %", "allies' ATK %"), "a": ("raio", "radius")},
    "guardBreak": {"p": ("DEF retirada %", "DEF off %")}, "disarm": {"p": ("ATK retirado %", "ATK off %")}, "hookPull": {"t": ("turnos desequilibrado", "off-balance turns")},
    "impaleDrag": {"e": ("peso por hex arrastado", "weight per hex dragged"), "l": ("hexes arrastados", "hexes dragged")},
    "sentinelVigil": {"a": ("raio", "radius"), "m": ("peso do contra-golpe", "riposte weight")}, "spearToss": {"r": ("hexes do arremesso", "hexes thrown")},
    "leapingStrike": {"r": ("alcance do salto", "leap reach")},
    "arcaneReflection": {"t": ("turnos", "turns"), "p": ("devolvido %", "reflected %"), "x": ("cura %", "healing %")},
    "boneLance": {"l": ("comprimento", "length"), "t": ("turnos empalado", "impaled turns"), "p": ("estilhaços %", "shards %")},
}
for k in ("doubleStrike", "cleave", "shieldBash", "bullRush", "steelWhirlwind", "guardBreak", "leapingStrike", "shoulderSmash", "stampede", "spearToss", "sweep",
          "hookPull", "trip", "poleVault", "disarm", "swapPost", "impaleDrag"):
    OWN.setdefault(k, {}).setdefault("m", ("peso da arma", "weapon weight"))
DICE_LABEL = {"bullRush": {"k": ("dano na parede", "wall damage")}, "spearToss": {"k": ("dano da colisão", "collision damage")}}
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
            lab = DICE_LABEL.get(kind, {}).get(dl, lab)
            parts.append(f"{lab[0 if pt else 1]} {d0}D{f0} → {d1}D{f1}" if d0 else f"{lab[0 if pt else 1]} 0 → {d1}D{f1}")
            used |= {dl, fl}
    for letter in ORDER:
        if letter in used or letter not in gain:
            continue
        g = gain[letter]
        if letter == "a" and "a" not in OWN.get(kind, {}):
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
        if letter == "Z":
            t = TALENTS[kind]
            parts.append(f"talento {t[0]} ({t[1]})" if pt else f"talent {t[2]} ({t[3]})")
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
        if letter == "Z":
            txt = ("talento " + TALENTS[kind][0]) if pt else ("talent " + TALENTS[kind][2])
        elif letter == "a" and "a" in OWN.get(kind, {}):
            txt = "+1 de raio" if pt else "+1 radius"
        elif name:
            txt = name[0 if pt else 1]
        else:
            txt = f"{'+' if step > 0 else ''}{num(step if not step.is_integer() else int(step), pt)} {lab[0 if pt else 1]}"
            if letter == "r":
                txt = ("+1 hex de alcance" if pt else "+1 hex of range") if kind not in SHOT else ("+1 hex além do arco" if pt else "+1 hex past the bow")
        out.append(f"{L + 1 + i}: {txt}")
    return ", ".join(out)


# Como jogar: the skills table (Tier 1-6 tab), one row a spell or skill in the same order in both languages; every row of a kind
# with a track is written whole from it: the formula at the learning level, 7 levels on and at the track's end, and what it
# does (spellExplain in Spells.cpp, read from there) followed by its scaling sentence.
HELP = "content/engine2/help.json"
RAW = io.open(HELP, encoding="utf-8", newline="").read()
d = json.loads(RAW)
LAYOUT = next(((ind, end) for ind in (None, 1, 2, 4) for end in ("", "\n") if json.dumps(d, ensure_ascii=False, indent=ind) + end == RAW))
ROW_KIND = {0: "bless", 1: "magicMissile", 2: "longShot", 3: "cureMinor", 4: "doubleStrike", 5: "provoke", 6: "piercingThrust",
            7: "summonFamiliar", 8: "poisonBreath", 9: "bloodyShot", 10: "summonFamiliar2", 11: "lightning", 13: "cureWounds", 14: "cleave",
            15: "sweep", 16: "webOfDreams", 17: "frost", 18: "summonFamiliar4", 20: "trip", 21: "fireball", 22: "turnUndead",
            23: "cureDisease", 24: "multiShot", 26: "summonFamiliar3", 28: "divineBolt", 29: "cureLight", 30: "shoulderSmash",
            31: "lightningTier3", 32: "causticVenom", 33: "auraOfProtection", 34: "intimidatingPresence", 35: "divineWrath", 36: "stampede"}
ROW_TIER = {26: "T5"}  # Familiar Titã (the user, 2026-10-20)
fn = src[src.index("std::string spellExplain(const std::string& kind, bool en) {"):]
fn = fn[:fn.index("  for (const Line& l : kLines)")]
EXPLAIN = {k: (pt, en)
           for k, pt, en in re.findall(r'\{"(\w+)", "((?:[^"\\]|\\.)*)",\s*"((?:[^"\\]|\\.)*)"\}', fn)}


def gains_at(kind, lv):
    L, slots = tracks[kind]
    g = {}
    for i, tok in enumerate(slots):
        if tok != "-" and L + 1 + i <= lv:
            g[tok[0]] = g.get(tok[0], 0) + float(tok[1:])
    return g


def state(kind, lv, pt):
    """The spell's numbers at a level, short: "dano 2D6 · alcance 7 · ×1,15"."""
    base, g = S[kind][1], gains_at(kind, lv)
    parts, used = [], set()
    for dl, fl, lab in DICE:
        if dl in base:
            n_, f_ = base[dl] + round(g.get(dl, 0)), base.get(fl, 6) + round(g.get(fl, 0))
            lab = DICE_LABEL.get(kind, {}).get(dl, lab)
            if n_ > 0:
                parts.append(f"{lab[0 if pt else 1]} {n_}D{f_}")
            used |= {dl, fl}
    for letter in ORDER:
        if letter in used or (letter not in base and letter != "Z"):
            continue
        if letter == "Z":
            if g.get("Z"):
                parts.append(("talento " + TALENTS[kind][0]) if pt else ("talent " + TALENTS[kind][2]))
            continue
        v = base[letter] + g.get(letter, 0) if letter != "v" else None
        if letter == "a" and "a" not in OWN.get(kind, {}):
            st = round(v)
            parts.append("3 hexes" if st <= 0 else (f"raio {st}" if pt else f"radius {st}"))
            continue
        if letter == "v":
            t = TIERS[TIERS.index(base["v"]) + round(g.get("v", 0))]
            parts.append((TIER_PT if pt else TIER_EN)[t])
            continue
        if letter == "u":
            parts.append(f"{'duração' if pt else 'duration'} {1 + round(g.get('u', 0))}D6")
            continue
        if letter == "m":
            parts.append(f"×{num(round(v, 3), pt)}")
            continue
        lab = OWN.get(kind, {}).get(letter) or LABEL[letter]
        lab = lab[0 if pt else 1]
        if letter == "r" and kind in SHOT:
            lab = "hexes além do arco" if pt else "hexes past the bow"
        val = int(round(v)) if isinstance(base[letter], int) and float(v).is_integer() else round(v, 3)
        parts.append(f"{lab} {num(val, pt)}")
    return " · ".join(parts)


def txt(cell):
    if isinstance(cell, str):
        return cell
    return "".join((x.get("text", "") if isinstance(x, dict) else str(x)) for line in cell for x in (line if isinstance(line, list) else [line]))


def setc(cell, text):
    if isinstance(cell, str):
        return text
    muted = any(isinstance(x, dict) and x.get("muted") for line in cell for x in (line if isinstance(line, list) else [line]))
    return [[{"text": text, "muted": True} if muted else {"text": text}]]


changed = 0
seen = set()
for lang in ("pt", "en"):
    pt = lang == "pt"
    rows = d[lang]["tabs"][2]["blocks"][3]["rows"]
    for i, row in enumerate(rows):
        kind = ROW_KIND.get(i)
        if not kind or kind not in tracks:
            continue
        seen.add(kind)
        L = tracks[kind][0]
        lv = "Nv" if pt else "Lv"
        formula = " · ".join(f"{lv}{x}: {state(kind, x, pt)}" for x in (L, L + 7, L + SLOTS))
        formula = formula.replace(" · Nv", " | Nv").replace(" · Lv", " | Lv")
        about = EXPLAIN.get(kind, ("", ""))[0 if pt else 1]
        effect = (about + " " + sentence(kind, pt)).strip()
        new = [txt(c) for c in row]
        if i in ROW_TIER:
            row[2] = setc(row[2], ROW_TIER[i])
        if txt(row[3]) != formula or txt(row[4]) != effect:
            row[3] = setc(row[3], formula)
            row[4] = setc(row[4], effect)
            changed += 1
io.open(HELP, "w", encoding="utf-8", newline="").write(json.dumps(d, ensure_ascii=False, indent=LAYOUT[0]) + LAYOUT[1])
print("Como jogar rows changed:", changed, "| with a track and no row there:", ", ".join(k for k in S if k not in seen))

# docs/SPELLS_SCALING.md
NAMES = {
    "magicMissile": "Míssil Mágico", "lightning": "Relâmpago", "frost": "Frost", "poisonBreath": "Sopro Venenoso", "fireball": "Bola de Fogo",
    "causticVenom": "Veneno Cáustico", "lightningTier3": "Relâmpago Maior", "cureMinor": "Cura Menor", "bless": "Bênção", "cureWounds": "Cura Média",
    "burningHands": "Mãos Flamejantes", "turnUndead": "Expulsar Mortos-Vivos", "cureDisease": "Curar Doença", "divineBolt": "Raio Divino",
    "circleOfHealing": "Círculo de Cura", "resurrection": "Ressurreição", "judgment": "Julgamento", "cureLight": "Mãos Curativas",
    "auraOfProtection": "Aura de Proteção", "intimidatingPresence": "Presença Intimidante", "divineWrath": "Ira Divina",
    "summonFamiliar": "Invocar Familiar", "phantasmalForce": "Força Fantasmagórica", "webOfDreams": "Teia dos Sonhos",
    "summonFamiliar2": "Familiar Maior", "placeSwap": "Troca de Lugar", "boneLance": "Lança de Ossos (Necromante, T6)", "arcaneReflection": "Reflexo Arcano", "summonFamiliar4": "Familiar Radiante",
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
    "doubleStrike": "Corte Duplo", "provoke": "Provocar", "cleave": "Cleave", "shieldBash": "Golpe de Escudo", "bullRush": "Investida Touro",
    "executionerStrike": "Golpe do Carrasco", "steelWhirlwind": "Redemoinho de Aço", "guardBreak": "Quebra-Guarda", "warCry": "Grito de Guerra",
    "leapingStrike": "Salto Devastador", "shoulderSmash": "Investida de Ombro (Cavaleiro Pesado)", "stampede": "Debandada (Cavaleiro Pesado)",
    "piercingThrust": "Investida Perfurante", "spearToss": "Arremesso", "sweep": "Varredura", "hookPull": "Gancho", "trip": "Rasteira",
    "poleVault": "Salto de Vara", "disarm": "Desarmar", "swapPost": "Troca de Posto", "impaleDrag": "Empalar e Arrastar",
    "sentinelVigil": "Vigília da Sentinela",
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

As habilidades do Guerreiro, do Cavaleiro Pesado e do Lanceiro seguem a mesma regra; quase tudo nelas é dano (dados, faces, peso da
arma), e cada uma ganha um **talento** num nível da trilha (o usuário, 2026-10-20: "eles não tem muito o que escalar além de dano, mas
pode dar um talento nelas sim").

""" + "\n".join(rows)
io.open("docs/SPELLS_SCALING.md", "w", encoding="utf-8", newline="\n").write(doc)
for kind, (L, slots) in tracks.items():
    print(f"{kind:22s} {L:2d}  {' '.join(slots)}")
