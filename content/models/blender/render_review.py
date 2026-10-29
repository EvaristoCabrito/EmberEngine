"""Review renders of a kit's assets (content/models/README.md § Tree Creating Pipeline, step 4): each GLB alone on a neutral floor,
four views (three-quarter, front, a close detail, the base), 1024 px, into captures/models/<kit>-review/<name>-<view>.png.
Presented to the user for approval before the asset is catalogued; the game is never opened for it.

  blender -b --python content/models/blender/render_review.py -- <kit> [<name>-vNN ...]
"""
import math
import os
import sys

import bpy
from mathutils import Vector

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
argv = sys.argv[sys.argv.index('--') + 1:]
kit, only = argv[0], set(argv[1:])
SRC = os.path.join(ROOT, 'content', 'models', kit)
OUT = os.path.join(ROOT, 'captures', 'models', f'{kit}-review')
os.makedirs(OUT, exist_ok=True)
VIEWS = {'three-quarter': (math.radians(38), math.radians(22), 1.0, 0.5), 'front': (math.radians(2), math.radians(8), 1.0, 0.5),
         'detail': (math.radians(-30), math.radians(14), 0.42, 0.72), 'base': (math.radians(55), math.radians(6), 0.5, 0.12)}


def setup():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    s = bpy.context.scene
    s.render.engine = 'CYCLES'
    s.cycles.samples = 96
    s.cycles.use_denoising = True
    s.render.resolution_x = s.render.resolution_y = 1024
    s.render.image_settings.file_format = 'PNG'
    s.view_settings.view_transform = 'Filmic' if 'Filmic' in [i.identifier for i in s.view_settings.bl_rna.properties['view_transform'].enum_items] else 'Standard'
    world = bpy.data.worlds.new('review')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs[0].default_value = (0.5, 0.53, 0.58, 1)
    world.node_tree.nodes['Background'].inputs[1].default_value = 0.7
    s.world = world
    key = bpy.data.objects.new('key', bpy.data.lights.new('key', 'SUN'))
    key.data.energy = 3.0
    key.data.angle = math.radians(6)
    key.rotation_euler = (math.radians(48), 0, math.radians(-35))
    s.collection.objects.link(key)
    fill = bpy.data.objects.new('fill', bpy.data.lights.new('fill', 'AREA'))
    fill.data.energy = 250
    fill.data.size = 6
    fill.location = (-4, -5, 4)
    fill.rotation_euler = (math.radians(55), 0, math.radians(-40))
    s.collection.objects.link(fill)
    bpy.ops.mesh.primitive_plane_add(size=40, location=(0, 0, -0.001))
    floor = bpy.context.active_object
    m = bpy.data.materials.new('floor')
    m.use_nodes = True
    m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.32, 0.31, 0.3, 1)
    m.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 0.95
    floor.data.materials.append(m)
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    cam.data.lens = 60
    cam.data.clip_start, cam.data.clip_end = 0.01, 1000
    s.collection.objects.link(cam)
    s.camera = cam
    return s, cam, floor


def bounds(objs):
    lo, hi = Vector((1e9, 1e9, 1e9)), Vector((-1e9, -1e9, -1e9))
    for o in objs:
        if o.type != 'MESH':
            continue
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            lo, hi = Vector(map(min, lo, w)), Vector(map(max, hi, w))
    return lo, hi


def aim(cam, lo, hi, yaw, pitch, share, focus):
    size = hi - lo
    centre = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z + size.z * focus))
    view = Vector((math.sin(yaw) * math.cos(pitch), -math.cos(yaw) * math.cos(pitch), math.sin(pitch)))
    radius = max(size.x, size.y, size.z) * 0.5 * share
    dist = radius / math.tan(cam.data.angle / 2) * 1.1
    cam.location = centre + view * dist
    cam.rotation_euler = (-view).to_track_quat('-Z', 'Y').to_euler()


scene, cam, floor = setup()
for f in sorted(os.listdir(SRC)):
    if not f.endswith('.glb'):
        continue
    stem = f[:-4]
    if only and stem not in only:
        continue
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(SRC, f))
    objs = [o for o in bpy.data.objects if o not in before]
    lo, hi = bounds(objs)
    for view, (yaw, pitch, share, focus) in VIEWS.items():
        aim(cam, lo, hi, yaw, pitch, share, focus)
        scene.render.filepath = os.path.join(OUT, f'{stem}-{view}.png')
        bpy.ops.render.render(write_still=True)
        print(f'review {stem} {view}')
    for o in objs:
        bpy.data.objects.remove(o, do_unlink=True)
print('done')
