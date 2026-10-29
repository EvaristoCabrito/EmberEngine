"""A Poly Haven model (content/models/polyhaven/<id>/<id>_2k.gltf, CC0) -> one GLB the mesh cooker reads, textures embedded.

  blender -b --factory-startup -P content/models/blender/import_polyhaven.py -- <gltf> <out.glb> [maxTris] [keepNode]

The source files stay as downloaded; the GLB keeps the full geometry and the textures' own bytes. maxTris (a decimated copy) is
only for a light ALTERNATIVE beside the full model, when the full one is shown not to run (the user, 2026-10-29), never in its place. With keepNode, only that glTF node (and its children) is exported, moved to the
origin (several variants side by side in one file: a fern, a shrub, three firs). Prints the bounds in metres (Poly Haven models are
real size) and the triangle count.
"""
import sys

import bpy

args = sys.argv[sys.argv.index('--') + 1:]
src, out = args[0], args[1]
max_tris = int(args[2]) if len(args) > 2 and args[2] else 0
keep = args[3] if len(args) > 3 else ''

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
if keep:
    root = bpy.data.objects.get(keep)
    if root is None:
        raise SystemExit(f'polyhaven: no node {keep} in {src}')
    kept = {root} | set(root.children_recursive)
    for o in list(bpy.context.scene.objects):
        if o not in kept:
            bpy.data.objects.remove(o, do_unlink=True)
    root.location = (0.0, 0.0, root.location.z)
    bpy.context.view_layer.update()
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']


def tris():
    deps = bpy.context.evaluated_depsgraph_get()
    n = 0
    for o in meshes:
        m = o.evaluated_get(deps).to_mesh()
        m.calc_loop_triangles()
        n += len(m.loop_triangles)
        o.evaluated_get(deps).to_mesh_clear()
    return n


before = tris()
if max_tris and before > max_tris:
    for o in meshes:
        d = o.modifiers.new('decimate', 'DECIMATE')
        d.ratio = max_tris / before
    bpy.context.view_layer.update()
after = tris()

lo = [1e9] * 3
hi = [-1e9] * 3
for o in meshes:
    for c in o.bound_box:
        w = o.matrix_world @ __import__('mathutils').Vector(c)
        for i in range(3):
            lo[i], hi[i] = min(lo[i], w[i]), max(hi[i], w[i])
print(f'polyhaven: {src} tris {before} -> {after}; size x {hi[0]-lo[0]:.3f} y {hi[1]-lo[1]:.3f} z(up) {hi[2]-lo[2]:.3f} m')
# The textures as downloaded (AUTO: a JPEG stays the same JPEG, a PNG the same PNG; the user, 2026-10-29: the original quality, never
# reduced or re-encoded).
bpy.ops.export_scene.gltf(filepath=out, export_format='GLB', export_apply=True, export_image_format='AUTO')
print(f'polyhaven: wrote {out}')
