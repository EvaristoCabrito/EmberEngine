"""Wave 2, group 8: Ember Ashes' own objects (content/models/ember): Ember crystals, relics, cult stones, ancient machines."""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kitlib import *  # noqa


def g(o):
    ground(o)
    return o


def refined_ember_shard():
    stand = lathe('stand', [(0, 0), (0.25, 0), (0.26, 0.03), (0.1, 0.06), (0.06, 0.2), (0, 0.2)], segs=12, mat=M('iron'))
    parts = [stand]
    for i in range(3):
        a = math.tau * i / 3
        p = cylinder(f'prong{i}', 0.02, 0.012, 0.35, loc=(math.cos(a) * 0.1, math.sin(a) * 0.1, 0.35), segs=6, mat=M('iron'))
        aim(p, (math.cos(a) * 0.25, math.sin(a) * 0.25, 1))
        apply_transform(p)
        parts.append(p)
    parts.append(crystal('shard', 0.8, 0.12, (0, 0, 0.22), (0, 0, 1), 'ember-glow', 801))
    o = join(parts, 'refined-ember-shard')
    return g(o)


def raw_ember_deposit():
    spots = [(0, 0, 1.3, 0.22), (0.5, 0.3, 1.0, 0.17), (-0.45, 0.35, 0.9, 0.15), (0.2, -0.6, 0.8, 0.13), (-0.6, -0.3, 0.7, 0.12), (0.8, -0.3, 0.55, 0.1), (-0.1, 0.7, 0.5, 0.09), (0.9, 0.5, 0.45, 0.08), (-0.9, 0.1, 0.4, 0.08)]
    o = crystal_cluster('raw-ember-deposit', 802, mat='ember-crystal', base=(2.4, 2.0, 0.9), spots=spots)
    return g(o)


def ember_container():
    can = lathe('can', [(0, 0), (0.3, 0), (0.32, 0.05), (0.32, 0.75), (0.28, 0.8), (0.12, 0.82), (0.12, 0.9), (0, 0.9)], segs=16, mat=M('iron'))
    parts = [can]
    for z in (0.12, 0.68):
        parts.append(torus(f'band{z}', 0.325, 0.025, loc=(0, 0, z), mat=M('brass'), segs=16, ring=6))
    for i in range(4):
        a = math.tau * i / 4
        parts.append(box(f'slit{i}', (0.05, 0.03, 0.4), loc=(math.cos(a) * 0.32, math.sin(a) * 0.32, 0.4), rot=(0, 0, a), mat=M('ember-glow')))
    parts.append(kit_extra.arc('handle', 0.16, 0.015, math.pi, loc=(0, 0, 0.9), rot=(math.pi / 2, 0, 0), mat=M('brass'), segs=14, ring=6))
    o = join(parts, 'ember-container')
    smooth(o, 40)
    return g(o)


def ember_lantern():
    S, H = 0.34, 0.5
    parts = [box('base', (S, S, 0.05), loc=(0, 0, 0.025), mat=M('iron')), box('top', (S * 0.9, S * 0.9, 0.04), loc=(0, 0, H + 0.05), mat=M('iron')),
             box('glass', (S * 0.8, S * 0.8, H - 0.02), loc=(0, 0, H * 0.5 + 0.04), mat=M('glass'))]
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(cylinder(f'bar{sx}{sy}', 0.018, 0.018, H, loc=(sx * S * 0.44, sy * S * 0.44, H * 0.5 + 0.04), segs=6, mat=M('iron')))
    parts.append(cylinder('cap', S * 0.4, 0.03, 0.14, loc=(0, 0, H + 0.14), segs=6, mat=M('iron')))
    parts.append(torus('handle', 0.13, 0.014, loc=(0, 0, H + 0.3), rot=(math.pi / 2, 0, 0), mat=M('brass'), segs=18, ring=6))
    parts.append(crystal('core', 0.3, 0.07, (0, 0, 0.08), (0, 0, 1), 'ember-glow', 803))
    parts.append(cylinder('pipe', 0.02, 0.02, 0.2, loc=(S * 0.45, 0, 0.2), rot=(0, 0.5, 0), segs=6, mat=M('brass')))
    o = join(parts, 'ember-lantern')
    smooth(o, 40)
    return g(o)


