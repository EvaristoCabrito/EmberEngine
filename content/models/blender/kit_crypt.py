"""Wave 2, group 6: dungeons, crypts and ancient ruins (content/models/crypt)."""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kitlib import *  # noqa

PILLAR = [(0, 0), (0.85, 0), (0.85, 0.3), (0.65, 0.42), (0.55, 0.5), (0.55, 3.4), (0.62, 3.55), (0.62, 3.75), (0.8, 3.9), (0.8, 4.2), (0, 4.2)]


def g(o):
    ground(o)
    return o


def cracked_pillar():
    p = pillar('cracked-pillar', PILLAR, 601, chips=2)
    boolean(p, box('crack', (0.05, 2.0, 2.2), loc=(0.45, 0, 2.0), rot=(0.2, 0.25, 0.6)))
    boolean(p, box('crack2', (0.04, 2.0, 1.0), loc=(-0.4, 0.1, 1.2), rot=(0.1, -0.3, 1.2)))
    smooth(p, 40)
    return g(p)


def column_base():
    p = pillar('column-base', [(0, 0), (0.85, 0), (0.85, 0.3), (0.65, 0.42), (0.55, 0.5), (0.55, 1.2), (0, 1.2)], 602, chips=1)
    boolean(p, box('break', (2.0, 2.0, 1.0), loc=(0.2, 0.1, 1.5), rot=(0.4, 0.3, 0.3)))
    smooth(p, 40)
    return g(p)


def stone_capital():
    cap = stone_block('cap', (1.3, 1.3, 0.4), (0, 0, 0.75), 603, 'masonry-crypt', bev=0.04)
    flare = lathe('flare', [(0, 0), (0.55, 0), (0.62, 0.3), (0.72, 0.5), (0.6, 0.55), (0, 0.55)], segs=20, mat=M('masonry-crypt'))
    subdivide(flare, 1, simple=True)
    displace(flare, 0.03, 0.2, seed=603)
    apply_mods(flare)
    box_uv(flare, 1.1)
    o = join([cap, flare], 'stone-capital')
    o.rotation_euler = (0.12, 0.05, 0)
    apply_transform(o)
    smooth(o, 40)
    return g(o)


def iron_grate():
    parts = [plank_box('frame', (1.5, 1.5, 0.08), (0, 0, 0.04), 'iron-rust', 0.01, 1.0)]
    boolean(parts[0], box('hole', (1.3, 1.3, 0.2), loc=(0, 0, 0.04)))
    for i in range(7):
        parts.append(box(f'bar{i}', (1.32, 0.05, 0.05), loc=(0, -0.6 + i * 0.2, 0.04), mat=M('iron-rust')))
    for i in range(2):
        parts.append(box(f'cross{i}', (0.05, 1.32, 0.05), loc=(-0.3 + i * 0.6, 0, 0.04), mat=M('iron-rust')))
    o = join(parts, 'iron-grate')
    smooth(o, 40)
    return g(o)


def barred_gate():
    W, H = 2.2, 2.9
    parts = [box('postL', (0.12, 0.12, H), loc=(-W * 0.5, 0, H * 0.5), mat=M('iron-rust')), box('postR', (0.12, 0.12, H), loc=(W * 0.5, 0, H * 0.5), mat=M('iron-rust')),
             box('top', (W + 0.12, 0.12, 0.12), loc=(0, 0, H - 0.06), mat=M('iron-rust'))]
    for i in range(1, 10):
        x = -W * 0.5 + W * i / 10
        parts.append(cylinder(f'bar{i}', 0.03, 0.03, H - 0.12, loc=(x, 0, (H - 0.12) * 0.5), segs=6, mat=M('iron-rust')))
        parts.append(cylinder(f'spike{i}', 0.03, 0.0, 0.18, loc=(x, 0, H + 0.03), segs=6, mat=M('iron-rust')))
    for z in (0.4, H * 0.5, H - 0.4):
        parts.append(box(f'rail{z}', (W, 0.06, 0.08), loc=(0, 0, z), mat=M('iron-rust')))
    for z in (0.5, H - 0.5):
        parts.append(cylinder(f'hinge{z}', 0.06, 0.06, 0.2, loc=(-W * 0.5 - 0.06, 0, z), segs=8, mat=M('iron')))
    parts.append(box('lock', (0.2, 0.08, 0.3), loc=(W * 0.5 - 0.25, 0, H * 0.5), mat=M('iron')))
    o = join(parts, 'barred-gate')
    smooth(o, 40)
    return g(o)


