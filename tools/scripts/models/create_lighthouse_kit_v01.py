"""The Light House kit v01 (2026-11-11; the user: "create a new dungeon the Light House complete with new 3D Objects ...
Use some of the texture archives we have been building over time"; picked: lamp and lens, lighthouse machinery, sea and dock pieces).

  blender -b -t 4 -P tools/scripts/models/create_lighthouse_kit_v01.py [-- name ...]

Nine pieces, each content/models/lighthouse/<name>-v01.glb, all of them in lighthouse-kit-v01.blend, and their measures in
build/models-tmp/lighthouse/manifest.json (register_lighthouse_kit_v01.py reads it). Never an overwrite.
1 unit = one hex radius (a human is 2.65 units). Every part carries a photograph from the archives:
content/textures/ambientcg (metal-024, planks-037a, wood-060, lava-001), content/models/textures (the user's wt-iron, wt-dark-oak,
brass-detail, lamp-glass + its glow, dark-green-glass, the hemp rope).
"""
from pathlib import Path
import json
import math
import random
import sys
import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[3]
KIT, VERSION = "lighthouse", "v01"
OUT = ROOT / "content" / "models" / KIT
TMP = ROOT / "build" / "models-tmp" / KIT
ACG = ROOT / "content" / "textures" / "ambientcg"
MT = ROOT / "content" / "models" / "textures"
TEXTURE_SIZE = 1024
TMP.mkdir(parents=True, exist_ok=True)
OUT.mkdir(parents=True, exist_ok=True)
bpy.context.preferences.filepaths.save_version = 0
I = Matrix.Identity(4)
pi, cos, sin = math.pi, math.cos, math.sin


def T(x=0.0, y=0.0, z=0.0):
    return Matrix.Translation((x, y, z))


def Rx(deg):
    return Matrix.Rotation(math.radians(deg), 4, 'X')


def Ry(deg):
    return Matrix.Rotation(math.radians(deg), 4, 'Y')


def Rz(deg):
    return Matrix.Rotation(math.radians(deg), 4, 'Z')


def along(p0, p1, up=(0, 0, 1)):
    """A frame at p0 with local X towards p1."""
    p0, p1 = Vector(p0), Vector(p1)
    x = (p1 - p0).normalized()
    u = Vector(up)
    if abs(x.dot(u)) > 0.98:
        u = Vector((0, 1, 0))
    y = u.cross(x).normalized()
    z = x.cross(y)
    m = Matrix((x, y, z)).transposed().to_4x4()
    m.translation = p0
    return m


# --- materials: every one a photograph ---------------------------------------------------------------------------------------
def image(path, colour):
    path = Path(path)
    small = TMP / f"{path.stem}-{TEXTURE_SIZE}.jpg"
    if not small.exists():
        src = bpy.data.images.load(str(path))
        w, h = src.size
        k = TEXTURE_SIZE / max(w, h)
        if k < 1:
            src.scale(max(1, round(w * k)), max(1, round(h * k)))
        src.filepath_raw = str(small)
        src.file_format = "JPEG"
        src.save()
        bpy.data.images.remove(src)
    img = bpy.data.images.load(str(small), check_existing=True)
    img.colorspace_settings.name = "sRGB" if colour else "Non-Color"
    return img


def acg(name, kind):
    return ACG / name / f"{name}_{kind}_2k.jpg"


MATERIALS = {
    # key: colour, normal, roughness map, roughness, metallic, tint, emission map, emission strength
    'iron': dict(colour=acg('metal-024', 'color'), normal=acg('metal-024', 'normalgl'), rough=acg('metal-024', 'roughness'), metal=0.75, tint=(0.42, 0.42, 0.45)),
    'sea-iron': dict(colour=MT / 'wt-iron.jpg', normal=acg('metal-024', 'normalgl'), rough_v=0.78, metal=0.45, tint=(0.80, 0.76, 0.72)),
    'brass': dict(colour=MT / 'brass-detail-v01.jpg', normal=acg('metal-024', 'normalgl'), rough_v=0.38, metal=0.95, tint=(0.86, 0.63, 0.27), nstrength=0.5),
    'copper': dict(colour=MT / 'brass-detail-v01.jpg', normal=acg('metal-024', 'normalgl'), rough_v=0.45, metal=0.92, tint=(0.62, 0.33, 0.20), nstrength=0.6),
    'bronze': dict(colour=MT / 'brass-detail-v01.jpg', normal=acg('metal-024', 'normalgl'), rough_v=0.52, metal=0.9, tint=(0.36, 0.28, 0.15), nstrength=0.8),
    'oak': dict(colour=MT / 'wt-dark-oak.jpg', normal=acg('wood-060', 'normalgl'), rough_v=0.82, tint=(0.95, 0.90, 0.84)),
    'planks': dict(colour=acg('planks-037a', 'color'), normal=acg('planks-037a', 'normalgl'), rough=acg('planks-037a', 'roughness'), tint=(0.92, 0.86, 0.78)),
    'wood': dict(colour=acg('wood-060', 'color'), normal=acg('wood-060', 'normalgl'), rough=acg('wood-060', 'roughness'), tint=(0.78, 0.70, 0.60)),
    'cork': dict(colour=acg('wood-060', 'color'), normal=acg('wood-060', 'normalgl'), rough_v=0.95, tint=(1.0, 0.86, 0.66)),
    'rope': dict(colour=MT / 'winch-rope-hemp-color-v01.png', normal=MT / 'winch-rope-hemp-normal-v01.png', rough=MT / 'winch-rope-hemp-roughness-v01.png', tint=(0.62, 0.55, 0.44)),
    'net': dict(colour=MT / 'winch-rope-hemp-color-v01.png', normal=MT / 'winch-rope-hemp-normal-v01.png', rough_v=0.95, tint=(0.36, 0.33, 0.27)),
    'lens': dict(colour=MT / 'lamp-glass-v01.jpg', rough_v=0.12, emit=MT / 'lamp-glass-glow-v01.jpg', emit_strength=9.0),
    'ember': dict(colour=acg('lava-001', 'color'), rough_v=0.6, emit=acg('lava-001', 'color'), emit_strength=14.0),
    'glass': dict(colour=MT / 'lamp-glass-v01.jpg', rough_v=0.15),
    'green-glass': dict(colour=MT / 'dark-green-glass-photo-v01.png', rough_v=0.12, tint=(1.25, 1.3, 1.2)),
}
_made = {}


def material(key):
    if key in _made:
        return _made[key]
    d = MATERIALS[key]
    mat = bpy.data.materials.new(f"Lighthouse {key}")
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    shader = nodes.get("Principled BSDF")
    shader.inputs["Metallic"].default_value = d.get('metal', 0.0)
    shader.inputs["Roughness"].default_value = d.get('rough_v', 0.6)
    tex = nodes.new("ShaderNodeTexImage")
    tex.image = image(d['colour'], True)
    tint = d.get('tint')
    if tint:
        mix = nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.blend_type = "MULTIPLY"
        mix.inputs["Factor"].default_value = 1.0
        mix.inputs["B"].default_value = (*tint, 1.0)
        links.new(tex.outputs["Color"], mix.inputs["A"])
        links.new(mix.outputs["Result"], shader.inputs["Base Color"])
    else:
        links.new(tex.outputs["Color"], shader.inputs["Base Color"])
    if d.get('normal'):
        ntex = nodes.new("ShaderNodeTexImage")
        ntex.image = image(d['normal'], False)
        nmap = nodes.new("ShaderNodeNormalMap")
        nmap.inputs["Strength"].default_value = d.get('nstrength', 1.0)
        links.new(ntex.outputs["Color"], nmap.inputs["Color"])
        links.new(nmap.outputs["Normal"], shader.inputs["Normal"])
    if d.get('rough'):
        rtex = nodes.new("ShaderNodeTexImage")
        rtex.image = image(d['rough'], False)
        links.new(rtex.outputs["Color"], shader.inputs["Roughness"])
    if d.get('emit'):
        etex = nodes.new("ShaderNodeTexImage")
        etex.image = image(d['emit'], True)
        links.new(etex.outputs["Color"], shader.inputs["Emission Color"])
        shader.inputs["Emission Strength"].default_value = d['emit_strength']
    _made[key] = mat
    return mat