def ember_extraction_altar(seed=804):
    base = stone_block('base', (2.2, 1.4, 0.9), (0, 0, 0.45), seed, 'masonry-crypt', bev=0.04)
    for z in (0.3, 0.6):
        boolean(base, box(f'g{z}', (2.0, 0.05, 0.04), loc=(0, -0.7, z)))
    parts = [base]
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(box(f'post{sx}{sy}', (0.08, 0.08, 1.6), loc=(sx * 0.9, sy * 0.5, 1.7), mat=M('iron')))
    parts.append(torus('ring', 0.6, 0.04, loc=(0, 0, 2.4), mat=M('brass'), segs=28, ring=8))
    for sx in (-1, 1):
        parts.append(box(f'bar{sx}', (0.06, 1.1, 0.06), loc=(sx * 0.9, 0, 2.45), mat=M('iron')))
        parts.append(cylinder(f'pipe{sx}', 0.04, 0.04, 1.3, loc=(sx * 0.6, 0.55, 0.9), rot=(0, 0.4 * sx, 0), segs=8, mat=M('brass')))
    parts.append(crystal('crystal', 1.1, 0.2, (0, 0, 0.9), (0, 0, 1), 'ember-glow', seed))
    for i in range(4):
        a = math.tau * i / 4 + 0.4
        parts.append(crystal(f'c{i}', 0.45, 0.08, (math.cos(a) * 0.4, math.sin(a) * 0.4, 0.9), (math.cos(a) * 0.5, math.sin(a) * 0.5, 1), 'ember-crystal', seed + i))
    parts.append(vessel('bowl', BOWL, (0.7, -0.4, 0.9), 'bronze', 1.0))
    o = join(parts, 'ember-extraction-altar')
    smooth(o, 40)
    return g(o)


def containment_box():
    body = plank_box('body', (1.0, 0.7, 0.55), (0, 0, 0.275), 'iron', 0.03, 1.0)
    lid = plank_box('lid', (1.04, 0.74, 0.12), (0, 0, 0.61), 'iron-rust', 0.03, 1.0)
    for x in (-0.3, 0, 0.3):
        boolean(lid, box(f'r{x}', (0.12, 0.12, 0.03), loc=(x, 0, 0.67), rot=(0, 0, 0.785)))
        boolean(body, box(f'rf{x}', (0.1, 0.04, 0.18), loc=(x, -0.35, 0.3)))
    parts = [body, lid]
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(box(f'corner{sx}{sy}', (0.1, 0.1, 0.62), loc=(sx * 0.48, sy * 0.33, 0.31), mat=M('brass')))
    for x in (-0.3, 0, 0.3):
        parts.append(box(f'rune{x}', (0.1, 0.1, 0.02), loc=(x, 0, 0.67), rot=(0, 0, 0.785), mat=M('ember-glow')))
    parts.append(box('clasp', (0.14, 0.05, 0.16), loc=(0, -0.37, 0.55), mat=M('brass')))
    o = join(parts, 'containment-box')
    smooth(o, 40)
    return g(o)


def ancient_reliquary():
    body = plank_box('body', (0.7, 0.45, 0.4), (0, 0, 0.2), 'bronze', 0.025, 1.0)
    dome = sphere('dome', 1.0, (0, 0, 0.4), subdiv=3, mat=M('bronze'), scale=(0.35, 0.22, 0.2))
    boolean(dome, box('cut', (2, 2, 1), loc=(0, 0, -0.1)))
    parts = [body, dome, box('band', (0.74, 0.49, 0.06), loc=(0, 0, 0.4), mat=M('gold')), box('band2', (0.74, 0.49, 0.04), loc=(0, 0, 0.05), mat=M('gold'))]
    for i in range(5):
        for sy in (-1, 1):
            parts.append(sphere(f'gem{i}{sy}', 0.03, (-0.26 + i * 0.13, sy * 0.235, 0.22), subdiv=2, mat=M('ember-crystal')))
    parts.append(sphere('finial', 0.05, (0, 0, 0.62), subdiv=2, mat=M('gold')))
    o = join(parts, 'ancient-reliquary')
    smooth(o, 40)
    return g(o)


