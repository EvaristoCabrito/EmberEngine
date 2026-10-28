"""Offline Blender composition preview for the large Wisp Forest Crossing draft."""
import json
import math
from pathlib import Path

import bpy
import bmesh
from mathutils import Vector

ROOT = Path('C:/EmberEngine')
PUBLIC = ROOT / 'content/assets'
MAP = ROOT / 'content/maps/ember/wisp-forest-crossing015.json'
SCENE_EXPORT = Path(r'C:/Users/evari/.codex/visualizations/2026/10/24/01a16c18-9fe6-7212-a2e8-e2e0c6a23af1/wisp-crossing-revisited-scene.json')
OUT = Path(r'C:/Users/evari/.codex/visualizations/2026/10/24/01a16c18-9fe6-7212-a2e8-e2e0c6a23af1/wisp-crossing-revisited-map-v02.png')

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = 'BLENDER_EEVEE'
scene.render.resolution_x = 2400
scene.render.resolution_y = 1600
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.view_settings.view_transform = 'AgX'
scene.view_settings.look = 'AgX - Medium High Contrast'
scene.view_settings.exposure = 1.0
scene.world = bpy.data.worlds.new('neutral Wisp Crossing composition studio')
scene.world.color = (.035, .04, .038)

data = json.loads(MAP.read_text(encoding='utf-8'))['draft']
cols, rows = data['cols'], data['rows']
variants = data['tileVariants']
terrain = data['tiles']


def make_material(name, color_path, roughness=.96, normal_path=None, rough_path=None):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    bsdf = nodes.get('Principled BSDF')
    bsdf.inputs['Roughness'].default_value = roughness
    color = nodes.new('ShaderNodeTexImage')
    color.image = bpy.data.images.load(str(color_path), check_existing=True)
    links.new(color.outputs['Color'], bsdf.inputs['Base Color'])
    if rough_path and rough_path.exists():
        tex = nodes.new('ShaderNodeTexImage')
        tex.image = bpy.data.images.load(str(rough_path), check_existing=True)
        tex.image.colorspace_settings.name = 'Non-Color'
        links.new(tex.outputs['Color'], bsdf.inputs['Roughness'])
    if normal_path and normal_path.exists():
        tex = nodes.new('ShaderNodeTexImage')
        tex.image = bpy.data.images.load(str(normal_path), check_existing=True)
        tex.image.colorspace_settings.name = 'Non-Color'
        normal = nodes.new('ShaderNodeNormalMap')
        links.new(tex.outputs['Color'], normal.inputs['Color'])
        links.new(normal.outputs['Normal'], bsdf.inputs['Normal'])
    return material


terrain_materials = {}
for name in ('plains', 'woods', 'ruins', 'water', 'nave', 'door'):
    folder = 'plains-v2' if name == 'plains' else name
    path = PUBLIC / 'game/ground-claude' / folder
    terrain_materials[name] = make_material(
        f'Wisp Crossing | {name}', path / 'color.png', .97,
        path / 'normal.png', path / 'rough.png')

variant_materials = {
    62: make_material('Wisp | leaf litter', ROOT / 'content/models/textures/wisp-leaf-litter-v01.jpg', .98),
    63: make_material('Wisp | root soil', ROOT / 'content/models/textures/wisp-root-soil-v01.jpg', .98),
    64: make_material('Wisp | ash litter', ROOT / 'content/models/textures/wisp-charred-leaf-v01.jpg', .99),
}
all_materials = list(terrain_materials.values()) + list(variant_materials.values())
material_index = {m.name: i for i, m in enumerate(all_materials)}

verts, faces, face_materials, uv_faces = [], [], [], []
radius = 1.0
ox = (math.sqrt(3) * (cols - .5)) / 2
oy = .75 * (rows - 1)
corners = [(math.sin(k * math.pi / 3), math.cos(k * math.pi / 3)) for k in range(6)]
for y in range(rows):
    for x in range(cols):
        idx = y * cols + x
        tile = terrain[idx]
        if tile == 'void':
            continue
        cx = math.sqrt(3) * (x + .5 * (y & 1)) - ox
        cy = 1.5 * y - oy
        base = len(verts)
        verts.extend((cx + px * radius, cy + py * radius, .005) for px, py in corners)
        variant = variants[idx]
        mat = variant_materials.get(variant, terrain_materials.get(tile, terrain_materials['plains']))
        ids = list(range(base, base + 6))
        faces.append(ids)
        face_materials.append(material_index[mat.name])
        uv_faces.append(([(.5 + px * .48, .5 + py * .48) for px, py in corners]))

