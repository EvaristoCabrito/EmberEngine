"""Fire kit v01: the 3D props that carry the game's real 3D fire (shaders/vfx.hlsl FireVolume) in place of the 2D fire cards.

Headless: blender.exe -b -t 2 -P tools/scripts/models/create_fire_kit_v01.py
1 unit = one hex radius (a human is 2.65 units). Four props, none with a flame of its own (the engine draws the fire as a
volume at the points listed under FIRE below, written to the manifest for register_fire_kit_v01.py):
  candle           an iron candlestick with a wax candle
  torch-stand      a standing torch: an iron tripod foot, an oak pole, an iron basket of pitch
  brazier-tripod   a forged tripod holding a bowl of coals
  campfire         a ring of stones, charred logs leaning together over a bed of embers
Textures: the stash already in content/ (wt-iron, wt-dark-oak, wt-limestone, the Wisp dead bark, ambientCG rock-030 and lava-001).
Outputs content/models/fire/<name>-v01.glb and build/models-tmp/fire/manifest.json. Never an overwrite.
"""
import json
import math
import random
from pathlib import Path

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
VERSION = "v01"
OUT = ROOT / "content" / "models" / "fire"
TMP = ROOT / "build" / "models-tmp" / "fire"
TEX = ROOT / "content" / "models" / "textures"
ACG = ROOT / "content" / "textures" / "ambientcg"
OUT.mkdir(parents=True, exist_ok=True)
TMP.mkdir(parents=True, exist_ok=True)
bpy.context.preferences.filepaths.save_version = 0
rng = random.Random(20261112)

# Where each prop burns, in its own space (x, y across, z up, units): the foot of each flame, its height and radius, and how
# wild it is (1 a torch, less a candle).
FIRE = {
    "candle": [{"at": [0.0, 0.0, 0.93], "height": 0.2, "radius": 0.04, "turbulence": 0.3}],
    "torch-stand": [{"at": [0.0, 0.0, 1.72], "height": 0.7, "radius": 0.17, "turbulence": 1.0}],
    "brazier-tripod": [{"at": [0.0, 0.0, 1.14], "height": 0.8, "radius": 0.27, "turbulence": 1.0}],
    "campfire": [{"at": [0.0, 0.0, 0.14], "height": 1.0, "radius": 0.3, "turbulence": 1.0},
                 {"at": [-0.2, 0.1, 0.1], "height": 0.62, "radius": 0.2, "turbulence": 1.0},
                 {"at": [0.18, -0.12, 0.1], "height": 0.7, "radius": 0.21, "turbulence": 1.0}],
}
NAMES = {"candle": "Vela em castiçal 3D", "torch-stand": "Tocha de pé 3D", "brazier-tripod": "Braseiro de tripé 3D", "campfire": "Fogueira 3D"}


def clear():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for m in list(bpy.data.materials):
        bpy.data.materials.remove(m)


