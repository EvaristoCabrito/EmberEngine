"""Wave 2, group 4: taverns and interiors (content/models/interior)."""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kitlib import *  # noqa


def g(o):
    ground(o)
    return o


def long_bench():
    return g(bench('long-bench', 2.4))


def drink_shelf():
    s = shelf('drink-shelf', 1.8, 0.35, 1.3, n=2)
    parts = [s]
    rng = rand(401)
    for level in (0.06, 0.68):
        for i in range(6):
            x = -0.7 + i * 0.28 + rng.uniform(-0.03, 0.03)
            parts.append(vessel(f'b{level}{i}', BOTTLE, (x, -0.02, level), 'dark-glass' if i % 3 else 'clay', rng.uniform(0.8, 1.0)))
    o = join(parts, 'drink-shelf')
    smooth(o, 50)
    return g(o)


def bookshelf(seed=402):
    rng = rand(seed)
    s = shelf('bookshelf', 1.4, 0.4, 2.2, n=4)
    parts = [s]
    mats = ['leather', 'cloth-red', 'cloth-dark', 'paper', 'clay']
    for level in range(4):
        z = 0.03 + (2.2 - 0.06) * level / 4 + 0.03
        x = -0.62
        while x < 0.55:
            w = rng.uniform(0.05, 0.1)
            h = rng.uniform(0.25, 0.42)
            lean = 0 if rng.random() < 0.85 else rng.uniform(-0.25, 0.25)
            parts.append(plank_box(f'book{level}{x:.2f}', (w, 0.28, h), (x + w * 0.5, 0.0, z + h * 0.5), rng.choice(mats), 0.005, 1.0, rot=(0, lean, 0)))
            x += w + 0.012
            if rng.random() < 0.12:
                x += rng.uniform(0.05, 0.15)
    o = join(parts, 'bookshelf')
    smooth(o, 40)
    return g(o)


def wooden_mug():
    o = vessel('wooden-mug', MUG, (0, 0, 0), 'planks-dark', 1.0, handle=True)
    smooth(o, 50)
    return g(o)


def wine_bottle():
    o = vessel('wine-bottle', BOTTLE, (0, 0, 0), 'dark-glass', 1.15)
    smooth(o, 50)
    return g(o)


def beer_jug():
    o = vessel('beer-jug', JUG, (0, 0, 0), 'clay', 1.0, handle=True)
    smooth(o, 50)
    return g(o)


def wooden_plate():
    o = vessel('wooden-plate', PLATE, (0, 0, 0), 'planks', 1.0, segs=24)
    box_uv(o, 0.5)
    smooth(o, 50)
    return g(o)


def ceramic_bowl():
    o = vessel('ceramic-bowl', BOWL, (0, 0, 0), 'clay', 1.0, segs=24)
    smooth(o, 50)
    return g(o)


def table_candelabra():
    base = lathe('base', [(0, 0), (0.16, 0), (0.17, 0.02), (0.06, 0.05), (0.025, 0.1), (0.025, 0.4), (0.05, 0.45), (0, 0.45)], segs=14, mat=M('brass'))
    parts = [base]
    for i, (x, z) in enumerate(((-0.18, 0.42), (0, 0.5), (0.18, 0.42))):
        if x:
            arm = kit_extra.arc(f'arm{i}', 0.18, 0.012, math.pi, loc=(0, 0, 0.3), rot=(math.pi / 2, 0, 0 if x > 0 else math.pi), mat=M('brass'), segs=14, ring=6)
            parts.append(arm)
        parts.append(lathe(f'cup{i}', [(0, 0), (0.05, 0), (0.055, 0.03), (0.035, 0.03), (0.035, 0.0), (0, 0.0)], segs=12, loc=(x, 0, z), mat=M('brass')))
        parts.append(cylinder(f'candle{i}', 0.022, 0.022, 0.22, loc=(x, 0, z + 0.13), segs=10, mat=M('wool')))
        parts.append(cylinder(f'wick{i}', 0.004, 0.004, 0.03, loc=(x, 0, z + 0.25), segs=4, mat=M('coal')))
    o = join(parts, 'table-candelabra')
    smooth(o, 40)
    return g(o)


