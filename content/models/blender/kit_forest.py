"""Kit A: forest, rocks and natural decoration (content/models/forest)."""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emberlib import *  # noqa

BARK = (0.30, 0.22, 0.15)
DEAD_BARK = (0.21, 0.17, 0.14)
LEAF = (0.16, 0.34, 0.12)


def rock_cluster(seed=1):
    rng = rand(seed)
    parts = []
    spots = [(0, 0, 0.42), (0.55, 0.25, 0.3), (-0.5, 0.3, 0.26), (0.15, -0.55, 0.3), (-0.3, -0.35, 0.2)]
    for i, (x, y, r) in enumerate(spots):
        s = sphere(f'rock{i}', r, (x, y, r * 0.65), subdiv=3, mat=M('granite'),
                   scale=(1 + rng.uniform(-0.15, 0.25), 1 + rng.uniform(-0.15, 0.25), 0.8))
        s.rotation_euler = (rng.uniform(0, 0.4), rng.uniform(0, 0.4), rng.uniform(0, 6.28))
        apply_transform(s)
        displace(s, 0.22 * r / 0.3, 0.45, seed=seed * 10 + i)
        displace(s, 0.05, 0.12, seed=seed * 10 + i + 5)
        apply_mods(s)
        box_uv(s, 1.1)
        parts.append(s)
    o = join(parts, 'rock-cluster')
    clamp_floor(o, 0.0)
    smooth(o, 45)
    ground(o)
    return o


def moss_boulder(seed=2, snow=False):
    b = sphere('boulder', 1.0, (0, 0, 0.75), subdiv=4, mat=M('stone'), scale=(1.35, 1.05, 0.9))
    displace(b, 0.38, 0.95, seed=seed)
    displace(b, 0.07, 0.18, seed=seed + 1)
    apply_mods(b)
    clamp_floor(b, 0.0)
    box_uv(b, 1.6)
    set_material_by_normal(b, M('snow' if snow else 'moss'), up=0.5 if snow else 0.42, noise_scale=0.6, seed=seed, threshold=0.5)
    smooth(b, 50)
    ground(b)
    return b


def _branch(parts, rng, base, direction, length, r1, r2, depth, mat, twist=0.5, children=3, min_len=0.5):
    d = Vector(direction).normalized()
    c = cylinder(f'branch{len(parts)}', r1, r2, length, loc=tuple(Vector(base) + d * length * 0.5), segs=10, mat=mat, caps=True)
    aim(c, d)
    apply_transform(c)
    parts.append(c)
    tip = Vector(base) + d * length
    if depth <= 0 or length < min_len:
        return tip
    for k in range(children):
        t = rng.uniform(0.55, 0.95)
        at = Vector(base) + d * length * t
        # a new direction: tilted away from the parent, twisted around it, drooping a little with depth
        side = d.cross(Vector((0, 0, 1)))
        if side.length < 1e-3:
            side = Vector((1, 0, 0))
        side.normalize()
        up = side.cross(d).normalized()
        ang = rng.uniform(0.45, 0.9) * twist + 0.3
        rot = rng.uniform(0, math.tau) + k * math.tau / children
        nd = (d * math.cos(ang) + (side * math.cos(rot) + up * math.sin(rot)) * math.sin(ang)).normalized()
        nd.z += rng.uniform(0.05, 0.25) - 0.08 * (3 - depth)
        _branch(parts, rng, at, nd, length * rng.uniform(0.5, 0.72), r2 * 1.1, r2 * 0.35, depth - 1, mat, twist, children, min_len)
    return tip


def _roots(parts, rng, n, r, length, mat, z=0.25):
    for i in range(n):
        a = math.tau * i / n + rng.uniform(-0.3, 0.3)
        d = Vector((math.cos(a), math.sin(a), -0.28)).normalized()
        c = cylinder(f'root{i}', r, r * 0.2, length, loc=tuple(Vector((0, 0, z)) + d * length * 0.45), segs=8, mat=mat)
        aim(c, d)
        apply_transform(c)
        parts.append(c)


