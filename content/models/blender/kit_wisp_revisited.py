"""Photoreal Wisp Forest Revisited tree and ground kit (v01, additive)."""
import math, os, random, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emberlib import *  # noqa: F401,F403,E402

KIT = 'wisp-forest-revisited'
V = 'v02'
TEX = 'C:/EmberEngine/content/models/textures'


def _photo_material(name, color_file, normal_file, roughness):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes.get('Principled BSDF')
    bsdf.inputs['Roughness'].default_value = roughness
    col = bpy.data.images.load(os.path.join(TEX, color_file), check_existing=True)
    tex = nt.nodes.new('ShaderNodeTexImage'); tex.image = col
    nt.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
    normal = bpy.data.images.load(os.path.join(TEX, normal_file), check_existing=True)
    normal.colorspace_settings.name = 'Non-Color'
    ntex = nt.nodes.new('ShaderNodeTexImage'); ntex.image = normal
    nmap = nt.nodes.new('ShaderNodeNormalMap'); nmap.inputs['Strength'].default_value = 0.65
    nt.links.new(ntex.outputs['Color'], nmap.inputs['Color'])
    nt.links.new(nmap.outputs['Normal'], bsdf.inputs['Normal'])
    return m


def _materials():
    return {
        'bark': _photo_material('Wisp dead bark | scanned', 'wisp-dead-bark-v01.jpg', 'wisp-dead-bark-v01-normal.png', 0.92),
        'floor': _photo_material('Wisp leaf litter | scanned', 'wisp-leaf-litter-v01.jpg', 'wisp-leaf-litter-v01-normal.png', 0.97),
        'root': _photo_material('Wisp compact root soil | scanned', 'wisp-root-soil-v01.jpg', 'wisp-root-soil-v01-normal.png', 0.98),
        'ash': _photo_material('Wisp ash leaf litter | scanned', 'wisp-charred-leaf-v01.jpg', 'wisp-charred-leaf-v01-normal.png', 0.99),
    }


def _limb(parts, mat, a, b, r1, r2, name, sides=9):
    d = Vector(b) - Vector(a)
    if d.length < 0.025: return
    o = cylinder(name, r1, r2, d.length, loc=tuple((Vector(a)+Vector(b))*0.5), segs=sides, mat=mat)
    aim(o, d.normalized()); apply_transform(o); parts.append(o)


