"""Kit E: tavern interior (content/models/tavern). The fireplace is the Meshy hearth (export_hearth.py)."""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emberlib import *  # noqa


def table():
    L, D, H = 2.5, 1.15, 1.25
    top = box('top', (L, D, 0.11), loc=(0, 0, H - 0.055), mat=M('planks-worn'))
    bevel(top, 0.02, 2)
    apply_mods(top)
    box_uv(top, 1.3, (0.2, 0.1))
    parts = [top]
    for sx in (-1, 1):
        for sy in (-1, 1):
            leg = box(f'leg{sx}{sy}', (0.14, 0.14, H - 0.11), loc=(sx * (L * 0.5 - 0.22), sy * (D * 0.5 - 0.2), (H - 0.11) * 0.5), mat=M('planks-dark'))
            box_uv(leg, 0.7)
            parts.append(leg)
        s = box(f'stretcher{sx}', (0.08, D - 0.5, 0.1), loc=(sx * (L * 0.5 - 0.22), 0, 0.3), mat=M('planks-dark'))
        box_uv(s, 0.7)
        parts.append(s)
    s = box('stretcher', (L - 0.5, 0.08, 0.1), loc=(0, 0, 0.3), mat=M('planks-dark'))
    box_uv(s, 0.7)
    parts.append(s)
    o = join(parts, 'table')
    smooth(o, 40)
    ground(o)
    return o


def chair():
    S, H = 0.62, 0.68
    seat = box('seat', (S, S, 0.06), loc=(0, 0, H - 0.03), mat=M('planks-worn'))
    bevel(seat, 0.012, 2)
    apply_mods(seat)
    box_uv(seat, 0.8)
    parts = [seat]
    for sx in (-1, 1):
        for sy in (-1, 1):
            h = H - 0.06 if sy > 0 else 1.45
            parts.append(box(f'leg{sx}{sy}', (0.07, 0.07, h), loc=(sx * (S * 0.5 - 0.05), sy * (S * 0.5 - 0.05), h * 0.5), mat=M('planks-dark')))
    for z in (1.0, 1.25):
        parts.append(box(f'bar{z}', (S - 0.1, 0.05, 0.12), loc=(0, -(S * 0.5 - 0.05), z), mat=M('planks-dark')))
    for sx in (-1, 1):
        parts.append(box(f'rung{sx}', (0.05, S - 0.12, 0.05), loc=(sx * (S * 0.5 - 0.05), 0, 0.25), mat=M('planks-dark')))
    for p in parts[1:]:
        box_uv(p, 0.6)
    o = join(parts, 'chair')
    smooth(o, 40)
    ground(o)
    return o


def counter():
    L, D, H = 3.2, 0.95, 1.45
    front = box('front', (L, D, H - 0.1), loc=(0, 0, (H - 0.1) * 0.5), mat=M('planks-dark'))
    bevel(front, 0.015, 1)
    apply_mods(front)
    box_uv(front, 1.3)
    top = box('top', (L + 0.15, D + 0.2, 0.1), loc=(0, 0, H - 0.05), mat=M('planks-worn'))
    bevel(top, 0.025, 2)
    apply_mods(top)
    box_uv(top, 1.3, (0.4, 0.0))
    parts = [front, top]
    # the back is open shelving: cut two recesses
    for z in (0.35, 0.95):
        cutter = box(f'cut{z}', (L - 0.3, D * 0.6, 0.45), loc=(0, D * 0.35, z), mat=None)
        boolean(front, cutter)
    for x in (-L * 0.3, 0, L * 0.3):
        parts.append(box(f'panel{x}', (0.06, D * 0.3, H - 0.2), loc=(x, -D * 0.5 + 0.2, (H - 0.2) * 0.5), mat=M('planks-worn')))
    parts.append(box('rail', (L + 0.1, 0.05, 0.05), loc=(0, -D * 0.5 - 0.02, 0.35), mat=M('iron')))
    o = join(parts, 'counter')
    smooth(o, 40)
    ground(o)
    return o


def bottles_jugs(seed=44):
    rng = rand(seed)
    parts = []
    bottle = [(0, 0), (0.085, 0), (0.09, 0.3), (0.06, 0.38), (0.03, 0.42), (0.03, 0.55), (0.035, 0.57), (0, 0.57)]
    jug = [(0, 0), (0.14, 0), (0.19, 0.18), (0.17, 0.36), (0.1, 0.44), (0.11, 0.5), (0, 0.5)]
    cup = [(0, 0), (0.07, 0), (0.08, 0.2), (0.0, 0.2)]
    spots = [((0, 0), bottle, 'dark-glass', 1.0), ((0.28, 0.1), bottle, 'dark-glass', 0.9), ((-0.3, 0.08), bottle, 'dark-glass', 1.1),
             ((0.1, -0.42), jug, 'clay', 1.0), ((-0.45, -0.3), jug, 'clay', 0.85), ((0.45, -0.3), cup, 'brass', 1.0), ((0.3, 0.42), cup, 'clay', 1.0)]
    for i, ((x, y), prof, mat, k) in enumerate(spots):
        p = [(r * k, z * k) for r, z in prof]
        o = lathe(f'vessel{i}', p, segs=18, loc=(x, y, 0), mat=M(mat))
        parts.append(o)
        if prof is jug:
            parts.append(torus(f'handle{i}', 0.1 * k, 0.02, loc=(x + 0.19 * k, y, 0.3 * k), rot=(0, math.pi / 2, 0), mat=M(mat), segs=14, ring=6))
    o = join(parts, 'bottles-jugs')
    smooth(o, 50)
    ground(o)
    return o


if __name__ == '__main__':
    run('tavern', [
        ('table', table),
        ('chair', chair),
        ('counter', counter),
        ('bottles-jugs', bottles_jugs),
    ])