def mature_tree(seed=3):
    rng = rand(seed)
    bark = M('bark')
    parts = []
    trunk = cylinder('trunk', 0.6, 0.32, 3.6, loc=(0, 0, 1.8), segs=18, mat=bark)
    subdivide(trunk, 2, simple=True)
    displace(trunk, 0.16, 0.35, seed=seed)
    apply_mods(trunk)
    parts.append(trunk)
    _roots(parts, rng, 6, 0.26, 1.3, bark)
    tips = []
    for i in range(6):
        a = math.tau * i / 6 + rng.uniform(-0.25, 0.25)
        d = Vector((math.cos(a) * 0.8, math.sin(a) * 0.8, 0.75)).normalized()
        tips.append(_branch(parts, rng, (0, 0, 2.3 + i * 0.22), d, 2.4, 0.24, 0.09, 1, bark, children=2))
    tips.append(_branch(parts, rng, (0, 0, 3.5), (0.1, -0.05, 1), 1.8, 0.3, 0.1, 1, bark, children=2))
    wood = join(parts, 'wood')
    vertex_colors(wood, BARK, 0.38, 0.5, seed, gradient=(0, 2.5, 0.7))
    smooth(wood, 60)
    canopy = []
    for i, tip in enumerate(tips):
        for j in range(2):
            r = rng.uniform(1.25, 1.9)
            at = tip + Vector((rng.uniform(-0.6, 0.6), rng.uniform(-0.6, 0.6), rng.uniform(0.2, 0.9) + j * 0.5))
            s = sphere(f'leaf{i}{j}', r, tuple(at), subdiv=2, mat=M('leaves'), scale=(1, 1, 0.8))
            displace(s, 0.45, 0.7, seed=seed * 7 + i * 3 + j)
            apply_mods(s)
            canopy.append(s)
    crown = join(canopy, 'crown')
    vertex_colors(crown, LEAF, 0.45, 0.9, seed + 1, gradient=(3.0, 7.5, 0.55))
    smooth(crown, 70)
    o = join([wood, crown], 'mature-tree')
    ground(o)
    return o


def dead_tree(seed=4, pale=False):
    rng = rand(seed)
    bark = M('bark-pale' if pale else 'bark')
    parts = []
    trunk = cylinder('trunk', 0.5, 0.28, 3.0, loc=(0, 0, 1.5), segs=14, mat=bark)
    subdivide(trunk, 2, simple=True)
    displace(trunk, 0.14, 0.3, seed=seed)
    apply_mods(trunk)
    parts.append(trunk)
    _roots(parts, rng, 5, 0.22, 1.2, bark)
    for i in range(4):
        a = math.tau * i / 4 + rng.uniform(-0.4, 0.4)
        d = Vector((math.cos(a) * 0.9, math.sin(a) * 0.9, 0.9)).normalized()
        _branch(parts, rng, (0, 0, 2.2 + i * 0.25), d, 2.6, 0.22, 0.05, 2, bark, twist=0.9, children=2, min_len=0.6)
    _branch(parts, rng, (0, 0, 3.0), (0.15, 0.1, 1), 2.4, 0.28, 0.06, 2, bark, twist=0.8, children=3, min_len=0.6)
    # snapped limbs: short thick stubs
    for i in range(2):
        a = rng.uniform(0, math.tau)
        d = Vector((math.cos(a), math.sin(a), 0.3)).normalized()
        c = cylinder(f'stub{i}', 0.16, 0.1, 0.5, loc=tuple(Vector((0, 0, 1.4 + i * 0.5)) + d * 0.3), segs=8, mat=bark)
        aim(c, d)
        apply_transform(c)
        parts.append(c)
    o = join(parts, 'dead-tree')
    vertex_colors(o, (0.55, 0.56, 0.58) if pale else DEAD_BARK, 0.42, 0.45, seed, gradient=(0, 2.0, 0.65))
    if pale:
        set_material_by_normal(o, M('snow'), up=0.72, noise_scale=0.4, seed=seed, threshold=0.3)
    smooth(o, 60)
    ground(o)
    return o


def hollow_log(seed=5):
    rng = rand(seed)
    bark = M('bark')
    log = cylinder('log', 0.8, 0.7, 4.2, loc=(0, 0, 0.8), rot=(0, math.pi / 2, 0), segs=24, mat=bark)
    subdivide(log, 2, simple=True)
    displace(log, 0.1, 0.35, seed=seed)
    apply_mods(log)
    inner = cylinder('hole', 0.55, 0.45, 4.6, loc=(0, 0, 0.78), rot=(0, math.pi / 2, 0), segs=16)
    displace(inner, 0.12, 0.3, seed=seed + 3)
    apply_mods(inner)
    boolean(log, inner)
    # splinters: wedges cut from the near mouth
    for i in range(4):
        a = rng.uniform(0, math.tau)
        w = box(f'wedge{i}', (0.5, 0.18, 0.5), loc=(2.1, math.cos(a) * 0.65, 0.8 + math.sin(a) * 0.65), rot=(a, 0.4, rng.uniform(-0.3, 0.3)))
        boolean(log, w)
    parts = [log]
    _roots(parts, rng, 3, 0.2, 1.0, bark, z=0.5)
    for p in parts[1:]:
        p.location.x -= 2.0
        apply_transform(p)
    o = join(parts, 'hollow-log')
    vertex_colors(o, BARK, 0.4, 0.4, seed, gradient=(0, 1.2, 0.75))
    set_material_where(o, M('planks-worn'), lambda c, n: abs(n.x) > 0.85)  # the cut ends: pale wood
    set_material_by_normal(o, M('moss'), up=0.62, noise_scale=0.7, seed=seed, threshold=0.6)
    box_uv(o, 1.2)
    smooth(o, 55)
    ground(o)
    return o


