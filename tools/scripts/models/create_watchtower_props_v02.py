"""Photoreal Watchtower props v02; separate assets, leaving v01 untouched."""
import bpy, math
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'content/models/watchtower-v02'; TEX=ROOT/'content/models/textures'
OUT.mkdir(parents=True,exist_ok=False); bpy.context.preferences.filepaths.save_version=0

def clear():
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
    for m in list(bpy.data.materials): bpy.data.materials.remove(m)

def mat(name,image,metal=0.0,rough=.78):
    m=bpy.data.materials.new(name);m.use_nodes=True;n=m.node_tree.nodes;links=m.node_tree.links
    bs=n.get('Principled BSDF'); tex=n.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(TEX/image),check_existing=True)
    links.new(tex.outputs['Color'],bs.inputs['Base Color']);bs.inputs['Metallic'].default_value=metal;bs.inputs['Roughness'].default_value=rough
    return m

def cube(name,loc,dims,material,bevel=.025):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.name=name;o.dimensions=dims
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(material)
    if bevel:
        b=o.modifiers.new('Rounded worn edges','BEVEL');b.width=bevel;b.segments=3
        o.modifiers.new('Weighted surface normals','WEIGHTED_NORMAL')
    return o

def cylinder(name,loc,r,depth,material,segments=40,r_top=None,bevel=.018):
    if r_top is None:bpy.ops.mesh.primitive_cylinder_add(vertices=segments,radius=r,depth=depth,location=loc)
    else:bpy.ops.mesh.primitive_cone_add(vertices=segments,radius1=r,radius2=r_top,depth=depth,location=loc)
    o=bpy.context.object;o.name=name;o.data.materials.append(material)
    if bevel:
        b=o.modifiers.new('Soft forged edges','BEVEL');b.width=bevel;b.segments=2
        o.modifiers.new('Weighted normals','WEIGHTED_NORMAL')
    return o

def rod(name,a,b,r,material,segments=16):
    a,b=Vector(a),Vector(b);d=b-a
    bpy.ops.mesh.primitive_cylinder_add(vertices=segments,radius=r,depth=d.length,location=(a+b)/2)
    o=bpy.context.object;o.name=name;o.rotation_mode='QUATERNION';o.rotation_quaternion=d.to_track_quat('Z','Y');o.data.materials.append(material)
    bevel=o.modifiers.new('Edge rounding','BEVEL');bevel.width=min(.012,r*.28);bevel.segments=2
    o.modifiers.new('Weighted normals','WEIGHTED_NORMAL');return o

def torus(name,loc,major,minor,material,majorseg=48,minorseg=12):
    bpy.ops.mesh.primitive_torus_add(major_radius=major,minor_radius=minor,major_segments=majorseg,minor_segments=minorseg,location=loc)
    o=bpy.context.object;o.name=name;o.data.materials.append(material);return o

def lathe_bowl(name,profile,material,segments=64):
    verts=[];faces=[]
    for r,z in profile:
        for i in range(segments):
            a=math.tau*i/segments;verts.append((r*math.cos(a),r*math.sin(a),z))
    for ring in range(len(profile)-1):
        a0=ring*segments;b0=(ring+1)*segments
        for i in range(segments):
            j=(i+1)%segments;faces.append((a0+i,a0+j,b0+j,b0+i))
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.materials.append(material);mesh.update()
    o=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(o);bpy.context.view_layer.objects.active=o;o.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.normals_make_consistent(inside=False);bpy.ops.object.mode_set(mode='OBJECT');o.select_set(False)
    return o

