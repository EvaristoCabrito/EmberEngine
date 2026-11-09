import json
import math
import os
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TEX = ROOT / "content/assets/game/textures/material-overhaul-v01"


def png(name):
    return (TEX / name).read_bytes()


MATERIALS = {
    "content/models/ember/ember-lantern-v02.glb": [
        "lantern-smoked-amber-glass-v01.png",
        "lantern-forged-iron-v01.png",
        "lantern-aged-bronze-v01.png",
        "lantern-worn-iron-v01.png",
        "lantern-amber-flame-v01.png",
        "lantern-gold-flame-v01.png",
        "lantern-warm-core-v01.png",
    ],
    "content/assets/game/ember/ember/ember-lantern-v02.glb": [
        "lantern-smoked-amber-glass-v01.png",
        "lantern-forged-iron-v01.png",
        "lantern-aged-bronze-v01.png",
        "lantern-worn-iron-v01.png",
        "lantern-amber-flame-v01.png",
        "lantern-gold-flame-v01.png",
        "lantern-warm-core-v01.png",
    ],
    "content/models/treasure/coin-pile-v01.glb": ["coin-pile-gold-v01.png"],
    "content/models/sketchfab/swamp-shack/swamp-shack-water-512-v01.glb": ["swamp-water-v01.png"],
    "content/models/sketchfab/swamp-shack/swamp-shack-water-1k-v01.glb": ["swamp-water-v01.png"],
    "content/models/sketchfab/swamp-shack/swamp-shack-water-2k-v01.glb": ["swamp-water-v01.png"],
}

for animal, texture in {
    "ruffed-lemur-cranium": "bone-ruffed-lemur-cranium-v01.png",
    "orangutan-cranium": "bone-orangutan-cranium-v01.png",
    "mountain-gorilla-ulna-right": "bone-mountain-gorilla-ulna-v01.png",
    "mountain-gorilla-metacarpal2-left": "bone-mountain-gorilla-metacarpal-v01.png",
    "kloss-gibbon-cranium": "bone-kloss-gibbon-cranium-v01.png",
    "gorilla-cranium": "bone-gorilla-cranium-v01.png",
    "chimpanzee-fibula-right": "bone-chimpanzee-fibula-v01.png",
    "chimpanzee-metatarsal5-right": "bone-chimpanzee-metatarsal-v01.png",
    "black-crested-gibbon-mandible": "bone-black-crested-gibbon-mandible-v01.png",
}.items():
    for scale in ("512", "1k", "2k"):
        source = f"content/models/smithsonian/{animal}/{animal}-{scale}-v01.glb"
        MATERIALS[source] = [texture]

MATERIALS.update(
    {
        "content/assets/game/models/trees/dead-oak.glb": ["carvalho-morto-casca-v01.png"],
        "content/assets/game/models/trees/dead-snag.glb": ["arvore-morta-fina-casca-v01.png"],
        "content/assets/game/models/trees/twisted-stump.glb": [
            "toco-retorcido-casca-v01.png",
            "stump-heartwood-v01.png",
        ],
        "content/assets/game/models/trees/snowy-pine.glb": [
            "pine-needles-v01.png",
            "pine-needles-v01.png",
            "pine-needles-v01.png",
            "pine-needles-v01.png",
            "pine-bark-v01.png",
            "snow-powder-v01.png",
        ],
        "content/assets/game/models/trees/broadleaf.glb": [
            "broadleaf-leaves-v01.png",
            "broadleaf-leaves-v01.png",
            "broadleaf-leaves-v01.png",
            "broadleaf-leaves-v01.png",
            "broadleaf-leaves-v01.png",
            "broadleaf-bark-v01.png",
        ],
        "content/assets/game/models/rocks/grey-outcrop.glb": ["granite-grey-v01.png"],
    }
)


def glb_read(path):
    raw = path.read_bytes()
    magic, version, total = struct.unpack_from("<4sII", raw, 0)
    if magic != b"glTF" or version != 2 or total != len(raw):
        raise ValueError(f"Invalid GLB header: {path}")
    json_len, json_type = struct.unpack_from("<I4s", raw, 12)
    if json_type != b"JSON":
        raise ValueError(f"No JSON chunk: {path}")
    obj = json.loads(raw[20 : 20 + json_len].decode("utf-8"))
    offset = 20 + json_len
    bin_data = b""
    if offset < len(raw):
        bin_len, bin_type = struct.unpack_from("<I4s", raw, offset)
        if bin_type != b"BIN\0":
            raise ValueError(f"Unexpected GLB chunk: {path}")
        bin_data = raw[offset + 8 : offset + 8 + bin_len]
    return obj, bytearray(bin_data)


def accessor_vec3(obj, blob, accessor_index):
    accessor = obj["accessors"][accessor_index]
    if accessor["componentType"] != 5126 or accessor["type"] != "VEC3":
        raise ValueError("Texture UV generation expects float VEC3 positions/normals")
    view = obj["bufferViews"][accessor["bufferView"]]
    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    stride = view.get("byteStride", 12)
    return [struct.unpack_from("<3f", blob, start + i * stride) for i in range(accessor["count"])]


