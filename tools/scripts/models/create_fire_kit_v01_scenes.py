"""Fire kit v01, second batch: the lanterns, the shrines and the forges that take the place of the 2D fire props (the burning
houses stay 2D cards: the user, 2026-11-12, "these houses are not going to work").

Headless: blender.exe -b -t 2 -P tools/scripts/models/create_fire_kit_v01_scenes.py
Uses create_fire_kit_v01.py's helpers, materials and manifest (build/models-tmp/fire/manifest.json, added to). None of these has a
flame of its own: the engine draws the fire as a volume at the FIRE points. Every lantern is an open iron cage round a candle, so
its flame truly stands inside it. Textures: the stash already in content/. Never an overwrite.
"""
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
import create_fire_kit_v01 as kit  # noqa: E402

TEX, ACG, rng = kit.TEX, kit.ACG, kit.rng
cylinder, rod, torus, lathe, lump, mat, clear, finish, soften = kit.cylinder, kit.rod, kit.torus, kit.lathe, kit.lump, kit.mat, kit.clear, kit.finish, kit.soften

kit.NAMES.update({
    "lantern-pillar": "Lanterna de cemitério 3D", "street-lamp": "Candeeiro de rua de gaiola 3D", "lantern-post-wood": "Poste de lanterna 3D",
    "lantern-post-low": "Lanterna em poste baixo 3D", "lantern-signpost": "Placa com lanterna 3D", "candle-menhir": "Menir das velas 3D",
    "incense-burner": "Incensário 3D", "candle-shrine": "Santuário de velas 3D", "campfire-cauldron": "Fogueira com caldeirão 3D",
    "hand-lantern": "Lanterna de mão 3D", "ice-lantern": "Lanterna caída no gelo 3D", "chandelier": "Candelabro 3D",
    "candle-sconce": "Arandela de vela 3D", "forge": "Forja 3D", "smithy": "Ferraria 3D",
})


def flame(at, height, radius, turbulence=1.0):
    return {"at": [round(at[0], 4), round(at[1], 4), round(at[2], 4)], "height": height, "radius": radius, "turbulence": turbulence}


def candle_flame(at, scale=1.0):
    return flame(at, 0.2 * scale, 0.04 * scale, 0.3)


def cube(name, loc, dims, material, rot=(0, 0, 0), bevel=0.02):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc, rotation=rot)
    o = bpy.context.object
    o.name = name
    o.scale = dims
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    o.data.materials.append(material)
    return soften(o, bevel, 1)


def prism(name, verts, faces, material):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.materials.append(material)
    mesh.update()
    o = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(o)
    return o


def materials():
    m = kit.materials()
    m.update({
        "masonry": mat("Grey masonry", TEX / "masonry-grey.jpg", 0.0, 0.9),
        "mossy": mat("Mossy masonry", TEX / "masonry-mossy.jpg", 0.0, 0.92),
        "grave": mat("Weathered grave stone", TEX / "wt-gravestone.jpg", 0.0, 0.9),
        "plaster": mat("Smoke-stained plaster", TEX / "wt-limestone.jpg", 0.0, 0.9, tint=(0.86, 0.8, 0.7)),
        "timber": mat("Dark timber", TEX / "farmhouse-dark-wood-v02-color.jpg", 0.0, 0.85),
        "charred": mat("Charred timber", TEX / "farmhouse-dark-wood-v02-color.jpg", 0.0, 0.95, tint=(0.2, 0.18, 0.17)),
        "shingle": mat("Old shingles", TEX / "planks-dark.jpg", 0.0, 0.9, tint=(0.62, 0.6, 0.6)),
        "planks": mat("Pale planks", TEX / "planks-pale.jpg", 0.0, 0.85),
        "brass": mat("Tarnished brass", TEX / "brass-detail-v01.jpg", 0.7, 0.5),
        "ice": mat("Rime ice", TEX / "snow.jpg", 0.0, 0.35),
        "rubble": mat("Stone rubble", TEX / "stone-rubble.jpg", 0.0, 0.92),
    })
    return m


