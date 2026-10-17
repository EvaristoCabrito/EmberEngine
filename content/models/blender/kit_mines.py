"""Wave 2, group 7: mines and the Profundezas Famintas (content/models/mines)."""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kitlib import *  # noqa
import kit_props


def g(o):
    ground(o)
    return o


def tunnel_entrance(seed=701):
    W, H, T = 4.0, 4.0, 0.36
    parts = []
    for sx in (-1, 1):
        parts.append(plank_box(f'post{sx}', (T, T, H - T), (sx * (W * 0.5 - T * 0.5), 0, (H - T) * 0.5), 'planks-dark', 0.015, 1.0))
        parts.append(plank_box(f'brace{sx}', (1.3, 0.16, 0.18), (sx * (W * 0.5 - T - 0.35), 0, H - T - 0.45), 'planks-worn', 0.01, 1.0, rot=(0, sx * math.radians(42), 0)))
    parts.append(plank_box('cap', (W + 0.5, T, T), (0, 0, H - T * 0.5), 'planks-dark', 0.015, 1.0))
    for i in range(9):
        parts.append(plank_box(f'lag{i}', (0.42, 1.2, 0.07), (-W * 0.5 + 0.25 + i * (W - 0.5) / 8, 0.3, H + 0.035), 'planks-worn', 0.01, 1.0, (0.2 * i, 0)))
    for sx in (-1, 1):
        parts.append(rock(f'rock{sx}', (1.6, 1.4, 2.6), seed + sx, 'stone', loc=(sx * (W * 0.5 + 0.7), 0.3, 0)))
        parts.append(rock(f'rock2{sx}', (1.2, 1.0, 1.4), seed + 3 + sx, 'stone', loc=(sx * (W * 0.5 + 0.9), -0.6, 0)))
    parts.append(rock('roof', (W + 2.5, 1.6, 1.3), seed + 7, 'stone', loc=(0, 0.3, H + 0.1)))
    parts.append(box('dark', (W - 0.8, 0.05, H - 0.6), loc=(0, 0.7, (H - 0.6) * 0.5), mat=M('passage-dark')))
    o = join(parts, 'tunnel-entrance')
    smooth(o, 40)
    return g(o)


def mine_rails():
    L = 4.4
    parts = []
    for sy in (-0.45, 0.45):
        parts.append(box(f'rail{sy}', (L, 0.06, 0.1), loc=(0, sy, 0.12), mat=M('iron-rust')))
        parts.append(box(f'foot{sy}', (L, 0.1, 0.02), loc=(0, sy, 0.08), mat=M('iron-rust')))
    for i in range(8):
        parts.append(plank_box(f'sleeper{i}', (0.22, 1.3, 0.08), (-L * 0.5 + 0.3 + i * (L - 0.6) / 7, 0, 0.04), 'planks-dark', 0.01, 0.8, (0.3 * i, 0)))
    o = join(parts, 'mine-rails')
    smooth(o, 40)
    return g(o)


def ore_cart(seed=702):
    body = plank_box('body', (1.5, 0.95, 0.8), (0, 0, 0.75), 'iron-rust', 0.02, 1.0)
    boolean(body, box('hollow', (1.36, 0.81, 0.7), loc=(0, 0, 0.85)))
    parts = [body]
    for x in (-0.55, 0, 0.55):
        parts.append(box(f'rib{x}', (0.06, 1.0, 0.82), loc=(x, 0, 0.75), mat=M('iron')))
    parts.append(box('frame', (1.6, 1.0, 0.1), loc=(0, 0, 0.32), mat=M('iron')))
    for sx in (-1, 1):
        parts.append(cylinder(f'axle{sx}', 0.03, 0.03, 1.1, loc=(sx * 0.5, 0, 0.2), rot=(math.pi / 2, 0, 0), segs=8, mat=M('iron')))
        for sy in (-1, 1):
            parts.append(cylinder(f'wheel{sx}{sy}', 0.2, 0.2, 0.08, loc=(sx * 0.5, sy * 0.52, 0.2), rot=(math.pi / 2, 0, 0), segs=16, mat=M('iron')))
    rng = rand(seed)
    for i in range(9):
        parts.append(rock(f'ore{i}', (0.3, 0.25, 0.25), seed + i, 'ore', loc=(rng.uniform(-0.5, 0.5), rng.uniform(-0.25, 0.25), 0.95 + rng.uniform(0, 0.2)), subdiv=2, uv=0.5))
    o = join(parts, 'ore-cart')
    smooth(o, 40)
    return g(o)


