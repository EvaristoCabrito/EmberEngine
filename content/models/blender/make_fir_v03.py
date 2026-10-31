"""The Wisp Forest's bare fir v03 (the user, 2026-10-30: "vamos customizar para não ficar igual de um arquivo público", "criando uma
nova textura, deixando elas um pouco mais baixas", "sem criar novos galhos ou geometrias tortas"): v02 (the trunk unwrapped,
fix_fir_trunk_uv.py) with the Poly Haven bark replaced by the user's own (Attachments/Pine.jpg: its fibres straightened by a 0.2 shear,
made seamless with tools/scripts/ground/make_tile.py --size 784, its maps by tools/scripts/models/bark_maps.py), on the trunk and the
dead branches alike. Geometry untouched but the base (below); the lower height is the catalogue's (models.json "height").
v04 (the user, same evening: the Pine.jpg bark "ainda está uma bosta", then "procurar outras texturas que já existem", "A 5 (Bosque
Morto)"): the same with the Bosque Morto's dead bark (content/models/textures/wisp-dead-bark-v01: seamless, its own normal map, the
ARM from bark_maps.py).

blender -b -P content/models/blender/make_fir_v03.py -- v03|v04|v05   (v04 when omitted)
"""
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "polyhaven-wisp-v02")
VERSION = (sys.argv[sys.argv.index("--") + 1:] or ["v04"])[0] if "--" in sys.argv else "v04"
BARKS = {
    "v03": (os.path.join(HERE, "..", "..", "textures", "bark"), "pine-bark-v01.png", "pine-bark-nor-v01.png", "pine-bark-arm-v01.png"),
    "v04": (os.path.join(HERE, "..", "textures"), "wisp-dead-bark-v01.jpg", "wisp-dead-bark-v01-normal.png", "wisp-dead-bark-v01-arm.png"),
}
BARKS["v05"] = BARKS["v04"]  # v05: v04's bark, the smooth root flare (below)
BARK, DIFF, NOR, ARM = BARKS[VERSION]
SRC = os.path.join(ROOT, "fir_tree_01_bare_v02.glb")
OUT = os.path.join(ROOT, f"fir_tree_01_bare_{VERSION}.glb")
SWAP = {  # the Poly Haven bark image -> ours
    "fir_tree_01_bark_diff_2k": (DIFF, "sRGB"),
    "fir_tree_01_bark_nor_gl_2k": (NOR, "Non-Color"),
    "fir_tree_01_bark_arm_2k": (ARM, "Non-Color"),
}

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=SRC)

loaded = {}
swapped = 0
for mat in bpy.data.materials:
    if not mat.node_tree:
        continue
    for node in mat.node_tree.nodes:
        if node.type != "TEX_IMAGE" or not node.image:
            continue
        key = next((k for k in SWAP if node.image.name.startswith(k)), None)
        if not key:
            continue
        if key not in loaded:
            file, space = SWAP[key]
            img = bpy.data.images.load(os.path.join(BARK, file))
            img.colorspace_settings.name = space
            img.name = os.path.splitext(file)[0]
            loaded[key] = img
        node.image = loaded[key]
        swapped += 1
    print(f"make_fir_v03: {mat.name}")


# The base (the user, same day: "antes precisa ajustar a base dessas pines, não ficam certas no chão reto"): the root flare's lowest
# point is one root at -0.09 while the others end up to 0.26 above it, so on flat ground most roots hang in the air. Each direction
# round the trunk is lowered until its own lowest root vertex reaches the plane of the lowest one, the shift fading to nothing at
# BASE_TOP, so the trunk and every vertex above it stay where they were (no new geometry).
import math  # noqa: E402