def hanging_chandelier():
    Z = 2.6
    wheel = torus('wheel', 0.6, 0.05, loc=(0, 0, Z), mat=M('planks-dark'), segs=32, ring=8)
    box_uv(wheel, 0.6)
    parts = [wheel, torus('rim', 0.62, 0.015, loc=(0, 0, Z + 0.03), mat=M('iron'), segs=32, ring=6)]
    for i in range(8):
        a = math.tau * i / 8
        x, y = math.cos(a) * 0.6, math.sin(a) * 0.6
        parts.append(lathe(f'cup{i}', [(0, 0), (0.05, 0), (0.055, 0.03), (0.035, 0.03), (0.035, 0.0), (0, 0.0)], segs=10, loc=(x, y, Z + 0.05), mat=M('iron')))
        parts.append(cylinder(f'candle{i}', 0.022, 0.022, 0.2, loc=(x, y, Z + 0.17), segs=8, mat=M('wool')))
    for i in range(3):
        a = math.tau * i / 3
        parts.append(chain(f'chain{i}', (math.cos(a) * 0.6, math.sin(a) * 0.6, Z + 0.02), (0, 0, Z + 1.0), 8, link=0.045))
    parts.append(torus('ring', 0.08, 0.015, loc=(0, 0, Z + 1.05), rot=(math.pi / 2, 0, 0), mat=M('iron'), segs=16, ring=6))
    parts.append(cylinder('hook', 0.02, 0.02, 0.3, loc=(0, 0, Z + 1.25), segs=6, mat=M('iron')))
    o = join(parts, 'hanging-chandelier')
    smooth(o, 40)
    return g(o)


def fireplace_grate():
    parts = []
    for i in range(5):
        parts.append(cylinder(f'bar{i}', 0.02, 0.02, 0.9, loc=(-0.4 + i * 0.2, 0, 0.18), rot=(math.pi / 2, 0, 0), segs=6, mat=M('iron')))
    for y in (-0.4, 0.4):
        parts.append(cylinder(f'cross{y}', 0.025, 0.025, 1.0, loc=(0, y, 0.18), rot=(0, math.pi / 2, 0), segs=6, mat=M('iron')))
        for x in (-0.45, 0.45):
            parts.append(cylinder(f'leg{x}{y}', 0.025, 0.02, 0.18, loc=(x, y, 0.09), segs=6, mat=M('iron')))
            parts.append(cylinder(f'post{x}{y}', 0.02, 0.015, 0.25, loc=(x, y, 0.3), segs=6, mat=M('iron')))
            parts.append(sphere(f'knob{x}{y}', 0.035, (x, y, 0.43), subdiv=1, mat=M('iron')))
    rng = rand(403)
    for i in range(3):
        l = cylinder(f'log{i}', 0.09, 0.08, 0.8, loc=(0, -0.15 + i * 0.15, 0.3 + i * 0.1), rot=(0, math.pi / 2, rng.uniform(-0.15, 0.15)), segs=8, mat=M('bark'))
        parts.append(l)
    o = join(parts, 'fireplace-grate')
    vertex_colors(o, (0.25, 0.18, 0.12), 0.35, 0.3, 403)
    smooth(o, 50)
    return g(o)


def barrel_rack():
    parts = []
    for z in (0.12, 0.2):
        pass
    for sy in (-1, 1):
        parts.append(plank_box(f'rail{sy}', (2.6, 0.12, 0.12), (0, sy * 0.45, 0.06), 'planks-dark', 0.01, 1.0))
    for x in (-1.2, 0, 1.2):
        parts.append(plank_box(f'cross{x}', (0.12, 1.1, 0.12), (x, 0, 0.06), 'planks-dark', 0.01, 1.0))
        parts.append(plank_box(f'chock{x}', (0.12, 0.3, 0.2), (x, 0, 0.2), 'planks-dark', 0.01, 1.0))
    for i, x in enumerate((-0.6, 0.6)):
        b = barrel(f'barrel{i}', H=1.2, R=0.45, hoops=4)
        b.rotation_euler = (0, math.pi / 2, 0)
        b.location = (x - 0.6, 0, 0.6)
        apply_transform(b)
        parts.append(b)
    o = join(parts, 'barrel-rack')
    smooth(o, 40)
    return g(o)


