"""Parametric builders shared by the second-wave kits (nature2, treasure, structures, tavern2, winter2, dungeon2,
mines, ember): rocks, crystals, logs, trees, chests, barrels, shelves, tables, gears, figures. Each returns one object
(parts joined), not yet grounded: the kit grounds it."""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emberlib import *  # noqa
from emberlib import _new_mesh_object
import kit_forest
import kit_extra

BARK = (0.30, 0.22, 0.15)
DEAD = (0.21, 0.17, 0.14)
LEAF = (0.16, 0.34, 0.12)
BONE = (0.82, 0.76, 0.62)


# ------------------------------------------------------------------------------------------------------- rocks
def rock(name, size, seed, mat='stone', top=None, up=0.5, subdiv=3, rough=0.3, fine=0.06, loc=(0, 0, 0), uv=1.3):
    r = max(size) * 0.5
    s = sphere(name, 1.0, (loc[0], loc[1], loc[2] + size[2] * 0.42), subdiv=subdiv, mat=M(mat), scale=(size[0] * 0.5, size[1] * 0.5, size[2] * 0.5))
    displace(s, rough * r, 0.9 * r, seed=seed)
    displace(s, fine * r, 0.2 * r, seed=seed + 1)
    apply_mods(s)
    clamp_floor(s, 0.0)
    box_uv(s, uv)
    if top:
        set_material_by_normal(s, M(top), up=up, noise_scale=0.6, seed=seed, threshold=0.5)
    return s


def rock_cluster(name, seed, n=5, rmax=0.45, mat='granite', top=None, spread=0.6):
    rng = rand(seed)
    parts = []
    for i in range(n):
        r = rmax * rng.uniform(0.45, 1.0)
        a = rng.uniform(0, math.tau)
        d = 0 if i == 0 else spread * rng.uniform(0.6, 1.2)
        parts.append(rock(f'{name}{i}', (r * 2 * rng.uniform(0.9, 1.4), r * 2 * rng.uniform(0.9, 1.3), r * 1.5), seed * 10 + i, mat, top,
                          loc=(math.cos(a) * d, math.sin(a) * d, 0)))
    o = join(parts, name)
    smooth(o, 45)
    return o


def stalagmite(name, h, r, loc=(0, 0, 0), mat='stone', seed=0, up=True, segs=12):
    prof = [(0, 0), (r, 0), (r * 0.8, h * 0.3), (r * 0.5, h * 0.65), (r * 0.22, h * 0.9), (0, h)]
    o = lathe(name, prof, segs=segs, mat=M(mat))
    subdivide(o, 1, simple=True)
    displace(o, r * 0.25, h * 0.3, seed=seed)
    apply_mods(o)
    if not up:
        o.rotation_euler = (math.pi, 0, 0)
    o.location = loc
    apply_transform(o)
    box_uv(o, 1.0)
    return o


# ---------------------------------------------------------------------------------------------------- crystals
def crystal(name, h, r, loc, direction=(0, 0, 1), mat='ember-crystal', seed=0, segs=6):
    rng = rand(seed)
    prof = [(0, 0), (r, 0), (r * 1.05, h * 0.55), (r * 0.6, h * 0.86), (0, h)]
    c = lathe(name, prof, segs=segs, mat=M(mat))
    c.rotation_euler = (0, 0, rng.uniform(0, 1.0))
    apply_transform(c)
    aim(c, Vector(direction).normalized())
    c.location = loc
    apply_transform(c)
    flat(c)
    return c


def crystal_cluster(name, seed, mat='ember-crystal', base_mat='stone', base=(1.6, 1.3, 0.6), spots=None, scale=1.0):
    rng = rand(seed)
    parts = []
    if base:
        parts.append(rock(f'{name}-base', base, seed, base_mat))
    spots = spots or [(0, 0, 1.4, 0.2), (0.35, 0.2, 1.0, 0.15), (-0.3, 0.25, 0.9, 0.14), (0.1, -0.4, 0.8, 0.12), (-0.45, -0.15, 0.7, 0.11), (0.5, -0.2, 0.55, 0.09)]
    for i, (x, y, h, r) in enumerate(spots):
        d = (x * 0.6 + rng.uniform(-0.2, 0.2), y * 0.6 + rng.uniform(-0.2, 0.2), 1.0)
        parts.append(crystal(f'{name}{i}', h * scale, r * scale, (x * scale, y * scale, base[2] * 0.35 if base else 0), d, mat, seed + i))
    o = join(parts, name)
    if base:
        for p in o.data.polygons:
            if p.material_index == 0:
                p.use_smooth = True
    return o


