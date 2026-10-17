"""Wave 2, group 1: nature and forests (content/models/nature)."""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kitlib import *  # noqa


def g(o):
    ground(o)
    return o


def ancient_tree_roots():
    return g(tree('ancient-tree', 101, roots=9, root_r=0.42, root_len=2.4, trunk_h=4.2))


def hollow_standing_tree():
    return g(tree('hollow-tree', 102, canopy=False, hollow=True, dead=True, bark=DEAD, trunk_h=3.4))


def rotten_fallen_log(seed=103):
    rng = rand(seed)
    l = log('log', 4.4, 0.5, seed)
    for i in range(4):
        hole = sphere(f'hole{i}', rng.uniform(0.25, 0.45), (rng.uniform(-1.6, 1.6), rng.uniform(-0.3, 0.3), rng.uniform(0.5, 1.0)), subdiv=2)
        boolean(l, hole)
    colour_bark(l, seed, base=(0.17, 0.13, 0.1), top='moss')
    parts = [l]
    for i in range(3):
        m, _ = kit_extra._mushroom(f'fungus{i}', rng.uniform(-1.5, 1.5), 0.45, rng.uniform(0.1, 0.18), rng.uniform(0.15, 0.3), rng)
        m.rotation_euler = (-1.2, 0, 0)
        m.location.z = rng.uniform(0.3, 0.7)
        apply_transform(m)
        parts.append(m)
    o = join(parts, 'rotten-fallen-log')
    vertex_colors(o, (0.17, 0.13, 0.1), 0.4, 0.4, seed, select=lambda c, n: (0.62, 0.45, 0.28) if c.y > 0.42 else None)
    return g(o)


def rotten_stump(seed=104):
    rng = rand(seed)
    bark = M('bark')
    body = cylinder('stump', 0.62, 0.52, 1.05, loc=(0, 0, 0.525), segs=18, mat=bark)
    subdivide(body, 2, simple=True)
    displace(body, 0.12, 0.3, seed=seed)
    apply_mods(body)
    hole = cylinder('hole', 0.38, 0.3, 0.8, loc=(0, 0, 0.9), segs=12)
    displace(hole, 0.08, 0.2, seed=seed + 1)
    apply_mods(hole)
    boolean(body, hole)
    parts = [body]
    kit_forest._roots(parts, rng, 5, 0.24, 1.1, bark, z=0.22)
    o = join(parts, 'rotten-stump')
    vertex_colors(o, (0.19, 0.14, 0.1), 0.45, 0.35, seed, gradient=(0, 0.9, 0.7))
    set_material_where(o, M('moss'), lambda c, n: (c.z < 0.3 and noise.noise(c * 2.5) > 0.05) or (n.z > 0.8 and c.z > 0.9))
    box_uv(o, 1.0)
    smooth(o, 55)
    return g(o)


def dry_branch_pile():
    return g(branch_pile('branches', 105, n=14))


def exposed_roots(seed=106):
    rng = rand(seed)
    parts = []
    for i in range(7):
        a = rng.uniform(0, math.tau)
        R = rng.uniform(0.5, 1.1)
        r = kit_extra.arc(f'root{i}', R, rng.uniform(0.07, 0.15), math.pi * rng.uniform(0.6, 1.0), loc=(rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5), -0.05),
                          rot=(math.pi / 2 + rng.uniform(-0.3, 0.3), 0, a), mat=M('bark'), segs=16, ring=8)
        subdivide(r, 1, simple=True)
        displace(r, 0.04, 0.2, seed=seed + i)
        apply_mods(r)
        parts.append(r)
    soil = cylinder('soil', 1.2, 1.05, 0.1, loc=(0, 0, 0.05), segs=18, mat=M('forest-floor'))
    displace(soil, 0.08, 0.3, seed=seed + 20)
    apply_mods(soil)
    box_uv(soil, 1.4)
    parts.append(soil)
    o = join(parts, 'exposed-roots')
    clamp_floor(o, 0.0)
    vertex_colors(o, BARK, 0.4, 0.3, seed)
    smooth(o, 55)
    return g(o)


