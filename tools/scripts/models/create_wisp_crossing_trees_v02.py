import math, os, random
from pathlib import Path
import bpy
from mathutils import Vector

ROOT = Path('C:/EmberEngine')
OUT = ROOT / 'content/models/wisp-crossing-trees'
TEX = ROOT / 'content/models/textures'
BARK = TEX / 'wisp-dead-bark-v01.jpg'
NORMAL = TEX / 'wisp-dead-bark-v01-normal.png'
OUT.mkdir(parents=True, exist_ok=True)


def material(name, image_path, roughness=0.94):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; p = nt.nodes.get('Principled BSDF')
    p.inputs['Roughness'].default_value = roughness
    im = bpy.data.images.load(str(image_path), check_existing=True)
    c = nt.nodes.new('ShaderNodeTexImage'); c.image = im; nt.links.new(c.outputs['Color'], p.inputs['Base Color'])
    if NORMAL.exists():
        ni = bpy.data.images.load(str(NORMAL), check_existing=True); ni.colorspace_settings.name = 'Non-Color'
        n = nt.nodes.new('ShaderNodeTexImage'); n.image = ni
        mp = nt.nodes.new('ShaderNodeNormalMap'); mp.inputs['Strength'].default_value = 0.48
        nt.links.new(n.outputs['Color'], mp.inputs['Color']); nt.links.new(mp.outputs['Normal'], p.inputs['Normal'])
    return m


def tube(name, points, radii, mat, sides=12, groove=0.08, seed=0):
    rng = random.Random(seed); pts = [Vector(p) for p in points]
    verts=[]; faces=[]
    for i, p in enumerate(pts):
        tangent=(pts[min(i+1,len(pts)-1)]-pts[max(0,i-1)]).normalized()
        ref=Vector((0,1,0)) if abs(tangent.dot(Vector((0,0,1))))>0.90 else Vector((0,0,1))
        u=tangent.cross(ref).normalized(); v=tangent.cross(u).normalized()
        for j in range(sides):
            a=math.tau*j/sides
            rib=1.0+groove*math.sin(a*7.0+0.4)+0.025*math.sin(a*13.0+i*0.7)
            r=radii[i]*rib
            verts.append(tuple(p+r*(u*math.cos(a)+v*math.sin(a))))
    for i in range(len(pts)-1):
        for j in range(sides):
            a=i*sides+j; b=i*sides+(j+1)%sides
            faces.append((a,b,b+sides,a+sides))
    faces.append(tuple(range(sides-1,-1,-1))); faces.append(tuple((len(pts)-1)*sides+j for j in range(sides)))
    mesh=bpy.data.meshes.new(name+' | fluted mesh'); mesh.from_pydata(verts,[],faces); mesh.materials.append(mat); mesh.update()
    ob=bpy.data.objects.new(name,mesh); bpy.context.collection.objects.link(ob)
    # cylindrical UVs let the scanned bark grain run along each trunk and branch
    uv=mesh.uv_layers.new(name='Bark grain')
    for poly in mesh.polygons:
        for li in poly.loop_indices:
            vi=mesh.loops[li].vertex_index; ring=vi//sides; side=vi%sides
            uv.data[li].uv=(side/sides, ring/max(1,len(pts)-1))
        poly.use_smooth=True
    return ob


def curve_branch(name, p0, p1, p2, p3, r0, r1, mat, seed, rings=10, sides=10, groove=.10):
    # Cubic Bézier path, sampled as a swept taper with ridged, irregular bark.
    a,b,c,d=map(Vector,(p0,p1,p2,p3)); pts=[]; rr=[]
    for i in range(rings):
        t=i/(rings-1); s=1-t
        pts.append(s**3*a+3*s*s*t*b+3*s*t*t*c+t**3*d)
        rr.append(max(.012,r0*(1-t)**.83+r1*t))
    return tube(name,pts,rr,mat,sides,groove,seed)