# ------------------------------------------------------------------------------------------------------- wood
def log(name, length, r, seed, axis='x', loc=(0, 0, 0), taper=0.8, rot=0.0):
    rotv = (0, math.pi / 2, rot) if axis == 'x' else ((math.pi / 2, 0, rot) if axis == 'y' else (0, 0, rot))
    c = cylinder(name, r, r * taper, length, loc=(loc[0], loc[1], loc[2] + r), rot=rotv, segs=16, mat=M('bark'))
    subdivide(c, 2, simple=True)
    displace(c, r * 0.18, r * 0.7, seed=seed)
    apply_mods(c)
    apply_transform(c)
    return c


def colour_bark(o, seed, base=BARK, var=0.4, top=None):
    vertex_colors(o, base, var, 0.4, seed, gradient=(0, 1.0, 0.75))
    set_material_where(o, M('planks-worn'), lambda c, n: abs(n.x) > 0.85)  # the cut ends
    if top:
        set_material_by_normal(o, M(top), up=0.6, noise_scale=0.6, seed=seed, threshold=0.5)
    box_uv(o, 1.0)
    smooth(o, 55)
    return o


def branch_pile(name, seed, n=12, length=1.6, r=0.05, spread=0.6, snow=False):
    rng = rand(seed)
    parts = []
    for i in range(n):
        a = rng.uniform(0, math.tau)
        L = length * rng.uniform(0.5, 1.0)
        c = cylinder(f'{name}{i}', r * rng.uniform(0.6, 1.2), r * 0.3, L, loc=(rng.uniform(-spread, spread), rng.uniform(-spread, spread), 0.04 + i * r * 0.9), segs=6, mat=M('bark'))
        aim(c, (math.cos(a), math.sin(a), rng.uniform(-0.1, 0.25)))
        apply_transform(c)
        parts.append(c)
        if rng.random() < 0.5:
            t = cylinder(f'{name}t{i}', r * 0.5, r * 0.15, L * 0.4, loc=(rng.uniform(-spread, spread), rng.uniform(-spread, spread), 0.06 + i * r), segs=5, mat=M('bark'))
            aim(t, (math.cos(a + 0.6), math.sin(a + 0.6), 0.3))
            apply_transform(t)
            parts.append(t)
    o = join(parts, name)
    clamp_floor(o, 0.0)
    vertex_colors(o, DEAD, 0.4, 0.3, seed)
    if snow:
        set_material_by_normal(o, M('snow'), up=0.5, noise_scale=0.3, seed=seed, threshold=0.4)
    smooth(o, 60)
    return o