def cult_symbol_stone(seed=805):
    s = box('stone', (1.5, 0.32, 1.9), loc=(0, 0, 0.95), mat=M('masonry-crypt'))
    boolean(s, cylinder('circle', 0.5, 0.5, 0.1, loc=(0, -0.16, 1.1), rot=(math.pi / 2, 0, 0), segs=24))
    inner = cylinder('inner', 0.42, 0.42, 0.2, loc=(0, -0.16, 1.1), rot=(math.pi / 2, 0, 0), segs=24, mat=M('masonry-crypt'))
    s = boolean(s, inner, 'UNION')
    boolean(s, cylinder('tri', 0.36, 0.36, 0.06, loc=(0, -0.17, 1.1), rot=(math.pi / 2, 0, 0), segs=3))
    for z in (0.3, 0.5):
        boolean(s, box(f'g{z}', (1.0, 0.06, 0.04), loc=(0, -0.16, z)))
    subdivide(s, 1, simple=True)
    displace(s, 0.02, 0.2, seed=seed)
    apply_mods(s)
    box_uv(s, 1.0)
    base = stone_block('base', (1.8, 0.7, 0.2), (0, 0, 0.1), seed, 'masonry-mossy', bev=0.03)
    o = join([s, base], 'cult-symbol-stone')
    smooth(o, 40)
    return g(o)


def ritual_totem(seed=806):
    post = lathe('post', [(0, 0), (0.3, 0), (0.3, 0.6), (0.24, 0.7), (0.3, 1.4), (0.22, 1.5), (0.28, 2.2), (0.2, 2.3), (0, 2.3)], segs=10, mat=M('planks-dark'))
    box_uv(post, 0.8)
    for z in (0.3, 0.95, 1.8):
        boolean(post, box(f'g{z}', (0.7, 0.06, 0.05), loc=(0, -0.3, z)))
        boolean(post, box(f'v{z}', (0.06, 0.06, 0.25), loc=(0.1, -0.3, z + 0.15)))
    parts = [post, skull('skull', (0, -0.02, 2.3), r=0.2, seed=seed)]
    for z in (0.65, 1.45):
        parts.append(torus(f'rope{z}', 0.29, 0.02, loc=(0, 0, z), mat=M('rope'), segs=16, ring=6))
    for i in range(4):
        a = math.tau * i / 4 + 0.3
        parts.append(box(f'strip{i}', (0.08, 0.01, 0.5), loc=(math.cos(a) * 0.3, math.sin(a) * 0.3, 1.2), rot=(0.1, 0, a), mat=M('cloth-red' if i % 2 else 'cloth-dark')))
    parts.append(crystal('crystal', 0.4, 0.07, (0.2, -0.2, 1.0), (0.4, -0.4, 1), 'ember-crystal', seed))
    base = rock('base', (1.0, 1.0, 0.3), seed, 'stone')
    parts.append(base)
    o = join(parts, 'ritual-totem')
    smooth(o, 40)
    return g(o)


def ember_transformed_statue(seed=807):
    f = figure('statue', 2.8, 'masonry-crypt', hood=True, seed=seed)
    ped = pillar('pedestal', [(0, 0), (0.8, 0), (0.8, 0.15), (0.65, 0.25), (0.65, 0.45), (0, 0.45)], seed, segs=8, chips=0, disp=0.015)
    f.data.transform(Matrix.Translation((0, 0, 0.45)))
    parts = [f, ped]
    rng = rand(seed)
    for i in range(9):
        z = rng.uniform(0.6, 2.6)
        a = rng.uniform(-1.2, 1.2)
        r = 0.42 * (1 - z / 3.4) + 0.1
        parts.append(crystal(f'c{i}', rng.uniform(0.25, 0.6), rng.uniform(0.05, 0.1), (math.cos(a) * r, -math.sin(a) * r - 0.1, z), (math.cos(a), -math.sin(a) - 0.3, 0.5), 'ember-crystal' if i % 3 else 'ember-glow', seed + i))
    o = join(parts, 'ember-transformed-statue')
    return g(o)


def cracked_ceremonial_stone(seed=808):
    s = lathe('stone', [(0, 0), (0.6, 0), (0.62, 0.3), (0.5, 1.4), (0.42, 2.2), (0.25, 2.6), (0, 2.7)], segs=9, mat=M('masonry-crypt'))
    subdivide(s, 1, simple=True)
    displace(s, 0.06, 0.4, seed=seed)
    apply_mods(s)
    box_uv(s, 1.1)
    boolean(s, box('crack', (0.05, 1.6, 1.8), loc=(0.1, 0, 1.4), rot=(0.1, 0.2, 0.3)))
    glow = box('glow', (0.03, 1.2, 1.6), loc=(0.1, 0, 1.4), rot=(0.1, 0.2, 0.3), mat=M('ember-glow'))
    o = join([s, glow], 'cracked-ceremonial-stone')
    smooth(o, 40)
    return g(o)