def chain_ring():
    parts = [plank_box('plate', (0.4, 0.06, 0.4), (0, 0, 1.6), 'iron-rust', 0.01, 1.0), torus('ring', 0.15, 0.03, loc=(0, -0.08, 1.55), mat=M('iron-rust'), segs=20, ring=8)]
    for sx in (-1, 1):
        for z in (1.45, 1.75):
            parts.append(sphere(f'bolt{sx}{z}', 0.03, (sx * 0.14, -0.04, z), subdiv=1, mat=M('iron')))
    parts.append(chain('chain', (0, -0.12, 1.42), (0.15, -0.2, 0.05), 11, link=0.06))
    o = join(parts, 'chain-ring')
    smooth(o, 40)
    return g(o)


def ritual_brazier():
    bowl = lathe('bowl', [(0, 0.9), (0.4, 0.9), (0.62, 1.05), (0.68, 1.3), (0.6, 1.3), (0.55, 1.1), (0.38, 0.98), (0, 0.98)], segs=20, mat=M('masonry-crypt'))
    subdivide(bowl, 1, simple=True)
    displace(bowl, 0.03, 0.2, seed=604)
    apply_mods(bowl)
    box_uv(bowl, 1.0)
    parts = [bowl]
    for i in range(3):
        a = math.tau * i / 3
        l = box(f'leg{i}', (0.18, 0.18, 1.0), loc=(math.cos(a) * 0.35, math.sin(a) * 0.35, 0.5), rot=(0, 0, a), mat=M('masonry-crypt'))
        bevel(l, 0.02, 1)
        apply_mods(l)
        box_uv(l, 1.0)
        parts.append(l)
    rng = rand(604)
    for i in range(10):
        parts.append(sphere(f'coal{i}', rng.uniform(0.08, 0.14), (rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), 1.0 + rng.uniform(0, 0.15)), subdiv=1, mat=M('ember-glow' if i % 2 else 'coal')))
    o = join(parts, 'ritual-brazier')
    smooth(o, 40)
    return g(o)


def artifact_pedestal():
    p = pillar('artifact-pedestal', [(0, 0), (0.55, 0), (0.55, 0.15), (0.4, 0.25), (0.3, 0.3), (0.3, 1.05), (0.42, 1.15), (0.5, 1.2), (0.5, 1.3), (0, 1.3)], 605, segs=8, chips=0, disp=0.015)
    boolean(p, box('groove', (0.7, 0.03, 0.03), loc=(0, -0.3, 0.7)))
    smooth(p, 40)
    return g(p)


def _sarcophagus(seed, open_lid=False):
    body = stone_block('body', (2.6, 1.15, 0.9), (0, 0, 0.45), seed, 'masonry-crypt', bev=0.04)
    boolean(body, box('hollow', (2.3, 0.85, 0.8), loc=(0, 0, 0.6)))
    for z in (0.25, 0.6):
        boolean(body, box(f'groove{z}', (2.4, 0.05, 0.04), loc=(0, -0.575, z)))
    parts = [body]
    lid = stone_block('lid', (2.7, 1.25, 0.2), (0, 0, 1.0), seed + 1, 'masonry-crypt', bev=0.04)
    fig = figure('effigy', 1.6, 'masonry-crypt', hood=False, seed=seed)
    fig.rotation_euler = (-math.pi / 2, 0, -math.pi / 2)
    fig.scale = (0.9, 0.9, 0.45)
    fig.location = (-0.7, 0, 1.12)
    apply_transform(fig)
    lid = join([lid, fig], 'lid')
    if open_lid:
        lid.rotation_euler = (0.35, 0.08, 0.12)
        lid.location = (0.3, -0.9, 0.3)
        apply_transform(lid)
        piece = stone_block('piece', (0.8, 0.6, 0.18), (1.6, 0.9, 0.09), seed + 2, 'masonry-crypt', bev=0.03, rot=(0, 0, 0.4))
        parts.append(piece)
    parts.append(lid)
    return parts


