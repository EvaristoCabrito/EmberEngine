"""Kit G: the sprint's extra list (content/models/extra): fences, door and doorway, grave marker, broken wheel,
firewood, bucket, torch bracket, Ember crystals, mine supports, mushrooms."""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emberlib import *  # noqa
from emberlib import _new_mesh_object

BARK = (0.30, 0.22, 0.15)


def arc(name, major, minor, angle, loc=(0, 0, 0), rot=(0, 0, 0), mat=None, segs=16, ring=8):
    """A torus swept only `angle` radians from +x (a handle, a hoop with a gap). Rotated by `rot` after the sweep."""
    bm = bmesh.new()
    verts = [bm.verts.new((major + minor * math.cos(a), 0.0, minor * math.sin(a))) for a in [math.tau * i / ring for i in range(ring)]]
    edges = [bm.edges.new((verts[i], verts[(i + 1) % ring])) for i in range(ring)]
    bmesh.ops.spin(bm, geom=verts + edges, cent=(0, 0, 0), axis=(0, 0, 1), dvec=(0, 0, 0), angle=angle, steps=segs,
                   use_merge=False, use_normal_flip=False)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    o = _new_mesh_object(name, bm, mat)
    o.location = loc
    o.rotation_euler = rot
    apply_transform(o)
    return o


# ------------------------------------------------------------------------------------------------------ fences
def _post(name, x, h=1.65, lean=(0, 0, 0), mat=None):
    p = box(name, (0.15, 0.15, h), loc=(x, 0, h * 0.5), rot=lean, mat=mat or M('planks-worn'))
    bevel(p, 0.012, 1)
    apply_mods(p)
    box_uv(p, 0.8, (0.1 * x, 0.0))
    return p


def _rail(name, length, loc, rot=(0, 0, 0), mat=None):
    r = box(name, (length, 0.07, 0.16), loc=loc, rot=rot, mat=mat or M('planks-worn'))
    bevel(r, 0.01, 1)
    apply_mods(r)
    box_uv(r, 0.9, (0.3 * loc[2], 0.5))
    return r


def fence_segment():
    L = 3.0
    parts = [_post('postL', -L * 0.5 + 0.1), _post('postR', L * 0.5 - 0.1)]
    for z in (0.42, 0.88, 1.34):
        parts.append(_rail(f'rail{z}', L, (0, 0.11, z)))
    for sx in (-1, 1):
        for z in (0.42, 0.88, 1.34):
            parts.append(sphere(f'nail{sx}{z}', 0.02, (sx * (L * 0.5 - 0.1), 0.15, z), subdiv=1, mat=M('iron')))
    o = join(parts, 'fence-segment')
    smooth(o, 40)
    ground(o)
    return o


def broken_fence(seed=61):
    rng = rand(seed)
    L = 3.0
    parts = [_post('postL', -L * 0.5 + 0.1), _post('postR', L * 0.5 - 0.1, h=1.1, lean=(0.18, 0.12, 0))]
    # the top rail hangs from the left post, snapped; the middle one is whole; the bottom one lies on the ground
    parts.append(_rail('rail-top', L * 0.55, (-L * 0.5 + 0.1 + L * 0.27, 0.11, 1.2), rot=(0, 0.22, 0)))
    parts.append(_rail('rail-mid', L, (0, 0.11, 0.88)))
    parts.append(_rail('rail-ground', L * 0.7, (0.4, 0.55, 0.08), rot=(math.pi / 2 - 0.3, 0, 0.35)))
    for i in range(3):
        s = box(f'splinter{i}', (rng.uniform(0.25, 0.5), 0.05, 0.06), loc=(rng.uniform(-0.3, 0.9), rng.uniform(0.2, 0.7), 0.03),
                rot=(0, 0, rng.uniform(0, 3)), mat=M('planks-worn'))
        box_uv(s, 0.8)
        parts.append(s)
    for z in (0.88,):
        parts.append(sphere(f'nailL{z}', 0.02, (-L * 0.5 + 0.1, 0.15, z), subdiv=1, mat=M('iron')))
    o = join(parts, 'broken-fence')
    clamp_floor(o, 0.0)
    smooth(o, 40)
    ground(o)
    return o


