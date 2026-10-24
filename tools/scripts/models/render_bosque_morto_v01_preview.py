"""Render the active Bosque Morto v01 map draft with its new ground set and 3D dead trees."""
import json
import math
from pathlib import Path

import bpy
from mathutils import Vector

ROOT = Path('C:/EmberEngine')
MAP = ROOT / 'content/maps/ember/bosque007.json'
TREE_DIR = ROOT / 'content/models/bosque-morto-v01'
GROUND = ROOT / 'content/ground'
OUT = Path(r'C:/Users/evari/.codex/visualizations/2026/10/24/01a16c58-11bd-7b00-9587-0dead5c843d3/bosque-morto-v01-preview.png')

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = 'BLENDER_EEVEE'
scene.render.resolution_x = 2400
scene.render.resolution_y = 1700
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.view_settings.view_transform = 'AgX'
scene.view_settings.look = 'AgX - Medium High Contrast'
scene.world = bpy.data.worlds.new('neutral forest preview')
scene.world.color = (.075, .08, .07)

draft = json.loads(MAP.read_text(encoding='utf-8'))['draft']
cols, rows = draft['cols'], draft['rows']
root3 = math.sqrt(3)
ox = root3 * (cols - .5) / 2
oy = .75 * (rows - 1)

def ground_material(name, file):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    bsdf = nodes.get('Principled BSDF')
    bsdf.inputs['Roughness'].default_value = .98
    tex = nodes.new('ShaderNodeTexImage')
    tex.image = bpy.data.images.load(str(GROUND / file), check_existing=True)
    mat.node_tree.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
    return mat

mats = {
    'woods': ground_material('Bosque Morto | damp moss', 'bosque-morto-moss-v01.png'),
    'plains': ground_material('Bosque Morto | dead leaves', 'bosque-morto-litter-v01.png'),
    'hill': ground_material('Bosque Morto | exposed roots', 'bosque-morto-roots-v01.png'),
}
verts, faces, face_materials, uv_specs = [], [], [], []
for row in range(rows):
    for col in range(cols):
        terrain = draft['tiles'][row * cols + col]
        if terrain in ('void', 'barricade'):
            continue
        cx = root3 * (col + .5 * (row & 1)) - ox
        cy = 1.5 * row - oy
        first = len(verts)
        corners = [(math.sin(k * math.pi / 3), math.cos(k * math.pi / 3)) for k in range(6)]
        verts.extend([(cx + x, cy + y, 0) for x, y in corners])
        faces.append(list(range(first, first + 6)))
        face_materials.append(terrain if terrain in mats else 'plains')
        repeat = {'woods': 4.5, 'plains': 4.0, 'hill': 4.5}.get(terrain, 4.0)
        uv_specs.append([((cx + x) / repeat + .5, (cy + y) / repeat + .5) for x, y in corners])

mesh = bpy.data.meshes.new('Bosque Morto hex ground')
mesh.from_pydata(verts, [], faces)
for mat in mats.values():
    mesh.materials.append(mat)
mat_index = {name: i for i, name in enumerate(mats)}
for poly, material in zip(mesh.polygons, face_materials):
    poly.material_index = mat_index[material]
uv = mesh.uv_layers.new(name='UVMap')
for poly, coords in zip(mesh.polygons, uv_specs):
    for loop, coord in zip(poly.loop_indices, coords):
        uv.data[loop].uv = coord
floor = bpy.data.objects.new('Bosque Morto ground', mesh)
scene.collection.objects.link(floor)

tree_stems = {
    'ember-bosque-morto-giant-dead-oak': 'giant-dead-oak-v01.glb',
    'ember-bosque-morto-hollow-snag': 'hollow-snag-v01.glb',
    'ember-bosque-morto-lightning-split': 'lightning-split-v01.glb',
    'ember-bosque-morto-weeping-willow': 'weeping-willow-v01.glb',
    'ember-bosque-morto-uprooted-leaning': 'uprooted-leaning-v01.glb',
}
for decoration in draft['decorations']:
    stem = tree_stems.get(decoration['id'])
    if not stem:
        continue
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(TREE_DIR / stem))
    cx = root3 * (decoration['x'] + .5 * (decoration['y'] & 1)) - ox
    cy = 1.5 * decoration['y'] - oy
    for obj in set(bpy.data.objects) - before:
        obj.location += Vector((cx, cy, .04))

def aim(obj, point):
    obj.rotation_euler = (Vector(point) - obj.location).to_track_quat('-Z', 'Y').to_euler()

camera_data = bpy.data.cameras.new('Bosque Morto preview camera')
camera = bpy.data.objects.new('Bosque Morto preview camera', camera_data)
scene.collection.objects.link(camera)
camera.location = (0, -20, 43)
aim(camera, (0, 0, 3.5))
camera_data.type = 'ORTHO'
camera_data.ortho_scale = 37
scene.camera = camera

for name, loc, power, size in [
    ('soft key', (-14, -16, 28), 9000, 22),
    ('soft fill', (16, -3, 22), 4300, 18),
    ('soft rim', (0, 14, 24), 5500, 20),
]:
    light_data = bpy.data.lights.new(name, 'AREA')
    light_data.energy = power
    light_data.shape = 'DISK'
    light_data.size = size
    light = bpy.data.objects.new(name, light_data)
    scene.collection.objects.link(light)
    light.location = loc
    aim(light, (0, 0, 2))

OUT.parent.mkdir(parents=True, exist_ok=True)
scene.render.filepath = str(OUT)
bpy.ops.render.render(write_still=True)
print(f'[ok] rendered {len(tree_stems)} 3D dead trees on Bosque Morto {cols}x{rows} ground -> {OUT}')