def obelisk_fragment(seed=809):
    ob = lathe('obelisk', [(0, 0), (0.55, 0), (0.55, 0.3), (0.45, 0.4), (0.45, 2.9), (0, 2.9)], segs=4, mat=M('masonry-crypt'))
    ob.rotation_euler = (0, 0, math.pi / 4)
    apply_transform(ob)
    box_uv(ob, 1.0)
    boolean(ob, box('break', (2, 2, 1.4), loc=(0.2, 0.1, 3.3), rot=(0.5, 0.2, 0.3)))
    for z in (1.0, 1.5, 2.0):
        boolean(ob, box(f'g{z}', (0.25, 0.08, 0.04), loc=(0, -0.45, z)))
        boolean(ob, cylinder(f'c{z}', 0.05, 0.05, 0.08, loc=(0, -0.45, z + 0.15), rot=(math.pi / 2, 0, 0), segs=8))
    tip = lathe('tip', [(0, 0), (0.4, 0), (0.1, 0.5), (0, 0.5)], segs=4, mat=M('masonry-crypt'))
    tip.rotation_euler = (1.3, 0.3, 0.6)
    tip.location = (1.3, 0.6, 0.3)
    apply_transform(tip)
    box_uv(tip, 1.0)
    o = join([ob, tip], 'obelisk-fragment')
    clamp_floor(o, 0.0)
    smooth(o, 40)
    return g(o)


def ancient_pedestal(seed=810):
    p = pillar('pedestal', [(0, 0), (0.75, 0), (0.75, 0.2), (0.55, 0.35), (0.5, 1.1), (0.62, 1.2), (0.72, 1.3), (0.72, 1.4), (0, 1.4)], seed, segs=8, chips=0, disp=0.015)
    parts = [p]
    for i in range(8):
        a = math.tau * (i + 0.5) / 8
        parts.append(box(f'line{i}', (0.03, 0.03, 0.6), loc=(math.cos(a) * 0.52, math.sin(a) * 0.52, 0.7), rot=(0, 0, a), mat=M('ember-glow')))
    parts.append(torus('ring', 0.4, 0.02, loc=(0, 0, 1.41), mat=M('brass'), segs=24, ring=6))
    o = join(parts, 'ancient-pedestal')
    return g(o)


def unknown_machine(seed=811):
    body = plank_box('body', (1.8, 1.1, 1.2), (0, 0, 0.7), 'iron', 0.04, 1.0)
    parts = [body, plank_box('base', (2.0, 1.3, 0.2), (0, 0, 0.1), 'iron-rust', 0.02, 1.0),
             cylinder('boiler', 0.45, 0.45, 1.5, loc=(0, 0, 1.6), rot=(0, math.pi / 2, 0), segs=20, mat=M('bronze')),
             gear('gear', 0.5, 16, 0.1, loc=(1.0, 0, 1.0), rot=(0, 0, 0), hole=0.06), gear('gear2', 0.3, 10, 0.1, loc=(1.0, 0.0, 1.7), rot=(0, 0, 0), hole=0.04),
             cylinder('axle', 0.06, 0.06, 0.6, loc=(1.05, 0, 1.0), rot=(0, math.pi / 2, 0), segs=8, mat=M('iron')),
             sphere('core', 0.3, (-0.6, -0.56, 0.8), subdiv=3, mat=M('ember-glow'), scale=(1, 0.3, 1))]
    for i in range(3):
        parts.append(kit_extra.arc(f'pipe{i}', 0.35, 0.05, math.pi, loc=(-0.5 + i * 0.4, 0.5, 1.3), rot=(0, 0, math.pi / 2), mat=M('bronze'), segs=14, ring=8))
    for i in range(4):
        parts.append(cylinder(f'valve{i}', 0.07, 0.07, 0.1, loc=(-0.7 + i * 0.4, -0.58, 1.1), rot=(math.pi / 2, 0, 0), segs=8, mat=M('brass')))
    parts.append(torus('dial', 0.15, 0.02, loc=(0.6, -0.56, 0.5), rot=(math.pi / 2, 0, 0), mat=M('brass'), segs=16, ring=6))
    o = join(parts, 'unknown-machine')
    smooth(o, 40)
    return g(o)


