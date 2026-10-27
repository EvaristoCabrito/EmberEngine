import bpy, math
from mathutils import Vector
out=r'C:\Users\evari\.codex\visualizations\2026\10\25\01a173d0-1874-7980-a25d-ede55ba788b1\catalog-next-review\chain-ring-v02-preview.png'
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene
# Reuse the embedded iron photo texture from the approved gate asset.
bpy.ops.import_scene.gltf(filepath=r'C:\EmberEngine\content\models\crypt\barred-gate-v02.glb')
mat=bpy.data.materials['Ferro oxidado']
for obj in list(scene.objects): bpy.data.objects.remove(obj,do_unlink=True)

def unwrap(obj):
    bpy.context.view_layer.objects.active=obj; obj.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.uv.smart_project(island_margin=.02); bpy.ops.object.mode_set(mode='OBJECT')

def box(name,dims,loc,bevel):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc); o=bpy.context.object; o.name=name; o.dimensions=dims; bpy.ops.object.transform_apply(location=False,rotation=False,scale=True); o.data.materials.append(mat); unwrap(o)
    m=o.modifiers.new('Bordas arredondadas pelo uso','BEVEL'); m.width=bevel; m.segments=3; o.modifiers.new('Normais de peso','WEIGHTED_NORMAL')

def torus(name,loc,rot,major,minor):
    bpy.ops.mesh.primitive_torus_add(major_radius=major,minor_radius=minor,major_segments=32,minor_segments=10,location=loc,rotation=rot)
    o=bpy.context.object; o.name=name; o.data.materials.append(mat); unwrap(o)

# A bolted iron wall plate physically anchors the ring.
box('Chapa de ancoragem',(0.28,.10,.34),(0,0,1.66),.018)
for side in (-1,1):
    bpy.ops.mesh.primitive_cylinder_add(vertices=16,radius=.024,depth=.065,location=(side*.09,-.075,1.74),rotation=(math.pi/2,0,0))
    rivet=bpy.context.object; rivet.name='Rebite da chapa'; rivet.data.materials.append(mat)
    m=rivet.modifiers.new('Borda do rebite','BEVEL'); m.width=.004; m.segments=2; rivet.modifiers.new('Normais de peso','WEIGHTED_NORMAL')
    unwrap(rivet)
# Large hanging ring joined visually to the plate, followed by interlinked chain loops.
torus('Argola de ferro',(0,-.035,1.46),(math.pi/2,0,0),.155,.026)
for i in range(12):
    z=1.215-i*.10
    rot=(math.pi/2,0,0) if i%2==0 else (0,math.pi/2,0)
    torus(f'Elos da corrente {i+1}',(0,-.04,z),rot,.052,.014)
scene.render.engine='BLENDER_EEVEE'; scene.eevee.taa_render_samples=96
scene.render.resolution_x=960; scene.render.resolution_y=960; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.image_settings.color_mode='RGB'; scene.render.filepath=out
scene.world=bpy.data.worlds.new('Fundo cinza neutro'); scene.world.use_nodes=True
scene.world.node_tree.nodes.get('Background').inputs['Color'].default_value=(.34,.34,.34,1); scene.world.node_tree.nodes.get('Background').inputs['Strength'].default_value=.42
scene.view_settings.view_transform='AgX'; center=Vector((0,0,.95))
for name,loc,energy,size in [('key',(1,-3,4),450,2),('fill',(-3,-2,1.5),210,2.5),('rim',(1,2,3),280,2)]:
    bpy.ops.object.light_add(type='AREA',location=loc); light=bpy.context.object; light.name=name; light.data.energy=energy; light.data.shape='DISK'; light.data.size=size; light.rotation_euler=(center-Vector(light.location)).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(.55,-3.6,1.45)); cam=bpy.context.object; cam.rotation_euler=(center-Vector(cam.location)).to_track_quat('-Z','Y').to_euler(); cam.data.type='ORTHO'; cam.data.ortho_scale=1.95; scene.camera=cam
bpy.ops.render.render(write_still=True)
print('PREVIEW_OK',out)