# -------------------------------------------------------------------------------------------- door and doorway
def wooden_door():
    W, H, T = 1.5, 2.9, 0.1
    n = 5
    pw = W / n
    parts = []
    for i in range(n):
        x = -W * 0.5 + pw * (i + 0.5)
        p = box(f'plank{i}', (pw - 0.012, T, H), loc=(x, 0, H * 0.5), mat=M('planks-dark'))
        bevel(p, 0.01, 1)
        apply_mods(p)
        box_uv(p, 1.5, (0.37 * i, 0.0))
        parts.append(p)
    iron = M('iron')
    for z in (0.5, H * 0.5, H - 0.5):
        strap = box(f'strap{z}', (W * 0.86, 0.03, 0.16), loc=(0, -T * 0.5 - 0.012, z), mat=iron)
        parts.append(strap)
        # the strap's hinge knuckle at the left edge
        parts.append(cylinder(f'knuckle{z}', 0.05, 0.05, 0.26, loc=(-W * 0.5 + 0.02, -T * 0.5 - 0.02, z), segs=10, mat=iron))
        for x in (-0.55, -0.28, 0.0, 0.28, 0.55):
            parts.append(sphere(f'rivet{x}{z}', 0.022, (x * W * 0.86 / 1.25, -T * 0.5 - 0.03, z), subdiv=1, mat=iron))
    # ring handle on a boss, right side
    parts.append(cylinder('boss', 0.09, 0.09, 0.04, loc=(W * 0.32, -T * 0.5 - 0.02, H * 0.47), rot=(math.pi / 2, 0, 0), segs=12, mat=iron))
    parts.append(torus('ring', 0.11, 0.02, loc=(W * 0.32, -T * 0.5 - 0.06, H * 0.47 - 0.1), rot=(math.pi / 2, 0, 0), mat=iron, segs=20, ring=6))
    o = join(parts, 'wooden-door')
    smooth(o, 40)
    ground(o)
    return o


def stone_doorway(seed=62):
    W, H, D, J = 2.7, 3.5, 0.9, 0.5  # outer width, height, depth, jamb width
    parts = []
    for sx in (-1, 1):
        parts.append(box(f'jamb{sx}', (J, D, H - J), loc=(sx * (W * 0.5 - J * 0.5), 0, (H - J) * 0.5), mat=M('masonry')))
    parts.append(box('lintel', (W, D, J), loc=(0, 0, H - J * 0.5), mat=M('masonry')))
    parts.append(box('keystone', (0.55, D + 0.06, J * 0.9), loc=(0, 0, H - J * 0.45), mat=M('masonry')))
    for sx in (-1, 1):
        parts.append(box(f'foot{sx}', (J + 0.14, D + 0.12, 0.3), loc=(sx * (W * 0.5 - J * 0.5), 0, 0.15), mat=M('masonry-mossy')))
    for p in parts:
        bevel(p, 0.03, 1)
        displace(p, 0.03, 0.22, seed=seed + len(p.name))
        apply_mods(p)
        box_uv(p, 1.2)
    # the recessed opening: an inner frame set back, and the dark of the passage behind it
    inner = M('masonry-crypt')
    ow = W - 2 * J
    for sx in (-1, 1):
        parts.append(box(f'reveal{sx}', (0.12, D * 0.6, H - J), loc=(sx * (ow * 0.5 - 0.06), D * 0.2, (H - J) * 0.5), mat=inner))
    parts.append(box('head', (ow, D * 0.6, 0.12), loc=(0, D * 0.2, H - J - 0.06), mat=inner))
    parts.append(box('sill', (ow, D * 0.6, 0.06), loc=(0, D * 0.2, 0.03), mat=inner))
    for p in parts[-4:]:
        box_uv(p, 1.0)
    dark = material('passage-dark', flat=(0.02, 0.018, 0.02), roughness=1.0)
    parts.append(box('passage', (ow - 0.1, 0.05, H - J - 0.1), loc=(0, D * 0.5 - 0.05, (H - J) * 0.5), mat=dark))
    o = join(parts, 'stone-doorway')
    smooth(o, 40)
    ground(o)
    return o


