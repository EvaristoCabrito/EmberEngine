"""Wave 2, group 5: frozen Mordavia (content/models/frost)."""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kitlib import *  # noqa
import kit_props
import kit_city


def g(o):
    ground(o)
    return o


def snow_cap(o, up=0.55, seed=0, threshold=0.4):
    set_material_by_normal(o, M('snow'), up=up, noise_scale=0.4, seed=seed, threshold=threshold)
    box_uv(o, 1.2)
    return o


def snow_crown_tree():
    return g(tree('snow-crown-tree', 501, snow_canopy=True))


def frozen_stump():
    o = kit_forest.stump(seed=502)
    snow_cap(o, 0.6, 502)
    parts = [o]
    for i, (x, y) in enumerate(((0.55, 0.1), (-0.5, 0.2), (0.1, -0.55))):
        parts.append(stalagmite(f'ice{i}', 0.3, 0.05, (x, y, 0.95), 'ice', seed=i, up=False, segs=6))
    return g(join(parts, 'frozen-stump'))


def ice_cracked_rocks():
    o = rock_cluster('rocks', 503, n=6, rmax=0.55, mat='stone')
    set_material_where(o, M('ice'), lambda c, n: noise.noise(c * 4.0) > 0.35)
    snow_cap(o, 0.7, 503)
    return g(o)


def icicles():
    ledge = rock('ledge', (2.2, 1.2, 0.5), 504, 'stone', top='snow')
    ledge.data.transform(Matrix.Translation((0, 0, 2.0)))
    parts = [ledge]
    rng = rand(504)
    for i in range(9):
        parts.append(stalagmite(f'icicle{i}', rng.uniform(0.3, 0.9), rng.uniform(0.04, 0.1), (rng.uniform(-0.9, 0.9), rng.uniform(-0.4, 0.4), 2.05), 'ice', seed=i, up=False, segs=6))
    for sx in (-1, 1):
        parts.append(rock(f'pier{sx}', (0.7, 0.8, 2.1), 505 + sx, 'stone', loc=(sx * 0.8, 0, 0)))
    o = join(parts, 'icicles')
    smooth(o, 45)
    return g(o)


def ice_stalagmites():
    rng = rand(506)
    parts = [stalagmite(f'ice{i}', rng.uniform(0.6, 1.8), rng.uniform(0.12, 0.3), (rng.uniform(-0.7, 0.7), rng.uniform(-0.6, 0.6), 0), 'ice', seed=i, segs=7) for i in range(7)]
    parts.append(rock('base', (2.0, 1.8, 0.3), 507, 'snow', uv=1.3))
    o = join(parts, 'ice-stalagmites')
    smooth(o, 45)
    return g(o)


def snow_drift():
    s = sphere('drift', 1.0, (0, 0, 0.1), subdiv=4, mat=M('snow'), scale=(2.2, 1.5, 0.6))
    displace(s, 0.15, 0.7, seed=508)
    apply_mods(s)
    clamp_floor(s, 0.0)
    box_uv(s, 1.3)
    smooth(s, 60)
    return g(s)


def frozen_branches():
    return g(branch_pile('frozen-branches', 509, n=12, snow=True))


def snowy_dry_bush():
    import kit_nature
    o = kit_nature.dry_bush(seed=510)
    rng = rand(510)
    parts = [o]
    for i in range(10):
        a = rng.uniform(0, math.tau)
        d = rng.uniform(0.2, 0.9)
        s = sphere(f'snow{i}', rng.uniform(0.1, 0.2), (math.cos(a) * d, math.sin(a) * d, rng.uniform(0.5, 1.1)), subdiv=2, mat=M('snow'), scale=(1.3, 1.1, 0.5))
        box_uv(s, 1.0)
        parts.append(s)
    return g(join(parts, 'snowy-dry-bush'))


