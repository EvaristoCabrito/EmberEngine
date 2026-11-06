"""Photo textures on the last untextured 3D pieces the user kept (2026-11-05: "Todos esses objetos são válidos, todos devem ser
texturizados"): the nine Smithsonian bones (one material "default", scanned UVs kept) and the Ember lantern (glass, iron, brass,
flame; no UVs: smart-projected). New versions only: <name>-v02.glb beside each source, the originals untouched.

  blender -b --python content/models/blender/texture_bones_lantern.py
"""
import json
import pathlib

import bpy

ROOT = pathlib.Path(__file__).resolve().parents[3]
TEX = ROOT / "content" / "models" / "textures"
BONES = ["gorilla-cranium", "orangutan-cranium", "ruffed-lemur-cranium", "kloss-gibbon-cranium", "black-crested-gibbon-mandible",
         "chimpanzee-fibula-right", "chimpanzee-metatarsal5-right", "mountain-gorilla-ulna-right", "mountain-gorilla-metacarpal2-left"]
BONE_SRC = ROOT / "build" / "meshes" / "src-v38" / "ember" / "smithsonian"   # the files the game cooked (v01)
BONE_OUT = ROOT / "content" / "models" / "smithsonian"
LANTERN_SRC = ROOT / "content" / "models" / "ember" / "ember-lantern-v02.glb"
LANTERN_OUT = ROOT / "content" / "models" / "ember" / "ember-lantern-v03.glb"


def lantern_map(name):
    n = name.lower()
    if "glass" in n:
        return TEX / "lamp-glass-v01.jpg"
    if "flame" in n:
        return TEX / "brazier-flame-v01.png"
    if "iron" in n or "brass" in n or "bronze" in n:
        return TEX / "lamp-iron-v01.jpg"
    return TEX / "lamp-iron-v01.jpg"


def texture_material(mat, path, images):
    mat.use_nodes = True
    tree = mat.node_tree
    bsdf = next(n for n in tree.nodes if n.type == "BSDF_PRINCIPLED")
    base = bsdf.inputs["Base Color"]
    img = images.get(str(path))
    if img is None:
        img = images[str(path)] = bpy.data.images.load(str(path), check_existing=True)
        img.colorspace_settings.name = "sRGB"
    tex = tree.nodes.new("ShaderNodeTexImage")
    tex.image = img
    prev = next((l for l in tree.links if l.to_socket == base), None)
    factor = tuple(base.default_value)
    mix = tree.nodes.new("ShaderNodeMix")
    mix.data_type, mix.blend_type = "RGBA", "MULTIPLY"
    mix.inputs["Factor"].default_value = 1.0
    tree.links.new(tex.outputs["Color"], mix.inputs[6])
    if prev:
        src = prev.from_socket
        tree.links.remove(prev)
        tree.links.new(src, mix.inputs[7])
    else:
        # bone: the photo carries the colour (white factor); lantern: tint by the authored colour
        mix.inputs[7].default_value = factor
    tree.links.new(mix.outputs[2], base)


def smart_uv(obj):
    if obj.data.uv_layers:
        return
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(island_margin=0.02)
    bpy.ops.object.mode_set(mode="OBJECT")


def run(src, out, mapping):
    if out.exists():
        print(f"[skip] {out} exists")
        return
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(src))
    images, done = {}, []
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    for o in meshes:
        smart_uv(o)
        for slot in o.material_slots:
            if slot.material and slot.material.name not in done:
                texture_material(slot.material, mapping(slot.material.name), images)
                done.append(slot.material.name)
    for img in images.values():
        img.pack()
    out.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.export_scene.gltf(filepath=str(out), export_format="GLB", use_selection=True, export_apply=True, export_yup=True,
                              export_texcoords=True, export_normals=True, export_materials="EXPORT", export_animations=False,
                              export_image_format="JPEG", export_jpeg_quality=92)
    print("[ok] " + json.dumps({"out": str(out.relative_to(ROOT)), "materials": done}))


for b in BONES:
    run(BONE_SRC / f"{b}-v01.glb", BONE_OUT / f"{b}-v02.glb", lambda name: TEX / "bone-photo-v01.png")
run(LANTERN_SRC, LANTERN_OUT, lantern_map)