# ------------------------------------------------------------------------------------------------- grave marker
def grave_marker(seed=63):
    W, T = 0.9, 0.2
    slab = box('slab', (W, T, 1.05), loc=(0, 0, 0.525), mat=M('masonry-mossy'))
    top = cylinder('top', W * 0.5, W * 0.5, T, loc=(0, 0, 1.05), rot=(math.pi / 2, 0, 0), segs=24, mat=M('masonry-mossy'))
    s = boolean(slab, top, op='UNION')
    s.name = 'stone'
    # a chipped corner and two worn inscription grooves, cut while the mesh is still simple (cutting the subdivided,
    # displaced slab collapsed it to nothing: the exact solver on the coplanar groove)
    chip = sphere('chip', 0.2, (W * 0.5, 0, 1.35), subdiv=2)
    boolean(s, chip)
    for z in (0.75, 0.6):
        g = box(f'groove{z}', (W * 0.55, 0.06, 0.035), loc=(0, -T * 0.5, z))
        boolean(s, g)
    subdivide(s, 2, simple=True)
    displace(s, 0.045, 0.2, seed=seed)
    displace(s, 0.015, 0.06, seed=seed + 1)
    apply_mods(s)
    box_uv(s, 1.1, (0.2, 0.1))
    s.rotation_euler = (0.07, 0.04, 0.0)
    apply_transform(s)
    mound = sphere('mound', 0.65, (0.05, 0.1, -0.35), subdiv=3, mat=M('forest-floor'), scale=(1.2, 1.5, 0.7))
    displace(mound, 0.08, 0.3, seed=seed + 2)
    apply_mods(mound)
    clamp_floor(mound, 0.0)
    box_uv(mound, 1.3)
    o = join([s, mound], 'grave-marker')
    set_material_where(o, M('moss'), lambda c, n: c.z < 0.4 and c.z > 0.05 and abs(n.z) < 0.6 and noise.noise(c * 3.0) > 0.1)
    smooth(o, 45)
    ground(o)
    return o


# --------------------------------------------------------------------------------------------- broken cart wheel
def broken_cart_wheel(seed=64):
    rng = rand(seed)
    R = 0.58
    rim = arc('rim', R, 0.06, math.tau * 0.78, rot=(0, 0, 0.9), mat=M('planks-dark'), segs=30, ring=8)
    hub = cylinder('hub', 0.1, 0.1, 0.18, loc=(0, 0, 0), segs=10, mat=M('iron'))
    parts = [rim, hub, torus('hubring', 0.1, 0.018, loc=(0, 0, 0.08), mat=M('iron-rust'), segs=16, ring=6)]
    for i in range(8):
        a = math.tau * i / 8 + 0.9
        L = R if i not in (7, 0) else R * rng.uniform(0.35, 0.55)  # the two spokes under the missing rim are snapped
        s = cylinder(f'spoke{i}', 0.03, 0.025, L, loc=(math.cos(a) * L * 0.5, math.sin(a) * L * 0.5, 0), segs=6, mat=M('planks-worn'))
        aim(s, (math.cos(a), math.sin(a), 0))
        apply_transform(s)
        parts.append(s)
    o = join(parts, 'broken-cart-wheel')
    # lying against the ground, propped at a slant like a wheel left by a wall
    o.rotation_euler = (math.radians(68), 0, 0.3)
    apply_transform(o)
    for i in range(2):
        f = box(f'fragment{i}', (0.35, 0.07, 0.07), loc=(rng.uniform(0.2, 0.6), rng.uniform(-0.5, -0.2), 0.035), rot=(0, 0, rng.uniform(0, 3)), mat=M('planks-dark'))
        box_uv(f, 0.8)
        o = join([o, f], 'broken-cart-wheel')
    smooth(o, 40)
    ground(o)
    return o