def sarcophagus():
    o = join(_sarcophagus(606), 'sarcophagus')
    smooth(o, 40)
    return g(o)


def sarcophagus_broken_lid():
    o = join(_sarcophagus(607, open_lid=True), 'sarcophagus-broken-lid')
    clamp_floor(o, 0.0)
    smooth(o, 40)
    return g(o)


def funeral_urns():
    a = vessel('urnA', URN, (0, 0, 0), 'masonry-crypt', 1.0, segs=22)
    b = vessel('urnB', URN, (0.8, 0.2, 0), 'clay', 0.7, segs=22)
    o = join([a, b], 'funeral-urns')
    box_uv(o, 0.8)
    smooth(o, 50)
    return g(o)


def lone_skull():
    return g(skull('skull', (0, 0, 0), r=0.24, seed=608))


def incomplete_skeleton(seed=609):
    rng = rand(seed)
    parts = [skull('skull', (-0.9, 0.05, 0), r=0.22, seed=seed)]
    for i in range(9):
        parts.append(cylinder(f'vert{i}', 0.06, 0.06, 0.08, loc=(-0.55 + i * 0.12, 0, 0.07), rot=(0, math.pi / 2, 0), segs=8, mat=M('bone')))
    for i in range(5):
        for sy in (-1, 1):
            r = kit_extra.arc(f'rib{i}{sy}', 0.28 - i * 0.02, 0.018, math.pi * 0.8, loc=(-0.45 + i * 0.13, 0, 0.08), rot=(math.pi / 2 + sy * 0.5, 0, sy * math.pi / 2), mat=M('bone'), segs=14, ring=5)
            parts.append(r)
    parts.append(bone('arm', 0.55, (-0.3, 0.45, 0.05), (1, 0.3, 0), r=0.04))
    parts.append(bone('leg', 0.75, (0.9, -0.2, 0.06), (1, -0.4, 0), r=0.05))
    parts.append(bone('leg2', 0.6, (1.3, 0.3, 0.05), (0.7, 1, 0), r=0.045))
    parts.append(stone_block('pelvis', (0.3, 0.45, 0.12), (0.5, 0, 0.06), seed, 'masonry', bev=0.04, disp=0))
    o = join(parts, 'incomplete-skeleton')
    clamp_floor(o, 0.0)
    vertex_colors(o, BONE, 0.2, 0.3, seed)
    set_material_where(o, M('bone'), lambda c, n: True)
    smooth(o, 50)
    return g(o)


def stone_tomb():
    body = stone_block('body', (2.3, 1.1, 0.75), (0, 0, 0.375), 610, 'masonry-mossy', bev=0.04)
    slab = stone_block('slab', (2.45, 1.25, 0.14), (0, 0, 0.82), 611, 'masonry-crypt', bev=0.03)
    for x in (-0.8, 0, 0.8):
        boolean(body, box(f'panel{x}', (0.5, 0.06, 0.4), loc=(x, -0.55, 0.37)))
    o = join([body, slab], 'stone-tomb')
    smooth(o, 40)
    return g(o)


def ancient_tombstone():
    W, T, H = 1.1, 0.26, 1.5
    s = box('slab', (W, T, H), loc=(0, 0, H * 0.5), mat=M('masonry-crypt'))
    top = cylinder('top', W * 0.5, W * 0.5, T, loc=(0, 0, H), rot=(math.pi / 2, 0, 0), segs=24, mat=M('masonry-crypt'))
    s = boolean(s, top, 'UNION')
    for z in (0.5, 0.7, 0.9, 1.1):
        boolean(s, box(f'line{z}', (W * 0.6, 0.06, 0.03), loc=(0, -T * 0.5, z)))
    boolean(s, cylinder('medal', 0.22, 0.22, 0.08, loc=(0, -T * 0.5, 1.45), rot=(math.pi / 2, 0, 0), segs=16))
    subdivide(s, 2, simple=True)
    displace(s, 0.03, 0.2, seed=612)
    apply_mods(s)
    box_uv(s, 1.0)
    base = stone_block('base', (W + 0.4, T + 0.5, 0.2), (0, 0, 0.1), 612, 'masonry-mossy', bev=0.03)
    o = join([s, base], 'ancient-tombstone')
    smooth(o, 40)
    return g(o)