def mat(name, image, metal=0.0, rough=0.8, tint=None, emission=None, emission_strength=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nodes, links = m.node_tree.nodes, m.node_tree.links
    bs = nodes.get("Principled BSDF")
    tex = nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(str(image), check_existing=True)
    if tint:
        mix = nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.blend_type = "MULTIPLY"
        mix.inputs["Factor"].default_value = 1.0
        mix.inputs["B"].default_value = (*tint, 1.0)
        links.new(tex.outputs["Color"], mix.inputs["A"])
        links.new(mix.outputs["Result"], bs.inputs["Base Color"])
    else:
        links.new(tex.outputs["Color"], bs.inputs["Base Color"])
    bs.inputs["Metallic"].default_value = metal
    bs.inputs["Roughness"].default_value = rough
    if emission:
        etex = nodes.new("ShaderNodeTexImage")
        etex.image = bpy.data.images.load(str(emission), check_existing=True)
        links.new(etex.outputs["Color"], bs.inputs["Emission Color"])
        bs.inputs["Emission Strength"].default_value = emission_strength
    return m


def soften(o, width, segments=2):
    if width:
        b = o.modifiers.new("Worn edges", "BEVEL")
        b.width = width
        b.segments = segments
        o.modifiers.new("Weighted normals", "WEIGHTED_NORMAL")
    return o


def cylinder(name, loc, r, depth, material, segments=32, r_top=None, bevel=0.012):
    if r_top is None:
        bpy.ops.mesh.primitive_cylinder_add(vertices=segments, radius=r, depth=depth, location=loc)
    else:
        bpy.ops.mesh.primitive_cone_add(vertices=segments, radius1=r, radius2=r_top, depth=depth, location=loc)
    o = bpy.context.object
    o.name = name
    o.data.materials.append(material)
    bpy.ops.object.shade_smooth()
    return soften(o, bevel)


def rod(name, a, b, r, material, segments=14, r_end=None, bevel=None):
    a, b = Vector(a), Vector(b)
    d = b - a
    if r_end is None:
        bpy.ops.mesh.primitive_cylinder_add(vertices=segments, radius=r, depth=d.length, location=(a + b) / 2)
    else:
        bpy.ops.mesh.primitive_cone_add(vertices=segments, radius1=r, radius2=r_end, depth=d.length, location=(a + b) / 2)
    o = bpy.context.object
    o.name = name
    o.rotation_mode = "QUATERNION"
    o.rotation_quaternion = d.to_track_quat("Z", "Y")
    o.data.materials.append(material)
    bpy.ops.object.shade_smooth()
    return soften(o, min(0.012, r * 0.28) if bevel is None else bevel)


def torus(name, loc, major, minor, material, majorseg=40, minorseg=10):
    bpy.ops.mesh.primitive_torus_add(major_radius=major, minor_radius=minor, major_segments=majorseg, minor_segments=minorseg, location=loc)
    o = bpy.context.object
    o.name = name
    o.data.materials.append(material)
    bpy.ops.object.shade_smooth()
    return o


def lathe(name, profile, material, segments=48):
    """A surface of revolution about z through (radius, z) points."""
    verts, faces = [], []
    for r, z in profile:
        for i in range(segments):
            a = math.tau * i / segments
            verts.append((r * math.cos(a), r * math.sin(a), z))
    for ring in range(len(profile) - 1):
        a0, b0 = ring * segments, (ring + 1) * segments
        for i in range(segments):
            j = (i + 1) % segments
            faces.append((a0 + i, a0 + j, b0 + j, b0 + i))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.materials.append(material)
    mesh.update()
    o = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(o)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = o
    o.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    bpy.ops.object.shade_smooth()
    o.select_set(False)
    return o


def lump(name, loc, size, material, seed, squash=0.7, detail=3, rough=0.22):
    """A lumpy stone or coal: an icosphere pushed about by its own seeded noise."""
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=detail, radius=1.0, location=(0, 0, 0))
    o = bpy.context.object
    o.name = name
    r = random.Random(seed)
    axes = [Vector((r.uniform(-1, 1), r.uniform(-1, 1), r.uniform(-1, 1))).normalized() for _ in range(7)]
    amps = [r.uniform(-rough, rough) for _ in axes]
    for v in o.data.vertices:
        n = v.co.normalized()
        v.co = n * (1.0 + sum(a * max(0.0, n.dot(ax)) ** 2 for a, ax in zip(amps, axes)))
    o.scale = (size * r.uniform(0.85, 1.2), size * r.uniform(0.8, 1.15), size * squash * r.uniform(0.85, 1.15))
    o.rotation_euler = (0, 0, r.uniform(0, math.tau))
    o.location = loc
    o.data.materials.append(material)
    bpy.ops.object.shade_smooth()
    return o


def finish(key):
    name = f"{key}-{VERSION}"
    glb = OUT / f"{name}.glb"
    if glb.exists():
        raise FileExistsError(f"Refusing to overwrite existing file: {glb}")
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.convert(target="MESH")  # the bevels become geometry
    bpy.ops.object.join()
    o = bpy.context.object
    o.name = name
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(island_margin=0.006)
    bpy.ops.object.mode_set(mode="OBJECT")
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    o.data.calc_loop_triangles()
    xs = [v.co.x for v in o.data.vertices]
    ys = [v.co.y for v in o.data.vertices]
    zs = [v.co.z for v in o.data.vertices]
    bpy.ops.export_scene.gltf(filepath=str(glb), export_format="GLB", use_selection=True, export_apply=True, export_texcoords=True)
    return {"key": key, "name": NAMES[key], "file": glb.name, "height": round(max(zs) - min(zs), 3),
            "width": round(max(max(xs) - min(xs), max(ys) - min(ys)), 3), "centre": [round((max(xs) + min(xs)) / 2, 4), round((max(ys) + min(ys)) / 2, 4)],
            "base": round(min(zs), 4), "triangles": len(o.data.loop_triangles), "fire": FIRE[key]}


