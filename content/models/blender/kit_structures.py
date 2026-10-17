"""Wave 2, group 3: city streets and constructions (content/models/structures)."""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kitlib import *  # noqa


def g(o):
    ground(o)
    return o


def animal_trough():
    body = plank_box('body', (2.2, 0.7, 0.55), (0, 0, 0.45), 'planks-worn', 0.015, 1.2)
    boolean(body, box('hollow', (2.05, 0.55, 0.5), loc=(0, 0, 0.6)))
    parts = [body, cylinder('water', 1.0, 1.0, 0.02, loc=(0, 0, 0.55), segs=4, mat=M('dark-glass'))]
    parts[1].scale = (1.0, 0.27, 1)
    apply_transform(parts[1])
    for sx in (-1, 1):
        parts.append(plank_box(f'leg{sx}', (0.12, 0.6, 0.2), (sx * 0.85, 0, 0.1), 'planks-dark', 0.01, 0.8))
    o = join(parts, 'animal-trough')
    smooth(o, 40)
    return g(o)


def hanging_sign_post():
    post = cylinder('post', 0.09, 0.08, 2.9, loc=(0, 0, 1.45), segs=10, mat=M('planks-worn'))
    box_uv(post, 0.7)
    arm = box('arm', (1.1, 0.06, 0.06), loc=(0.5, 0, 2.75), mat=M('iron'))
    brace = box('brace', (0.7, 0.04, 0.04), loc=(0.3, 0, 2.5), rot=(0, -0.7, 0), mat=M('iron'))
    board = plank_box('board', (0.9, 0.05, 0.55), (0.55, 0, 2.05), 'planks-dark', 0.01, 1.0, (0.4, 0.2))
    chains = [chain(f'chain{i}', (0.55 + i, 0, 2.72), (0.55 + i, 0, 2.35), 4, link=0.04) for i in (-0.35, 0.35)]
    o = join([post, arm, brace, board] + chains, 'hanging-sign-post')
    smooth(o, 40)
    return g(o)


def wooden_gate():
    H = 2.2
    parts = [plank_box('postL', (0.2, 0.2, H + 0.2), (-1.3, 0, (H + 0.2) * 0.5), 'planks-worn', 0.015, 0.8), plank_box('postR', (0.2, 0.2, H + 0.2), (1.3, 0, (H + 0.2) * 0.5), 'planks-worn', 0.015, 0.8)]
    gate = planks_row('gate', 8, H - 0.3, 0.26, 0.05, (0, 0, (H - 0.3) * 0.5 + 0.15), 'planks-dark', axis='y', seed=301)
    gate.rotation_euler = (0, 0, 0)
    parts.append(gate)
    for z in (0.5, H - 0.4):
        parts.append(plank_box(f'rail{z}', (2.2, 0.05, 0.14), (0, -0.05, z), 'planks-dark', 0.01, 1.0))
        for sx in (-1, 1):
            parts.append(cylinder(f'hinge{sx}{z}', 0.04, 0.04, 0.2, loc=(sx * 1.17, -0.08, z), segs=8, mat=M('iron')))
    parts.append(plank_box('diag', (2.3, 0.05, 0.14), (0, -0.05, H * 0.5 + 0.05), 'planks-dark', 0.01, 1.0, rot=(0, 0.55, 0)))
    parts.append(box('latch', (0.3, 0.04, 0.08), loc=(1.1, -0.1, H * 0.55), mat=M('iron')))
    o = join(parts, 'wooden-gate')
    smooth(o, 40)
    return g(o)


def barred_door():
    d = kit_extra.wooden_door()
    boolean(d, box('window', (0.6, 0.4, 0.45), loc=(0, 0, 2.15)))
    parts = [d]
    for x in (-0.18, 0, 0.18):
        parts.append(cylinder(f'bar{x}', 0.02, 0.02, 0.5, loc=(x, 0, 2.15), segs=6, mat=M('iron')))
    parts.append(box('frame', (0.68, 0.03, 0.05), loc=(0, -0.06, 2.4), mat=M('iron')))
    parts.append(box('frame2', (0.68, 0.03, 0.05), loc=(0, -0.06, 1.9), mat=M('iron')))
    o = join(parts, 'barred-door')
    smooth(o, 40)
    return g(o)