def candle_at(m, x, y, z, scale=1.0, dish=True):
    """A wax candle standing at (x, y, z); returns where its flame stands."""
    h = 0.2 * scale
    if dish:
        cylinder("Candle dish", (x, y, z + 0.008 * scale), 0.07 * scale, 0.016 * scale, m["iron"], 16, bevel=0)
    cylinder("Wax candle", (x, y, z + 0.016 * scale + h / 2), 0.036 * scale, h, m["wax"], 14, r_top=0.032 * scale, bevel=0)
    return (x, y, z + 0.016 * scale + h + 0.01 * scale)


def cage(m, x, y, z, w, h, metal="iron"):
    """An open lantern: a floor, four corner bars, a rail round the top, a peaked cap with its ring, a candle inside. (x, y, z) is the
    middle of its floor; returns the candle's flame point."""
    iron = m[metal]
    cube("Lantern floor", (x, y, z), (w + 0.04, w + 0.04, 0.03), iron, bevel=0.006)
    for sx in (-1, 1):
        for sy in (-1, 1):
            rod("Lantern bar", (x + sx * w / 2, y + sy * w / 2, z), (x + sx * w / 2, y + sy * w / 2, z + h), 0.012, iron, 6, bevel=0)
    for s in (-1, 1):
        rod("Lantern rail", (x - w / 2, y + s * w / 2, z + h), (x + w / 2, y + s * w / 2, z + h), 0.011, iron, 6, bevel=0)
        rod("Lantern rail", (x + s * w / 2, y - w / 2, z + h), (x + s * w / 2, y + w / 2, z + h), 0.011, iron, 6, bevel=0)
        rod("Lantern guard", (x - w / 2, y + s * w / 2, z + h * 0.3), (x + w / 2, y + s * w / 2, z + h * 0.3), 0.008, iron, 6, bevel=0)
        rod("Lantern guard", (x + s * w / 2, y - w / 2, z + h * 0.3), (x + s * w / 2, y + w / 2, z + h * 0.3), 0.008, iron, 6, bevel=0)
    bpy.ops.mesh.primitive_cone_add(vertices=4, radius1=(w + 0.1) * 0.7071, radius2=0.03, depth=h * 0.3, location=(x, y, z + h + h * 0.15),
                                    rotation=(0, 0, math.pi / 4))
    cap = bpy.context.object
    cap.name = "Lantern cap"
    cap.data.materials.append(iron)
    torus("Lantern ring", (x, y, z + h * 1.3 + 0.03), 0.035, 0.008, iron, 16, 6).rotation_euler = (math.pi / 2, 0, 0)
    return candle_at(m, x, y, z + 0.015, min(1.0, h / 0.42))