def tree(name, seed, canopy=True, snow_canopy=False, roots=6, root_r=0.26, root_len=1.3, trunk_h=3.6, bark=BARK, hollow=False, dead=False):
    rng = rand(seed)
    bm = M('bark')
    parts = []
    trunk = cylinder('trunk', 0.6, 0.32, trunk_h, loc=(0, 0, trunk_h * 0.5), segs=18, mat=bm)
    subdivide(trunk, 2, simple=True)
    displace(trunk, 0.16, 0.35, seed=seed)
    apply_mods(trunk)
    if hollow:
        hole = cylinder('hole', 0.42, 0.3, 1.6, loc=(0, 0, 0.6), segs=12)
        boolean(trunk, hole)
        mouth = box('mouth', (0.6, 0.9, 1.1), loc=(0, -0.55, 0.62))
        boolean(trunk, mouth)
    parts.append(trunk)
    kit_forest._roots(parts, rng, roots, root_r, root_len, bm, z=0.3)
    tips = []
    for i in range(6 if not dead else 4):
        a = math.tau * i / 6 + rng.uniform(-0.25, 0.25)
        d = Vector((math.cos(a) * 0.8, math.sin(a) * 0.8, 0.75)).normalized()
        tips.append(kit_forest._branch(parts, rng, (0, 0, trunk_h * 0.65 + i * 0.22), d, 2.4, 0.24, 0.09, 2 if dead else 1, bm, twist=0.9 if dead else 0.5, children=2))
    tips.append(kit_forest._branch(parts, rng, (0, 0, trunk_h), (0.1, -0.05, 1), 1.8, 0.3, 0.1, 1, bm, children=2))
    wood = join(parts, 'wood')
    vertex_colors(wood, bark, 0.38, 0.5, seed, gradient=(0, 2.5, 0.7))
    smooth(wood, 60)
    if not canopy:
        return wood
    canopy_parts = []
    for i, tip in enumerate(tips):
        for j in range(2):
            r = rng.uniform(1.25, 1.9)
            at = tip + Vector((rng.uniform(-0.6, 0.6), rng.uniform(-0.6, 0.6), rng.uniform(0.2, 0.9) + j * 0.5))
            s = sphere(f'leaf{i}{j}', r, tuple(at), subdiv=2, mat=M('leaves'), scale=(1, 1, 0.8))
            displace(s, 0.45, 0.7, seed=seed * 7 + i * 3 + j)
            apply_mods(s)
            canopy_parts.append(s)
    crown = join(canopy_parts, 'crown')
    vertex_colors(crown, LEAF, 0.45, 0.9, seed + 1, gradient=(3.0, 7.5, 0.55))
    if snow_canopy:
        set_material_by_normal(crown, M('snow'), up=0.45, noise_scale=0.8, seed=seed, threshold=0.4)
        box_uv(crown, 1.4)
    smooth(crown, 70)
    return join([wood, crown], name)


# ------------------------------------------------------------------------------------------------ containers
def plank_box(name, size, loc, mat='planks-worn', bev=0.015, uv=1.2, off=(0, 0), rot=(0, 0, 0)):
    b = box(name, size, loc=loc, rot=rot, mat=M(mat))
    if bev:
        bevel(b, bev, 1)
        apply_mods(b)
    box_uv(b, uv, off)
    return b


def chest(name, L=1.3, D=0.8, H=0.6, R=0.4, wood='planks-dark', band='iron', bands=3, studs=False, lid='arch', open_lid=False, broken=False):
    parts = [plank_box('body', (L, D, H), (0, 0, H * 0.5), wood, 0.02)]
    if lid == 'arch':
        lidobj = prism('lid', half_circle(R, 14, 0.0), L, loc=(0, 0, H), mat=M(wood))
    elif lid == 'flat':
        lidobj = plank_box('lid', (L + 0.06, D + 0.06, 0.1), (0, 0, H + 0.05), wood, 0.015)
    else:
        lidobj = None
    if lidobj is not None:
        if open_lid:
            lidobj.location = (0, D * 0.5, 0)
            lidobj.rotation_euler = (math.radians(-105), 0, 0)
            lidobj.location = (0, D * 0.5 + 0.05, H + 0.02)
            # rotate about the back edge: shift so the hinge stays on the body's back edge
        if lid == 'arch':
            bevel(lidobj, 0.015, 1)
            apply_mods(lidobj)
        box_uv(lidobj, 1.3, (0.3, 0))
        if open_lid:
            apply_transform(lidobj)
        parts.append(lidobj)
    if band:
        bm = M(band)
        for i in range(bands):
            x = -L * 0.36 + (L * 0.72) * i / max(1, bands - 1) if bands > 1 else 0
            parts.append(box(f'band{i}', (0.08, D + 0.03, H + 0.015), loc=(x, 0, H * 0.5 + 0.005), mat=bm))
            if lid == 'arch' and not open_lid:
                parts.append(prism(f'arch{i}', half_circle(R + 0.03, 14, 0.0) + half_circle(R - 0.01, 14, 0.0)[::-1], 0.08, loc=(x, 0, H), mat=bm))
        parts.append(box('lock', (0.16, 0.05, 0.2), loc=(0, -D * 0.5 - 0.01, H - 0.06), mat=bm))
        for sx in (-1, 1):
            for sy in (-1, 1):
                parts.append(box(f'corner{sx}{sy}', (0.07, 0.07, 0.2), loc=(sx * (L * 0.5 - 0.02), sy * (D * 0.5 - 0.02), 0.1), mat=bm))
    if studs:
        for i in range(6):
            for z in (0.2, H - 0.2):
                parts.append(sphere(f'stud{i}{z}', 0.025, (-L * 0.42 + L * 0.84 * i / 5, -D * 0.5 - 0.01, z), subdiv=1, mat=M('brass')))
    o = join(parts, name)
    if broken:
        cut = box('cut', (0.6, 0.5, 0.7), loc=(L * 0.45, -D * 0.4, H * 0.9), rot=(0.3, 0.5, 0.4))
        boolean(o, cut)
    smooth(o, 40)
    return o