def kitchen_cabinet():
    W, D, H = 1.3, 0.6, 1.5
    body = plank_box('body', (W, D, H), (0, 0, H * 0.5), 'planks-worn', 0.015, 1.2)
    parts = [body, plank_box('top', (W + 0.08, D + 0.08, 0.06), (0, 0, H + 0.03), 'planks-dark', 0.015, 1.2)]
    for sx in (-1, 1):
        parts.append(plank_box(f'door{sx}', (W * 0.46, 0.04, H * 0.55), (sx * W * 0.25, -D * 0.5 - 0.02, H * 0.32), 'planks-dark', 0.01, 1.0, (0.3 * sx, 0.2)))
        parts.append(sphere(f'knob{sx}', 0.03, (sx * 0.08, -D * 0.5 - 0.05, H * 0.32), subdiv=1, mat=M('brass')))
        parts.append(plank_box(f'drawer{sx}', (W * 0.46, 0.04, H * 0.2), (sx * W * 0.25, -D * 0.5 - 0.02, H * 0.75), 'planks-dark', 0.01, 1.0))
        parts.append(box(f'pull{sx}', (0.12, 0.03, 0.03), loc=(sx * W * 0.25, -D * 0.5 - 0.05, H * 0.75), mat=M('brass')))
    o = join(parts, 'kitchen-cabinet')
    smooth(o, 40)
    return g(o)


def pantry_shelf(seed=404):
    rng = rand(seed)
    s = shelf('pantry', 1.6, 0.45, 1.8, n=3)
    parts = [s]
    for level in range(3):
        z = 0.06 + (1.8 - 0.06) * level / 3
        for i in range(4):
            x = -0.6 + i * 0.4 + rng.uniform(-0.05, 0.05)
            k = rng.choice(('jar', 'sack', 'bowl'))
            if k == 'jar':
                parts.append(vessel(f'jar{level}{i}', JAR, (x, 0, z), 'clay', rng.uniform(0.35, 0.5)))
            elif k == 'sack':
                parts.append(sack(f'sack{level}{i}', 0.14, (x, 0, z), seed + level * 10 + i, tie=False))
            else:
                parts.append(vessel(f'bowl{level}{i}', BOWL, (x, 0, z), 'clay', rng.uniform(0.8, 1.1)))
    o = join(parts, 'pantry-shelf')
    smooth(o, 50)
    return g(o)


def cauldron():
    pot = lathe('pot', [(0, 0.25), (0.3, 0.25), (0.5, 0.45), (0.52, 0.8), (0.46, 0.95), (0.5, 1.0), (0.44, 1.0), (0.42, 0.95), (0.47, 0.8), (0.45, 0.45), (0.28, 0.3), (0, 0.3)], segs=24, mat=M('iron'))
    parts = [pot, torus('rim', 0.48, 0.03, loc=(0, 0, 1.0), mat=M('iron'), segs=24, ring=6)]
    for i in range(3):
        a = math.tau * i / 3
        parts.append(cylinder(f'leg{i}', 0.04, 0.035, 0.3, loc=(math.cos(a) * 0.3, math.sin(a) * 0.3, 0.15), segs=8, mat=M('iron')))
    handle = kit_extra.arc('handle', 0.5, 0.02, math.pi, loc=(0, 0, 0.98), rot=(math.pi / 2, 0, math.pi / 2), mat=M('iron'), segs=20, ring=6)
    parts.append(handle)
    parts.append(cylinder('broth', 0.42, 0.42, 0.02, loc=(0, 0, 0.9), segs=24, mat=M('dark-glass')))
    o = join(parts, 'cauldron')
    smooth(o, 40)
    return g(o)


def iron_pot():
    pot = lathe('pot', [(0, 0), (0.25, 0), (0.3, 0.05), (0.31, 0.3), (0.33, 0.33), (0.29, 0.33), (0.27, 0.3), (0.26, 0.05), (0, 0.04)], segs=22, mat=M('iron'))
    lid = lathe('lid', [(0, 0.33), (0.3, 0.33), (0.2, 0.4), (0.05, 0.42), (0.05, 0.46), (0, 0.46)], segs=22, mat=M('iron'))
    parts = [pot, lid]
    for sx in (-1, 1):
        parts.append(kit_extra.arc(f'ear{sx}', 0.05, 0.012, math.pi, loc=(sx * 0.31, 0, 0.26), rot=(math.pi / 2, 0, 0), mat=M('iron'), segs=10, ring=5))
    o = join(parts, 'iron-pot')
    smooth(o, 40)
    return g(o)


