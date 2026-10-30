"""The Wisp Forest's bare fir (polyhaven-wisp-v02/fir_tree_01_bare_v01.glb) has its trunk's UVs all at (0, 0): in the game the
trunk reads one texel of the bark and shows flat and dark (the user, 2026-10-30: "the tall pines used in wisp forest are never
loading their textures"). This writes fir_tree_01_bare_v02.glb: the same model with the trunk unwrapped round its axis (u around,
v up, the bark tiling at its own scale), every other part untouched. v01 stays.

blender -b -P content/models/blender/fix_fir_trunk_uv.py
"""
import math
import os

import bmesh
import bpy

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "polyhaven-wisp-v02")
SRC = os.path.join(ROOT, "fir_tree_01_bare_v01.glb")
OUT = os.path.join(ROOT, "fir_tree_01_bare_v02.glb")
TRUNK = "fir_tree_01_trunk_c"
AROUND = 2.0  # bark repeats twice round the trunk

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=SRC)

done = 0
for obj in bpy.data.objects:
    if obj.type != "MESH":
        continue
    me = obj.data
    slots = [i for i, m in enumerate(me.materials) if m and m.name.startswith(TRUNK)]
    if not slots:
        continue
    bm = bmesh.new()
    bm.from_mesh(me)
    uv = bm.loops.layers.uv.active or bm.loops.layers.uv.new()
    faces = [f for f in bm.faces if f.material_index in slots]
    verts = {v for f in faces for v in f.verts}
    # The axis: the trunk's vertical (glTF +Y = Blender +Z); its centre at each height is the mean of the ring there.
    zs = sorted(v.co.z for v in verts)
    low, high = zs[0], zs[-1]
    bands = 64
    sums = [[0.0, 0.0, 0] for _ in range(bands)]
    for v in verts:
        b = min(bands - 1, int((v.co.z - low) / max(high - low, 1e-6) * bands))
        sums[b][0] += v.co.x
        sums[b][1] += v.co.y
        sums[b][2] += 1
    centres = [(s[0] / s[2], s[1] / s[2]) if s[2] else None for s in sums]
    for i in range(bands):  # empty bands take the nearest filled one
        if centres[i] is None:
            centres[i] = next(c for d in range(1, bands) for c in (centres[max(i - d, 0)], centres[min(i + d, bands - 1)]) if c)
    radii = [math.hypot(v.co.x - centres[min(bands - 1, int((v.co.z - low) / max(high - low, 1e-6) * bands))][0],
                        v.co.y - centres[min(bands - 1, int((v.co.z - low) / max(high - low, 1e-6) * bands))][1]) for v in verts]
    radius = sorted(radii)[len(radii) // 2]
    vscale = AROUND / (2.0 * math.pi * max(radius, 1e-3))  # square texels: one repeat up = one repeat round
    for f in faces:
        us = []
        for loop in f.loops:
            co = loop.vert.co
            cx, cy = centres[min(bands - 1, int((co.z - low) / max(high - low, 1e-6) * bands))]
            us.append(math.atan2(co.y - cy, co.x - cx) / (2.0 * math.pi) * AROUND)
        # A face across the seam: its u's are brought onto one side.
        span = AROUND
        ref = us[0]
        for k, loop in enumerate(f.loops):
            u = us[k]
            while u - ref > span / 2:
                u -= span
            while ref - u > span / 2:
                u += span
            loop[uv].uv = (u, (loop.vert.co.z - low) * vscale)
    bm.to_mesh(me)
    bm.free()
    done += len(faces)
    # The importer's texture transform (scale 1.4, offset -0.4) stays out: the new UVs are the mapping.
    for i in slots:
        mat = me.materials[i]
        if mat and mat.node_tree:
            for node in list(mat.node_tree.nodes):
                if node.type == "MAPPING":
                    mat.node_tree.nodes.remove(node)

print(f"fix_fir_trunk_uv: {done} trunk faces unwrapped")
bpy.ops.export_scene.gltf(filepath=OUT, export_format="GLB")
print("wrote", OUT)