def _frame(W, H, T=0.1, D=0.12, mat='planks-dark'):
    parts = [plank_box('jambL', (T, D, H), (-W * 0.5 + T * 0.5, 0, H * 0.5), mat, 0.01, 0.8), plank_box('jambR', (T, D, H), (W * 0.5 - T * 0.5, 0, H * 0.5), mat, 0.01, 0.8),
             plank_box('head', (W, D, T), (0, 0, H - T * 0.5), mat, 0.01, 0.8), plank_box('sill', (W + 0.1, D + 0.08, T), (0, 0, T * 0.5), mat, 0.01, 0.8)]
    return parts


def shuttered_window():
    W, H = 1.2, 1.5
    parts = _frame(W, H)
    for sx, ang in ((-1, 0.5), (1, -0.9)):
        s = planks_row(f'shutter{sx}', 3, H - 0.2, 0.17, 0.04, (0, 0, (H - 0.2) * 0.5 + 0.1), 'planks-worn', axis='y', seed=302)
        s.data.transform(Matrix.Translation((sx * 0.27, 0, 0)) @ Matrix.Rotation(ang, 4, 'Z') @ Matrix.Translation((-sx * 0.27, 0, 0)))
        s.location = (sx * (W * 0.5 - 0.05), -0.08, 0)
        apply_transform(s)
        parts.append(s)
    parts.append(box('glass', (W - 0.2, 0.02, H - 0.2), loc=(0, 0.02, H * 0.5), mat=M('glass')))
    o = join(parts, 'shuttered-window')
    smooth(o, 40)
    return g(o)


def broken_window(seed=303):
    rng = rand(seed)
    W, H = 1.1, 1.4
    parts = _frame(W, H)
    parts.append(plank_box('mullion', (0.06, 0.08, H - 0.2), (0, 0, H * 0.5), 'planks-dark', 0.005, 0.8))
    parts.append(plank_box('transom', (W - 0.2, 0.08, 0.06), (0, 0, H * 0.5), 'planks-dark', 0.005, 0.8))
    glass = box('glass', (W - 0.2, 0.02, H - 0.2), loc=(0, 0, H * 0.5), mat=M('glass'))
    for i in range(3):
        boolean(glass, box(f'hole{i}', (rng.uniform(0.3, 0.5), 0.2, rng.uniform(0.3, 0.5)), loc=(rng.uniform(-0.35, 0.35), 0, rng.uniform(0.3, 1.1)), rot=(0, rng.uniform(0, 1), 0)))
    parts.append(glass)
    for i in range(6):
        parts.append(box(f'shard{i}', (rng.uniform(0.08, 0.2), 0.01, rng.uniform(0.06, 0.16)), loc=(rng.uniform(-0.6, 0.6), rng.uniform(-0.5, -0.1), 0.005), rot=(math.pi / 2, 0, rng.uniform(0, 3)), mat=M('glass')))
    o = join(parts, 'broken-window')
    smooth(o, 40)
    return g(o)


def window_frame():
    W, H = 1.1, 1.4
    parts = _frame(W, H)
    parts.append(plank_box('mullion', (0.06, 0.08, H - 0.2), (0, 0, H * 0.5), 'planks-dark', 0.005, 0.8))
    parts.append(plank_box('transom', (W - 0.2, 0.08, 0.06), (0, 0, H * 0.5), 'planks-dark', 0.005, 0.8))
    parts.append(box('glass', (W - 0.2, 0.02, H - 0.2), loc=(0, 0, H * 0.5), mat=M('glass')))
    o = join(parts, 'window-frame')
    smooth(o, 40)
    return g(o)


def wooden_ladder():
    H, W = 3.4, 0.6
    parts = []
    for sx in (-1, 1):
        parts.append(plank_box(f'rail{sx}', (0.07, 0.1, H), (sx * W * 0.5, 0, H * 0.5), 'planks-worn', 0.008, 0.8))
    for i in range(8):
        parts.append(cylinder(f'rung{i}', 0.03, 0.03, W, loc=(0, 0, 0.3 + i * 0.4), rot=(0, math.pi / 2, 0), segs=8, mat=M('planks-dark')))
    o = join(parts, 'wooden-ladder')
    o.rotation_euler = (0.3, 0, 0)
    apply_transform(o)
    smooth(o, 40)
    return g(o)


