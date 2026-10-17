"""Wave 2, group 2: general objects and treasure (content/models/treasure)."""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kitlib import *  # noqa
import kit_props


def g(o):
    ground(o)
    return o


def plain_chest():
    return g(chest('plain-chest', wood='planks-worn', band=None))


def ornate_chest():
    return g(chest('ornate-chest', L=1.4, D=0.85, H=0.65, wood='planks-dark', band='brass', bands=4, studs=True))


def broken_chest():
    o = chest('broken-chest', wood='planks-worn', band='iron-rust', open_lid=True, broken=True)
    rng = rand(201)
    parts = [o]
    for i in range(3):
        parts.append(plank_box(f'plank{i}', (rng.uniform(0.3, 0.6), 0.14, 0.03), (rng.uniform(0.5, 1.0), rng.uniform(-0.6, -0.2), 0.02), 'planks-worn', 0.005, 0.8, rot=(0, 0, rng.uniform(0, 3))))
    return g(join(parts, 'broken-chest'))


def stone_ark(seed=202):
    body = stone_block('body', (1.6, 0.95, 0.85), (0, 0, 0.425), seed, 'masonry-crypt', bev=0.04)
    lid = stone_block('lid', (1.72, 1.07, 0.2), (0, 0, 0.95), seed + 1, 'masonry-crypt', bev=0.04)
    for z in (0.25, 0.6):
        boolean(body, box(f'g{z}', (1.7, 0.05, 0.04), loc=(0, -0.475, z)))
    o = join([body, lid], 'stone-ark')
    smooth(o, 40)
    return g(o)


def metal_safe():
    iron = 'iron'
    parts = [plank_box('body', (0.9, 0.85, 1.1), (0, 0, 0.55), iron, 0.03, 1.0),
             plank_box('door', (0.72, 0.06, 0.9), (0, -0.45, 0.55), 'iron-rust', 0.02, 1.0),
             cylinder('dial', 0.09, 0.09, 0.05, loc=(0.12, -0.5, 0.65), rot=(math.pi / 2, 0, 0), segs=16, mat=M('brass')),
             box('handle', (0.05, 0.04, 0.3), loc=(-0.2, -0.51, 0.5), mat=M('brass'))]
    for sy in (-1, 1):
        for z in (0.25, 0.85):
            parts.append(cylinder(f'hinge{sy}{z}', 0.035, 0.035, 0.16, loc=(0.41, -0.46, z), segs=8, mat=M(iron)))
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(box(f'foot{sx}{sy}', (0.1, 0.1, 0.08), loc=(sx * 0.38, sy * 0.36, 0.04), mat=M(iron)))
    o = join(parts, 'metal-safe')
    smooth(o, 40)
    return g(o)


def jewel_box():
    o = chest('jewel-box', L=0.42, D=0.3, H=0.17, R=0.11, wood='planks-dark', band='brass', bands=2)
    return g(o)


def coin_pile(seed=203):
    rng = rand(seed)
    parts = []
    for i in range(90):
        x, y = rng.gauss(0, 0.22), rng.gauss(0, 0.2)
        d = math.hypot(x, y)
        z = max(0.0, 0.25 - d * 0.6) * rng.uniform(0.3, 1.0)
        c = cylinder(f'coin{i}', 0.045, 0.045, 0.012, loc=(x, y, z + 0.006), rot=(rng.uniform(-0.4, 0.4), rng.uniform(-0.4, 0.4), rng.uniform(0, 3)), segs=10, mat=M('gold'))
        parts.append(c)
    o = join(parts, 'coin-pile')
    clamp_floor(o, 0.0)
    flat(o)
    return g(o)


def coin_purse(seed=204):
    rng = rand(seed)
    s = sack('purse', 0.2, (0, 0, 0), seed, mat='leather')
    parts = [s]
    for i in range(9):
        parts.append(cylinder(f'coin{i}', 0.04, 0.04, 0.01, loc=(0.25 + rng.uniform(0, 0.3), rng.uniform(-0.2, 0.2), 0.006 + (0.012 if i < 3 else 0)), rot=(0, 0, rng.uniform(0, 3)), segs=10, mat=M('gold')))
    o = join(parts, 'coin-purse')
    smooth(o, 60)
    return g(o)


