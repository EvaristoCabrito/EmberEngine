"""Photoreal farmland ground clusters for Farmlands Revisited (v01)."""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emberlib import *  # noqa: E402,F403

ASSET_VERSION = {'tall-grass': 'v02', 'wheat-tile': 'v03', 'hay-bales': 'v02'}


def _leaf(name, base, angle, height, width, bend, mat):
    """A folded, tapered 3D blade, with two faces and a slight natural curve."""
    dx, dy = math.cos(angle), math.sin(angle)
    sx, sy = -dy, dx
    verts = []
    for t, w in ((0.0, width * 0.25), (0.36, width * 0.5), (0.72, width * 0.3)):
        cx = base[0] + dx * bend * t * t
        cy = base[1] + dy * bend * t * t
        z = base[2] + height * t
        verts += [(cx - sx * w, cy - sy * w, z), (cx + sx * w, cy + sy * w, z)]
    tip_t = 1.0
    verts.append((base[0] + dx * bend, base[1] + dy * bend, base[2] + height * tip_t))
    faces = []
    for i in range(2):
        a = i * 2
        faces += [(a, a + 1, a + 3), (a, a + 3, a + 2)]
    faces.append((4, 5, 6))
    me = bpy.data.meshes.new(name + '-mesh')
    me.from_pydata(verts, [], faces)
    me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    return ob


def _patch(name, surface, seed=1):
    bpy.ops.mesh.primitive_cylinder_add(vertices=6, radius=0.92, depth=0.075, location=(0, 0, 0.038))
    ob = bpy.context.object
    ob.name = name + '-hex-soil'
    ob.data.materials.append(surface)
    box_uv(ob, 1.55)
    return ob


def tall_grass(seed=17):
    rng = random.Random(seed)
    pasture = material('farmlands-pasture-surface', color='farmlands-pasture-v01', roughness=0.96)
    greens = [material('grass-' + str(i), flat=c, roughness=0.93) for i, c in enumerate(
        [(0.105, 0.22, 0.055), (0.15, 0.29, 0.07), (0.21, 0.34, 0.09), (0.28, 0.37, 0.1)])]
    parts = [_patch('tall-grass', pasture)]
    for i in range(86):
        a = rng.random() * math.tau
        r = math.sqrt(rng.random()) * 0.8
        x, y = math.cos(a) * r, math.sin(a) * r
        ang = a + rng.uniform(-1.1, 1.1)
        h = rng.uniform(0.42, 0.82)
        w = rng.uniform(0.045, 0.09)
        bend = rng.uniform(0.04, 0.2)
        parts.append(_leaf(f'grass-blade-{i:03}', (x, y, 0.07), ang, h, w, bend, rng.choice(greens)))
    ob = join(parts, 'tall-grass-hex-v01')
    ground(ob)
    return ob


def wheat_tile(seed=23):
    rng = random.Random(seed)
    soil = material('farmlands-wheat-surface', color='farmlands-wheat-v01', roughness=0.94)
    stem = material('wheat-stem', flat=(0.42, 0.32, 0.12), roughness=0.9)
    leaves = [material('wheat-leaf-' + str(i), flat=c, roughness=0.9) for i, c in enumerate(
        [(0.38, 0.39, 0.11), (0.48, 0.44, 0.12), (0.57, 0.48, 0.14)])]
    grain = [material('wheat-grain-' + str(i), flat=c, roughness=0.86) for i, c in enumerate(
        [(0.66, 0.46, 0.15), (0.78, 0.58, 0.23), (0.86, 0.67, 0.31)])]
    parts = [_patch('wheat-field', soil)]
    # Short, orderly crop rows leave the hex outline clear and repeat cleanly on neighboring cells.
    for row in range(7):
        y = -0.66 + row * 0.22
        half = 0.65 if abs(y) < 0.32 else 0.51
        for col in range(5):
            x = -half + col * (2 * half / 4) + rng.uniform(-0.035, 0.035)
            h = rng.uniform(0.88, 1.16)
            parts.append(cylinder(f'wheat-stalk-{row}-{col}', 0.018, 0.011, h, loc=(x, y, 0.075 + h / 2), segs=5, mat=stem))
            for k in range(2):
                a = rng.choice((-1, 1)) * rng.uniform(0.5, 1.1)
                parts.append(_leaf(f'wheat-leaf-{row}-{col}-{k}', (x, y, 0.25 + h * (0.25 + 0.2 * k)), a,
                                   rng.uniform(0.22, 0.36), 0.035, 0.04, rng.choice(leaves)))
            # Three small kernels on each head read as ripe wheat at normal gameplay distance.
            for k in range(3):
                z = 0.075 + h - 0.025 * k
                g = sphere(f'wheat-kernel-{row}-{col}-{k}', rng.uniform(0.028, 0.037),
                           (x + (k - 1) * 0.026, y + (k % 2) * 0.022, z), subdiv=1, mat=rng.choice(grain), scale=(0.75, 0.8, 1.5))
                parts.append(g)
    ob = join(parts, 'wheat-tile-hex-v01')
    ground(ob)
    return ob


def hay_bales(seed=31):
    rng = random.Random(seed)
    straw = material('hay-straw', flat=(0.48, 0.33, 0.13), roughness=0.97)
    rope = material('hay-rope', flat=(0.25, 0.19, 0.1), roughness=0.95)
    soil = material('farmlands-hay-yard-surface', color='farmlands-tilled-soil-v01', roughness=0.98)
    parts = [_patch('hay-yard', soil)]
    for i, (x, y, z, rot) in enumerate([(-0.35, -0.06, 0.0, 0.0), (0.35, -0.06, 0.0, 0.04),
                                         (0.0, 0.42, 0.68, -0.05)]):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, z + 0.33))
        bale = bpy.context.object
        bale.name = f'hay-bale-{i}'
        bale.dimensions = (0.82, 0.62, 0.64)
        bale.rotation_euler[2] = rot + rng.uniform(-0.06, 0.06)
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        bale.data.materials.append(straw)
        bevel = bale.modifiers.new('soft compressed straw edges', 'BEVEL')
        bevel.width = 0.08
        bevel.segments = 3
        bpy.context.view_layer.objects.active = bale
        bpy.ops.object.modifier_apply(modifier=bevel.name)
        parts.append(bale)
        for side in (-1, 1):
            tie = cylinder(f'bale-tie-{i}-{side}', 0.018, 0.018, 0.59, loc=(x + side * 0.21, y, z + 0.33),
                           rot=(math.pi / 2, 0, 0), segs=6, mat=rope)
            parts.append(tie)
    ob = join(parts, 'hay-bales-hex-v01')
    ground(ob)
    return ob


if __name__ == '__main__':
    builders = [
        ('tall-grass', tall_grass),
        ('wheat-tile', wheat_tile),
        ('hay-bales', hay_bales),
    ]
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    only = set(argv)
    for name, builder in builders:
        if only and name not in only:
            continue
        reset()
        obj = builder()
        report = export(obj, 'farmlands', name, ASSET_VERSION[name])
        print(f'[ok] farmlands/{name}: {report["triangles"]} tris, {report["size"]}')