def utensils_set():
    board = plank_box('board', (0.8, 0.5, 0.04), (0, 0, 0.02), 'planks', 0.01, 0.6)
    parts = [board, cylinder('ladle', 0.012, 0.012, 0.4, loc=(-0.2, 0.1, 0.05), rot=(0, math.pi / 2, 0.2), segs=6, mat=M('planks-dark')),
             sphere('ladle-bowl', 0.06, (0.0, 0.14, 0.06), subdiv=2, mat=M('iron'), scale=(1, 1, 0.5)),
             box('knife', (0.25, 0.035, 0.008), loc=(0.1, -0.1, 0.05), rot=(0, 0, -0.2), mat=M('silver')), cylinder('knife-grip', 0.015, 0.015, 0.1, loc=(-0.07, -0.07, 0.05), rot=(0, math.pi / 2, -0.2), segs=6, mat=M('planks-dark')),
             cylinder('spoon', 0.008, 0.008, 0.25, loc=(0.2, 0.05, 0.05), rot=(0, math.pi / 2, 0.5), segs=5, mat=M('planks-dark')), sphere('spoon-bowl', 0.035, (0.3, 0.1, 0.055), subdiv=2, mat=M('planks-dark'), scale=(1, 1, 0.4)),
             vessel('bowl', BOWL, (0.3, -0.15, 0.04), 'clay', 0.6), cylinder('rolling', 0.035, 0.035, 0.4, loc=(-0.1, -0.18, 0.075), rot=(0, math.pi / 2, 0.1), segs=10, mat=M('planks'))]
    o = join(parts, 'utensils-set')
    smooth(o, 40)
    return g(o)


def worn_rug(seed=405):
    rug = plane('rug', (2.2, 1.4), loc=(0, 0, 0.015))
    rug.data.materials.append(M('cloth-red'))
    subdivide(rug, 3, simple=True)
    displace(rug, 0.025, 0.3, seed=seed)
    apply_mods(rug)
    sol = rug.modifiers.new('solid', 'SOLIDIFY')
    sol.thickness = 0.02
    apply_mods(rug)
    parts = [rug, plank_box('border', (2.2, 0.12, 0.012), (0, 0.64, 0.02), 'cloth-dark', 0, 1.0), plank_box('border2', (2.2, 0.12, 0.012), (0, -0.64, 0.02), 'cloth-dark', 0, 1.0),
             plank_box('border3', (0.12, 1.4, 0.012), (1.04, 0, 0.02), 'cloth-dark', 0, 1.0), plank_box('border4', (0.12, 1.4, 0.012), (-1.04, 0, 0.02), 'cloth-dark', 0, 1.0)]
    for i in range(14):
        for sx in (-1, 1):
            parts.append(cylinder(f'fringe{i}{sx}', 0.008, 0.008, 0.1, loc=(sx * 1.15, -0.63 + i * 0.097, 0.01), rot=(0, math.pi / 2, 0), segs=4, mat=M('wool')))
    o = join(parts, 'worn-rug')
    smooth(o, 60)
    return g(o)


def cloth_curtain(seed=406):
    rod = cylinder('rod', 0.03, 0.03, 2.4, loc=(0, 0, 2.6), rot=(0, math.pi / 2, 0), segs=10, mat=M('iron'))
    parts = [rod, sphere('finialL', 0.05, (-1.2, 0, 2.6), subdiv=1, mat=M('brass')), sphere('finialR', 0.05, (1.2, 0, 2.6), subdiv=1, mat=M('brass'))]
    for sx in (-1, 1):
        parts.append(box(f'bracket{sx}', (0.05, 0.2, 0.05), loc=(sx * 1.0, 0.1, 2.6), mat=M('iron')))
    cur = plane('curtain', (2.1, 2.5), loc=(0, -0.04, 1.33), rot=(math.pi / 2, 0, 0))
    cur.data.materials.append(M('cloth-dark'))
    subdivide(cur, 4, simple=True)
    wave = bpy.data.textures.new('wave', type='WOOD')
    wave.noise_scale = 0.22
    wave.wood_type = 'BANDS'
    md = cur.modifiers.new('folds', 'DISPLACE')
    md.texture = wave
    md.strength = 0.16
    md.direction = 'Y'
    md.texture_coords = 'GLOBAL'
    apply_mods(cur)
    sol = cur.modifiers.new('solid', 'SOLIDIFY')
    sol.thickness = 0.015
    apply_mods(cur)
    parts.append(cur)
    for i in range(9):
        parts.append(torus(f'ring{i}', 0.05, 0.008, loc=(-1.0 + i * 0.25, 0, 2.6), rot=(0, math.pi / 2, 0), mat=M('brass'), segs=12, ring=5))
    o = join(parts, 'cloth-curtain')
    smooth(o, 60)
    return g(o)