def buried_fence():
    f = kit_extra.fence_segment()
    parts = [f]
    rng = rand(511)
    for i in range(5):
        s = sphere(f'mound{i}', rng.uniform(0.5, 0.8), (-1.3 + i * 0.65, rng.uniform(-0.1, 0.2), 0.0), subdiv=3, mat=M('snow'), scale=(1.4, 1.1, 0.55))
        displace(s, 0.08, 0.5, seed=511 + i)
        apply_mods(s)
        clamp_floor(s, 0.0)
        box_uv(s, 1.3)
        parts.append(s)
    o = join(parts, 'buried-fence')
    snow_cap(o, 0.75, 511, 0.3)
    return g(o)


def frozen_barrel():
    b = barrel('frozen-barrel', snow=True)
    ring = torus('ice-ring', 0.52, 0.04, loc=(0, 0, 0.3), mat=M('ice'), segs=24, ring=6)
    return g(join([b, ring], 'frozen-barrel'))


def snow_abandoned_cart():
    c = kit_props.handcart()
    c.rotation_euler = (0.12, 0, 0.2)
    apply_transform(c)
    slab = sphere('slab', 1.0, (0, 0, 0.9), subdiv=3, mat=M('snow'), scale=(0.9, 0.5, 0.2))
    displace(slab, 0.1, 0.5, seed=512)
    apply_mods(slab)
    box_uv(slab, 1.2)
    o = join([c, slab], 'snow-abandoned-cart')
    snow_cap(o, 0.8, 512, 0.3)
    clamp_floor(o, 0.0)
    return g(o)


def icy_post():
    post = cylinder('post', 0.09, 0.08, 2.8, loc=(0, 0, 1.4), segs=10, mat=M('planks-worn'))
    box_uv(post, 0.7)
    cap = cylinder('cap', 0.14, 0.1, 0.12, loc=(0, 0, 2.85), segs=10, mat=M('snow'))
    box_uv(cap, 1.0)
    parts = [post, cap, box('arm', (0.6, 0.05, 0.05), loc=(0.25, 0, 2.6), mat=M('iron'))]
    rng = rand(513)
    for i in range(6):
        parts.append(stalagmite(f'drip{i}', rng.uniform(0.15, 0.4), 0.03, (rng.uniform(-0.1, 0.5), rng.uniform(-0.04, 0.04), 2.58), 'ice', seed=i, up=False, segs=5))
    for i in range(5):
        parts.append(stalagmite(f'glaze{i}', rng.uniform(0.2, 0.5), 0.04, (rng.uniform(-0.08, 0.08), rng.uniform(-0.08, 0.08), rng.uniform(1.2, 2.4)), 'ice', seed=10 + i, up=False, segs=5))
    o = join(parts, 'icy-post')
    smooth(o, 40)
    return g(o)


def frozen_well():
    w = kit_city.well(seed=514)
    snow_cap(w, 0.8, 514, 0.3)
    ice = cylinder('ice', 0.7, 0.7, 0.05, loc=(0, 0, 0.75), segs=24, mat=M('ice'))
    return g(join([w, ice], 'frozen-well'))


def snowy_grave_marker():
    o = kit_extra.grave_marker(seed=515)
    snow_cap(o, 0.6, 515, 0.3)
    return g(o)


