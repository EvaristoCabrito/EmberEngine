"""A TripoSR reconstruction (build/ext/TripoSR, run on the CPU: OBJ + baked colour atlas) as an EmberEngine prop: the raw
mesh welded, turned upright (TripoSR's frame is z-up) and levelled (the photo's camera tilt taken out: the flattest base),
decimated (--decimate, the share of triangles kept), scaled so its height is the catalog height in world units, stood on
z = 0, smooth shaded, its baked atlas as the base colour. Exported as <kit>/<name>-vNN.glb + .blend (the next free version:
nothing is overwritten). The atlas is copied beside the asset as <name>-vNN-color.png.

  blender -b --python content/models/blender/import_triposr.py -- <mesh.obj> <texture.png> <kit> <name> <height> [--up-axis Z]
        [--roughness 0.45] [--metallic 0.5] [--turn DEG] [--decimate 0.25] [--dry-run]
"""
import sys, os, json, math, shutil
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
import bmesh
import numpy as np
from emberlib import *  # noqa

argv = sys.argv[sys.argv.index('--') + 1:]
obj_path, tex_path, kit, name, height = argv[0], argv[1], argv[2], argv[3], float(argv[4])


def opt(flag, default):
    return type(default)(argv[argv.index(flag) + 1]) if flag in argv else default


up_axis, roughness, metallic, turn = opt('--up-axis', 'Z'), opt('--roughness', 0.45), opt('--metallic', 0.5), opt('--turn', 0.0)
ratio = opt('--decimate', 0.25)

reset()
bpy.ops.wm.obj_import(filepath=obj_path, forward_axis='NEGATIVE_Z' if up_axis == 'Y' else 'Y', up_axis=up_axis)
meshes = [o for o in bpy.data.objects if o.type == 'MESH']
assert len(meshes) == 1, meshes
o = meshes[0]
# xatlas wrote one vertex per atlas chart: weld them (UVs are per corner in Blender, the seams stay) so shading is continuous
bm = bmesh.new()
bm.from_mesh(o.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
bm.to_mesh(o.data)
bm.free()


def level(o):
    """TripoSR rebuilds the object in the photo camera's frame, so it leans by the photo's elevation. Find the tilt (x, y) that
    puts the most vertices on the floor plane (a flat base), coarse 1 deg then fine 0.2 deg, within +-35 deg."""
    co = np.array([v.co[:] for v in o.data.vertices])
    co -= co.mean(0)

    def score(rx, ry):
        a, b = math.radians(rx), math.radians(ry)
        z = -co[:, 0] * math.sin(b) + (co[:, 1] * math.sin(a) + co[:, 2] * math.cos(a)) * math.cos(b)
        h = z.max() - z.min()
        return np.count_nonzero(z < z.min() + 0.01 * h)

    best = max(((score(x, y), x, y) for x in range(-35, 36) for y in range(-35, 36)))
    _, bx, by = best
    best = max(((score(bx + i * 0.2, by + j * 0.2), bx + i * 0.2, by + j * 0.2) for i in range(-5, 6) for j in range(-5, 6)))
    return best[1], best[2], best[0]


rx, ry, on_floor = level(o)
o.rotation_euler = (math.radians(rx), math.radians(ry), 0.0)
apply_transform(o)
o.rotation_euler = (0.0, 0.0, math.radians(turn))
apply_transform(o)
if ratio < 1.0:
    md = o.modifiers.new('decimate', 'DECIMATE')
    md.ratio = ratio
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.modifier_apply(modifier=md.name)
zs = [v.co.z for v in o.data.vertices]
s = height / (max(zs) - min(zs))
o.scale = (s, s, s)
apply_transform(o)
ground(o)

v = 1
while os.path.exists(f'{ROOT}/{kit}/{name}-v{v:02d}.glb'):
    v += 1
version = f'v{v:02d}'
os.makedirs(f'{ROOT}/{kit}', exist_ok=True)
tex_copy = f'{ROOT}/{kit}/{name}-{version}-color.png'
shutil.copyfile(tex_path, tex_copy)

m = bpy.data.materials.new(f'{name}-triposr')
m.use_nodes = True
nt = m.node_tree
bsdf = nt.nodes['Principled BSDF']
t = nt.nodes.new('ShaderNodeTexImage')
t.image = bpy.data.images.load(tex_copy)
t.image.colorspace_settings.name = 'sRGB'
nt.links.new(t.outputs['Color'], bsdf.inputs['Base Color'])
bsdf.inputs['Roughness'].default_value = roughness
bsdf.inputs['Metallic'].default_value = metallic
o.data.materials.clear()
o.data.materials.append(m)
for p in o.data.polygons:
    p.use_smooth = True
o.name = o.data.name = name


def preview(prefix):
    """Four workbench views (front, right, back, left) of the result, for a look before it is exported."""
    from mathutils import Vector
    s = bpy.context.scene
    s.render.engine = 'BLENDER_WORKBENCH'
    s.display.shading.light = 'STUDIO'
    s.display.shading.color_type = 'TEXTURE'
    s.render.resolution_x = s.render.resolution_y = 512
    s.world = bpy.data.worlds.new('preview')
    cam = bpy.data.objects.new('preview', bpy.data.cameras.new('preview'))
    s.collection.objects.link(cam)
    s.camera = cam
    co = [v.co for v in o.data.vertices]
    c = Vector((0.0, 0.0, max(v.z for v in co) * 0.5))
    d = max(max(v.z for v in co), max(abs(v.x) for v in co) * 2, max(abs(v.y) for v in co) * 2) * 1.9
    for i, az in enumerate((0, 90, 180, 270)):
        a = math.radians(az)
        cam.location = c + Vector((math.sin(a) * d, -math.cos(a) * d, d * 0.3))
        cam.rotation_euler = (c - cam.location).to_track_quat('-Z', 'Y').to_euler()
        s.render.filepath = f'{prefix}_{i}.png'
        bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam)


if '--preview' in argv:
    preview(argv[argv.index('--preview') + 1])
if '--dry-run' in argv:
    os.remove(tex_copy)
    print('DRY', json.dumps({'level': [rx, ry, int(on_floor)], 'triangles': len(o.data.polygons)}))
    sys.exit(0)

r = export(o, kit, name, version=version)
r['version'] = version
r['source'] = os.path.abspath(obj_path).replace('\\', '/')
r['level'] = {'x': round(rx, 2), 'y': round(ry, 2), 'floor_vertices': int(on_floor)}
print('JSON_BEGIN' + json.dumps({'kit': kit, 'assets': [r], 'errors': []}) + 'JSON_END')
