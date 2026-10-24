"""Studio contact sheet for the unintegrated Watchtower leafy-tree candidates."""
import bpy, math
from pathlib import Path
from mathutils import Vector
ROOT=Path('C:/EmberEngine'); OUT=ROOT/'content/models/watchtower-leafy-trees-v01'
bpy.ops.wm.read_factory_settings(use_empty=True)
s=bpy.context.scene; s.render.engine='BLENDER_EEVEE'; s.eevee.taa_render_samples=96
s.render.resolution_x=2100; s.render.resolution_y=1200; s.render.resolution_percentage=100
s.render.image_settings.file_format='PNG'; s.view_settings.view_transform='AgX'; s.view_settings.look='AgX - Medium High Contrast'
s.world=bpy.data.worlds.new('neutral studio'); s.world.color=(.14,.15,.16)
for i,kind in enumerate(('oak','willow','birch')):
    before=set(s.objects)
    bpy.ops.import_scene.gltf(filepath=str(OUT/f'watchtower-leafy-{kind}-v01.glb'))
    imported=[o for o in s.objects if o not in before and o.type=='MESH']
    obj=max(imported,key=lambda o:o.dimensions.z)
    obj.location.x=(i-1)*6.3
    obj.name=kind.title()
    # low neutral stage, independent of the production scene
    bpy.ops.mesh.primitive_cylinder_add(vertices=96,radius=2.25,depth=.12,location=(obj.location.x,0,-.06))
    p=bpy.context.object; p.name=kind.title()+' preview plinth'; p.data.materials.append(bpy.data.materials.new('matte warm-grey stone'))
    p.data.materials[0].diffuse_color=(.24,.23,.20,1); p.data.materials[0].use_nodes=True
    p.data.materials[0].node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=(.24,.23,.20,1)
    p.data.materials[0].node_tree.nodes.get('Principled BSDF').inputs['Roughness'].default_value=.9
    obj.location.z=0
def area(name,loc,power,size):
    d=bpy.data.lights.new(name,'AREA'); d.energy=power; d.shape='DISK'; d.size=size
    o=bpy.data.objects.new(name,d); s.collection.objects.link(o); o.location=loc; o.rotation_euler=(Vector((0,0,4))-o.location).to_track_quat('-Z','Y').to_euler()
area('large soft key',(-4,-8,13),1800,9); area('soft fill',(8,-3,9),1100,8); area('rim',(0,6,11),2100,7)
d=bpy.data.cameras.new('tree lineup'); c=bpy.data.objects.new('tree lineup',d); s.collection.objects.link(c); s.camera=c
c.location=(0,-22,10.0); c.rotation_euler=(Vector((0,0,4.5))-c.location).to_track_quat('-Z','Y').to_euler(); d.type='ORTHO'; d.ortho_scale=22
s.render.filepath=str(OUT/'watchtower-leafy-trees-preview.png')
bpy.ops.render.render(write_still=True)
