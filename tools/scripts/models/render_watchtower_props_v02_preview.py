import bpy
from mathutils import Vector
from pathlib import Path
root=Path(__file__).resolve().parents[3]
assets=root/'content/models/watchtower-v02'
output=assets/'watchtower-props-preview.png'
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
models=['watchtower-signal-brazier-v02.glb','watchtower-arms-rack-v02.glb','watchtower-command-table-v02.glb']
for i,name in enumerate(models):
 bpy.ops.import_scene.gltf(filepath=str(assets/name)); imported=list(bpy.context.selected_objects)
 for o in imported:o.location.x += (i-1)*3.3
# ground plane
bpy.ops.mesh.primitive_plane_add(size=16,location=(0,0,-0.02));ground=bpy.context.object
mat=bpy.data.materials.new('Neutral limestone preview ground');mat.diffuse_color=(0.23,0.23,0.22,1);ground.data.materials.append(mat)
# warm, broad studio lighting
world=bpy.context.scene.world;world.color=(0.12,0.12,0.12)
def point(obj,target):obj.rotation_euler=(Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler()
for loc,power,size in [((-3,-4,6),900,5),((4,1,5),650,4),((0,5,4),400,3)]:
 bpy.ops.object.light_add(type='AREA',location=loc);l=bpy.context.object;l.data.energy=power;l.data.shape='DISK';l.data.size=size;point(l,(0,1,0))
bpy.ops.object.camera_add(location=(6,-10,6.2));cam=bpy.context.object;point(cam,(0,1,0));cam.data.type='ORTHO';cam.data.ortho_scale=10;bpy.context.scene.camera=cam
scene=bpy.context.scene;scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1600;scene.render.resolution_y=740;scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX';scene.render.image_settings.file_format='PNG';scene.render.filepath=str(output)
bpy.ops.render.render(write_still=True);print(output)