def post_lantern(key, post, height, arm_reach, cage_w, cage_h, cage_z, signs=0, pillar=False):
    clear()
    m = materials()
    top = height
    if pillar:
        cube("Stepped plinth", (0, 0, 0.11), (0.7, 0.7, 0.22), m["grave"], bevel=0.03)
        cube("Pillar", (0, 0, 0.22 + (top - 0.55) / 2), (0.44, 0.44, top - 0.55), m["grave"], bevel=0.03)
        cube("Pillar band", (0, 0, top - 0.5), (0.5, 0.5, 0.1), m["grave"], bevel=0.02)
        bpy.ops.mesh.primitive_cone_add(vertices=4, radius1=0.34, radius2=0.02, depth=0.34, location=(0, 0, top - 0.17), rotation=(0, 0, math.pi / 4))
        bpy.context.object.data.materials.append(m["grave"])
        cube("Iron strap", (0, 0, top - 0.82), (0.47, 0.47, 0.07), m["iron"], bevel=0.008)
        arm_z = top - 0.78
        start = (0.22, 0, arm_z)
    else:
        material = m[post]
        r = 0.05 if post == "iron" else 0.075
        if post == "iron":
            cylinder("Cast foot", (0, 0, 0.14), 0.17, 0.28, m["iron"], 20, r_top=0.09)
            torus("Foot ring", (0, 0, 0.3), 0.09, 0.02, m["iron"], 20, 8)
            torus("Shaft ring", (0, 0, top * 0.55), r + 0.008, 0.016, m["iron"], 20, 8)
        else:
            pass  # (no stones at a wooden post's foot: the user, 2026-11-12)
        rod("Post", (0, 0, 0), (0.0, 0.0, top), r, material, 12, r_end=r * 0.82)
        arm_z = top - 0.12
        start = (0, 0, arm_z)
    # the arm: out and up, then curling down to the hook the lantern hangs from
    hook = (arm_reach, 0, arm_z + 0.02)
    mid = (arm_reach * 0.55, 0, arm_z + 0.2)
    rod("Arm", start, mid, 0.022, m["iron"], 8)
    rod("Arm", mid, hook, 0.02, m["iron"], 8)
    if not pillar:
        rod("Arm brace", (0, 0, arm_z - 0.3), (arm_reach * 0.5, 0, arm_z + 0.16), 0.014, m["iron"], 8)
    cap_top = cage_z + cage_h * 1.3 + 0.06
    rod("Hanger", hook, (arm_reach, 0, cap_top), 0.008, m["iron"], 6, bevel=0)
    at = cage(m, arm_reach, 0, cage_z, cage_w, cage_h)
    for i in range(signs):
        z = top - 0.75 - i * 0.34
        side = 1 if i % 2 == 0 else -1
        cube("Sign board", (-side * 0.36, 0.02 * side, z), (0.86, 0.045, 0.2), m["planks"], rot=(0, side * 0.07, 0.06 * side), bevel=0.01)
    kit.FIRE[key] = [candle_flame(at, min(1.0, cage_h / 0.42) * 1.25)]
    return finish(key)


def lantern_pillar():
    return post_lantern("lantern-pillar", None, 2.5, 0.72, 0.34, 0.46, 1.02, pillar=True)


def street_lamp():
    return post_lantern("street-lamp", "iron", 3.86, 0.6, 0.4, 0.54, 2.9)


def lantern_post_wood():
    return post_lantern("lantern-post-wood", "oak", 3.86, 0.55, 0.36, 0.48, 2.95)


def lantern_post_low():
    return post_lantern("lantern-post-low", "oak", 1.86, 0.42, 0.3, 0.4, 1.12)


def lantern_signpost():
    return post_lantern("lantern-signpost", "oak", 3.9, 0.46, 0.24, 0.32, 2.05, signs=3)


def candle_menhir():
    clear()
    m = materials()
    stone = lump("Standing stone", (0, 0.12, 1.25), 0.62, m["grave"], 41, squash=2.15, detail=4, rough=0.14)
    stone.rotation_euler = (0.05, -0.04, 0.3)
    for i in range(7):
        a = math.tau * i / 7 + 0.2
        d = 0.75 + rng.uniform(-0.1, 0.15)
        lump("Foot stone", (math.cos(a) * d, math.sin(a) * d * 0.8 + 0.1, 0.1), rng.uniform(0.16, 0.28), m["mossy"], 600 + i, squash=0.55, detail=2)
    cube("Offering slab", (0, -0.62, 0.09), (0.7, 0.4, 0.14), m["grave"], rot=(0, 0, 0.1), bevel=0.03)
    torus("Carved wheel", (0, -0.47, 1.7), 0.17, 0.02, m["iron"], 24, 6).rotation_euler = (math.pi / 2 - 0.12, 0, 0)
    fires = []
    for x, y, s in ((-0.16, -0.62, 1.5), (0.02, -0.7, 1.0), (0.2, -0.58, 1.25)):
        fires.append(candle_flame(candle_at(m, x, y, 0.16, s, dish=False), s))
    kit.FIRE["candle-menhir"] = fires
    return finish("candle-menhir")