def mechanical_parts(seed=812):
    rng = rand(seed)
    parts = [gear('g1', 0.5, 18, 0.1, loc=(0, 0, 0.05), rot=(0, math.pi / 2, 0), hole=0.06), gear('g2', 0.32, 12, 0.08, loc=(0.6, 0.4, 0.04), rot=(0, math.pi / 2, 0.3), hole=0.04),
             gear('g3', 0.22, 9, 0.08, loc=(-0.4, 0.5, 0.3), rot=(0.4, 0.9, 0.2), hole=0.03), gear('g4', 0.4, 14, 0.1, loc=(-0.5, -0.5, 0.05), rot=(0, math.pi / 2, 1.0), hole=0.05),
             cylinder('axle', 0.04, 0.04, 1.1, loc=(0.2, -0.3, 0.05), rot=(0, math.pi / 2, 0.6), segs=8, mat=M('iron')),
             cylinder('pipe', 0.07, 0.07, 0.8, loc=(0.5, -0.5, 0.08), rot=(0, math.pi / 2, -0.4), segs=10, mat=M('bronze'))]
    for i in range(8):
        parts.append(cylinder(f'bolt{i}', 0.03, 0.03, 0.06, loc=(rng.uniform(-0.8, 0.8), rng.uniform(-0.7, 0.7), 0.03), segs=6, mat=M('iron')))
    o = join(parts, 'mechanical-parts')
    clamp_floor(o, 0.0)
    smooth(o, 40)
    return g(o)


def god_king_transport(seed=813):
    hull = sphere('hull', 1.0, (0, 0, 1.3), subdiv=4, mat=M('iron-rust'), scale=(2.6, 1.2, 1.0))
    displace(hull, 0.04, 0.3, seed=seed)
    apply_mods(hull)
    box_uv(hull, 1.0)
    parts = [hull]
    for x in (-1.3, 0, 1.3):
        parts.append(torus(f'rib{x}', 1.15 - abs(x) * 0.25, 0.06, loc=(x, 0, 1.3), rot=(0, math.pi / 2, 0), mat=M('bronze'), segs=28, ring=8))
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(box(f'skid{sx}{sy}', (1.2, 0.2, 0.2), loc=(sx * 0.8, sy * 0.9, 0.1), mat=M('iron')))
            parts.append(box(f'leg{sx}{sy}', (0.15, 0.15, 0.7), loc=(sx * 0.8, sy * 0.8, 0.5), rot=(sy * 0.3, 0, 0), mat=M('iron')))
    parts.append(sphere('dome', 0.5, (0.6, 0, 2.2), subdiv=3, mat=M('glass'), scale=(1, 1, 0.6)))
    for i in range(6):
        a = math.tau * i / 6
        parts.append(crystal(f'c{i}', 0.5, 0.09, (-1.2 + math.cos(a) * 0.3, math.sin(a) * 0.7, 1.9), (math.cos(a) * 0.3, math.sin(a) * 0.5, 1), 'ember-glow', seed + i))
    parts.append(cylinder('stack', 0.18, 0.14, 0.9, loc=(-1.8, 0, 2.3), rot=(0, -0.3, 0), segs=12, mat=M('bronze')))
    o = join(parts, 'god-king-transport')
    smooth(o, 40)
    return g(o)


def ancient_hull_fragment(seed=814):
    prof = [(math.cos(a) * 2.2, math.sin(a) * 2.2) for a in [math.pi * 0.15 + math.pi * 0.7 * i / 16 for i in range(17)]]
    prof += [(math.cos(a) * 2.1, math.sin(a) * 2.1) for a in [math.pi * 0.85 - math.pi * 0.7 * i / 16 for i in range(17)]]
    plate = prism('plate', prof, 2.6, loc=(0, 0, -1.3), mat=M('iron-rust'))
    box_uv(plate, 1.0)
    parts = [plate]
    for x in (-1.0, 0, 1.0):
        rib = prism(f'rib{x}', [(math.cos(a) * 2.1, math.sin(a) * 2.1) for a in [math.pi * 0.18 + math.pi * 0.64 * i / 12 for i in range(13)]] + [(math.cos(a) * 1.95, math.sin(a) * 1.95) for a in [math.pi * 0.82 - math.pi * 0.64 * i / 12 for i in range(13)]], 0.15, loc=(x, 0, -1.3), mat=M('bronze'))
        parts.append(rib)
    o = join(parts, 'ancient-hull-fragment')
    o.rotation_euler = (0.3, 0.2, 0.4)
    apply_transform(o)
    clamp_floor(o, 0.0)
    smooth(o, 40)
    return g(o)