def ritual_table():
    top = stone_block('top', (2.2, 1.0, 0.18), (0, 0, 0.95), 613, 'masonry-crypt', bev=0.03)
    for y in (-0.3, 0.3):
        boolean(top, box(f'channel{y}', (1.9, 0.05, 0.05), loc=(0, y, 1.04)))
    boolean(top, box('channel2', (0.05, 0.75, 0.05), loc=(0.8, 0, 1.04)))
    parts = [top]
    for sx in (-1, 1):
        parts.append(stone_block(f'leg{sx}', (0.3, 0.8, 0.86), (sx * 0.8, 0, 0.43), 613 + sx, 'masonry-crypt', bev=0.03))
    parts.append(vessel('bowl', BOWL, (-0.5, 0.2, 1.04), 'bronze', 1.0))
    o = join(parts, 'ritual-table')
    smooth(o, 40)
    return g(o)


def ancient_bookshelf(seed=614):
    rng = rand(seed)
    parts = [stone_block('side-l', (0.18, 0.5, 2.3), (-0.7, 0, 1.15), seed, 'masonry-crypt', bev=0.02), stone_block('side-r', (0.18, 0.5, 2.3), (0.7, 0, 1.15), seed + 1, 'masonry-crypt', bev=0.02),
             stone_block('back', (1.4, 0.1, 2.3), (0, 0.2, 1.15), seed + 2, 'masonry-crypt', bev=0.01, disp=0)]
    for i in range(5):
        parts.append(stone_block(f'shelf{i}', (1.3, 0.45, 0.08), (0, 0, 0.1 + i * 0.52), seed + 3 + i, 'masonry-crypt', bev=0.01, disp=0))
    for level in range(4):
        z = 0.14 + level * 0.52
        x = -0.58
        while x < 0.5:
            w = rng.uniform(0.06, 0.12)
            h = rng.uniform(0.25, 0.4)
            parts.append(plank_box(f'book{level}{x:.2f}', (w, 0.3, h), (x + w * 0.5, 0.0, z + h * 0.5), rng.choice(('leather', 'paper', 'cloth-dark', 'leather')), 0.005, 1.0, rot=(0, rng.uniform(-0.1, 0.1) if rng.random() < 0.2 else 0, 0)))
            x += w + 0.015
            if rng.random() < 0.2:
                x += rng.uniform(0.08, 0.2)
    o = join(parts, 'ancient-bookshelf')
    smooth(o, 40)
    return g(o)


def stone_pulpit():
    base = pillar('base', [(0, 0), (0.6, 0), (0.6, 0.2), (0.4, 0.3), (0.35, 1.2), (0.5, 1.3), (0, 1.3)], 615, segs=8, chips=0, disp=0.02)
    top = stone_block('top', (0.9, 0.7, 0.1), (0, -0.1, 1.4), 615, 'masonry-crypt', bev=0.02, rot=(0.3, 0, 0))
    parts = [base, top, stone_block('step', (1.2, 0.6, 0.25), (0, 0.6, 0.125), 616, 'masonry-crypt', bev=0.02)]
    o = join(parts, 'stone-pulpit')
    smooth(o, 40)
    return g(o)