def incense_burner():
    clear()
    m = materials()
    cube("Cracked flagstone", (0, 0, 0.05), (1.2, 1.1, 0.1), m["mossy"], rot=(0, 0, 0.2), bevel=0.03)
    for i in range(3):
        a = math.tau * i / 3 + 0.5
        ux, uy = math.cos(a), math.sin(a)
        lathe_leg = [(0.1, 0.1), (0.13, 0.16), (0.09, 0.3), (0.12, 0.52), (0.15, 0.66), (0.1, 0.72)]
        leg = lathe("Lion leg", lathe_leg, m["mossy"], 16)
        leg.location = (ux * 0.36, uy * 0.36, 0)
    lathe("Bronze bowl", [(0.12, 0.7), (0.4, 0.78), (0.56, 0.98), (0.58, 1.12), (0.52, 1.18), (0.5, 1.12), (0.46, 0.98), (0.34, 0.86), (0.02, 0.84)],
          m["brass"], 48)
    torus("Bowl rim", (0, 0, 1.17), 0.54, 0.03, m["brass"], 48, 8)
    for a in (0.0, math.pi):
        torus("Ring handle", (math.cos(a) * 0.63, math.sin(a) * 0.63, 1.0), 0.09, 0.016, m["iron"], 16, 6).rotation_euler = (math.pi / 2, 0, a + math.pi / 2)
    lathe("Bed of coals", [(0.0, 0.92), (0.3, 0.94), (0.44, 1.0)], m["coal"], 24)
    # the lid: open ribs gathered into a finial, so the fire inside shows through
    for i in range(10):
        a = math.tau * i / 10
        ux, uy = math.cos(a), math.sin(a)
        rod("Lid rib", (ux * 0.5, uy * 0.5, 1.2), (ux * 0.38, uy * 0.38, 1.46), 0.016, m["brass"], 6, bevel=0)
        rod("Lid rib", (ux * 0.38, uy * 0.38, 1.46), (ux * 0.1, uy * 0.1, 1.62), 0.014, m["brass"], 6, bevel=0)
    torus("Lid hoop", (0, 0, 1.46), 0.38, 0.016, m["brass"], 32, 6)
    lathe("Finial", [(0.1, 1.6), (0.14, 1.66), (0.06, 1.72), (0.1, 1.8), (0.03, 1.88), (0.0, 1.96)], m["brass"], 16)
    kit.FIRE["incense-burner"] = [flame((0, 0, 0.98), 0.5, 0.26, 0.8)]
    return finish("incense-burner")


def candle_shrine():
    clear()
    m = materials()
    cube("Plinth", (0, 0, 0.09), (1.0, 0.7, 0.18), m["mossy"], bevel=0.03)
    cube("Step", (0, 0.04, 0.24), (0.84, 0.56, 0.12), m["grave"], bevel=0.02)
    cube("Back slab", (0, 0.24, 1.0), (0.72, 0.12, 1.4), m["grave"], bevel=0.02)
    for sx in (-1, 1):
        cylinder("Column", (sx * 0.36, 0.08, 0.86), 0.06, 1.12, m["grave"], 12)
        cube("Column foot", (sx * 0.36, 0.08, 0.33), (0.16, 0.16, 0.07), m["grave"], bevel=0.01)
        cube("Column head", (sx * 0.36, 0.08, 1.44), (0.16, 0.16, 0.07), m["grave"], bevel=0.01)
        cube("Arch stone", (sx * 0.2, 0.08, 1.66), (0.46, 0.2, 0.1), m["grave"], rot=(0, -sx * 0.9, 0), bevel=0.015)
    lump("Arch finial", (0, 0.08, 1.9), 0.07, m["grave"], 71, squash=1.5, detail=2)
    # the figure in the niche: robe, shoulders, bowed head
    cylinder("Robe", (0, 0.14, 0.72), 0.17, 0.84, m["mossy"], 14, r_top=0.1)
    lump("Shoulders", (0, 0.14, 1.12), 0.15, m["mossy"], 72, squash=0.7, detail=2)
    lump("Hooded head", (0, 0.12, 1.3), 0.095, m["mossy"], 73, squash=1.15, detail=2)
    fires = []
    for x, y, s in ((-0.22, -0.14, 1.0), (-0.07, -0.2, 0.7), (0.08, -0.17, 1.15), (0.22, -0.12, 0.8)):
        fires.append(candle_flame(candle_at(m, x, y, 0.3, s, dish=False), s))
    kit.FIRE["candle-shrine"] = fires
    return finish("candle-shrine")