def leather_satchel():
    body = plank_box('body', (0.55, 0.22, 0.4), (0, 0, 0.2), 'leather', 0.05, 1.0)
    flap = plank_box('flap', (0.56, 0.24, 0.22), (0, -0.01, 0.31), 'leather', 0.04, 1.0)
    strap = torus('strap', 0.42, 0.02, loc=(0, 0, 0.3), rot=(0, math.pi / 2, 0), mat=M('leather'), segs=28, ring=6)
    strap.scale = (1.0, 0.5, 1.0)
    apply_transform(strap)
    buckle = box('buckle', (0.06, 0.04, 0.08), loc=(0, -0.13, 0.18), mat=M('brass'))
    o = join([body, flap, strap, buckle], 'leather-satchel')
    smooth(o, 40)
    return g(o)


def broken_crate(seed=205):
    rng = rand(seed)
    c = kit_props.crate()
    cut = box('cut', (0.7, 0.7, 0.7), loc=(0.55, -0.45, 0.85), rot=(0.4, 0.3, 0.5))
    boolean(c, cut)
    parts = [c]
    for i in range(4):
        parts.append(plank_box(f'plank{i}', (rng.uniform(0.3, 0.7), 0.12, 0.03), (rng.uniform(0.6, 1.2), rng.uniform(-0.8, 0.2), 0.02), 'planks-worn', 0.005, 0.8, rot=(0, 0, rng.uniform(0, 3))))
    o = join(parts, 'broken-crate')
    return g(o)


def open_barrel():
    return g(barrel('open-barrel', open_top=True))


def broken_barrel(seed=206):
    rng = rand(seed)
    b = barrel('broken-barrel', broken=True)
    parts = [b]
    for i in range(5):
        st = prism(f'stave{i}', half_circle(0.07, 6, 0.0), rng.uniform(0.7, 1.2), loc=(rng.uniform(0.5, 1.1), rng.uniform(-0.7, 0.7), 0.035), rot=(0, 0, rng.uniform(0, 3)), mat=M('planks-dark'))
        box_uv(st, 0.8)
        parts.append(st)
    return g(join(parts, 'broken-barrel'))


def wicker_baskets():
    a = vessel('basketA', BASKET, (0, 0, 0), 'wicker', 1.0, segs=20)
    b = vessel('basketB', BASKET, (0.75, 0.2, 0), 'wicker', 0.7, segs=20)
    handle = kit_extra.arc('handle', 0.36, 0.025, math.pi, loc=(0, 0, 0.42), rot=(math.pi / 2, 0, 0), mat=M('wicker'), segs=18, ring=6)
    o = join([a, b, handle], 'wicker-baskets')
    box_uv(o, 0.35)
    smooth(o, 50)
    return g(o)


def ceramic_jars():
    parts = [vessel('jarA', JAR, (0, 0, 0), 'clay', 1.0), vessel('jarB', JAR, (0.7, 0.15, 0), 'clay', 0.75), vessel('jarC', JAR, (-0.35, 0.55, 0), 'clay', 0.55)]
    o = join(parts, 'ceramic-jars')
    smooth(o, 50)
    return g(o)


def coiled_rope():
    parts = []
    for i in range(6):
        parts.append(torus(f'coil{i}', 0.36 - i * 0.01, 0.03, loc=(0, 0, 0.03 + i * 0.055), rot=(0, 0, i * 0.3), mat=M('rope'), segs=32, ring=8))
    end = cylinder('end', 0.03, 0.03, 0.6, loc=(0.55, 0.1, 0.03), rot=(0, math.pi / 2, 0.4), segs=8, mat=M('rope'))
    parts.append(end)
    o = join(parts, 'coiled-rope')
    smooth(o, 50)
    return g(o)