def _tree(recipe, mats):
    seed, height, trunk_r, lean, style = recipe
    rng = random.Random(seed)
    bark = mats['bark']; parts=[]
    # A winding segmented trunk, with buttress roots grounding the silhouette.
    steps = 10
    points=[]
    for j in range(steps+1):
        t=j/steps
        twist=(0.0 if style != 'spiral' else math.tau*1.12*t)
        x=lean*t + (0.18*math.sin(t*math.pi*2) if style=='sway' else 0.40*math.cos(twist)-0.40 if style=='spiral' else 0)
        y=(0.16*math.sin(t*math.pi*2) if style=='sway' else 0.40*math.sin(twist) if style=='spiral' else 0)
        points.append(Vector((x,y,height*t)))
    radii=[trunk_r*(1-0.83*j/steps)**0.82 for j in range(steps+1)]
    for j in range(steps):
        _limb(parts,bark,points[j],points[j+1],radii[j],radii[j+1],f'trunk-{j:02}',12)
    # Broad, irregular roots fan into the soil.
    for i in range(rng.randint(7,10)):
        a=math.tau*i/9+rng.uniform(-0.22,0.22)
        length=rng.uniform(0.75,1.4)*trunk_r*1.7
        start=points[1]+Vector((0,0,0.05))
        end=Vector((math.cos(a)*length,math.sin(a)*length,0.045))
        mid=(start+end)*0.52+Vector((0,0,0.04))
        rr=trunk_r*rng.uniform(0.22,0.35)
        _limb(parts,bark,start,mid,rr,rr*0.56,'buttress-root')
        _limb(parts,bark,mid,end,rr*0.56,0.018,'root-tip',7)

    if style == 'oak':
        count=10; spread=1.30; branch_start=.30; top=.98
    elif style == 'willow':
        count=11; spread=1.14; branch_start=.38; top=.94
    elif style in ('pine','burnt'):
        count=16; spread=.78; branch_start=.24; top=.96
    elif style == 'hollow':
        count=8; spread=1.03; branch_start=.36; top=.95
    elif style == 'split':
        count=11; spread=1.16; branch_start=.32; top=.95
    elif style == 'spiral':
        count=12; spread=1.08; branch_start=.26; top=.98
    elif style == 'lean':
        count=9; spread=1.16; branch_start=.34; top=.94
    elif style == 'broken':
        count=11; spread=1.22; branch_start=.34; top=.94
    elif style == 'arch':
        count=5; spread=1.08; branch_start=.44; top=.93
    else:
        count=10; spread=1.05; branch_start=.36; top=.97

    # Main limbs: each forks once into tapered, genuinely volumetric twigs.
    for i in range(count):
        ang=math.tau*i/count+rng.uniform(-.18,.18)
        t=rng.uniform(branch_start,top)
        root=points[min(steps-1,int(t*steps))].lerp(points[min(steps,int(t*steps)+1)],(t*steps)%1)
        L=height*spread*rng.uniform(.27,.42)*(1.0-t*.22)
        if style in ('pine','burnt'): L=height*.24*(1-t*.38)
        if style=='broken': L*=1.42 if math.cos(ang)>.12 else .56
        if style=='willow': L*=.68
        dx,dy=math.cos(ang),math.sin(ang)
        end=root+Vector((dx*L,dy*L,L*rng.uniform(.28,.76)))
        r=radii[min(steps-1,int(t*steps))]*rng.uniform(.26,.42)
        _limb(parts,bark,root,end,r,max(.025,r*.16),'limb')
        for branch in range(2 if style in ('pine','burnt') else 3):
            bt=rng.uniform(.42,.88)
            at=root.lerp(end,bt)
            side=(-1 if branch%2 else 1)
            tip=at+Vector((dx*L*.38-dy*L*.32*side,dy*L*.38+dx*L*.32*side,L*rng.uniform(.13,.42)))
            _limb(parts,bark,at,tip,r*.48,.012,'twig',7)
            for k in range(2):
                tw=tip+Vector((rng.uniform(-.17,.17),rng.uniform(-.17,.17),rng.uniform(.14,.30)))
                _limb(parts,bark,tip,tw,.022,.006,'fine-twig',5)
        if style=='willow':
            # Long, fine dead streamers characterize the hanging willow canopy.
            for _ in range(3):
                q=end+Vector((rng.uniform(-.18,.18),rng.uniform(-.18,.18),0))
                p1=q+Vector((rng.uniform(-.20,.20),rng.uniform(-.20,.20),-height*rng.uniform(.14,.25)))
                p2=p1+Vector((rng.uniform(-.13,.13),rng.uniform(-.13,.13),-.30))
                _limb(parts,bark,q,p1,.019,.008,'hanging-dead-tendril',5)
                _limb(parts,bark,p1,p2,.008,.002,'tendril-tip',5)

    if style=='split':
        # Two diverging upper leaders leave a clear lightning-split notch.
        z=height*.56
        for side in (-1,1):
            a=Vector((lean*z/height,0,z)); b=a+Vector((side*height*.20,0,height*.40))
            _limb(parts,bark,a,b,trunk_r*.37,.07,'split-leader',12)
    if style=='hollow':
        # Deep, dark inset in the front of the snag with an irregular raised wooden rim.
        z=height*.43; cx=points[int(steps*.43)].x
        cavity=material('Wisp snag cavity',flat=(0.025,0.022,0.019),roughness=1.0)
        verts=[]; faces=[]; n=20
        for i in range(n):
            a=math.tau*i/n
            verts.append((cx+math.cos(a)*trunk_r*.30,-trunk_r*.94,z+math.sin(a)*height*.14))
        me=bpy.data.meshes.new('snag-cavity-mesh'); me.from_pydata(verts,[],[tuple(range(n))]); me.materials.append(cavity)
        ob=bpy.data.objects.new('recessed hollow',me); bpy.context.collection.objects.link(ob); parts.append(ob)
        for i in range(n):
            a=math.tau*i/n; b=math.tau*(i+1)/n
            pa=(cx+math.cos(a)*trunk_r*.37,-trunk_r*1.02,z+math.sin(a)*height*.17)
            pb=(cx+math.cos(b)*trunk_r*.37,-trunk_r*1.02,z+math.sin(b)*height*.17)
            _limb(parts,bark,pa,pb,.045,.045,'hollow-rim',6)
    if style=='burnt':
        # Charred branch tips use matte charcoal, without emissive effects.
        char=material('Wisp matte charcoal wood',flat=(.075,.067,.06),roughness=.98)
        for i in range(13):
            a=math.tau*i/13; z=height*rng.uniform(.35,.86)
            root=Vector((lean*z/height,0,z)); end=root+Vector((math.cos(a)*.42,math.sin(a)*.42,rng.uniform(.16,.42)))
            _limb(parts,char,root,end,.105,.018,'burnt-stub',7)
    if style=='arch':
        # Dead leaders curve around an open, navigable arch.
        for side in (-1,1):
            p0=Vector((side*.52,0,.38)); p1=Vector((side*1.10,0,height*.42)); p2=Vector((side*.72,0,height*.83)); p3=Vector((0,0,height*.91))
            _limb(parts,bark,p0,p1,trunk_r*.62,trunk_r*.39,'arch-leg')
            _limb(parts,bark,p1,p2,trunk_r*.39,trunk_r*.24,'arch-curve')
            _limb(parts,bark,p2,p3,trunk_r*.24,.07,'arch-crown')
    o=join(parts,'dead-tree-'+style)
    ground(o); box_uv(o,2.0); smooth(o,48)
    return o


