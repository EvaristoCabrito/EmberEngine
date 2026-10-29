"""A Poly Haven model (content/models/polyhaven/<id>/<id>_2k.gltf, CC0) -> one GLB the mesh cooker reads, textures embedded.

  blender -b --factory-startup -P content/models/blender/import_polyhaven.py -- <gltf> <out.glb> [maxTris]

The source files stay as downloaded. With maxTris, a model above it is decimated (collapse) to about that many triangles in the GLB
only; the UVs and the photo textures are kept. Prints the bounds in metres (Poly Haven models are real size) and the triangle count.
"""
import sys

import bpy

args = sys.argv[sys.argv.index('--') + 1:]
src, out = args[0], args[1]
max_tris = int(args[2]) if len(args) > 2 else 0

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
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
bpy.ops.export_scene.gltf(filepath=out, export_format='GLB', export_apply=True, export_image_format='JPEG')
print(f'polyhaven: wrote {out}')