def pickaxe_on_rock():
    r = rock('rock', (1.3, 1.1, 0.8), 703, 'stone')
    handle = cylinder('handle', 0.03, 0.025, 1.1, loc=(0.4, -0.45, 0.6), segs=8, mat=M('planks-worn'))
    aim(handle, (0.3, -0.5, 1))
    apply_transform(handle)
    head = box('head', (0.6, 0.06, 0.08), loc=(0.6, -0.78, 1.1), rot=(0, 0.3, -0.5), mat=M('iron'))
    tip = cylinder('tip', 0.04, 0.0, 0.3, loc=(0.88, -0.65, 1.0), rot=(0, 1.3, -0.5), segs=6, mat=M('iron'))
    o = join([r, handle, head, tip], 'pickaxe-on-rock')
    smooth(o, 45)
    return g(o)


def mining_shovel():
    crate = kit_props.crate()
    handle = cylinder('handle', 0.025, 0.025, 1.3, loc=(0.75, -0.2, 0.75), segs=8, mat=M('planks-worn'))
    aim(handle, (0.25, 0, 1))
    apply_transform(handle)
    blade = box('blade', (0.26, 0.02, 0.32), loc=(0.58, -0.2, 0.15), rot=(0, 0.25, 0), mat=M('iron'))
    grip = box('grip', (0.14, 0.03, 0.03), loc=(0.92, -0.2, 1.42), mat=M('planks-dark'))
    o = join([crate, handle, blade, grip], 'mining-shovel')
    smooth(o, 40)
    return g(o)


def powder_barrel():
    b = barrel('powder-barrel', H=0.95, R=0.36, hoops=2, wood='planks-worn', hoop='iron')
    lid = cylinder('lid', 0.3, 0.3, 0.03, loc=(0, 0, 0.97), segs=20, mat=M('planks-dark'))
    fuse = chain('fuse', (0.1, 0, 1.0), (0.6, 0.4, 0.05), 7, link=0.03, mat='rope', sag=-0.2)
    o = join([b, lid, fuse], 'powder-barrel')
    smooth(o, 40)
    return g(o)


def ore_crates(seed=704):
    rng = rand(seed)
    c = plank_box('crate', (1.1, 0.8, 0.6), (0, 0, 0.3), 'planks-worn', 0.015, 1.1)
    boolean(c, box('hollow', (0.98, 0.68, 0.55), loc=(0, 0, 0.35)))
    parts = [c, plank_box('crate2', (0.8, 0.6, 0.5), (1.0, 0.3, 0.25), 'planks-dark', 0.015, 1.0)]
    for i in range(12):
        parts.append(rock(f'ore{i}', (0.25, 0.22, 0.2), seed + i, 'ore', loc=(rng.uniform(-0.4, 0.4), rng.uniform(-0.25, 0.25), 0.2 + rng.uniform(0, 0.35)), subdiv=2, uv=0.5))
    o = join(parts, 'ore-crates')
    smooth(o, 40)
    return g(o)


def raw_ore_pile():
    o = rock_cluster('raw-ore-pile', 705, n=14, rmax=0.3, mat='ore', spread=0.5)
    return g(o)


def mineral_crystals():
    return g(crystal_cluster('mineral-crystals', 706, mat='pale-crystal', base=(1.5, 1.2, 0.5)))


