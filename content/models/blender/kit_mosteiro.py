"""Kit Mosteiro (content/models/mosteiro): the church and scriptorium pieces of O Mosteiro das Cinzas (2026-10-29).

v01 (primitives, flat colours) was refused by the user ("awful low poly trash ... you know how to do much better 3D i saw from
your trees"): v02 follows the tree workflow (content/models/README.md § Tree Creating Pipeline): dense real geometry (subdivision,
carved mouldings, cloth folds, turned columns, dripped wax), the user's own photographed materials (textures/wt-*.jpg: limestone,
dark oak, iron, pillar stone) with relief from the photo itself, and offline review renders in four views
(captures/models/mosteiro-review/<name>-<view>.png) presented for approval before the asset is catalogued.

  blender -b --python content/models/blender/kit_mosteiro.py            # every asset (v02)
  blender -b --python content/models/blender/kit_mosteiro.py -- pew     # one

pew              a church bench: moulded top rail, two carved end panels with a quatrefoil, planked seat and back, the kneeler
lectern          the reading stand: an octagonal moulded base, a turned column with rings, a sloped desk, the open book
votive-rack      a wrought-iron stand of three tiers with scrolled brackets and twisted uprights; votive candles, dripped, lit
saint-statue     the Saint of the Ash: a veiled robed figure on an octagonal moulded plinth, hands open at the chest, folds
bell-cote        two piers of dressed limestone blocks, an oak beam with iron straps, the bronze bell on its yoke, the rope
scriptorium-desk the copyist's sloped desk with pegged joints, a book shelf, sheets, inkwell, quills, a dripping candle
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bmesh  # noqa: E402
from emberlib import *  # noqa
from emberlib import _materials, _images  # noqa: E402


def img(name, colorspace='sRGB'):
    """A texture by name, .jpg or .png (emberlib.image knows only .jpg)."""
    if name in _images:
        return _images[name]
    for ext in ('.jpg', '.png'):
        path = f'{TEX}/{name}{ext}'
        if os.path.exists(path):
            im = bpy.data.images.load(path, check_existing=True)
            im.colorspace_settings.name = colorspace
            _images[name] = im
            return im
    raise FileNotFoundError(name)

KIT, V = 'mosteiro', 'v02'


# ------------------------------------------------------------------------------------------------ materials
def photo(key, file, roughness=0.85, bump=0.25, metallic=0.0, tint=None, emission=None):
    """A Principled material on one of the user's photographs, its relief from the photo's own luminance (a Bump node)."""
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
    t = nt.nodes.new('ShaderNodeTexImage')
    t.image = img(file, 'sRGB')
    base = t.outputs['Color']
    if tint:
        mix = nt.nodes.new('ShaderNodeMix')
        mix.data_type = 'RGBA'
        mix.blend_type = 'MULTIPLY'
        mix.inputs['Factor'].default_value = 1.0
        nt.links.new(base, mix.inputs[6])
        mix.inputs[7].default_value = (*tint[:3], 1.0)
        base = mix.outputs[2]
    nt.links.new(base, bsdf.inputs['Base Color'])
    bw = nt.nodes.new('ShaderNodeRGBToBW')
    nt.links.new(t.outputs['Color'], bw.inputs['Color'])
    b = nt.nodes.new('ShaderNodeBump')
    b.inputs['Strength'].default_value = bump
    b.inputs['Distance'].default_value = 0.02
    nt.links.new(bw.outputs['Val'], b.inputs['Height'])
    nt.links.new(b.outputs['Normal'], bsdf.inputs['Normal'])
    if emission:
        bsdf.inputs['Emission Color'].default_value = (*emission[:3], 1.0)
        bsdf.inputs['Emission Strength'].default_value = emission[3]
    _materials[key] = m
    return m


def OAK():
    return photo('Mosteiro oak | user photo', 'wt-dark-oak', 0.62, 0.35)


def OAK_WORN():
    return photo('Mosteiro oak worn | user photo', 'wt-dark-oak', 0.55, 0.3, tint=(1.12, 1.05, 0.95))


def LIME():
    return photo('Mosteiro limestone | user photo', 'wt-limestone', 0.9, 0.45)


def PILLAR():
    return photo('Mosteiro pillar stone | user photo', 'wt-pillar', 0.88, 0.4)


def IRON():
    return photo('Mosteiro iron | user photo', 'wt-iron', 0.6, 0.3, metallic=0.75)


def BRONZE():
    return material('Mosteiro bronze', flat=(0.38, 0.26, 0.14), roughness=0.42, metallic=0.95)


def WAX():
    return material('Mosteiro wax', flat=(0.93, 0.86, 0.7), roughness=0.55)


def WAX_LIT():
    return material('Mosteiro wax lit', flat=(0.95, 0.85, 0.62), roughness=0.5, emission=(1.0, 0.72, 0.35, 1.6))


def FLAME():
    return material('Mosteiro flame', flat=(1.0, 0.75, 0.3), roughness=0.3, emission=(1.0, 0.55, 0.12, 9.0))


def PAPER():
    return material('Mosteiro vellum', flat=(0.8, 0.72, 0.56), roughness=0.92)


def LEATHER():
    return photo('Mosteiro leather | photo', 'leather-worn-photo-v01', 0.7, 0.25)


def ROPE():
    return photo('Mosteiro rope | photo', 'winch-rope-hemp-color-v01', 0.95, 0.4)


# ------------------------------------------------------------------------------------------------ helpers
def subsurf(o, levels=2):
    m = o.modifiers.new('subsurf', 'SUBSURF')
    m.levels = m.render_levels = levels
    apply_mods(o)


def wear(o, strength=0.006, scale=0.18, seed=1):
    displace(o, strength, scale, seed=seed)
    apply_mods(o)


def rod(name, a, b, r0, r1, mat, segs=16):
    """A cylinder from point a to point b."""
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    length = math.sqrt(dx * dx + dy * dy + dz * dz)
    theta, phi = math.acos(max(-1.0, min(1.0, dz / length))), math.atan2(dy, dx)
    return cylinder(name, r0, r1, length, loc=((a[0] + b[0]) * 0.5, (a[1] + b[1]) * 0.5, (a[2] + b[2]) * 0.5), rot=(0, theta, phi), segs=segs, mat=mat)


def moulding(name, profile, length, loc, rot=(0, 0, 0), mat=None):
    """A moulded rail: the 2D profile (y, z) extruded along x."""
    return prism(name, profile, length, loc=loc, rot=rot, mat=mat)


def octagon(r):
    return [(math.cos(math.tau * i / 8 + math.pi / 8) * r, math.sin(math.tau * i / 8 + math.pi / 8) * r) for i in range(8)]


def oct_block(name, r, h, z, mat):
    o = prism(name, octagon(r), h, loc=(0, 0, z + h * 0.5), rot=(0, 0, 0), mat=mat)
    return o


def plinth(name, r=0.5, mat=None):
    """An octagonal moulded plinth: base, chamfered step, dado, cap (stone, chipped at two corners)."""
    mat = mat or LIME()
    parts = [oct_block('base', r, 0.12, 0.0, mat), oct_block('step', r * 0.92, 0.07, 0.12, mat), oct_block('dado', r * 0.8, 0.3, 0.19, mat),
             oct_block('neck', r * 0.86, 0.05, 0.49, mat), oct_block('cap', r * 0.94, 0.08, 0.54, mat)]
    for p in parts:
        bevel(p, 0.012, 3)
        apply_mods(p)
        box_uv(p, 0.9)
    o = join(parts, name)
    for i, a in enumerate((0.4, 2.9)):
        chip = sphere(f'chip{i}', 0.07, loc=(math.cos(a) * r * 0.96, math.sin(a) * r * 0.96, 0.08 + i * 0.5), subdiv=2)
        boolean(o, chip)
    return o


def fold_cloth(o, folds=9, amp=0.02, z_from=0.0, z_to=1.0, seed=0):
    """Vertical cloth folds on a lathe body: each vertex pushed radially by a sine of its angle, stronger toward the hem."""
    rng = rand(seed)
    phases = [rng.random() * math.tau for _ in range(3)]
    bm = bmesh.new()
    bm.from_mesh(o.data)
    for v in bm.verts:
        x, y, z = v.co
        r = math.hypot(x, y)
        if r < 1e-4:
            continue
        t = max(0.0, min(1.0, (z_to - z) / max(1e-6, z_to - z_from)))  # 1 at the hem, 0 at the top
        a = math.atan2(y, x)
        d = (math.sin(a * folds + phases[0] + z * 1.5) * 0.6 + math.sin(a * (folds // 2 + 1) + phases[1] - z * 0.8) * 0.3
             + math.sin(a * 2 + phases[2]) * 0.1) * amp * (0.25 + 0.75 * t)
        v.co.x += x / r * d
        v.co.y += y / r * d
    bm.to_mesh(o.data)
    bm.free()


def candle(name, loc, h, r=0.028, lit=True, seed=0, segs=18):
    """A votive candle with dripped wax running down one side and a flame."""
    rng = rand(seed)
    prof = [(0, 0), (r, 0)]
    steps = 10
    for i in range(1, steps + 1):
        z = h * i / steps
        rr = r * (1 + 0.08 * math.sin(i * 1.7 + seed) * (1 - i / steps))
        prof.append((rr, z))
    prof += [(r * 0.55, h + 0.004), (0, h + 0.004)]
    c = lathe(name, prof, segs=segs, loc=loc, mat=WAX_LIT() if lit else WAX())
    parts = [c]
    for k in range(rng.randint(1, 3)):  # drips: thin lathes hanging on the side
        a = rng.random() * math.tau
        dl = h * rng.uniform(0.3, 0.8)
        drip = lathe(f'{name}-drip{k}', [(0, 0), (0.009, 0), (0.011, dl * 0.2), (0.006, dl * 0.9), (0.012, dl * 0.96), (0.01, dl), (0, dl)], segs=10,
                     loc=(loc[0] + math.cos(a) * r * 0.95, loc[1] + math.sin(a) * r * 0.95, loc[2] + h - dl), mat=WAX_LIT() if lit else WAX())
        parts.append(drip)
    parts.append(cylinder(f'{name}-wick', 0.003, 0.002, 0.02, loc=(loc[0], loc[1], loc[2] + h + 0.01), segs=6, mat=M('coal')))
    if lit:
        parts.append(lathe(f'{name}-flame', [(0, 0), (0.012, 0.004), (0.016, 0.02), (0.009, 0.045), (0, 0.065)], segs=12, loc=(loc[0], loc[1], loc[2] + h + 0.016), mat=FLAME()))
    return parts


# ------------------------------------------------------------------------------------------------ the pieces
def pew():
    L, D, SEAT = 2.4, 0.46, 0.47
    oak, worn = OAK(), OAK_WORN()
    parts = []
    # the planked seat: four planks with shrink gaps, each bevelled and worn at the top
    for i, w in enumerate((0.12, 0.11, 0.12, 0.1)):
        y = -D * 0.5 + sum((0.12, 0.11, 0.12, 0.1)[:i]) + i * 0.006 + w * 0.5
        p = box(f'seat{i}', (L - 0.14, w, 0.045), loc=(0, y, SEAT), mat=worn)
        bevel(p, 0.006, 3)
        apply_mods(p)
        box_uv(p, 1.1, (0.13 * i, 0.0))
        subdivide(p, 2, simple=True)
        wear(p, 0.004, 0.12, seed=i)
        parts.append(p)
    # the back: three planks leaning 8 degrees, the moulded top rail over them
    lean = math.radians(-8)
    for i in range(3):
        z = SEAT + 0.1 + i * 0.165
        p = box(f'back{i}', (L - 0.14, 0.03, 0.15), loc=(0, D * 0.5 - 0.02 + (z - SEAT) * math.tan(-lean), z), rot=(lean, 0, 0), mat=oak)
        bevel(p, 0.004, 2)
        apply_mods(p)
        box_uv(p, 1.1, (0.0, 0.2 * i))
        parts.append(p)
    rail_prof = [(-0.05, 0), (0.05, 0), (0.06, 0.02), (0.055, 0.035), (0.04, 0.045), (0.045, 0.06), (0.03, 0.075), (-0.03, 0.075), (-0.045, 0.06), (-0.04, 0.045), (-0.055, 0.035), (-0.06, 0.02)]
    zt = SEAT + 0.1 + 2 * 0.165 + 0.08
    rail = moulding('toprail', rail_prof, L + 0.02, loc=(0, D * 0.5 - 0.02 + (zt - SEAT) * math.tan(-lean), zt), rot=(lean, 0, 0), mat=oak)
    bevel(rail, 0.004, 2)
    apply_mods(rail)
    box_uv(rail, 1.1)
    parts.append(rail)
    # the end panels: a thick oak slab with a scrolled top and a quatrefoil pierced through
    H = zt + 0.12
    for sx in (-1, 1):
        end = box(f'end{sx}', (0.07, D + 0.1, H), loc=(sx * (L * 0.5 - 0.035), 0.03, H * 0.5), mat=oak)
        # the scrolled top: round the back-top corner, cut a cove at the front-top
        scroll = cylinder(f'scroll{sx}', 0.14, 0.14, 0.2, loc=(sx * (L * 0.5 - 0.035), -D * 0.5 - 0.02 + 0.14, H + 0.02), rot=(0, math.pi / 2, 0), segs=32, mat=None)
        boolean(end, scroll)
        for k in range(4):  # the quatrefoil: four lobes bored through
            a = math.tau * k / 4
            lobe = cylinder(f'lobe{sx}{k}', 0.055, 0.055, 0.3, loc=(sx * (L * 0.5 - 0.035), 0.03 + math.cos(a) * 0.055, SEAT + 0.42 + math.sin(a) * 0.055), rot=(0, math.pi / 2, 0), segs=24, mat=None)
            boolean(end, lobe)
        groove = box(f'groove{sx}', (0.3, D - 0.04, 0.012), loc=(sx * (L * 0.5 - 0.035 + 0.03), 0.03, 0.2), mat=None)
        boolean(end, groove)
        bevel(end, 0.006, 3)
        apply_mods(end)
        box_uv(end, 0.9, (0.3 * sx, 0.0))
        parts.append(end)
        # a pegged cross-brace under the seat and the kneeler's foot
        parts.append(box(f'brace{sx}', (0.05, D - 0.1, 0.08), loc=(sx * (L * 0.5 - 0.11), 0, SEAT - 0.06), mat=oak))
    stretcher = box('stretcher', (L - 0.2, 0.05, 0.09), loc=(0, 0.02, 0.18), mat=oak)
    bevel(stretcher, 0.005, 2)
    apply_mods(stretcher)
    box_uv(stretcher, 1.1)
    parts.append(stretcher)
    # the kneeler: a plank on two shaped feet, its top worn pale, a leather pad
    kn = box('kneeler', (L - 0.18, 0.24, 0.045), loc=(0, -D * 0.5 - 0.3, 0.17), mat=worn)
    bevel(kn, 0.006, 3)
    apply_mods(kn)
    box_uv(kn, 1.0)
    parts.append(kn)
    pad = box('pad', (L - 0.3, 0.2, 0.03), loc=(0, -D * 0.5 - 0.3, 0.205), mat=LEATHER())
    bevel(pad, 0.012, 3)
    apply_mods(pad)
    box_uv(pad, 0.5)
    subdivide(pad, 2, simple=True)
    wear(pad, 0.006, 0.1, seed=4)
    parts.append(pad)
    for sx in (-1, 1):
        f = box(f'kfoot{sx}', (0.06, 0.22, 0.15), loc=(sx * (L * 0.5 - 0.2), -D * 0.5 - 0.3, 0.075), mat=oak)
        arch = cylinder(f'karch{sx}', 0.05, 0.05, 0.1, loc=(sx * (L * 0.5 - 0.2), -D * 0.5 - 0.3, 0.0), rot=(0, math.pi / 2, 0), segs=20, mat=None)
        boolean(f, arch)
        bevel(f, 0.004, 2)
        apply_mods(f)
        box_uv(f, 0.8)
        parts.append(f)
    for p in parts:
        if not p.data.uv_layers:
            box_uv(p, 1.0)
    o = join(parts, 'pew')
    smooth(o, 32)
    ground(o)
    return o


def lectern():
    oak = OAK()
    parts = []
    # the base: an octagonal moulded foot in oak, a tall turned column with rings and a knop
    base = plinth('base', 0.33, mat=oak)
    parts.append(base)
    col_prof = [(0, 0), (0.14, 0), (0.15, 0.02), (0.12, 0.05), (0.09, 0.08), (0.075, 0.2), (0.07, 0.35), (0.095, 0.4), (0.1, 0.43), (0.08, 0.47),
                (0.07, 0.62), (0.07, 0.75), (0.11, 0.8), (0.12, 0.86), (0.1, 0.9), (0.075, 0.94), (0.07, 1.06), (0.11, 1.1), (0.13, 1.13), (0.12, 1.16), (0, 1.16)]
    col = lathe('column', col_prof, segs=48, loc=(0, 0, 0.62), mat=oak)
    bevel(col, 0.003, 2)
    apply_mods(col)
    box_uv(col, 0.8)
    parts.append(col)
    # the desk: a sloped board with a moulded frame on three sides and a lip, carried by a bracket
    H = 0.62 + 1.16
    tilt = math.radians(-28)
    desk = box('desk', (0.66, 0.5, 0.035), loc=(0, 0.06, H + 0.04), rot=(tilt, 0, 0), mat=oak)
    bevel(desk, 0.004, 2)
    apply_mods(desk)
    box_uv(desk, 0.9)
    parts.append(desk)
    frame_prof = [(-0.02, 0), (0.02, 0), (0.025, 0.02), (0.015, 0.03), (0.02, 0.04), (0.0, 0.05), (-0.02, 0.04), (-0.015, 0.03), (-0.025, 0.02)]
    for sx in (-1, 1):
        side = moulding(f'frame{sx}', frame_prof, 0.5, loc=(sx * 0.33, 0.06, H + 0.07), rot=(tilt, 0, math.pi / 2), mat=oak)
        box_uv(side, 0.8)
        parts.append(side)
    lip = moulding('lip', [(-0.03, 0), (0.03, 0), (0.035, 0.05), (0.02, 0.07), (-0.02, 0.07), (-0.035, 0.05)], 0.68, loc=(0, 0.06 - 0.25 * math.cos(tilt) + 0.02, H + 0.04 - 0.25 * math.sin(-tilt) + 0.02), rot=(tilt, 0, 0), mat=oak)
    box_uv(lip, 0.8)
    parts.append(lip)
    parts.append(rod('bracket', (0, 0.1, H - 0.03), (0, 0.28, H + 0.08), 0.03, 0.025, oak, 12))
    # the book: leather boards, the page block with its rounded fore-edge, the two open leaves with a slight curl
    cover = box('cover', (0.5, 0.36, 0.025), loc=(0, 0.07, H + 0.075), rot=(tilt, 0, 0), mat=LEATHER())
    bevel(cover, 0.008, 3)
    apply_mods(cover)
    box_uv(cover, 0.6)
    parts.append(cover)
    for sx in (-1, 1):
        block = box(f'pages{sx}', (0.22, 0.33, 0.04), loc=(sx * 0.125, 0.07, H + 0.105), rot=(tilt, 0, 0), mat=PAPER())
        bevel(block, 0.01, 4)
        apply_mods(block)
        subdivide(block, 2, simple=True)
        wear(block, 0.002, 0.05, seed=sx + 3)
        box_uv(block, 0.5)
        parts.append(block)
        leaf = plane(f'leaf{sx}', (0.21, 0.33), loc=(sx * 0.13, 0.07, H + 0.128), rot=(tilt, 0, 0), mat=PAPER())
        subdivide(leaf, 3, simple=True)
        wear(leaf, 0.004, 0.08, seed=sx + 9)
        parts.append(leaf)
    parts.append(torus('clasp', 0.03, 0.006, loc=(0, 0.07 - 0.19 * math.cos(tilt), H + 0.1 - 0.19 * math.sin(-tilt)), rot=(math.pi / 2 + tilt, 0, 0), mat=IRON(), segs=16, ring=8))
    o = join(parts, 'lectern')
    smooth(o, 40)
    ground(o)
    return o


def votive_rack(seed=7):
    rng = rand(seed)
    iron = IRON()
    W = 1.1
    parts = []
    # twisted uprights: a square bar turned about its axis (a stack of short twisted boxes), on scrolled feet
    for sx in (-1, 1):
        x = sx * (W * 0.5 - 0.03)
        n = 24
        for i in range(n):
            z0 = 0.08 + i * (1.0 / n)
            seg = box(f'up{sx}{i}', (0.035, 0.035, 1.0 / n + 0.004), loc=(x, 0, z0 + 0.5 / n), rot=(0, 0, i * math.tau / n * 0.5), mat=iron)
            parts.append(seg)
        for sy in (-1, 1):  # the feet: a scroll curling out
            for k in range(10):
                a = k / 9 * math.pi * 0.9
                parts.append(sphere(f'foot{sx}{sy}{k}', 0.018, loc=(x, sy * (0.05 + math.sin(a) * 0.2), 0.02 + (1 - math.cos(a)) * 0.08), subdiv=1, mat=iron))
        parts.append(cylinder(f'collar{sx}', 0.035, 0.035, 0.05, loc=(x, 0, 0.1), segs=12, mat=iron))
        parts.append(sphere(f'finial{sx}', 0.03, loc=(x, 0, 1.12), subdiv=2, mat=iron))
    tiers = ((0.32, 0.16), (0.62, 0.02), (0.92, -0.12))
    for i, (z, y) in enumerate(tiers):
        tray = box(f'tray{i}', (W - 0.02, 0.2, 0.012), loc=(0, y, z), mat=iron)
        bevel(tray, 0.003, 2)
        apply_mods(tray)
        box_uv(tray, 0.6)
        parts.append(tray)
        for sy in (-1, 1):  # raised edges
            parts.append(box(f'edge{i}{sy}', (W - 0.02, 0.01, 0.025), loc=(0, y + sy * 0.095, z + 0.012), mat=iron))
        for sx in (-1, 1):  # scrolled brackets under each tray
            x = sx * (W * 0.5 - 0.03)
            for k in range(8):
                a = k / 7 * math.pi * 0.5
                parts.append(sphere(f'br{i}{sx}{k}', 0.014, loc=(x - sx * math.sin(a) * 0.16, y, z - 0.012 - (1 - math.cos(a)) * 0.16), subdiv=1, mat=iron))
        n = 8
        for k in range(n):
            if rng.random() < 0.22:
                continue  # an empty cup
            x = -W * 0.5 + 0.1 + k * (W - 0.2) / (n - 1) + rng.uniform(-0.012, 0.012)
            parts.append(lathe(f'cup{i}{k}', [(0, 0), (0.036, 0), (0.038, 0.03), (0.034, 0.038), (0.03, 0.038), (0.03, 0.004), (0, 0.004)], segs=20, loc=(x, y, z + 0.006), mat=BRONZE()))
            parts += candle(f'c{i}{k}', (x, y, z + 0.01), rng.uniform(0.03, 0.12), lit=rng.random() < 0.8, seed=i * 10 + k)
    for p in parts:
        if not p.data.uv_layers:
            box_uv(p, 0.5)
    o = join(parts, 'votive-rack')
    smooth(o, 45)
    ground(o)
    return o


def saint_statue():
    lime, pil = LIME(), PILLAR()
    parts = [plinth('plinth', 0.5, mat=pil)]
    z0 = 0.62
    # the robe: a dense lathe, folds falling from the waist to the hem, a knotted cord at the waist
    robe = [(0, 0), (0.44, 0), (0.43, 0.04), (0.36, 0.3), (0.3, 0.6), (0.26, 0.9), (0.245, 1.1), (0.27, 1.25), (0.31, 1.4), (0.3, 1.52), (0.2, 1.58), (0.1, 1.6), (0, 1.6)]
    body = lathe('robe', robe, segs=96, loc=(0, 0, 0), mat=lime)
    subdivide(body, 1, simple=True)
    fold_cloth(body, folds=11, amp=0.028, z_from=0.0, z_to=1.3, seed=3)
    displace(body, 0.006, 0.2, seed=3)
    apply_mods(body)
    body.location = (0, 0, z0)
    apply_transform(body)
    box_uv(body, 1.0)
    parts.append(body)
    cord = torus('cord', 0.255, 0.018, loc=(0, 0, z0 + 1.08), mat=lime, segs=48, ring=10)
    parts.append(cord)
    for k in range(2):  # the cord's ends hanging at the front
        parts.append(rod(f'cordend{k}', (0.06 - k * 0.12, -0.25, z0 + 1.08), (0.09 - k * 0.14, -0.3, z0 + 0.7 - k * 0.08), 0.018, 0.014, lime, 10))
        parts.append(sphere(f'knot{k}', 0.028, loc=(0.09 - k * 0.14, -0.3, z0 + 0.7 - k * 0.08), subdiv=2, mat=lime))
    # the veil: a hood from the crown over the shoulders, its front cut open so the face shows under it
    veil = [(0, 0), (0.44, 0), (0.4, 0.1), (0.3, 0.26), (0.21, 0.4), (0.165, 0.5), (0.15, 0.56), (0, 0.58)]
    hood = lathe('veil', veil, segs=96, loc=(0, 0, 0), mat=lime)
    subdivide(hood, 1, simple=True)
    fold_cloth(hood, folds=14, amp=0.014, z_from=0.0, z_to=0.45, seed=5)
    apply_mods(hood)
    hood.location = (0, 0.05, z0 + 1.4)
    apply_transform(hood)
    opening = box('opening', (0.26, 0.4, 0.34), loc=(0, -0.2, z0 + 1.72), mat=None)
    boolean(hood, opening)
    box_uv(hood, 1.0)
    parts.append(hood)
    # the head: an oval with a brow, the nose a small wedge, the eyes closed (two shallow cuts), the chin
    head = sphere('head', 0.13, loc=(0, -0.05, z0 + 1.71), subdiv=4, mat=lime, scale=(0.92, 1.0, 1.22))
    for sx in (-1, 1):
        eye = sphere(f'eye{sx}', 0.028, loc=(sx * 0.048, -0.165, z0 + 1.735), subdiv=2, mat=None, scale=(1.3, 0.6, 0.5))
        boolean(head, eye)
    parts.append(head)
    parts.append(prism('nose', [(-0.018, 0), (0.018, 0), (0, 0.05)], 0.07, loc=(0, -0.17, z0 + 1.695), rot=(math.radians(-10), math.pi / 2, 0), mat=lime))
    parts.append(cylinder('neck', 0.055, 0.07, 0.14, loc=(0, -0.02, z0 + 1.56), segs=24, mat=lime))
    # the arms: sleeves hanging from the shoulders, the hands open at the chest, fingers
    for sx in (-1, 1):
        shoulder = (sx * 0.28, 0.0, z0 + 1.44)
        elbow = (sx * 0.31, -0.13, z0 + 1.06)
        wrist = (sx * 0.1, -0.31, z0 + 1.2)
        parts.append(rod(f'upper{sx}', shoulder, elbow, 0.1, 0.085, lime, 24))
        parts.append(sphere(f'elbow{sx}', 0.085, loc=elbow, subdiv=3, mat=lime))
        sleeve = rod(f'sleeve{sx}', elbow, wrist, 0.085, 0.07, lime, 24)
        parts.append(sleeve)
        hand = sphere(f'palm{sx}', 0.062, loc=(wrist[0] + sx * 0.02, wrist[1] - 0.05, wrist[2] + 0.01), subdiv=3, mat=lime, scale=(0.95, 1.2, 0.45))
        parts.append(hand)
        for f in range(4):
            fx = wrist[0] + sx * (0.045 - f * 0.028)
            parts.append(rod(f'finger{sx}{f}', (fx, wrist[1] - 0.09, wrist[2] + 0.005), (fx - sx * 0.01, wrist[1] - 0.16 - (0.01 if f in (1, 2) else 0), wrist[2] + 0.03), 0.013, 0.01, lime, 10))
        parts.append(rod(f'thumb{sx}', (wrist[0] + sx * 0.07, wrist[1] - 0.05, wrist[2] + 0.01), (wrist[0] + sx * 0.1, wrist[1] - 0.1, wrist[2] + 0.04), 0.013, 0.01, lime, 10))
    # the feet showing under the hem
    for sx in (-1, 1):
        parts.append(sphere(f'foot{sx}', 0.07, loc=(sx * 0.13, -0.36, z0 + 0.03), subdiv=3, mat=lime, scale=(0.8, 1.6, 0.5)))
    for p in parts:
        if not p.data.uv_layers:
            box_uv(p, 1.0)
    o = join(parts, 'saint-statue')
    smooth(o, 55)
    ground(o)
    return o


def bell_cote():
    lime, oak, iron = PILLAR(), OAK(), IRON()
    W, H = 2.2, 2.9
    parts = []
    rng = rand(11)
    # two piers of dressed blocks, every course a different height, joints recessed, corners chipped
    for sx in (-1, 1):
        x = sx * (W * 0.5 - 0.24)
        z = 0.0
        course = 0
        while z < H - 0.42:
            h = rng.uniform(0.2, 0.3)
            if z + h > H - 0.42:
                h = H - 0.42 - z
            if course % 2 == 0:
                b = box(f'block{sx}{course}', (0.46, 0.46, h - 0.012), loc=(x, 0, z + h * 0.5), mat=lime)
            else:  # two blocks side by side, turned each course
                b = box(f'blockA{sx}{course}', (0.46, 0.22, h - 0.012), loc=(x, -0.115, z + h * 0.5), mat=lime)
                b2 = box(f'blockB{sx}{course}', (0.46, 0.22, h - 0.012), loc=(x, 0.115, z + h * 0.5), mat=lime)
                bevel(b2, 0.012, 3)
                apply_mods(b2)
                box_uv(b2, 0.7, (rng.random(), rng.random()))
                subdivide(b2, 2, simple=True)
                wear(b2, 0.005, 0.1, seed=course + 7)
                parts.append(b2)
            bevel(b, 0.012, 3)
            apply_mods(b)
            box_uv(b, 0.7, (rng.random(), rng.random()))
            subdivide(b, 2, simple=True)
            wear(b, 0.005, 0.1, seed=course)
            parts.append(b)
            z += h
            course += 1
        capstone = box(f'capstone{sx}', (0.56, 0.56, 0.1), loc=(x, 0, H - 0.37), mat=lime)
        bevel(capstone, 0.02, 3)
        apply_mods(capstone)
        box_uv(capstone, 0.7)
        parts.append(capstone)
        chip = sphere(f'chip{sx}', 0.09, loc=(x + sx * 0.23, -0.23, 0.9), subdiv=2)
        boolean(parts[-2], chip)
    # the oak beam, chamfered, with iron straps and bolts at the piers
    beam = box('beam', (W + 0.16, 0.32, 0.28), loc=(0, 0, H - 0.18), mat=oak)
    bevel(beam, 0.03, 3)
    apply_mods(beam)
    box_uv(beam, 1.2)
    subdivide(beam, 2, simple=True)
    wear(beam, 0.004, 0.2, seed=2)
    parts.append(beam)
    for sx in (-1, 1):
        x = sx * (W * 0.5 - 0.24)
        strap = box(f'strap{sx}', (0.08, 0.36, 0.32), loc=(x, 0, H - 0.18), mat=iron)
        inner = box(f'strapin{sx}', (0.1, 0.32, 0.28), loc=(x, 0, H - 0.18), mat=None)
        boolean(strap, inner)
        parts.append(strap)
        for sy in (-1, 1):
            parts.append(cylinder(f'bolt{sx}{sy}', 0.018, 0.018, 0.03, loc=(x, sy * 0.175, H - 0.18), rot=(math.pi / 2, 0, 0), segs=12, mat=iron))
    # the yoke and the bell: a real bell profile (crown, shoulder, waist, sound bow, lip), the clapper, the rope with its twist
    parts.append(box('yoke', (0.6, 0.12, 0.1), loc=(0, 0, H - 0.37), mat=oak))
    parts.append(cylinder('gudgeon', 0.025, 0.025, 0.2, loc=(0, 0, H - 0.37), rot=(0, math.pi / 2, 0), segs=12, mat=iron))
    crown_z = H - 0.42
    bell_prof = [(0, 0), (0.05, 0), (0.06, -0.03), (0.09, -0.05), (0.14, -0.08), (0.17, -0.14), (0.19, -0.3), (0.21, -0.45), (0.25, -0.56), (0.33, -0.63), (0.4, -0.67),
                 (0.42, -0.7), (0.41, -0.73), (0.33, -0.7), (0.27, -0.64), (0.22, -0.52), (0.2, -0.4), (0.17, -0.2), (0.12, -0.1), (0.06, -0.07), (0, -0.07)]
    bell = lathe('bell', [(r, crown_z + z) for r, z in bell_prof], segs=64, mat=BRONZE())
    bevel(bell, 0.004, 2)
    apply_mods(bell)
    box_uv(bell, 0.8)
    parts.append(bell)
    for k in range(6):  # the crown's loops
        a = math.tau * k / 6
        parts.append(torus(f'loop{k}', 0.035, 0.012, loc=(math.cos(a) * 0.06, math.sin(a) * 0.06, crown_z + 0.02), rot=(math.pi / 2, 0, a), mat=BRONZE(), segs=14, ring=8))
    for z in (-0.6, -0.42):  # cast rings round the bell
        parts.append(torus(f'ring{z}', 0.21 if z == -0.42 else 0.31, 0.008, loc=(0, 0, crown_z + z), mat=BRONZE(), segs=64, ring=8))
    parts.append(rod('clapper-rod', (0, 0, crown_z - 0.1), (0, 0, crown_z - 0.6), 0.02, 0.02, iron, 12))
    parts.append(sphere('clapper', 0.07, loc=(0, 0, crown_z - 0.62), subdiv=3, mat=iron))
    rope_top, rope_bottom = crown_z - 0.66, 0.85
    for i in range(14):  # the rope: a twisted pair of strands
        t0, t1 = i / 14, (i + 1) / 14
        for s in (0, 1):
            a0, a1 = t0 * 10 + s * math.pi, t1 * 10 + s * math.pi
            parts.append(rod(f'rope{i}{s}', (0.03 + math.cos(a0) * 0.012, math.sin(a0) * 0.012, rope_top - (rope_top - rope_bottom) * t0),
                             (0.03 + math.cos(a1) * 0.012, math.sin(a1) * 0.012, rope_top - (rope_top - rope_bottom) * t1), 0.013, 0.013, ROPE(), 10))
    parts.append(sphere('rope-knot', 0.035, loc=(0.03, 0, rope_bottom), subdiv=2, mat=ROPE()))
    for p in parts:
        if not p.data.uv_layers:
            box_uv(p, 0.6)
    o = join(parts, 'bell-cote')
    smooth(o, 40)
    ground(o)
    return o


def scriptorium_desk():
    oak, worn = OAK(), OAK_WORN()
    W, D, H = 1.1, 0.7, 0.95
    tilt = math.radians(-14)
    parts = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            leg = box(f'leg{sx}{sy}', (0.075, 0.075, H - 0.04), loc=(sx * (W * 0.5 - 0.06), sy * (D * 0.5 - 0.06), (H - 0.04) * 0.5), mat=oak)
            bevel(leg, 0.008, 3)
            apply_mods(leg)
            box_uv(leg, 0.7, (0.2 * sx, 0.3 * sy))
            parts.append(leg)
        side = box(f'side{sx}', (0.045, D - 0.15, 0.2), loc=(sx * (W * 0.5 - 0.06), 0, 0.26), mat=oak)
        bevel(side, 0.004, 2)
        apply_mods(side)
        box_uv(side, 0.7)
        parts.append(side)
        for sy in (-1, 1):  # the pegs of the through-tenons
            parts.append(cylinder(f'peg{sx}{sy}', 0.012, 0.012, 0.09, loc=(sx * (W * 0.5 - 0.06), sy * (D * 0.5 - 0.06), 0.26), rot=(0, math.pi / 2, 0), segs=10, mat=worn))
    stretcher = box('stretcher', (W - 0.2, 0.045, 0.14), loc=(0, 0, 0.26), mat=oak)
    bevel(stretcher, 0.004, 2)
    apply_mods(stretcher)
    box_uv(stretcher, 0.9)
    parts.append(stretcher)
    top = box('top', (W, D, 0.04), loc=(0, 0, H + 0.08), rot=(tilt, 0, 0), mat=worn)
    bevel(top, 0.006, 3)
    apply_mods(top)
    box_uv(top, 0.9)
    subdivide(top, 2, simple=True)
    wear(top, 0.003, 0.15, seed=5)
    parts.append(top)
    ledge = moulding('ledge', [(-0.025, 0), (0.025, 0), (0.03, 0.03), (0.02, 0.05), (-0.02, 0.05), (-0.03, 0.03)], W, loc=(0, -D * 0.5 + 0.02, H - 0.005), rot=(tilt, 0, 0), mat=oak)
    box_uv(ledge, 0.8)
    parts.append(ledge)
    # the shelf behind, with a row of leaning books
    shelf = box('shelf', (W, 0.2, 0.03), loc=(0, D * 0.5 + 0.1, H + 0.2), mat=oak)
    bevel(shelf, 0.004, 2)
    apply_mods(shelf)
    box_uv(shelf, 0.8)
    parts.append(shelf)
    for sx in (-1, 1):
        parts.append(box(f'shelfend{sx}', (0.03, 0.2, 0.26), loc=(sx * (W * 0.5 - 0.015), D * 0.5 + 0.1, H + 0.33), mat=oak))
    rng = rand(9)
    x = -W * 0.5 + 0.05
    while x < W * 0.5 - 0.12:
        t = rng.uniform(0.03, 0.06)
        h = rng.uniform(0.16, 0.22)
        leanb = rng.uniform(-0.12, 0.12) if rng.random() < 0.3 else 0.0
        bk = box(f'book{x:.2f}', (t, 0.17, h), loc=(x + t * 0.5, D * 0.5 + 0.1, H + 0.215 + h * 0.5), rot=(0, leanb, 0), mat=LEATHER() if rng.random() < 0.7 else PAPER())
        bevel(bk, 0.004, 2)
        apply_mods(bk)
        box_uv(bk, 0.4, (rng.random(), 0))
        parts.append(bk)
        x += t + 0.004
    # on the desk: two curled sheets, the inkwell, quills, a dripping candle in a dish
    for i, (sx, rz) in enumerate(((-0.18, 0.03), (-0.1, -0.06))):
        sheet = plane(f'sheet{i}', (0.42, 0.57), loc=(sx, 0.0, H + 0.105 + i * 0.004), rot=(tilt, 0, rz), mat=PAPER())
        subdivide(sheet, 4, simple=True)
        wear(sheet, 0.006, 0.12, seed=i + 20)
        parts.append(sheet)
    ink = lathe('inkwell', [(0, 0), (0.045, 0), (0.05, 0.02), (0.04, 0.06), (0.028, 0.07), (0.028, 0.065), (0, 0.065)], segs=24, loc=(0.36, 0.22, H + 0.13), mat=M('dark-glass'))
    parts.append(ink)
    for k, a in enumerate((0.5, -0.3)):
        q = rod(f'quill{k}', (0.36 + k * 0.03, 0.22, H + 0.19), (0.36 + math.sin(a) * 0.25, 0.22 + math.cos(a) * 0.12, H + 0.42), 0.005, 0.002, PAPER(), 8)
        parts.append(q)
        vane = plane(f'vane{k}', (0.035, 0.16), loc=(0.36 + math.sin(a) * 0.19, 0.22 + math.cos(a) * 0.09, H + 0.36), rot=(0, a, 0.3), mat=PAPER())
        parts.append(vane)
    parts.append(lathe('dish', [(0, 0), (0.06, 0), (0.07, 0.012), (0.06, 0.02), (0, 0.02)], segs=24, loc=(0.42, 0.44, H + 0.135), mat=BRONZE()))
    parts += candle('candle', (0.42, 0.44, H + 0.155), 0.14, r=0.03, lit=True, seed=4)
    for p in parts:
        if not p.data.uv_layers:
            box_uv(p, 0.6)
    o = join(parts, 'scriptorium-desk')
    smooth(o, 40)
    ground(o)
    return o


BUILDERS = [('pew', pew), ('lectern', lectern), ('votive-rack', votive_rack), ('saint-statue', saint_statue), ('bell-cote', bell_cote), ('scriptorium-desk', scriptorium_desk)]

if __name__ == '__main__':
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    only = set(argv)
    reports, errors = [], []
    for name, fn in BUILDERS:
        if only and name not in only:
            continue
        try:
            reset()
            o = fn()
            reports.append(export(o, KIT, name, version=V))
            print(f'[ok] {KIT}/{name}-{V}: {reports[-1]["triangles"]} tris, {reports[-1]["size"]}')
        except Exception as err:  # noqa: BLE001
            import traceback
            traceback.print_exc()
            errors.append(f'{name}: {err}')
            print(f'[fail] {KIT}/{name}: {err}')
    print('JSON_BEGIN' + json.dumps({'kit': KIT, 'assets': reports, 'errors': errors}) + 'JSON_END')
