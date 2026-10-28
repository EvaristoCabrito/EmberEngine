"""Render the additive O Vau map version from Ember scene-export coordinates."""
import json, math
from pathlib import Path
import bpy
from mathutils import Vector

ROOT = Path('C:/EmberEngine')
PUBLIC = ROOT / 'content/assets'
MAP = ROOT / 'content/maps/ember/vau030.json'
SCENE_EXPORT = Path(r'C:/Users/evari/.codex/visualizations/2026/10/24/01a16c18-9fe6-7212-a2e8-e2e0c6a23af1/vau030-scene.json')
OUT = Path(r'C:/Users/evari/.codex/visualizations/2026/10/24/01a16c18-9fe6-7212-a2e8-e2e0c6a23af1/vau030-map.png')

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = 'BLENDER_EEVEE'
scene.render.resolution_x = 2200
scene.render.resolution_y = 1700
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.view_settings.view_transform = 'AgX'
scene.view_settings.look = 'AgX - Medium High Contrast'
scene.world = bpy.data.worlds.new('neutral O Vau composition studio')
scene.world.color = (.035, .04, .038)

d = json.loads(MAP.read_text(encoding='utf-8'))['draft']
export = json.loads(SCENE_EXPORT.read_text(encoding='utf-8'))
cols, rows = d['cols'], d['rows']
ox, oy = (math.sqrt(3) * (cols - .5)) / 2, .75 * (rows - 1)

def material(name, color_path, roughness=.96):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Roughness'].default_value = roughness
    tex = m.node_tree.nodes.new('ShaderNodeTexImage')
    tex.image = bpy.data.images.load(str(color_path), check_existing=True)
    m.node_tree.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
    return m

terrain = {}
for name in ('plains', 'woods', 'water', 'hill'):
    terrain[name] = material('O Vau | ' + name, PUBLIC / f'game/ground-engine2-v4/{name}/color.png')
variants = {
    53: material('O Vau | Photo 3D ford bank', PUBLIC / 'game/ground-engine2-v4/plains/color.png'),
    15: material('O Vau | Photo 3D woods', PUBLIC / 'game/ground-engine2-v4/woods/color.png'),
    29: material('O Vau | Photo 3D shallows', PUBLIC / 'game/ground-engine2-v4/shallow-water/color.png'),
    30: material('O Vau | Photo 3D ford stones', PUBLIC / 'game/ground-engine2-v4/river-rocks/color.png'),
    7: material('O Vau | Photo 3D hills', PUBLIC / 'game/ground-engine2-v4/hill/color.png'),
}
materials = list(terrain.values()) + list(variants.values())
mat_index = {m.name: i for i, m in enumerate(materials)}

verts, faces, face_mats, uv_faces = [], [], [], []
corners = [(math.sin(k * math.pi / 3), math.cos(k * math.pi / 3)) for k in range(6)]
for y in range(rows):
    for x in range(cols):
        i = y * cols + x
        tile = d['tiles'][i]
        if tile == 'void':
            continue
        cx = math.sqrt(3) * (x + .5 * (y & 1)) - ox
        cy = 1.5 * y - oy
        base = len(verts)
        verts.extend((cx + px, cy + py, .005) for px, py in corners)
        variant = d['tileVariants'][i]
        m = variants.get(variant, terrain.get(tile, terrain['plains']))
        faces.append(list(range(base, base + 6)))
        face_mats.append(mat_index[m.name])
        uv_faces.append([(.5 + px * .48, .5 + py * .48) for px, py in corners])

mesh = bpy.data.meshes.new('O Vau irregular hex terrain')
mesh.from_pydata(verts, [], faces)
for m in materials:
    mesh.materials.append(m)
for poly, mat_id in zip(mesh.polygons, face_mats):
    poly.material_index = mat_id
uv = mesh.uv_layers.new(name='UVMap')
for poly, coords in zip(mesh.polygons, uv_faces):
    for loop, coord in zip(poly.loop_indices, coords):
        uv.data[loop].uv = coord
ground = bpy.data.objects.new('O Vau terrain', mesh)
scene.collection.objects.link(ground)

for dec in export['decor']:
    if dec['kind'] == 'model':
        model_path = PUBLIC / dec['file'].lstrip('/').replace('/', '\\')
        before = set(bpy.data.objects)
        bpy.ops.import_scene.gltf(filepath=str(model_path))
        for obj in (o for o in bpy.data.objects if o not in before):
            obj.location += Vector((dec['x'], dec['z'], .05))
    elif dec['kind'] == 'card':
        imgpath = PUBLIC / dec['image'].lstrip('/').replace('/', '\\')
        img = bpy.data.images.load(str(imgpath), check_existing=True)
        mat = bpy.data.materials.new('O Vau prop | ' + dec['id'])
        mat.use_nodes = True
        nodes, links = mat.node_tree.nodes, mat.node_tree.links
        bsdf = nodes.get('Principled BSDF')
        tex = nodes.new('ShaderNodeTexImage')
        tex.image = img
        links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
        links.new(tex.outputs['Alpha'], bsdf.inputs['Alpha'])
        bsdf.inputs['Roughness'].default_value = .95
        mat.surface_render_method = 'DITHERED'
        bpy.ops.mesh.primitive_plane_add(size=1, location=(dec['x'], dec['z'] - .04, dec['h'] / 2 + .06), rotation=(math.pi / 2, 0, 0))
        card = bpy.context.object
        card.name = dec['id']
        card.scale = (dec['w'], dec['h'], 1)
        card.data.materials.append(mat)

def aim(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat('-Z', 'Y').to_euler()

cam_data = bpy.data.cameras.new('O Vau camera')
cam = bpy.data.objects.new('O Vau camera', cam_data)
scene.collection.objects.link(cam)
cam.location = (0, -27, 66)
aim(cam, (0, 0, 4.2))
cam_data.type = 'ORTHO'
cam_data.ortho_scale = 33
scene.camera = cam
for name, loc, power, size in [
    ('studio key', (-15, -18, 30), 6200, 17),
    ('studio fill', (19, -8, 23), 3100, 15),
    ('studio rim', (0, 17, 25), 4300, 14),
]:
    light_data = bpy.data.lights.new(name, 'AREA')
    light_data.energy, light_data.shape, light_data.size = power, 'DISK', size
    light = bpy.data.objects.new(name, light_data)
    scene.collection.objects.link(light)
    light.location = loc
    aim(light, (0, 0, 3))

scene.render.filepath = str(OUT)
bpy.ops.render.render(write_still=True)
print(f'[ok] O Vau {d["cols"]}x{d["rows"]}: {len(export["decor"])} rendered props, {len(export["units"])} unit spawns -> {OUT}')