def ember_vein_wall(seed=707):
    wall = rock('wall', (2.6, 0.9, 2.6), seed, 'stone', subdiv=4, rough=0.2)
    parts = [wall]
    rng = rand(seed)
    for i in range(12):
        t = i / 11
        x = -1.0 + 2.0 * t
        z = 0.6 + 1.4 * math.sin(t * 2.5) * 0.5 + t * 0.8
        parts.append(crystal(f'vein{i}', rng.uniform(0.15, 0.35), rng.uniform(0.04, 0.08), (x, -0.42, z), (rng.uniform(-0.3, 0.3), -1, rng.uniform(-0.3, 0.3)), 'ember-glow', seed + i))
    o = join(parts, 'ember-vein-wall')
    return g(o)


def large_crystal_formation():
    spots = [(0, 0, 2.6, 0.4), (0.7, 0.3, 1.9, 0.3), (-0.6, 0.4, 1.6, 0.28), (0.2, -0.7, 1.4, 0.25), (-0.8, -0.3, 1.2, 0.22), (0.9, -0.4, 1.0, 0.18), (-0.2, 0.9, 0.9, 0.16)]
    o = crystal_cluster('large-crystal-formation', 708, mat='amethyst', base=(2.6, 2.2, 0.8), spots=spots)
    return g(o)


def natural_stone_column():
    lo = stalagmite('lo', 2.0, 0.55, (0, 0, 0), 'stone', seed=709)
    hi = stalagmite('hi', 2.0, 0.5, (0, 0, 3.6), 'stone', seed=710, up=False)
    mid = cylinder('mid', 0.26, 0.24, 0.6, loc=(0, 0, 1.9), segs=12, mat=M('stone'))
    box_uv(mid, 1.0)
    o = join([lo, hi, mid], 'natural-stone-column')
    smooth(o, 45)
    return g(o)


def stalactites(seed=711):
    rng = rand(seed)
    ceiling = rock('ceiling', (2.4, 2.0, 0.6), seed, 'stone')
    ceiling.data.transform(Matrix.Translation((0, 0, 3.0)))
    parts = [ceiling]
    for i in range(8):
        parts.append(stalagmite(f'st{i}', rng.uniform(0.5, 1.4), rng.uniform(0.1, 0.22), (rng.uniform(-0.9, 0.9), rng.uniform(-0.7, 0.7), 3.05), 'stone', seed=seed + i, up=False))
    for sx in (-1, 1):
        parts.append(rock(f'pier{sx}', (0.8, 0.9, 3.1), seed + 20 + sx, 'stone', loc=(sx * 0.9, 0, 0)))
    o = join(parts, 'stalactites')
    smooth(o, 45)
    return g(o)


def stalagmites(seed=712):
    rng = rand(seed)
    parts = [stalagmite(f'st{i}', rng.uniform(0.6, 1.9), rng.uniform(0.15, 0.35), (rng.uniform(-0.7, 0.7), rng.uniform(-0.6, 0.6), 0), 'stone', seed=seed + i) for i in range(6)]
    parts.append(rock('base', (2.2, 1.9, 0.3), seed + 9, 'stone'))
    o = join(parts, 'stalagmites')
    smooth(o, 45)
    return g(o)


def rock_arch_formation(seed=713):
    a = kit_extra.arc('arch', 2.0, 0.55, math.pi, rot=(math.pi / 2, 0, 0), mat=M('stone'), segs=22, ring=12)
    subdivide(a, 1, simple=True)
    displace(a, 0.25, 0.8, seed=seed)
    displace(a, 0.06, 0.2, seed=seed + 1)
    apply_mods(a)
    box_uv(a, 1.4)
    parts = [a, rock('footL', (1.3, 1.2, 0.8), seed + 2, 'stone', loc=(-2.0, 0, 0)), rock('footR', (1.3, 1.2, 0.8), seed + 3, 'stone', loc=(2.0, 0, 0))]
    o = join(parts, 'rock-arch-formation')
    clamp_floor(o, 0.0)
    smooth(o, 50)
    return g(o)


