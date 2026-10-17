"""Kit C: general props and treasure (content/models/props)."""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emberlib import *  # noqa


def treasure_chest():
    L, D, H, R = 1.35, 0.85, 0.62, 0.42
    body = box('body', (L, D, H), loc=(0, 0, H * 0.5), mat=M('planks-dark'))
    bevel(body, 0.025, 2)
    apply_mods(body)
    box_uv(body, 1.4, (0.1, 0.2))
    lid = prism('lid', half_circle(R, 14, 0.0) + [], L, loc=(0, 0, H), rot=(0, 0, 0), mat=M('planks-dark'))
    # the half circle is drawn in (y, z): rotate the prism so the arch stands up along x
    bevel(lid, 0.02, 2)
    apply_mods(lid)
    box_uv(lid, 1.4, (0.3, 0.0))
    parts = [body, lid]
    iron = M('iron')
    for x in (-L * 0.36, 0.0, L * 0.36):
        parts.append(box(f'band{x}', (0.09, D + 0.03, H + 0.015), loc=(x, 0, H * 0.5 + 0.005), mat=iron))
        arch = prism(f'arch{x}', half_circle(R + 0.03, 14, 0.0) + half_circle(R - 0.01, 14, 0.0)[::-1], 0.09, loc=(x, 0, H), mat=iron)
        parts.append(arch)
    parts.append(box('lock', (0.16, 0.05, 0.22), loc=(0, -D * 0.5 - 0.01, H - 0.05), mat=iron))
    parts.append(box('hasp', (0.1, 0.04, 0.14), loc=(0, -D * 0.5 - 0.035, H + 0.02), mat=iron))
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(box(f'corner{sx}{sy}', (0.07, 0.07, 0.2), loc=(sx * (L * 0.5 - 0.02), sy * (D * 0.5 - 0.02), 0.1), mat=iron))
    o = join(parts, 'treasure-chest')
    smooth(o, 40)
    ground(o)
    return o


def crate():
    S = 1.0
    b = box('crate', (S, S, S * 0.92), loc=(0, 0, S * 0.46), mat=M('planks-worn'))
    bevel(b, 0.02, 2)
    apply_mods(b)
    box_uv(b, 1.2, (0.15, 0.0))
    parts = [b]
    w = 0.09
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(box(f'post{sx}{sy}', (w, w, S * 0.92), loc=(sx * (S * 0.5 - w * 0.45), sy * (S * 0.5 - w * 0.45), S * 0.46), mat=M('planks-dark')))
    for sy in (-1, 1):
        parts.append(box(f'diag{sy}', (S * 1.15, 0.05, 0.12), loc=(0, sy * (S * 0.5 + 0.02), S * 0.46), rot=(0, 0.55, 0), mat=M('planks-dark')))
    for p in parts[1:]:
        box_uv(p, 0.8)
    o = join(parts, 'crate')
    smooth(o, 40)
    ground(o)
    return o


def barrel():
    H, R = 1.3, 0.48
    prof = [(0.0, 0.0), (R * 0.92, 0.0)]
    for i in range(1, 8):
        z = H * i / 8
        t = 2 * z / H - 1
        prof.append((R * (1.0 + 0.16 * (1 - t * t)), z))
    prof += [(R * 0.92, H), (0.0, H)]
    b = lathe('barrel', prof, segs=28, mat=M('planks-dark'))
    box_uv(b, 1.1)
    parts = [b]
    for z in (0.14, H * 0.5 - 0.28, H * 0.5 + 0.28, H - 0.14):
        t = 2 * z / H - 1
        parts.append(torus(f'hoop{z}', R * (1.0 + 0.16 * (1 - t * t)) + 0.005, 0.035, loc=(0, 0, z), mat=M('iron'), segs=28, ring=8))
    o = join(parts, 'barrel')
    smooth(o, 40)
    ground(o)
    return o


def sacks(seed=21):
    rng = rand(seed)
    parts = []
    for i, (x, y, r) in enumerate([(0, 0, 0.5), (0.72, 0.15, 0.4), (0.25, -0.65, 0.33)]):
        s = sphere(f'sack{i}', r, (x, y, r * 0.95), subdiv=3, mat=M('burlap'), scale=(1.05, 0.95, 1.1))
        displace(s, 0.14, 0.4, seed=seed + i)
        apply_mods(s)
        clamp_floor(s, 0.0)
        neck = cylinder(f'neck{i}', r * 0.3, r * 0.22, r * 0.45, loc=(x, y, r * 2.0), segs=10, mat=M('burlap'))
        tie = torus(f'tie{i}', r * 0.3, 0.025, loc=(x, y, r * 1.95), mat=M('rope'), segs=16, ring=6)
        parts += [s, neck, tie]
    o = join(parts, 'sacks')
    smooth(o, 60)
    ground(o)
    return o


