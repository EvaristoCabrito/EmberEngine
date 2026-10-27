import bpy, math
from mathutils import Vector
out=r'C:\Users\evari\.codex\visualizations\2026\10\25\01a173d0-1874-7980-a25d-ede55ba788b1\catalog-next-review\column-base-v02-preview.png'
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene
# Use the embedded, approved gray gravestone photo texture on every stone surface.
bpy.ops.import_scene.gltf(filepath=r'C:\EmberEngine\content\models\crypt\ancient-tombstone-v03.glb')
mat=bpy.data.materials['Pedra cinza fotografica']
for obj in list(scene.objects): bpy.data.objects.remove(obj,do_unlink=True)

def unwrap(obj):
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active=obj
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.uv.smart_project(island_margin=.02); bpy.ops.object.mode_set(mode='OBJECT')

def finish(obj,name,bevel):
    obj.name=name; obj.data.materials.append(mat); unwrap(obj)
    if bevel:
        b=obj.modifiers.new('Bordas gastas','BEVEL'); b.width=bevel; b.segments=3; obj.modifiers.new('Normais de peso','WEIGHTED_NORMAL')

# Wide lower plinth and a shallow sloped course, joined without gaps.
bpy.ops.mesh.primitive_cylinder_add(vertices=20,radius=.85,depth=.22,location=(0,0,.11)); finish(bpy.context.object,'Plinto circular de pedra',.035)
bpy.ops.mesh.primitive_cone_add(vertices=24,radius1=.72,radius2=.60,depth=.22,location=(0,0,.32)); finish(bpy.context.object,'Transição inclinada do plinto',.018)
# Broken column stump with a rough, uneven fracture line; the top is stone-textured too.
n=20; bottom_z=.42; base_r=.57
verts=[]
for i in range(n):
    a=2*math.pi*i/n; r=base_r*(1+.018*math.sin(i*2.1)); verts.append((r*math.cos(a),r*math.sin(a),bottom_z))
heights=[1.14+.06*math.sin(i*1.17)+.035*math.cos(i*2.43) for i in range(n)]
for i in range(n):
    a=2*math.pi*i/n; r=base_r*(.96+.025*math.cos(i*1.73)); verts.append((r*math.cos(a),r*math.sin(a),heights[i]))
faces=[]
for i in range(n): faces.append((i,(i+1)%n,n+(i+1)%n,n+i))
faces.append(tuple(range(n,2*n)))
mesh=bpy.data.meshes.new('Fratura irregular da coluna'); mesh.from_pydata(verts,[],faces); mesh.update()
stump=bpy.data.objects.new('Trecho quebrado da coluna',mesh); scene.collection.objects.link(stump); finish(stump,'Trecho quebrado da coluna',.012)
scene.render.engine='BLENDER_EEVEE'; scene.eevee.taa_render_samples=96
scene.render.resolution_x=960; scene.render.resolution_y=960; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.image_settings.color_mode='RGB'; scene.render.filepath=out
scene.world=bpy.data.worlds.new('Fundo cinza neutro'); scene.world.use_nodes=True
scene.world.node_tree.nodes.get('Background').inputs['Color'].default_value=(.34,.34,.34,1); scene.world.node_tree.nodes.get('Background').inputs['Strength'].default_value=.42
scene.view_settings.view_transform='AgX'; center=Vector((0,0,.62))
for name,loc,energy,size in [('key',(2,-4,4),650,2.8),('fill',(-4,-3,2),260,3),('rim',(2,3,4),400,2.5)]:
    bpy.ops.object.light_add(type='AREA',location=loc); light=bpy.context.object; light.name=name; light.data.energy=energy; light.data.shape='DISK'; light.data.size=size; light.rotation_euler=(center-Vector(light.location)).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(.65,-4.4,1.8)); cam=bpy.context.object; cam.rotation_euler=(center-Vector(cam.location)).to_track_quat('-Z','Y').to_euler(); cam.data.type='ORTHO'; cam.data.ortho_scale=2.35; scene.camera=cam
bpy.ops.render.render(write_still=True)
for image in bpy.data.images:
    if image.source=='FILE': image.pack()
blend=r'C:\EmberEngine\content\models\crypt\column-base-v02.blend'
glb=r'C:\EmberEngine\content\models\crypt\column-base-v02.glb'
bpy.ops.wm.save_as_mainfile(filepath=blend)
bpy.ops.object.select_all(action='DESELECT')
mesh_objs=[o for o in scene.objects if o.type=='MESH']
for o in mesh_objs: o.select_set(True)
bpy.context.view_layer.objects.active=mesh_objs[0]
bpy.ops.export_scene.gltf(filepath=glb,export_format='GLB',use_selection=True,export_apply=True,export_texcoords=True,export_materials='EXPORT')
deps=bpy.context.evaluated_depsgraph_get(); tris=0
lo=[float('inf')]*3; hi=[float('-inf')]*3
for obj in mesh_objs:
    ev=obj.evaluated_get(deps); temp=ev.to_mesh(); temp.calc_loop_triangles(); tris+=len(temp.loop_triangles)
    for corner in temp.vertices:
        point=ev.matrix_world @ corner.co
        for axis in range(3): lo[axis]=min(lo[axis],point[axis]); hi[axis]=max(hi[axis],point[axis])
    ev.to_mesh_clear()
print('EXPORT_OK',glb,blend,'triangles',tris,'mesh_objects',len(mesh_objs),'dimensions',[round(hi[i]-lo[i],4) for i in range(3)])
print('PREVIEW_OK',out)

