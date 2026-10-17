"""EmberEngine's 3D prop kits: the shared Blender library (headless, Blender 5.2).

Units: 1 Blender unit = 1 world unit = one hex radius (Engine2's scale: a human is 2.65 units, 1 unit ~ 0.68 m).
Every asset is authored at its in-game size, standing on z = 0, centred on the origin; the catalog's `height` equals
the authored height, so the engine's uniform fit (MapStage::placeModels) is a no-op. The glTF exporter turns Z-up
into glTF's Y-up.

Textures: content/models/textures (downscaled copies of Engine2's own photo textures, see its README); bark and
leaves are vertex colours, the convention Engine2's trees already use (dead-oak.glb, broadleaf.glb: "vertex bark").

Run a kit:  blender -b --python content/models/blender/kit_forest.py -- [asset-name ...]
Each asset is saved as <kit>/<name>-vNN.blend + .glb (a new version is a new file: nothing is overwritten).
"""
import bpy, bmesh, math, os, sys, json, random
from mathutils import Vector, Matrix, noise

ROOT = 'C:/EmberEngine/content/models'
TEX = ROOT + '/textures'
VERSION = 'v01'

_materials = {}
_images = {}


# ----------------------------------------------------------------------------------------------------- scene
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    s = bpy.context.scene
    s.unit_settings.system = 'METRIC'
    s.unit_settings.scale_length = 1.0
    _materials.clear()
    _images.clear()


def image(name, colorspace='sRGB'):
    if name in _images:
        return _images[name]
    path = f'{TEX}/{name}.jpg'
    im = bpy.data.images.load(path, check_existing=True)
    im.colorspace_settings.name = colorspace
    _images[name] = im
    return im


def material(key, color=None, normal=None, rough=None, roughness=0.85, metallic=0.0, tint=(1, 1, 1, 1), vertex_color=False,
             flat=None, emission=None):
    """A Principled material: photo texture (optional) x tint x vertex colour (optional); flat = a plain colour."""
    if key in _materials:
        return _materials[key]
    m = bpy.data.materials.new(key)
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    bsdf = nt.nodes.new('ShaderNodeBsdfPrincipled')
    nt.links.new(bsdf.outputs['BSDF'], out.inputs['Surface'])
    bsdf.inputs['Roughness'].default_value = roughness
    bsdf.inputs['Metallic'].default_value = metallic
    base = None
    if color:
        t = nt.nodes.new('ShaderNodeTexImage')
        t.image = image(color, 'sRGB')
        base = t.outputs['Color']
    if flat is not None:
        bsdf.inputs['Base Color'].default_value = (*flat[:3], 1.0)
    if tint != (1, 1, 1, 1) and base is not None:
        mix = nt.nodes.new('ShaderNodeMix')
        mix.data_type = 'RGBA'
        mix.blend_type = 'MULTIPLY'
        mix.inputs['Factor'].default_value = 1.0
        nt.links.new(base, mix.inputs[6])
        mix.inputs[7].default_value = tint
        base = mix.outputs[2]
    if vertex_color:
        ca = nt.nodes.new('ShaderNodeVertexColor')
        ca.layer_name = 'Col'
        if base is not None:
            mix = nt.nodes.new('ShaderNodeMix')
            mix.data_type = 'RGBA'
            mix.blend_type = 'MULTIPLY'
            mix.inputs['Factor'].default_value = 1.0
            nt.links.new(base, mix.inputs[6])
            nt.links.new(ca.outputs['Color'], mix.inputs[7])
            base = mix.outputs[2]
        else:
            base = ca.outputs['Color']
    if base is not None:
        nt.links.new(base, bsdf.inputs['Base Color'])
    if normal:
        t = nt.nodes.new('ShaderNodeTexImage')
        t.image = image(normal, 'Non-Color')
        nm = nt.nodes.new('ShaderNodeNormalMap')
        nm.inputs['Strength'].default_value = 0.8
        nt.links.new(t.outputs['Color'], nm.inputs['Color'])
        nt.links.new(nm.outputs['Normal'], bsdf.inputs['Normal'])
    if rough:
        t = nt.nodes.new('ShaderNodeTexImage')
        t.image = image(rough, 'Non-Color')
        nt.links.new(t.outputs['Color'], bsdf.inputs['Roughness'])
    if emission:
        bsdf.inputs['Emission Color'].default_value = (*emission[:3], 1.0)
        bsdf.inputs['Emission Strength'].default_value = emission[3]
    _materials[key] = m
    return m


