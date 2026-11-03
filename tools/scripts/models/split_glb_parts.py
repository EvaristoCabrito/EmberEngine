"""Split a GLB into parts that keep the original's coordinates (no recentring), one GLB per part, for a model whose pieces move on
their own in the game (the iron gate's leaves swinging open like the doors: the user, 2026-11-03, "Open, like the doors").

Each part = the named nodes only (the Cooker reads every node of a GLB, so the others are left out of the node list); buffers,
materials and textures are kept whole (the cooker takes what the kept nodes use).

  python -I tools/scripts/models/split_glb_parts.py <in.glb> <out-prefix> part=node[,node...] [part=...]
  e.g. ... content/models/polyhaven/large_iron_gate-v01.glb content/models/polyhaven/large_iron_gate- \\
           frame-v01=large_iron_gate leaf-left-v01=large_iron_gate_left_door leaf-right-v01=large_iron_gate_right_door,large_iron_gate_bolt
"""
import json
import struct
import sys


def read_glb(path):
    b = open(path, "rb").read()
    magic, version, length = struct.unpack_from("<4sII", b, 0)
    assert magic == b"glTF" and version == 2
    chunks, off = [], 12
    while off < length:
        size, kind = struct.unpack_from("<I4s", b, off)
        chunks.append((kind, b[off + 8:off + 8 + size]))
        off += 8 + size
    j = json.loads(chunks[0][1])
    return j, [c for c in chunks[1:]]


def write_glb(path, j, rest):
    js = json.dumps(j, separators=(",", ":")).encode("utf-8")
    js += b" " * ((4 - len(js) % 4) % 4)
    body = struct.pack("<I4s", len(js), b"JSON") + js
    for kind, data in rest:
        data = data + b"\0" * ((4 - len(data) % 4) % 4)
        body += struct.pack("<I4s", len(data), kind) + data
    open(path, "wb").write(struct.pack("<4sII", b"glTF", 2, 12 + len(body)) + body)


src, prefix = sys.argv[1], sys.argv[2]
j0, rest = read_glb(src)
names = [n.get("name") for n in j0["nodes"]]
assert all(not n.get("children") for n in j0["nodes"]), "nested nodes: not handled"
for spec in sys.argv[3:]:
    part, wanted = spec.split("=", 1)
    keep = [names.index(w) for w in wanted.split(",")]
    j = json.loads(json.dumps(j0))
    j["nodes"] = [j0["nodes"][i] for i in keep]
    j["scenes"] = [{"nodes": list(range(len(keep)))}]
    j["scene"] = 0
    out = f"{prefix}{part}.glb"
    write_glb(out, j, rest)
    print(out, [names[i] for i in keep])