def blade(name,center,scale,material):
    # Double-edged spearhead with lenticular cross-section and a narrow socket.
    x,y,z=center;w,h,t=scale
    outline=[(-.11,0),(-.27,.27),(-.43,.66),(-.38,.83),(0,1),(.38,.83),(.43,.66),(.27,.27),(.11,0)]
    verts=[]
    for xx,zz in outline:verts.append((x+xx*w,y,z+zz*h))
    for xx,zz in outline:verts.append((x+xx*w*.55,y-t,z+zz*h))
    faces=[];n=len(outline)
    faces.append(tuple(range(n)));faces.append(tuple(range(n,2*n)))
    for i in range(n):j=(i+1)%n;faces.append((i,j,n+j,n+i))
    me=bpy.data.meshes.new(name);me.from_pydata(verts,[],faces);me.materials.append(material);me.update();o=bpy.data.objects.new(name,me);bpy.context.collection.objects.link(o)
    o.modifiers.new('Blade bevel','BEVEL').width=.012;o.modifiers.new('Weighted normals','WEIGHTED_NORMAL');return o

def finish(name):
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    bpy.ops.object.select_all(action='DESELECT')
    for o in meshes:o.select_set(True)
    bpy.context.view_layer.objects.active=meshes[0];bpy.ops.object.join();o=bpy.context.object;o.name=name
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(island_margin=.006);bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/(name+'.blend')))
    bpy.ops.export_scene.gltf(filepath=str(OUT/(name+'.glb')),export_format='GLB',use_selection=True,export_apply=True)

def signal_brazier():
    clear();iron=mat('Smoke-darkened hand-forged iron','wt-iron.jpg',.78,.45);stone=mat('Carved weathered limestone','wt-limestone.jpg',0,.87)
    cylinder('Octagonal stone plinth',(0,0,.10),.50,.20,stone,12)
    cylinder('Foot ring',(0,0,.23),.35,.10,iron,32)
    cylinder('Tapered carved pedestal',(0,0,.55),.27,.60,stone,20,r_top=.18)
    torus('Pedestal iron collar lower',(0,0,.31),.29,.026,iron)
    torus('Pedestal iron collar upper',(0,0,.84),.22,.026,iron)
    cylinder('Central iron standard',(0,0,1.00),.075,.42,iron,24)
    # Three swept legs support the open basin; collars and rivets retain the bowl.
    for i in range(3):
        a=math.tau*i/3;ux,uy=math.cos(a),math.sin(a)
        rod('Splayed forged leg',(ux*.43,uy*.43,.24),(ux*.26,uy*.26,1.17),.044,iron,16)
        rod('Leg brace',(ux*.43,uy*.43,.27),(ux*.25,uy*.25,.65),.026,iron,12)
    # Hollow, 64-sided flared basin with an inner surface and rolled rim.
    profile=[(.13,1.08),(.16,1.12),(.28,1.24),(.38,1.38),(.43,1.48),(.40,1.51),(.36,1.45),(.31,1.34),(.20,1.22),(.10,1.18)]
    lathe_bowl('Hollow flared signal bowl',profile,iron,64)
    torus('Rolled lip',(0,0,1.49),.415,.028,iron,64,12)
    torus('Bowl reinforcing band',(0,0,1.26),.29,.022,iron,56,10)
    torus('Bowl lower rivet band',(0,0,1.18),.22,.018,iron,48,8)
    # Discreet hand-set rivets around the bowl and vertical grate bars in its opening.
    for i in range(12):
        a=math.tau*i/12;x,y=math.cos(a)*.39,math.sin(a)*.39
        cylinder('Bowl rim rivet',(x,y,1.43),.018,.025,iron,12,.018,0)
    for i in range(8):
        a=math.tau*i/8
        rod('Removable grate spoke',(math.cos(a)*.08,math.sin(a)*.08,1.52),(math.cos(a)*.36,math.sin(a)*.36,1.52),.012,iron,8)
    finish('watchtower-signal-brazier-v02')