# The kits' palette (one entry per look; the same material is shared by every part that uses it).
def M(name):
    table = {
        'stone': dict(color='stone-rubble', roughness=0.9),
        'sandstone': dict(color='stone-sandstone', roughness=0.9),
        'granite': dict(color='stone-rubble', roughness=0.92, tint=(0.75, 0.78, 0.8, 1)),
        'masonry': dict(color='masonry-grey', roughness=0.88),
        'masonry-mossy': dict(color='masonry-mossy', roughness=0.9),
        'masonry-city': dict(color='masonry-city', roughness=0.88),
        'masonry-crypt': dict(color='masonry-crypt', roughness=0.9),
        'planks': dict(color='planks-pale', roughness=0.7),
        'planks-dark': dict(color='planks-dark', roughness=0.72),
        'planks-iron': dict(color='planks-iron', roughness=0.7),
        'planks-worn': dict(color='planks-pale', roughness=0.8, tint=(0.72, 0.62, 0.5, 1)),
        'iron': dict(flat=(0.16, 0.15, 0.14), roughness=0.55, metallic=0.9),
        'iron-rust': dict(flat=(0.28, 0.17, 0.11), roughness=0.75, metallic=0.6),
        'brass': dict(flat=(0.55, 0.42, 0.2), roughness=0.4, metallic=0.9),
        'glass': dict(flat=(0.85, 0.9, 0.95), roughness=0.1, metallic=0.0),
        'snow': dict(color='snow', normal='snow-normal', rough='snow-rough', roughness=0.6),
        'moss': dict(color='mossy-ground', roughness=0.95, tint=(0.75, 0.9, 0.55, 1)),
        'forest-floor': dict(color='forest-floor', normal='forest-floor-normal', roughness=0.95),
        'ruins-floor': dict(color='ruins-floor', normal='ruins-floor-normal', roughness=0.92),
        'bark': dict(vertex_color=True, roughness=0.93),
        'bark-pale': dict(vertex_color=True, roughness=0.9),
        'leaves': dict(vertex_color=True, roughness=0.72),
        'needles': dict(vertex_color=True, roughness=0.75),
        'burlap': dict(flat=(0.52, 0.42, 0.28), roughness=1.0),
        'rope': dict(flat=(0.6, 0.5, 0.32), roughness=0.95),
        'bone': dict(vertex_color=True, roughness=0.6),
        'canvas': dict(flat=(0.55, 0.45, 0.32), roughness=0.9),
        'dark-glass': dict(flat=(0.08, 0.1, 0.07), roughness=0.15),
        'clay': dict(flat=(0.45, 0.3, 0.2), roughness=0.8),
        'ember-crystal': dict(flat=(0.55, 0.06, 0.04), roughness=0.2, emission=(0.9, 0.12, 0.05, 1.5)),
        'fungus': dict(vertex_color=True, roughness=0.7),
        # the second wave (2026-10-16 night: the user's long list)
        'ice': dict(flat=(0.72, 0.84, 0.92), roughness=0.12),
        'lichen': dict(color='mossy-ground', roughness=0.95, tint=(0.8, 0.85, 0.6, 1)),
        'leather': dict(flat=(0.36, 0.22, 0.13), roughness=0.75),
        'wicker': dict(color='planks-pale', roughness=0.9, tint=(0.95, 0.8, 0.55, 1)),
        'cloth-red': dict(flat=(0.42, 0.1, 0.08), roughness=0.95),
        'cloth-dark': dict(flat=(0.16, 0.14, 0.16), roughness=0.95),
        'wool': dict(flat=(0.6, 0.55, 0.45), roughness=1.0),
        'ember-glow': dict(flat=(0.7, 0.12, 0.04), roughness=0.25, emission=(1.0, 0.25, 0.05, 4.0)),
        'bio-glow': dict(flat=(0.2, 0.75, 0.65), roughness=0.5, emission=(0.2, 0.9, 0.7, 2.5)),
        'light-crystal': dict(flat=(0.6, 0.8, 0.95), roughness=0.15, emission=(0.5, 0.75, 1.0, 3.0)),
        'pale-crystal': dict(flat=(0.78, 0.8, 0.84), roughness=0.2),
        'amethyst': dict(flat=(0.4, 0.22, 0.55), roughness=0.2),
        'gold': dict(flat=(0.85, 0.65, 0.25), roughness=0.3, metallic=1.0),
        'silver': dict(flat=(0.75, 0.75, 0.78), roughness=0.35, metallic=1.0),
        'bronze': dict(flat=(0.45, 0.3, 0.16), roughness=0.5, metallic=0.9),
        'paper': dict(flat=(0.75, 0.68, 0.52), roughness=0.9),
        'coal': dict(flat=(0.06, 0.06, 0.06), roughness=0.9),
        'ore': dict(color='stone-rubble', roughness=0.8, tint=(0.6, 0.55, 0.5, 1)),
        'petrified': dict(color='stone-rubble', roughness=0.9, tint=(0.55, 0.5, 0.45, 1)),
        'passage-dark': dict(flat=(0.02, 0.018, 0.02), roughness=1.0),
    }
    return material(name, **table[name])