def add_uvs_for_trees(obj, blob, source_rel):
    projectable = (
        "/models/trees/" in source_rel
        or "/models/rocks/" in source_rel
        or source_rel.endswith("/ember-lantern-v02.glb")
    )
    if not projectable:
        return 0
    added = 0
    basename = Path(source_rel).name
    for mesh in obj.get("meshes", []):
        for prim in mesh.get("primitives", []):
            attrs = prim.get("attributes", {})
            if "TEXCOORD_0" in attrs:
                continue
            material_index = prim.get("material", 0)
            positions = accessor_vec3(obj, blob, attrs["POSITION"])
            normals = accessor_vec3(obj, blob, attrs["NORMAL"]) if "NORMAL" in attrs else None
            coords = []
            for i, (x, y, z) in enumerate(positions):
                if basename == "dead-oak.glb" or basename == "dead-snag.glb":
                    uv = (math.atan2(z, x) / (2 * math.pi) + 0.5, y * 0.38)
                elif basename == "twisted-stump.glb" and material_index == 1:
                    uv = (x * 1.8, z * 1.8)
                elif basename == "twisted-stump.glb":
                    uv = (math.atan2(z, x) / (2 * math.pi) + 0.5, y * 1.15)
                elif basename == "ember-lantern-v02.glb":
                    n = normals[i] if normals else (0.0, 1.0, 0.0)
                    ax, ay, az = map(abs, n)
                    if ay >= ax and ay >= az:
                        u, v = x, z
                    elif ax >= az:
                        u, v = z, y
                    else:
                        u, v = x, y
                    uv = (u * 2.0, v * 2.0)
                elif basename in ("snowy-pine.glb", "broadleaf.glb"):
                    n = normals[i] if normals else (0.0, 1.0, 0.0)
                    ax, ay, az = map(abs, n)
                    if ay >= ax and ay >= az:
                        u, v = x, z
                    elif ax >= az:
                        u, v = z, y
                    else:
                        u, v = x, y
                    uv = (u * 1.7, v * 1.7)
                else:  # granite outcrop, world-space triplanar-style projection
                    n = normals[i] if normals else (0.0, 1.0, 0.0)
                    ax, ay, az = map(abs, n)
                    if ay >= ax and ay >= az:
                        u, v = x, z
                    elif ax >= az:
                        u, v = z, y
                    else:
                        u, v = x, y
                    uv = (u * 1.5, v * 1.5)
                coords.append(uv)
            raw_uv = b"".join(struct.pack("<2f", *uv) for uv in coords)
            while len(blob) % 4:
                blob.append(0)
            view_index = len(obj.setdefault("bufferViews", []))
            obj["bufferViews"].append(
                {"buffer": 0, "byteOffset": len(blob), "byteLength": len(raw_uv), "target": 34962}
            )
            blob.extend(raw_uv)
            accessor_index = len(obj.setdefault("accessors", []))
            obj["accessors"].append(
                {
                    "bufferView": view_index,
                    "componentType": 5126,
                    "count": len(coords),
                    "type": "VEC2",
                    "min": [min(v[0] for v in coords), min(v[1] for v in coords)],
                    "max": [max(v[0] for v in coords), max(v[1] for v in coords)],
                }
            )
            attrs["TEXCOORD_0"] = accessor_index
            added += 1
    return added


def add_texture(obj, blob, filename, cache):
    if filename in cache:
        return cache[filename]
    image = png(filename)
    while len(blob) % 4:
        blob.append(0)
    view_index = len(obj.setdefault("bufferViews", []))
    obj["bufferViews"].append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(image)})
    blob.extend(image)
    image_index = len(obj.setdefault("images", []))
    obj["images"].append({"name": Path(filename).stem, "bufferView": view_index, "mimeType": "image/png"})
    texture_index = len(obj.setdefault("textures", []))
    obj["textures"].append({"name": Path(filename).stem, "source": image_index})
    cache[filename] = texture_index
    return texture_index


def write_glb(path, obj, blob):
    obj.setdefault("buffers", [{}])[0]["byteLength"] = len(blob)
    json_data = json.dumps(obj, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    json_data += b" " * ((-len(json_data)) % 4)
    blob.extend(b"\0" * ((-len(blob)) % 4))
    total = 12 + 8 + len(json_data) + 8 + len(blob)
    with path.open("wb") as f:
        f.write(struct.pack("<4sII", b"glTF", 2, total))
        f.write(struct.pack("<I4s", len(json_data), b"JSON"))
        f.write(json_data)
        f.write(struct.pack("<I4s", len(blob), b"BIN\0"))
        f.write(blob)


def main():
    results = []
    for rel, textures in sorted(MATERIALS.items()):
        texture_cache = {}  # Texture indices are local to each GLB.
        source = ROOT / rel
        if not source.exists():
            raise FileNotFoundError(source)
        if not TEX.exists() or any(not (TEX / name).exists() for name in textures):
            raise FileNotFoundError(f"Missing image for {rel}")
        output = source.with_name(source.stem + "-textured-v03.glb")
        if output.exists():
            raise FileExistsError(f"Refusing to overwrite versioned output: {output}")
        obj, blob = glb_read(source)
        uv_added = add_uvs_for_trees(obj, blob, rel)
        materials = obj.get("materials", [])
        if len(textures) != len(materials):
            raise ValueError(f"Material count mismatch: {rel}: {len(materials)} != {len(textures)}")
        for material, filename in zip(materials, textures):
            pbr = material.setdefault("pbrMetallicRoughness", {})
            texture_index = add_texture(obj, blob, filename, texture_cache)
            pbr["baseColorTexture"] = {"index": texture_index}
        write_glb(output, obj, blob)
        results.append(
            {
                "source": rel.replace("\\", "/"),
                "output": output.relative_to(ROOT).as_posix(),
                "materials_textured": len(materials),
                "uv_primitives_added": uv_added,
                "textures": textures,
                "bytes": output.stat().st_size,
                "status": "ok",
            }
        )
    report = ROOT / "work/material-overhaul-v03-report.json"
    report.write_text(json.dumps({"assets": results}, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps({"models": len(results), "materials": sum(r["materials_textured"] for r in results), "report": str(report)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