def narrow_wooden_bridge():
    L = 4.2
    deck = planks_row('deck', 12, 0.9, 0.28, 0.05, (0, 0, 0.25), 'planks-worn', axis='y', seed=714)
    parts = [deck]
    for sy in (-1, 1):
        parts.append(plank_box(f'beam{sy}', (L, 0.1, 0.14), (0, sy * 0.38, 0.15), 'planks-dark', 0.01, 1.0))
        for x in (-1.9, 0, 1.9):
            parts.append(cylinder(f'post{sy}{x}', 0.04, 0.035, 0.9, loc=(x, sy * 0.46, 0.65), segs=8, mat=M('planks-worn')))
        parts.append(cylinder(f'rope{sy}', 0.018, 0.018, L, loc=(0, sy * 0.46, 1.05), rot=(0, math.pi / 2, 0), segs=6, mat=M('rope')))
    o = join(parts, 'narrow-wooden-bridge')
    smooth(o, 40)
    return g(o)


def mine_walkway():
    L = 4.0
    deck = planks_row('deck', 11, 1.3, 0.3, 0.06, (0, 0, 0.8), 'planks-worn', axis='y', seed=715)
    parts = [deck]
    for sy in (-1, 1):
        parts.append(plank_box(f'joist{sy}', (L, 0.12, 0.18), (0, sy * 0.5, 0.68), 'planks-dark', 0.01, 1.0))
        for x in (-1.7, 0, 1.7):
            parts.append(plank_box(f'leg{sy}{x}', (0.16, 0.16, 0.6), (x, sy * 0.5, 0.3), 'planks-dark', 0.01, 0.8))
    for x in (-1.7, 0, 1.7):
        parts.append(cylinder(f'post{x}', 0.05, 0.04, 1.0, loc=(x, 0.6, 1.3), segs=8, mat=M('planks-worn')))
    parts.append(plank_box('rail', (L, 0.06, 0.08), (0, 0.6, 1.78), 'planks-worn', 0.01, 1.0))
    o = join(parts, 'mine-walkway')
    smooth(o, 40)
    return g(o)


def cargo_lift():
    W, H = 2.2, 4.2
    plat = planks_row('platform', 8, W, 0.26, 0.08, (0, 0, 0.25), 'planks-worn', axis='y', seed=716)
    parts = [plat, plank_box('frame', (W, W, 0.12), (0, 0, 0.15), 'iron-rust', 0.01, 1.0)]
    boolean(parts[1], box('hole', (W - 0.3, W - 0.3, 0.3), loc=(0, 0, 0.15)))
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(plank_box(f'post{sx}{sy}', (0.2, 0.2, H), (sx * W * 0.55, sy * W * 0.55, H * 0.5), 'planks-dark', 0.015, 0.9))
    for sy in (-1, 1):
        parts.append(plank_box(f'beam{sy}', (W * 1.25, 0.2, 0.2), (0, sy * W * 0.55, H - 0.1), 'planks-dark', 0.015, 1.0))
    parts.append(plank_box('cross', (0.2, W * 1.25, 0.2), (0, 0, H + 0.05), 'planks-dark', 0.015, 1.0))
    parts.append(gear('wheel', 0.45, 14, 0.12, loc=(0, 0, H - 0.5), rot=(0, 0, math.pi / 2)))
    parts.append(chain('chain', (0, 0, H - 0.95), (0, 0, 0.45), 20, link=0.06))
    parts.append(torus('ring', 0.1, 0.02, loc=(0, 0, 0.4), rot=(math.pi / 2, 0, 0), mat=M('iron'), segs=14, ring=6))
    o = join(parts, 'cargo-lift')
    smooth(o, 40)
    return g(o)