def materials():
    return {
        "iron": mat("Smoke-darkened forged iron", TEX / "wt-iron.jpg", 0.78, 0.5),
        "oak": mat("Oiled dark oak", TEX / "wt-dark-oak.jpg", 0.0, 0.82),
        "wax": mat("Tallow wax", TEX / "wt-limestone.jpg", 0.0, 0.55, tint=(1.0, 0.93, 0.74)),
        "pitch": mat("Pitch-soaked wrapping", TEX / "wt-burlap.jpg", 0.0, 0.9, tint=(0.16, 0.13, 0.11)),
        "stone": mat("Hearth stone", ACG / "rock-030" / "rock-030_color_1k.jpg", 0.0, 0.9),
        "bark": mat("Charred log", TEX / "wisp-dead-bark-v01.jpg", 0.0, 0.92, tint=(0.5, 0.46, 0.43)),
        "coal": mat("Glowing embers", ACG / "lava-001" / "lava-001_color_1k.jpg", 0.0, 0.85,
                    emission=ACG / "lava-001" / "lava-001_emission_1k.jpg", emission_strength=3.0),
    }


def candle():
    clear()
    m = materials()
    cylinder("Weighted foot", (0, 0, 0.02), 0.19, 0.04, m["iron"], 36)
    cylinder("Stepped foot", (0, 0, 0.055), 0.13, 0.03, m["iron"], 36)
    torus("Foot ring", (0, 0, 0.04), 0.19, 0.012, m["iron"])
    rod("Stem", (0, 0, 0.07), (0, 0, 0.66), 0.022, m["iron"], 16)
    for z, r in ((0.2, 0.04), (0.38, 0.05), (0.56, 0.038)):
        lathe("Stem knop", [(0.022, z - 0.035), (r, z - 0.012), (r, z + 0.012), (0.022, z + 0.035)], m["iron"], 24)
    lathe("Drip pan", [(0.02, 0.655), (0.12, 0.675), (0.15, 0.71), (0.14, 0.715), (0.11, 0.69), (0.02, 0.675)], m["iron"], 40)
    cylinder("Candle cup", (0, 0, 0.705), 0.05, 0.05, m["iron"], 24)
    cylinder("Wax candle", (0, 0, 0.82), 0.04, 0.21, m["wax"], 24, r_top=0.036, bevel=0.006)
    for i in range(5):  # runs of wax down the side
        a = rng.uniform(0, math.tau)
        top = rng.uniform(0.84, 0.92)
        rod("Wax run", (math.cos(a) * 0.039, math.sin(a) * 0.039, top), (math.cos(a) * 0.042, math.sin(a) * 0.042, top - rng.uniform(0.05, 0.13)),
            0.008, m["wax"], 8, r_end=0.004, bevel=0)
    rod("Wick", (0, 0, 0.92), (0.004, 0, 0.945), 0.004, m["pitch"], 6, bevel=0)
    return finish("candle")


def torch_stand():
    clear()
    m = materials()
    for i in range(3):
        a = math.tau * i / 3
        ux, uy = math.cos(a), math.sin(a)
        rod("Splayed foot", (ux * 0.34, uy * 0.34, 0.03), (ux * 0.03, uy * 0.03, 0.42), 0.026, m["iron"], 12)
        cylinder("Foot pad", (ux * 0.34, uy * 0.34, 0.012), 0.05, 0.024, m["iron"], 16, bevel=0.006)
    torus("Foot collar", (0, 0, 0.43), 0.05, 0.016, m["iron"], 24, 8)
    rod("Oak pole", (0, 0, 0.3), (0, 0, 1.5), 0.042, m["oak"], 14, r_end=0.036)
    torus("Pole band", (0, 0, 0.9), 0.043, 0.01, m["iron"], 24, 8)
    torus("Head collar", (0, 0, 1.46), 0.045, 0.014, m["iron"], 24, 8)
    # the basket: six bars opening upwards between two hoops, the pitch-soaked head inside
    for i in range(6):
        a = math.tau * i / 6
        ux, uy = math.cos(a), math.sin(a)
        rod("Basket bar", (ux * 0.045, uy * 0.045, 1.46), (ux * 0.13, uy * 0.13, 1.76), 0.011, m["iron"], 8)
    torus("Basket hoop", (0, 0, 1.6), 0.09, 0.011, m["iron"], 28, 8)
    torus("Basket rim", (0, 0, 1.76), 0.13, 0.013, m["iron"], 32, 8)
    cylinder("Pitch head", (0, 0, 1.61), 0.07, 0.26, m["pitch"], 16, r_top=0.1, bevel=0.01)
    lump("Charred crown", (0, 0, 1.73), 0.085, m["coal"], 11, squash=0.45, detail=2)
    return finish("torch-stand")