# --- the mesh builder --------------------------------------------------------------------------------------------------------
class Build:
    def __init__(self, name, seed=1):
        self.name = name
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.soft = self.bm.faces.layers.int.new("soft")  # thin tubes: smooth whatever the angle between their sides
        self.mats = []
        self.xf = I.copy()
        self.rng = random.Random(seed)

    def mi(self, key):
        if key not in self.mats:
            self.mats.append(key)
        return self.mats.index(key)

    def v(self, p, xf=None):
        m = self.xf @ xf if xf is not None else self.xf
        return self.bm.verts.new(m @ Vector(p))

    def face(self, verts, uvs, mi, smooth=True, soft=False):
        try:
            f = self.bm.faces.new(verts)
        except ValueError:
            return None
        f.material_index = mi
        f.smooth = smooth
        if soft:
            f[self.soft] = 1
        for loop, co in zip(f.loops, uvs):
            loop[self.uv].uv = co
        return f

    def lathe(self, prof, key, segs=24, xf=None, us=0.8, vs=0.8, smooth=True):
        """A profile [(r, z), ...] turned round local Z."""
        mi = self.mi(key)
        rmax = max(r for r, _ in prof)
        girth = 2 * pi * rmax * us
        girth = max(1.0, round(girth))  # a whole number of repeats: no seam
        rings, vacc = [], [0.0]
        for i, (r, z) in enumerate(prof):
            if i:
                vacc.append(vacc[-1] + math.hypot(r - prof[i - 1][0], z - prof[i - 1][1]) * vs)
            if r < 1e-5:
                rings.append([self.v((0, 0, z), xf)])
            else:
                rings.append([self.v((r * cos(2 * pi * s / segs), r * sin(2 * pi * s / segs), z), xf) for s in range(segs)])
        off = self.rng.random()
        for i in range(len(prof) - 1):
            a, b = rings[i], rings[i + 1]
            v0, v1 = vacc[i] + off, vacc[i + 1] + off
            if len(a) == 1 and len(b) == 1:
                continue
            for s in range(segs):
                n = (s + 1) % segs
                u0, u1 = s / segs * girth, (s + 1) / segs * girth
                if len(a) == 1:
                    self.face((a[0], b[s], b[n]), (((u0 + u1) / 2, v0), (u0, v1), (u1, v1)), mi, smooth)
                elif len(b) == 1:
                    self.face((a[s], a[n], b[0]), ((u0, v0), (u1, v0), ((u0 + u1) / 2, v1)), mi, smooth)
                else:
                    self.face((a[s], a[n], b[n], b[s]), ((u0, v0), (u1, v0), (u1, v1), (u0, v1)), mi, smooth)

    def sphere(self, r, key, xf=None, segs=16, rings=10, squash=1.0, us=0.8):
        prof = [(r * sin(pi * i / rings), -r * squash * cos(pi * i / rings)) for i in range(rings + 1)]
        prof[0], prof[-1] = (0.0, -r * squash), (0.0, r * squash)
        self.lathe(prof, key, segs, xf, us, us)

    def tube(self, pts, r, key, sides=8, closed=False, caps=True, us=1.0, vs=0.8, soft=None):
        """A round bar along the points; r one radius or one per point."""
        mi = self.mi(key)
        pts = [Vector(p) for p in pts]
        n = len(pts)
        radii = list(r) if isinstance(r, (list, tuple)) else [r] * n
        soft = sides < 8 if soft is None else soft
        tangents = []
        for i in range(n):
            a = pts[(i - 1) % n] if (closed or i > 0) else pts[i]
            b = pts[(i + 1) % n] if (closed or i < n - 1) else pts[i]
            t = b - a
            tangents.append(t.normalized() if t.length > 1e-9 else Vector((0, 0, 1)))
        normal = tangents[0].orthogonal().normalized()
        rings, vacc = [], [0.0]
        for i in range(n):
            if i:
                normal = tangents[i - 1].rotation_difference(tangents[i]) @ normal
                vacc.append(vacc[-1] + (pts[i] - pts[i - 1]).length * vs)
            normal = (normal - tangents[i] * normal.dot(tangents[i])).normalized()
            side = tangents[i].cross(normal)
            rings.append([self.v(pts[i] + (normal * cos(2 * pi * k / sides) + side * sin(2 * pi * k / sides)) * radii[i]) for k in range(sides)])
        if closed:
            vacc.append(vacc[-1] + (pts[0] - pts[-1]).length * vs)
        off = self.rng.random()
        for i in range(n if closed else n - 1):
            a, b = rings[i], rings[(i + 1) % n]
            v0, v1 = vacc[i] + off, vacc[i + 1] + off
            for k in range(sides):
                m = (k + 1) % sides
                self.face((a[k], a[m], b[m], b[k]), ((k / sides * us, v0), ((k + 1) / sides * us, v0), ((k + 1) / sides * us, v1), (k / sides * us, v1)), mi, True, soft)
        if caps and not closed:
            for ring in (list(reversed(rings[0])), rings[-1]):
                self.face(ring, [(0.5 + 0.3 * cos(2 * pi * k / sides), 0.5 + 0.3 * sin(2 * pi * k / sides)) for k in range(sides)], mi, False, soft)

    def box(self, size, key, xf=None, bevel=0.012, us=0.8):
        """A box centred on the frame; the photograph's U runs along each face's longer side (the grain)."""
        mi = self.mi(key)
        h = [s / 2 for s in size]
        corners = {}
        for sx in (-1, 1):
            for sy in (-1, 1):
                for sz in (-1, 1):
                    corners[(sx, sy, sz)] = self.v((sx * h[0], sy * h[1], sz * h[2]), xf)
        off = (self.rng.random(), self.rng.random())
        made = []
        for axis in range(3):
            a, b = [i for i in range(3) if i != axis]
            if size[b] > size[a]:
                a, b = b, a
            for sign in (-1, 1):
                quad, uvs = [], []
                for sa, sb in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                    key3 = [0, 0, 0]
                    key3[axis], key3[a], key3[b] = sign, sa, sb
                    quad.append(corners[tuple(key3)])
                    uvs.append((sa * h[a] * us + off[0], sb * h[b] * us + off[1] + axis * 0.31))
                f = self.face(quad, uvs, mi, False)
                if f:
                    made.append(f)
        if bevel and min(size) > bevel * 3:
            edges = list({e for f in made for e in f.edges})
            bmesh.ops.bevel(self.bm, geom=edges, offset=bevel, segments=1, profile=0.5, affect='EDGES', clamp_overlap=True)

    def beam(self, p0, p1, w, h, key, bevel=0.012, up=(0, 0, 1), us=0.8):
        """A squared timber / bar from p0 to p1 (w across, h along `up`)."""
        p0, p1 = Vector(p0), Vector(p1)
        self.box(((p1 - p0).length, w, h), key, along(p0, p1, up) @ T((p1 - p0).length / 2), bevel, us)

    def gear(self, r, teeth, thick, key, xf=None, hole=0.03, spokes=4, depth=None):
        """A toothed wheel in the frame's XY plane: rim with square teeth, spokes, hub."""
        mi = self.mi(key)
        depth = depth or max(0.02, r * 0.11)
        n = teeth * 4
        rin = r - depth * 2.4 if spokes else hole
        loops = {}
        for z in (-thick / 2, thick / 2):
            outer, inner = [], []
            for k in range(n):
                a = 2 * pi * (k + 0.5) / n
                rr = r if k % 4 in (0, 1) else r - depth
                outer.append(self.v((rr * cos(a), rr * sin(a), z), xf))
                inner.append(self.v((rin * cos(a), rin * sin(a), z), xf))
            loops[z] = (outer, inner)
        us = 1.2
        for z, (outer, inner) in loops.items():
            for k in range(n):
                m = (k + 1) % n
                vs = (outer[k], outer[m], inner[m], inner[k])
                self.face(vs, [(v.co.x * us, v.co.y * us + v.co.z * us) for v in vs], mi, False)
        lo, hi = loops[-thick / 2], loops[thick / 2]
        for which in (0, 1):
            for k in range(n):
                m = (k + 1) % n
                self.face((lo[which][k], lo[which][m], hi[which][m], hi[which][k]), ((k * 0.05, 0), (k * 0.05 + 0.05, 0), (k * 0.05 + 0.05, thick * us), (k * 0.05, thick * us)), mi, False)
        if spokes:
            hub = max(hole * 2.2, r * 0.2)
            self.lathe([(hole, -thick * 0.75), (hub, -thick * 0.75), (hub, thick * 0.75), (hole, thick * 0.75), (hole, -thick * 0.75)], key, 16, xf)
            for s in range(spokes):
                a = 360 * s / spokes
                m = (xf if xf is not None else I) @ Rz(a) @ T((hub + rin) / 2)
                self.box((rin - hub + depth * 0.4, max(0.025, r * 0.13), thick * 0.7), key, m, bevel=0)

    def chain(self, pts, key, link=0.12, wire=0.017):
        pts = [Vector(p) for p in pts]
        step, samples, carry = link * 0.70, [], 0.0
        for a, b in zip(pts, pts[1:]):
            seg = (b - a).length
            d = carry
            while d < seg:
                samples.append((a.lerp(b, d / seg), (b - a).normalized()))
                d += step
            carry = d - seg
        for i, (c, t) in enumerate(samples):
            side = t.cross(Vector((0, 0, 1)))
            side = side.normalized() if side.length > 0.05 else Vector((1, 0, 0))
            up = side.cross(t)
            n = side if i % 2 else up
            loop = [c + t * cos(2 * pi * k / 10) * link / 2 + n * sin(2 * pi * k / 10) * link * 0.30 for k in range(10)]
            self.tube(loop, wire, key, sides=6, closed=True, soft=True)

    def coil(self, r, turns, pitch, rope_r, key, xf=None, grow=0.0, sides=6, tail=None):
        """A helix round the frame's Z (grow: the radius gained per turn, a flat spiral with pitch 0)."""
        m = xf if xf is not None else I
        steps = int(turns * 18)
        pts = [m @ Vector(((r + grow * i / 18) * cos(2 * pi * i / 18), (r + grow * i / 18) * sin(2 * pi * i / 18), pitch * i / 18)) for i in range(steps + 1)]
        if tail:
            pts += [Vector(p) for p in tail]
        self.tube(pts, rope_r, key, sides=sides, soft=True, vs=3.0)

    def finish(self):
        bm = self.bm
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        sharp = [e for e in bm.edges if len(e.link_faces) == 2 and not any(f[self.soft] for f in e.link_faces)
                 and e.calc_face_angle(0.0) > math.radians(50)]
        bmesh.ops.split_edges(bm, edges=sharp)
        bmesh.ops.triangulate(bm, faces=bm.faces[:])
        zmin = min(v.co.z for v in bm.verts)
        cx = (min(v.co.x for v in bm.verts) + max(v.co.x for v in bm.verts)) / 2
        cy = (min(v.co.y for v in bm.verts) + max(v.co.y for v in bm.verts)) / 2
        for v in bm.verts:  # the base on the ground, the bounds centred (the engine centres a model on its bounds)
            v.co -= Vector((cx, cy, zmin))
        bm.normal_update()
        mesh = bpy.data.meshes.new(self.name)
        bm.to_mesh(mesh)
        bm.free()
        obj = bpy.data.objects.new(self.name, mesh)
        bpy.context.collection.objects.link(obj)
        for key in self.mats:
            mesh.materials.append(material(key))
        return obj