def frozen_bridge():
    L = 4.6
    deck = planks_row('deck', 14, 1.6, 0.28, 0.06, (0, 0, 0.5), 'planks-worn', axis='y', seed=516)
    parts = [deck]
    for sy in (-1, 1):
        parts.append(plank_box(f'beam{sy}', (L, 0.14, 0.18), (0, sy * 0.72, 0.38), 'planks-dark', 0.01, 1.0))
        for x in (-2.0, -0.7, 0.7, 2.0):
            parts.append(plank_box(f'post{sy}{x}', (0.1, 0.1, 1.0), (x, sy * 0.82, 0.95), 'planks-worn', 0.01, 0.8))
            parts.append(plank_box(f'pile{sy}{x}', (0.16, 0.16, 0.45), (x, sy * 0.6, 0.22), 'planks-dark', 0.01, 0.8))
        parts.append(plank_box(f'rail{sy}', (L, 0.08, 0.08), (0, sy * 0.82, 1.4), 'planks-worn', 0.01, 1.0))
    rng = rand(516)
    for i in range(5):
        s = sphere(f'snow{i}', rng.uniform(0.3, 0.6), (rng.uniform(-2, 2), rng.uniform(-0.5, 0.5), 0.5), subdiv=3, mat=M('snow'), scale=(1.4, 1.0, 0.3))
        displace(s, 0.06, 0.4, seed=516 + i)
        apply_mods(s)
        box_uv(s, 1.2)
        parts.append(s)
    o = join(parts, 'frozen-bridge')
    snow_cap(o, 0.85, 516, 0.25)
    return g(o)


def abandoned_camp():
    tent = prism('tent', [(-1.0, 0.0), (0.0, 0.9), (1.0, 0.0), (0.95, 0.0), (0.0, 0.84), (-0.95, 0.0)], 2.2, loc=(-1.0, 0, 0), rot=(0, 0, 0.3), mat=M('canvas'))
    subdivide(tent, 2, simple=True)
    displace(tent, 0.12, 0.5, seed=517)
    apply_mods(tent)
    box_uv(tent, 1.2)
    parts = [tent, cylinder('ridge', 0.04, 0.03, 2.4, loc=(-1.0, 0, 0.9), rot=(0, math.pi / 2, 0.3), segs=6, mat=M('planks-worn'))]
    rng = rand(517)
    for i in range(8):
        a = math.tau * i / 8
        parts.append(rock(f'ringstone{i}', (0.3, 0.25, 0.2), 517 + i, 'stone', loc=(1.1 + math.cos(a) * 0.5, math.sin(a) * 0.5, 0)))
    for i in range(4):
        parts.append(cylinder(f'charred{i}', 0.06, 0.05, 0.5, loc=(1.1 + rng.uniform(-0.2, 0.2), rng.uniform(-0.2, 0.2), 0.06), rot=(0, math.pi / 2, rng.uniform(0, 3)), segs=6, mat=M('coal')))
    pot = lathe('pot', [(0, 0), (0.18, 0), (0.2, 0.22), (0.17, 0.22), (0, 0.2)], segs=14, loc=(1.6, 0.6, 0), mat=M('iron-rust'))
    parts.append(pot)
    parts.append(sack('sack', 0.22, (0.3, -0.9, 0), 517))
    for i in range(3):
        s = sphere(f'snow{i}', rng.uniform(0.5, 0.9), (rng.uniform(-1.5, 1.5), rng.uniform(-1.0, 1.0), 0.0), subdiv=3, mat=M('snow'), scale=(1.5, 1.2, 0.4))
        displace(s, 0.08, 0.5, seed=520 + i)
        apply_mods(s)
        clamp_floor(s, 0.0)
        box_uv(s, 1.3)
        parts.append(s)
    o = join(parts, 'abandoned-camp')
    snow_cap(o, 0.8, 517, 0.3)
    return g(o)


if __name__ == '__main__':
    run('frost', [
        ('snow-crown-tree', snow_crown_tree), ('frozen-stump', frozen_stump), ('ice-cracked-rocks', ice_cracked_rocks), ('icicles', icicles),
        ('ice-stalagmites', ice_stalagmites), ('snow-drift', snow_drift), ('frozen-branches', frozen_branches), ('snowy-dry-bush', snowy_dry_bush),
        ('buried-fence', buried_fence), ('frozen-barrel', frozen_barrel), ('snow-abandoned-cart', snow_abandoned_cart), ('icy-post', icy_post),
        ('frozen-well', frozen_well), ('snowy-grave-marker', snowy_grave_marker), ('frozen-bridge', frozen_bridge), ('abandoned-camp', abandoned_camp),
    ])