def stump(seed=6):
    rng = rand(seed)
    bark = M('bark')
    body = cylinder('stump', 0.62, 0.52, 0.95, loc=(0, 0, 0.475), segs=18, mat=bark)
    subdivide(body, 2, simple=True)
    displace(body, 0.12, 0.3, seed=seed)
    apply_mods(body)
    crown = cylinder('crown', 0.52, 0.3, 0.45, loc=(0, 0, 1.15), segs=18, mat=M('planks-worn'))
    subdivide(crown, 2, simple=True)
    displace(crown, 0.42, 0.22, seed=seed + 2)
    apply_mods(crown)
    box_uv(crown, 0.9)
    parts = [body, crown]
    _roots(parts, rng, 5, 0.24, 1.1, bark, z=0.22)
    o = join(parts, 'stump')
    vertex_colors(o, BARK, 0.4, 0.4, seed, gradient=(0, 0.9, 0.7))
    set_material_where(o, M('moss'), lambda c, n: c.z < 0.3 and n.z < 0.6 and noise.noise(c * 2.5) > 0.05)
    smooth(o, 55)
    ground(o)
    return o


def fallen_log(seed=7, snow=False):
    rng = rand(seed)
    bark = M('bark')
    log = cylinder('log', 0.52, 0.4, 4.6, loc=(0, 0, 0.5), rot=(0, math.pi / 2, 0), segs=20, mat=bark)
    subdivide(log, 2, simple=True)
    displace(log, 0.08, 0.35, seed=seed)
    apply_mods(log)
    parts = [log]
    stub = cylinder('stub', 0.14, 0.06, 0.9, loc=(0.7, 0.2, 0.95), segs=8, mat=bark)
    aim(stub, (0.2, 0.5, 0.85))
    apply_transform(stub)
    parts.append(stub)
    o = join(parts, 'fallen-log')
    vertex_colors(o, BARK, 0.4, 0.4, seed, gradient=(0, 0.6, 0.78))
    set_material_where(o, M('planks-worn'), lambda c, n: abs(n.x) > 0.8 and abs(c.x) > 2.0)
    if snow:
        set_material_by_normal(o, M('snow'), up=0.35, noise_scale=0.8, seed=seed, threshold=0.35)
    else:
        set_material_by_normal(o, M('moss'), up=0.7, noise_scale=0.6, seed=seed, threshold=0.6)
    box_uv(o, 1.0)
    smooth(o, 55)
    ground(o)
    return o


def forest_floor_cluster(seed=8):
    rng = rand(seed)
    parts = []
    patch = cylinder('patch', 0.95, 0.85, 0.08, loc=(0, 0, 0.04), segs=20, mat=M('forest-floor'))
    displace(patch, 0.06, 0.3, seed=seed)
    apply_mods(patch)
    box_uv(patch, 1.4)
    parts.append(patch)
    for i, (x, y, r) in enumerate([(0.25, 0.1, 0.26), (0.45, -0.2, 0.18), (0.05, 0.42, 0.2)]):
        cap = sphere(f'cap{i}', r, (x, y, 0.1 + r * 1.6), subdiv=3, mat=M('fungus'), scale=(1, 1, 0.55))
        stem = cylinder(f'stem{i}', r * 0.3, r * 0.22, r * 1.7, loc=(x, y, 0.08 + r * 0.85), segs=10, mat=M('fungus'))
        parts += [cap, stem]
    fungi = join(parts[1:], 'fungi')
    vertex_colors(fungi, (0.62, 0.42, 0.26), 0.3, 0.25, seed, gradient=(0.1, 0.6, 0.8))
    smooth(fungi, 60)
    fronds = []
    for i in range(5):
        a = math.tau * i / 5 + rng.uniform(-0.3, 0.3)
        f = plane(f'frond{i}', (0.95, 0.28), loc=(math.cos(a) * 0.35, math.sin(a) * 0.35, 0.3), rot=(0, -0.65, a))
        f.data.materials.append(M('leaves'))
        subdivide(f, 2, simple=True)
        displace(f, 0.12, 0.25, seed=seed + i)
        apply_mods(f)
        fronds.append(f)
    ferns = join(fronds, 'ferns')
    vertex_colors(ferns, (0.2, 0.4, 0.14), 0.4, 0.3, seed + 9)
    smooth(ferns, 80)
    root = torus('root', 0.45, 0.06, loc=(-0.4, -0.3, 0.02), rot=(math.pi / 2, 0, 0.6), mat=M('bark'))
    vertex_colors(root, BARK, 0.3, 0.3, seed)
    o = join([fungi, patch, ferns, root], 'forest-floor-cluster')  # the active object carries the colour layer
    ground(o)
    return o


if __name__ == '__main__':
    run('forest', [
        ('rock-cluster', rock_cluster),
        ('moss-boulder', moss_boulder),
        ('mature-tree', mature_tree),
        ('dead-tree', dead_tree),
        ('hollow-log', hollow_log),
        ('stump', stump),
        ('fallen-log', fallen_log),
        ('forest-floor-cluster', forest_floor_cluster),
    ])
