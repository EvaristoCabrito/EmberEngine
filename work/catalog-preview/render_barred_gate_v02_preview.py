import bpy, math
from mathutils import Vector
out=r'C:\Users\evari\.codex\visualizations\2026\10\25\01a173d0-1874-7980-a25d-ede55ba788b1\catalog-next-review\barred-gate-v02-preview.png'
stone_img=r'C:\EmberEngine\Attachments\watchtower\gravestone.jpg'
iron_img=r'C:\EmberEngine\Attachments\watchtower\metal.jpg'
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene

def photo_mat(name,path,metallic,roughness):
    m=bpy.data.materials.new(name); m.diffuse_color=(.3,.32,.34,1); m.use_nodes=True
    nodes=m.node_tree.nodes; bs=nodes.get('Principled BSDF'); bs.inputs['Metallic'].default_value=metallic; bs.inputs['Roughness'].default_value=roughness
    im=bpy.data.images.load(path,check_existing=True); n=nodes.new('ShaderNodeTexImage'); n.image=im
    m.node_tree.links.new(n.outputs['Color'],bs.inputs['Base Color'])
    return m
stone=photo_mat('Calcario cinza antigo',stone_img,0,.9)
iron=photo_mat('Ferro oxidado',iron_img,.62,.78)

def box(name,dims,loc,mat,bevel=0):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc)
    o=bpy.context.object; o.name=name; o.dimensions=dims; bpy.ops.object.transform_apply(location=False,rotation=False,scale=True); o.data.materials.append(mat)
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.uv.smart_project(island_margin=.015); bpy.ops.object.mode_set(mode='OBJECT')
    if bevel:
        b=o.modifiers.new('Arestas quebradas','BEVEL'); b.width=bevel; b.segments=3; o.modifiers.new('Normais de peso','WEIGHTED_NORMAL')
    return o

# Stone doorway frame, with the gate visibly seated between both jambs.
box('Jamba esquerda de pedra',(.32,.40,2.70),(-1.03,0,1.35),stone,.035)
box('Jamba direita de pedra',(.32,.40,2.70),(1.03,0,1.35),stone,.035)
box('Verga superior de pedra',(2.38,.40,.34),(0,0,2.80),stone,.035)
box('Soleira de pedra',(2.38,.48,.22),(0,0,.11),stone,.025)
# Iron gate: eight vertical bars, two cross rails and a compact latch plate.
bar_y=-.11
for i in range(8):
    x=-.77+i*(1.54/7)
    bpy.ops.mesh.primitive_cylinder_add(vertices=16,radius=.034,depth=2.48,location=(x,bar_y,1.46))
    o=bpy.context.object; o.name=f'Barra vertical {i+1}'; o.data.materials.append(iron)
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.uv.smart_project(island_margin=.015); bpy.ops.object.mode_set(mode='OBJECT')
    b=o.modifiers.new('Borda de ferro','BEVEL'); b.width=.006; b.segments=2; o.modifiers.new('Normais de peso','WEIGHTED_NORMAL')
box('Travessa inferior de ferro',(1.62,.10,.10),(0,bar_y,.40),iron,.012)
box('Travessa central de ferro',(1.62,.10,.10),(0,bar_y,1.43),iron,.012)
box('Travessa superior de ferro',(1.62,.10,.10),(0,bar_y,2.46),iron,.012)
box('Placa da tranca',(.16,.12,.28),(.78,-.18,1.52),iron,.018)
bpy.ops.mesh.primitive_cylinder_add(vertices=16,radius=.018,depth=.10,location=(.78,-.27,1.47),rotation=(math.pi/2,0,0))
bpy.context.object.name='Pino da tranca'; bpy.context.object.data.materials.append(iron)

scene.render.engine='BLENDER_EEVEE'; scene.eevee.taa_render_samples=96
scene.render.resolution_x=960; scene.render.resolution_y=960; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.image_settings.color_mode='RGB'; scene.render.filepath=out
scene.world=bpy.data.worlds.new('Fundo cinza neutro'); scene.world.use_nodes=True
scene.world.node_tree.nodes.get('Background').inputs['Color'].default_value=(.34,.34,.34,1)
scene.world.node_tree.nodes.get('Background').inputs['Strength'].default_value=.4
scene.view_settings.view_transform='AgX'
center=Vector((0,0,1.52))
for name,loc,energy,size in [('key',(2,-5,5),800,3),('fill',(-4,-4,2),320,3.5),('rim',(1,3,4),450,2.5)]:
    bpy.ops.object.light_add(type='AREA',location=loc); light=bpy.context.object; light.name=name; light.data.energy=energy; light.data.shape='DISK'; light.data.size=size; light.rotation_euler=(center-Vector(light.location)).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(.9,-6.8,2.25)); cam=bpy.context.object; cam.rotation_euler=(center-Vector(cam.location)).to_track_quat('-Z','Y').to_euler(); cam.data.type='ORTHO'; cam.data.ortho_scale=3.85; scene.camera=cam
bpy.ops.render.render(write_still=True)
print('PREVIEW_OK',out)