def root_arch(seed=107):
    rng = rand(seed)
    big = kit_extra.arc('arch', 1.4, 0.22, math.pi, loc=(0, 0, 0), rot=(math.pi / 2, 0, 0), mat=M('bark'), segs=24, ring=10)
    subdivide(big, 1, simple=True)
    displace(big, 0.07, 0.3, seed=seed)
    apply_mods(big)
    parts = [big]
    for i in range(5):
        t = rng.uniform(0.1, 0.9)
        a = math.pi * t
        p = Vector((1.4 * math.cos(a), 0, 1.4 * math.sin(a)))
        d = Vector((rng.uniform(-0.5, 0.5), rng.choice((-1, 1)), rng.uniform(-0.8, -0.2))).normalized()
        c = cylinder(f'tendril{i}', 0.08, 0.02, rng.uniform(0.8, 1.6), loc=tuple(p + d * 0.6), segs=8, mat=M('bark'))
        aim(c, d)
        apply_transform(c)
        parts.append(c)
    for sx in (-1, 1):
        for k in range(3):
            a = rng.uniform(0, math.tau)
            c = cylinder(f'foot{sx}{k}', 0.14, 0.03, 0.9, loc=(sx * 1.4 + math.cos(a) * 0.35, math.sin(a) * 0.35, 0.08), segs=8, mat=M('bark'))
            aim(c, (math.cos(a), math.sin(a), -0.25))
            apply_transform(c)
            parts.append(c)
    o = join(parts, 'root-arch')
    clamp_floor(o, 0.0)
    vertex_colors(o, BARK, 0.4, 0.35, seed, gradient=(0, 1.0, 0.75))
    set_material_by_normal(o, M('moss'), up=0.75, noise_scale=0.5, seed=seed, threshold=0.5)
    box_uv(o, 1.0)
    smooth(o, 55)
    return g(o)


def cracked_boulder(seed=108):
    b = rock('boulder', (2.7, 2.1, 2.0), seed, 'stone', top='moss', subdiv=4, rough=0.3)
    crack = box('crack', (0.07, 3.2, 1.6), loc=(0.2, 0, 1.6), rot=(0.15, 0.1, 0.5))
    boolean(b, crack)
    smooth(b, 50)
    return g(b)


def lichen_rocks():
    return g(rock_cluster('lichen-rocks', 109, n=6, rmax=0.5, mat='stone', top='lichen'))


def ferns(seed=110):
    rng = rand(seed)
    fronds = []
    for i in range(9):
        a = math.tau * i / 9 + rng.uniform(-0.3, 0.3)
        tilt = rng.uniform(-0.9, -0.45)
        f = plane(f'frond{i}', (rng.uniform(0.9, 1.3), 0.3), loc=(math.cos(a) * 0.3, math.sin(a) * 0.3, 0.25), rot=(0, tilt, a))
        f.data.materials.append(M('leaves'))
        subdivide(f, 2, simple=True)
        displace(f, 0.12, 0.25, seed=seed + i)
        apply_mods(f)
        fronds.append(f)
    o = join(fronds, 'ferns')
    vertex_colors(o, (0.18, 0.4, 0.13), 0.4, 0.3, seed)
    smooth(o, 80)
    return g(o)


def dry_bush(seed=111):
    rng = rand(seed)
    parts = []
    for i in range(34):
        a = rng.uniform(0, math.tau)
        el = rng.uniform(0.2, 1.3)
        d = Vector((math.cos(a) * math.cos(el), math.sin(a) * math.cos(el), math.sin(el)))
        L = rng.uniform(0.6, 1.3)
        c = cylinder(f'twig{i}', 0.035, 0.008, L, loc=tuple(d * L * 0.5 + Vector((0, 0, 0.05))), segs=5, mat=M('bark'))
        aim(c, d)
        apply_transform(c)
        parts.append(c)
        for k in range(2):
            d2 = (d + Vector((rng.uniform(-0.7, 0.7), rng.uniform(-0.7, 0.7), rng.uniform(0, 0.5)))).normalized()
            base = d * L * rng.uniform(0.4, 0.9) + Vector((0, 0, 0.05))
            t = cylinder(f'twig{i}{k}', 0.015, 0.004, L * 0.45, loc=tuple(base + d2 * L * 0.22), segs=4, mat=M('bark'))
            aim(t, d2)
            apply_transform(t)
            parts.append(t)
    o = join(parts, 'dry-bush')
    vertex_colors(o, (0.32, 0.25, 0.16), 0.35, 0.3, seed)
    smooth(o, 70)
    return g(o)


def dense_bush(seed=112):
    rng = rand(seed)
    parts = []
    for i in range(6):
        r = rng.uniform(0.55, 0.85)
        s = sphere(f'leaf{i}', r, (rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5), r * 0.8 + rng.uniform(0, 0.3)), subdiv=3, mat=M('leaves'), scale=(1, 1, 0.85))
        displace(s, 0.22, 0.35, seed=seed + i)
        apply_mods(s)
        parts.append(s)
    o = join(parts, 'dense-bush')
    clamp_floor(o, 0.0)
    vertex_colors(o, (0.12, 0.3, 0.1), 0.5, 0.35, seed, gradient=(0, 1.2, 0.5))
    smooth(o, 70)
    return g(o)