def stone_stairs(seed=304):
    parts = []
    n = 6
    for i in range(n):
        parts.append(stone_block(f'step{i}', (2.0, 0.42, 0.3), (0, -i * 0.4, 0.15 + i * 0.28), seed + i, 'masonry', bev=0.02))
    for i in range(n):
        parts.append(stone_block(f'fill{i}', (2.0, 0.42, 0.28 * i + 0.01), (0, -i * 0.4, (0.28 * i) * 0.5), seed + 10 + i, 'masonry-mossy', bev=0.02, disp=0))
    o = join(parts, 'stone-stairs')
    smooth(o, 40)
    return g(o)


def cloth_awning(seed=305):
    parts = []
    for sx in (-1, 1):
        parts.append(cylinder(f'post{sx}', 0.06, 0.05, 2.2, loc=(sx * 1.3, -1.2, 1.1), segs=8, mat=M('planks-worn')))
        parts.append(plank_box(f'bracket{sx}', (0.08, 0.3, 0.08), (sx * 1.3, 0.15, 2.65), 'iron', 0, 1.0))
    parts.append(plank_box('bar', (2.8, 0.06, 0.06), (0, -1.2, 2.2), 'planks-worn', 0, 1.0))
    parts.append(plank_box('wallbar', (2.8, 0.06, 0.06), (0, 0.0, 2.65), 'planks-worn', 0, 1.0))
    canopy = plane('canopy', (2.9, 1.45), loc=(0, -0.6, 2.42), rot=(0.32, 0, 0))
    canopy.data.materials.append(M('cloth-red'))
    subdivide(canopy, 3, simple=True)
    displace(canopy, 0.08, 0.4, seed=seed)
    apply_mods(canopy)
    sol = canopy.modifiers.new('solid', 'SOLIDIFY')
    sol.thickness = 0.02
    apply_mods(canopy)
    parts.append(canopy)
    for i in range(7):
        parts.append(box(f'fringe{i}', (0.3, 0.02, 0.18), loc=(-1.2 + i * 0.4, -1.3, 2.1), mat=M('cloth-red')))
    o = join(parts, 'cloth-awning')
    smooth(o, 40)
    return g(o)


def vendor_cart(seed=306):
    parts = [plank_box('bed', (2.0, 1.0, 0.08), (0, 0, 0.75), 'planks-worn', 0.01, 1.1)]
    for sy in (-1, 1):
        parts.append(plank_box(f'side{sy}', (2.0, 0.05, 0.35), (0, sy * 0.48, 0.95), 'planks-dark', 0.01, 1.1))
    parts.append(cylinder('axle', 0.045, 0.045, 1.3, loc=(0, 0, 0.5), rot=(math.pi / 2, 0, 0), segs=10, mat=M('iron')))
    for sy in (-1, 1):
        rim = torus(f'rim{sy}', 0.5, 0.06, loc=(0, sy * 0.62, 0.5), rot=(math.pi / 2, 0, 0), mat=M('planks-dark'), segs=28, ring=8)
        parts.append(rim)
        parts.append(cylinder(f'hub{sy}', 0.09, 0.09, 0.16, loc=(0, sy * 0.62, 0.5), rot=(math.pi / 2, 0, 0), segs=10, mat=M('iron')))
        for i in range(8):
            a = math.tau * i / 8
            s = cylinder(f'spoke{sy}{i}', 0.028, 0.025, 0.5, loc=(math.cos(a) * 0.25, sy * 0.62, 0.5 + math.sin(a) * 0.25), segs=6, mat=M('planks-worn'))
            aim(s, (math.cos(a), 0, math.sin(a)))
            apply_transform(s)
            parts.append(s)
    for sx in (-1, 1):
        parts.append(cylinder(f'post{sx}', 0.05, 0.04, 1.6, loc=(sx * 0.9, 0.42, 1.55), segs=8, mat=M('planks-worn')))
        parts.append(cylinder(f'post2{sx}', 0.05, 0.04, 1.3, loc=(sx * 0.9, -0.42, 1.4), segs=8, mat=M('planks-worn')))
    canopy = plane('canopy', (2.2, 1.3), loc=(0, 0, 2.25), rot=(0.28, 0, 0))
    canopy.data.materials.append(M('canvas'))
    subdivide(canopy, 3, simple=True)
    displace(canopy, 0.07, 0.4, seed=seed)
    apply_mods(canopy)
    sol = canopy.modifiers.new('solid', 'SOLIDIFY')
    sol.thickness = 0.02
    apply_mods(canopy)
    parts.append(canopy)
    for sx in (-1, 1):
        h = cylinder(f'handle{sx}', 0.04, 0.035, 1.2, loc=(1.5, sx * 0.3, 0.72), segs=8, mat=M('planks-worn'))
        aim(h, (1, 0, -0.1))
        apply_transform(h)
        parts.append(h)
    parts.append(cylinder('leg', 0.04, 0.04, 0.7, loc=(-0.8, 0, 0.35), segs=8, mat=M('planks-worn')))
    parts.append(plank_box('crate', (0.5, 0.5, 0.45), (-0.4, 0, 1.02), 'planks-dark', 0.01, 0.8))
    parts.append(sack('sack', 0.25, (0.5, 0.1, 0.79), seed))
    o = join(parts, 'vendor-cart')
    smooth(o, 40)
    return g(o)


