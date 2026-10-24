"""Create three high-poly, leafy tree assets for Watchtower review (no map wiring)."""
import bpy, math, random
from pathlib import Path
from mathutils import Vector, Quaternion

ROOT = Path('C:/EmberEngine')
OUT = ROOT / 'content/models/watchtower-leafy-trees-v01'
TEX = ROOT / 'content/models/textures'
OUT.mkdir(parents=True, exist_ok=True)
bpy.context.preferences.filepaths.save_version = 0

def reset():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for m in list(bpy.data.materials): bpy.data.materials.remove(m)

def material(name, color, rough=.88, image=None):
    m=bpy.data.materials.new(name); m.diffuse_color=(*color,1); m.use_nodes=True
    p=m.node_tree.nodes.get('Principled BSDF'); p.inputs['Base Color'].default_value=(*color,1); p.inputs['Roughness'].default_value=rough
    if image:
        im=bpy.data.images.load(str(image),check_existing=True)
        tex=m.node_tree.nodes.new('ShaderNodeTexImage'); tex.image=im
        m.node_tree.links.new(tex.outputs['Color'],p.inputs['Base Color'])
    return m

def tube(name, points, radii, mat, sides=12):
    pts=[Vector(p) for p in points]; verts=[]; faces=[]
    for i,p in enumerate(pts):
        tangent=(pts[min(i+1,len(pts)-1)]-pts[max(0,i-1)]).normalized()
        u=tangent.cross(Vector((0,1,0))).normalized()
        if u.length<.01: u=tangent.cross(Vector((1,0,0))).normalized()
        v=tangent.cross(u).normalized()
        for j in range(sides):
            a=math.tau*j/sides; r=radii[i]*(1+.035*math.sin(j*7+i*.43))
            verts.append(tuple(p+r*(u*math.cos(a)+v*math.sin(a))))
    for i in range(len(pts)-1):
        for j in range(sides):
            a=i*sides+j; b=i*sides+(j+1)%sides; faces.append((a,b,b+sides,a+sides))
    faces += [tuple(range(sides-1,-1,-1)),tuple((len(pts)-1)*sides+j for j in range(sides))]
    me=bpy.data.meshes.new(name); me.from_pydata(verts,[],faces); me.materials.append(mat); me.update()
    uv=me.uv_layers.new(name='UVMap')
    for poly in me.polygons:
        for li in poly.loop_indices:
            vi=me.loops[li].vertex_index; ring=vi//sides; side=vi%sides
            uv.data[li].uv=(side/sides,ring/max(1,len(pts)-1))
    ob=bpy.data.objects.new(name,me); bpy.context.collection.objects.link(ob)
    for p in me.polygons: p.use_smooth=True
    return ob

def leaf_mesh(name, leaves, mat):
    verts=[]; faces=[]; uvcoords=[]
    for center, direction, length, width, mi in leaves:
        c=Vector(center); d=Vector(direction).normalized(); side=d.cross(Vector((0,0,1))).normalized()
        if side.length<.01: side=d.cross(Vector((0,1,0))).normalized()
        side.rotate(Quaternion(Vector((0,0,1)),random.random()*math.tau))
        # Sculpted leaf blade: pointed ends, raised midrib and a slight natural curl.
        p0=c-d*length*.52; p1=c-d*length*.18+side*width*.5+Vector((0,0,.025)); p2=c+d*length*.18+Vector((0,0,.045)); p3=c-d*length*.18-side*width*.5+Vector((0,0,.025)); p4=c+d*length*.52
        base=len(verts); verts.extend(map(tuple,(p0,p1,p2,p3,p4)))
        faces.extend(((base,base+1,base+2),(base,base+2,base+3),(base+1,base+4,base+2),(base+2,base+4,base+3)))
        # Each physical blade samples a different patch of the photographic leaf atlas.
        u0=random.uniform(.025,.76); v0=random.uniform(.025,.76); span=.22
        uvcoords.extend(((u0+span*.5,v0),(u0,v0+span*.32),(u0+span*.5,v0+span*.68),(u0+span,v0+span*.32),(u0+span*.5,v0+span)))
    me=bpy.data.meshes.new(name); me.from_pydata(verts,[],faces); me.materials.append(mat)
    uv=me.uv_layers.new(name='UVMap')
    for poly in me.polygons:
        for li in poly.loop_indices: uv.data[li].uv=uvcoords[me.loops[li].vertex_index]
    me.update(); ob=bpy.data.objects.new(name,me); bpy.context.collection.objects.link(ob)
    return ob

