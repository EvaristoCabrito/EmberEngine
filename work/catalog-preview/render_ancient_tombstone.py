import bpy
from mathutils import Vector
src = r'C:\EmberEngine\content\models\crypt\ancient-tombstone-v01.glb'
texture = r'C:\EmberEngine\Attachments\watchtower\gravestone.jpg'
out = r'C:\Users\evari\.codex\visualizations\2026\10\25\01a173d0-1874-7980-a25d-ede55ba788b1\catalog-next-review\ancient-tombstone-gravestone-texture-preview.png'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
for material in bpy.data.materials:
    if material.name not in {'masonry-crypt', 'masonry-mossy'} or not material.use_nodes:
        continue
    for node in material.node_tree.nodes:
        if node.type == 'TEX_IMAGE':
            node.image = bpy.data.images.load(texture, check_existing=True)
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
pts = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
center = (lo + hi) * .5
size = hi - lo
span = max(size.x, size.y, size.z)
sc = bpy.context.scene
sc.render.engine = 'BLENDER_EEVEE'
sc.eevee.taa_render_samples = 128
sc.render.resolution_x = 960
sc.render.resolution_y = 960
sc.render.resolution_percentage = 100
sc.render.image_settings.file_format = 'PNG'
sc.render.image_settings.color_mode = 'RGB'
sc.render.filepath = out
sc.world = bpy.data.worlds.new('preview_world')
sc.world.use_nodes = True
sc.world.node_tree.nodes.get('Background').inputs['Color'].default_value = (.36,.36,.36,1)
sc.world.node_tree.nodes.get('Background').inputs['Strength'].default_value = .35
sc.view_settings.view_transform = 'AgX'
for name, loc, energy, sz in [('key',(2,-4,5),550,4),('fill',(-4,-3,2),220,4),('rim',(2,3,4),360,3)]:
    bpy.ops.object.light_add(type='AREA', location=(center.x+loc[0]*span/3, center.y+loc[1]*span/3, center.z+loc[2]*span/3))
    light=bpy.context.object
    light.name=name
    light.data.energy=energy
    light.data.shape='DISK'
    light.data.size=sz*span/3
    light.rotation_euler=(center-Vector(light.location)).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(center.x+span*.45, center.y-span*3.2, center.z+size.z*.5))
cam=bpy.context.object
cam.rotation_euler=(center-Vector(cam.location)).to_track_quat('-Z','Y').to_euler()
cam.data.type='ORTHO'
cam.data.ortho_scale=span*1.28
sc.camera=cam
bpy.ops.render.render(write_still=True)
print('PREVIEW_OK',out)

