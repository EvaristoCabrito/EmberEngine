"""Kit B: snowy Mordavia (content/models/winter). Reuses the forest builders with winter materials."""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emberlib import *  # noqa
import kit_forest

NEEDLE = (0.07, 0.19, 0.1)


def snowy_pine(seed=11):
    rng = rand(seed)
    parts = []
    trunk = cylinder('trunk', 0.38, 0.1, 7.2, loc=(0, 0, 3.6), segs=14, mat=M('bark'))
    subdivide(trunk, 1, simple=True)
    displace(trunk, 0.08, 0.3, seed=seed)
    apply_mods(trunk)
    vertex_colors(trunk, (0.24, 0.18, 0.13), 0.35, 0.4, seed)
    parts.append(trunk)
    tiers, snow = [], []
    n = 7
    for i in range(n):
        z = 1.4 + i * 0.82
        r = 2.5 - i * 0.3 + rng.uniform(-0.12, 0.12)
        h = 1.25
        t = cylinder(f'tier{i}', r, 0.05, h, loc=(0, 0, z + h * 0.5), segs=22, mat=M('needles'), caps=True)
        subdivide(t, 2, simple=True)
        displace(t, 0.22, 0.45, seed=seed * 3 + i)
        apply_mods(t)
        tiers.append(t)
        s = cylinder(f'snow{i}', r * 0.88, 0.04, h * 0.42, loc=(0, 0, z + h * 0.78), segs=22, mat=M('snow'), caps=True)
        subdivide(s, 2, simple=True)
        displace(s, 0.12, 0.35, seed=seed * 5 + i)
        apply_mods(s)
        box_uv(s, 1.3)
        snow.append(s)
    top = cylinder('top', 0.45, 0.0, 1.3, loc=(0, 0, 7.0), segs=14, mat=M('needles'))
    tiers.append(top)
    needles = join(tiers, 'needles')
    vertex_colors(needles, NEEDLE, 0.4, 0.45, seed + 2, gradient=(1.0, 7.5, 0.7))
    smooth(needles, 70)
    caps = join(snow, 'snowcaps')
    smooth(caps, 70)
    o = join([trunk, needles, caps], 'snowy-pine')
    ground(o)
    return o


def frost_dead_tree():
    return kit_forest.dead_tree(seed=12, pale=True)


def snow_fallen_log():
    o = kit_forest.fallen_log(seed=13, snow=True)
    slab = sphere('slab', 1.0, (0.2, 0.05, 0.95), subdiv=3, mat=M('snow'), scale=(2.1, 0.5, 0.22))
    displace(slab, 0.12, 0.5, seed=13)
    apply_mods(slab)
    box_uv(slab, 1.3)
    smooth(slab, 70)
    o = join([o, slab], 'snow-fallen-log')
    ground(o)
    return o


def snow_boulder():
    return kit_forest.moss_boulder(seed=14, snow=True)


if __name__ == '__main__':
    run('winter', [
        ('snowy-pine', snowy_pine),
        ('frost-dead-tree', frost_dead_tree),
        ('snow-fallen-log', snow_fallen_log),
        ('snow-boulder', snow_boulder),
    ])