# -------------------------------------------------------------------------------------------------- firewood
def firewood_stack(seed=65):
    rng = rand(seed)
    parts = []
    r = 0.17
    rows = [(5, 0.0), (4, r * 1.0), (3, r * 2.0)]
    for row, (n, z) in enumerate(rows):
        for i in range(n):
            y = (i - (n - 1) * 0.5) * r * 2.05
            L = rng.uniform(1.25, 1.45)
            flip = rng.random() < 0.5
            prof = half_circle(r * rng.uniform(0.86, 1.06), 10, 0.0)
            log = prism(f'log{row}{i}', prof, L, loc=(rng.uniform(-0.1, 0.1), y, z + 0.0), rot=(math.pi if flip else 0, 0, rng.uniform(-0.07, 0.07)), mat=M('bark'))
            if flip:
                log.location.z += r * 0.95
                apply_transform(log)
            subdivide(log, 1, simple=True)
            displace(log, 0.03, 0.15, seed=seed + row * 10 + i)
            apply_mods(log)
            parts.append(log)
    o = join(parts, 'firewood')
    clamp_floor(o, 0.0)
    vertex_colors(o, BARK, 0.35, 0.3, seed, gradient=(0, 0.6, 0.8))
    set_material_where(o, M('planks-worn'), lambda c, n: abs(n.x) > 0.8 or (abs(n.z) > 0.9 and abs(n.y) < 0.2 and c.z > 0.02))
    box_uv(o, 0.7)
    smooth(o, 50)
    for x in (-0.42, 0.42):
        # object scale acts in the torus's own frame (before its rotation): local x is world -z once turned on its side
        rope = torus(f'rope{x}', 0.5, 0.022, loc=(0, 0, 0), rot=(0, math.pi / 2, 0), mat=M('rope'), segs=28, ring=6)
        rope.scale = (0.58, 1.85, 1.0)
        rope.location = (x, 0, 0.3)
        apply_transform(rope)
        knot = sphere(f'knot{x}', 0.045, (x, 0.0, 0.59), subdiv=1, mat=M('rope'))
        o = join([o, rope, knot], 'firewood-stack')
    ground(o)
    return o


# ------------------------------------------------------------------------------------------------------ bucket
def wooden_bucket():
    R, H = 0.3, 0.55
    body = lathe('body', [(0, 0), (R * 0.88, 0), (R, H), (R * 0.9, H), (R * 0.8, 0.05), (0, 0.05)], segs=16, mat=M('planks-dark'))
    box_uv(body, 0.8)
    flat(body)  # the staves read as facets
    parts = [body]
    for z, s in ((0.1, 0.9), (H - 0.07, 0.99)):
        rr = R * (0.88 + 0.12 * z / H) + 0.008
        parts.append(torus(f'hoop{z}', rr, 0.014, loc=(0, 0, z), mat=M('iron'), segs=24, ring=6))
    for sx in (-1, 1):
        parts.append(box(f'ear{sx}', (0.05, 0.03, 0.14), loc=(sx * (R - 0.01), 0, H + 0.04), mat=M('iron')))
    parts.append(arc('handle', R + 0.01, 0.014, math.pi, loc=(0, 0, H + 0.1), rot=(math.pi / 2, 0, 0), mat=M('iron'), segs=18, ring=6))
    o = join(parts, 'wooden-bucket')
    for p in o.data.polygons:
        if p.material_index == 1:
            p.use_smooth = True
    ground(o)
    return o


# ------------------------------------------------------------------------------------------------ torch bracket
def torch_bracket():
    iron = M('iron-rust')
    Z = 2.1  # the ring's height on the wall
    parts = [box('strap', (0.07, 0.03, Z + 0.45), loc=(0, 0, (Z + 0.45) * 0.5), mat=iron),
             box('plate', (0.24, 0.035, 0.42), loc=(0, 0, Z), mat=iron)]
    bevel(parts[1], 0.01, 1)
    apply_mods(parts[1])
    for z in (Z - 0.15, Z + 0.15):
        parts.append(sphere(f'bolt{z}', 0.025, (0, -0.02, z), subdiv=1, mat=M('iron')))
    parts.append(cylinder('arm', 0.02, 0.02, 0.3, loc=(0, -0.15, Z), rot=(math.pi / 2, 0, 0), segs=8, mat=iron))
    parts.append(torus('ring', 0.075, 0.016, loc=(0, -0.28, Z + 0.02), mat=iron, segs=18, ring=6))
    parts.append(cylinder('shaft', 0.045, 0.035, 0.95, loc=(0, -0.28, Z + 0.3), segs=10, mat=M('planks-worn')))
    box_uv(parts[-1], 0.6)
    head = cylinder('head', 0.085, 0.07, 0.28, loc=(0, -0.28, Z + 0.86), segs=12, mat=M('burlap'))
    displace(head, 0.03, 0.08, seed=66)
    apply_mods(head)
    parts.append(head)
    parts.append(torus('wrap', 0.085, 0.012, loc=(0, -0.28, Z + 0.78), mat=M('rope'), segs=16, ring=6))
    parts.append(torus('wrap2', 0.083, 0.012, loc=(0, -0.28, Z + 0.94), mat=M('rope'), segs=16, ring=6))
    o = join(parts, 'torch-bracket')
    smooth(o, 40)
    ground(o)
    return o