def arc(c, r, a0, a1, n, plane='xz'):
    out = []
    for i in range(n + 1):
        a = math.radians(a0 + (a1 - a0) * i / n)
        if plane == 'xz':
            out.append(Vector((c[0] + r * cos(a), c[1], c[2] + r * sin(a))))
        elif plane == 'yz':
            out.append(Vector((c[0], c[1] + r * cos(a), c[2] + r * sin(a))))
        else:
            out.append(Vector((c[0] + r * cos(a), c[1] + r * sin(a), c[2])))
    return out


def rivets(b, r, z, count, key, size=0.022, xf=None, phase=0.0):
    for i in range(count):
        a = 2 * pi * (i + phase) / count
        b.sphere(size, key, (xf if xf is not None else I) @ T(r * cos(a), r * sin(a), z), segs=6, rings=4)


# --- 1 · the lamp and its lens -----------------------------------------------------------------------------------------------
def lamp():
    b = Build("lighthouse-lamp", 11)
    # the cast-iron pedestal
    b.lathe([(0, 0), (0.82, 0), (0.82, 0.09), (0.74, 0.15), (0.50, 0.24), (0.36, 0.40), (0.30, 0.60), (0.29, 0.96), (0.35, 1.02),
             (0.35, 1.10), (0.56, 1.20), (0.78, 1.25), (0.80, 1.33), (0, 1.33)], 'iron', 32)
    rivets(b, 0.77, 0.12, 12, 'iron', 0.03)
    for i in range(4):  # the braces from the foot to the table
        a = math.radians(45 + 90 * i)
        b.tube([(0.62 * cos(a), 0.62 * sin(a), 0.16), (0.52 * cos(a), 0.52 * sin(a), 0.70), (0.70 * cos(a), 0.70 * sin(a), 1.24)], 0.03, 'iron')
    # the turning table: a brass gear ring and the deck over it
    b.gear(0.90, 60, 0.07, 'brass', T(0, 0, 1.37), hole=0.5, spokes=0, depth=0.035)
    b.lathe([(0, 1.405), (0.86, 1.405), (0.88, 1.44), (0.88, 1.50), (0.82, 1.54), (0, 1.54)], 'brass', 40)
    # the hand crank and its pinion on an iron bracket
    px = 0.90 + 0.115
    b.gear(0.13, 9, 0.06, 'iron', T(px, 0, 1.37), hole=0.02, spokes=0, depth=0.03)
    b.tube([(px, 0, 1.00), (px, 0, 1.70)], 0.022, 'iron')
    b.tube([(0.74, 0, 1.06), (px, 0, 1.06), (px, 0, 1.00)], 0.026, 'iron')
    b.tube([(px, 0, 1.70), (px + 0.20, 0, 1.70), (px + 0.20, 0, 1.84)], 0.018, 'iron')
    b.lathe([(0, 0), (0.03, 0), (0.034, 0.07), (0.03, 0.14), (0, 0.14)], 'oak', 10, T(px + 0.20, 0, 1.82))
    # the lens: stacked glass prisms, a barrel wider at the middle, the bullseye belt at the flame's height
    z0, z1, rings = 1.60, 3.52, 15
    def radius(t):
        return 0.60 + 0.17 * sin(pi * t) ** 0.8
    prof = [(radius(0) - 0.07, z0)]
    for i in range(rings):
        ta, tb = i / rings, (i + 1) / rings
        za, zb = z0 + (z1 - z0) * ta, z0 + (z1 - z0) * tb
        belt = abs((ta + tb) / 2 - 0.5) < 0.11
        out = 0.075 if belt else 0.045
        if (ta + tb) / 2 < 0.5:   # the prisms lean to throw the light level: lower ones face up, upper ones face down
            prof += [(radius(ta) - 0.02, za + 0.006), (radius(tb) + out, zb - 0.03), (radius(tb) - 0.02, zb - 0.006)]
        else:
            prof += [(radius(ta) - 0.02, za + 0.006), (radius(ta) + out, za + 0.03), (radius(tb) - 0.02, zb - 0.006)]
    prof.append((radius(1) - 0.07, z1))
    b.lathe(prof, 'lens', 48, us=0.55, vs=0.55)
    # the Ember inside, seen from above through the crown's vents and from below the lens
    b.lathe([(0, 1.54), (0.30, 1.54), (0.34, 1.62), (0.22, 1.72), (0, 1.74)], 'ember', 16)
    # the brass cage: rings below and above, the mid band, ten ribs following the barrel
    b.lathe([(0.50, 1.54), (0.70, 1.54), (0.70, 1.62), (0.50, 1.62), (0.50, 1.54)], 'brass', 40)
    b.lathe([(0.50, z1 - 0.02), (0.72, z1 - 0.02), (0.72, z1 + 0.06), (0.50, z1 + 0.06), (0.50, z1 - 0.02)], 'brass', 40)
    for i in range(10):
        a = 2 * pi * i / 10
        rib = []
        for k in range(15):
            t = k / 14
            rr = radius(t) + 0.085
            rib.append((rr * cos(a), rr * sin(a), z0 + (z1 - z0) * t))
        b.tube(rib, 0.024, 'brass')
    for t in (0.36, 0.64):
        rr = radius(t) + 0.085
        zz = z0 + (z1 - z0) * t
        b.lathe([(rr - 0.03, zz - 0.022), (rr + 0.03, zz - 0.022), (rr + 0.03, zz + 0.022), (rr - 0.03, zz + 0.022), (rr - 0.03, zz - 0.022)], 'brass', 40)
    # the crown: a vented brass dome, the ball and the spike
    b.lathe([(0.74, z1 + 0.06), (0.78, z1 + 0.10), (0.74, z1 + 0.16), (0.60, z1 + 0.34), (0.38, z1 + 0.52), (0.20, z1 + 0.60), (0.15, z1 + 0.66),
             (0.15, z1 + 0.74), (0.22, z1 + 0.77), (0.22, z1 + 0.80), (0.08, z1 + 0.84), (0.05, z1 + 0.90), (0.10, z1 + 0.96), (0.10, z1 + 1.00),
             (0.04, z1 + 1.06), (0.015, z1 + 1.30), (0, z1 + 1.32)], 'brass', 32)
    for i in range(8):  # the vents' hoods
        a = 360 * i / 8 + 22.5
        b.box((0.16, 0.10, 0.03), 'brass', Rz(a) @ T(0.52, 0, z1 + 0.43) @ Ry(40), bevel=0.006)
    rivets(b, 0.76, z1 + 0.10, 20, 'brass', 0.02)
    return b.finish(), dict(name='Lanterna do Farol 3D', footprint='one', blocks=True,
                            light={'color': [1.0, 0.56, 0.22], 'intensity': 18, 'radius': 9.0, 'flicker': 0.35, 'minHeight': 2.5})


