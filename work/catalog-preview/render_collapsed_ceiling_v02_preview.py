import bpy, math
from mathutils import Vector
out=r'C:\Users\evari\.codex\visualizations\2026\10\25\01a173d0-1874-7980-a25d-ede55ba788b1\catalog-next-review\collapsed-ceiling-v02-preview.png'
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene
# Reuse the embedded gray stone photo texture from the approved tombstone.
bpy.ops.import_scene.gltf(filepath=r'C:\EmberEngine\content\models\crypt\ancient-tombstone-v03.glb')
mat=bpy.data.materials['Pedra cinza fotografica']
for obj in list(scene.objects): bpy.data.objects.remove(obj,do_unlink=True)

def unwrap(obj):
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active=obj
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.uv.smart_project(island_margin=.02); bpy.ops.object.mode_set(mode='OBJECT')

def slab(name,dims,loc,rot,bevel):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc,rotation=rot)
    obj=bpy.context.object; obj.name=name; obj.dimensions=dims; bpy.ops.object.transform_apply(location=False,rotation=False,scale=True); obj.data.materials.append(mat); unwrap(obj)
    b=obj.modifiers.new('Arestas quebradas','BEVEL'); b.width=bevel; b.segments=2; obj.modifiers.new('Normais de peso','WEIGHTED_NORMAL')
    return obj

def rubble(name,scale,loc,seed):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3,radius=1,location=loc)
    obj=bpy.context.object; obj.name=name; obj.scale=scale; bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    for idx,v in enumerate(obj.data.vertices):
        f=1.0 + .035*math.sin((idx+1)*(seed+2)*1.37) + .02*math.cos((idx+4)*(seed+1)*.91)
        v.co *= f
    obj.data.materials.append(mat); unwrap(obj)
    for p in obj.data.polygons: p.use_smooth=True
    return obj

# All pieces overlap and rest on the ground; no suspended beams or detached fragments.
rubble('Massa de pedra esquerda',(0.75,.38,.30),(-.68,0,.30),1)
rubble('Massa de pedra central',(0.78,.40,.34),(.05,-.02,.34),2)
rubble('Massa de pedra direita',(.66,.34,.27),(.78,.04,.27),3)
rubble('Fragmento frontal',(.55,.24,.20),(-.98,-.24,.20),4)
rubble('Fragmento lateral',(.45,.26,.20),(.98,-.17,.20),5)
slab('Laje quebrada inferior',(1.92,.72,.24),(-.12,0,.52),(.04,-.05,-.04),.07)
slab('Laje quebrada sobreposta',(1.18,.62,.24),(.47,.02,.74),(-.08,.17,.06),.055)
slab('Fragmento superior',(0.78,.48,.20),(-.28,-.02,.94),(.07,-.12,-.08),.045)
scene.render.engine='BLENDER_EEVEE'; scene.eevee.taa_render_samples=96
scene.render.resolution_x=960; scene.render.resolution_y=960; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.image_settings.color_mode='RGB'; scene.render.filepath=out
scene.world=bpy.data.worlds.new('Fundo cinza neutro'); scene.world.use_nodes=True
scene.world.node_tree.nodes.get('Background').inputs['Color'].default_value=(.34,.34,.34,1); scene.world.node_tree.nodes.get('Background').inputs['Strength'].default_value=.42
scene.view_settings.view_transform='AgX'; center=Vector((0,0,.57))
for name,loc,energy,size in [('key',(2,-4,4),650,3),('fill',(-4,-3,2),260,3),('rim',(2,3,4),400,2.5)]:
    bpy.ops.object.light_add(type='AREA',location=loc); light=bpy.context.object; light.name=name; light.data.energy=energy; light.data.shape='DISK'; light.data.size=size; light.rotation_euler=(center-Vector(light.location)).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(.65,-5.5,2.4)); cam=bpy.context.object; cam.rotation_euler=(center-Vector(cam.location)).to_track_quat('-Z','Y').to_euler(); cam.data.type='ORTHO'; cam.data.ortho_scale=3.4; scene.camera=cam
bpy.ops.render.render(write_still=True)
print('PREVIEW_OK',out)