def _lantern(loc, core=None):
    S, H = 0.3, 0.46
    x, y, z = loc
    parts = [box('base', (S, S, 0.04), loc=(x, y, z + 0.02), mat=M('iron')), box('top', (S * 0.9, S * 0.9, 0.04), loc=(x, y, z + H + 0.04), mat=M('iron')),
             box('glass', (S * 0.8, S * 0.8, H - 0.02), loc=(x, y, z + H * 0.5 + 0.03), mat=M('glass'))]
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(cylinder(f'bar{sx}{sy}', 0.016, 0.016, H, loc=(x + sx * S * 0.44, y + sy * S * 0.44, z + H * 0.5 + 0.03), segs=6, mat=M('iron')))
    parts.append(cylinder('cap', S * 0.4, 0.03, 0.1, loc=(x, y, z + H + 0.11), segs=4, mat=M('iron')))
    parts.append(torus('ring', 0.06, 0.012, loc=(x, y, z + H + 0.2), rot=(math.pi / 2, 0, 0), mat=M('iron'), segs=14, ring=6))
    if core:
        parts.append(crystal('core', 0.22, 0.06, (x, y, z + 0.1), mat=core))
    return join(parts, 'lantern')


def hanging_lantern():
    post = cylinder('post', 0.08, 0.07, 2.4, loc=(0, 0, 1.2), segs=10, mat=M('planks-worn'))
    box_uv(post, 0.7)
    arm = box('arm', (0.7, 0.06, 0.06), loc=(0.3, 0, 2.3), mat=M('iron'))
    brace = box('brace', (0.5, 0.04, 0.04), loc=(0.2, 0, 2.05), rot=(0, -0.8, 0), mat=M('iron'))
    ch = chain('chain', (0.6, 0, 2.27), (0.6, 0, 1.95), 4, link=0.05)
    lan = _lantern((0.6, 0, 1.25))
    o = join([post, arm, brace, ch, lan], 'hanging-lantern')
    smooth(o, 40)
    return g(o)


def unlit_torch():
    stand = lathe('stand', [(0, 0), (0.2, 0), (0.22, 0.03), (0.08, 0.06), (0.06, 0.5), (0.1, 0.52), (0, 0.52)], segs=12, mat=M('iron-rust'))
    ring = torus('ring', 0.07, 0.014, loc=(0, 0, 0.55), mat=M('iron-rust'), segs=16, ring=6)
    shaft = cylinder('shaft', 0.045, 0.035, 1.0, loc=(0, 0, 0.75), segs=10, mat=M('planks-worn'))
    box_uv(shaft, 0.6)
    head = cylinder('head', 0.085, 0.07, 0.26, loc=(0, 0, 1.33), segs=12, mat=M('burlap'))
    displace(head, 0.03, 0.08, seed=207)
    apply_mods(head)
    wrap = torus('wrap', 0.085, 0.012, loc=(0, 0, 1.25), mat=M('rope'), segs=16, ring=6)
    o = join([stand, ring, shaft, head, wrap], 'unlit-torch')
    smooth(o, 40)
    return g(o)


def weapon_rack():
    W, H = 1.8, 1.9
    parts = [plank_box('postL', (0.12, 0.12, H), (-W * 0.5, 0, H * 0.5), 'planks-dark', 0.01, 0.8), plank_box('postR', (0.12, 0.12, H), (W * 0.5, 0, H * 0.5), 'planks-dark', 0.01, 0.8),
             plank_box('rail', (W + 0.12, 0.1, 0.12), (0, 0, H - 0.3), 'planks-dark', 0.01, 1.0), plank_box('foot', (W + 0.12, 0.3, 0.1), (0, 0, 0.05), 'planks-dark', 0.01, 1.0),
             plank_box('footL', (0.14, 0.6, 0.1), (-W * 0.5, 0, 0.05), 'planks-dark', 0.01, 1.0), plank_box('footR', (0.14, 0.6, 0.1), (W * 0.5, 0, 0.05), 'planks-dark', 0.01, 1.0)]
    for i in range(4):
        x = -W * 0.36 + W * 0.72 * i / 3
        parts.append(cylinder(f'shaft{i}', 0.025, 0.02, 2.2, loc=(x, -0.1, 1.1), rot=(0.08, 0, 0), segs=8, mat=M('planks-worn')))
        parts.append(box(f'blade{i}', (0.08, 0.02, 0.4), loc=(x, -0.19, 2.35), mat=M('silver')))
    for i, x in enumerate((-W * 0.5 + 0.25, W * 0.5 - 0.25)):
        parts.append(box(f'sword{i}', (0.09, 0.015, 1.0), loc=(x, 0.12, 1.0), mat=M('silver')))
        parts.append(box(f'guard{i}', (0.28, 0.03, 0.04), loc=(x, 0.12, 1.5), mat=M('brass')))
        parts.append(cylinder(f'grip{i}', 0.025, 0.025, 0.22, loc=(x, 0.12, 1.63), segs=8, mat=M('leather')))
    o = join(parts, 'weapon-rack')
    smooth(o, 40)
    return g(o)