# --- 2 · the clockwork that turns it ------------------------------------------------------------------------------------------
def clockwork():
    b = Build("lighthouse-clockwork", 22)
    W, D, H = 0.62, 0.42, 2.25
    for sx in (-1, 1):  # two oak side frames
        for sy in (-1, 1):
            b.beam((sx * W, sy * D, 0.10), (sx * W, sy * D, H), 0.13, 0.13, 'oak', up=(1, 0, 0))
        b.beam((sx * W, -D - 0.22, 0.07), (sx * W, D + 0.22, 0.07), 0.16, 0.14, 'oak')
        b.beam((sx * W, -D - 0.08, H + 0.06), (sx * W, D + 0.08, H + 0.06), 0.15, 0.13, 'oak')
        b.beam((sx * W, -D, 1.22), (sx * W, D, 1.22), 0.11, 0.16, 'oak')
        b.beam((sx * W, -D, 1.86), (sx * W, D, 1.86), 0.11, 0.12, 'oak')
        for sy in (-1, 1):  # braces
            b.beam((sx * W, sy * (D + 0.18), 0.14), (sx * W, sy * D, 0.62), 0.07, 0.07, 'oak')
    for sy in (-1, 1):
        b.beam((-W, sy * D, 0.30), (W, sy * D, 0.30), 0.10, 0.10, 'oak')
    b.beam((-W - 0.06, 0, H + 0.06), (W + 0.06, 0, H + 0.06), 0.12, 0.12, 'oak')
    # the drum on its iron axle, the rope wound on it, the weight below
    ax = Ry(90)
    b.tube([(-W - 0.12, 0, 1.22), (W + 0.30, 0, 1.22)], 0.028, 'iron')
    b.lathe([(0.03, -0.36), (0.30, -0.36), (0.30, -0.33), (0.19, -0.33), (0.18, 0.20), (0.30, 0.20), (0.30, 0.23), (0.03, 0.23)], 'wood', 24, T(0, 0, 1.22) @ ax)
    b.coil(0.205, 7, 0.036, 0.017, 'rope', T(-0.32, 0, 1.22) @ ax, tail=[(-0.06, 0.205, 1.20), (-0.06, 0.21, 0.92)])
    b.tube([(-0.06, 0.21, 0.92), (-0.06, 0.21, 0.80)], 0.017, 'rope', sides=6)
    b.tube(arc((-0.06, 0.21, 0.755), 0.045, 0, 360, 12, 'xz')[:-1], 0.012, 'iron', sides=6, closed=True)
    b.lathe([(0, 0.30), (0.10, 0.30), (0.13, 0.34), (0.13, 0.66), (0.09, 0.71), (0, 0.71)], 'iron', 20, T(-0.06, 0.21, 0))
    # the great wheel on the drum, the pinion and the crank
    b.gear(0.44, 40, 0.05, 'brass', T(0.36, 0, 1.22) @ ax, hole=0.03, spokes=6)
    b.tube([(-W - 0.10, 0.02, 1.86), (W + 0.10, 0.02, 1.86)], 0.022, 'iron')
    b.gear(0.20, 18, 0.05, 'iron', T(0.36, 0.02, 1.86) @ ax, hole=0.022, spokes=4)
    b.gear(0.30, 28, 0.04, 'brass', T(0.02, 0.02, 1.86) @ ax, hole=0.022, spokes=5)
    b.gear(0.24, 15, 0.035, 'brass', T(-0.30, 0.02, 1.86) @ ax, hole=0.022, spokes=4, depth=0.05)  # the escape wheel
    b.tube([(W + 0.30, 0, 1.22), (W + 0.30, 0, 0.92), (W + 0.50, 0, 0.92)], 0.022, 'iron')
    b.lathe([(0, 0), (0.035, 0), (0.04, 0.10), (0.035, 0.20), (0, 0.20)], 'oak', 10, T(W + 0.40, 0, 0.92) @ ax)
    # the pendulum from the top beam: the rod, the brass bob, the anchor over the escape wheel
    b.tube([(-0.30, 0.02, H), (-0.30, 0.02, 2.12)], 0.02, 'iron')
    b.tube([(-0.30, -0.14, 2.06), (-0.30, 0.02, 2.14), (-0.30, 0.18, 2.06)], 0.018, 'iron')
    b.tube([(-0.30, -0.30, 2.14), (-0.30, -0.30, 0.52)], 0.012, 'iron', sides=6)
    b.tube([(-0.30, 0.02, 2.14), (-0.30, -0.30, 2.14)], 0.012, 'iron', sides=6)
    b.lathe([(0, -0.04), (0.10, -0.03), (0.15, 0), (0.10, 0.03), (0, 0.04)], 'brass', 24, T(-0.30, -0.30, 0.52) @ Ry(90))
    for sx in (-1, 1):  # iron straps
        b.box((0.02, 2 * D + 0.20, 0.07), 'iron', T(sx * (W + 0.07), 0, 1.22), bevel=0.004)
        rivets(b, 0, 0, 1, 'iron', 0.02, T(sx * (W + 0.085), D, 1.22))
        rivets(b, 0, 0, 1, 'iron', 0.02, T(sx * (W + 0.085), -D, 1.22))
    return b.finish(), dict(name='Mecanismo do Farol 3D', footprint='one', blocks=True)