def brazier_tripod():
    clear()
    m = materials()
    for i in range(3):
        a = math.tau * i / 3 + 0.3
        ux, uy = math.cos(a), math.sin(a)
        rod("Forged leg", (ux * 0.42, uy * 0.42, 0.02), (ux * 0.2, uy * 0.2, 1.0), 0.03, m["iron"], 12)
        rod("Leg scroll", (ux * 0.42, uy * 0.42, 0.02), (ux * 0.5, uy * 0.5, 0.06), 0.026, m["iron"], 10)
        cylinder("Leg foot", (ux * 0.5, uy * 0.5, 0.02), 0.045, 0.04, m["iron"], 14, bevel=0.008)
        b = a + math.tau / 3
        rod("Stretcher", (ux * 0.335, uy * 0.335, 0.4), (math.cos(b) * 0.335, math.sin(b) * 0.335, 0.4), 0.014, m["iron"], 8)
    torus("Leg ring", (0, 0, 0.72), 0.265, 0.014, m["iron"], 40, 8)
    lathe("Hammered bowl", [(0.05, 0.94), (0.2, 0.97), (0.33, 1.06), (0.4, 1.18), (0.41, 1.24), (0.385, 1.245), (0.37, 1.19), (0.3, 1.09),
                            (0.18, 1.02), (0.02, 1.0)], m["iron"], 56)
    torus("Rolled lip", (0, 0, 1.243), 0.398, 0.02, m["iron"], 56, 10)
    torus("Bowl band", (0, 0, 1.1), 0.355, 0.012, m["iron"], 48, 8)
    for i in range(10):
        a = math.tau * i / 10
        cylinder("Rim rivet", (math.cos(a) * 0.405, math.sin(a) * 0.405, 1.19), 0.014, 0.02, m["iron"], 8, bevel=0)
    lathe("Bed of coals", [(0.0, 1.1), (0.2, 1.11), (0.3, 1.13), (0.345, 1.16)], m["coal"], 32)
    for i in range(16):
        a = rng.uniform(0, math.tau)
        d = rng.uniform(0.0, 0.27)
        lump("Coal", (math.cos(a) * d, math.sin(a) * d, 1.13 + rng.uniform(0.0, 0.04)), rng.uniform(0.04, 0.075), m["coal"] if i % 3 else m["bark"],
             100 + i, squash=0.7, detail=1)
    return finish("brazier-tripod")


def campfire():
    clear()
    m = materials()
    stones = 11
    for i in range(stones):
        a = math.tau * i / stones + rng.uniform(-0.08, 0.08)
        d = 0.62 + rng.uniform(-0.03, 0.04)
        size = rng.uniform(0.13, 0.19)
        lump("Ring stone", (math.cos(a) * d, math.sin(a) * d, size * 0.5), size, m["stone"], 200 + i, squash=0.68)
    lathe("Ash and ember bed", [(0.0, 0.07), (0.25, 0.065), (0.42, 0.04), (0.5, 0.012)], m["coal"], 32)
    for i in range(14):
        a = rng.uniform(0, math.tau)
        d = rng.uniform(0.05, 0.4)
        lump("Ember", (math.cos(a) * d, math.sin(a) * d, 0.07 + rng.uniform(0.0, 0.03)), rng.uniform(0.035, 0.07), m["coal"] if i % 4 else m["bark"], 300 + i,
             squash=0.6, detail=1)
    # the logs leaning together over the bed, and two lying across its foot
    for i in range(6):
        a = math.tau * i / 6 + rng.uniform(-0.2, 0.2)
        foot = 0.44 + rng.uniform(-0.04, 0.04)
        top = Vector((math.cos(a + 2.6) * 0.07, math.sin(a + 2.6) * 0.07, 0.6 + rng.uniform(-0.05, 0.08)))
        r = rng.uniform(0.05, 0.068)
        rod("Leaning log", (math.cos(a) * foot, math.sin(a) * foot, 0.06), top, r, m["bark"], 12, r_end=r * 0.8, bevel=0.01)
    for a in (0.5, 2.3):
        rod("Lying log", (math.cos(a) * 0.4, math.sin(a) * 0.4, 0.085), (-math.cos(a) * 0.38, -math.sin(a) * 0.38, 0.1), 0.062, m["bark"], 12, r_end=0.052,
            bevel=0.01)
    return finish("campfire")


def build_all(builders):
    """Builds each prop and adds it to the manifest (the ones already there are kept)."""
    path = TMP / "manifest.json"
    manifest = json.loads(path.read_text(encoding="utf-8")) if path.exists() else {}
    for build in builders:
        info = build()
        manifest[info["key"]] = info
        print("built", info)
    path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Fire kit {VERSION}: {len(manifest)} models in {OUT}")


if __name__ == "__main__":
    build_all((candle, torch_stand, brazier_tripod, campfire))
