import bpy, math
from mathutils import Vector
from mathutils import Quaternion
out = r'C:\Users\evari\.codex\visualizations\2026\10\25\01a173d0-1874-7980-a25d-ede55ba788b1\catalog-next-review\ancient-tombstone-v02-preview.png'
tex = r'C:\EmberEngine\Attachments\watchtower\gravestone.jpg'
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
# One clear, readable headstone silhouette with a separate stone footing.
width, straight, radius, depth = 1.12, 1.28, .56, .25
profile = [(-width/2, 0), (width/2, 0), (width/2, straight)]
for i in range(1, 17):
    a = math.pi * i / 16
    profile.append((radius*math.cos(a), straight+radius*math.sin(a)))
profile.append((-width/2, 0))
# Remove duplicate left bottom if the arc already overlaps; retain ordered contour.
profile = profile[:-1]
n = len(profile)
verts = [(x,-depth/2,z) for x,z in profile] + [(x,depth/2,z) for x,z in profile]
faces = [tuple(range(n)), tuple(range(n,2*n))[::-1]]
for i in range(n):
    j=(i+1)%n
    faces.append((i,j,n+j,n+i))
mesh=bpy.data.meshes.new('Lápide antiga - pedra arqueada')
mesh.from_pydata(verts,[],faces); mesh.update()
stone=bpy.data.objects.new('Lápide antiga',mesh); scene.collection.objects.link(stone)
# Photographic gray gravestone surface, using the project's supplied texture.
mat=bpy.data.materials.new('Pedra cinza fotografica'); mat.diffuse_color=(.34,.36,.37,1); mat.use_nodes=True
nodes=mat.node_tree.nodes; bsdf=nodes.get('Principled BSDF'); bsdf.inputs['Roughness'].default_value=.92
image=bpy.data.images.load(tex,check_existing=True)
texnode=nodes.new('ShaderNodeTexImage'); texnode.image=image
mat.node_tree.links.new(texnode.outputs['Color'],bsdf.inputs['Base Color'])
mesh.materials.append(mat)
# Project UVs per face so the front/back read as a full upright stone slab.
uv=mesh.uv_layers.new(name='StoneUV')
for poly in mesh.polygons:
    normal=poly.normal
    if abs(normal.y) >= max(abs(normal.x),abs(normal.z)):
        ax1,ax2='x','z'
    elif abs(normal.x) >= abs(normal.z):
        ax1,ax2='y','z'
    else:
        ax1,ax2='x','y'
    coords=[]
    for vi in poly.vertices:
        v=mesh.vertices[vi].co
        d={'x':v.x,'y':v.y,'z':v.z}
        coords.append((d[ax1],d[ax2]))
    xs=[p[0] for p in coords]; ys=[p[1] for p in coords]
    xlo,xhi=min(xs),max(xs); ylo,yhi=min(ys),max(ys)
    dx=max(xhi-xlo,.001); dy=max(yhi-ylo,.001)
    for li,vi in enumerate(poly.loop_indices):
        x,y=coords[li]; uv.data[vi].uv=((x-xlo)/dx,(y-ylo)/dy)
stone_bevel=stone.modifiers.new('Bordas gastas e arredondadas','BEVEL'); stone_bevel.width=.035; stone_bevel.segments=3
stone.modifiers.new('Normais de peso','WEIGHTED_NORMAL')
# Broad low rectangular foot; no symbols or extra ornament.
bpy.ops.mesh.primitive_cube_add(size=1, location=(0,0,.13))
foot=bpy.context.object; foot.name='Base baixa da lápide'; foot.dimensions=(1.50,.54,.26); bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
foot.data.materials.append(mat)
fb=foot.modifiers.new('Cantos suaves','BEVEL'); fb.width=.035; fb.segments=3
foot.modifiers.new('Normais de peso','WEIGHTED_NORMAL')
uv=foot.data.uv_layers.new(name='StoneUV')
for poly in foot.data.polygons:
    normal=poly.normal
    if abs(normal.z) >= max(abs(normal.x),abs(normal.y)): ax1,ax2='x','y'
    elif abs(normal.x) >= abs(normal.y): ax1,ax2='y','z'
    else: ax1,ax2='x','z'
    coords=[]
    for vi in poly.vertices:
        v=foot.data.vertices[vi].co; d={'x':v.x,'y':v.y,'z':v.z}; coords.append((d[ax1],d[ax2]))
    xs=[p[0] for p in coords]; ys=[p[1] for p in coords]; xlo,xhi=min(xs),max(xs); ylo,yhi=min(ys),max(ys)
    for li,vi in enumerate(poly.loop_indices): uv.data[vi].uv=((coords[li][0]-xlo)/max(xhi-xlo,.001),(coords[li][1]-ylo)/max(yhi-ylo,.001))
# Neutral studio render for isolated review; no game/browser launch.
scene.render.engine='BLENDER_EEVEE'; scene.eevee.taa_render_samples=96
scene.render.resolution_x=960; scene.render.resolution_y=960; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.image_settings.color_mode='RGB'; scene.render.filepath=out
scene.world=bpy.data.worlds.new('Neutral gray preview'); scene.world.use_nodes=True
scene.world.node_tree.nodes.get('Background').inputs['Color'].default_value=(.35,.35,.35,1)
scene.world.node_tree.nodes.get('Background').inputs['Strength'].default_value=.45
scene.view_settings.view_transform='AgX'
center=Vector((0,0,1.0)); span=2.0
for name,loc,energy,size in [('key',(2,-4,5),650,3),('fill',(-4,-3,2),260,3),('rim',(2,3,4),400,2.5)]:
    bpy.ops.object.light_add(type='AREA',location=(loc[0],loc[1],loc[2]))
    light=bpy.context.object; light.name=name; light.data.energy=energy; light.data.shape='DISK'; light.data.size=size
    light.rotation_euler=(center-Vector(light.location)).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(.7,-6.4,2.2)); cam=bpy.context.object
cam.rotation_euler=(center-Vector(cam.location)).to_track_quat('-Z','Y').to_euler(); cam.data.type='ORTHO'; cam.data.ortho_scale=2.65; scene.camera=cam
bpy.ops.render.render(write_still=True)
print('PREVIEW_OK',out)