# --- 3 · the oil tank ---------------------------------------------------------------------------------------------------------
def oil_tank():
    b = Build("lighthouse-oil-tank", 33)
    R = 0.52
    b.lathe([(0, 0.44), (0.10, 0.44), (0.30, 0.50), (0.46, 0.60), (R, 0.72), (R, 1.66), (0.48, 1.76), (0.36, 1.86), (0.18, 1.93), (0.10, 1.95),
             (0.10, 2.00), (0, 2.00)], 'copper', 32)
    for z in (0.76, 1.19, 1.62):  # riveted bands
        b.lathe([(R - 0.01, z - 0.035), (R + 0.014, z - 0.035), (R + 0.014, z + 0.035), (R - 0.01, z + 0.035)], 'brass', 32)
        rivets(b, R + 0.014, z, 18, 'brass', 0.018)
    # the iron cradle: a ring and four splayed legs with feet
    b.lathe([(R - 0.02, 0.66), (R + 0.045, 0.66), (R + 0.045, 0.73), (R - 0.02, 0.73)], 'iron', 32)
    for i in range(4):
        a = math.radians(45 + 90 * i)
        c, s = cos(a), sin(a)
        b.beam((0.53 * c, 0.53 * s, 0.70), (0.70 * c, 0.70 * s, 0.03), 0.07, 0.05, 'iron', bevel=0.006, up=(-s, c, 0))
        b.lathe([(0, 0), (0.07, 0), (0.07, 0.03), (0, 0.03)], 'iron', 10, T(0.70 * c, 0.70 * s, 0))
    for i in range(4):
        a0, a1 = math.radians(45 + 90 * i), math.radians(135 + 90 * i)
        b.tube([(0.62 * cos(a0), 0.62 * sin(a0), 0.36), (0.62 * cos(a1), 0.62 * sin(a1), 0.36)], 0.016, 'iron', sides=6)
    # the lid with its handle, the feed pipe rising and turning away to the lamp
    b.lathe([(0, 2.00), (0.14, 2.00), (0.14, 2.04), (0, 2.05)], 'brass', 16)
    b.tube(arc((0, 0, 2.04), 0.07, 0, 180, 8, 'xz'), 0.012, 'brass', sides=6)
    b.tube([(0.30, 0, 1.84), (0.30, 0, 2.18), (0.36, 0, 2.30), (0.50, 0, 2.36), (0.74, 0, 2.36)], 0.035, 'copper')
    b.lathe([(0.035, 0), (0.055, 0), (0.055, 0.05), (0.035, 0.05)], 'brass', 12, T(0.30, 0, 1.86))
    b.lathe([(0.035, 0), (0.06, 0), (0.06, 0.04), (0.035, 0.04)], 'brass', 12, T(0.70, 0, 2.36) @ Ry(90))
    # the tap low on the front and its wheel
    b.tube([(0, -0.40, 0.58), (0, -0.66, 0.58), (0, -0.72, 0.54), (0, -0.72, 0.44)], 0.032, 'brass')
    b.tube([(0, -0.60, 0.58), (0, -0.60, 0.74)], 0.014, 'brass', sides=6)
    b.tube(arc((0, -0.60, 0.74), 0.07, 0, 360, 14, 'xy')[:-1], 0.012, 'iron', sides=6, closed=True)
    for a in (0, 90):
        b.tube([(0.07 * cos(math.radians(a)), -0.60 + 0.07 * sin(math.radians(a)), 0.74), (-0.07 * cos(math.radians(a)), -0.60 - 0.07 * sin(math.radians(a)), 0.74)], 0.008, 'iron', sides=6)
    # the sight glass on the side
    b.tube([(-0.40, -0.40, 0.86), (-0.40, -0.40, 1.52)], 0.022, 'glass')
    for z in (0.86, 1.52):
        b.tube([(-0.36, -0.36, z), (-0.42, -0.42, z)], 0.03, 'brass')
    # the oil can and the funnel on the floor
    b.lathe([(0, 0), (0.11, 0), (0.12, 0.02), (0.12, 0.20), (0.05, 0.30), (0.03, 0.31), (0.03, 0.34), (0, 0.34)], 'copper', 16, T(0.30, -0.72, 0))
    b.tube([(0.36, -0.72, 0.22), (0.50, -0.80, 0.36), (0.60, -0.86, 0.36)], [0.022, 0.014, 0.009], 'copper', sides=6)
    b.tube(arc((0.20, -0.72, 0.17), 0.07, 90, 270, 8, 'xz'), 0.01, 'brass', sides=6)
    return b.finish(), dict(name='Tanque de óleo do Farol 3D', footprint='one', blocks=True)


# --- 4 · the fog bell ---------------------------------------------------------------------------------------------------------
def fog_bell():
    b = Build("lighthouse-fog-bell", 44)
    W, H = 0.66, 2.62
    for sx in (-1, 1):
        b.beam((sx * W, 0, 0.14), (sx * W, 0, H), 0.17, 0.17, 'oak', up=(1, 0, 0))
        b.beam((sx * W, -0.62, 0.08), (sx * W, 0.62, 0.08), 0.19, 0.16, 'oak')
        for sy in (-1, 1):
            b.beam((sx * W, sy * 0.52, 0.16), (sx * W, sy * 0.04, 0.98), 0.10, 0.10, 'oak')
        b.beam((sx * W, 0, 2.04), (sx * 0.20, 0, H - 0.02), 0.10, 0.10, 'oak')  # the knee under the top beam
        b.box((0.03, 0.21, 0.08), 'iron', T(sx * (W + 0.09), 0, H - 0.10), bevel=0.004)
    b.beam((-W - 0.24, 0, H + 0.085), (W + 0.24, 0, H + 0.085), 0.20, 0.18, 'oak')
    b.beam((-W, 0, 0.30), (W, 0, 0.30), 0.10, 0.12, 'oak')
    for sx in (-1, 1):
        b.box((0.06, 0.24, 0.22), 'iron', T(sx * 0.30, 0, H + 0.085), bevel=0.004)
    # the yoke: iron straps from the beam to the bell's crown
    top = 2.36
    for sx in (-1, 1):
        b.tube([(sx * 0.30, 0, H - 0.02), (sx * 0.30, 0, top + 0.08), (sx * 0.10, 0, top - 0.02)], 0.022, 'iron')
    b.tube(arc((0, 0, top + 0.01), 0.075, 0, 360, 12, 'xz')[:-1], 0.02, 'bronze', sides=6, closed=True)
    # the bell, with its wall thickness, cast bands at the shoulder and the lip
    outer = [(0.07, top), (0.17, top - 0.02), (0.25, top - 0.09), (0.29, top - 0.24), (0.31, top - 0.50), (0.35, top - 0.74), (0.43, top - 0.93),
             (0.52, top - 1.03), (0.54, top - 1.07)]
    inner = [(0.49, top - 1.07), (0.44, top - 0.99), (0.36, top - 0.86), (0.28, top - 0.70), (0.25, top - 0.48), (0.22, top - 0.24), (0.15, top - 0.12), (0, top - 0.09)]
    b.lathe([(0, top)] + outer + inner, 'bronze', 36)
    for z, rr in ((top - 0.20, 0.288), (top - 0.26, 0.296), (top - 0.88, 0.405), (top - 0.99, 0.495)):
        b.lathe([(rr - 0.004, z - 0.012), (rr + 0.012, z - 0.006), (rr + 0.012, z + 0.006), (rr - 0.004, z + 0.012)], 'bronze', 36)
    # the clapper, swung a little, and its pull rope
    c0, c1 = Vector((0, 0, top - 0.10)), Vector((0.10, 0.03, top - 0.98))
    b.tube([c0, c1], 0.018, 'iron')
    b.sphere(0.075, 'iron', T(*c1), segs=12, rings=8)
    tail = c1 + Vector((0.01, 0, -0.10))
    b.tube([c1, tail, (0.16, 0.05, 1.10), (0.22, 0.10, 0.86)], 0.016, 'rope', sides=6)
    b.sphere(0.035, 'rope', T(0.22, 0.10, 0.84), segs=8, rings=6)
    b.tube([(0.22, 0.10, 0.84), (0.23, 0.11, 0.70)], [0.02, 0.03], 'rope', sides=6)
    return b.finish(), dict(name='Sino de neblina 3D', footprint='one', blocks=True)


