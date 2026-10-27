import bpy, math
from mathutils import Vector
blend = r'C:\EmberEngine\content\models\crypt\ancient-tombstone-v03.blend'
glb = r'C:\EmberEngine\content\models\crypt\ancient-tombstone-v03.glb'
tex = r'C:\EmberEngine\Attachments\watchtower\gravestone.jpg'
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
width, straight, radius, depth = 1.12, 1.28, .56, .25
profile = [(-width/2, 0), (width/2, 0), (width/2, straight)]
for i in range(1, 17):
    a = math.pi * i / 16
    profile.append((radius*math.cos(a), straight+radius*math.sin(a)))
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
mat=bpy.data.materials.new('Pedra cinza fotografica'); mat.diffuse_color=(.34,.36,.37,1); mat.use_nodes=True
nodes=mat.node_tree.nodes; bsdf=nodes.get('Principled BSDF'); bsdf.inputs['Roughness'].default_value=.92
image=bpy.data.images.load(tex,check_existing=True); image.pack()
texnode=nodes.new('ShaderNodeTexImage'); texnode.image=image
mat.node_tree.links.new(texnode.outputs['Color'],bsdf.inputs['Base Color'])
mesh.materials.append(mat)
uv=mesh.uv_layers.new(name='StoneUV')
for poly in mesh.polygons:
    normal=poly.normal
    if abs(normal.y) >= max(abs(normal.x),abs(normal.z)): ax1,ax2='x','z'
    elif abs(normal.x) >= abs(normal.z): ax1,ax2='y','z'
    else: ax1,ax2='x','y'
    coords=[]
    for vi in poly.vertices:
        v=mesh.vertices[vi].co; d={'x':v.x,'y':v.y,'z':v.z}; coords.append((d[ax1],d[ax2]))
    xs=[p[0] for p in coords]; ys=[p[1] for p in coords]; xlo,xhi=min(xs),max(xs); ylo,yhi=min(ys),max(ys)
    for li,vi in enumerate(poly.loop_indices): uv.data[vi].uv=((coords[li][0]-xlo)/max(xhi-xlo,.001),(coords[li][1]-ylo)/max(yhi-ylo,.001))
stone.modifiers.new('Bordas gastas e arredondadas','BEVEL').width=.035
stone.modifiers[-1].segments=3
stone.modifiers.new('Normais de peso','WEIGHTED_NORMAL')
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
# Preserve an editable Blender source with the photo texture packed inside.
bpy.ops.wm.save_as_mainfile(filepath=blend)
# Export only the two visible 3D mesh objects into the game asset.
bpy.ops.object.select_all(action='DESELECT')
stone.select_set(True); foot.select_set(True); bpy.context.view_layer.objects.active=stone
bpy.ops.export_scene.gltf(filepath=glb, export_format='GLB', use_selection=True, export_apply=True, export_texcoords=True, export_materials='EXPORT')
# Report exact evaluated mesh size for catalog metadata.
deps=bpy.context.evaluated_depsgraph_get(); tris=0
for obj in (stone,foot):
    ev=obj.evaluated_get(deps); temp=ev.to_mesh(); temp.calc_loop_triangles(); tris += len(temp.loop_triangles); ev.to_mesh_clear()
print('EXPORT_OK',glb,blend,'triangles',tris,'dimensions',stone.dimensions,foot.dimensions)
