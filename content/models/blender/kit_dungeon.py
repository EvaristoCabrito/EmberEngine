"""Kit F: dungeon, ruins and underground (content/models/dungeon)."""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emberlib import *  # noqa


def pillar(seed=51):
    prof = [(0, 0), (0.85, 0), (0.85, 0.3), (0.65, 0.42), (0.55, 0.5), (0.55, 3.4), (0.62, 3.55), (0.62, 3.75), (0.8, 3.9), (0.8, 4.2), (0, 4.2)]
    p = lathe('pillar', prof, segs=22, mat=M('masonry-crypt'))
    subdivide(p, 1, simple=True)
    displace(p, 0.05, 0.25, seed=seed)
    apply_mods(p)
    box_uv(p, 1.2)
    for i in range(3):
        chip = sphere(f'chip{i}', 0.28, (0.72 * math.cos(i * 2.1), 0.72 * math.sin(i * 2.1), 0.2 + i * 1.6), subdiv=2)
        boolean(p, chip)
    for z in (1.3, 2.3):
        band = torus(f'band{z}', 0.56, 0.05, loc=(0, 0, z), mat=M('masonry-crypt'), segs=22, ring=6)
        box_uv(band, 1.2)
        p = join([p, band], 'pillar')
    smooth(p, 40)
    ground(p)
    return p


def iron_cage():
    W, D, H = 1.25, 1.25, 2.3
    parts = [box('base', (W, D, 0.1), loc=(0, 0, 0.05), mat=M('iron-rust')), box('top', (W, D, 0.08), loc=(0, 0, H - 0.04), mat=M('iron-rust'))]
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(box(f'post{sx}{sy}', (0.08, 0.08, H), loc=(sx * (W * 0.5 - 0.04), sy * (D * 0.5 - 0.04), H * 0.5), mat=M('iron-rust')))
    n = 6
    for i in range(1, n):
        t = -W * 0.5 + W * i / n
        for s in (-1, 1):
            parts.append(cylinder(f'barx{i}{s}', 0.025, 0.025, H - 0.1, loc=(t, s * (D * 0.5 - 0.04), H * 0.5), segs=6, mat=M('iron-rust')))
            if not (s < 0 and 2 <= i <= 3):  # the door's bars sit on the hinged door frame
                parts.append(cylinder(f'bary{i}{s}', 0.025, 0.025, H - 0.1, loc=(s * (W * 0.5 - 0.04), t, H * 0.5), segs=6, mat=M('iron-rust')))
    # the door: a framed panel on the -x side, slightly open
    door = [box('doorframe-b', (0.05, 0.42, 0.05), loc=(0, 0, 0.15), mat=M('iron-rust')), box('doorframe-t', (0.05, 0.42, 0.05), loc=(0, 0, H - 0.15), mat=M('iron-rust')),
            box('doorframe-s', (0.05, 0.05, H - 0.3), loc=(0, 0.2, H * 0.5), mat=M('iron-rust')), box('doorframe-s2', (0.05, 0.05, H - 0.3), loc=(0, -0.2, H * 0.5), mat=M('iron-rust'))]
    for i in range(1, 3):
        door.append(cylinder(f'doorbar{i}', 0.022, 0.022, H - 0.3, loc=(0, -0.2 + 0.4 * i / 3, H * 0.5), segs=6, mat=M('iron-rust')))
    d = join(door, 'door')
    d.location = (-W * 0.5 + 0.02, -D * 0.5 + 0.24 + 0.2, 0)
    d.rotation_euler = (0, 0, -0.35)
    apply_transform(d)
    parts.append(d)
    for sx in (-1, 1):
        for sy in (-1, 1):
            for z in (0.12, H - 0.12):
                parts.append(sphere(f'rivet{sx}{sy}{z}', 0.03, (sx * (W * 0.5 - 0.04), sy * (D * 0.5 - 0.04), z), subdiv=1, mat=M('iron')))
    o = join(parts, 'iron-cage')
    smooth(o, 40)
    ground(o)
    return o


def bone_pile(seed=53):
    rng = rand(seed)
    parts = []
    skull = sphere('skull', 0.24, (0.15, 0.1, 0.24), subdiv=3, mat=M('bone'), scale=(1.0, 1.15, 0.95))
    for sy in (-1, 1):
        socket = sphere(f'socket{sy}', 0.075, (0.15 + sy * 0.09, 0.1 + 0.2, 0.26), subdiv=2)
        boolean(skull, socket)
    jaw = box('jaw', (0.2, 0.16, 0.08), loc=(0.15, 0.2, 0.06), mat=M('bone'))
    bevel(jaw, 0.03, 2)
    apply_mods(jaw)
    parts += [skull, jaw]
    for i in range(7):
        a = rng.uniform(0, math.tau)
        L = rng.uniform(0.6, 1.0)
        c = (rng.uniform(-0.45, 0.45), rng.uniform(-0.45, 0.45), 0.05 + rng.uniform(0, 0.12))
        d = Vector((math.cos(a), math.sin(a), rng.uniform(-0.05, 0.15))).normalized()
        shaft = cylinder(f'bone{i}', 0.045, 0.04, L, loc=c, segs=8, mat=M('bone'))
        aim(shaft, d)
        apply_transform(shaft)
        parts.append(shaft)
        for e in (-1, 1):
            k = sphere(f'knob{i}{e}', 0.065, tuple(Vector(c) + d * (L * 0.5 * e)), subdiv=2, mat=M('bone'), scale=(1.3, 1, 0.8))
            parts.append(k)
    for i in range(4):
        rib = torus(f'rib{i}', 0.3, 0.02, loc=(rng.uniform(-0.4, 0.4), rng.uniform(-0.4, 0.4), 0.0), rot=(rng.uniform(1.2, 1.9), rng.uniform(-0.4, 0.4), rng.uniform(0, 3)), mat=M('bone'), segs=20, ring=6)
        parts.append(rib)
    o = join(parts, 'bone-pile')
    clamp_floor(o, 0.0)
    vertex_colors(o, (0.82, 0.76, 0.62), 0.22, 0.3, seed, gradient=(0, 0.4, 0.7))
    smooth(o, 50)
    ground(o)
    return o