# --- 5 · the rowboat ----------------------------------------------------------------------------------------------------------
def rowboat():
    b = Build("lighthouse-rowboat", 55)
    L, B = 3.30, 0.62
    ns, strakes = 25, 6
    def half(t):
        return B * max(0.0, 1 - t * t) ** 0.72
    def sheer(t):
        return 0.70 + 0.17 * t * t
    def keel(t):
        return 0.10 + 0.12 * abs(t) ** 3
    def point(t, s, inner=False):
        bw, zk, zs = half(t), keel(t), sheer(t)
        y = bw * s ** 0.52
        z = zk + (zs - zk) * s ** 1.7
        if inner:
            y = max(0.0, y - 0.034 * (0.3 + 0.7 * s))
            z += 0.034 * (1 - s) ** 0.6
        return Vector((t * L / 2, y, z))
    ts = [0.996 * -cos(pi * i / (ns - 1)) for i in range(ns)]
    mi = b.mi('planks')
    for sign in (-1, 1):
        for j in range(strakes):  # clinker: each strake its own strip, its lower edge standing proud of the one below
            s0, s1 = j / strakes, (j + 1) / strakes
            for inner in (False, True):
                rows = []
                for t in ts:
                    p0, p1 = point(t, s0, inner), point(t, s1, inner)
                    if not inner and j:
                        lap = 0.016 * min(1.0, half(t) / 0.2)
                        p0 = p0 + Vector((0, lap, -0.012))
                    p0.y *= sign
                    p1.y *= sign
                    rows.append((b.v(p0), b.v(p1)))
                for i in range(ns - 1):
                    (a0, a1), (b0, b1) = rows[i], rows[i + 1]
                    u0, u1 = (ts[i] + 1) * L * 0.25, (ts[i + 1] + 1) * L * 0.25
                    v0, v1 = (j + (0.5 if inner else 0)) * 0.167, (j + 1 + (0.5 if inner else 0)) * 0.167
                    b.face((a0, b0, b1, a1), ((u0, v0), (u1, v0), (u1, v1), (u0, v1)), mi, True)
        # the gunwale: the cap over the planking and the rubbing strake outside
        cap_o = [point(t, 1.0) for t in ts]
        rail = [Vector((p.x, sign * (p.y + 0.006), p.z + 0.012)) for p in cap_o]
        b.tube(rail, 0.036, 'oak', sides=8, vs=0.6)
    # keel, stem and stern posts
    b.tube([(t * L / 2, 0, keel(t) - 0.028) for t in ts], 0.042, 'oak', sides=8)
    for sx in (-1, 1):
        xe = sx * L / 2 * 0.996
        b.tube([(xe - sx * 0.10, 0, keel(1) - 0.05), (xe, 0, keel(1) + 0.02), (xe + sx * 0.02, 0, sheer(1) * 0.6), (xe + sx * 0.03, 0, sheer(1) + 0.11)],
               [0.045, 0.05, 0.05, 0.04], 'oak', sides=8)
    # ribs inside
    for t in (-0.78, -0.56, -0.34, -0.12, 0.12, 0.34, 0.56, 0.78):
        rib = [point(t, s, True) for s in (1.0, 0.8, 0.6, 0.42, 0.26, 0.12, 0.03)]
        pts = [Vector((p.x, p.y - 0.012, p.z + 0.008)) for p in rib]
        pts = pts + [Vector((p.x, -p.y, p.z)) for p in reversed(pts)]
        b.tube(pts, 0.022, 'oak', sides=6, soft=True)
    # floor boards, three thwarts on their risers, the oars laid across
    for y in (-0.16, 0, 0.16):
        b.box((1.9, 0.14, 0.022), 'planks', T(0, y, 0.20), bevel=0.004)
    for t in (-0.50, 0.0, 0.50):
        w = half(t) * 0.86
        b.box((0.22, 2 * w, 0.035), 'oak', T(t * L / 2, 0, 0.50), bevel=0.006)
        for sy in (-1, 1):
            b.box((0.06, 0.05, 0.28), 'oak', T(t * L / 2, sy * w * 0.72, 0.36), bevel=0.004)
    for sy, zz in ((-1, 0.535), (1, 0.538)):
        p0, p1 = Vector((-1.18, sy * 0.20, zz + 0.03)), Vector((1.02, sy * 0.13, zz + 0.03))
        b.tube([p0, p1], [0.03, 0.024], 'wood', sides=8)
        b.box((0.56, 0.14, 0.022), 'wood', along(p1, p1 + (p1 - p0)) @ T(0.26), bevel=0.006)
        b.lathe([(0.03, 0), (0.04, 0), (0.04, 0.10), (0.03, 0.10)], 'rope', 10, along(p0, p1) @ T(0.62) @ Ry(90), us=3, vs=3)
    # oarlocks, the bow ring and the painter coiled in the bow
    for sy in (-1, 1):
        for t in (-0.14, 0.36):
            p = point(t, 1.0)
            b.box((0.12, 0.05, 0.03), 'oak', T(p.x, sy * (p.y - 0.01), p.z + 0.05), bevel=0.004)
            for k in (-1, 1):
                b.tube([(p.x + k * 0.035, sy * (p.y - 0.01), p.z + 0.05), (p.x + k * 0.04, sy * (p.y - 0.01), p.z + 0.15)], 0.011, 'iron', sides=6)
    xb = L / 2 * 0.996
    b.tube(arc((xb + 0.05, 0, sheer(1) - 0.06), 0.05, 0, 360, 12, 'xz')[:-1], 0.011, 'iron', sides=6, closed=True)
    b.coil(0.10, 4.5, 0.004, 0.02, 'rope', T(1.12, 0, 0.30), grow=0.026,
           tail=[(1.36, 0.02, 0.42), (1.52, 0.0, 0.66), (xb + 0.03, 0, sheer(1) - 0.10)])
    # the chocks it sits on
    for x in (-0.95, 0.95):
        b.box((0.20, 0.95, 0.08), 'oak', T(x, 0, 0.04))
        for sy in (-1, 1):
            b.beam((x, sy * 0.42, 0.08), (x, sy * 0.20, 0.20), 0.18, 0.07, 'oak', up=(1, 0, 0))
    return b.finish(), dict(name='Bote a remo 3D', footprint='pair', blocks=True)