def wild_mushrooms(seed=113):
    rng = rand(seed)
    parts = []
    for i, (x, y) in enumerate([(0, 0), (0.3, 0.12), (-0.25, 0.2), (0.1, -0.3), (-0.15, -0.2)]):
        m, _ = kit_extra._mushroom(f'm{i}', x, y, rng.uniform(0.1, 0.2), rng.uniform(0.18, 0.4), rng)
        parts.append(m)
    o = join(parts, 'wild-mushrooms')
    vertex_colors(o, (0.6, 0.42, 0.26), 0.3, 0.2, seed, select=lambda c, n: (0.55, 0.2, 0.12) if n.z > 0.5 and c.z > 0.15 else None)
    smooth(o, 60)
    return g(o)


def roots_with_mushrooms(seed=114):
    rng = rand(seed)
    parts = []
    for i in range(4):
        a = rng.uniform(0, math.tau)
        r = kit_extra.arc(f'root{i}', rng.uniform(0.5, 0.9), rng.uniform(0.08, 0.14), math.pi * 0.9, loc=(rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), -0.05),
                          rot=(math.pi / 2, 0, a), mat=M('bark'), segs=14, ring=8)
        parts.append(r)
    roots = join(parts, 'roots')
    clamp_floor(roots, 0.0)
    vertex_colors(roots, BARK, 0.4, 0.3, seed)
    smooth(roots, 55)
    shrooms = []
    for i in range(4):
        m, _ = kit_extra._mushroom(f'm{i}', rng.uniform(-0.6, 0.6), rng.uniform(-0.6, 0.6), rng.uniform(0.12, 0.22), rng.uniform(0.2, 0.45), rng)
        shrooms.append(m)
    s = join(shrooms, 'shrooms')
    vertex_colors(s, (0.7, 0.6, 0.45), 0.3, 0.2, seed, select=lambda c, n: (0.45, 0.3, 0.2) if n.z > 0.5 and c.z > 0.15 else None)
    smooth(s, 60)
    return g(join([roots, s], 'roots-with-mushrooms'))


def leaf_litter(seed=115):
    rng = rand(seed)
    patch = cylinder('patch', 1.1, 0.95, 0.08, loc=(0, 0, 0.04), segs=20, mat=M('forest-floor'))
    displace(patch, 0.07, 0.3, seed=seed)
    apply_mods(patch)
    box_uv(patch, 1.3)
    parts = [patch]
    for i in range(7):
        a = rng.uniform(0, math.tau)
        c = cylinder(f'twig{i}', 0.03, 0.015, rng.uniform(0.5, 1.1), loc=(rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5), 0.1), segs=5, mat=M('bark'))
        aim(c, (math.cos(a), math.sin(a), 0.05))
        apply_transform(c)
        parts.append(c)
    o = join(parts, 'leaf-litter')
    vertex_colors(o, DEAD, 0.3, 0.3, seed)
    smooth(o, 55)
    return g(o)


def axe_cut_stump(seed=116):
    rng = rand(seed)
    bark = M('bark')
    body = cylinder('stump', 0.58, 0.5, 0.75, loc=(0, 0, 0.375), segs=18, mat=bark)
    subdivide(body, 2, simple=True)
    displace(body, 0.1, 0.3, seed=seed)
    apply_mods(body)
    for i in range(3):
        a = rng.uniform(0, math.tau)
        w = box(f'cut{i}', (0.5, 0.25, 0.3), loc=(math.cos(a) * 0.45, math.sin(a) * 0.45, 0.8), rot=(0.5, 0, a))
        boolean(body, w)
    parts = [body]
    kit_forest._roots(parts, rng, 5, 0.2, 1.0, bark, z=0.18)
    o = join(parts, 'axe-cut-stump')
    vertex_colors(o, BARK, 0.4, 0.4, seed, gradient=(0, 0.7, 0.7))
    set_material_where(o, M('planks-worn'), lambda c, n: n.z > 0.7 and c.z > 0.4)
    box_uv(o, 0.9)
    smooth(o, 55)
    return g(o)


