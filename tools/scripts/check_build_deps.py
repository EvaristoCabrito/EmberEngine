"""Before a build: every object of the game's own code (engine/, apps/, tools/) must have its header dependencies recorded in
ninja's log, or a change to a header never recompiles it (2026-11-05: apps/sandbox main.cpp.obj had "#deps 0": it had been
compiled outside this folder and copied in, so OverworldStage.h's changes never reached the exe).

An object with no record or "#deps 0" is set aside (renamed "<name>.nodeps-<stamp>", never deleted), so ninja compiles it again
and records its headers. Third-party objects (_deps) are left alone.

    python -I tools/scripts/check_build_deps.py build/release
"""
import os
import subprocess
import sys
import time

build = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "build/release")
if not os.path.isfile(os.path.join(build, "build.ninja")):
    sys.exit(0)  # not configured yet: the first build records everything

try:
    out = subprocess.run(["ninja", "-C", build, "-t", "deps"], capture_output=True, text=True, errors="replace").stdout
except FileNotFoundError:
    print("check_build_deps: ninja not found, skipped")
    sys.exit(0)

recorded = {}
for line in out.splitlines():
    if line and not line.startswith(" ") and ": #deps " in line:
        target, rest = line.split(": #deps ", 1)
        recorded[target.replace("\\", "/")] = int(rest.split(",", 1)[0])

# the objects this build makes (build.ninja's "build <obj>: CXX_COMPILER__..." lines), own code only
own = []
with open(os.path.join(build, "build.ninja"), encoding="utf-8", errors="replace") as f:
    for line in f:
        if line.startswith("build ") and "_COMPILER__" in line:
            target = line[6:line.index(":", 8)].replace("$:", ":").replace("\\", "/")
            if target.endswith(".obj") and target.split("/", 1)[0] in ("engine", "apps", "tools"):
                own.append(target)

stamp = time.strftime("%Y%m%d-%H%M%S")
moved = 0
for target in own:
    path = os.path.join(build, target)
    if not os.path.isfile(path):
        continue  # not built yet
    if recorded.get(target, 0) > 0:
        continue
    os.replace(path, f"{path}.nodeps-{stamp}")
    moved += 1
    print(f"check_build_deps: {target} had no header record: set aside, it will be compiled again")
if moved:
    print(f"check_build_deps: {moved} object(s) set aside")