BASE_TOP = 0.45   # model units above the lowest vertex: the flare's height
SECTORS = 48
for obj in [o for o in bpy.data.objects if o.type == "MESH"]:
    me = obj.data
    trunk = {i for i, m in enumerate(me.materials) if m and m.name.startswith("fir_tree_01_trunk")}
    if not trunk:
        continue
    vids = sorted({v for p in me.polygons if p.material_index in trunk for v in p.vertices})
    zs = [me.vertices[i].co.z for i in vids]
    low = min(zs)
    ring = [i for i in vids if me.vertices[i].co.z > low + BASE_TOP * 1.2 and me.vertices[i].co.z < low + BASE_TOP * 2.0]
    cx = sum(me.vertices[i].co.x for i in ring) / len(ring)
    cy = sum(me.vertices[i].co.y for i in ring) / len(ring)
    base = [i for i in vids if me.vertices[i].co.z < low + BASE_TOP]
    angle = lambda i: math.atan2(me.vertices[i].co.y - cy, me.vertices[i].co.x - cx)
    lowest = [math.inf] * SECTORS
    for i in base:
        s = int((angle(i) + math.pi) / (2 * math.pi) * SECTORS) % SECTORS
        lowest[s] = min(lowest[s], me.vertices[i].co.z)
    for s in range(SECTORS):  # a sector with no root takes its nearest neighbours'
        if lowest[s] == math.inf:
            near = [lowest[(s + d) % SECTORS] for d in range(-SECTORS // 2, SECTORS // 2) if lowest[(s + d) % SECTORS] != math.inf]
            lowest[s] = min(near) if near else low
    drop = [max(z - low, 0.0) for z in lowest]
    moved = 0
    for i in base:
        a = (angle(i) + math.pi) / (2 * math.pi) * SECTORS - 0.5
        s0 = math.floor(a)
        t = a - s0
        d = drop[s0 % SECTORS] * (1 - t) + drop[(s0 + 1) % SECTORS] * t  # smooth round the trunk: no tearing between sectors
        z = me.vertices[i].co.z
        w = min(1.0, max(0.0, 1.0 - (z - (low + d)) / max(BASE_TOP - d, 1e-3)))  # 1 at this direction's lowest root, 0 at BASE_TOP
        if d * w > 0:
            me.vertices[i].co.z = z - d * w
            moved += 1
    # The trunk's UVs again (v02's followed a centre per height band, which the roots pulled sideways: a step in the bark where the
    # trunk meets them): round one straight axis through the trunk above the flare; faces turned up or down (the tops of the roots)
    # take the bark flat from above instead (round the axis it swirled into a pinwheel).
    import bmesh  # noqa: E402

    bm = bmesh.new()
    bm.from_mesh(me)
    bm.normal_update()
    uv = bm.loops.layers.uv.active
    radius = sorted(math.hypot(me.vertices[i].co.x - cx, me.vertices[i].co.y - cy) for i in ring)[len(ring) // 2]
    around = 2.0
    vscale = around / (2.0 * math.pi * max(radius, 1e-3))
    flat = 0
    for f in bm.faces:
        if f.material_index not in trunk:
            continue
        if abs(f.normal.z) > 0.6:
            flat += 1
            for loop in f.loops:
                loop[uv].uv = ((loop.vert.co.x - cx) * vscale, (loop.vert.co.y - cy) * vscale)
            continue
        us = [math.atan2(l.vert.co.y - cy, l.vert.co.x - cx) / (2.0 * math.pi) * around for l in f.loops]
        for k, loop in enumerate(f.loops):
            u = us[k]
            while u - us[0] > around / 2:
                u -= around
            while us[0] - u > around / 2:
                u += around
            loop[uv].uv = (u, (loop.vert.co.z - low) * vscale)
    bm.to_mesh(me)
    bm.free()
    me.update()
    print(f"make_fir_v03: base of {obj.name}: {moved} vertices lowered, largest drop {max(drop):.3f}, lowest at {low:.3f}; "
          f"UVs round the axis ({cx:.3f}, {cy:.3f}), radius {radius:.3f}, {flat} faces from above")

    if VERSION >= "v05":
        # v05 (the user: "consegue suavizar essas raízes? está muito feio assim", with a drawing of a trunk swelling into a few thick
        # roots that dive into the ground): the flat, ragged root splat goes; in its place one smooth swelling of the trunk, a surface
        # of revolution whose radius grows towards the ground and bulges into ROOTS rounded lobes, its bottom ring just under the
        # ground. It starts inside the trunk (FLARE_TOP, a little under the trunk's radius) and leaves it as it widens, so the old
        # trunk's cut edge (CUT) is inside it; the bark is mapped as on the trunk (same axis, same scale).
        CUT, FLARE_TOP, FLARE_BOTTOM = 0.40, 1.0, -0.06  # heights above the lowest point
        AROUND_N, ROWS = 96, 28
        ROOTS = [(0.0, 1.00), (1.35, 0.80), (2.45, 0.95), (3.75, 0.70), (4.95, 0.90)]  # (angle, strength): five uneven roots
        bm = bmesh.new()
        bm.from_mesh(me)
        uv = bm.loops.layers.uv.active
        doomed = [f for f in bm.faces if f.material_index in trunk and f.calc_center_median().z < low + CUT]
        bmesh.ops.delete(bm, geom=doomed, context="FACES")
        mat = min(trunk)

        def lobe(theta):
            s = 0.0
            for a, k in ROOTS:
                d = math.atan2(math.sin(theta - a), math.cos(theta - a))
                s += k * math.exp(-(d / 0.36) ** 2)  # each root about 40 degrees wide
            return s

        rows = []
        for r_i in range(ROWS + 1):
            t = r_i / ROWS  # 0 at the top, 1 at the ground
            z = low + FLARE_TOP + (FLARE_BOTTOM - FLARE_TOP) * t
            ring_v = []
            for a_i in range(AROUND_N):
                theta = 2.0 * math.pi * a_i / AROUND_N
                r = radius * (0.85 + 1.9 * t * t) + 0.45 * t ** 3 * lobe(theta)
                ring_v.append(bm.verts.new((cx + r * math.cos(theta), cy + r * math.sin(theta), z)))
            rows.append(ring_v)
        new_faces = []
        for r_i in range(ROWS):
            for a_i in range(AROUND_N):
                b = (a_i + 1) % AROUND_N
                quad = (rows[r_i][a_i], rows[r_i + 1][a_i], rows[r_i + 1][b], rows[r_i][b])
                f = bm.faces.new(quad)
                f.material_index = mat
                f.smooth = True
                for k, loop in enumerate(f.loops):
                    col = a_i if k in (0, 1) else a_i + 1
                    loop[uv].uv = (col / AROUND_N * around, (loop.vert.co.z - low) * vscale)
                new_faces.append(f)
        made = len(new_faces)
        bm.normal_update()
        # Faces must point outward: the quad order above winds them so; flip them all if the first one looks inward.
        f0 = new_faces[0]
        c0 = f0.calc_center_median()
        if (c0.x - cx) * f0.normal.x + (c0.y - cy) * f0.normal.y < 0:
            bmesh.ops.reverse_faces(bm, faces=new_faces)
        bm.to_mesh(me)
        bm.free()
        me.update()
        print(f"make_fir_v03: {len(doomed)} root-splat faces removed, a smooth flare of {made} faces with {len(ROOTS)} roots in their place")

missing = [k for k in SWAP if k not in loaded]
if missing:
    raise SystemExit(f"make_fir_v03: bark images not found in the model: {missing}")
print(f"make_fir_v03: {swapped} image nodes now read the user's bark")
bpy.ops.export_scene.gltf(filepath=OUT, export_format="GLB")
print("wrote", OUT)
