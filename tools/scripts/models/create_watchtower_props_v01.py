"""Create three original, physically based 3D props for the Watchtower upper floors."""
import bpy
import math
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'content/models/watchtower-v01'
TEXTURES = ROOT / 'content/models/textures'
OUT.mkdir(parents=True, exist_ok=True)
bpy.context.preferences.filepaths.save_version = 0


def clear():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for block in bpy.data.materials: bpy.data.materials.remove(block)


def material(name, texture, metallic=0.0, roughness=0.75):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get('Principled BSDF')
    img = bpy.data.images.load(str(TEXTURES / texture), check_existing=True)
    tex = m.node_tree.nodes.new('ShaderNodeTexImage')
    tex.image = img
    m.node_tree.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
    bsdf.inputs['Metallic'].default_value = metallic
    bsdf.inputs['Roughness'].default_value = roughness
    return m

IRON = None; OAK = None; STONE = None; PINE = None

def finish(name):
    objs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    obj = bpy.context.object; obj.name = name
    # Authoring uses Y-up coordinates; convert the joined model to Blender's Z-up before glTF export.
    obj.rotation_euler.x = math.pi / 2
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(island_margin=0.008)
    bpy.ops.object.mode_set(mode='OBJECT')
    # Origin at the object's centre of footprint, with its base on Y-up zero for glTF.
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    bpy.context.scene.unit_settings.system = 'METRIC'
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active = obj
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / f'{name}.blend'))
    bpy.ops.export_scene.gltf(filepath=str(OUT / f'{name}.glb'), export_format='GLB', use_selection=True, export_apply=True)


def cube(name, loc, scale, mat, bevel=0.035):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc); o=bpy.context.object; o.name=name; o.dimensions=scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True); o.data.materials.append(mat)
    if bevel:
        mod=o.modifiers.new('Soft worn edges','BEVEL'); mod.width=bevel; mod.segments=3
        o.modifiers.new('Weighted corner normals','WEIGHTED_NORMAL')
    return o


def cyl(name, loc, radius, depth, mat, vertices=24, top=None):
    if top is None: bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=loc)
    else: bpy.ops.mesh.primitive_cone_add(vertices=vertices, radius1=radius, radius2=top, depth=depth, location=loc)
    o=bpy.context.object; o.name=name; o.rotation_euler[0]=math.pi/2; o.data.materials.append(mat)
    mod=o.modifiers.new('Worn bevel','BEVEL'); mod.width=min(0.04,depth*0.12); mod.segments=2
    o.modifiers.new('Weighted corner normals','WEIGHTED_NORMAL'); return o


def rod(name, a, b, radius, mat, vertices=12):
    a,b=Vector(a),Vector(b); delta=b-a
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=delta.length, location=(a+b)/2)
    o=bpy.context.object; o.name=name; o.rotation_mode='QUATERNION'; o.rotation_quaternion=delta.to_track_quat('Z','Y'); o.data.materials.append(mat)
    mod=o.modifiers.new('Edge wear','BEVEL'); mod.width=0.015; mod.segments=2
    return o


def torus(name, loc, major, minor, mat):
    bpy.ops.mesh.primitive_torus_add(major_radius=major, minor_radius=minor, major_segments=32, minor_segments=8, location=loc)
    o=bpy.context.object; o.name=name; o.rotation_euler[0]=math.pi/2; o.data.materials.append(mat); return o


def make_brazier():
    clear(); global IRON, STONE; IRON=material('Forged, smoke-darkened iron','wt-iron.jpg',0.72,0.5); STONE=material('Carved grey limestone','wt-limestone.jpg',0.0,0.86)
    # Three splayed feet support a faceted basin on a restrained stone plinth.
    cyl('Brazier foot', (0,0,0.08),0.56,0.16,STONE,12)
    cyl('Brazier plinth',(0,0,0.22),0.42,0.16,IRON,12)
    for angle in (0,2*math.pi/3,4*math.pi/3):
        x,z=math.cos(angle),math.sin(angle)
        rod('Splayed iron leg',(x*0.43,0.31,z*0.43),(x*0.20,1.12,z*0.20),0.045,IRON)
        rod('Foot brace',(x*0.43,0.32,z*0.43),(x*0.30,0.52,z*0.30),0.028,IRON)
    cyl('Central standard',(0,0.70,0),0.075,0.86,IRON,16)
    torus('Lower forged collar',(0,0.52,0),0.12,0.024,IRON)
    torus('Upper forged collar',(0,0.99,0),0.16,0.027,IRON)
    # Deep open bowl: tapered stone basin, iron outer hoops, and an empty grated top.
    cyl('Basin body',(0,1.22,0),0.31,0.38,STONE,16,0.48)
    torus('Iron basin rim',(0,1.41,0),0.43,0.034,IRON)
    torus('Basin lower band',(0,1.08,0),0.42,0.022,IRON)
    for i in range(8):
        a=i*math.tau/8
        rod('Grate bar',(math.cos(a)*0.08,1.414,math.sin(a)*0.08),(math.cos(a)*0.38,1.414,math.sin(a)*0.38),0.012,IRON,8)
    # The signal brazier is unlit; gameplay fire remains a separate protected effect.
    finish('watchtower-signal-brazier-v01')


