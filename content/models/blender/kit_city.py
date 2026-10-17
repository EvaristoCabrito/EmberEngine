"""Kit D: city streets and settlement objects (content/models/city)."""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emberlib import *  # noqa


def well(seed=31):
    ring = lathe('ring', [(0.72, 0.0), (0.98, 0.0), (1.0, 0.5), (0.95, 0.95), (0.72, 0.95), (0.72, 0.0)], segs=26, mat=M('masonry-city'))
    subdivide(ring, 1, simple=True)
    displace(ring, 0.05, 0.3, seed=seed)
    apply_mods(ring)
    box_uv(ring, 1.3)
    parts = [ring]
    for sx in (-1, 1):
        parts.append(box(f'post{sx}', (0.13, 0.13, 2.1), loc=(sx * 0.82, 0, 1.05 + 0.6), mat=M('planks-worn')))
    parts.append(box('beam', (2.0, 0.14, 0.14), loc=(0, 0, 2.75), mat=M('planks-worn')))
    parts.append(cylinder('spindle', 0.09, 0.09, 1.55, loc=(0, 0, 2.45), rot=(0, math.pi / 2, 0), segs=12, mat=M('planks-dark')))
    parts.append(cylinder('crankarm', 0.03, 0.03, 0.35, loc=(0.92, 0, 2.6), segs=8, mat=M('iron')))
    parts.append(cylinder('crank', 0.025, 0.025, 0.3, loc=(1.05, 0, 2.77), rot=(0, math.pi / 2, 0), segs=8, mat=M('iron')))
    roof = prism('roof', [(-1.1, 0.0), (0.0, 0.7), (1.1, 0.0), (1.1, -0.08), (0.0, 0.62), (-1.1, -0.08)], 1.4, loc=(0, 0, 2.85), mat=M('planks-dark'))
    parts.append(roof)
    parts.append(cylinder('rope', 0.012, 0.012, 1.1, loc=(0, 0, 1.9), segs=6, mat=M('rope')))
    bucket = lathe('bucket', [(0, 0), (0.18, 0), (0.22, 0.32), (0.19, 0.32), (0.155, 0.03), (0, 0.03)], segs=14, loc=(0, 0, 1.1), mat=M('planks-dark'))
    parts.append(bucket)
    parts.append(torus('bucket-hoop', 0.215, 0.012, loc=(0, 0, 1.38), mat=M('iron'), segs=16, ring=6))
    for p in parts[1:]:
        if p.name not in ('rope', 'bucket-hoop', 'crank', 'crankarm'):
            box_uv(p, 1.0)
    o = join(parts, 'well')
    smooth(o, 40)
    ground(o)
    return o


def market_stall(seed=32):
    parts = [box('counter', (2.3, 0.9, 0.95), loc=(0, 0.1, 0.475), mat=M('planks-worn')),
             box('countertop', (2.4, 1.0, 0.06), loc=(0, 0.1, 0.98), mat=M('planks-dark'))]
    for p in parts:
        bevel(p, 0.015, 1)
        apply_mods(p)
        box_uv(p, 1.2)
    for sx in (-1, 1):
        for y, h in ((-0.55, 2.45), (0.85, 2.15)):
            parts.append(box(f'post{sx}{y}', (0.11, 0.11, h), loc=(sx * 1.1, y, h * 0.5), mat=M('planks-worn')))
    for y, z in ((-0.55, 2.42), (0.85, 2.12)):
        parts.append(box(f'rail{y}', (2.3, 0.08, 0.08), loc=(0, y, z), mat=M('planks-worn')))
    canopy = plane('canopy', (2.6, 1.7), loc=(0, 0.15, 2.3), rot=(0.21, 0, 0))
    canopy.data.materials.append(M('canvas'))
    subdivide(canopy, 3, simple=True)
    displace(canopy, 0.1, 0.45, seed=seed)
    apply_mods(canopy)
    sol = canopy.modifiers.new('solid', 'SOLIDIFY')
    sol.thickness = 0.02
    apply_mods(canopy)
    parts.append(canopy)
    o = join(parts, 'market-stall')
    smooth(o, 40)
    ground(o)
    return o