def campfire_cauldron():
    clear()
    m = materials()
    for i in range(10):
        a = math.tau * i / 10 + rng.uniform(-0.08, 0.08)
        size = rng.uniform(0.11, 0.16)
        lump("Ring stone", (math.cos(a) * 0.5, math.sin(a) * 0.5, size * 0.5), size, m["stone"], 700 + i, squash=0.68)
    lathe("Ember bed", [(0.0, 0.06), (0.22, 0.055), (0.4, 0.015)], m["coal"], 24)
    for i in range(4):
        a = math.tau * i / 4 + 0.3
        rod("Burning log", (math.cos(a) * 0.36, math.sin(a) * 0.36, 0.06), (-math.cos(a) * 0.1, -math.sin(a) * 0.1, 0.2), 0.05, m["bark"], 10, r_end=0.04, bevel=0.008)
    apex = Vector((0, 0, 1.86))
    for i in range(3):
        a = math.tau * i / 3 + 0.9
        rod("Tripod pole", (math.cos(a) * 0.82, math.sin(a) * 0.82, 0.0), apex + Vector((-math.cos(a) * 0.06, -math.sin(a) * 0.06, 0.1)), 0.03, m["bark"], 8,
            r_end=0.022, bevel=0)
    torus("Lashing", (0, 0, 1.84), 0.05, 0.016, m["pitch"], 14, 6)
    rod("Pot chain", (0, 0, 1.82), (0, 0, 1.06), 0.01, m["iron"], 6, bevel=0)
    torus("Bail", (0, 0, 0.95), 0.2, 0.012, m["iron"], 24, 6).rotation_euler = (math.pi / 2, 0, 0)
    lathe("Cauldron", [(0.02, 0.56), (0.16, 0.58), (0.25, 0.68), (0.27, 0.82), (0.23, 0.93), (0.25, 0.96), (0.22, 0.97), (0.2, 0.92), (0.23, 0.82),
                       (0.21, 0.7), (0.14, 0.62), (0.01, 0.6)], m["iron"], 32)
    kit.FIRE["campfire-cauldron"] = [flame((0, 0, 0.1), 0.48, 0.2, 1.0), flame((0.16, 0.1, 0.08), 0.34, 0.14, 1.0)]
    return finish("campfire-cauldron")