def hook_chain():
    parts = [torus('ring', 0.12, 0.025, loc=(0, 0, 2.4), rot=(math.pi / 2, 0, 0), mat=M('iron-rust'), segs=18, ring=6), chain('chain', (0, 0, 2.3), (0, 0, 0.75), 13, link=0.06)]
    hook = kit_extra.arc('hook', 0.12, 0.022, math.pi * 1.3, loc=(0.0, 0, 0.6), rot=(math.pi / 2, 0, -math.pi / 2), mat=M('iron-rust'), segs=16, ring=6)
    parts.append(hook)
    parts.append(cylinder('tip', 0.022, 0.0, 0.12, loc=(-0.12, 0, 0.62), segs=6, mat=M('iron-rust')))
    base = cylinder('post', 0.04, 0.04, 2.5, loc=(0.5, 0, 1.25), segs=8, mat=M('iron-rust'))
    arm = box('arm', (0.6, 0.05, 0.05), loc=(0.25, 0, 2.45), mat=M('iron-rust'))
    o = join(parts + [base, arm], 'hook-chain')
    smooth(o, 40)
    return g(o)


def collapsed_ceiling(seed=617):
    rng = rand(seed)
    o = block_pile('collapsed-ceiling', seed, n=14, smax=0.8, spread=1.0, mat='masonry-crypt', mat2='masonry-mossy', dust='ruins-floor')
    parts = [o]
    for i in range(3):
        parts.append(plank_box(f'beam{i}', (rng.uniform(1.8, 2.6), 0.2, 0.2), (rng.uniform(-0.4, 0.4), rng.uniform(-0.5, 0.5), 0.5 + i * 0.25), 'planks-dark', 0.01, 1.0, rot=(rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), rng.uniform(0, 3))))
    return g(join(parts, 'collapsed-ceiling'))


def wall_blocks(seed=618):
    parts = []
    for row in range(3):
        for i in range(4 - row % 2):
            x = (i - (3 - row % 2) * 0.5) * 0.85
            parts.append(stone_block(f'b{row}{i}', (0.8, 0.55, 0.45), (x, 0, 0.225 + row * 0.46), seed + row * 10 + i, 'masonry', bev=0.03))
    parts.append(stone_block('fallen', (0.8, 0.55, 0.45), (1.9, 0.5, 0.225), seed + 50, 'masonry-mossy', bev=0.03, rot=(0.1, 0, 0.5)))
    o = join(parts, 'wall-blocks')
    smooth(o, 40)
    return g(o)


def statue_fragments(seed=619):
    f = figure('statue', 2.6, 'masonry-crypt', hood=True, seed=seed)
    boolean(f, box('cut', (3, 3, 2), loc=(0, 0, 2.3), rot=(0.3, 0.2, 0)))
    f.rotation_euler = (0, 0.25, 0)
    apply_transform(f)
    head = sphere('head', 0.26, (1.2, 0.5, 0.22), subdiv=3, mat=M('masonry-crypt'))
    box_uv(head, 1.0)
    arm = cylinder('arm', 0.13, 0.11, 0.9, loc=(0.9, -0.6, 0.12), rot=(0, math.pi / 2, 0.8), segs=12, mat=M('masonry-crypt'))
    box_uv(arm, 1.0)
    o = join([f, head, arm, block_pile('bits', seed, n=6, smax=0.35, spread=0.9, mat='masonry-crypt', dust=None)], 'statue-fragments')
    clamp_floor(o, 0.0)
    smooth(o, 50)
    return g(o)


def deity_statue():
    f = figure('deity', 3.2, 'masonry-crypt', hood=True, seed=620)
    f.data.transform(Matrix.Translation((0, 0, 0.6)))
    ped = pillar('pedestal', [(0, 0), (0.9, 0), (0.9, 0.2), (0.75, 0.3), (0.75, 0.5), (0.85, 0.6), (0, 0.6)], 620, segs=8, chips=0, disp=0.015)
    o = join([f, ped], 'deity-statue')
    smooth(o, 50)
    return g(o)


def secret_stone_door():
    wall = stone_block('wall', (2.6, 0.45, 3.2), (0, 0, 1.6), 621, 'masonry-crypt', bev=0.02)
    boolean(wall, box('opening', (1.3, 0.6, 2.7), loc=(0, 0, 1.35)))
    dark = box('dark', (1.28, 0.05, 2.68), loc=(0, 0.2, 1.35), mat=M('passage-dark'))
    door = stone_block('door', (1.26, 0.35, 2.66), (0, 0, 1.33), 622, 'masonry-crypt', bev=0.02)
    door.data.transform(Matrix.Translation((0.63, 0, 0)) @ Matrix.Rotation(-0.45, 4, 'Z') @ Matrix.Translation((-0.63, 0, 0)))
    door.data.transform(Matrix.Translation((-0.63 + 0.63, -0.05, 0)))
    o = join([wall, dark, door], 'secret-stone-door')
    smooth(o, 40)
    return g(o)