def lift_mechanism_part():
    parts = [gear('big', 0.7, 20, 0.14, loc=(0, 0, 0.75), rot=(0, 0, 0), hole=0.08), cylinder('axle', 0.08, 0.08, 1.4, loc=(0, 0, 0.75), rot=(math.pi / 2, 0, 0), segs=10, mat=M('iron')),
             cylinder('drum', 0.3, 0.3, 0.7, loc=(0, 0.6, 0.75), rot=(math.pi / 2, 0, 0), segs=16, mat=M('planks-dark'))]
    box_uv(parts[-1], 0.6)
    for sy in (-1, 1):
        parts.append(box(f'bearing{sy}', (0.3, 0.1, 0.5), loc=(0, sy * 0.75, 0.3), mat=M('iron-rust')))
        parts.append(box(f'foot{sy}', (0.6, 0.14, 0.1), loc=(0, sy * 0.75, 0.05), mat=M('iron-rust')))
    parts.append(chain('chain', (0, 0.6, 1.0), (0.9, 0.6, 0.05), 9, link=0.06, sag=-0.1))
    o = join(parts, 'lift-mechanism-part')
    smooth(o, 40)
    return g(o)


def light_crystal():
    stand = lathe('stand', [(0, 0), (0.4, 0), (0.42, 0.05), (0.15, 0.1), (0.12, 0.5), (0.25, 0.55), (0.25, 0.6), (0, 0.6)], segs=12, mat=M('iron'))
    parts = [stand, torus('ring', 0.22, 0.02, loc=(0, 0, 0.62), mat=M('brass'), segs=18, ring=6)]
    spots = [(0, 0, 1.0, 0.16), (0.12, 0.08, 0.6, 0.09), (-0.1, 0.1, 0.5, 0.08), (0.05, -0.14, 0.45, 0.07)]
    for i, (x, y, h, r) in enumerate(spots):
        parts.append(crystal(f'c{i}', h, r, (x, y, 0.55), (x * 2, y * 2, 1), 'light-crystal', 815 + i))
    o = join(parts, 'light-crystal')
    return g(o)


def ritual_vessel():
    v = vessel('vessel', URN, (0, 0, 0), 'masonry-crypt', 1.1, segs=24)
    for z in (0.3, 0.55, 0.8):
        boolean(v, torus(f'g{z}', 0.37 * 1.1 if z == 0.55 else 0.33 * 1.1, 0.02, loc=(0, 0, z), segs=24, ring=6))
    for i in range(6):
        a = math.tau * i / 6
        boolean(v, box(f'mark{i}', (0.08, 0.1, 0.18), loc=(math.cos(a) * 0.39, math.sin(a) * 0.39, 0.55), rot=(0, 0, a)))
    box_uv(v, 0.8)
    lid = lathe('lid', [(0, 1.12), (0.25, 1.12), (0.22, 1.2), (0.08, 1.25), (0.08, 1.32), (0, 1.32)], segs=24, mat=M('bronze'))
    o = join([v, lid], 'ritual-vessel')
    smooth(o, 45)
    return g(o)


if __name__ == '__main__':
    run('ember', [
        ('refined-ember-shard', refined_ember_shard), ('raw-ember-deposit', raw_ember_deposit), ('ember-container', ember_container), ('ember-lantern', ember_lantern),
        ('ember-extraction-altar', ember_extraction_altar), ('containment-box', containment_box), ('ancient-reliquary', ancient_reliquary), ('cult-symbol-stone', cult_symbol_stone),
        ('ritual-totem', ritual_totem), ('ember-transformed-statue', ember_transformed_statue), ('cracked-ceremonial-stone', cracked_ceremonial_stone),
        ('obelisk-fragment', obelisk_fragment), ('ancient-pedestal', ancient_pedestal), ('unknown-machine', unknown_machine), ('mechanical-parts', mechanical_parts),
        ('god-king-transport', god_king_transport), ('ancient-hull-fragment', ancient_hull_fragment), ('lift-mechanism-part', lift_mechanism_part),
        ('light-crystal', light_crystal), ('ritual-vessel', ritual_vessel),
    ])