def hand_lantern():
    clear()
    m = materials()
    lathe("Oil tank", [(0.0, 0.0), (0.2, 0.0), (0.23, 0.04), (0.21, 0.14), (0.12, 0.2), (0.1, 0.24)], m["iron"], 28)
    torus("Tank band", (0, 0, 0.04), 0.225, 0.012, m["iron"], 28, 6)
    cylinder("Burner", (0, 0, 0.27), 0.08, 0.06, m["brass"], 16)
    for i in range(6):
        a = math.tau * i / 6
        ux, uy = math.cos(a), math.sin(a)
        rod("Guard wire", (ux * 0.1, uy * 0.1, 0.3), (ux * 0.19, uy * 0.19, 0.5), 0.008, m["iron"], 6, bevel=0)
        rod("Guard wire", (ux * 0.19, uy * 0.19, 0.5), (ux * 0.1, uy * 0.1, 0.72), 0.008, m["iron"], 6, bevel=0)
    torus("Guard hoop", (0, 0, 0.5), 0.19, 0.009, m["iron"], 24, 6)
    lathe("Chimney cap", [(0.1, 0.72), (0.12, 0.76), (0.07, 0.84), (0.09, 0.88), (0.03, 0.92)], m["iron"], 20)
    for s in (-1, 1):
        rod("Frame", (s * 0.22, 0, 0.12), (s * 0.24, 0, 0.7), 0.012, m["iron"], 6, bevel=0)
        rod("Frame", (s * 0.24, 0, 0.7), (s * 0.1, 0, 0.86), 0.012, m["iron"], 6, bevel=0)
    torus("Carry ring", (0, 0, 1.02), 0.1, 0.012, m["iron"], 20, 6).rotation_euler = (math.pi / 2, 0, 0)
    rod("Wick", (0, 0, 0.3), (0, 0, 0.33), 0.012, m["pitch"], 6, bevel=0)
    kit.FIRE["hand-lantern"] = [flame((0, 0, 0.33), 0.3, 0.07, 0.4)]
    return finish("hand-lantern")


def ice_lantern():
    clear()
    m = materials()
    at = cage(m, 0, 0, 0.12, 0.34, 0.44)
    # (no ice or snow on it: the user, 2026-11-12; the bare cage, standing)
    kit.FIRE["ice-lantern"] = [candle_flame(at, 1.2)]
    return finish("ice-lantern")


def chandelier():
    clear()
    m = materials()
    ring_z = 1.3
    torus("Wheel", (0, 0, ring_z), 0.62, 0.035, m["iron"], 48, 8)
    torus("Inner wheel", (0, 0, ring_z - 0.12), 0.3, 0.022, m["iron"], 32, 6)
    fires = []
    for i in range(6):
        a = math.tau * i / 6
        ux, uy = math.cos(a), math.sin(a)
        rod("Spoke", (ux * 0.3, uy * 0.3, ring_z - 0.12), (ux * 0.62, uy * 0.62, ring_z), 0.014, m["iron"], 6, bevel=0)
        fires.append(candle_flame(candle_at(m, ux * 0.62, uy * 0.62, ring_z + 0.03, 1.0), 1.1))
        if i % 2 == 0:
            rod("Chain", (ux * 0.62, uy * 0.62, ring_z + 0.03), (0, 0, 2.3), 0.012, m["iron"], 6, bevel=0)
    rod("Chain", (0, 0, 2.3), (0, 0, 2.7), 0.014, m["iron"], 6, bevel=0)
    lathe("Drop finial", [(0.0, ring_z - 0.42), (0.06, ring_z - 0.34), (0.1, ring_z - 0.2), (0.04, ring_z - 0.12)], m["iron"], 16)
    kit.FIRE["chandelier"] = fires
    return finish("chandelier")


def candle_sconce():
    clear()
    m = materials()
    z = 1.5
    cube("Back plate", (0, 0.2, z), (0.26, 0.04, 0.9), m["iron"], bevel=0.015)
    for dz in (-0.36, 0.36):
        cylinder("Plate boss", (0, 0.17, z + dz), 0.035, 0.03, m["iron"], 10, bevel=0).rotation_euler = (math.pi / 2, 0, 0)
    rod("Arm", (0, 0.18, z - 0.2), (0, -0.05, z - 0.3), 0.03, m["iron"], 8)
    rod("Arm", (0, -0.05, z - 0.3), (0, -0.3, z - 0.12), 0.028, m["iron"], 8)
    lathe_pan = [(0.03, z - 0.14), (0.2, z - 0.1), (0.24, z - 0.04), (0.22, z - 0.03), (0.18, z - 0.07), (0.02, z - 0.08)]
    pan = lathe("Drip pan", lathe_pan, m["iron"], 28)
    pan.location = (0, -0.3, 0)
    cylinder("Fat candle", (0, -0.3, z + 0.12), 0.09, 0.4, m["wax"], 18, r_top=0.082, bevel=0.01)
    kit.FIRE["candle-sconce"] = [flame((0, -0.3, z + 0.33), 0.32, 0.07, 0.35)]
    return finish("candle-sconce")