# ------------------------------------------------------------------------------------------------- primitives
def _finish_new(name, mat):
    o = bpy.context.active_object
    o.name = name
    if mat is not None:
        o.data.materials.append(mat)
    return o


def cylinder(name, r1, r2, h, loc=(0, 0, 0), rot=(0, 0, 0), segs=24, mat=None, caps=True):
    bpy.ops.mesh.primitive_cone_add(vertices=segs, radius1=r1, radius2=r2, depth=h, location=loc, rotation=rot,
                                    end_fill_type='NGON' if caps else 'NOTHING')
    return _finish_new(name, mat)


def box(name, size, loc=(0, 0, 0), rot=(0, 0, 0), mat=None):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc, rotation=rot)
    o = _finish_new(name, mat)
    o.scale = size
    apply_transform(o)
    return o


def sphere(name, r, loc=(0, 0, 0), subdiv=3, mat=None, scale=(1, 1, 1)):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=subdiv, radius=r, location=loc)
    o = _finish_new(name, mat)
    o.scale = scale
    apply_transform(o)
    return o


def torus(name, major, minor, loc=(0, 0, 0), rot=(0, 0, 0), mat=None, segs=32, ring=12):
    bpy.ops.mesh.primitive_torus_add(major_radius=major, minor_radius=minor, location=loc, rotation=rot,
                                     major_segments=segs, minor_segments=ring)
    return _finish_new(name, mat)


def plane(name, size, loc=(0, 0, 0), rot=(0, 0, 0), mat=None):
    bpy.ops.mesh.primitive_plane_add(size=1.0, location=loc, rotation=rot)
    o = _finish_new(name, mat)
    o.scale = (size[0], size[1], 1)
    apply_transform(o)
    return o


def apply_transform(o):
    bpy.ops.object.select_all(action='DESELECT')
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def aim(o, direction):
    """Turn a z-aligned part (cylinder) along `direction`."""
    d = Vector(direction).normalized()
    o.rotation_euler = Vector((0, 0, 1)).rotation_difference(d).to_euler()


# --------------------------------------------------------------------------------------------------- modifiers
def displace(o, strength, scale, seed=0, mid=0.5, coords='GLOBAL'):
    t = bpy.data.textures.new(f'noise{seed}_{scale}', type='CLOUDS')
    t.noise_scale = scale
    t.noise_depth = 3
    t.noise_basis = 'ORIGINAL_PERLIN'
    md = o.modifiers.new('displace', 'DISPLACE')
    md.texture = t
    md.strength = strength
    md.mid_level = mid
    md.texture_coords = coords
    if coords == 'GLOBAL' and seed:
        # a different patch of the same noise per seed: shift the object's texture space through an empty
        e = bpy.data.objects.new(f'noise_origin{seed}', None)
        bpy.context.collection.objects.link(e)
        e.location = (seed * 7.31, seed * 3.17, seed * 1.23)
        md.texture_coords = 'OBJECT'
        md.texture_coords_object = e
    return md


def subdivide(o, levels, simple=False):
    md = o.modifiers.new('subd', 'SUBSURF')
    md.levels = md.render_levels = levels
    md.subdivision_type = 'SIMPLE' if simple else 'CATMULL_CLARK'
    return md


def bevel(o, width, segments=2, angle=30):
    md = o.modifiers.new('bevel', 'BEVEL')
    md.width = width
    md.segments = segments
    md.limit_method = 'ANGLE'
    md.angle_limit = math.radians(angle)
    return md


def apply_mods(o):
    """Bake every modifier into the mesh (depsgraph evaluation), keeping materials."""
    if not o.modifiers:
        return o
    dg = bpy.context.evaluated_depsgraph_get()
    ev = o.evaluated_get(dg)
    mesh = bpy.data.meshes.new_from_object(ev, preserve_all_data_layers=True, depsgraph=dg)
    old = o.data
    o.modifiers.clear()
    o.data = mesh
    if old.users == 0:
        bpy.data.meshes.remove(old)
    return o