def iron_trapdoor():
    frame = plank_box('frame', (1.5, 1.5, 0.1), (0, 0, 0.05), 'masonry', 0.01, 1.0)
    boolean(frame, box('hole', (1.2, 1.2, 0.3), loc=(0, 0, 0.05)))
    hatch = plank_box('hatch', (1.18, 1.18, 0.06), (0, 0, 0.07), 'iron-rust', 0.01, 1.0)
    parts = [frame, hatch, torus('ring', 0.12, 0.02, loc=(0.2, 0, 0.11), rot=(0, 0, 0), mat=M('iron'), segs=18, ring=6)]
    for x in (-0.4, 0.4):
        parts.append(cylinder(f'hinge{x}', 0.03, 0.03, 0.2, loc=(x, 0.6, 0.1), rot=(0, math.pi / 2, 0), segs=8, mat=M('iron')))
    for z in (-0.4, 0, 0.4):
        parts.append(box(f'strap{z}', (1.1, 0.08, 0.02), loc=(0, z, 0.11), mat=M('iron')))
    o = join(parts, 'iron-trapdoor')
    smooth(o, 40)
    return g(o)


def inscribed_stones(seed=623):
    rng = rand(seed)
    parts = []
    for i, (x, y, h) in enumerate(((0, 0, 1.9), (-1.1, 0.4, 1.4), (1.0, 0.5, 1.2))):
        s = box(f'stone{i}', (0.7, 0.3, h), loc=(x, y, h * 0.5), rot=(rng.uniform(-0.08, 0.08), rng.uniform(-0.08, 0.08), rng.uniform(-0.4, 0.4)), mat=M('masonry-crypt'))
        for k in range(5):
            z = 0.3 + k * (h - 0.5) / 5
            boolean(s, box(f'g{i}{k}', (0.4 if k % 2 else 0.25, 0.08, 0.03), loc=(x + rng.uniform(-0.1, 0.1), y - 0.15, z), rot=(0, 0, 0)))
            boolean(s, cylinder(f'c{i}{k}', 0.06, 0.06, 0.08, loc=(x + rng.uniform(-0.2, 0.2), y - 0.15, z + 0.08), rot=(math.pi / 2, 0, 0), segs=8))
        subdivide(s, 1, simple=True)
        displace(s, 0.02, 0.2, seed=seed + i)
        apply_mods(s)
        box_uv(s, 1.0)
        parts.append(s)
    o = join(parts, 'inscribed-stones')
    smooth(o, 40)
    return g(o)


if __name__ == '__main__':
    run('crypt', [
        ('cracked-pillar', cracked_pillar), ('column-base', column_base), ('stone-capital', stone_capital), ('iron-grate', iron_grate), ('barred-gate', barred_gate),
        ('chain-ring', chain_ring), ('ritual-brazier', ritual_brazier), ('artifact-pedestal', artifact_pedestal), ('sarcophagus', sarcophagus),
        ('sarcophagus-broken-lid', sarcophagus_broken_lid), ('funeral-urns', funeral_urns), ('lone-skull', lone_skull), ('incomplete-skeleton', incomplete_skeleton),
        ('stone-tomb', stone_tomb), ('ancient-tombstone', ancient_tombstone), ('ritual-table', ritual_table), ('ancient-bookshelf', ancient_bookshelf),
        ('stone-pulpit', stone_pulpit), ('hook-chain', hook_chain), ('collapsed-ceiling', collapsed_ceiling), ('wall-blocks', wall_blocks),
        ('statue-fragments', statue_fragments), ('deity-statue', deity_statue), ('secret-stone-door', secret_stone_door), ('iron-trapdoor', iron_trapdoor),
        ('inscribed-stones', inscribed_stones),
    ])