def anvil(m, x, y):
    cylinder("Oak stump", (x, y, 0.3), 0.3, 0.6, m["bark"], 14, bevel=0.02)
    torus("Stump band", (x, y, 0.45), 0.3, 0.02, m["iron"], 20, 6)
    cube("Anvil foot", (x, y, 0.66), (0.36, 0.24, 0.12), m["iron"], bevel=0.02)
    cube("Anvil waist", (x, y, 0.78), (0.2, 0.16, 0.14), m["iron"], bevel=0.02)
    cube("Anvil face", (x, y, 0.91), (0.5, 0.2, 0.13), m["iron"], bevel=0.02)
    rod("Anvil horn", (x + 0.25, y, 0.92), (x + 0.52, y, 0.94), 0.07, m["iron"], 10, r_end=0.012, bevel=0)


def forge_block(m, x, y, w, d, h):
    """A stone forge hearth with a bed of coals; returns its flames."""
    cube("Forge hearth", (x, y, h / 2), (w, d, h), m["masonry"], bevel=0.04)
    cube("Hearth kerb", (x, y - d / 2 + 0.08, h + 0.07), (w, 0.16, 0.14), m["masonry"], bevel=0.02)
    cube("Hearth kerb", (x, y + d / 2 - 0.08, h + 0.07), (w, 0.16, 0.14), m["masonry"], bevel=0.02)
    for s in (-1, 1):
        cube("Hearth kerb", (x + s * (w / 2 - 0.08), y, h + 0.07), (0.16, d - 0.3, 0.14), m["masonry"], bevel=0.02)
    cube("Coal bed", (x, y, h + 0.05), (w - 0.34, d - 0.34, 0.08), m["coal"], bevel=0.02)
    for i in range(12):
        lump("Coal", (x + rng.uniform(-w * 0.3, w * 0.3), y + rng.uniform(-d * 0.3, d * 0.3), h + 0.11), rng.uniform(0.05, 0.09), m["coal"] if i % 3 else m["bark"],
             950 + i, squash=0.7, detail=1)
    return [flame((x, y, h + 0.1), 0.7, 0.3, 1.0), flame((x - w * 0.2, y + 0.05, h + 0.1), 0.45, 0.2, 1.0)]


def forge():
    clear()
    m = materials()
    fires = forge_block(m, 0, 0.3, 1.8, 1.4, 0.9)
    for sx in (-1, 1):
        for sy in (-1, 1):
            rod("Hood post", (sx * 0.84, 0.3 + sy * 0.64, 0.9), (sx * 0.84, 0.3 + sy * 0.64, 2.3), 0.035, m["iron"], 8)
    bpy.ops.mesh.primitive_cone_add(vertices=4, radius1=1.5, radius2=0.3, depth=1.1, location=(0, 0.3, 2.85), rotation=(0, 0, math.pi / 4))
    hood = bpy.context.object
    hood.name = "Iron hood"
    hood.data.materials.append(m["iron"])
    cube("Hood flue", (0, 0.3, 3.6), (0.5, 0.5, 0.5), m["iron"], bevel=0.02)
    cube("Hood rim", (0, 0.3, 2.3), (2.14, 2.14, 0.08), m["iron"], bevel=0.01)
    anvil(m, -1.6, -0.6)
    cube("Bench top", (1.55, -0.7, 0.72), (1.1, 0.6, 0.1), m["planks"], bevel=0.02)
    for sx in (-1, 1):
        for sy in (-1, 1):
            cube("Bench leg", (1.55 + sx * 0.46, -0.7 + sy * 0.22, 0.34), (0.1, 0.1, 0.68), m["timber"], bevel=0.01)
    rod("Hammer haft", (1.3, -0.75, 0.8), (1.75, -0.6, 0.8), 0.02, m["oak"], 8)
    cube("Hammer head", (1.78, -0.59, 0.8), (0.1, 0.2, 0.1), m["iron"], rot=(0, 0, 0.32), bevel=0.01)
    kit.FIRE["forge"] = fires
    return finish("forge")