def build_tree(style, seed, bark, scar, ground):
    rng=random.Random(seed); parts=[]
    height={'oak':10.2,'willow':9.2,'split':9.7,'hollow':8.8}[style]
    lean={'oak':.18,'willow':-.48,'split':.05,'hollow':-.10}[style]
    # Sinuous trunk with a wide root flare and subtle taper; no straight stacked cylinders.
    trunk=[]; radii=[]
    for i in range(19):
        t=i/18
        sway=0.26*math.sin(t*math.pi*2.4+seed*.1)*t
        trunk.append((lean*t+sway,0.12*math.sin(t*math.pi*1.7)*t,height*t))
        flare=1.32 if t<.13 else 1.0
        radii.append((.72*(1-t)**.88+.06)*flare*(1+.035*math.sin(i*1.7)))
    parts.append(tube('deeply furrowed winding trunk',trunk,radii,bark,20,.13,seed))
    # Root buttresses split outward through the leaf litter.
    root_count=10 if style=='oak' else 8
    for i in range(root_count):
        a=math.tau*i/root_count+rng.uniform(-.16,.16); L=rng.uniform(1.35,2.25)*(1.22 if style=='oak' else 1.0)
        z0=rng.uniform(.16,.52); x0=lean*z0/height
        p0=(x0,0,z0); p1=(x0+math.cos(a)*.50,math.sin(a)*.50,.20); p2=(math.cos(a)*L*.82,math.sin(a)*L*.82,.08); p3=(math.cos(a)*L,math.sin(a)*L,.025)
        parts.append(curve_branch('radiating buttress root',p0,p1,p2,p3,rng.uniform(.22,.34),.018,bark,seed+i,9,12,.16))
    if style=='oak':
        branch_count=9; zlo=.34; zhi=.82; spread=1.0
    elif style=='willow':
        branch_count=12; zlo=.32; zhi=.93; spread=.78
    elif style=='split':
        branch_count=8; zlo=.30; zhi=.72; spread=.86
    else:
        branch_count=7; zlo=.28; zhi=.86; spread=.76
    # Heavy asymmetric scaffold branches, secondary leaders and fine crown twigs.
    for i in range(branch_count):
        ang=math.tau*i/branch_count+rng.uniform(-.24,.24)
        t=rng.uniform(zlo,zhi); z=height*t; start=Vector((lean*t+0.20*math.sin(t*math.pi*2),0.11*math.sin(t*math.pi*1.7),z))
        length=height*spread*rng.uniform(.34,.50)*(1.0-t*.32)
        if style=='willow': length*=.84
        dx,dy=math.cos(ang),math.sin(ang)
        p0=tuple(start); p1=tuple(start+Vector((dx*length*.18,dy*length*.18,height*.09)))
        p2=tuple(start+Vector((dx*length*.74,dy*length*.72,height*rng.uniform(.025,.11))))
        p3=tuple(start+Vector((dx*length,dy*length,height*rng.uniform(-.055,.045))))
        r0=max(.16,(.72*(1-t)**.88+.06)*rng.uniform(.52,.72)); r1=.035
        parts.append(curve_branch('massive gnarled bough',p0,p1,p2,p3,r0,r1,bark,seed*5+i,12,12,.15))
        for k in range(3 if style!='willow' else 4):
            u=rng.uniform(.38,.90); q=Vector(p0).lerp(Vector(p3),u); side=rng.choice([-1,1]); twlen=length*rng.uniform(.25,.40)
            p1b=q+Vector((dx*twlen*.25-dy*twlen*.22*side,dy*twlen*.25+dx*twlen*.22*side,height*.025))
            p2b=q+Vector((dx*twlen*.68-dy*twlen*.25*side,dy*twlen*.68+dx*twlen*.25*side,height*rng.uniform(.07,.16)))
            p3b=q+Vector((dx*twlen-dy*twlen*.18*side,dy*twlen+dx*twlen*.18*side,height*rng.uniform(.13,.25)))
            rsec=max(.065,r0*rng.uniform(.36,.50))
            parts.append(curve_branch('forked weathered limb',tuple(q),tuple(p1b),tuple(p2b),tuple(p3b),rsec,.010,bark,seed*13+i*7+k,8,9,.12))
            for j in range(2):
                q2=Vector(p3b); a2=ang+rng.uniform(-.65,.65); L2=twlen*rng.uniform(.28,.52)
                end=q2+Vector((math.cos(a2)*L2,math.sin(a2)*L2,height*rng.uniform(.035,.09)))
                parts.append(curve_branch('fine broken crown twig',tuple(q2),tuple(q2.lerp(end,.34)+Vector((0,0,.02))),tuple(q2.lerp(end,.72)+Vector((0,0,.02))),tuple(end),rsec*.27,.005,bark,seed*31+i*11+j,6,7,.08))
        if style=='willow':
            # Long broken pendants form the ragged curtain of the dead willow.
            for j in range(2):
                q=Vector(p3)+Vector((rng.uniform(-.18,.18),rng.uniform(-.18,.18),0)); drop=height*rng.uniform(.13,.24)
                e=q+Vector((rng.uniform(-.22,.22),rng.uniform(-.22,.22),-drop))
                parts.append(curve_branch('hanging willow branch',tuple(q),tuple(q+Vector((.06,0,-drop*.27))),tuple(e+Vector((-.08,.04,drop*.22))),tuple(e),.045,.005,bark,seed+i*17+j,10,8,.09))
    if style=='split':
        for side in (-1,1):
            p0=(lean*.55,0,height*.57); p1=(side*.22,-.03,height*.72); p2=(side*.45,.02,height*.91); p3=(side*.70,.05,height*.99)
            parts.append(curve_branch('lightning split twin leader',p0,p1,p2,p3,.42,.035,bark,seed+side,15,14,.15))
        # Jagged exposed heartwood scar between the twin leaders.
        sc=curve_branch('exposed pale broken heartwood',(.02,-.64,height*.69),(.01,-.61,height*.79),(.03,-.55,height*.91),(.12,-.44,height*.97),.07,.012,scar,seed+55,10,8,.05); parts.append(sc)
    if style=='hollow':
        # Deep cavity as an inset dark oval with a raised irregular rim.
        cave=bpy.data.materials.new('shadow inside hollow snag'); cave.diffuse_color=(.018,.012,.009,1); cave.use_nodes=True
        bs=cave.node_tree.nodes.get('Principled BSDF'); bs.inputs['Base Color'].default_value=(.018,.012,.009,1); bs.inputs['Roughness'].default_value=1.0
        cx=-.11; z=height*.43; verts=[]; faces=[]; n=32
        for j in range(n):
            a=math.tau*j/n; verts.append((cx+math.cos(a)*.25,-.66,z+math.sin(a)*.48))
        me=bpy.data.meshes.new('recessed hollow darkness'); me.from_pydata(verts,[],[tuple(range(n))]); me.materials.append(cave)
        ob=bpy.data.objects.new('deep hollow cavity',me); bpy.context.collection.objects.link(ob); parts.append(ob)
        rim=[]
        for j in range(n+1):
            a=math.tau*j/n; rim.append((cx+math.cos(a)*.33,-.72,z+math.sin(a)*.58))
        parts.append(tube('gnarled cavity rim',rim,[.075+.025*math.sin(i*.7) for i in range(n+1)],bark,10,.14,seed+30))
    # A compact low root-and-leaf mound anchors the tree without reading as a tile prop.
    mound=bpy.data.materials.new('root mound dark soil'); mound.diffuse_color=(.10,.068,.043,1); mound.use_nodes=True
    bs=mound.node_tree.nodes.get('Principled BSDF'); bs.inputs['Base Color'].default_value=(.10,.068,.043,1); bs.inputs['Roughness'].default_value=1.0
    bpy.ops.mesh.primitive_uv_sphere_add(segments=20, ring_count=12, location=(0,0,.055)); base=bpy.context.object; base.name='subtle root and soil mound'; base.scale=(1.25,1.05,.12); bpy.ops.object.transform_apply(location=False,rotation=False,scale=True); base.data.materials.append(ground); parts.append(base)
    return parts, height