def street_lamp():
    base = lathe('base', [(0, 0), (0.38, 0), (0.38, 0.12), (0.22, 0.2), (0.14, 0.5), (0.1, 0.7), (0, 0.7)], segs=16, mat=M('iron'))
    post = cylinder('post', 0.075, 0.055, 2.9, loc=(0, 0, 2.1), segs=12, mat=M('iron'))
    parts = [base, post, torus('collar', 0.085, 0.02, loc=(0, 0, 1.9), mat=M('iron'), segs=16, ring=6)]
    S, H, Z = 0.42, 0.62, 3.55
    parts.append(box('floor', (S, S, 0.05), loc=(0, 0, Z), mat=M('iron')))
    parts.append(box('glass', (S * 0.84, S * 0.84, H), loc=(0, 0, Z + H * 0.5 + 0.02), mat=M('glass')))
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(cylinder(f'bar{sx}{sy}', 0.02, 0.02, H + 0.05, loc=(sx * S * 0.46, sy * S * 0.46, Z + H * 0.5 + 0.02), segs=6, mat=M('iron')))
    parts.append(cylinder('cap', S * 0.72, 0.04, 0.3, loc=(0, 0, Z + H + 0.17), segs=4, mat=M('iron')))
    parts.append(sphere('finial', 0.05, (0, 0, Z + H + 0.36), subdiv=2, mat=M('brass')))
    o = join(parts, 'street-lamp')
    smooth(o, 40)
    ground(o)
    return o


def bench():
    seat = box('seat', (1.9, 0.52, 0.09), loc=(0, 0, 0.6), mat=M('planks-worn'))
    bevel(seat, 0.02, 2)
    apply_mods(seat)
    box_uv(seat, 1.2, (0.2, 0.3))
    parts = [seat]
    for sx in (-1, 1):
        parts.append(box(f'leg{sx}', (0.08, 0.44, 0.56), loc=(sx * 0.75, 0, 0.28), mat=M('iron')))
        parts.append(box(f'foot{sx}', (0.1, 0.56, 0.05), loc=(sx * 0.75, 0, 0.025), mat=M('iron')))
    parts.append(box('stretcher', (1.5, 0.06, 0.06), loc=(0, 0, 0.3), mat=M('iron')))
    o = join(parts, 'bench')
    smooth(o, 40)
    ground(o)
    return o


def rubble(seed=35):
    rng = rand(seed)
    parts = []
    for i in range(16):
        s = rng.uniform(0.22, 0.62)
        b = box(f'block{i}', (s, s * rng.uniform(0.6, 1.0), s * rng.uniform(0.5, 0.9)),
                loc=(rng.uniform(-0.75, 0.75), rng.uniform(-0.65, 0.65), rng.uniform(0.05, 0.45) * (1 if i > 5 else 0.3)),
                rot=(rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5), rng.uniform(0, 3.14)), mat=M('masonry' if i % 3 else 'masonry-mossy'))
        bevel(b, 0.03, 1)
        displace(b, 0.04, 0.2, seed=seed + i)
        apply_mods(b)
        box_uv(b, 1.1)
        parts.append(b)
    dust = cylinder('dust', 1.1, 0.95, 0.12, loc=(0, 0, 0.06), segs=18, mat=M('ruins-floor'))
    displace(dust, 0.08, 0.3, seed=seed + 40)
    apply_mods(dust)
    box_uv(dust, 1.5)
    parts.append(dust)
    o = join(parts, 'rubble')
    clamp_floor(o, 0.0)
    smooth(o, 40)
    ground(o)
    return o


if __name__ == '__main__':
    run('city', [
        ('well', well),
        ('market-stall', market_stall),
        ('street-lamp', street_lamp),
        ('bench', bench),
        ('rubble', rubble),
    ])
