"""Offline Blender layout preview using Ember's exported map coordinates and registered GLBs."""
import json, math
from pathlib import Path
import bpy, bmesh
from mathutils import Vector

ROOT=Path('C:/EmberEngine')
PUBLIC=ROOT/'content/assets'
KIT=ROOT/'content/models/wisp-forest-revisited'
MAP=ROOT/'content/maps/ember/wisp-forest010.json'
OUT=Path(r'C:/Users/evari/.codex/visualizations/2026/10/24/01a16c18-9fe6-7212-a2e8-e2e0c6a23af1/wisp-forest-revisited-map.png')

bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene; scene.render.engine='BLENDER_EEVEE'
scene.render.resolution_x=2400; scene.render.resolution_y=1600; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.view_settings.view_transform='AgX'; scene.view_settings.look='AgX - Medium High Contrast'
scene.world=bpy.data.worlds.new('neutral Wisp map studio'); scene.world.color=(.035,.04,.038)
d=json.loads(MAP.read_text(encoding='utf-8'))['draft']; cols,rows=d['cols'],d['rows']; R=1.0; ox=(math.sqrt(3)*(cols-.5))/2; oz=.75*(rows-1)

def photo_mat(name,path,roughness):
    m=bpy.data.materials.new(name); m.use_nodes=True; bsdf=m.node_tree.nodes.get('Principled BSDF'); bsdf.inputs['Roughness'].default_value=roughness
    tex=m.node_tree.nodes.new('ShaderNodeTexImage'); tex.image=bpy.data.images.load(str(path),check_existing=True)
    m.node_tree.links.new(tex.outputs['Color'],bsdf.inputs['Base Color']); return m

ground_materials=[
    photo_mat('Wisp | fallen leaves',ROOT/'content/models/textures/wisp-leaf-litter-v01.jpg',.98),
    photo_mat('Wisp | exposed roots',ROOT/'content/models/textures/wisp-root-soil-v01.jpg',.98),
    photo_mat('Wisp | ash and leaves',ROOT/'content/models/textures/wisp-charred-leaf-v01.jpg',.99),
]
variant_material={62:0,63:1,64:2}
verts=[]; faces=[]; face_mats=[]; uv_faces=[]
for r in range(rows):
  for c in range(cols):
    idx=r*cols+c
    if d['tiles'][idx]=='void': continue
    cx=math.sqrt(3)*(c+.5*(r&1))-ox; cy=1.5*r-oz
    base=len(verts); corners=[(math.sin(k*math.pi/3),math.cos(k*math.pi/3)) for k in range(6)]
    verts.extend([(cx+px,cy+py,.005) for px,py in corners])
    mat=variant_material.get(d['tileVariants'][idx],0)
    f=list(range(base,base+6)); faces.append(f); face_mats.append(mat)
    uv_faces.append((f,[(.5+px*.48,.5+py*.48) for px,py in corners],mat))

mesh=bpy.data.meshes.new('irregular hex forest floor'); mesh.from_pydata(verts,[],faces)
for m in ground_materials: mesh.materials.append(m)
for p,mi in zip(mesh.polygons,face_mats): p.material_index=mi
bm=bmesh.new(); bm.from_mesh(mesh); bmesh.ops.recalc_face_normals(bm,faces=bm.faces); bm.to_mesh(mesh); bm.free()
uv=mesh.uv_layers.new(name='UVMap')
for p,(ids,coords,mi) in zip(mesh.polygons,uv_faces):
    for loop_id,co in zip(p.loop_indices,coords): uv.data[loop_id].uv=co
floor=bpy.data.objects.new('irregular forest floor',mesh); scene.collection.objects.link(floor)

def model_name(dec):
    name=dec['model']
    return (name+'-v02') if (KIT/(name+'-v02.glb')).exists() else (name+'-v01')

decorations=json.loads((Path(r'C:/Users/evari/.codex/visualizations/2026/10/24/01a16c18-9fe6-7212-a2e8-e2e0c6a23af1/wisp-forest-revisited-v03-scene.json')).read_text(encoding='utf-8'))['decor']
for dec in decorations:
    if dec['kind']=='model':
        stem=model_name(dec)
        before=set(bpy.data.objects); bpy.ops.import_scene.gltf(filepath=str(KIT/(stem+'.glb')))
        new=[o for o in bpy.data.objects if o not in before]
        for o in new: o.location += Vector((dec['x'],dec['z'],.05))
    elif dec['kind']=='card':
        imgpath=PUBLIC/dec['image'].lstrip('/').replace('/','\\')
        img=bpy.data.images.load(str(imgpath),check_existing=True)
        mat=bpy.data.materials.new('preserved '+dec['id']); mat.use_nodes=True
        nt=mat.node_tree; bs=nt.nodes.get('Principled BSDF'); tx=nt.nodes.new('ShaderNodeTexImage'); tx.image=img
        nt.links.new(tx.outputs['Color'],bs.inputs['Base Color']); nt.links.new(tx.outputs['Alpha'],bs.inputs['Alpha'])
        bs.inputs['Roughness'].default_value=.95
        mat.surface_render_method='DITHERED'
        bpy.ops.mesh.primitive_plane_add(size=1,location=(dec['x'],dec['z']-.04,dec['h']/2+.06),rotation=(math.pi/2,0,0))
        card=bpy.context.object; card.name=dec['id']; card.scale=(dec['w'],dec['h'],1); card.data.materials.append(mat)

def aim(o,p): o.rotation_euler=(Vector(p)-o.location).to_track_quat('-Z','Y').to_euler()
camd=bpy.data.cameras.new('Wisp forest map camera'); cam=bpy.data.objects.new('Wisp forest map camera',camd); scene.collection.objects.link(cam)
cam.location=(0,-30,85); aim(cam,(0,0,5.0)); camd.type='ORTHO'; camd.ortho_scale=54; scene.camera=cam
for name,loc,power,size in [('key',(-18,-22,32),6200,18),('fill',(20,-8,25),3200,16),('rim',(0,18,28),4400,14)]:
    ld=bpy.data.lights.new(name,'AREA'); ld.energy=power; ld.shape='DISK'; ld.size=size
    light=bpy.data.objects.new(name,ld); scene.collection.objects.link(light); light.location=loc; aim(light,(0,0,3))
scene.render.filepath=str(OUT); bpy.ops.render.render(write_still=True)
print(f'[ok] rendered map draft {d["id"]}: {len(decorations)} preserved/new decorations -> {OUT}')