def barrel(name, H=1.3, R=0.48, open_top=False, broken=False, hoops=4, wood='planks-dark', hoop='iron', snow=False):
    prof = [(0.0, 0.0), (R * 0.92, 0.0)]
    for i in range(1, 8):
        z = H * i / 8
        t = 2 * z / H - 1
        prof.append((R * (1.0 + 0.16 * (1 - t * t)), z))
    if open_top:
        prof += [(R * 0.92, H), (R * 0.84, H), (R * 0.8, H * 0.7), (0.0, H * 0.7)]
    else:
        prof += [(R * 0.92, H), (0.0, H)]
    b = lathe(name + '-body', prof, segs=28, mat=M(wood))
    box_uv(b, 1.1)
    parts = [b]
    zs = [0.14, H - 0.14] if hoops == 2 else [0.14, H * 0.5 - 0.28, H * 0.5 + 0.28, H - 0.14]
    for z in zs:
        t = 2 * z / H - 1
        parts.append(torus(f'hoop{z}', R * (1.0 + 0.16 * (1 - t * t)) + 0.005, 0.035, loc=(0, 0, z), mat=M(hoop), segs=28, ring=8))
    if open_top:
        parts.append(cylinder('liquid', R * 0.82, R * 0.82, 0.02, loc=(0, 0, H * 0.7), segs=28, mat=M('dark-glass')))
    o = join(parts, name)
    if broken:
        for i, a in enumerate((0.4, 2.3, 4.1)):
            cut = box(f'cut{i}', (0.5, 0.35, 0.8), loc=(math.cos(a) * R * 1.1, math.sin(a) * R * 1.1, H * 0.85), rot=(0.3, 0.2, a))
            boolean(o, cut)
    if snow:
        set_material_by_normal(o, M('snow'), up=0.6, noise_scale=0.4, seed=3, threshold=0.4)
    smooth(o, 40)
    return o


def sack(name, r, loc, seed, mat='burlap', tie=True):
    s = sphere(name, r, (loc[0], loc[1], loc[2] + r * 0.95), subdiv=3, mat=M(mat), scale=(1.05, 0.95, 1.1))
    displace(s, r * 0.3, r * 0.8, seed=seed)
    apply_mods(s)
    clamp_floor(s, loc[2])
    parts = [s]
    if tie:
        parts.append(cylinder(f'{name}neck', r * 0.3, r * 0.22, r * 0.45, loc=(loc[0], loc[1], loc[2] + r * 2.0), segs=10, mat=M(mat)))
        parts.append(torus(f'{name}tie', r * 0.3, 0.02, loc=(loc[0], loc[1], loc[2] + r * 1.95), mat=M('rope'), segs=16, ring=6))
    return join(parts, name)


def vessel(name, prof, loc, mat, scale=1.0, segs=18, handle=False):
    p = [(r * scale, z * scale) for r, z in prof]
    o = lathe(name, p, segs=segs, loc=loc, mat=M(mat))
    if handle:
        rmax = max(r for r, z in p)
        h = torus(f'{name}h', 0.1 * scale, 0.02 * scale, loc=(loc[0] + rmax, loc[1], loc[2] + 0.3 * scale), rot=(0, math.pi / 2, 0), mat=M(mat), segs=14, ring=6)
        o = join([o, h], name)
    return o