def altar(seed=54):
    body = box('body', (2.2, 1.0, 0.85), loc=(0, 0, 0.425), mat=M('masonry-crypt'))
    bevel(body, 0.04, 2)
    apply_mods(body)
    top = box('top', (2.4, 1.18, 0.14), loc=(0, 0, 0.92), mat=M('masonry-crypt'))
    bevel(top, 0.03, 2)
    apply_mods(top)
    for z in (0.25, 0.6):
        groove = box(f'groove{z}', (2.3, 0.06, 0.05), loc=(0, -0.5, z), mat=None)
        boolean(body, groove)
    corner = box('corner', (0.5, 0.5, 0.5), loc=(1.15, 0.55, 1.0), rot=(0.4, 0.5, 0.3), mat=None)
    boolean(top, corner)
    for p in (body, top):
        displace(p, 0.02, 0.2, seed=seed)
        apply_mods(p)
        box_uv(p, 1.3)
    o = join([body, top], 'altar')
    set_material_by_normal(o, M('masonry-mossy'), up=-0.1, noise_scale=0.5, seed=seed, threshold=0.9)
    smooth(o, 40)
    ground(o)
    return o


def broken_arch(seed=55):
    rng = rand(seed)
    parts = []
    R, span = 1.5, 1.7
    bs = (0.62, 0.62, 0.46)
    for sx in (-1, 1):
        rows = 5 if sx < 0 else 3
        for i in range(rows):
            b = box(f'pier{sx}{i}', bs, loc=(sx * span, 0, bs[2] * (i + 0.5)), rot=(0, 0, rng.uniform(-0.03, 0.03)), mat=M('masonry'))
            parts.append(b)
    n = 11
    for i in range(n):
        if i >= 7:
            continue  # the right half fell
        a = math.pi * (i + 0.5) / n
        x, z = -span * math.cos(a), 5 * bs[2] + R * math.sin(a) * 0.95 - 0.1
        b = box(f'vous{i}', (0.6, 0.6, 0.5), loc=(x, 0, z + 0.15), rot=(0, -a + math.pi / 2, 0), mat=M('masonry'))
        parts.append(b)
    for i in range(5):
        b = box(f'fallen{i}', (0.5, 0.5, 0.4), loc=(span + rng.uniform(-0.6, 0.8), rng.uniform(-0.7, 0.7), 0.2 + i * 0.05),
                rot=(rng.uniform(-0.4, 0.4), rng.uniform(-0.3, 0.3), rng.uniform(0, 3)), mat=M('masonry-mossy'))
        parts.append(b)
    for p in parts:
        bevel(p, 0.03, 1)
        displace(p, 0.035, 0.2, seed=seed + len(p.name))
        apply_mods(p)
        box_uv(p, 1.1)
    o = join(parts, 'broken-arch')
    clamp_floor(o, 0.0)
    smooth(o, 40)
    ground(o)
    return o


def chains(seed=56):
    plate = box('plate', (0.5, 0.08, 0.5), loc=(0, 0, 1.55), mat=M('iron-rust'))
    bevel(plate, 0.02, 1)
    apply_mods(plate)
    parts = [plate, torus('ring', 0.17, 0.035, loc=(0, -0.1, 1.5), rot=(0, 0, 0), mat=M('iron-rust'), segs=20, ring=8)]
    for i in range(4):
        parts.append(sphere(f'bolt{i}', 0.035, (0.18 * (1 if i % 2 else -1), -0.045, 1.55 + 0.18 * (1 if i < 2 else -1)), subdiv=1, mat=M('iron')))
    rng = rand(seed)
    z, y = 1.32, -0.12
    for i in range(11):
        rot = (0, math.pi / 2 if i % 2 else 0, 0)
        y += rng.uniform(-0.01, 0.03)
        parts.append(torus(f'link{i}', 0.085, 0.024, loc=(0, y, z), rot=rot, mat=M('iron-rust'), segs=12, ring=6))
        z -= 0.125
    # the tail lies on the floor
    for i in range(5):
        parts.append(torus(f'tail{i}', 0.085, 0.024, loc=(0.02 + i * 0.13, y + 0.02 + i * 0.03, 0.03), rot=(math.pi / 2 if i % 2 else 0, 0, 0.3), mat=M('iron-rust'), segs=12, ring=6))
    o = join(parts, 'chains')
    smooth(o, 40)
    ground(o)
    return o


if __name__ == '__main__':
    run('dungeon', [
        ('pillar', pillar),
        ('iron-cage', iron_cage),
        ('bone-pile', bone_pile),
        ('altar', altar),
        ('broken-arch', broken_arch),
        ('chains', chains),
    ])