RECIPES=[('ancient-dead-oak','oak',322),('weeping-willow','willow',337),('lightning-split','split',359),('hollow-sentinel','hollow',377)]
for asset,style,seed in RECIPES:
    glb=OUT/f'{asset}-v02.glb'; blend=OUT/f'{asset}-v02.blend'
    if glb.exists() or blend.exists(): raise RuntimeError(f'preserving existing output: {glb if glb.exists() else blend}')
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bark=material('Wisp Forest | scanned dead bark',BARK,.94)
    scar=material('Wisp Forest | exposed broken pale wood',TEX/'wisp-dead-bark-v01.jpg',.97)
    ground=material('Wisp Forest | root mound leaf litter',TEX/'wisp-leaf-litter-v01.jpg',.98)
    parts,height=build_tree(style,seed,bark,scar,ground)
    # Preserve separated branches in the source .blend while packing textures into both outputs.
    bpy.ops.wm.save_as_mainfile(filepath=str(blend))
    bpy.ops.export_scene.gltf(filepath=str(glb),export_format='GLB',export_apply=True,export_texcoords=True,export_normals=True,export_materials='EXPORT',export_image_format='AUTO',export_yup=True)
    print(f'[ok] {asset}: {glb.stat().st_size:,} bytes; authored height {height}')