# ---------------------------------------------------------------------------------------------- Ember crystals
def ember_crystal_cluster(seed=67):
    rng = rand(seed)
    base = sphere('base', 0.75, (0, 0, 0.3), subdiv=3, mat=M('stone'), scale=(1.3, 1.1, 0.55))
    displace(base, 0.2, 0.5, seed=seed)
    displace(base, 0.05, 0.12, seed=seed + 1)
    apply_mods(base)
    clamp_floor(base, 0.0)
    box_uv(base, 1.2)
    parts = [base]
    spots = [(0, 0, 1.45, 0.2), (0.35, 0.2, 1.05, 0.15), (-0.3, 0.25, 0.9, 0.14), (0.1, -0.4, 0.8, 0.12), (-0.45, -0.15, 0.7, 0.11), (0.5, -0.2, 0.55, 0.09), (-0.1, 0.5, 0.5, 0.08)]
    for i, (x, y, h, r) in enumerate(spots):
        prof = [(0, 0), (r, 0), (r * 1.05, h * 0.55), (r * 0.6, h * 0.86), (0, h)]
        c = lathe(f'crystal{i}', prof, segs=6, mat=M('ember-crystal'))
        c.rotation_euler = (0, 0, rng.uniform(0, 1.0))
        apply_transform(c)
        d = Vector((x * 0.6 + rng.uniform(-0.2, 0.2), y * 0.6 + rng.uniform(-0.2, 0.2), 1.0)).normalized()
        aim(c, d)
        c.location = (x, y, 0.25)
        apply_transform(c)
        flat(c)
        parts.append(c)
    o = join(parts, 'ember-crystal-cluster')
    for p in o.data.polygons:
        if p.material_index == 0:
            p.use_smooth = True
    ground(o)
    return o


# ------------------------------------------------------------------------------------------------- mine supports
def mine_supports(seed=68):
    W, H, T = 3.4, 3.6, 0.3
    parts = []
    for sx in (-1, 1):
        parts.append(box(f'post{sx}', (T, T, H - T), loc=(sx * (W * 0.5 - T * 0.5), 0, (H - T) * 0.5), mat=M('planks-dark')))
    parts.append(box('cap', (W + 0.3, T, T), loc=(0, 0, H - T * 0.5), mat=M('planks-dark')))
    # knee braces in the corners (the cross bracing that keeps the frame square without closing the passage)
    for sx in (-1, 1):
        b = box(f'knee{sx}', (1.15, 0.14, 0.16), loc=(sx * (W * 0.5 - T - 0.3), 0.0, H - T - 0.38), rot=(0, sx * math.radians(42), 0), mat=M('planks-worn'))
        parts.append(b)
    # a second thinner frame behind, tied to the first by a stringer along the top: the tunnel's next set
    for sx in (-1, 1):
        parts.append(box(f'post2{sx}', (T * 0.85, T * 0.85, H - T - 0.1), loc=(sx * (W * 0.5 - T * 0.5), 1.4, (H - T - 0.1) * 0.5), mat=M('planks-dark')))
    parts.append(box('cap2', (W + 0.2, T * 0.85, T * 0.85), loc=(0, 1.4, H - T * 0.5 - 0.1), mat=M('planks-dark')))
    for sx in (-1, 1):
        s = box(f'stringer{sx}', (0.12, 1.7, 0.12), loc=(sx * (W * 0.5 - T * 0.5), 0.7, H - T * 0.5 - 0.06), mat=M('planks-worn'))
        parts.append(s)
        # X bracing on each side wall between the two sets
        for k in (-1, 1):
            x = box(f'x{sx}{k}', (0.1, 1.95, 0.14), loc=(sx * (W * 0.5 - T * 0.5), 0.7, H * 0.45), rot=(k * math.radians(52), 0, 0), mat=M('planks-worn'))
            parts.append(x)
    # lagging planks over the cap, holding back the roof
    for i in range(5):
        parts.append(box(f'lag{i}', (0.42, 1.85, 0.07), loc=(-W * 0.5 + 0.4 + i * (W - 0.6) / 4, 0.7, H + 0.035), mat=M('planks-worn')))
    for p in parts:
        bevel(p, 0.015, 1)
        apply_mods(p)
        box_uv(p, 1.0, (0.1 * len(p.name), 0.0))
    o = join(parts, 'mine-supports')
    smooth(o, 40)
    ground(o)
    return o