# --- 6 · the anchor -----------------------------------------------------------------------------------------------------------
def anchor():
    b = Build("lighthouse-anchor", 66)
    lay = T(0, 0, 0) @ Rx(-14) @ Ry(64)  # laid down: one fluke in the ground, the stock's end on it
    b.xf = lay
    Ls = 1.95
    b.tube([(0, 0, 0.06), (0, 0, Ls * 0.5), (0, 0, Ls)], [0.075, 0.062, 0.05], 'sea-iron', sides=10)
    b.lathe([(0.05, 0), (0.085, 0), (0.085, 0.07), (0.05, 0.07)], 'sea-iron', 12, T(0, 0, Ls - 0.30))
    b.tube(arc((0, 0, Ls + 0.11), 0.12, 0, 360, 16, 'yz')[:-1], 0.026, 'sea-iron', sides=8, closed=True)
    # the wooden stock, two halves banded with iron, across the arms' plane
    b.tube([(0, -0.95, Ls - 0.30), (0, -0.5, Ls - 0.30), (0, 0, Ls - 0.30), (0, 0.5, Ls - 0.30), (0, 0.95, Ls - 0.30)], [0.055, 0.085, 0.10, 0.085, 0.055], 'oak', sides=8)
    for y in (-0.72, -0.34, 0.34, 0.72):
        rr = 0.10 - 0.05 * abs(y)
        b.lathe([(rr, -0.03), (rr + 0.014, -0.03), (rr + 0.014, 0.03), (rr, 0.03)], 'sea-iron', 12, T(0, y, Ls - 0.30) @ Rx(90))
    # the crown and the two arms, each ending in a fluke with its palm
    b.sphere(0.11, 'sea-iron', T(0, 0, 0.05), segs=12, rings=8, squash=0.8)
    for sx in (-1, 1):
        pts = [Vector((sx * 0.78 * sin(math.radians(a)), 0, 0.78 - 0.78 * cos(math.radians(a)) * 1.0 + 0.02)) for a in (0, 14, 28, 42, 56, 68)]
        b.tube(pts, [0.075, 0.07, 0.064, 0.056, 0.048, 0.03], 'sea-iron', sides=10)
        tip, prev = pts[-1], pts[-3]
        fr = along(prev, tip, up=(0, 1, 0))
        mi = b.mi('sea-iron')
        t = 0.022
        shape = [(0.0, 0.0), (0.16, 0.17), (0.50, 0.0), (0.16, -0.17)]  # the palm: a spade
        lo = [b.v((x, y, -t), fr) for x, y in shape]
        hi = [b.v((x, y, t), fr) for x, y in shape]
        b.face(lo, [(x, y) for x, y in shape], mi, False)
        b.face(hi, [(x, y + 0.5) for x, y in shape], mi, False)
        for k in range(4):
            m = (k + 1) % 4
            b.face((lo[k], lo[m], hi[m], hi[k]), ((k * 0.2, 0), (k * 0.2 + 0.2, 0), (k * 0.2 + 0.2, 0.05), (k * 0.2, 0.05)), mi, False)
    b.xf = I.copy()
    # the chain: from the ring across the ground to a heap
    ring = lay @ Vector((0, 0, Ls + 0.22))
    path = [ring, (ring.x + 0.10, ring.y + 0.10, 0.03)]
    for i in range(1, 16):
        t = i / 15
        path.append((ring.x + 0.10 - 1.5 * t, ring.y + 0.10 + 0.34 * sin(t * 2.6 * pi) * (1 - 0.3 * t) + 0.5 * t, 0.03))
    b.chain(path, 'sea-iron')
    end = Vector(path[-1])
    heap = [end + Vector(((0.10 + 0.012 * i) * cos(i * 0.8), (0.10 + 0.012 * i) * sin(i * 0.8), 0.035 if i < 9 else 0.075)) for i in range(1, 17)]
    b.chain([end] + heap, 'sea-iron')
    return b.finish(), dict(name='Âncora 3D', footprint='one', blocks=True)


# --- 7 · the mooring post -----------------------------------------------------------------------------------------------------
def mooring_post():
    b = Build("lighthouse-mooring-post", 77)
    b.lathe([(0, 0), (0.27, 0), (0.265, 0.25), (0.25, 0.70), (0.24, 1.06), (0.25, 1.10), (0.25, 1.22), (0.22, 1.30), (0.14, 1.36), (0, 1.38)], 'oak', 20, us=1.0)
    for z in (0.14, 1.16):
        b.lathe([(0.24, z - 0.04), (0.284, z - 0.04), (0.284, z + 0.04), (0.24, z + 0.04)], 'sea-iron', 20)
        rivets(b, 0.284, z, 8, 'sea-iron', 0.016)
    b.tube([(-0.44, 0, 0.98), (0.44, 0, 0.98)], 0.034, 'sea-iron')  # the cross pin
    for sx in (-1, 1):
        b.sphere(0.05, 'sea-iron', T(sx * 0.44, 0, 0.98), segs=10, rings=6)
    # the hawser: turns round the post under the pin, a tail down to a coil on the ground
    b.coil(0.275, 4.2, 0.058, 0.03, 'rope', T(0, 0, 0.60), sides=8,
           tail=[(0.36, 0.20, 0.74), (0.52, 0.34, 0.40), (0.62, 0.42, 0.10), (0.66, 0.30, 0.045)])
    b.coil(0.14, 3.6, 0.0, 0.03, 'rope', T(0.60, 0.02, 0.035), grow=0.062, sides=8)
    b.coil(0.17, 2.6, 0.0, 0.03, 'rope', T(0.60, 0.02, 0.095) @ Rz(40), grow=0.062, sides=8)
    # an iron ring stapled to the post and a second, shorter bollard beside
    b.tube(arc((0, -0.31, 0.44), 0.07, 0, 360, 12, 'xz')[:-1], 0.014, 'sea-iron', sides=6, closed=True)
    b.tube([(-0.03, -0.24, 0.52), (0, -0.30, 0.52), (0.03, -0.24, 0.52)], 0.012, 'sea-iron', sides=6)
    b.lathe([(0, 0), (0.15, 0), (0.145, 0.50), (0.15, 0.56), (0.12, 0.62), (0, 0.64)], 'oak', 14, T(-0.50, 0.34, 0), us=1.0)
    b.lathe([(0.14, 0.44), (0.168, 0.44), (0.168, 0.50), (0.14, 0.50)], 'sea-iron', 14, T(-0.50, 0.34, 0))
    return b.finish(), dict(name='Poste de amarração 3D', footprint='one', blocks=True)


# --- 8 · the net drying rack --------------------------------------------------------------------------------------------------
def net_rack():
    b = Build("lighthouse-net-rack", 88)
    X, PZ = 1.36, 1.92
    for sx in (-1, 1):  # two crossed-pole trestles and the pole laid in their forks
        b.tube([(sx * X, 0.62, 0), (sx * X, 0.18, 1.2), (sx * X, -0.13, 2.16)], [0.055, 0.05, 0.04], 'wood', sides=8)
        b.tube([(sx * (X - 0.09), -0.62, 0), (sx * (X - 0.09), -0.18, 1.2), (sx * (X - 0.09), 0.13, 2.16)], [0.055, 0.05, 0.04], 'wood', sides=8)
        b.coil(0.075, 5, 0.022, 0.012, 'rope', T(sx * (X - 0.045) - 0.055, 0, PZ - 0.06) @ Ry(90))
    b.tube([(-X - 0.22, 0.0, PZ), (0, 0.01, PZ + 0.015), (X + 0.22, 0.0, PZ)], [0.05, 0.047, 0.043], 'wood', sides=8)

    def surf(u, v):
        x = -1.18 + 2.36 * u
        if v < 0.6:
            w = v / 0.6
            y = 0.86 * (1 - w) ** 1.25 + 0.05 + 0.03 * sin(u * 11) * (1 - w)
            z = 0.02 + (PZ + 0.035) * w ** 0.82 - 0.05 * sin(pi * w) * (0.5 + 0.5 * sin(u * 7))
        else:
            w = (v - 0.6) / 0.4
            y = 0.05 - 0.62 * w ** 0.85 - 0.02 * sin(u * 9) * w
            z = PZ + 0.055 - 1.45 * w ** 1.35
        if v < 0.06:
            z = max(z, 0.012)
        return Vector((x, y, z))

    # the net: two families of twine crossing in diamonds, selvedge ropes round the edge
    N, c, steps = 26, 0.42, 30
    for fam in (1, -1):
        for k in range(-int(N * c) - 1, N + int(N * c) + 2):
            run = []
            for i in range(steps + 1):
                v = i / steps
                u = k / N + fam * c * (v - (0 if fam > 0 else 0))
                if 0.0 <= u <= 1.0:
                    run.append(surf(u, v))
                else:
                    if len(run) > 1:
                        b.tube(run, 0.0065, 'net', sides=4, caps=False, vs=4)
                    run = []
            if len(run) > 1:
                b.tube(run, 0.0065, 'net', sides=4, caps=False, vs=4)
    for u in (0.0, 1.0):
        b.tube([surf(u, i / steps) for i in range(steps + 1)], 0.012, 'rope', sides=6, vs=3)
    for v in (0.0, 1.0):
        b.tube([surf(i / 24, v) for i in range(25)], 0.012, 'rope', sides=6, vs=3)
    # cork floats along the foot rope, lead weights along the back edge
    for i in range(12):
        p = surf((i + 0.5) / 12, 0.0)
        b.sphere(0.055, 'cork', T(p.x, p.y, p.z + 0.03) @ Ry(90), segs=10, rings=6, squash=1.5)
    for i in range(9):
        p = surf((i + 0.5) / 9, 1.0)
        b.lathe([(0.008, -0.04), (0.024, -0.03), (0.024, 0.03), (0.008, 0.04)], 'sea-iron', 8, T(*p) @ Ry(90))
    # three glass floats in their rope nets on the ground
    for x, y, r in ((-0.62, 1.10, 0.17), (-0.28, 1.22, 0.14), (0.86, 1.06, 0.15)):
        b.sphere(r, 'green-glass', T(x, y, r), segs=20, rings=12)
        for a in (0, 60, 120):
            ring = [Vector((x, y, r)) + Rz(a) @ Vector(((r + 0.008) * cos(2 * pi * i / 20), 0, (r + 0.008) * sin(2 * pi * i / 20))) for i in range(20)]
            b.tube(ring, 0.008, 'rope', sides=4, closed=True, vs=4)
        b.tube([Vector((x + (r + 0.008) * 0.82 * cos(2 * pi * i / 16), y + (r + 0.008) * 0.82 * sin(2 * pi * i / 16), r + r * 0.57)) for i in range(16)], 0.008, 'rope', sides=4, closed=True, vs=4)
    return b.finish(), dict(name='Varal de redes 3D', footprint='pair', blocks=True)


