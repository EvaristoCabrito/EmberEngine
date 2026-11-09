import os
import subprocess
import sys


def changed_paths():
    raw = subprocess.check_output(
        ["git", "status", "--porcelain=v1", "-z", "--untracked-files=all"]
    )
    records = raw.split(b"\0")
    paths = []
    i = 0
    while i < len(records) and records[i]:
        record = records[i]
        i += 1
        status = record[:2]
        path = record[3:].decode("utf-8", errors="surrogateescape")
        if b"R" in status or b"C" in status:
            i += 1
        if path == "JOGAR-log.txt" or path.startswith("work/") or path.startswith(".claude/"):
            continue
        if path.endswith("-textured-v01.glb") or path.endswith("-textured-v02.glb"):
            continue
        paths.append(path)
    return paths


paths = changed_paths()
total = 0
largest = (0, "")
for path in paths:
    if os.path.isfile(path):
        size = os.path.getsize(path)
        total += size
        if size > largest[0]:
            largest = (size, path)

print(f"paths={len(paths)} bytes={total} largest={largest[0]}:{largest[1]}")
if "--stage" in sys.argv:
    if paths:
        payload = b"\0".join(p.encode("utf-8", errors="surrogateescape") for p in paths) + b"\0"
        subprocess.run(
            ["git", "add", "--pathspec-from-file=-", "--pathspec-file-nul"],
            input=payload,
            check=True,
        )
    print(f"staged={len(paths)}")
