"""EmberEngine's own 3D pieces for spell effects (headless Blender 5.2): the Nuvem de Adagas' dagger.

Our own model, built from scratch. Poly Haven's ornate_medieval_dagger (CC0) was only taken apart to learn from: a low-poly
mesh whose detail lives in the textures (engravings, leather grain and stitching in the normal map), a blade section of a few
lines with a sharp edge 0.7-0.8 mm thick, one material and one texture set for the whole weapon. Nothing of its mesh or its
textures is in this file.

  cloud-dagger v01  steel: a leaf blade with a fuller and an engraved ricasso (an ember ring with three flames), a crescent
                    guard with ball finials and a cabochon boss, a leather cord grip bound by brass wire, a faceted pommel.
  cloud-dagger v02  arcane: the same geometry; blackened steel, runes glowing violet in the fuller, a faint violet light along
                    both edges, amethysts in the boss and the pommel, a dark leather grip bound by silver wire.

Modelled in metres (0.34 m long), exported in world units (1 unit = 0.68 m). Pivot = the guard's face on the blade side, the
point along +z (+y in the glTF), as the spell's dagger already is (DaggerFx). One material per version: base colour, ORM
(occlusion, roughness, metal), normal and, for v02, emissive, baked at 2048 from procedural materials kept in the .blend.

Run:  blender -b --python content/models/blender/kit_spells.py -- [v01] [v02]
"""
import bpy, bmesh, math, os, sys, json
import numpy as np
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import emberlib as E

KIT = 'spells'
NAME = 'cloud-dagger'
OUT = f'{E.ROOT}/{KIT}'
TEXDIR = f'{OUT}/textures'
UNITS = 1.0 / 0.68           # metres -> world units
BAKE = 2048
VIOLET = (0.62, 0.4, 1.0)    # DaggerFx kViolet

# ---------------------------------------------------------------------------------------------------- the blade
L = 0.215                     # guard face to point
RICASSO = 0.022               # the unsharpened base
PLUNGE = 0.008                # where the ricasso widens into the blade
F = (-1.0, -0.62, -0.34, -0.22, 0.0, 0.22, 0.34, 0.62, 1.0)   # section points across the width, as fractions of it