# --- 9 · the lobster pots -----------------------------------------------------------------------------------------------------
def lobster_pots():
    b = Build("lighthouse-lobster-pots", 99)

    def pot(xf):
        b.xf = xf
        L, R = 0.96, 0.30
        for sy in (-1, 1):  # the base frame and its floor laths
            b.box((L, 0.05, 0.045), 'wood', T(0, sy * R, 0.0225), bevel=0.005)
        for x in (-L / 2 + 0.025, 0, L / 2 - 0.025):
            b.box((0.05, 2 * R, 0.035), 'wood', T(x, 0, 0.02), bevel=0.005)
        for y in (-0.20, -0.10, 0, 0.10, 0.20):
            b.box((L - 0.04, 0.05, 0.012), 'planks', T(0, y, 0.046), bevel=0)
        for x in (-L / 2 + 0.025, 0, L / 2 - 0.025):  # the bent hoops
            b.tube(arc((x, 0, 0.03), R, 0, 180, 14, 'yz'), 0.019, 'wood', sides=6, soft=True)
        for a in range(12, 180, 17):  # the laths over the hoops
            ar = math.radians(a)
            b.box((L, 0.04, 0.011), 'planks', T(0, (R + 0.022) * cos(ar), 0.03 + (R + 0.022) * sin(ar)) @ Rx(a - 90), bevel=0)
        for sx in (-1, 1):  # the netted ends, drawn in to the entrance ring
            xe = sx * (L / 2 - 0.025)
            centre = Vector((xe - sx * 0.17, 0, 0.15))
            ring = [centre + Vector((0, 0.075 * cos(2 * pi * i / 12), 0.075 * sin(2 * pi * i / 12))) for i in range(12)]
            b.tube(ring, 0.011, 'rope', sides=5, closed=True, vs=4)
            rim = arc((xe, 0, 0.03), R - 0.01, 0, 180, 12, 'yz') + [Vector((xe, -R + 0.03, 0.04)), Vector((xe, 0, 0.04)), Vector((xe, R - 0.03, 0.04))]
            for i, p in enumerate(rim):
                q = centre + (p - Vector((xe, 0, 0.15))).normalized() * 0.075
                q.x = centre.x
                b.tube([p, p.lerp(q, 0.5) + Vector((0, 0, -0.006)), q], 0.0055, 'net', sides=4, caps=False, vs=4)
            for f in (0.35, 0.68):  # the cross twine
                loop = []
                for p in arc((xe, 0, 0.03), R - 0.01, 0, 360, 20, 'yz')[:-1]:
                    p.z = max(p.z, 0.04)
                    q = centre + (p - Vector((xe, 0, 0.15))).normalized() * 0.075
                    q.x = centre.x
                    loop.append(p.lerp(q, f))
                b.tube(loop, 0.0055, 'net', sides=4, closed=True, vs=4)
        b.xf = I.copy()

    pot(T(0, -0.34, 0))
    pot(T(0.04, 0.36, 0) @ Rz(-9))
    pot(T(-0.03, 0.02, 0.37) @ Rz(7) @ Rx(3))
    # the buoy line coiled on the ground with its cork buoy
    b.coil(0.12, 3.4, 0.0, 0.017, 'rope', T(0.80, -0.05, 0.02), grow=0.04,
           tail=[(1.02, 0.2, 0.03), (0.92, 0.48, 0.05), (0.72, 0.60, 0.09)])
    b.lathe([(0, -0.15), (0.05, -0.13), (0.085, -0.04), (0.085, 0.05), (0.04, 0.14), (0.02, 0.16), (0, 0.16)], 'cork', 12, T(0.66, 0.66, 0.09) @ Ry(78) @ Rz(30))
    b.lathe([(0.086, -0.01), (0.09, -0.01), (0.09, 0.02), (0.086, 0.02)], 'sea-iron', 12, T(0.66, 0.66, 0.09) @ Ry(78) @ Rz(30))
    return b.finish(), dict(name='Covos de lagosta 3D', footprint='one', blocks=True)


ASSETS = {'lamp': lamp, 'clockwork': clockwork, 'oil-tank': oil_tank, 'fog-bell': fog_bell, 'rowboat': rowboat, 'anchor': anchor,
          'mooring-post': mooring_post, 'net-rack': net_rack, 'lobster-pots': lobster_pots}


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    force = '--replace-unreleased' in argv  # only while v01 has never been in a map or a pack
    wanted = [a for a in argv if not a.startswith('--')] or list(ASSETS)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    manifest_path = TMP / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding='utf-8')) if manifest_path.exists() else {}
    made = []
    for key in wanted:
        glb = OUT / f"{key}-{VERSION}.glb"
        if glb.exists() and not force:
            raise FileExistsError(f"Refusing to overwrite existing file: {glb}")
        obj, info = ASSETS[key]()
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.export_scene.gltf(filepath=str(glb), export_format="GLB", use_selection=True, export_apply=True, export_texcoords=True,
                                  export_normals=True, export_tangents=True, export_materials="EXPORT", export_image_format="JPEG", export_yup=True)
        bounds = [Vector(c) for c in obj.bound_box]
        size = [max(p[i] for p in bounds) - min(p[i] for p in bounds) for i in range(3)]
        tris = len(obj.data.polygons)
        manifest[key] = dict(info, key=key, file=f"{key}-{VERSION}.glb", height=round(size[2], 3), width=round(max(size[0], size[1]), 3),
                             depth=round(min(size[0], size[1]), 3), triangles=tris, materials=[m.name for m in obj.data.materials])
        print(f"LIGHTHOUSE {key}: x={size[0]:.3f} y={size[1]:.3f} z={size[2]:.3f} triangles={tris} materials={len(obj.data.materials)}")
        made.append(obj)
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    if len(wanted) == len(ASSETS):
        for i, obj in enumerate(made):
            obj.location.x = (i - len(made) / 2) * 4.0
        blend = OUT / f"lighthouse-kit-{VERSION}.blend"
        if not blend.exists() or force:
            bpy.ops.file.pack_all()
            bpy.ops.wm.save_as_mainfile(filepath=str(blend))
    print("LIGHTHOUSE done", ", ".join(wanted))


main()