mesh = bpy.data.meshes.new('44x36 irregular Wisp Crossing surface')
mesh.from_pydata(verts, [], faces)
for material in all_materials:
    mesh.materials.append(material)
for polygon, index in zip(mesh.polygons, face_materials):
    polygon.material_index = index
bm = bmesh.new()
bm.from_mesh(mesh)
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
bm.to_mesh(mesh)
bm.free()
uv = mesh.uv_layers.new(name='UVMap')
for polygon, coords in zip(mesh.polygons, uv_faces):
    for loop_id, coord in zip(polygon.loop_indices, coords):
        uv.data[loop_id].uv = coord
floor = bpy.data.objects.new('irregular crossing ground', mesh)
scene.collection.objects.link(floor)

scene_export = json.loads(SCENE_EXPORT.read_text(encoding='utf-8'))
kit = ROOT / 'content/models/wisp-forest-revisited'
for decoration in scene_export['decor']:
    if decoration['kind'] == 'model':
        version = 'v02' if (kit / f"{decoration['model']}-v02.glb").exists() else 'v01'
        before = set(bpy.data.objects)
        bpy.ops.import_scene.gltf(filepath=str(kit / f"{decoration['model']}-{version}.glb"))
        for obj in set(bpy.data.objects) - before:
            obj.location += Vector((decoration['x'], decoration['z'], .05))
    elif decoration['kind'] == 'card':
        image_path = PUBLIC / decoration['image'].lstrip('/').replace('/', '\\')
        if not image_path.exists():
            continue
        image = bpy.data.images.load(str(image_path), check_existing=True)
        material = bpy.data.materials.new('preserved ' + decoration['id'])
        material.use_nodes = True
        nodes, links = material.node_tree.nodes, material.node_tree.links
        bsdf = nodes.get('Principled BSDF')
        texture = nodes.new('ShaderNodeTexImage')
        texture.image = image
        links.new(texture.outputs['Color'], bsdf.inputs['Base Color'])
        links.new(texture.outputs['Alpha'], bsdf.inputs['Alpha'])
        bsdf.inputs['Roughness'].default_value = .95
        material.surface_render_method = 'DITHERED'
        bpy.ops.mesh.primitive_plane_add(size=1, location=(decoration['x'], decoration['z'] - .04, decoration['h'] / 2 + .06), rotation=(math.pi / 2, 0, 0))
        card = bpy.context.object
        card.name = decoration['id']
        card.scale = (decoration['w'], decoration['h'], 1)
        card.data.materials.append(material)


def aim(obj, point):
    obj.rotation_euler = (Vector(point) - obj.location).to_track_quat('-Z', 'Y').to_euler()


camera_data = bpy.data.cameras.new('Wisp Crossing camera')
camera = bpy.data.objects.new('Wisp Crossing camera', camera_data)
scene.collection.objects.link(camera)
camera.location = (0, -77, 135)
aim(camera, (0, 0, 2))
camera_data.type = 'ORTHO'
camera_data.ortho_scale = 98
scene.camera = camera

for name, location, power, size in [
    ('key', (-35, -40, 62), 14000, 30),
    ('fill', (40, -16, 48), 7000, 26),
    ('rim', (0, 38, 55), 9500, 24),
]:
    light_data = bpy.data.lights.new(name, 'AREA')
    light_data.energy = power
    light_data.shape = 'DISK'
    light_data.size = size
    light = bpy.data.objects.new(name, light_data)
    scene.collection.objects.link(light)
    light.location = location
    aim(light, (0, 0, 2))

scene.render.filepath = str(OUT)
bpy.ops.render.render(write_still=True)
print(f"[ok] rendered {data['title']}: {len(scene_export['decor'])} preserved/new decorations -> {OUT}")