def lantern():
    S, H = 0.32, 0.5
    parts = [box('base', (S, S, 0.045), loc=(0, 0, 0.02), mat=M('iron')),
             box('top', (S * 0.9, S * 0.9, 0.04), loc=(0, 0, H + 0.045), mat=M('iron')),
             box('glass', (S * 0.8, S * 0.8, H - 0.02), loc=(0, 0, H * 0.5 + 0.04), mat=M('glass'))]
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(cylinder(f'bar{sx}{sy}', 0.018, 0.018, H, loc=(sx * S * 0.44, sy * S * 0.44, H * 0.5 + 0.04), segs=8, mat=M('iron')))
    parts.append(cylinder('cap', S * 0.4, 0.03, 0.12, loc=(0, 0, H + 0.12), segs=4, mat=M('iron')))
    parts.append(torus('handle', 0.13, 0.014, loc=(0, 0, H + 0.26), rot=(math.pi / 2, 0, 0), mat=M('iron'), segs=20, ring=6))
    parts.append(box('door', (0.012, S * 0.5, H * 0.7), loc=(S * 0.41, 0, H * 0.5 + 0.04), mat=M('iron')))
    o = join(parts, 'lantern')
    smooth(o, 40)
    ground(o)
    return o


def signpost():
    post = cylinder('post', 0.075, 0.065, 2.2, loc=(0, 0, 1.1), segs=10, mat=M('planks-worn'))
    box_uv(post, 0.6)
    board = box('board', (1.15, 0.06, 0.36), loc=(0.25, 0.09, 1.75), mat=M('planks-dark'))
    bevel(board, 0.012, 2)
    apply_mods(board)
    box_uv(board, 1.2, (0.4, 0.5))
    parts = [post, board]
    for x in (-0.15, 0.55):
        parts.append(sphere(f'nail{x}', 0.018, (x, 0.125, 1.75), subdiv=1, mat=M('iron')))
    parts.append(cylinder('split', 0.05, 0.02, 0.25, loc=(0.02, 0, 2.3), segs=6, mat=M('planks-worn')))
    o = join(parts, 'signpost')
    smooth(o, 40)
    ground(o)
    return o


def _wheel(name, loc, r=0.42):
    rim = torus(f'{name}rim', r, 0.055, loc=loc, rot=(math.pi / 2, 0, 0), mat=M('planks-dark'), segs=28, ring=8)
    hub = cylinder(f'{name}hub', 0.09, 0.09, 0.16, loc=loc, rot=(math.pi / 2, 0, 0), segs=10, mat=M('iron'))
    parts = [rim, hub]
    for i in range(8):
        a = math.tau * i / 8
        s = cylinder(f'{name}spoke{i}', 0.03, 0.025, r, loc=(loc[0] + math.cos(a) * r * 0.5, loc[1], loc[2] + math.sin(a) * r * 0.5), segs=6, mat=M('planks-worn'))
        aim(s, (math.cos(a), 0, math.sin(a)))
        apply_transform(s)
        parts.append(s)
    return parts


def handcart():
    parts = [box('bed', (1.7, 0.95, 0.07), loc=(0, 0, 0.6), mat=M('planks-worn'))]
    for sy in (-1, 1):
        parts.append(box(f'side{sy}', (1.7, 0.05, 0.42), loc=(0, sy * 0.46, 0.83), mat=M('planks-dark')))
    parts.append(box('front', (0.05, 0.9, 0.42), loc=(-0.83, 0, 0.83), mat=M('planks-dark')))
    for p in parts:
        bevel(p, 0.012, 1)
        apply_mods(p)
        box_uv(p, 1.1)
    parts.append(cylinder('axle', 0.045, 0.045, 1.25, loc=(0.1, 0, 0.42), rot=(math.pi / 2, 0, 0), segs=10, mat=M('iron')))
    for sy in (-1, 1):
        parts += _wheel(f'w{sy}', (0.1, sy * 0.58, 0.42))
        h = cylinder(f'handle{sy}', 0.04, 0.035, 1.5, loc=(1.35, sy * 0.3, 0.62), segs=8, mat=M('planks-worn'))
        aim(h, (1, 0, 0.03))
        apply_transform(h)
        parts.append(h)
        parts.append(cylinder(f'leg{sy}', 0.04, 0.04, 0.58, loc=(-0.6, sy * 0.3, 0.29), segs=8, mat=M('planks-worn')))
    o = join(parts, 'handcart')
    smooth(o, 40)
    ground(o)
    return o


if __name__ == '__main__':
    run('props', [
        ('treasure-chest', treasure_chest),
        ('crate', crate),
        ('barrel', barrel),
        ('sacks', sacks),
        ('lantern', lantern),
        ('signpost', signpost),
        ('handcart', handcart),
    ])