def winch():
    parts = []
    for sy in (-1, 1):
        parts.append(plank_box(f'post{sy}', (0.16, 0.16, 1.1), (0, sy * 0.55, 0.55), 'planks-dark', 0.01, 0.8))
        parts.append(plank_box(f'foot{sy}', (0.7, 0.16, 0.1), (0, sy * 0.55, 0.05), 'planks-dark', 0.01, 0.8))
    parts.append(cylinder('drum', 0.22, 0.22, 1.0, loc=(0, 0, 0.9), rot=(math.pi / 2, 0, 0), segs=16, mat=M('planks-worn')))
    box_uv(parts[-1], 0.6)
    for i in range(6):
        parts.append(torus(f'coil{i}', 0.24, 0.02, loc=(0, -0.3 + i * 0.1, 0.9), rot=(math.pi / 2, 0, 0), mat=M('rope'), segs=20, ring=5))
    parts.append(cylinder('axle', 0.04, 0.04, 1.4, loc=(0, 0, 0.9), rot=(math.pi / 2, 0, 0), segs=8, mat=M('iron')))
    parts.append(gear('cog', 0.25, 12, 0.06, loc=(0, 0.66, 0.9), rot=(0, 0, 0)))
    parts.append(cylinder('crank', 0.025, 0.025, 0.4, loc=(0.1, 0.78, 1.05), rot=(0, 1.0, 0), segs=8, mat=M('iron')))
    parts.append(cylinder('grip', 0.03, 0.03, 0.2, loc=(0.3, 0.78, 1.25), rot=(math.pi / 2, 0, 0), segs=8, mat=M('planks-dark')))
    parts.append(box('pawl', (0.08, 0.04, 0.25), loc=(0.2, 0.66, 1.15), rot=(0, 0.5, 0), mat=M('iron')))
    o = join(parts, 'winch')
    smooth(o, 40)
    return g(o)


def lift_chain():
    parts = [plank_box('beam', (2.0, 0.24, 0.24), (0, 0, 3.0), 'planks-dark', 0.015, 1.0), torus('ring', 0.12, 0.025, loc=(0, 0, 2.8), rot=(math.pi / 2, 0, 0), mat=M('iron-rust'), segs=16, ring=6),
             chain('chain', (0, 0, 2.7), (0, 0, 0.5), 18, link=0.07)]
    hook = kit_extra.arc('hook', 0.15, 0.03, math.pi * 1.3, loc=(0, 0, 0.3), rot=(math.pi / 2, 0, -math.pi / 2), mat=M('iron-rust'), segs=16, ring=6)
    parts.append(hook)
    for sx in (-1, 1):
        parts.append(plank_box(f'post{sx}', (0.2, 0.2, 2.9), (sx * 0.95, 0, 1.45), 'planks-dark', 0.015, 0.9))
    o = join(parts, 'lift-chain')
    smooth(o, 40)
    return g(o)


def wooden_platform():
    deck = planks_row('deck', 10, 2.6, 0.26, 0.07, (0, 0, 0.42), 'planks-worn', axis='y', seed=717)
    parts = [deck]
    for y in (-1.1, 0, 1.1):
        parts.append(plank_box(f'joist{y}', (2.6, 0.14, 0.2), (0, y, 0.28), 'planks-dark', 0.01, 1.0))
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(plank_box(f'block{sx}{sy}', (0.3, 0.3, 0.2), (sx * 1.0, sy * 1.0, 0.1), 'planks-dark', 0.01, 0.8))
    o = join(parts, 'wooden-platform')
    smooth(o, 40)
    return g(o)


