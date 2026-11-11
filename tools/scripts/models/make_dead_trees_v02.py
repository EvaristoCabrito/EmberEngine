"""Dead trees grown from scratch: an upright trunk with a root flare and roots, limbs that leave it at a real
angle and bend upward, each limb splitting into thinner limbs down to twigs, some limbs snapped off.

  blender --background --python tools/scripts/models/make_dead_trees_v02.py -- [--preview out.png] [--export dir]

Every tree stands on z = 0 with the trunk centred on the origin.
"""
import math
import random
import sys
from pathlib import Path

import bpy
from mathutils import Vector, Quaternion, noise

ROOT = Path(__file__).resolve().parents[3]
BARK = ROOT / 'content/textures/polyhaven/bark_brown_02'
TINT = (0.60, 0.47, 0.43, 1.0)  # dead, dark bark
TILE = 2.0  # metres of bark one texture repeat covers

SIDES = [16, 9, 6, 4, 3]
SEGS = [24, 11, 7, 5, 3]

# one entry per tree: what makes its outline different from the others
TREES = {
    'old-oak': dict(seed=11, height=8.2, radius=0.46, first=0.40, limbs=9, spread=(55, 78), reach=0.62, broken=0.15),
    'tall-ash': dict(seed=23, height=10.0, radius=0.33, first=0.48, limbs=10, spread=(38, 58), reach=0.42, broken=0.10),
    'forked-elm': dict(seed=37, height=9.0, radius=0.42, first=0.30, limbs=8, spread=(48, 70), reach=0.52, broken=0.15,
                       fork=0.42),
    'broken-snag': dict(seed=41, height=6.2, radius=0.50, first=0.42, limbs=6, spread=(55, 80), reach=0.58, broken=0.45,
                        snapped=True),
    'wide-beech': dict(seed=71, height=7.4, radius=0.52, first=0.40, limbs=9, spread=(58, 80), reach=0.68, broken=0.20),
}


class Mesh:
    def __init__(self):
        self.verts, self.faces, self.uvs = [], [], []

    def tube(self, pts, radii, sides, shape=None, cap=False, rng=None):
        """A tube along pts. shape(i, angle) scales the radius of ring i at that angle. cap closes a snapped end."""
        n = len(pts)
        tangents = []
        for i in range(n):
            a = pts[max(i - 1, 0)]
            b = pts[min(i + 1, n - 1)]
            tangents.append((b - a).normalized())
        t0 = tangents[0]
        ref = Vector((1, 0, 0)) if abs(t0.x) < 0.9 else Vector((0, 1, 0))
        nrm = t0.cross(ref).normalized()
        if abs(t0.z) > 0.9:  # trunks: keep angle 0 on world +X so root lobes land where the roots are
            nrm = Vector((1, 0, 0))
        u_span = max(0.2, 2 * math.pi * radii[0] / TILE)
        if u_span > 0.75:
            u_span = round(u_span) or 1
        rings, vs = [], []
        length = 0.0
        pointed = not cap and radii[-1] < 0.012
        for i in range(n):
            if i:
                q = tangents[i - 1].rotation_difference(tangents[i])
                nrm = (q @ nrm).normalized()
                length += (pts[i] - pts[i - 1]).length
            vs.append(length / TILE)
            if pointed and i == n - 1:
                rings.append([len(self.verts)])
                self.verts.append(pts[i][:])
                break
            bi = tangents[i].cross(nrm).normalized()
            ring = []
            for j in range(sides):
                ang = 2 * math.pi * j / sides
                r = radii[i] * (shape(i, ang) if shape else 1.0)
                p = pts[i] + (nrm * math.cos(ang) + bi * math.sin(ang)) * r
                if cap and i == n - 1:
                    p += tangents[i] * radii[i] * rng.uniform(-0.5, 0.9)
                ring.append(len(self.verts))
                self.verts.append(p[:])
            rings.append(ring)
        for i in range(len(rings) - 1):
            a, b = rings[i], rings[i + 1]
            for j in range(sides):
                k = (j + 1) % sides
                u0, u1 = u_span * j / sides, u_span * (j + 1) / sides
                if len(b) == 1:
                    self.faces.append((a[j], a[k], b[0]))
                    self.uvs.append(((u0, vs[i]), (u1, vs[i]), ((u0 + u1) / 2, vs[i + 1])))
                else:
                    self.faces.append((a[j], a[k], b[k], b[j]))
                    self.uvs.append(((u0, vs[i]), (u1, vs[i]), (u1, vs[i + 1]), (u0, vs[i + 1])))
        if cap:
            c = len(self.verts)
            self.verts.append((pts[-1] - tangents[-1] * radii[-1] * 0.5)[:])
            last = rings[-1]
            for j in range(sides):
                k = (j + 1) % sides
                self.faces.append((last[j], last[k], c))
                self.uvs.append(((0.1, 0.1), (0.2, 0.1), (0.15, 0.2)))

    def to_object(self, name, material):
        me = bpy.data.meshes.new(name)
        me.from_pydata(self.verts, [], self.faces)
        uv = me.uv_layers.new(name='UVMap')
        flat = [c for face in self.uvs for corner in face for c in corner]
        uv.data.foreach_set('uv', flat)
        me.polygons.foreach_set('use_smooth', [True] * len(me.polygons))
        me.materials.append(material)
        me.update()
        ob = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(ob)
        return ob