def simple_bed():
    L, W, H = 2.6, 1.3, 0.5
    parts = [plank_box('frame-l', (L, 0.08, 0.3), (0, -W * 0.5, 0.35), 'planks-dark', 0.01, 1.0), plank_box('frame-r', (L, 0.08, 0.3), (0, W * 0.5, 0.35), 'planks-dark', 0.01, 1.0),
             plank_box('head', (0.08, W + 0.08, 1.1), (-L * 0.5, 0, 0.55), 'planks-dark', 0.01, 1.0), plank_box('foot', (0.08, W + 0.08, 0.7), (L * 0.5, 0, 0.35), 'planks-dark', 0.01, 1.0)]
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(plank_box(f'leg{sx}{sy}', (0.1, 0.1, 0.35), (sx * (L * 0.5 - 0.1), sy * (W * 0.5 - 0.02), 0.175), 'planks-dark', 0.01, 0.8))
    mat = plank_box('mattress', (L - 0.1, W - 0.1, 0.22), (0, 0, 0.6), 'wool', 0.06, 1.2)
    blanket = plank_box('blanket', (L * 0.6, W + 0.08, 0.1), (L * 0.18, 0, 0.73), 'cloth-red', 0.04, 1.2)
    pillow = sphere('pillow', 0.3, (-L * 0.5 + 0.45, 0, 0.78), subdiv=3, mat=M('wool'), scale=(1.0, 1.6, 0.45))
    o = join(parts + [mat, blanket, pillow], 'simple-bed')
    smooth(o, 40)
    return g(o)


def stool():
    seat = cylinder('seat', 0.3, 0.3, 0.06, loc=(0, 0, 0.6), segs=18, mat=M('planks-worn'))
    box_uv(seat, 0.6)
    parts = [seat]
    for i in range(3):
        a = math.tau * i / 3
        l = cylinder(f'leg{i}', 0.035, 0.03, 0.62, loc=(math.cos(a) * 0.22, math.sin(a) * 0.22, 0.3), segs=8, mat=M('planks-dark'))
        aim(l, (math.cos(a) * 0.15, math.sin(a) * 0.15, -1))
        apply_transform(l)
        parts.append(l)
    o = join(parts, 'stool')
    smooth(o, 40)
    return g(o)


def folding_screen():
    parts = []
    for i, (x, a) in enumerate(((-0.78, 0.35), (0, -0.35), (0.78, 0.35))):
        f = [plank_box(f'f{i}l', (0.06, 0.06, 1.9), (-0.38, 0, 0.95), 'planks-dark', 0.005, 0.8), plank_box(f'f{i}r', (0.06, 0.06, 1.9), (0.38, 0, 0.95), 'planks-dark', 0.005, 0.8),
             plank_box(f'f{i}t', (0.8, 0.06, 0.06), (0, 0, 1.87), 'planks-dark', 0.005, 0.8), plank_box(f'f{i}b', (0.8, 0.06, 0.06), (0, 0, 0.09), 'planks-dark', 0.005, 0.8),
             plank_box(f'f{i}c', (0.72, 0.015, 1.72), (0, 0, 0.98), 'cloth-red', 0, 1.0)]
        panel = join(f, f'panel{i}')
        panel.rotation_euler = (0, 0, a)
        panel.location = (x, 0, 0)
        apply_transform(panel)
        parts.append(panel)
    o = join(parts, 'folding-screen')
    smooth(o, 40)
    return g(o)


if __name__ == '__main__':
    run('interior', [
        ('long-bench', long_bench), ('drink-shelf', drink_shelf), ('bookshelf', bookshelf), ('wooden-mug', wooden_mug), ('wine-bottle', wine_bottle), ('beer-jug', beer_jug),
        ('wooden-plate', wooden_plate), ('ceramic-bowl', ceramic_bowl), ('table-candelabra', table_candelabra), ('hanging-chandelier', hanging_chandelier),
        ('fireplace-grate', fireplace_grate), ('barrel-rack', barrel_rack), ('kitchen-cabinet', kitchen_cabinet), ('pantry-shelf', pantry_shelf), ('cauldron', cauldron),
        ('iron-pot', iron_pot), ('utensils-set', utensils_set), ('worn-rug', worn_rug), ('cloth-curtain', cloth_curtain), ('simple-bed', simple_bed), ('stool', stool),
        ('folding-screen', folding_screen),
    ])