def mine_barricade():
    parts = []
    for sx in (-1, 1):
        parts.append(plank_box(f'post{sx}', (0.16, 0.16, 1.6), (sx * 1.2, 0, 0.8), 'planks-dark', 0.01, 0.8))
    for z in (0.5, 1.2):
        parts.append(plank_box(f'rail{z}', (2.6, 0.06, 0.2), (0, 0.1, z), 'planks-worn', 0.01, 1.0))
    for k in (-1, 1):
        parts.append(plank_box(f'x{k}', (2.7, 0.06, 0.18), (0, 0.17, 0.85), 'planks-worn', 0.01, 1.0, rot=(0, k * 0.5, 0)))
    parts.append(box('sign', (0.5, 0.03, 0.3), loc=(0.6, 0.22, 1.0), rot=(0, 0, 0.1), mat=M('planks-dark')))
    o = join(parts, 'mine-barricade')
    smooth(o, 40)
    return g(o)


def dwarven_door(seed=718):
    W, H, T = 3.2, 3.8, 0.5
    frame = stone_block('frame', (W + 0.8, T + 0.2, H + 0.5), (0, 0, (H + 0.5) * 0.5), seed, 'masonry-crypt', bev=0.03, disp=0.015)
    boolean(frame, box('opening', (W - 0.3, T + 0.6, H - 0.2), loc=(0, 0, (H - 0.2) * 0.5)))
    door = plank_box('door', (W - 0.34, 0.3, H - 0.24), (0, 0, (H - 0.24) * 0.5), 'masonry-crypt', 0.02, 1.1)
    for z in (0.6, H * 0.5, H - 0.6):
        boolean(door, box(f'g{z}', (W - 0.8, 0.06, 0.05), loc=(0, -0.15, z)))
    for x in (-0.9, 0.9):
        boolean(door, box(f'v{x}', (0.05, 0.06, H - 1.0), loc=(x, -0.15, H * 0.5)))
    boolean(door, cylinder('oct', 0.6, 0.6, 0.1, loc=(0, -0.15, H * 0.5), rot=(math.pi / 2, 0, 0), segs=8))
    parts = [frame, door]
    for z in (0.9, H - 0.9):
        parts.append(box(f'band{z}', (W - 0.5, 0.06, 0.2), loc=(0, -0.19, z), mat=M('iron')))
        for x in (-1.1, -0.4, 0.4, 1.1):
            parts.append(sphere(f'rivet{x}{z}', 0.035, (x, -0.22, z), subdiv=1, mat=M('iron')))
    parts.append(torus('ring', 0.18, 0.03, loc=(0.9, -0.25, H * 0.45), rot=(math.pi / 2, 0, 0), mat=M('iron'), segs=20, ring=6))
    o = join(parts, 'dwarven-door')
    smooth(o, 40)
    return g(o)


def dwarven_pillar():
    prof = [(0, 0), (0.9, 0), (0.9, 0.35), (0.65, 0.5), (0.55, 0.6), (0.55, 1.5), (0.65, 1.6), (0.55, 1.7), (0.55, 3.2), (0.65, 3.3), (0.55, 3.4), (0.55, 3.9), (0.8, 4.1), (0.9, 4.4), (0, 4.4)]
    p = lathe('dwarven-pillar', prof, segs=8, mat=M('masonry-crypt'))
    box_uv(p, 1.2)
    for z in (1.0, 2.5):
        for i in range(8):
            a = math.tau * (i + 0.5) / 8
            boolean(p, box(f'g{z}{i}', (0.2, 0.08, 0.4), loc=(math.cos(a) * 0.55, math.sin(a) * 0.55, z), rot=(0, 0, a)))
    flat(p)
    return g(p)


def stone_brazier():
    ped = pillar('ped', [(0, 0), (0.6, 0), (0.6, 0.15), (0.35, 0.25), (0.3, 0.9), (0.55, 1.0), (0.75, 1.15), (0.78, 1.25), (0.6, 1.25), (0.5, 1.1), (0.3, 1.05), (0, 1.05)], 719, segs=8, chips=0, disp=0.02)
    rng = rand(719)
    parts = [ped]
    for i in range(10):
        parts.append(sphere(f'coal{i}', rng.uniform(0.08, 0.14), (rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), 1.05 + rng.uniform(0, 0.12)), subdiv=1, mat=M('ember-glow' if i % 2 else 'coal')))
    o = join(parts, 'stone-brazier')
    smooth(o, 40)
    return g(o)