def brick_pile(seed=307):
    rng = rand(seed)
    parts = []
    for row in range(4):
        n = 6 - row
        for i in range(n):
            parts.append(plank_box(f'b{row}{i}', (0.42, 0.2, 0.12), ((i - (n - 1) * 0.5) * 0.44 + rng.uniform(-0.02, 0.02), (row % 2) * 0.05, 0.06 + row * 0.125), 'masonry-city', 0.008, 0.6, rot=(0, 0, rng.uniform(-0.05, 0.05))))
    for i in range(5):
        parts.append(plank_box(f'loose{i}', (0.42, 0.2, 0.12), (rng.uniform(-1.4, 1.4), rng.uniform(-0.6, 0.6), 0.06), 'masonry-city', 0.008, 0.6, rot=(0, 0, rng.uniform(0, 3))))
    o = join(parts, 'brick-pile')
    smooth(o, 40)
    return g(o)


def broken_column(seed=308):
    p = pillar('column', [(0, 0), (0.8, 0), (0.8, 0.3), (0.6, 0.42), (0.52, 0.5), (0.52, 2.6), (0, 2.6)], seed, 'masonry', chips=2)
    boolean(p, box('break', (2.0, 2.0, 1.2), loc=(0.3, 0, 3.0), rot=(0.35, 0.3, 0.2)))
    drum = cylinder('drum', 0.5, 0.5, 1.1, loc=(1.6, 0.4, 0.5), rot=(math.pi / 2, 0, 0.6), segs=22, mat=M('masonry-mossy'))
    box_uv(drum, 1.2)
    o = join([p, drum], 'broken-column')
    smooth(o, 40)
    return g(o)


def scattered_blocks():
    return g(block_pile('scattered-blocks', 309, n=7, smax=0.75, spread=1.3, dust=None))


def roof_debris(seed=310):
    rng = rand(seed)
    parts = []
    for i in range(4):
        parts.append(plank_box(f'beam{i}', (rng.uniform(1.6, 2.4), 0.16, 0.16), (rng.uniform(-0.4, 0.4), rng.uniform(-0.5, 0.5), 0.1 + i * 0.12), 'planks-dark', 0.01, 1.0, rot=(0, rng.uniform(-0.15, 0.15), rng.uniform(0, 3))))
    for i in range(30):
        parts.append(plank_box(f'tile{i}', (0.3, 0.2, 0.025), (rng.uniform(-1.0, 1.0), rng.uniform(-0.9, 0.9), 0.02 + rng.uniform(0, 0.4)), 'clay', 0, 0.5, rot=(rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5), rng.uniform(0, 3))))
    o = join(parts, 'roof-debris')
    clamp_floor(o, 0.0)
    smooth(o, 40)
    return g(o)


def wooden_gutter():
    prof = [(-0.16, 0.2), (-0.16, 0.06), (-0.1, 0.0), (0.1, 0.0), (0.16, 0.06), (0.16, 0.2), (0.12, 0.2), (0.12, 0.08), (0.08, 0.04), (-0.08, 0.04), (-0.12, 0.08), (-0.12, 0.2)]
    gutter = prism('gutter', prof, 3.2, loc=(0, 0, 2.3), mat=M('planks-dark'))
    box_uv(gutter, 0.8)
    parts = [gutter]
    for x in (-1.2, 0, 1.2):
        parts.append(box(f'bracket{x}', (0.06, 0.4, 0.05), loc=(x, 0.3, 2.28), mat=M('iron')))
        parts.append(box(f'brace{x}', (0.05, 0.45, 0.05), loc=(x, 0.35, 2.05), rot=(0.8, 0, 0), mat=M('iron')))
        parts.append(box(f'plate{x}', (0.08, 0.04, 0.5), loc=(x, 0.52, 2.0), mat=M('iron')))
    parts.append(cylinder('spout', 0.08, 0.08, 1.9, loc=(1.5, 0.2, 1.1), segs=10, mat=M('planks-dark')))
    o = join(parts, 'wooden-gutter')
    smooth(o, 40)
    return g(o)


