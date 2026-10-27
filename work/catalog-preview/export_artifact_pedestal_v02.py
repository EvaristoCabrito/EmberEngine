import bpy
from mathutils import Vector
out=r'C:\Users\evari\.codex\visualizations\2026\10\25\01a173d0-1874-7980-a25d-ede55ba788b1\catalog-next-review\artifact-pedestal-v02-preview.png'
tex=r'C:\EmberEngine\Attachments\watchtower\gravestone.jpg'
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene
mat=bpy.data.materials.new('Pedra cinza fotografica'); mat.diffuse_color=(.34,.36,.37,1); mat.use_nodes=True
bsdf=mat.node_tree.nodes.get('Principled BSDF'); bsdf.inputs['Roughness'].default_value=.92
im=bpy.data.images.load(tex,check_existing=True); node=mat.node_tree.nodes.new('ShaderNodeTexImage'); node.image=im
mat.node_tree.links.new(node.outputs['Color'],bsdf.inputs['Base Color'])
parts=[('Base larga',(1.10,.76,.27),.135,.035),('Corpo do pedestal',(.76,.52,.72),.63,.035),('Moldura superior',(.86,.62,.12),1.05,.025),('Tampo',(1.00,.74,.20),1.21,.035)]
for name,dims,z,bevel in parts:
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0,0,z))
    ob=bpy.context.object; ob.name=name; ob.dimensions=dims; bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    ob.data.materials.append(mat)
    # Unwrap each stone block cleanly so the photograph stays legible on every face.
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.uv.smart_project(island_margin=.02); bpy.ops.object.mode_set(mode='OBJECT')
    mod=ob.modifiers.new('Bordas suavizadas','BEVEL'); mod.width=bevel; mod.segments=3
    ob.modifiers.new('Normais de peso','WEIGHTED_NORMAL')
scene.render.engine='BLENDER_EEVEE'; scene.eevee.taa_render_samples=96
scene.render.resolution_x=960; scene.render.resolution_y=960; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.image_settings.color_mode='RGB'; scene.render.filepath=out
scene.world=bpy.data.worlds.new('Neutral gray preview'); scene.world.use_nodes=True
scene.world.node_tree.nodes.get('Background').inputs['Color'].default_value=(.35,.35,.35,1)
scene.world.node_tree.nodes.get('Background').inputs['Strength'].default_value=.45
scene.view_settings.view_transform='AgX'
center=Vector((0,0,.70))
for name,loc,energy,size in [('key',(2,-4,5),650,3),('fill',(-4,-3,2),260,3),('rim',(2,3,4),400,2.5)]:
    bpy.ops.object.light_add(type='AREA',location=loc); light=bpy.context.object; light.name=name
    light.data.energy=energy; light.data.shape='DISK'; light.data.size=size; light.rotation_euler=(center-Vector(light.location)).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(1.05,-4.8,2.0)); cam=bpy.context.object
cam.rotation_euler=(center-Vector(cam.location)).to_track_quat('-Z','Y').to_euler(); cam.data.type='ORTHO'; cam.data.ortho_scale=2.25; scene.camera=cam
bpy.ops.render.render(write_still=True)
im.pack()
blend=r'C:\EmberEngine\content\models\crypt\artifact-pedestal-v02.blend'
glb=r'C:\EmberEngine\content\models\crypt\artifact-pedestal-v02.glb'
bpy.ops.wm.save_as_mainfile(filepath=blend)
bpy.ops.object.select_all(action='DESELECT')
mesh_objs=[o for o in scene.objects if o.type=='MESH']
for o in mesh_objs: o.select_set(True)
bpy.context.view_layer.objects.active=mesh_objs[0]
bpy.ops.export_scene.gltf(filepath=glb,export_format='GLB',use_selection=True,export_apply=True,export_texcoords=True,export_materials='EXPORT')
deps=bpy.context.evaluated_depsgraph_get(); tris=0
for obj in mesh_objs:
    ev=obj.evaluated_get(deps); temp=ev.to_mesh(); temp.calc_loop_triangles(); tris+=len(temp.loop_triangles); ev.to_mesh_clear()
print('EXPORT_OK',glb,blend,'triangles',tris,'mesh_objects',len(mesh_objs))
print('PREVIEW_OK',out)