# ---------------------------------------------------------------------------------------------- mesh helpers
def box_uv(o, scale=1.0, offset=(0.0, 0.0)):
    """Per-face planar projection along the face's dominant axis (a triplanar unwrap): seamless on boxes and
    cylinders, good enough on rocks. `scale` = world units per texture tile."""
    me = o.data
    bm = bmesh.new()
    bm.from_mesh(me)
    uv = bm.loops.layers.uv.verify()
    inv = 1.0 / scale
    for f in bm.faces:
        n = f.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        for l in f.loops:
            p = l.vert.co
            if ax == 0:
                u, v = p.y * (1 if n.x > 0 else -1), p.z
            elif ax == 1:
                u, v = p.x * (-1 if n.y > 0 else 1), p.z
            else:
                u, v = p.x, p.y * (1 if n.z > 0 else -1)
            l[uv].uv = (u * inv + offset[0], v * inv + offset[1])
    bm.to_mesh(me)
    bm.free()


def smooth(o, angle=35.0):
    """Smooth shading with sharp edges where faces meet at more than `angle` degrees."""
    me = o.data
    for p in me.polygons:
        p.use_smooth = True
    bm = bmesh.new()
    bm.from_mesh(me)
    limit = math.radians(angle)
    for e in bm.edges:
        if len(e.link_faces) == 2:
            e.smooth = e.calc_face_angle(0.0) <= limit
    bm.to_mesh(me)
    bm.free()


def flat(o):
    for p in o.data.polygons:
        p.use_smooth = False


def vertex_colors(o, base, variation=0.15, scale=0.6, seed=0, gradient=None, select=None):
    """A 'Col' corner colour: `base` (r, g, b) shaded by 3D noise (+-variation), optionally darker toward the
    base (gradient = (z0, z1, dark)). `select(co) -> colour or None` overrides single verts (moss, snow)."""
    me = o.data
    attr = me.color_attributes.get('Col') or me.color_attributes.new('Col', 'BYTE_COLOR', 'CORNER')
    off = Vector((seed * 13.7, seed * 5.3, seed * 2.9))
    for poly in me.polygons:
        for li in poly.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            n = noise.noise((co + off) / scale) * variation
            k = 1.0 + n
            c = [base[0] * k, base[1] * k, base[2] * k]
            if gradient:
                z0, z1, dark = gradient
                t = min(1.0, max(0.0, (co.z - z0) / max(1e-5, z1 - z0)))
                k2 = dark + (1 - dark) * t
                c = [c[0] * k2, c[1] * k2, c[2] * k2]
            if select:
                s = select(co, poly.normal)
                if s is not None:
                    c = list(s)
            attr.data[li].color = (min(1, max(0, c[0])), min(1, max(0, c[1])), min(1, max(0, c[2])), 1.0)


def set_material_by_normal(o, mat, up=0.55, noise_scale=0.0, seed=0, threshold=0.0):
    """Give the faces facing up (normal.z > up) a second material: moss on a boulder, snow on a cap."""
    if mat.name not in [m.name for m in o.data.materials if m]:
        o.data.materials.append(mat)
    idx = [m.name if m else '' for m in o.data.materials].index(mat.name)
    me = o.data
    for p in me.polygons:
        z = p.normal.z
        if noise_scale:
            z += noise.noise((p.center + Vector((seed, seed, 0))) / noise_scale) * threshold
        if z > up:
            p.material_index = idx


def set_material_where(o, mat, pred):
    """`pred(center, normal) -> bool` picks the faces that get `mat` (a second material slot)."""
    if mat.name not in [m.name for m in o.data.materials if m]:
        o.data.materials.append(mat)
    idx = [m.name if m else '' for m in o.data.materials].index(mat.name)
    for p in o.data.polygons:
        if pred(p.center, p.normal):
            p.material_index = idx


def _new_mesh_object(name, bm, mat):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(o)
    if mat is not None:
        me.materials.append(mat)
    bpy.context.view_layer.objects.active = o
    return o


def lathe(name, profile, segs=24, loc=(0, 0, 0), mat=None):
    """Revolve a (radius, z) profile around z. Start and end at radius 0 for a closed solid."""
    bm = bmesh.new()
    verts = [bm.verts.new((r, 0.0, z)) for r, z in profile]
    edges = [bm.edges.new((verts[i], verts[i + 1])) for i in range(len(verts) - 1)]
    bmesh.ops.spin(bm, geom=verts + edges, cent=(0, 0, 0), axis=(0, 0, 1), dvec=(0, 0, 0), angle=math.tau, steps=segs,
                   use_merge=True, use_normal_flip=False)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    o = _new_mesh_object(name, bm, mat)
    o.location = loc
    apply_transform(o)
    return o