def shield_stand():
    W = 1.9
    parts = [plank_box('rail', (W, 0.1, 0.1), (0, 0, 0.85), 'planks-dark', 0.01, 1.0), plank_box('low', (W, 0.1, 0.08), (0, 0.1, 0.1), 'planks-dark', 0.01, 1.0)]
    for sx in (-1, 1):
        parts.append(plank_box(f'leg{sx}', (0.1, 0.1, 1.0), (sx * (W * 0.5 - 0.05), 0.05, 0.5), 'planks-dark', 0.01, 0.8))
        parts.append(plank_box(f'foot{sx}', (0.12, 0.5, 0.08), (sx * (W * 0.5 - 0.05), 0.0, 0.04), 'planks-dark', 0.01, 0.8))
    for i in range(3):
        x = -0.6 + 0.6 * i
        s = cylinder(f'shield{i}', 0.42, 0.4, 0.06, loc=(x, -0.14, 0.55), rot=(math.pi / 2 - 0.15, 0, 0), segs=24, mat=M('planks-dark'))
        box_uv(s, 0.9)
        parts.append(s)
        parts.append(torus(f'rim{i}', 0.41, 0.025, loc=(x, -0.14, 0.55), rot=(math.pi / 2 - 0.15, 0, 0), mat=M('iron'), segs=24, ring=6))
        parts.append(sphere(f'boss{i}', 0.09, (x, -0.19, 0.55), subdiv=2, mat=M('iron'), scale=(1, 0.6, 1)))
    o = join(parts, 'shield-stand')
    smooth(o, 40)
    return g(o)


def workbench():
    t = table('bench', 2.4, 1.0, 1.25, wood='planks-worn', legs='planks-dark', leg=0.16)
    parts = [t, plank_box('vice-base', (0.25, 0.2, 0.12), (-0.8, -0.45, 1.31), 'iron', 0.01, 1.0), plank_box('vice-jaw', (0.25, 0.08, 0.2), (-0.8, -0.56, 1.35), 'iron', 0.01, 1.0),
             cylinder('vice-screw', 0.02, 0.02, 0.3, loc=(-0.8, -0.72, 1.33), rot=(math.pi / 2, 0, 0), segs=8, mat=M('iron')),
             cylinder('hammer-h', 0.02, 0.02, 0.4, loc=(0.3, 0.1, 1.28), rot=(0, math.pi / 2, 0.3), segs=8, mat=M('planks-worn')),
             box('hammer-head', (0.12, 0.07, 0.07), loc=(0.5, 0.16, 1.3), rot=(0, 0, 0.3), mat=M('iron')),
             box('saw', (0.7, 0.02, 0.12), loc=(0.6, -0.3, 1.33), rot=(0, 0, -0.2), mat=M('silver')), box('saw-grip', (0.18, 0.04, 0.12), loc=(0.95, -0.37, 1.33), mat=M('planks-dark')),
             plank_box('shelf', (2.0, 0.6, 0.05), (0, 0, 0.45), 'planks-worn', 0.01, 1.2)]
    o = join(parts, 'workbench')
    smooth(o, 40)
    return g(o)


if __name__ == '__main__':
    run('treasure', [
        ('plain-chest', plain_chest), ('ornate-chest', ornate_chest), ('broken-chest', broken_chest), ('stone-ark', stone_ark), ('metal-safe', metal_safe),
        ('jewel-box', jewel_box), ('coin-pile', coin_pile), ('coin-purse', coin_purse), ('leather-satchel', leather_satchel), ('broken-crate', broken_crate),
        ('open-barrel', open_barrel), ('broken-barrel', broken_barrel), ('wicker-baskets', wicker_baskets), ('ceramic-jars', ceramic_jars), ('coiled-rope', coiled_rope),
        ('hanging-lantern', hanging_lantern), ('unlit-torch', unlit_torch), ('weapon-rack', weapon_rack), ('shield-stand', shield_stand), ('workbench', workbench),
    ])