def make_arms_rack():
    clear(); global IRON, OAK, PINE; IRON=material('Old forged iron','wt-iron.jpg',0.78,0.48); OAK=material('Oiled dark oak','wt-dark-oak.jpg',0.0,0.82); PINE=material('Pale pine end grain','wt-pine.jpg',0.0,0.86)
    # Stable trestle frame with mortised rails and iron straps.
    for x in (-0.66,0.66):
        cube('Trestle foot',(x,0.10,0),(0.16,0.20,1.48),OAK,0.045)
        rod('Angled trestle',(x,0.18,-0.68),(x*0.72,1.48,-0.48),0.07,OAK)
        rod('Angled trestle',(x,0.18,0.68),(x*0.72,1.48,0.48),0.07,OAK)
    cube('Lower support rail',(0,0.51,0),(0.14,0.14,1.54),PINE,0.025)
    cube('Upper notched rail',(0,1.44,0),(0.22,0.20,1.58),OAK,0.04)
    for x in (-0.66,0.66):
        cube('Forged binding',(x,0.83,0),(0.20,0.10,1.58),IRON,0.02)
    # Five distinct long weapons: tapered wooden shafts, restrained steel heads, and butts.
    for i in range(5):
        z=-0.58+i*0.29
        rod('Ash spear shaft',(-0.04,0.50,z),(0.03,2.20,z),0.026,PINE,10)
        cyl('Steel ferrule',(0.03,2.16,z),0.043,0.14,IRON,8)
        # Alternating spear and narrow leaf-shaped halberd heads, simplified as beveled forged steel.
        rod('Spear socket',(0.03,2.22,z),(0.03,2.38,z),0.034,IRON,8)
        if i in (1,3):
            cube('Halberd blade',(0.03,2.28,z+0.09),(0.055,0.30,0.14),IRON,0.025)
    for x in (-0.55,0.55):
        rod('Wall anchor',(x,0.32,-0.72),(x,0.32,-0.50),0.035,IRON,8)
    finish('watchtower-arms-rack-v01')


def make_command_table():
    clear(); global IRON, OAK, STONE; IRON=material('Blackened iron straps','wt-iron.jpg',0.8,0.48); OAK=material('Carved dark oak','wt-dark-oak.jpg',0.0,0.82); STONE=material('Limestone feet','wt-limestone.jpg',0.0,0.86); PINE=material('Pine map board','wt-pine.jpg',0.0,0.86)
    # Solid trestle table, broad plank top, cross-bracing and iron corner plates.
    cube('Table top',(0,0.92,0),(2.05,0.16,1.32),OAK,0.055)
    for x in (-0.78,0.78):
        for z in (-0.48,0.48): cube('Limestone foot',(x,0.12,z),(0.22,0.24,0.22),STONE,0.03)
        rod('Trestle leg',(x*0.78,0.18,-0.50),(x,0.87,-0.50),0.075,OAK)
        rod('Trestle leg',(x*0.78,0.18,0.50),(x,0.87,0.50),0.075,OAK)
    cube('Long stretcher',(0,0.43,0),(0.12,0.13,1.52),OAK,0.025)
    cube('End stretcher',(0,0.46,0),(1.52,0.13,0.12),OAK,0.025)
    for x in (-0.94,0.94):
        cube('Forged table strap',(x,0.99,0),(0.08,0.06,1.34),IRON,0.015)
    # Raised map frame and carved marker pegs: no printed text or unrequested glowing effects.
    cube('Inset map board',(0,1.015,0),(1.24,0.035,0.78),PINE,0.018)
    for x,z in [(-0.42,-0.24),(-0.18,0.14),(0.22,0.18),(0.39,-0.08),(0.02,-0.05)]:
        cyl('Map marker',(x,1.055,z),0.032,0.055,IRON,8)
    for x in (-0.83,0.83):
        for z in (-0.50,0.50): cyl('Iron corner rivet',(x,1.02,z),0.025,0.035,IRON,8)
    finish('watchtower-command-table-v01')

make_brazier()
make_arms_rack()
make_command_table()
print(f'created three Watchtower GLB and Blend source models in {OUT}')