def tunnel_marker():
    post = stone_block('post', (0.4, 0.4, 1.7), (0, 0, 0.85), 720, 'masonry-crypt', bev=0.03)
    boolean(post, box('notch', (0.5, 0.5, 0.12), loc=(0, 0, 1.4)))
    boolean(post, box('arrow', (0.2, 0.1, 0.05), loc=(0, -0.2, 0.9)))
    boolean(post, box('arrow2', (0.05, 0.1, 0.25), loc=(0.08, -0.2, 0.9), rot=(0, 0.7, 0)))
    ring = torus('ring', 0.09, 0.015, loc=(0, -0.22, 1.2), rot=(math.pi / 2, 0, 0), mat=M('iron-rust'), segs=14, ring=6)
    o = join([post, ring], 'tunnel-marker')
    smooth(o, 40)
    return g(o)


def giant_mushrooms():
    rng = rand(721)
    parts = []
    for i, (x, y, r, h) in enumerate(((0, 0, 0.95, 2.1), (1.1, 0.5, 0.65, 1.4), (-0.9, 0.6, 0.5, 1.0))):
        m, _ = kit_extra._mushroom(f'm{i}', x, y, r, h, rng)
        parts.append(m)
    o = join(parts, 'giant-mushrooms')
    vertex_colors(o, (0.55, 0.5, 0.42), 0.25, 0.5, 721, gradient=(0, 1.0, 0.65), select=lambda c, n: (0.3, 0.28, 0.36) if n.z > 0.4 and c.z > 0.9 else None)
    smooth(o, 60)
    return g(o)


def bioluminescent_fungi():
    rng = rand(722)
    base = rock('base', (1.6, 1.3, 0.5), 722, 'stone', top='moss')
    parts = [base]
    for i in range(10):
        r, h = rng.uniform(0.06, 0.16), rng.uniform(0.15, 0.4)
        prof = [(0, h + r * 0.5), (r * 0.8, h + r * 0.3), (r, h), (r * 0.3, h - r * 0.1), (r * 0.18, 0.02), (0, 0)]
        m = lathe(f'f{i}', prof, segs=12, loc=(rng.uniform(-0.6, 0.6), rng.uniform(-0.5, 0.5), 0.15 + rng.uniform(0, 0.2)), mat=M('bio-glow'))
        parts.append(m)
    o = join(parts, 'bioluminescent-fungi')
    smooth(o, 50)
    return g(o)


def petrified_roots(seed=723):
    rng = rand(seed)
    parts = []
    for i in range(6):
        a = rng.uniform(0, math.tau)
        r = kit_extra.arc(f'root{i}', rng.uniform(0.5, 1.1), rng.uniform(0.08, 0.16), math.pi * rng.uniform(0.6, 1.0), loc=(rng.uniform(-0.4, 0.4), rng.uniform(-0.4, 0.4), -0.05),
                          rot=(math.pi / 2 + rng.uniform(-0.3, 0.3), 0, a), mat=M('petrified'), segs=16, ring=8)
        subdivide(r, 1, simple=True)
        displace(r, 0.04, 0.2, seed=seed + i)
        apply_mods(r)
        box_uv(r, 0.6)
        parts.append(r)
    parts.append(rock('ground', (2.2, 2.0, 0.25), seed, 'stone'))
    o = join(parts, 'petrified-roots')
    clamp_floor(o, 0.0)
    smooth(o, 55)
    return g(o)