def path(rng, start, direction, length, segs, gnarl, lift):
    """Points of one limb: it wanders by gnarl each step and bends toward the sky by lift."""
    pts = [start.copy()]
    d = direction.normalized()
    step = length / segs
    for i in range(segs):
        jitter = Vector((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1))) * gnarl
        d = (d + jitter + Vector((0, 0, lift))).normalized()
        pts.append(pts[-1] + d * step)
    return pts


def taper(r0, segs, tip, power=0.85, collar=False):
    out = []
    for i in range(segs + 1):
        t = i / segs
        r = r0 * (tip + (1 - tip) * (1 - t) ** power)
        if collar:  # thicker where the limb leaves its parent
            r *= 1 + 0.35 * math.exp(-t * 9)
        out.append(r)
    return out


def at(pts, t):
    f = t * (len(pts) - 1)
    i = min(int(f), len(pts) - 2)
    return pts[i].lerp(pts[i + 1], f - i), (pts[i + 1] - pts[i]).normalized(), i, f - i


def limb(mesh, rng, start, direction, length, r0, level, spec, lift):
    broken = level in (1, 2) and rng.random() < spec['broken']
    if broken:
        length *= rng.uniform(0.25, 0.5)
    segs = max(3, SEGS[level] // (2 if broken else 1))
    gnarl = [0.0, 0.15, 0.20, 0.24, 0.26][level]
    pts = path(rng, start, direction, length, segs, gnarl, lift)
    radii = taper(r0, segs, 0.62 if broken else 0.0, collar=True)
    mesh.tube(pts, radii, SIDES[level], cap=broken, rng=rng)
    if level >= 4:
        return
    count = [0, 6, 4, 2][level]
    if broken:
        count = rng.randint(0, 1)
    spin = rng.uniform(0, 6.28)
    for c in range(count):
        t = 0.28 + 0.68 * (c + rng.uniform(0.15, 0.85)) / max(count, 1)
        p, tan, i, f = at(pts, t)
        pr = radii[i] + (radii[i + 1] - radii[i]) * f
        cr = max(0.011, pr * rng.uniform(0.5, 0.72))
        spin += math.radians(137.5 + rng.uniform(-35, 35))
        side = tan.cross(Vector((0, 0, 1)))
        if side.length < 0.05:
            side = Vector((1, 0, 0))
        side = Quaternion(tan, spin) @ side.normalized()
        ang = math.radians(rng.uniform(32, 62))
        d = tan * math.cos(ang) + side * math.sin(ang)
        cl = length * rng.uniform(0.50, 0.72) * (1 - 0.30 * t)
        limb(mesh, rng, p, d, max(cl, 0.18), cr, level + 1, spec, lift * 0.6)


def stem(mesh, rng, start, direction, length, r0, tip, spec, first, limbs, roots=None, cap=False):
    """A trunk or one of the leaders of a forked trunk, and the limbs that leave it."""
    segs = SEGS[0]
    pts = path(rng, start, direction, length, segs, 0.045, 0.22)
    drift = pts[-1] - (start + direction.normalized() * length)
    pts = [p - Vector((drift.x, drift.y, 0)) * (i / segs) * 0.85 for i, p in enumerate(pts)]
    radii = taper(r0, segs, tip, power=0.9)
    off = Vector((spec['seed'] * 3.1, spec['seed'] * 1.7, 0))

    def shape(i, ang):
        h = pts[i].z
        s = 1 + 0.07 * noise.noise(Vector((math.cos(ang) * 1.2, math.sin(ang) * 1.2, h * 0.55)) + off)
        if roots:  # only a gentle widening where the trunk meets the ground, no root lobes
            s *= 1 + 0.30 * math.exp(-h / 0.28)
        return s

    mesh.tube(pts, radii, SIDES[0], shape=shape, cap=cap, rng=rng)
    spin = rng.uniform(0, 6.28)
    for c in range(limbs):
        t = first + (0.97 - first) * (c + rng.uniform(0.1, 0.9)) / limbs
        p, tan, i, f = at(pts, t)
        pr = radii[i] + (radii[i + 1] - radii[i]) * f
        spin += math.radians(137.5 + rng.uniform(-25, 25))
        side = Vector((math.cos(spin), math.sin(spin), 0))
        lo, hi = spec['spread']
        ang = math.radians((lo + (hi - lo) * rng.random()) * (1 - 0.40 * (t - first) / (1 - first)))
        d = tan * math.cos(ang) + side * math.sin(ang)
        cr = max(0.03, pr * rng.uniform(0.50, 0.70))
        cl = spec['height'] * spec['reach'] * (1 - 0.5 * (t - first) / (1 - first)) * rng.uniform(0.8, 1.15)
        limb(mesh, rng, p, d, cl, cr, 1, spec, 0.05)
    return pts, radii


def build(name, spec, material):
    rng = random.Random(spec['seed'])
    mesh = Mesh()
    h, r = spec['height'], spec['radius']
    nroots = rng.randint(5, 6)
    roots = [2 * math.pi * (k + rng.uniform(-0.25, 0.25)) / nroots for k in range(nroots)]
    up = Vector((0, 0, 1))
    base = Vector((0, 0, 0.0))
    if spec.get('fork'):
        fh = h * spec['fork']
        pts, radii = stem(mesh, rng, base, up, fh + 0.15, r, 0.80, spec, 0.75, 1, roots)
        az = rng.uniform(0, 6.28)
        for k in range(2):
            lean = math.radians(rng.uniform(15, 21))
            d = Vector((math.cos(az + math.pi * k) * math.sin(lean), math.sin(az + math.pi * k) * math.sin(lean),
                        math.cos(lean)))
            stem(mesh, rng, pts[-3], d, (h - fh) * rng.uniform(0.9, 1.05), radii[-1] * (0.78 - 0.1 * k), 0.0, spec,
                 0.22, spec['limbs'] // 2 + 1)
    elif spec.get('snapped'):
        stem(mesh, rng, base, up, h + 0.15, r, 0.52, spec, spec['first'], spec['limbs'], roots, cap=True)
    else:
        stem(mesh, rng, base, up, h + 0.15, r, 0.0, spec, spec['first'], spec['limbs'], roots)
    # no roots (the user, 2026-11-11: "remove the root"): the trunk stands straight on the ground
    return mesh.to_object(name, material)


def bark_material():
    mat = bpy.data.materials.new('DeadBark')
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')

    def image(suffix, data=False):
        img = bpy.data.images.load(str(BARK / f'bark_brown_02_{suffix}_2k.jpg'))
        img.scale(1024, 1024)
        img.pack()
        if data:
            img.colorspace_settings.name = 'Non-Color'
        node = nt.nodes.new('ShaderNodeTexImage')
        node.image = img
        return node

    tint = nt.nodes.new('ShaderNodeMix')  # the glTF exporter writes this as the base colour factor
    tint.data_type = 'RGBA'
    tint.blend_type = 'MULTIPLY'
    tint.inputs[0].default_value = 1.0
    tint.inputs[7].default_value = TINT
    nt.links.new(image('diff').outputs['Color'], tint.inputs[6])
    nt.links.new(tint.outputs[2], bsdf.inputs['Base Color'])
    nmap = nt.nodes.new('ShaderNodeNormalMap')
    nt.links.new(image('nor_gl', True).outputs['Color'], nmap.inputs['Color'])
    nt.links.new(nmap.outputs['Normal'], bsdf.inputs['Normal'])
    bsdf.inputs['Roughness'].default_value = 0.9
    bsdf.inputs['Metallic'].default_value = 0.0
    return mat


def preview(objects, out):
    scene = bpy.context.scene
    x = 0.0
    for ob in objects:
        ob.location.x = x
        x += 9.5
    mid = (x - 9.5) / 2
    ground = bpy.data.meshes.new('ground')
    ground.from_pydata([(-8, -9, 0), (x, -9, 0), (x, 9, 0), (-8, 9, 0)], [], [(0, 1, 2, 3)])
    gob = bpy.data.objects.new('ground', ground)
    gm = bpy.data.materials.new('ground')
    gm.diffuse_color = (0.20, 0.17, 0.16, 1)
    gm.use_nodes = True
    next(n for n in gm.node_tree.nodes if n.type == 'BSDF_PRINCIPLED').inputs['Base Color'].default_value = (0.2, 0.17, 0.16, 1)
    ground.materials.append(gm)
    scene.collection.objects.link(gob)
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    scene.collection.objects.link(cam)
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = x + 3
    pitch = math.radians(62)
    cam.location = (mid, -40 * math.sin(pitch), 5.6 + 40 * math.cos(pitch))
    cam.rotation_euler = (pitch, 0, 0)
    scene.camera = cam
    sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN'))
    sun.data.energy = 4.0
    sun.rotation_euler = (math.radians(50), 0, math.radians(35))
    scene.collection.objects.link(sun)
    world = bpy.data.worlds.new('w')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.55, 0.58, 0.62, 1)
    scene.world = world
    for engine in ('BLENDER_EEVEE', 'BLENDER_EEVEE_NEXT'):
        try:
            scene.render.engine = engine
            break
        except TypeError:
            pass
    scene.render.resolution_x = 2000
    scene.render.resolution_y = 760
    scene.render.filepath = out
    bpy.ops.render.render(write_still=True)


def main():
    args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    bpy.ops.wm.read_factory_settings(use_empty=True)
    material = bark_material()
    objects = []
    for name, spec in TREES.items():
        ob = build(name, spec, material)
        objects.append(ob)
        xs = [v.co.x for v in ob.data.vertices]
        ys = [v.co.y for v in ob.data.vertices]
        zs = [v.co.z for v in ob.data.vertices]
        tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
        print(f'[tree] {name}: {tris} triangles, {max(xs) - min(xs):.2f} x {max(ys) - min(ys):.2f} wide, {max(zs):.2f} tall')
    if '--export' in args:
        out = Path(args[args.index('--export') + 1])
        out.mkdir(parents=True, exist_ok=True)
        for ob in objects:
            bpy.ops.object.select_all(action='DESELECT')
            ob.select_set(True)
            bpy.context.view_layer.objects.active = ob
            bpy.ops.export_scene.gltf(filepath=str(out / f'{ob.name}-v01.glb'), export_format='GLB', use_selection=True,
                                      export_image_format='JPEG', export_yup=True, export_apply=True)
        bpy.ops.wm.save_as_mainfile(filepath=str(out / 'dead-trees-v02.blend'))
    if '--preview' in args:
        preview(objects, args[args.index('--preview') + 1])


main()
