"""Create non-destructive, photo-textured siblings for catalogued untextured GLBs.

Run with Blender 5.2:
  blender -b --python content/models/blender/apply_photo_textures.py

The originals are never modified. Successful outputs use a `-photo.glb` suffix;
the JSON report records which source/materials were processed.
"""
from __future__ import annotations

import json
import pathlib
import struct
import sys
import tempfile
import traceback

import bpy


ROOT = pathlib.Path(__file__).resolve().parents[3]
TEXTURES = ROOT / "content" / "models" / "textures"
REPORT = TEXTURES / "photoreal-texturing.report.json"
tempfile.tempdir = str(TEXTURES)

MAPS = {
    "bark": TEXTURES / "bark-bough-photo-v01.png",
    "leaves": TEXTURES / "foliage-leaves-photo-v01.png",
    "bone": TEXTURES / "bone-photo-v01.png",
    "clay": TEXTURES / "terracotta-photo-v01.png",
    "leather": TEXTURES / "leather-worn-photo-v01.png",
    "burlap": TEXTURES / "wt-burlap.jpg",
    "iron": TEXTURES / "wt-iron.jpg",
    "coal": TEXTURES / "wisp-dead-bark-v01.jpg",
    "glass": TEXTURES / "dark-green-glass-photo-v01.png",
    "granite": TEXTURES / "stone-rubble.jpg",
    "snow": TEXTURES / "snow.jpg",
}

FUNGUS_MAPS = {
    "extra/mushroom-cluster-v01.glb": TEXTURES / "mushroom-fallen-ground-user-v01.jpg",
    "nature/wild-mushrooms-v01.glb": TEXTURES / "shelf-fungus-top-user-v02.jpg",
    "nature/roots-with-mushrooms-v01.glb": TEXTURES / "mushroom-mycelium-wall-user-v01.jpg",
    "mines/giant-mushrooms-v01.glb": TEXTURES / "shelf-fungus-top-user-v02.jpg",
}

PUBLIC_MODELS = [
    ROOT / "content/assets/game/models/rocks/grey-outcrop.glb",
    ROOT / "content/assets/game/models/trees/broadleaf.glb",
    ROOT / "content/assets/game/models/trees/dead-oak.glb",
    ROOT / "content/assets/game/models/trees/dead-snag.glb",
    ROOT / "content/assets/game/models/trees/snowy-pine.glb",
    ROOT / "content/assets/game/models/trees/twisted-stump.glb",
]


def embedded_json(path: pathlib.Path) -> dict:
    data = path.read_bytes()
    if data[:4] != b"glTF":
        raise ValueError(f"Not a GLB: {path}")
    offset = 12
    while offset + 8 <= len(data):
        size, kind = struct.unpack_from("<II", data, offset)
        offset += 8
        chunk = data[offset:offset + size]
        offset += size
        if kind == 0x4E4F534A:
            return json.loads(chunk.decode("utf-8"))
    raise ValueError(f"GLB has no JSON chunk: {path}")


def has_image_texture(path: pathlib.Path) -> bool:
    gltf = embedded_json(path)
    textures = gltf.get("textures", [])
    images = gltf.get("images", [])
    materials = gltf.get("materials", [])
    return any(
        any(
            isinstance(binding, dict)
            and isinstance(binding.get("index"), int)
            and binding["index"] < len(textures)
            for binding in (
                material.get("pbrMetallicRoughness", {}).get("baseColorTexture"),
                material.get("normalTexture"),
                material.get("emissiveTexture"),
            )
        )
        for material in materials
    ) or bool(images)


def material_map(name: str, source: pathlib.Path) -> pathlib.Path | None:
    label = name.lower()
    relative = source.relative_to(ROOT / "content/models").as_posix() if source.is_relative_to(ROOT / "content/models") else ""
    if "fungus" in label:
        return FUNGUS_MAPS.get(relative)
    if "bark" in label:
        return MAPS["bark"]
    if "leaf" in label or "leaves" in label or "fern" in label:
        return MAPS["leaves"]
    if "bone" in label or "skull" in label:
        return MAPS["bone"]
    if "clay" in label or "ceramic" in label:
        return MAPS["clay"]
    if "leather" in label:
        return MAPS["leather"]
    if "burlap" in label or "rope" in label or "cloth" in label or "wool" in label:
        return MAPS["burlap"]
    if "glass" in label:
        return MAPS["glass"]
    if "snow" in label or "needle" in label:
        return MAPS["snow"]
    if "granite" in label or "stone" in label:
        return MAPS["granite"]
    if any(token in label for token in ("iron", "metal", "brass", "bronze", "gold")):
        return MAPS["iron"]
    if "coal" in label:
        return MAPS["coal"]
    return None