JUG = [(0, 0), (0.14, 0), (0.19, 0.18), (0.17, 0.36), (0.1, 0.44), (0.11, 0.5), (0, 0.5)]
BOTTLE = [(0, 0), (0.085, 0), (0.09, 0.3), (0.06, 0.38), (0.03, 0.42), (0.03, 0.55), (0.035, 0.57), (0, 0.57)]
MUG = [(0, 0), (0.09, 0), (0.1, 0.22), (0.085, 0.22), (0.075, 0.02), (0, 0.02)]
BOWL = [(0, 0), (0.1, 0), (0.22, 0.08), (0.24, 0.12), (0.22, 0.12), (0.2, 0.1), (0.1, 0.03), (0, 0.03)]
PLATE = [(0, 0), (0.14, 0), (0.26, 0.03), (0.27, 0.04), (0.25, 0.04), (0.14, 0.012), (0, 0.012)]
URN = [(0, 0), (0.22, 0), (0.3, 0.1), (0.36, 0.5), (0.3, 0.85), (0.2, 0.95), (0.24, 1.0), (0.2, 1.02), (0, 1.02)]
JAR = [(0, 0), (0.24, 0), (0.34, 0.3), (0.32, 0.7), (0.2, 0.85), (0.22, 0.9), (0, 0.9)]
BASKET = [(0, 0), (0.3, 0), (0.38, 0.4), (0.36, 0.42), (0.28, 0.04), (0, 0.04)]


# ---------------------------------------------------------------------------------------------------- furniture
def table(name, L, D, H, wood='planks-worn', legs='planks-dark', round_top=False, stretchers=True, leg=0.14):
    if round_top:
        top = cylinder('top', L * 0.5, L * 0.5, 0.1, loc=(0, 0, H - 0.05), segs=28, mat=M(wood))
        box_uv(top, 1.3)
        parts = [top, cylinder('stem', 0.14, 0.16, H - 0.1, loc=(0, 0, (H - 0.1) * 0.5), segs=12, mat=M(legs)),
                 cylinder('foot', L * 0.3, L * 0.3, 0.06, loc=(0, 0, 0.03), segs=20, mat=M(legs))]
    else:
        parts = [plank_box('top', (L, D, 0.1), (0, 0, H - 0.05), wood, 0.02, 1.3, (0.2, 0.1))]
        for sx in (-1, 1):
            for sy in (-1, 1):
                parts.append(plank_box(f'leg{sx}{sy}', (leg, leg, H - 0.1), (sx * (L * 0.5 - 0.22), sy * (D * 0.5 - 0.2), (H - 0.1) * 0.5), legs, 0, 0.7))
            if stretchers:
                parts.append(plank_box(f'str{sx}', (0.08, D - 0.5, 0.1), (sx * (L * 0.5 - 0.22), 0, 0.3), legs, 0, 0.7))
        if stretchers:
            parts.append(plank_box('str', (L - 0.5, 0.08, 0.1), (0, 0, 0.3), legs, 0, 0.7))
    o = join(parts, name)
    smooth(o, 40)
    return o


def shelf(name, W, D, H, n=3, wood='planks-dark', back=True, open_front=True):
    t = 0.06
    parts = [plank_box('side-l', (t, D, H), (-W * 0.5 + t * 0.5, 0, H * 0.5), wood, 0.01),
             plank_box('side-r', (t, D, H), (W * 0.5 - t * 0.5, 0, H * 0.5), wood, 0.01)]
    if back:
        parts.append(plank_box('back', (W, t, H), (0, D * 0.5 - t * 0.5, H * 0.5), 'planks-worn', 0.01))
    for i in range(n + 1):
        z = t * 0.5 + (H - t) * i / n
        parts.append(plank_box(f'shelf{i}', (W - 2 * t, D - t, t), (0, -t * 0.5, z), wood, 0.01, 1.2, (0, 0.3 * i)))
    o = join(parts, name)
    smooth(o, 40)
    return o


