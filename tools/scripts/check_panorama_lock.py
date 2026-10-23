"""The panorama lock (the user, 2026-10-23: "todos os mapas do jogo são panoramas, eu quero travar permanentemente qualquer tentativa
de desfazer essa única regra fixa de todos os mapas").

Run by build.bat before every build (and by hand: python -I tools/scripts/check_panorama_lock.py). The build FAILS when:
  1. content/engine2/backdrop-fit.json lost a map listed in content/engine2/backdrop-fit.locked.json, lost one of that map's
     panoramas, or changed a locked painting entry's values;
  2. shaders/resolve.hlsl lost the panorama path (its modes 2-4 and 5, the mirrored repeat, the room's flat ceiling);
  3. apps/sandbox/MapStage.h no longer opens a map on its newest panorama by default.
To change a locked value on the user's explicit order, change backdrop-fit.json AND copy it over backdrop-fit.locked.json in the
same commit (python -I tools/scripts/check_panorama_lock.py --relock), saying so in the commit message. Never relock on your own."""
import json
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FIT = ROOT / "content/engine2/backdrop-fit.json"
LOCK = ROOT / "content/engine2/backdrop-fit.locked.json"
RESOLVE = ROOT / "shaders/resolve.hlsl"
MAPSTAGE = ROOT / "apps/sandbox/MapStage.h"

SHADER_MARKERS = [
    "push.backgroundMode == 2 || push.backgroundMode == 3 || push.backgroundMode == 4",
    "m -= 2.0 * floor(m * 0.5);  // mirrored repeat",
    "float3 ceilingAt(MeshFrame f, float3 dir, float pixelAngle)",
    "if (push.ceilingTexture != kNoTexture) sky = ceilingAt(f, dir, pixelAngle);",
    "if (realElevation < 0.0 && push.planeTexture != kNoTexture)",
    "push.backgroundMode == 5",
]
MAPSTAGE_MARKERS = [
    "if (options.background < 0 && !panoramas_.empty()) background_ = 4 + static_cast<int>(panoramas_.size());",
]


def fail(msg):
    print("PANORAMA LOCK: " + msg)
    print("Every map is a panorama, no exception (the user, 2026-10-23). The build stops here. See tools/scripts/check_panorama_lock.py.")
    sys.exit(1)


def main():
    if "--relock" in sys.argv:
        shutil.copyfile(FIT, LOCK)
        print(f"relocked: {LOCK.relative_to(ROOT)} = {FIT.relative_to(ROOT)} (only on the user's explicit order)")
        return
    if not LOCK.exists():
        fail(f"{LOCK.relative_to(ROOT)} is missing (never delete it)")
    fit = json.loads(FIT.read_text(encoding="utf-8"))
    lock = json.loads(LOCK.read_text(encoding="utf-8"))
    for map_id, panoramas in lock.get("maps", {}).items():
        have = fit.get("maps", {}).get(map_id)
        if have is None:
            fail(f"map '{map_id}' lost its panoramas in backdrop-fit.json")
        for p in panoramas:
            if p not in have:
                fail(f"map '{map_id}' lost panorama '{p}' in backdrop-fit.json")
    for name, entry in lock.get("paintings", {}).items():
        have = fit.get("paintings", {}).get(name)
        if have is None:
            fail(f"painting entry '{name}' is gone from backdrop-fit.json")
        if have != entry:
            changed = sorted(k for k in set(entry) | set(have) if entry.get(k) != have.get(k))
            fail(f"painting entry '{name}' changed without relocking: {', '.join(changed)}")
    shader = RESOLVE.read_text(encoding="utf-8")
    for marker in SHADER_MARKERS:
        if marker not in shader:
            fail(f"shaders/resolve.hlsl lost the panorama path: '{marker}' is missing")
    stage = MAPSTAGE.read_text(encoding="utf-8")
    for marker in MAPSTAGE_MARKERS:
        if marker not in stage:
            fail(f"MapStage.h no longer opens a map on its newest panorama by default: '{marker}' is missing")
    print("panorama lock: OK")


if __name__ == "__main__":
    main()