def add_base_color_map(material, image):
    material.use_nodes = True
    tree = material.node_tree
    bsdf = next((node for node in tree.nodes if node.type == "BSDF_PRINCIPLED"), None)
    if bsdf is None:
        raise ValueError(f"No Principled BSDF in material {material.name!r}")
    base = bsdf.inputs.get("Base Color")
    if base is None:
        raise ValueError(f"No Base Color input in material {material.name!r}")
    previous = next((link for link in list(tree.links) if link.to_socket == base), None)
    original_factor = tuple(base.default_value)
    tex = tree.nodes.new("ShaderNodeTexImage")
    tex.name = "Photoreal Base Color"
    tex.label = "Photoreal Base Color"
    tex.image = image
    tex.interpolation = "Linear"
    mix = tree.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.blend_type = "MULTIPLY"
    mix.inputs["Factor"].default_value = 1.0
    tree.links.new(tex.outputs["Color"], mix.inputs[6])
    if previous:
        old_socket = previous.from_socket
        tree.links.remove(previous)
        tree.links.new(old_socket, mix.inputs[7])
    else:
        mix.inputs[7].default_value = original_factor
    tree.links.new(mix.outputs[2], base)


def ensure_uv(obj):
    if obj.type != "MESH" or obj.data.uv_layers:
        return False
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(island_margin=0.02)
    bpy.ops.object.mode_set(mode="OBJECT")
    return True


def process(source: pathlib.Path) -> dict:
    target = source.with_name(source.stem + "-photo.glb")
    blend = target.with_suffix(".blend")
    if target.exists() and blend.exists():
        return {"source": source.relative_to(ROOT).as_posix(), "glb": target.relative_to(ROOT).as_posix(), "status": "already-generated"}
    if target.exists() or blend.exists():
        raise FileExistsError(f"Refusing to overwrite existing output: {target}")
    if has_image_texture(source):
        return {"source": source.relative_to(ROOT).as_posix(), "status": "already-textured"}

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(source))
    all_meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    if not all_meshes:
        raise ValueError(f"Imported model has no mesh objects: {source}")

    images: dict[str, object] = {}
    assigned: set[str] = set()
    unmapped: set[str] = set()
    unwrapped = 0
    for obj in all_meshes:
        for slot in obj.material_slots:
            material = slot.material
            if material is None:
                continue
            path = material_map(material.name, source)
            if path is None:
                unmapped.add(material.name)
                continue
            if not path.is_file():
                raise FileNotFoundError(f"Missing texture {path} for {material.name}")
            image = images.get(str(path))
            if image is None:
                image = bpy.data.images.load(str(path), check_existing=True)
                image.colorspace_settings.name = "sRGB"
                images[str(path)] = image
            add_base_color_map(material, image)
            assigned.add(material.name)

    if not assigned:
        raise ValueError(f"No material received a photo texture: {source}")

    for obj in all_meshes:
        if any(slot.material and any(
            node.type == "TEX_IMAGE" and node.image and node.label == "Photoreal Base Color"
            for node in slot.material.node_tree.nodes
        ) for slot in obj.material_slots if slot.material):
            unwrapped += int(ensure_uv(obj))

    for image in images.values():
        image.pack()

    bpy.ops.object.select_all(action="SELECT")
    if "export_vertex_color" in bpy.ops.export_scene.gltf.get_rna_type().properties.keys():
        vertex_color = "MATERIAL"
    else:
        vertex_color = None
    export_args = dict(
        filepath=str(target), export_format="GLB", use_selection=True,
        export_apply=True, export_yup=True, export_texcoords=True,
        export_normals=True, export_materials="EXPORT", export_animations=False,
        export_skins=False, export_lights=False, export_cameras=False,
        export_image_format="JPEG", export_jpeg_quality=92,
    )
    if vertex_color is not None:
        export_args["export_vertex_color"] = vertex_color
    bpy.ops.export_scene.gltf(**export_args)
    bpy.ops.wm.save_as_mainfile(filepath=str(blend), compress=True, relative_remap=True)

    result = {
        "source": source.relative_to(ROOT).as_posix(),
        "glb": target.relative_to(ROOT).as_posix(),
        "blend": blend.relative_to(ROOT).as_posix(),
        "materials_textured": sorted(assigned),
        "materials_kept_as_authored": sorted(unmapped),
        "uv_unwrapped_meshes": unwrapped,
        "bytes": target.stat().st_size,
        "status": "ok",
    }
    print("[photo] " + json.dumps(result, ensure_ascii=False))
    return result


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    catalog = json.loads((ROOT / "content/models/models.json").read_text(encoding="utf-8"))
    candidates = set()
    for model in catalog.get("models", []):
        filename = model.get("file", "")
        if filename.startswith("/game/ember/"):
            candidates.add(ROOT / "content/models" / filename[len("/game/ember/"):])
    candidates.update(PUBLIC_MODELS)
    sources = sorted(path for path in candidates if path.is_file() and not has_image_texture(path))
    if args:
        filters = tuple(value.lower() for value in args)
        sources = [path for path in sources if any(value in path.as_posix().lower() for value in filters)]
    results = []
    for source in sources:
        try:
            results.append(process(source))
        except Exception as error:
            traceback.print_exc()
            results.append({"source": source.relative_to(ROOT).as_posix(), "status": "error", "error": str(error)})
    REPORT.write_text(json.dumps({"assets": results}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    ok = sum(entry["status"] == "ok" for entry in results)
    errors = sum(entry["status"] == "error" for entry in results)
    print(f"[photo] complete: {ok} textured, {errors} errors, {len(results)} inputs")


if __name__ == "__main__":
    main()