def bench(name, L, wood='planks-worn', legs='planks-dark', H=0.62):
    parts = [plank_box('seat', (L, 0.4, 0.08), (0, 0, H - 0.04), wood, 0.015, 1.2)]
    for sx in (-1, 1):
        parts.append(plank_box(f'leg{sx}', (0.08, 0.36, H - 0.08), (sx * (L * 0.5 - 0.25), 0, (H - 0.08) * 0.5), legs, 0.01, 0.8))
    parts.append(plank_box('str', (L - 0.6, 0.06, 0.08), (0, 0, 0.25), legs, 0, 0.7))
    o = join(parts, name)
    smooth(o, 40)
    return o


# ------------------------------------------------------------------------------------------------------- stone
def stone_block(name, size, loc, seed, mat='masonry', rot=(0, 0, 0), bev=0.03, disp=0.03):
    b = box(name, size, loc=loc, rot=rot, mat=M(mat))
    bevel(b, bev, 1)
    if disp:
        displace(b, disp, 0.2, seed=seed)
    apply_mods(b)
    box_uv(b, 1.1)
    return b


def block_pile(name, seed, n=12, smax=0.6, spread=0.7, mat='masonry', mat2='masonry-mossy', dust='ruins-floor'):
    rng = rand(seed)
    parts = []
    for i in range(n):
        s = rng.uniform(0.3, smax)
        parts.append(stone_block(f'{name}{i}', (s, s * rng.uniform(0.6, 1.0), s * rng.uniform(0.5, 0.9)),
                                 (rng.uniform(-spread, spread), rng.uniform(-spread * 0.9, spread * 0.9), rng.uniform(0.05, 0.4) * (1 if i > n // 3 else 0.3)),
                                 seed + i, mat if i % 3 else mat2, rot=(rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5), rng.uniform(0, 3.14))))
    if dust:
        d = cylinder('dust', spread * 1.5, spread * 1.3, 0.1, loc=(0, 0, 0.05), segs=18, mat=M(dust))
        displace(d, 0.06, 0.3, seed=seed + 40)
        apply_mods(d)
        box_uv(d, 1.5)
        parts.append(d)
    o = join(parts, name)
    clamp_floor(o, 0.0)
    smooth(o, 40)
    return o


def pillar(name, prof, seed, mat='masonry-crypt', segs=22, chips=3, disp=0.05):
    p = lathe(name, prof, segs=segs, mat=M(mat))
    subdivide(p, 1, simple=True)
    displace(p, disp, 0.25, seed=seed)
    apply_mods(p)
    box_uv(p, 1.2)
    rmax = max(r for r, z in prof)
    zmax = max(z for r, z in prof)
    for i in range(chips):
        chip = sphere(f'chip{i}', rmax * 0.4, (rmax * 0.85 * math.cos(i * 2.1), rmax * 0.85 * math.sin(i * 2.1), 0.2 + zmax * i / max(1, chips)), subdiv=2)
        boolean(p, chip)
    smooth(p, 40)
    return p


def figure(name, h, mat='masonry-crypt', hood=True, seed=0):
    """An abstract robed figure (a statue or a totem's top): lathe body, shoulders, head, hood; arms folded."""
    body = [(0, 0), (0.42, 0), (0.4, 0.05), (0.3, h * 0.55), (0.34, h * 0.7), (0.26, h * 0.8), (0.14, h * 0.84), (0, h * 0.84)]
    b = lathe('body', [(r * h / 2.2, z) for r, z in body], segs=16, mat=M(mat))
    head = sphere('head', h * 0.09, (0, 0, h * 0.9), subdiv=3, mat=M(mat))
    parts = [b, head]
    if hood:
        hp = [(0, 0), (0.13, 0), (0.14, 0.09), (0.1, 0.16), (0, 0.19)]
        hd = lathe('hood', [(r * h, z * h) for r, z in hp], segs=14, loc=(0, 0.02 * h, h * 0.82), mat=M(mat))
        parts.append(hd)
    arms = box('arms', (h * 0.3, h * 0.12, h * 0.08), loc=(0, -h * 0.12, h * 0.6), mat=M(mat))
    bevel(arms, h * 0.02, 2)
    apply_mods(arms)
    parts.append(arms)
    o = join(parts, name)
    subdivide(o, 1, simple=True)
    displace(o, 0.02, 0.2, seed=seed)
    apply_mods(o)
    box_uv(o, 1.2)
    smooth(o, 50)
    return o


# ----------------------------------------------------------------------------------------------------- machinery
def gear(name, r, teeth, thick, loc=(0, 0, 0), rot=(0, 0, 0), mat='iron', hole=0.0):
    prof = []
    for i in range(teeth):
        a0 = math.tau * i / teeth
        a1 = a0 + math.tau / teeth * 0.5
        rin, rout = r * 0.86, r
        for a, rr in ((a0, rin), (a0 + math.tau / teeth * 0.12, rout), (a1 - math.tau / teeth * 0.12, rout), (a1, rin)):
            prof.append((math.cos(a) * rr, math.sin(a) * rr))
    g = prism(name, prof, thick, loc=loc, rot=rot, mat=M(mat))
    if hole:
        h = cylinder('hole', hole, hole, thick * 3, loc=loc, rot=(rot[0], rot[1] + math.pi / 2, rot[2]), segs=12)
        boolean(g, h)
    return g


def chain(name, start, end, n, link=0.085, mat='iron-rust', sag=0.0):
    """n links from start to end (the ring plane contains the chain's direction; every second link turned 90 deg)."""
    s, e = Vector(start), Vector(end)
    d = (e - s).normalized()
    u = d.cross(Vector((0, 0, 1)))
    if u.length < 1e-3:
        u = d.cross(Vector((1, 0, 0)))
    u.normalize()
    v = d.cross(u).normalized()
    parts = []
    for i in range(n):
        t = i / max(1, n - 1)
        p = s.lerp(e, t) + Vector((0, 0, -sag * math.sin(t * math.pi)))
        o = torus(f'{name}{i}', link, link * 0.28, mat=M(mat), segs=12, ring=6)
        aim(o, u if i % 2 == 0 else v)
        o.location = p
        apply_transform(o)
        parts.append(o)
    return join(parts, name)


def planks_row(name, n, L, W, T, loc, mat='planks-worn', gap=0.02, axis='x', seed=0):
    rng = rand(seed)
    parts = []
    for i in range(n):
        off = (i - (n - 1) * 0.5) * (W + gap)
        p = (loc[0], loc[1] + off, loc[2]) if axis == 'x' else (loc[0] + off, loc[1], loc[2])
        size = (L * rng.uniform(0.97, 1.0), W, T) if axis == 'x' else (W, L * rng.uniform(0.97, 1.0), T)
        parts.append(plank_box(f'{name}{i}', size, p, mat, 0.01, 1.2, (0.3 * i, 0)))
    return join(parts, name)


def skull(name, loc, r=0.22, seed=0):
    s = sphere('skull', r, (loc[0], loc[1], loc[2] + r), subdiv=3, mat=M('bone'), scale=(1.0, 1.15, 0.95))
    for sy in (-1, 1):
        socket = sphere(f'socket{sy}', r * 0.34, (loc[0] + sy * r * 0.4, loc[1] + r * 0.9, loc[2] + r * 1.1), subdiv=2)
        boolean(s, socket)
    jaw = box('jaw', (r * 0.9, r * 0.7, r * 0.35), loc=(loc[0], loc[1] + r * 0.45, loc[2] + r * 0.25), mat=M('bone'))
    bevel(jaw, r * 0.12, 2)
    apply_mods(jaw)
    o = join([s, jaw], name)
    vertex_colors(o, BONE, 0.22, 0.3, seed)
    smooth(o, 50)
    return o


def bone(name, L, loc, direction, r=0.045, seed=0):
    d = Vector(direction).normalized()
    shaft = cylinder(name, r, r * 0.9, L, loc=loc, segs=8, mat=M('bone'))
    aim(shaft, d)
    apply_transform(shaft)
    parts = [shaft]
    for e in (-1, 1):
        parts.append(sphere(f'{name}k{e}', r * 1.5, tuple(Vector(loc) + d * (L * 0.5 * e)), subdiv=2, mat=M('bone'), scale=(1.3, 1, 0.8)))
    return join(parts, name)