def make_tree(kind, seed):
    reset(); rng=random.Random(seed)
    barkcolor={'oak':(.19,.085,.035),'willow':(.27,.20,.12),'birch':(.48,.43,.31)}[kind]
    bark=material('Bark | photographic bark grain',barkcolor,.94,TEX/'bark-bough-photo-v01.png')
    if kind=='oak':
        h=8.0; crown=(2.75,2.35,2.15); leafcols=[(.035,.13,.028),(.07,.20,.035),(.12,.25,.045),(.18,.29,.065)]; count=7200
    elif kind=='willow':
        h=7.6; crown=(2.0,2.0,3.0); leafcols=[(.045,.15,.05),(.09,.23,.065),(.14,.30,.09),(.20,.34,.11)]; count=7200
    else:
        h=8.4; crown=(2.35,2.0,2.25); leafcols=[(.045,.15,.045),(.09,.24,.065),(.15,.31,.085),(.21,.35,.11)]; count=6500
    nt=bark.node_tree; bs=nt.nodes.get('Principled BSDF'); noise=nt.nodes.new('ShaderNodeTexNoise'); noise.inputs['Scale'].default_value=5.5; noise.inputs['Detail'].default_value=4.0
    bump=nt.nodes.new('ShaderNodeBump'); bump.inputs['Strength'].default_value=.18; bump.inputs['Distance'].default_value=.045
    nt.links.new(noise.outputs['Fac'],bump.inputs['Height']); nt.links.new(bump.outputs['Normal'],bs.inputs['Normal'])
    leafmat=material('Leaves | photorealistic leaf atlas',(.82,.86,.70),.9,TEX/'foliage-leaves-photo-v01.png')
    # Keep the atlas detail crisp and matte; the photo contributes natural veins, color and edge variation.
    # Irregular flared trunk and exposed roots.
    trunk=[]
    for i in range(17):
        t=i/16; trunk.append((.10*math.sin(t*5+seed)*t,.07*math.sin(t*4)*t,h*t))
    radii=[.48*(1-i/16)**.9+.035 for i in range(17)]
    tube('Continuous tapered trunk with root flare',trunk,radii,bark,20)
    for i in range(9):
        a=math.tau*i/9+rng.uniform(-.15,.15); L=rng.uniform(.55,1.05)
        tube('Surface root',[(0,0,.25),(math.cos(a)*.35,math.sin(a)*.35,.18),(math.cos(a)*L,math.sin(a)*L,.025)],[.17,.075,.008],bark,12)
    # Scaffold limbs sweep outward with taper; foliage is attached to terminal twigs.
    leaves=[]; crown_center=Vector((0,0,h*.69))
    branch_n=13 if kind!='willow' else 17
    for bi in range(branch_n):
        az=math.tau*bi/branch_n+rng.uniform(-.22,.22)
        z0=h*rng.uniform(.35,.78); rise=h*rng.uniform(.08,.23)
        reach=rng.uniform(1.35,2.45) if kind!='willow' else rng.uniform(1.0,1.9)
        end=Vector((math.cos(az)*reach,math.sin(az)*reach,z0+rise))
        start=Vector((0,0,z0)); mid=Vector((math.cos(az)*reach*.48,math.sin(az)*reach*.48,z0+rise*.45))
        tube('Primary tapered branch',[start,mid,end],[rng.uniform(.10,.16),.075,.018],bark,14)
        # Three fine twigs per bough, with species-specific silhouette.
        for ti in range(3):
            tt=.34+ti*.20; anchor=start.lerp(end,tt)
            sideaz=az+rng.uniform(-.8,.8); twiglen=rng.uniform(.55,1.2)
            twigend=anchor+Vector((math.cos(sideaz)*twiglen,math.sin(sideaz)*twiglen,rng.uniform(.35,1.05)))
            tube('Fine terminal twig',[anchor,anchor.lerp(twigend,.52),twigend],[.035,.019,.006],bark,9)
            for k in range(34 if kind!='willow' else 42):
                t=rng.random(); p=anchor.lerp(twigend,t)
                if kind=='willow':
                    p.z-=t*t*rng.uniform(.25,.65)
                    direction=Vector((rng.uniform(-.4,.4),rng.uniform(-.3,.3),-1))
                    length=rng.uniform(.20,.34); width=length*.30
                else:
                    direction=Vector((rng.uniform(-.8,.8),rng.uniform(-.8,.8),rng.uniform(-.25,1))).normalized()
                    length=rng.uniform(.16,.29); width=length*rng.uniform(.38,.58)
                leaves.append((p,direction,length,width,0))
    # Fill the crown with layered, overlapping leaf sprays. Keep an uneven edge and visible gaps.
    for i in range(count-len(leaves)):
        if kind=='willow':
            z=rng.uniform(h*.48,h*.96); r=(1-(z-h*.48)/(h*.48))*.72
            ang=rng.random()*math.tau; p=Vector((math.cos(ang)*r*rng.uniform(.25,1.8),math.sin(ang)*r*rng.uniform(.25,1.8),z))
            direction=Vector((rng.uniform(-.4,.4),rng.uniform(-.4,.4),-1)); length=rng.uniform(.18,.33); width=length*.3
        else:
            x=rng.gauss(0,crown[0]*.48); y=rng.gauss(0,crown[1]*.48); z=rng.gauss(h*.70,crown[2]*.39)
            if x*x/crown[0]**2+y*y/crown[1]**2+(z-h*.70)**2/crown[2]**2>1: continue
            p=Vector((x,y,z)); direction=Vector((rng.uniform(-.8,.8),rng.uniform(-.8,.8),rng.uniform(-.35,1))).normalized()
            length=rng.uniform(.16,.30); width=length*rng.uniform(.4,.6)
        leaves.append((p,direction,length,width,0))
    leaf_mesh('Thousands of individually modeled leaves',leaves,leafmat)
    # Place origin at the base, normalize application-facing model dimensions and export.
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    bpy.ops.object.select_all(action='DESELECT')
    for o in meshes:o.select_set(True)
    bpy.context.view_layer.objects.active=meshes[0]; bpy.ops.object.join(); tree=bpy.context.object; tree.name=f'watchtower-leafy-{kind}-v01'
    tree.location=(0,0,0)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/(tree.name+'.blend')))
    bpy.ops.export_scene.gltf(filepath=str(OUT/(tree.name+'.glb')),export_format='GLB',use_selection=True,export_apply=True)

for i,kind in enumerate(('oak','willow','birch')): make_tree(kind,20261024+i)