def abandoned_nest(seed=117):
    rng = rand(seed)
    stub = cylinder('branch', 0.12, 0.06, 1.4, loc=(0, 0, 0.6), segs=8, mat=M('bark'))
    aim(stub, (0.3, 0.1, 1))
    apply_transform(stub)
    nest = torus('nest', 0.3, 0.12, loc=(0.4, 0.15, 1.3), mat=M('bark'), segs=20, ring=10)
    subdivide(nest, 1, simple=True)
    displace(nest, 0.07, 0.12, seed=seed)
    apply_mods(nest)
    bowl = cylinder('bowl', 0.22, 0.18, 0.08, loc=(0.4, 0.15, 1.26), segs=14, mat=M('forest-floor'))
    box_uv(bowl, 0.8)
    parts = [stub, nest, bowl]
    for i in range(10):
        a = rng.uniform(0, math.tau)
        t = cylinder(f'twig{i}', 0.012, 0.006, 0.45, loc=(0.4 + math.cos(a) * 0.3, 0.15 + math.sin(a) * 0.3, 1.3 + rng.uniform(-0.05, 0.08)), segs=4, mat=M('bark'))
        aim(t, (math.cos(a + 1.2), math.sin(a + 1.2), rng.uniform(-0.2, 0.2)))
        apply_transform(t)
        parts.append(t)
    o = join(parts, 'abandoned-nest')
    vertex_colors(o, (0.36, 0.28, 0.18), 0.35, 0.2, seed)
    smooth(o, 60)
    return g(o)


def log_over_stream(seed=118):
    l = log('log', 7.0, 0.45, seed, taper=0.75)
    l.location.z += 0.5
    apply_transform(l)
    colour_bark(l, seed, top='moss')
    parts = [l, rock('rockA', (1.3, 1.1, 0.9), seed + 1, 'stone', top='moss', loc=(-3.0, 0.1, 0)), rock('rockB', (1.2, 1.0, 0.8), seed + 2, 'stone', loc=(3.0, -0.1, 0))]
    o = join(parts, 'log-over-stream')
    return g(o)


def uprooted_tree(seed=119):
    rng = rand(seed)
    bark = M('bark')
    trunk = log('trunk', 5.5, 0.55, seed, taper=0.6)
    trunk.location.z += 0.2
    apply_transform(trunk)
    parts = [trunk]
    for i in range(10):
        a = math.tau * i / 10 + rng.uniform(-0.2, 0.2)
        d = Vector((-0.35, math.cos(a), math.sin(a))).normalized()
        c = cylinder(f'root{i}', 0.2, 0.04, rng.uniform(1.0, 1.7), loc=tuple(Vector((-2.75, 0, 0.75)) + d * 0.7), segs=8, mat=bark)
        aim(c, d)
        apply_transform(c)
        parts.append(c)
    disc = sphere('soil', 1.0, (-2.9, 0, 0.75), subdiv=3, mat=M('forest-floor'), scale=(0.3, 1.3, 1.3))
    displace(disc, 0.2, 0.4, seed=seed + 3)
    apply_mods(disc)
    box_uv(disc, 1.3)
    parts.append(disc)
    for i in range(3):
        a = rng.uniform(-0.6, 0.6)
        d = Vector((0.3, math.sin(a), 1)).normalized()
        c = cylinder(f'branch{i}', 0.16, 0.05, rng.uniform(1.2, 2.0), loc=tuple(Vector((0.5 + i * 0.9, 0, 0.9)) + d * 0.8), segs=8, mat=bark)
        aim(c, d)
        apply_transform(c)
        parts.append(c)
    o = join(parts, 'uprooted-tree')
    clamp_floor(o, 0.0)
    vertex_colors(o, BARK, 0.4, 0.4, seed)
    set_material_where(o, M('planks-worn'), lambda c, n: n.x > 0.85 and c.x > 2.5)
    box_uv(o, 1.0)
    smooth(o, 55)
    return g(o)


if __name__ == '__main__':
    run('nature', [
        ('ancient-tree-roots', ancient_tree_roots), ('hollow-standing-tree', hollow_standing_tree), ('rotten-fallen-log', rotten_fallen_log),
        ('rotten-stump', rotten_stump), ('dry-branch-pile', dry_branch_pile), ('exposed-roots', exposed_roots), ('root-arch', root_arch),
        ('cracked-boulder', cracked_boulder), ('lichen-rocks', lichen_rocks), ('ferns', ferns), ('dry-bush', dry_bush), ('dense-bush', dense_bush),
        ('wild-mushrooms', wild_mushrooms), ('roots-with-mushrooms', roots_with_mushrooms), ('leaf-litter', leaf_litter), ('axe-cut-stump', axe_cut_stump),
        ('abandoned-nest', abandoned_nest), ('log-over-stream', log_over_stream), ('uprooted-tree', uprooted_tree),
    ])