def prism(name, profile, length, loc=(0, 0, 0), rot=(0, 0, 0), mat=None):
    """A closed (y, z) polygon extruded along x by `length`, centred."""
    bm = bmesh.new()
    back = [bm.verts.new((-length * 0.5, y, z)) for y, z in profile]
    front = [bm.verts.new((length * 0.5, y, z)) for y, z in profile]
    bm.faces.new(back[::-1])
    bm.faces.new(front)
    n = len(profile)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((back[i], back[j], front[j], front[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    o = _new_mesh_object(name, bm, mat)
    o.location = loc
    o.rotation_euler = rot
    apply_transform(o)
    return o


def half_circle(r, segs=12, z0=0.0):
    return [(r * math.cos(math.pi * i / segs), z0 + r * math.sin(math.pi * i / segs)) for i in range(segs + 1)]


def boolean(o, cutter, op='DIFFERENCE'):
    md = o.modifiers.new('bool', 'BOOLEAN')
    md.operation = op
    md.object = cutter
    md.solver = 'EXACT'
    apply_mods(o)
    bpy.data.objects.remove(cutter, do_unlink=True)
    return o


def clamp_floor(o, z=0.0):
    """Flatten whatever hangs below z (a rock's buried half)."""
    for v in o.data.vertices:
        if v.co.z < z:
            v.co.z = z


def join(objs, name):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.join()
    o = bpy.context.active_object
    o.name = name
    o.data.name = name
    return o


def ground(o):
    """Stand the asset on z = 0, centred in x/y (the pivot the engine expects)."""
    apply_transform(o)  # a rotated first part (a log along x) would otherwise be measured in its own frame
    me = o.data
    xs = [v.co.x for v in me.vertices]
    ys = [v.co.y for v in me.vertices]
    zmin = min(v.co.z for v in me.vertices)
    cx, cy = (min(xs) + max(xs)) * 0.5, (min(ys) + max(ys)) * 0.5
    for v in me.vertices:
        v.co.x -= cx
        v.co.y -= cy
        v.co.z -= zmin
    o.location = (0, 0, 0)


def rand(seed):
    return random.Random(seed)


# ------------------------------------------------------------------------------------------------------ export
def export(o, kit, name, version=VERSION):
    folder = f'{ROOT}/{kit}'
    os.makedirs(folder, exist_ok=True)
    stem = f'{folder}/{name}-{version}'
    if os.path.exists(stem + '.glb'):
        raise RuntimeError(f'{stem}.glb exists: bump the version instead of overwriting')
    bpy.ops.object.select_all(action='DESELECT')
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    kw = dict(filepath=stem + '.glb', export_format='GLB', use_selection=True, export_apply=True, export_yup=True,
              export_image_format='AUTO', export_texcoords=True, export_normals=True, export_materials='EXPORT',
              export_animations=False, export_skins=False, export_lights=False, export_cameras=False)
    props = bpy.ops.export_scene.gltf.get_rna_type().properties.keys()
    if 'export_vertex_color' in props:
        kw['export_vertex_color'] = 'MATERIAL'
    if 'export_jpeg_quality' in props:
        kw['export_jpeg_quality'] = 90
    bpy.ops.export_scene.gltf(**kw)
    bpy.ops.wm.save_as_mainfile(filepath=stem + '.blend', compress=True, relative_remap=True)
    me = o.data
    xs = [v.co.x for v in me.vertices]; ys = [v.co.y for v in me.vertices]; zs = [v.co.z for v in me.vertices]
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    return {'name': name, 'kit': kit, 'glb': os.path.relpath(stem + '.glb', ROOT).replace('\\', '/'),
            'size': [round(max(xs) - min(xs), 3), round(max(ys) - min(ys), 3), round(max(zs) - min(zs), 3)],
            'height': round(max(zs) - min(zs), 3), 'triangles': tris, 'materials': [m.name for m in me.materials if m],
            'glb_bytes': os.path.getsize(stem + '.glb')}


def run(kit, builders):
    """builders: [(name, fn)], fn() -> the finished object (grounded, one object). Picks by name from argv."""
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    only = set(argv)
    reports, errors = [], []
    for name, fn in builders:
        if only and name not in only:
            continue
        try:
            reset()
            o = fn()
            reports.append(export(o, kit, name))
            print(f'[ok] {kit}/{name}: {reports[-1]["triangles"]} tris, {reports[-1]["size"]}')
        except Exception as e:  # keep the sprint going: one failed asset must not stop the kit
            import traceback
            traceback.print_exc()
            errors.append({'name': name, 'error': str(e)})
    print('JSON_BEGIN' + json.dumps({'kit': kit, 'assets': reports, 'errors': errors}) + 'JSON_END')
