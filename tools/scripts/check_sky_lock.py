"""The sky lock (the user, 2026-10-23: "só pode existir um tipo de céu no jogo inteiro", "100% igual em todos os mapas com céu
aberto", "depois de acertado NUNCA NINGUÉM VAI MEXER NOVAMENTE").

REGRA FIXA INQUEBRÁVEL DO JOGO (the user, 2026-10-23, later the same day): "só existe uma atmosfera no jogo, não importa qual o
mapa o céu é o mesmo, sempre sempre sempre só existe um único céu nesse jogo nunca vai existir excessão". No map ever gets its own
sky, sky scale, sky flag or sky path. (This check is not yet run by build.bat: the user said "não vai bloquear nada antes de
arrumar"; wire it only once they say the sky is right.)

Run by build.bat before every build (and by hand: python -I tools/scripts/check_sky_lock.py). The build FAILS when:
  1. a file of the sky (the shaders that draw the sky, the sun, the moon, the stars, the clouds; the star catalogue; the
     atmosphere's constants) differs from the copy recorded in content/sky/sky.locked.json;
  2. apps/sandbox/MapStage.h loses the one-sky lines (the moon's art on every map with a sky, the sunset's blue on every map) or
     gets a sky rule for one map back.
To change the sky on the user's explicit order only: change it, then run python -I tools/scripts/check_sky_lock.py --relock in the
same commit, saying so in the commit message. Never relock on your own, never weaken this check, never bypass build.bat."""
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LOCK = ROOT / "content/sky/sky.locked.json"
MAPSTAGE = ROOT / "apps/sandbox/MapStage.h"

SKY_FILES = [
    "shaders/atmosphere.hlsl",
    "shaders/atmosphere.hlsli",
    "shaders/clouds.hlsli",
    "shaders/cloudsvol.hlsl",
    "shaders/cloudsvol.hlsli",
    "shaders/skyreflect.hlsl",
    "shaders/sunglare.hlsl",
    "engine/render/Atmosphere.h",
    "engine/render/StarSky.h",
    "content/sky/stars.json",
]
# The sky's code in MapStage = the text of 2026-10-19 (236e0fa3; the user, 2026-10-23: "RESTAURAR DEFAULT DAQUELE DIA SEM MUDAR UMA
# VÍRGULA"): mapLighting / realLighting / cycleHour / cycleLighting verbatim (the 24 h cycle only with the panel's Hora), aldeia's own
# sunset blue as that day had it. The moon's art stays requested on every map but Subterrâneo.
MAPSTAGE_MARKERS = [
    "if (!underground())  // the map's night, or any night of the 24 h cycle",
    "if (lightingMode_ == 3 && aldeia() && !moonNight_) {",
    "if (lightingMode_ == 3 && dayCycle_ > 0 && !darkIndoor()) return cycleLighting(time, moon, phase);",
]
# Nothing of the tower's own sky (its scale, its clouds, its exposure of the sky) comes back.
FORBIDDEN_IN_MAPSTAGE = [
    "outsideScale_",
    "skyScaled",
    "bool cycleOn() const { return true; }",
]


def digest(path):
    # Line endings do not count (git turns LF into CRLF on checkout here).
    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def current():
    return {name: digest(ROOT / name) for name in SKY_FILES}


def fail(msg):
    print("SKY LOCK: " + msg)
    print("One sky for the whole game, locked (the user, 2026-10-23). The build stops here. See tools/scripts/check_sky_lock.py.")
    sys.exit(1)


def main():
    if "--relock" in sys.argv:
        LOCK.write_text(json.dumps(current(), indent=2) + "\n", encoding="utf-8")
        print("sky lock: relocked " + str(len(SKY_FILES)) + " files")
        return
    if not LOCK.exists():
        fail("content/sky/sky.locked.json is missing")
    locked = json.loads(LOCK.read_text(encoding="utf-8"))
    for name in SKY_FILES:
        if name not in locked:
            fail(name + " is not in the lock")
        path = ROOT / name
        if not path.exists():
            fail(name + " is missing")
        if digest(path) != locked[name]:
            fail(name + " changed without the user's order (relock only on their order)")
    text = MAPSTAGE.read_text(encoding="utf-8")
    for marker in MAPSTAGE_MARKERS:
        if marker not in text:
            fail("MapStage.h lost a one-sky line: " + marker)
    for bad in FORBIDDEN_IN_MAPSTAGE:
        if bad in text:
            fail("MapStage.h has a sky rule for one map: " + bad)
    print("sky lock: OK (" + str(len(SKY_FILES)) + " files, one sky on every map)")


if __name__ == "__main__":
    main()