def _ground_tile(mats, name, key):
    floor=mats[key]
    p=cylinder(name+'-hex',.98,.98,.075,loc=(0,0,.0375),segs=6,mat=floor)
    p.rotation_euler[2]=math.pi/6; apply_transform(p); box_uv(p,1.9); ground(p)
    return p


def _stump(mats, seed=41):
    rng=random.Random(seed); parts=[]; b=mats['bark']
    o=cylinder('broken-stump',.78,.58,1.35,loc=(0,0,.66),segs=14,mat=b); o.rotation_euler[0]=.08; parts.append(o)
    for i in range(7):
        a=math.tau*i/7; start=Vector((math.cos(a)*.38,math.sin(a)*.38,.18)); end=Vector((math.cos(a)*rng.uniform(.75,1.05),math.sin(a)*rng.uniform(.75,1.05),.04))
        _limb(parts,b,start,end,.18,.018,'stump-root')
    ob=join(parts,'broken-stump'); ground(ob); box_uv(ob,1.4); smooth(ob,50); return ob


def _log(mats, seed=42):
    b=mats['bark']; parts=[]
    main=cylinder('rotted-hollow-log',.53,.39,3.2,loc=(0,0,.47),rot=(math.pi/2,0,0),segs=14,mat=b); parts.append(main)
    # Dark hollow opening and splintered rim at one exposed end.
    dark=material('Wisp hollow log interior',flat=(.025,.022,.02),roughness=1.0)
    for y in (-1.62,1.62):
        disk=cylinder('dark end-grain recess',.34,.34,.018,loc=(0,y,.47),rot=(math.pi/2,0,0),segs=14,mat=dark); parts.append(disk)
    ob=join(parts,'rotted-hollow-log'); ground(ob); box_uv(ob,1.5); smooth(ob,55); return ob


def _mushrooms(mats, seed=43):
    rng=random.Random(seed); b=mats['bark']; parts=[]
    # A rooted dead-branch cluster with small photographic pale mushroom caps.
    for i in range(4):
        x=rng.uniform(-.55,.55); y=rng.uniform(-.5,.5); h=rng.uniform(.22,.44)
        stem=cylinder('mushroom-stem',.045,.035,h,loc=(x,y,h/2),segs=7,mat=b); parts.append(stem)
        cap=sphere('muted-fungus-cap',rng.uniform(.12,.20),(x,y,h),subdiv=2,mat=material('pale fungus',flat=(.35,.31,.26),roughness=.82),scale=(1.0,1.0,.42)); parts.append(cap)
    root=cylinder('mossless-root-base',.78,.7,.07,loc=(0,0,.035),segs=9,mat=mats['root']); parts.append(root)
    ob=join(parts,'roots-and-fungi'); ground(ob); box_uv(ob,1.1); smooth(ob,55); return ob


def build():
    mats=_materials()
    recipes=[
        ('weeping-willow', (101,8.6,.57,-.18,'willow')),
        ('giant-dead-oak', (102,9.1,.78,.12,'oak')),
        ('hollow-snag', (103,8.0,.69,-.05,'hollow')),
        ('uprooted-leaning', (104,7.8,.55,3.0,'lean')),
        ('lightning-split', (105,8.5,.61,.03,'split')),
        ('spiral-twisted', (106,8.2,.59,.08,'spiral')),
        ('tall-burnt-pine', (107,9.3,.45,-.04,'burnt')),
        ('broken-crown', (108,7.2,.62,1.55,'broken')),
        ('root-arch', (109,7.6,.58,-.12,'arch')),
    ]
    for name, recipe in recipes:
        reset(); mats=_materials(); obj=_tree(recipe,mats); print('[ok] tree',name,export(obj,KIT,name,V))
    for name,key in [('leaf-litter-tile','floor'),('rooted-soil-tile','root'),('ash-leaf-tile','ash')]:
        if not os.path.exists(f'{ROOT}/{KIT}/{name}-v01.glb'):
            reset(); mats=_materials(); obj=_ground_tile(mats,name,key); print('[ok] tile',name,export(obj,KIT,name,'v01'))
    for name,builder in [('broken-stump',_stump),('rotted-hollow-log',_log),('roots-and-fungi',_mushrooms)]:
        if not os.path.exists(f'{ROOT}/{KIT}/{name}-v01.glb'):
            reset(); mats=_materials(); obj=builder(mats); print('[ok] prop',name,export(obj,KIT,name,'v01'))


if __name__=='__main__': build()