def arms_rack():
    clear();iron=mat('Dark forged iron fittings','wt-iron.jpg',.78,.48);oak=mat('Oiled dark oak','wt-dark-oak.jpg',0,.82);pine=mat('Pine end grain','wt-pine.jpg',0,.86)
    # Mortised, braced timber trestle with a notched upper beam.
    for x in (-.67,.67):
        cube('Broad timber foot',(x,0,.11),(.18,1.55,.22),oak,.035)
        rod('Front splayed upright',(x,-.58,.20),(x*.79,-.39,1.62),.072,oak)
        rod('Rear splayed upright',(x,.58,.20),(x*.79,.39,1.62),.072,oak)
        cube('Iron upright strap',(x*.84,0,.97),(.055,.10,.63),iron,.012)
    cube('Lower weapon rest',(0,0,.58),(1.42,1.38,.15),pine,.035)
    cube('Notched upper rail',(0,0,1.52),(1.58,.22,.22),oak,.035)
    # Visible iron-lined notches and five individually turned ash shafts.
    for i in range(5):
        x=-.52+i*.26
        cube('Iron lined shaft notch',(x,-.005,1.645),(.055,.235,.025),iron,.008)
        rod('Ash spear shaft',(x,0,.42),(x,0,2.30),.026,pine,12)
        cylinder('Lower steel ferrule',(x,0,.49),.043,.12,iron,16,.035)
        rod('Head socket',(x,0,2.27),(x,0,2.43),.035,iron,12)
        if i in (1,3):blade('Forged halberd head',(x,0,2.38),(.66,.58,.026),iron)
        else:blade('Leaf spear head',(x,0,2.40),(.32,.43,.024),iron)
        cylinder('Butt cap',(x,0,.38),.033,.08,iron,12,.025)
    for x in (-.61,.61):
        for z in (.35,1.48):cylinder('Hand-set trestle peg',(x,0,z),.026,.025,iron,12,.026,0)
    finish('watchtower-arms-rack-v02')

def command_table():
    clear();iron=mat('Blackened iron straps','wt-iron.jpg',.8,.48);oak=mat('Carved dark oak','wt-dark-oak.jpg',0,.82);pine=mat('Pale pine map-board','wt-pine.jpg',0,.86);linen=mat('Worn chart parchment','wt-burlap.jpg',0,.91);stone=mat('Limestone foot blocks','wt-limestone.jpg',0,.88)
    # Six separate oak planks, rounded perimeter and a visible apron below the top.
    for i in range(6):cube('Individually joined tabletop plank',(0,-.54+i*.216,1.02),(2.10,.205,.13),oak,.026)
    for y in (-.61,.61):cube('Table end cleat',(0,y,.96),(2.12,.085,.12),pine,.018)
    for x in (-.82,.82):cube('Carved table apron',(x,0,.79),(.13,1.12,.35),oak,.032)
    # Braced trestle legs and stone foot plates.
    for x in (-.82,.82):
        for y in (-.45,.45):
            rod('Trestle leg',(x*.80,y,.16),(x,y,.82),.078,oak,20)
            cube('Limestone foot',(x*.80,y,.10),(.25,.25,.18),stone,.025)
    cube('Long lower stretcher',(0,0,.39),(1.48,.13,.14),oak,.025)
    cube('Cross stretcher',(0,0,.39),(.13,.80,.14),oak,.025)
    # Iron corner plates, washers and nail heads.
    for x in (-.93,.93):
        for y in (-.53,.53):
            cube('Forged corner plate',(x,y,1.105),(.18,.18,.035),iron,.018)
            cylinder('Corner rivet',(x,y,1.13),.025,.025,iron,16,.025,0)
    # A framed, blank, parchment campaign chart, pinned with cast markers and brass route tacks.
    cube('Map-board frame',(0,0,1.12),(1.37,.89,.06),iron,.028)
    cube('Inset parchment chart',(0,0,1.155),(1.29,.81,.025),linen,.012)
    for x,y in [(-.44,-.24),(-.21,.22),(.02,-.04),(.28,.20),(.43,-.18),(.08,.30)]:
        cylinder('Route marker',(x,y,1.18),.033,.034,iron,20,.025,.004)
    # Low-relief route path: thin dark cord follows the hand drawn line between pegs.
    points=[(-.44,-.24,1.181),(-.21,.22,1.181),(.02,-.04,1.181),(.28,.20,1.181),(.43,-.18,1.181)]
    for a,b in zip(points,points[1:]):rod('Raised route line',a,b,.009,oak,8)
    finish('watchtower-command-table-v02')

signal_brazier();arms_rack();command_table()
print(f'Created three detailed Watchtower v02 models in {OUT}')
