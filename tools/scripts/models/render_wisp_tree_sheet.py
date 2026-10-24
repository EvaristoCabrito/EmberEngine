"""Render an offline 3x3 studio inspection sheet of the new tree GLBs with Blender."""
import math
from pathlib import Path
import bpy
from mathutils import Vector

ROOT = Path('C:/EmberEngine')
ASSETS = ROOT / 'content/models/wisp-forest-revisited'
OUT = Path(r'C:/Users/evari/.codex/visualizations/2026/10/24/01a16c18-9fe6-7212-a2e8-e2e0c6a23af1/wisp-forest-revisited-trees.png')
NAMES = ['weeping-willow','giant-dead-oak','hollow-snag','uprooted-leaning','lightning-split','spiral-twisted','tall-burnt-pine','broken-crown','root-arch']

bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene
scene.render.engine='BLENDER_EEVEE'
scene.render.resolution_x=2400; scene.render.resolution_y=1800; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.film_transparent=False
scene.world=bpy.data.worlds.new('neutral studio world')
scene.world.color=(0.075,0.082,0.086)
scene.view_settings.view_transform='AgX'
scene.view_settings.look='AgX - Medium High Contrast'
scene.camera=None

mesh_groups=[]
for i,name in enumerate(NAMES):
    before=set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(ASSETS/f'{name}-v02.glb'))
    new=[o for o in bpy.data.objects if o not in before and o.type=='MESH']
    bpy.context.view_layer.update()
    corners=[o.matrix_world @ Vector(c) for o in new for c in o.bound_box]
    lo=Vector((min(v.x for v in corners),min(v.y for v in corners),min(v.z for v in corners)))
    hi=Vector((max(v.x for v in corners),max(v.y for v in corners),max(v.z for v in corners)))
    col=(i%3-1)*10.0; row=(1-i//3)*11.0
    delta=Vector((col-(lo.x+hi.x)/2,row-(lo.y+hi.y)/2,-lo.z))
    for o in new: o.location += delta
    mesh_groups.extend(new)
    # The textured forest surface grounds the model without obscuring its roots.
    curve=bpy.data.curves.new('label','FONT'); curve.body=name.replace('-',' '); curve.size=.32; curve.align_x='CENTER'
    label=bpy.data.objects.new('label '+name,curve); scene.collection.objects.link(label); label.location=(col,row-3.0,.012)
    label.data.materials.append(bpy.data.materials.new('label material')); label.data.materials[-1].diffuse_color=(.76,.75,.70,1)

bpy.ops.mesh.primitive_plane_add(size=50, location=(0,0,-.025))
floor=bpy.context.object; floor.name='photographic forest floor'
ground=bpy.data.materials.new('forest floor | inspection only'); ground.use_nodes=True
image=bpy.data.images.load(str(ROOT/'content/models/textures/wisp-leaf-litter-v01.jpg'),check_existing=True)
tex=ground.node_tree.nodes.new('ShaderNodeTexImage'); tex.image=image
bsdf=ground.node_tree.nodes.get('Principled BSDF'); ground.node_tree.links.new(tex.outputs['Color'],bsdf.inputs['Base Color'])
bsdf.inputs['Roughness'].default_value=.98; floor.data.materials.append(ground)

def aim(obj,point): obj.rotation_euler=(Vector(point)-obj.location).to_track_quat('-Z','Y').to_euler()
cam_data=bpy.data.cameras.new('inspection camera'); cam=bpy.data.objects.new('inspection camera',cam_data); scene.collection.objects.link(cam)
cam.location=(0,-40,65); aim(cam,(0,0,3)); cam.data.type='ORTHO'; cam.data.ortho_scale=38; scene.camera=cam

for name,loc,power,size in [('key',(-15,-20,28),5200,18),('fill',(16,-2,20),3100,15),('rim',(0,16,24),4200,12)]:
    ld=bpy.data.lights.new(name,'AREA'); ld.energy=power; ld.shape='DISK'; ld.size=size
    lo=bpy.data.objects.new(name,ld); scene.collection.objects.link(lo); lo.location=loc; aim(lo,(0,0,3))

scene.render.filepath=str(OUT)
bpy.ops.render.render(write_still=True)
print('[ok] rendered',OUT)