def flag_bracket(seed=311):
    parts = [box('plate', (0.1, 0.04, 0.5), loc=(0, 0, 2.6), mat=M('iron')), cylinder('ring', 0.08, 0.08, 0.1, loc=(0, -0.08, 2.75), rot=(0.7, 0, 0), segs=12, mat=M('iron')),
             cylinder('ring2', 0.08, 0.08, 0.1, loc=(0, -0.08, 2.45), rot=(0.7, 0, 0), segs=12, mat=M('iron'))]
    pole = cylinder('pole', 0.04, 0.03, 2.6, loc=(0, -0.7, 3.5), segs=8, mat=M('planks-worn'))
    aim(pole, (0, -0.75, 1))
    apply_transform(pole)
    parts.append(pole)
    banner = plane('banner', (0.7, 1.8), loc=(0, -1.0, 3.3), rot=(0.65, 0, 0))
    banner.data.materials.append(M('cloth-red'))
    subdivide(banner, 3, simple=True)
    displace(banner, 0.1, 0.35, seed=seed)
    apply_mods(banner)
    sol = banner.modifiers.new('solid', 'SOLIDIFY')
    sol.thickness = 0.015
    apply_mods(banner)
    parts.append(banner)
    o = join(parts, 'flag-bracket')
    smooth(o, 40)
    return g(o)


def street_brazier(seed=312):
    rng = rand(seed)
    bowl = lathe('bowl', [(0, 0.55), (0.3, 0.55), (0.52, 0.75), (0.58, 1.0), (0.6, 1.05), (0.55, 1.05), (0.5, 0.78), (0.3, 0.6), (0, 0.6)], segs=20, mat=M('iron-rust'))
    parts = [bowl, torus('rim', 0.58, 0.03, loc=(0, 0, 1.05), mat=M('iron'), segs=20, ring=6)]
    for i in range(3):
        a = math.tau * i / 3
        l = cylinder(f'leg{i}', 0.035, 0.03, 0.9, loc=(math.cos(a) * 0.3, math.sin(a) * 0.3, 0.42), segs=8, mat=M('iron'))
        aim(l, (math.cos(a) * 0.35, math.sin(a) * 0.35, -1))
        apply_transform(l)
        parts.append(l)
    for i in range(12):
        parts.append(sphere(f'coal{i}', rng.uniform(0.07, 0.13), (rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), 0.75 + rng.uniform(0, 0.15)), subdiv=1, mat=M('ember-glow' if i % 3 == 0 else 'coal')))
    o = join(parts, 'street-brazier')
    smooth(o, 40)
    return g(o)


def road_marker(seed=313):
    stone = lathe('marker', [(0, 0), (0.42, 0), (0.42, 0.15), (0.3, 0.2), (0.28, 1.3), (0.22, 1.45), (0, 1.5)], segs=8, mat=M('masonry'))
    subdivide(stone, 1, simple=True)
    displace(stone, 0.03, 0.2, seed=seed)
    apply_mods(stone)
    box_uv(stone, 1.0)
    for z in (0.6, 0.85, 1.1):
        boolean(stone, box(f'groove{z}', (0.3, 0.03, 0.04), loc=(0, -0.3, z)))
    smooth(stone, 40)
    return g(stone)


if __name__ == '__main__':
    run('structures', [
        ('animal-trough', animal_trough), ('hanging-sign-post', hanging_sign_post), ('wooden-gate', wooden_gate), ('barred-door', barred_door),
        ('shuttered-window', shuttered_window), ('broken-window', broken_window), ('window-frame', window_frame), ('wooden-ladder', wooden_ladder),
        ('stone-stairs', stone_stairs), ('cloth-awning', cloth_awning), ('vendor-cart', vendor_cart), ('brick-pile', brick_pile), ('broken-column', broken_column),
        ('scattered-blocks', scattered_blocks), ('roof-debris', roof_debris), ('wooden-gutter', wooden_gutter), ('flag-bracket', flag_bracket),
        ('street-brazier', street_brazier), ('road-marker', road_marker),
    ])