def boat_wreck(seed=724):
    hull = sphere('hull', 1.0, (0, 0, 0.9), subdiv=4, mat=M('planks-dark'), scale=(2.6, 0.9, 0.9))
    boolean(hull, box('top', (6, 3, 2), loc=(0, 0, 1.9)))
    inner = sphere('inner', 1.0, (0, 0, 0.95), subdiv=3, scale=(2.5, 0.8, 0.85))
    boolean(hull, inner)
    boolean(hull, box('break', (1.6, 1.2, 1.5), loc=(2.0, 0.5, 1.2), rot=(0.3, 0.2, 0.6)))
    box_uv(hull, 0.9)
    parts = [hull]
    for i in range(7):
        x = -1.9 + i * 0.6
        parts.append(kit_extra.arc(f'rib{i}', 0.75, 0.04, math.pi, loc=(x, 0, 0.95), rot=(math.pi / 2, 0, math.pi / 2), mat=M('planks-worn'), segs=14, ring=6))
    parts.append(plank_box('keel', (5.0, 0.12, 0.12), (0, 0, 0.06), 'planks-dark', 0.01, 1.0))
    rng = rand(seed)
    for i in range(5):
        parts.append(plank_box(f'plank{i}', (rng.uniform(0.6, 1.2), 0.18, 0.03), (rng.uniform(1.5, 2.8), rng.uniform(-1.0, 1.0), 0.02), 'planks-worn', 0.005, 0.9, rot=(0, 0, rng.uniform(0, 3))))
    o = join(parts, 'boat-wreck')
    o.rotation_euler = (0.25, 0, 0)
    apply_transform(o)
    clamp_floor(o, 0.0)
    smooth(o, 45)
    return g(o)


def ancient_fragments(seed=725):
    rng = rand(seed)
    parts = []
    for i in range(5):
        s = rng.uniform(0.5, 0.9)
        b = stone_block(f'frag{i}', (s, s * 0.7, s * 0.5), (rng.uniform(-1.0, 1.0), rng.uniform(-0.8, 0.8), s * 0.25), seed + i, 'masonry-crypt', rot=(rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), rng.uniform(0, 3)), bev=0.03, disp=0)
        for k in range(3):
            boolean(b, box(f'g{i}{k}', (s * 0.6, 0.05, 0.03), loc=(b.location.x, b.location.y - s * 0.35, s * 0.15 + k * 0.08)))
        parts.append(b)
    parts.append(pillar('drum', [(0, 0), (0.4, 0), (0.4, 0.7), (0, 0.7)], seed, segs=12, chips=1, disp=0.02))
    parts[-1].rotation_euler = (math.pi / 2, 0, 0.5)
    parts[-1].location = (1.3, -1.0, 0.4)
    apply_transform(parts[-1])
    o = join(parts, 'ancient-fragments')
    clamp_floor(o, 0.0)
    smooth(o, 40)
    return g(o)


if __name__ == '__main__':
    run('mines', [
        ('tunnel-entrance', tunnel_entrance), ('mine-rails', mine_rails), ('ore-cart', ore_cart), ('pickaxe-on-rock', pickaxe_on_rock), ('mining-shovel', mining_shovel),
        ('powder-barrel', powder_barrel), ('ore-crates', ore_crates), ('raw-ore-pile', raw_ore_pile), ('mineral-crystals', mineral_crystals), ('ember-vein-wall', ember_vein_wall),
        ('large-crystal-formation', large_crystal_formation), ('natural-stone-column', natural_stone_column), ('stalactites', stalactites), ('stalagmites', stalagmites),
        ('rock-arch-formation', rock_arch_formation), ('narrow-wooden-bridge', narrow_wooden_bridge), ('mine-walkway', mine_walkway), ('cargo-lift', cargo_lift),
        ('winch', winch), ('lift-chain', lift_chain), ('wooden-platform', wooden_platform), ('mine-barricade', mine_barricade), ('dwarven-door', dwarven_door),
        ('dwarven-pillar', dwarven_pillar), ('stone-brazier', stone_brazier), ('tunnel-marker', tunnel_marker), ('giant-mushrooms', giant_mushrooms),
        ('bioluminescent-fungi', bioluminescent_fungi), ('petrified-roots', petrified_roots), ('boat-wreck', boat_wreck), ('ancient-fragments', ancient_fragments),
    ])