# ------------------------------------------------------------------------------------------------- mushrooms
def _mushroom(name, x, y, r, h, rng):
    """Cap (domed, lip curled under, gills) and stem in one lathe."""
    prof = [(0, h + r * 0.55), (r * 0.35, h + r * 0.52), (r * 0.7, h + r * 0.38), (r * 0.95, h + r * 0.16), (r * 1.02, h),
            (r * 0.95, h - r * 0.08), (r * 0.5, h - r * 0.02), (r * 0.3, h - r * 0.1), (r * 0.22, h * 0.5), (r * 0.26, 0.04), (r * 0.3, 0), (0, 0)]
    m = lathe(name, prof, segs=18, mat=M('fungus'))
    subdivide(m, 1, simple=True)
    displace(m, r * 0.12, r * 0.6, seed=int(rng.random() * 1000))
    apply_mods(m)
    m.rotation_euler = (rng.uniform(-0.12, 0.12), rng.uniform(-0.12, 0.12), rng.uniform(0, 6.28))
    m.location = (x, y, 0)
    apply_transform(m)
    return m, h - r * 0.1


def mushroom_cluster(seed=69):
    rng = rand(seed)
    parts = []
    spec = [(0, 0, 0.5, 1.05), (0.55, 0.25, 0.38, 0.75), (-0.5, 0.3, 0.32, 0.6), (0.15, -0.55, 0.28, 0.5), (-0.35, -0.4, 0.2, 0.35),
            (0.7, -0.25, 0.14, 0.25), (-0.75, -0.05, 0.12, 0.2)]
    cap_z = {}
    for i, (x, y, r, h) in enumerate(spec):
        m, zc = _mushroom(f'mush{i}', x, y, r, h, rng)
        cap_z[m.name] = zc
        parts.append(m)
    o = join(parts, 'mushroom-cluster')
    stem = (0.78, 0.72, 0.58)
    cap = (0.36, 0.22, 0.42)
    spot = (0.72, 0.6, 0.7)

    def pick(co, n):
        # the cap: every vertex above the largest nearby stem's top... simpler: the cap is wide, the stem thin: use the
        # noise-free rule 'facing up or a wide radius from the nearest spec centre'
        best = None
        for (x, y, r, h) in spec:
            d = math.hypot(co.x - x, co.y - y)
            if d < r * 1.25 and (best is None or d / r < best[0]):
                best = (d / r, r, h)
        if best is None:
            return None
        _, r, h = best
        if co.z > h - r * 0.15:
            k = 1.0 + noise.noise(co * 2.2) * 0.25
            c = [cap[0] * k, cap[1] * k, cap[2] * k]
            if n.z > 0.2 and noise.noise(co * 6.0 + Vector((3, 1, 7))) > 0.42:
                c = list(spot)
            if n.z < -0.3:  # the gills underneath
                c = [0.52, 0.42, 0.34]
            return c
        return None
    vertex_colors(o, stem, 0.18, 0.4, seed, gradient=(0, 0.5, 0.7), select=pick)
    smooth(o, 60)
    ground(o)
    return o


if __name__ == '__main__':
    run('extra', [
        ('fence-segment', fence_segment),
        ('broken-fence', broken_fence),
        ('wooden-door', wooden_door),
        ('stone-doorway', stone_doorway),
        ('grave-marker', grave_marker),
        ('broken-cart-wheel', broken_cart_wheel),
        ('firewood-stack', firewood_stack),
        ('wooden-bucket', wooden_bucket),
        ('torch-bracket', torch_bracket),
        ('ember-crystal-cluster', ember_crystal_cluster),
        ('mine-supports', mine_supports),
        ('mushroom-cluster', mushroom_cluster),
    ])