def smoothstep(a, b, x):
    t = min(1.0, max(0.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


def blade_half_width(z):
    if z <= RICASSO:
        return 0.0090
    t = (z - RICASSO) / (L - RICASSO)
    hw = 0.0101 * (1.0 + 0.10 * math.sin(math.pi * min(t / 0.7, 1.0))) * max(0.0, 1.0 - t ** 2.6) ** 0.55
    return 0.0090 + (hw - 0.0090) * smoothstep(RICASSO, RICASSO + PLUNGE, z)


def blade_section(z):
    """Half-thicknesses (y >= 0) at the points F: a sharp edge, a bevel to the flats, the fuller in the middle."""
    t = max(0.0, (z - RICASSO) / (L - RICASSO))
    ric = [0.0013 if abs(f) == 1.0 else 0.0021 for f in F]
    tb = 0.0004 + 0.0019 * (1.0 - 0.8 * t)
    d = 0.0009 * smoothstep(0.0, 0.06, t) * (1.0 - smoothstep(0.62, 0.85, t))
    sharp = []
    for f in F:
        a = abs(f)
        sharp.append(0.00035 if a == 1.0 else tb if a >= 0.34 else tb - d * 0.55 if a >= 0.22 else tb - d)
    b = smoothstep(RICASSO, RICASSO + PLUNGE, z)
    return [r + (s - r) * b for r, s in zip(ric, sharp)]


def build_blade():
    zs = [i * RICASSO / 4 for i in range(4)]
    zs += [RICASSO + i * PLUNGE / 4 for i in range(4)]
    z = RICASSO + PLUNGE
    while z < L - 0.003:
        zs.append(z)
        z += 0.004
    zs.append(L - 0.0015)
    bm = bmesh.new()
    rings, edge_verts = [], set()
    for z in zs:
        hw = blade_half_width(z)
        ys = blade_section(z)
        top = [bm.verts.new((f * hw, y, z)) for f, y in zip(F, ys)]
        bottom = [bm.verts.new((f * hw, -y, z)) for f, y in list(zip(F, ys))[7:0:-1]]
        edge_verts.update((top[0], top[-1]))
        rings.append(top + bottom)
    n = len(rings[0])
    for a, b in zip(rings, rings[1:]):
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((a[i], a[j], b[j], b[i]))
    tip = bm.verts.new((0.0, 0.0, L))
    for i in range(n):
        bm.faces.new((rings[-1][i], rings[-1][(i + 1) % n], tip))
    bm.faces.new(rings[0][::-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    edge_index = {v.index for v in edge_verts}
    bm.verts.index_update()
    edge_index = {v.index for v in edge_verts}
    o = E._new_mesh_object('blade', bm, None)
    me = o.data
    attr = me.color_attributes.new('Edge', 'FLOAT_COLOR', 'POINT')
    for i, v in enumerate(me.vertices):
        attr.data[i].color = (1.0, 1.0, 1.0, 1.0) if i in edge_index else (0.0, 0.0, 0.0, 1.0)
    E.smooth(o, 10.0)
    return o


# ---------------------------------------------------------------------------------------------------- the hilt
def curve_tube(name, points, radius, radii=None, closed=False, resolution=4):
    cu = bpy.data.curves.new(name, 'CURVE')
    cu.dimensions = '3D'
    cu.bevel_mode = 'ROUND'
    cu.bevel_depth = radius
    cu.bevel_resolution = resolution
    cu.use_fill_caps = True
    sp = cu.splines.new('POLY')
    sp.points.add(len(points) - 1)
    for i, p in enumerate(points):
        sp.points[i].co = (*p, 1.0)
        sp.points[i].radius = radii[i] if radii else 1.0
    sp.use_cyclic_u = closed
    o = bpy.data.objects.new(name, cu)
    bpy.context.collection.objects.link(o)
    bpy.ops.object.select_all(action='DESELECT')
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.convert(target='MESH')
    o = bpy.context.active_object
    o.name = name
    return o


def build_guard():
    parts = []
    block = E.box('guard-block', (0.026, 0.015, 0.016), loc=(0, 0, -0.008))
    E.bevel(block, 0.0022, segments=3, angle=60)
    E.apply_mods(block)
    E.smooth(block, 40)
    parts.append(block)
    for side in (1, -1):
        pts, radii = [], []
        for i in range(25):
            s = i / 24
            x = 0.012 + 0.034 * s
            z = -0.0085 + 0.0215 * s ** 2.2 - 0.002 * math.sin(math.pi * s)
            pts.append((side * x, 0.0, z))
            radii.append(1.0 - 0.38 * s)
        q = curve_tube(f'quillon{side}', pts, 0.0042, radii, resolution=4)
        E.smooth(q, 60)
        parts.append(q)
        fin = E.sphere(f'finial{side}', 0.0049, loc=(side * 0.0465, 0, 0.0145), subdiv=3)
        E.smooth(fin, 80)
        parts.append(fin)
    gems = []
    for face in (1, -1):
        rim = E.cylinder(f'boss-rim{face}', 0.0062, 0.0062, 0.0022, loc=(0, face * 0.0081, -0.008),
                         rot=(math.pi / 2, 0, 0), segs=32)
        E.bevel(rim, 0.0005, segments=2, angle=50)
        E.apply_mods(rim)
        E.smooth(rim, 40)
        parts.append(rim)
        dome = E.sphere(f'boss-gem{face}', 0.0046, loc=(0, face * 0.0090, -0.008), subdiv=4, scale=(1, 0.45, 1))
        E.smooth(dome, 80)
        gems.append(dome)
    return parts, gems


GRIP_TOP, GRIP_BOTTOM = -0.016, -0.088


def grip_radius(z):
    t = (GRIP_TOP - z) / (GRIP_TOP - GRIP_BOTTOM)
    return 0.0086 + 0.0014 * math.sin(math.pi * t)


def build_grip():
    core = E.lathe('grip-core', [(0, GRIP_TOP - 0.004)] + [(grip_radius(GRIP_TOP - 0.004 - i * 0.004), GRIP_TOP - 0.004 - i * 0.004)
                                                          for i in range(17)] + [(0, GRIP_BOTTOM + 0.003)], segs=24)
    E.smooth(core, 60)
    ferrules = []
    for z0, d in ((GRIP_TOP, -1), (GRIP_BOTTOM, 1)):
        prof = [(0, z0), (0.0101, z0), (0.0103, z0 + d * 0.0015), (0.0097, z0 + d * 0.0026), (0.0103, z0 + d * 0.0037),
                (0.0099, z0 + d * 0.0058), (0, z0 + d * 0.0058)]
        if d < 0:
            prof = prof
        f = E.lathe(f'ferrule{d}', prof, segs=32)
        E.smooth(f, 40)
        ferrules.append(f)
    z_top, z_bottom = GRIP_TOP - 0.0062, GRIP_BOTTOM + 0.0062
    turns, pitch = 9, (z_top - z_bottom) / 9.0
    cord_pts, wire_pts = [], []
    steps = turns * 28
    for i in range(steps + 1):
        a = math.tau * i / 28
        z = z_top - pitch * i / 28
        r = grip_radius(z) + 0.0004
        cord_pts.append((r * math.cos(a), r * math.sin(a), z))
    for i in range(steps - 14 + 1):
        a = math.tau * i / 28 + math.pi
        z = z_top - pitch * 0.5 - pitch * i / 28
        r = grip_radius(z) + 0.0011
        wire_pts.append((r * math.cos(a), r * math.sin(a), z))
    cord = curve_tube('cord', cord_pts, 0.0019, resolution=3)
    E.smooth(cord, 70)
    wire = curve_tube('wire', wire_pts, 0.00062, resolution=2)
    E.smooth(wire, 70)
    return core, ferrules, cord, wire


def build_pommel():
    z0 = GRIP_BOTTOM - 0.0058
    prof = [(0, z0), (0.0084, z0), (0.0074, z0 - 0.003), (0.0118, z0 - 0.010), (0.0142, z0 - 0.018), (0.0128, z0 - 0.026),
            (0.0072, z0 - 0.031), (0.0046, z0 - 0.0325), (0, z0 - 0.0325)]
    p = E.lathe('pommel', prof, segs=8)
    p.rotation_euler = (0, 0, math.pi / 8)
    E.apply_transform(p)
    E.smooth(p, 30)
    button = E.sphere('pommel-gem', 0.0038, loc=(0, 0, z0 - 0.0335), subdiv=4, scale=(1, 1, 0.8))
    E.smooth(button, 80)
    return p, button


# -------------------------------------------------------------------------------------------- engraving masks
def draw_mask(x0, x1, z0, z1, px_per_m, strokes, dots=()):
    """A mask image over the rectangle [x0, x1] x [z0, z1] of the blade's face (seen from +y): 1 inside the strokes
    (polylines of (x, z) in metres, each with its width) and the dots ((x, z), r)."""
    w, h = int((x1 - x0) * px_per_m), int((z1 - z0) * px_per_m)
    X = x0 + (np.arange(w) + 0.5) / px_per_m
    Z = z0 + (np.arange(h) + 0.5) / px_per_m
    X, Z = np.meshgrid(X, Z)
    aa = 1.0 / px_per_m
    m = np.zeros((h, w), np.float32)
    for pts, width in strokes:
        for (ax, az), (bx, bz) in zip(pts, pts[1:]):
            dx, dz = bx - ax, bz - az
            t = np.clip(((X - ax) * dx + (Z - az) * dz) / max(dx * dx + dz * dz, 1e-12), 0.0, 1.0)
            d = np.hypot(X - ax - t * dx, Z - az - t * dz)
            m = np.maximum(m, np.clip((width * 0.5 - d) / aa + 0.5, 0.0, 1.0))
    for (cx, cz), r in dots:
        d = np.hypot(X - cx, Z - cz)
        m = np.maximum(m, np.clip((r - d) / aa + 0.5, 0.0, 1.0))
    return m, (x0, x1, z0, z1)


def ricasso_mask():
    s = []
    ring = [(0.0038 * math.cos(a), 0.0105 + 0.0038 * math.sin(a)) for a in np.linspace(0, math.tau, 64)]
    s.append((ring, 0.0003))
    for side in (1, -1):
        s.append(([(side * 0.0071, 0.0016), (side * 0.0071, 0.0196)], 0.00026))
        s.append(([(side * (0.0024 + 0.0034 * math.sin(math.pi * u * 0.8)), 0.0138 + 0.0225 * u) for u in np.linspace(0, 1, 24)],
                  0.00028))
    s.append(([(0.0, 0.0146), (0.0, 0.0315)], 0.0003))
    s.append(([(-0.0071, 0.0016), (0.0071, 0.0016)], 0.00026))
    return draw_mask(-0.0095, 0.0095, 0.0, 0.046, 40000, s, dots=[((0.0, 0.0105), 0.0011)])


def rune_mask():
    """Fifteen runes of our own down the fuller: a stave each, branches picked from a small set by a fixed seed."""
    rng = np.random.default_rng(7)
    shapes = [
        [((0, 0.5), (-1, 0.9))], [((0, 0.5), (1, 0.9))], [((0, 0.75), (-1, 1.0)), ((0, 0.75), (1, 1.0))],
        [((-1, 0.6), (1, 0.6))], [((0, 1.0), (1, 0.7)), ((1, 0.7), (0, 0.4))], [((0, 0.3), (-1, 0.0)), ((0, 0.3), (1, 0.0))],
        [((-1, 0.2), (1, 0.8))], [((0, 0.9), (-1, 0.55)), ((-1, 0.55), (0, 0.2))], [((-0.6, 0.35), (0.6, 0.35)), ((-0.6, 0.7), (0.6, 0.7))],
    ]
    hw, h, gap, z = 0.0013, 0.0062, 0.0016, 0.034
    strokes = []
    for _ in range(15):
        strokes.append(([(0.0, z), (0.0, z + h)], 0.0003))
        for k in rng.choice(len(shapes), size=int(rng.integers(1, 3)), replace=False):
            for (ax, az), (bx, bz) in shapes[k]:
                strokes.append(([(ax * hw, z + az * h), (bx * hw, z + bz * h)], 0.0003))
        z += h + gap
    return draw_mask(-0.0024, 0.0024, 0.032, 0.152, 30000, strokes)


def mask_image(name, mask):
    m, rect = mask
    h, w = m.shape
    im = bpy.data.images.new(name, w, h, alpha=False, float_buffer=True)
    im.colorspace_settings.name = 'Non-Color'
    rgba = np.ones((h, w, 4), np.float32)
    rgba[..., 0] = rgba[..., 1] = rgba[..., 2] = m
    im.pixels.foreach_set(rgba.ravel())
    im.pack()
    return im, rect


# ------------------------------------------------------------------------------------------- source materials
VARIANTS = {
    'v01': dict(
        blade=dict(base=(0.60, 0.61, 0.63), r0=0.12, r1=0.24, runes=False, edge=0.0),
        fitting=dict(base=(0.34, 0.33, 0.32), metal=0.95, r0=0.28, r1=0.46),
        gem=dict(base=(0.58, 0.58, 0.60), metal=1.0, r0=0.10, r1=0.16, emit=0.0),
        cord=dict(base=(0.25, 0.135, 0.06), r0=0.48, r1=0.68),
        wire=dict(base=(0.78, 0.56, 0.26), metal=1.0, r0=0.22, r1=0.32)),
    'v02': dict(
        blade=dict(base=(0.16, 0.16, 0.19), r0=0.16, r1=0.28, runes=True, edge=0.55),
        fitting=dict(base=(0.10, 0.10, 0.12), metal=0.95, r0=0.30, r1=0.45),
        gem=dict(base=(0.36, 0.16, 0.62), metal=0.0, r0=0.05, r1=0.09, emit=0.45),
        cord=dict(base=(0.075, 0.052, 0.09), r0=0.45, r1=0.62),
        wire=dict(base=(0.80, 0.80, 0.83), metal=1.0, r0=0.18, r1=0.28)),
}


class Graph:
    def __init__(self, name):
        self.m = bpy.data.materials.new(name)
        self.m.use_nodes = True
        self.nt = self.m.node_tree
        for n in list(self.nt.nodes):
            self.nt.nodes.remove(n)
        self.out = self.node('ShaderNodeOutputMaterial')
        self.bsdf = self.node('ShaderNodeBsdfPrincipled')
        self.link(self.bsdf.outputs['BSDF'], self.out.inputs['Surface'])
        self.obj = self.node('ShaderNodeTexCoord').outputs['Object']

    def node(self, kind, **props):
        n = self.nt.nodes.new(kind)
        for k, v in props.items():
            setattr(n, k, v)
        return n

    def link(self, a, b):
        self.nt.links.new(a, b)

    def math(self, op, a, b=None):
        n = self.node('ShaderNodeMath', operation=op)
        for i, v in enumerate((a, b)):
            if v is None:
                continue
            if isinstance(v, (int, float)):
                n.inputs[i].default_value = v
            else:
                self.link(v, n.inputs[i])
        return n.outputs[0]

    def noise(self, scale, detail=6.0, stretch=(1.0, 1.0, 1.0), seed_offset=(0.0, 0.0, 0.0)):
        mp = self.node('ShaderNodeMapping')
        mp.inputs['Scale'].default_value = (scale * stretch[0], scale * stretch[1], scale * stretch[2])
        mp.inputs['Location'].default_value = seed_offset
        self.link(self.obj, mp.inputs['Vector'])
        nz = self.node('ShaderNodeTexNoise')
        nz.inputs['Detail'].default_value = detail
        nz.inputs['Roughness'].default_value = 0.55
        self.link(mp.outputs['Vector'], nz.inputs['Vector'])
        return nz.outputs['Fac']

    def remap(self, v, lo, hi):
        mr = self.node('ShaderNodeMapRange')
        mr.inputs['From Min'].default_value = 0.3
        mr.inputs['From Max'].default_value = 0.7
        mr.inputs['To Min'].default_value = lo
        mr.inputs['To Max'].default_value = hi
        self.link(v, mr.inputs['Value'])
        return mr.outputs['Result']

    def colour(self, base, var, fac):
        mix = self.node('ShaderNodeMix', data_type='RGBA', clamp_factor=True)
        mix.inputs[6].default_value = (*(c * (1 - var) for c in base), 1.0)
        mix.inputs[7].default_value = (*(min(1.0, c * (1 + var)) for c in base), 1.0)
        self.link(fac, mix.inputs['Factor'])
        return mix.outputs[2]

    def projected(self, image, rect):
        """The mask image laid on the blade's faces by the object's x and z (both faces alike)."""
        x0, x1, z0, z1 = rect
        sep = self.node('ShaderNodeSeparateXYZ')
        self.link(self.obj, sep.inputs['Vector'])
        u = self.math('DIVIDE', self.math('SUBTRACT', sep.outputs['X'], x0), x1 - x0)
        v = self.math('DIVIDE', self.math('SUBTRACT', sep.outputs['Z'], z0), z1 - z0)
        cmb = self.node('ShaderNodeCombineXYZ')
        self.link(u, cmb.inputs['X'])
        self.link(v, cmb.inputs['Y'])
        tex = self.node('ShaderNodeTexImage', image=image, extension='CLIP', interpolation='Cubic')
        self.link(cmb.outputs['Vector'], tex.inputs['Vector'])
        return tex.outputs['Color']

    def bump(self, height, distance, strength=1.0, invert=False, normal=None):
        b = self.node('ShaderNodeBump', invert=invert)
        b.inputs['Strength'].default_value = strength
        b.inputs['Distance'].default_value = distance
        self.link(height, b.inputs['Height'])
        if normal is not None:
            self.link(normal, b.inputs['Normal'])
        return b.outputs['Normal']

    def finish(self, base, rough, metal, normal=None, emit=None):
        self.link(base, self.bsdf.inputs['Base Color'])
        self.link(rough, self.bsdf.inputs['Roughness']) if not isinstance(rough, float) else None
        if isinstance(rough, float):
            self.bsdf.inputs['Roughness'].default_value = rough
        self.bsdf.inputs['Metallic'].default_value = metal
        if normal is not None:
            self.link(normal, self.bsdf.inputs['Normal'])
        if emit is not None:
            self.link(emit, self.bsdf.inputs['Emission Color'])
            self.bsdf.inputs['Emission Strength'].default_value = 1.0
        self.m.use_fake_user = True   # the .blend keeps the recipe of the baked maps
        return self.m


def source_materials(version, masks):
    p = VARIANTS[version]
    mats = {}
    # blade: steel ground along its length (long streaks), the engraved ricasso, v02's runes and edge light
    g = Graph(f'{NAME}-{version}-src-blade')
    q = p['blade']
    streak = g.noise(60.0, 8.0, stretch=(14.0, 14.0, 0.25))
    fine = g.noise(900.0, 4.0)
    eng = g.math('MAXIMUM', g.projected(*masks['ricasso']), 0.0)
    height = eng
    emit = None
    if q['runes']:
        runes = g.projected(*masks['runes'])
        height = g.math('MAXIMUM', eng, runes)
        edge = g.node('ShaderNodeAttribute', attribute_name='Edge').outputs['Fac']
        edge = g.math('MULTIPLY', g.math('POWER', edge, 3.0), q['edge'])
        glow = g.math('MINIMUM', g.math('ADD', g.math('MULTIPLY', runes, 1.0), edge), 1.0)
        mix = g.node('ShaderNodeMix', data_type='RGBA')
        mix.inputs[6].default_value = (0, 0, 0, 1)
        mix.inputs[7].default_value = (*VIOLET, 1)
        g.link(glow, mix.inputs['Factor'])
        emit = mix.outputs[2]
    n = g.bump(g.math('MULTIPLY', streak, 1.0), 0.00002, 0.4)
    n = g.bump(fine, 0.000008, 0.25, normal=n)
    n = g.bump(height, 0.00035, 1.0, invert=True, normal=n)
    base = g.colour(q['base'], 0.05, streak)
    base_eng = g.node('ShaderNodeMix', data_type='RGBA')   # the engraved lines hold dark grime
    g.link(height, base_eng.inputs['Factor'])
    g.link(base, base_eng.inputs[6])
    base_eng.inputs[7].default_value = (*(c * 0.45 for c in q['base']), 1.0)
    rough = g.math('ADD', g.remap(streak, q['r0'], q['r1']), g.math('MULTIPLY', height, 0.25))
    mats['blade'] = g.finish(base_eng.outputs[2], rough, 1.0, n, emit)

    for key in ('fitting', 'gem', 'wire'):
        q = p[key]
        g = Graph(f'{NAME}-{version}-src-{key}')
        pit = g.noise(700.0 if key != 'wire' else 1400.0, 8.0, seed_offset=(3.1, 1.7, 0.4))
        n = g.bump(pit, 0.00003 if key != 'gem' else 0.000005, 0.5)
        emit = None
        if q.get('emit'):
            mix = g.node('ShaderNodeMix', data_type='RGBA')
            mix.inputs[6].default_value = (*(c * q['emit'] for c in VIOLET), 1)
            mix.inputs[7].default_value = (*(c * q['emit'] * 0.6 for c in VIOLET), 1)
            g.link(pit, mix.inputs['Factor'])
            emit = mix.outputs[2]
        mats[key] = g.finish(g.colour(q['base'], 0.12, pit), g.remap(pit, q['r0'], q['r1']), q['metal'], n, emit)

    q = p['cord']
    g = Graph(f'{NAME}-{version}-src-cord')
    vo = g.node('ShaderNodeTexVoronoi', feature='F1')
    vo.inputs['Scale'].default_value = 2600.0
    g.link(g.obj, vo.inputs['Vector'])
    grain = vo.outputs['Distance']
    wear = g.noise(220.0, 6.0, seed_offset=(5.0, 2.0, 1.0))
    n = g.bump(grain, 0.00012, 0.45)
    n = g.bump(wear, 0.00005, 0.3, normal=n)
    mats['cord'] = g.finish(g.colour(q['base'], 0.22, wear), g.remap(wear, q['r0'], q['r1']), 0.0, n)
    return mats


# ------------------------------------------------------------------------------------------------------ baking
def bake_target(name, colour):
    im = bpy.data.images.new(name, BAKE, BAKE, alpha=False, float_buffer=False)
    im.colorspace_settings.name = 'sRGB' if colour else 'Non-Color'
    return im


def with_targets(obj, image, fn):
    """Put `image` as the active bake target in every material of `obj`, run fn(), take the nodes out again."""
    added = []
    for m in obj.data.materials:
        n = m.node_tree.nodes.new('ShaderNodeTexImage')
        n.image = image
        m.node_tree.nodes.active = n
        added.append((m, n))
    fn()
    for m, n in added:
        m.node_tree.nodes.remove(n)


def bake_channel(obj, image, socket):
    """Bake one input of each material's Principled BSDF through an Emission shader (base colour, roughness, metal, emission)."""
    saved = []
    for m in obj.data.materials:
        nt = m.node_tree
        bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
        out = next(n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL')
        em = nt.nodes.new('ShaderNodeEmission')
        inp = bsdf.inputs[socket]
        if inp.is_linked:
            nt.links.new(inp.links[0].from_socket, em.inputs['Color'])
        else:
            v = inp.default_value
            em.inputs['Color'].default_value = (v, v, v, 1.0) if isinstance(v, float) else tuple(v)
        old = out.inputs['Surface'].links[0].from_socket
        nt.links.new(em.outputs['Emission'], out.inputs['Surface'])
        saved.append((nt, em, old, out))
    bpy.context.scene.cycles.samples = 4
    with_targets(obj, image, lambda: bpy.ops.object.bake(type='EMIT'))
    for nt, em, old, out in saved:
        nt.links.new(old, out.inputs['Surface'])
        nt.nodes.remove(em)


def bake_all(obj, version):
    s = bpy.context.scene
    s.render.engine = 'CYCLES'
    s.cycles.device = 'CPU'
    s.render.bake.margin = 8
    s.render.bake.use_clear = True
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    maps = {}
    maps['base'] = bake_target(f'{NAME}-{version}-base', True)
    bake_channel(obj, maps['base'], 'Base Color')
    rough = bake_target(f'{NAME}-{version}-rough', False)
    bake_channel(obj, rough, 'Roughness')
    metal = bake_target(f'{NAME}-{version}-metal', False)
    bake_channel(obj, metal, 'Metallic')
    ao = bake_target(f'{NAME}-{version}-ao', False)
    s.cycles.samples = 96
    with_targets(obj, ao, lambda: bpy.ops.object.bake(type='AO'))
    maps['normal'] = bake_target(f'{NAME}-{version}-normal', False)
    s.cycles.samples = 16
    s.render.bake.normal_space = 'TANGENT'
    with_targets(obj, maps['normal'], lambda: bpy.ops.object.bake(type='NORMAL'))
    if version == 'v02':
        maps['emissive'] = bake_target(f'{NAME}-{version}-emissive', True)
        bake_channel(obj, maps['emissive'], 'Emission Color')
    # ORM = occlusion, roughness, metal in r, g, b (the glTF packing)
    orm = bake_target(f'{NAME}-{version}-orm', False)
    px = lambda im: np.array(im.pixels[:], np.float32).reshape(BAKE, BAKE, 4)
    o = np.ones((BAKE, BAKE, 4), np.float32)
    o[..., 0], o[..., 1], o[..., 2] = px(ao)[..., 0], px(rough)[..., 0], px(metal)[..., 0]
    orm.pixels.foreach_set(o.ravel())
    maps['orm'] = orm
    for im in (rough, metal, ao):
        bpy.data.images.remove(im)
    os.makedirs(TEXDIR, exist_ok=True)
    for key, im in maps.items():
        path = f'{TEXDIR}/{NAME}-{version}-{key}.png'
        if os.path.exists(path):
            raise RuntimeError(f'{path} exists: bump the version instead of overwriting')
        im.filepath_raw = path
        im.file_format = 'PNG'
        im.save()
    return maps


def final_material(version, maps):
    m = bpy.data.materials.new(f'{NAME}-{version}')
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    bsdf = nt.nodes.new('ShaderNodeBsdfPrincipled')
    nt.links.new(bsdf.outputs['BSDF'], out.inputs['Surface'])
    tex = lambda key: nt.nodes.new('ShaderNodeTexImage')
    base = tex('base'); base.image = maps['base']
    nt.links.new(base.outputs['Color'], bsdf.inputs['Base Color'])
    orm = tex('orm'); orm.image = maps['orm']
    sep = nt.nodes.new('ShaderNodeSeparateColor')
    nt.links.new(orm.outputs['Color'], sep.inputs['Color'])
    nt.links.new(sep.outputs['Green'], bsdf.inputs['Roughness'])
    nt.links.new(sep.outputs['Blue'], bsdf.inputs['Metallic'])
    # the glTF exporter's occlusion socket
    grp = bpy.data.node_groups.get('glTF Material Output') or bpy.data.node_groups.new('glTF Material Output', 'ShaderNodeTree')
    if not grp.interface.items_tree:
        grp.interface.new_socket('Occlusion', in_out='INPUT', socket_type='NodeSocketFloat')
    gn = nt.nodes.new('ShaderNodeGroup')
    gn.node_tree = grp
    nt.links.new(sep.outputs['Red'], gn.inputs['Occlusion'])
    nrm = tex('normal'); nrm.image = maps['normal']
    nm = nt.nodes.new('ShaderNodeNormalMap')
    nt.links.new(nrm.outputs['Color'], nm.inputs['Color'])
    nt.links.new(nm.outputs['Normal'], bsdf.inputs['Normal'])
    if 'emissive' in maps:
        em = tex('emissive'); em.image = maps['emissive']
        nt.links.new(em.outputs['Color'], bsdf.inputs['Emission Color'])
        bsdf.inputs['Emission Strength'].default_value = 3.0
    return m


# -------------------------------------------------------------------------------------------------------- build
def build(version):
    E.reset()
    masks = {'ricasso': mask_image(f'{NAME}-engraving', ricasso_mask()), 'runes': mask_image(f'{NAME}-runes', rune_mask())}
    mats = source_materials(version, masks)
    blade = build_blade()
    guard, gems = build_guard()
    core, ferrules, cord, wire = build_grip()
    pommel, button = build_pommel()
    roles = [(blade, 'blade')] + [(p, 'fitting') for p in guard + ferrules + [pommel]] + [(g, 'gem') for g in gems + [button]] + \
            [(core, 'cord'), (cord, 'cord'), (wire, 'wire')]
    for o, role in roles:
        o.data.materials.clear()
        o.data.materials.append(mats[role])
    obj = E.join([o for o, _ in roles], NAME)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.003, area_weight=0.0, scale_to_bounds=False)
    bpy.ops.uv.pack_islands(margin=0.003, rotate=True)
    bpy.ops.object.mode_set(mode='OBJECT')
    maps = bake_all(obj, version)
    fm = final_material(version, maps)
    obj.data.materials.clear()
    obj.data.materials.append(fm)
    for p in obj.data.polygons:
        p.material_index = 0
    if 'Edge' in obj.data.color_attributes:
        obj.data.color_attributes.remove(obj.data.color_attributes['Edge'])
    obj.scale = (UNITS, UNITS, UNITS)
    E.apply_transform(obj)
    return E.export(obj, KIT, NAME, version)


if __name__ == '__main__':
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    reports = []
    for v in (argv or ['v01', 'v02']):
        reports.append(build(v))
        print(f'[ok] {KIT}/{NAME}-{v}: {reports[-1]["triangles"]} tris, {reports[-1]["size"]}')
    print('JSON_BEGIN' + json.dumps({'kit': KIT, 'assets': reports}) + 'JSON_END')