def smithy():
    clear()
    m = materials()
    fires = forge_block(m, 0.2, 0.6, 2.0, 1.3, 0.85)
    cube("Forge back wall", (0.2, 1.2, 1.5), (2.0, 0.3, 1.4), m["masonry"], bevel=0.03)
    cube("Chimney", (0.2, 1.2, 2.9), (0.7, 0.5, 1.5), m["masonry"], bevel=0.03)
    cube("Chimney cap", (0.2, 1.2, 3.7), (0.86, 0.66, 0.12), m["masonry"], bevel=0.02)
    for x, y in ((-2.3, -1.1), (-2.3, 1.3), (-0.9, -1.1)):
        rod("Canopy pole", (x, y, 0), (x, y, 2.5), 0.06, m["bark"], 8)
    cube("Canopy", (-1.5, 0.1, 2.56), (2.0, 2.7, 0.06), m["pitch"], rot=(0, 0.1, 0), bevel=0.01)
    anvil(m, -1.5, -0.3)
    lathe("Quench barrel", [(0.0, 0.0), (0.3, 0.0), (0.36, 0.4), (0.3, 0.8), (0.26, 0.8), (0.3, 0.42), (0.26, 0.06), (0.0, 0.06)], m["oak"], 20).location = (-2.1, 0.7, 0)
    torus("Barrel hoop", (-2.1, 0.7, 0.2), 0.34, 0.016, m["iron"], 24, 6)
    torus("Barrel hoop", (-2.1, 0.7, 0.62), 0.34, 0.016, m["iron"], 24, 6)
    cylinder("Grindstone", (1.9, -0.6, 0.62), 0.4, 0.12, m["stone"], 24, bevel=0.02).rotation_euler = (math.pi / 2, 0, 0)
    for s in (-1, 1):
        cube("Grindstone frame", (1.9, -0.6 + s * 0.16, 0.32), (0.9, 0.08, 0.64), m["timber"], bevel=0.01)
    cube("Tool rack", (2.3, 1.0, 0.9), (0.08, 1.2, 1.8), m["timber"], bevel=0.02)
    for i in range(4):
        rod("Tongs", (2.24, 0.55 + i * 0.3, 1.6), (2.24, 0.6 + i * 0.3, 0.8), 0.015, m["iron"], 6, bevel=0)
    kit.FIRE["smithy"] = fires
    return finish("smithy")


if __name__ == "__main__":
    builders = {"lantern-pillar": lantern_pillar, "street-lamp": street_lamp, "lantern-post-wood": lantern_post_wood, "lantern-post-low": lantern_post_low,
                "lantern-signpost": lantern_signpost, "candle-menhir": candle_menhir, "incense-burner": incense_burner, "candle-shrine": candle_shrine,
                "campfire-cauldron": campfire_cauldron, "hand-lantern": hand_lantern, "ice-lantern": ice_lantern, "chandelier": chandelier,
                "candle-sconce": candle_sconce, "forge": forge,
                "smithy": smithy}
    # only the ones not built yet (a model is never overwritten: to rebuild one, remove its .glb first)
    kit.build_all([f for key, f in builders.items() if not (kit.OUT / f"{key}-{kit.VERSION}.glb").exists()])
