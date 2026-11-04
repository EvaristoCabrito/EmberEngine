"""EmberEngine's own round shield (the user, 2026-10-30: "study some of these models and try to make our versions with much more
quality"; the reference studied: Medieval Round Shield by Yue96euY, Sketchfab, CC BY: never copied, only measured and looked at).

  blender -b --factory-startup -P content/models/blender/kit_shield.py

Built from scratch, every piece its own geometry: 8 planks (gaps, chamfered edges, a slight cup, the shield's dome), a hammered iron
rim (a U channel gripping the wood) with domed rivets on both lips, a boss on a flange, 4 wedge bars and 4 arrow bars with scroll
curls, scattered rivets, and on the inside a leather-wrapped grip on two plates and two crossed leather arm straps. Materials from
the CC0 sets in content/textures/ambientcg (Wood060, Metal024 darkened to forged iron, Leather030) with ambient occlusion and edge
wear, baked into one colour / roughness / normal set per material (wood, iron, leather) at 2048, then 1024 and 512 from it.

Writes content/models/shields/round-shield-{2k,1k,512}-v01.glb and round-shield-v01.blend (the user, 2026-10-30: 2K / 1K / 512).
Metres; the shield stands on its rim (base at z 0), the decorated face towards -y (glTF +z).
"""
import math
import os
import random

import bmesh
import bpy
from mathutils import Matrix, Vector, noise

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
TEX = os.path.join(ROOT, "content", "textures", "ambientcg")
OUT = os.path.join(ROOT, "content", "models", "shields")
os.makedirs(OUT, exist_ok=True)
random.seed(7)

R = 0.40          # outer radius (rim included)
RW = 0.392        # the wood disc under the rim
T = 0.016         # plank thickness
SAG = 0.022       # the dome: the front bulges this much at the centre
IRON = 0.003      # strap / rim sheet thickness
LIP_F, LIP_B = 0.030, 0.022  # the rim's lips over the front and the back


def y_front(r):
    """The wood's front surface (towards -y) at radius r."""
    return -T / 2 - SAG * max(0.0, 1.0 - (r / RW) ** 2)


def y_back(r):
    return y_front(r) + T


def front_at(x, z, lift=0.0):
    """A point on the front surface (lift = distance out of it, towards -y)."""
    return Vector((x, y_front(math.hypot(x, z)) - lift, z))


def back_at(x, z, lift=0.0):
    return Vector((x, y_back(math.hypot(x, z)) + lift, z))


def new_object(name, bm):
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    ob = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def hammer(ob, amount, scale):
    """Hammer dents: every vertex pushed along its normal by low-frequency noise."""
    me = ob.data
    me.update()
    for v in me.vertices:
        w = ob.matrix_world @ v.co
        d = noise.noise(w * scale) + 0.5 * noise.noise(w * scale * 2.3)
        v.co += v.normal * d * amount


def smooth(ob):
    for p in ob.data.polygons:
        p.use_smooth = True


def bevel(ob, width, segments=2, angle=35):
    m = ob.modifiers.new("bevel", "BEVEL")
    m.width, m.segments, m.limit_method, m.angle_limit = width, segments, "ANGLE", math.radians(angle)
    m.harden_normals = False
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.modifier_apply(modifier=m.name)


def solid_ribbon(points, widths, lift_front=True, height=IRON, name="ribbon", side_vec=None):
    """A flat iron (or leather) strip along a path lying on the shield's face: points are (x, z) on the face; widths per point;
    the strip sits on the surface (front or back) with its thickness height."""
    bm = bmesh.new()
    rows = []
    for i, (p, w) in enumerate(zip(points, widths)):
        a = Vector(points[max(i - 1, 0)]); b = Vector(points[min(i + 1, len(points) - 1)])
        t = (b - a).normalized() if (b - a).length > 1e-9 else Vector((1, 0))
        n = Vector((-t.y, t.x))
        ring = []
        for s in (-0.5, 0.5):
            x, z = p[0] + n.x * w * s, p[1] + n.y * w * s
            on = front_at if lift_front else back_at
            ring.append(bm.verts.new(on(x, z, 0.0002)))
            ring.append(bm.verts.new(on(x, z, height)))
        rows.append(ring)  # [low-, high-, low+, high+]
    for r0, r1 in zip(rows, rows[1:]):
        for a, b in ((1, 3), (3, 2), (2, 0), (0, 1)):
            bm.faces.new((r0[a], r1[a], r1[b], r0[b]))
    for ring, flip in ((rows[0], True), (rows[-1], False)):
        f = bm.faces.new((ring[0], ring[2], ring[3], ring[1]))
        if flip:
            f.normal_flip()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_object(name, bm)


def rivet(pos, normal, radius, name="rivet", flat=0.55):
    """A domed rivet head standing on a surface (normal = out of it)."""
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=14, v_segments=7, radius=radius)
    for v in list(bm.verts):
        if v.co.z < -1e-6:
            bm.verts.remove(v)
    # close the base
    edge_verts = [v for v in bm.verts if abs(v.co.z) < 1e-6]
    if len(edge_verts) > 2:
        edge_verts.sort(key=lambda v: math.atan2(v.co.y, v.co.x))
        bm.faces.new(edge_verts).normal_flip()
    s = random.uniform(0.88, 1.12)
    for v in bm.verts:
        v.co = Vector((v.co.x * s, v.co.y * s, v.co.z * flat * s))
    rot = Vector((0, 0, 1)).rotation_difference(normal).to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation(pos) @ rot, verts=bm.verts)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_object(name, bm)


def revolve(profile, segments, name):
    """A solid of revolution about the y axis from a closed (r, y) profile."""
    bm = bmesh.new()
    rings = []
    for k in range(segments):
        a = 2 * math.pi * k / segments
        c, s = math.cos(a), math.sin(a)
        rings.append([bm.verts.new((r * c, y, r * s)) for r, y in profile])
    n = len(profile)
    for k in range(segments):
        r0, r1 = rings[k], rings[(k + 1) % segments]
        for i in range(n):
            j = (i + 1) % n
            if r0[i] is r0[j]:
                continue
            try:
                bm.faces.new((r0[i], r1[i], r1[j], r0[j]))
            except ValueError:
                pass
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-7)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_object(name, bm)


# ---------------------------------------------------------------------------------------------------------------- geometry
bpy.ops.wm.read_factory_settings(use_empty=True)
wood_parts, iron_parts, leather_parts = [], [], []

# 1. planks
N_PLANKS, GAP = 8, 0.0014
pw = 2 * RW / N_PLANKS
for i in range(N_PLANKS):
    x0, x1 = -RW + i * pw + GAP / 2, -RW + (i + 1) * pw - GAP / 2
    nx, nz = 7, 64
    cup = random.uniform(0.0004, 0.0009)
    tilt = random.uniform(-0.0006, 0.0006)
    bm = bmesh.new()
    uv_off = (random.uniform(0, 1), random.uniform(0, 1))
    grids = {}
    for side in ("f", "b"):
        for a in range(nx + 1):
            x = x0 + (x1 - x0) * a / nx
            zmax = math.sqrt(max(RW * RW - x * x, 0.0)) - 0.0005
            for c in range(nz + 1):
                z = -zmax + 2 * zmax * c / nz
                u = (a / nx - 0.5)
                y = (y_front if side == "f" else y_back)(math.hypot(x, z)) + cup * (1 - 4 * u * u) * (1 if side == "f" else 1) + tilt * z / RW
                grids[(side, a, c)] = bm.verts.new((x, y, z))
    for a in range(nx):
        for c in range(nz):
            q = [grids[("f", a, c)], grids[("f", a + 1, c)], grids[("f", a + 1, c + 1)], grids[("f", a, c + 1)]]
            bm.faces.new(q)
            q = [grids[("b", a, c)], grids[("b", a, c + 1)], grids[("b", a + 1, c + 1)], grids[("b", a + 1, c)]]
            bm.faces.new(q)
    # side walls
    for a in range(nx):
        for c in (0, nz):
            v = [grids[("f", a, c)], grids[("f", a + 1, c)], grids[("b", a + 1, c)], grids[("b", a, c)]]
            bm.faces.new(v)
    for c in range(nz):
        for a in (0, nx):
            v = [grids[("f", a, c)], grids[("f", a, c + 1)], grids[("b", a, c + 1)], grids[("b", a, c)]]
            bm.faces.new(v)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    # texture UVs: the grain along the plank (z), 0.55 m of texture per repeat, each plank its own piece of the image
    uvl = bm.loops.layers.uv.new("tex")
    for f in bm.faces:
        for l in f.loops:
            co = l.vert.co
            l[uvl].uv = (co.z / 0.55 + uv_off[0], (co.x - x0) / 0.55 + uv_off[1] + (co.y * 2 if abs(f.normal.x) > 0.5 else 0))
    ob = new_object(f"plank{i}", bm)
    bevel(ob, 0.0012, 2, 40)
    smooth(ob)
    wood_parts.append(ob)

# 2. the rim: a U channel round the wood's edge, hammered
prof = []
steps = 6
for k in range(steps + 1):  # front lip following the dome, outside face
    r = RW - LIP_F + LIP_F * k / steps
    prof.append((r, y_front(r) - IRON))
prof += [(R - 0.002, -T / 2 - IRON - 0.001), (R, -T / 2 + 0.002), (R + 0.0015, 0.0), (R, T / 2 - 0.002), (R - 0.002, T / 2 + IRON + 0.001)]
for k in range(steps + 1):  # back lip, outside face
    r = RW - LIP_B * k / steps
    prof.append((r, T / 2 + IRON))
prof += [(RW - LIP_B, T / 2 + 0.0002)]
for k in range(steps, -1, -1):  # back lip inside, against the wood
    r = RW - LIP_B * k / steps
    prof.append((r, T / 2 + 0.0002))
prof += [(RW + 0.0003, 0.0)]
for k in range(steps, -1, -1):  # front lip inside
    r = RW - LIP_F + LIP_F * k / steps
    prof.append((r, y_front(r) - 0.0002))
# remove the duplicated corner points of the polygon walk
clean = []
for p in prof:
    if not clean or (abs(clean[-1][0] - p[0]) > 1e-6 or abs(clean[-1][1] - p[1]) > 1e-6):
        clean.append(p)
rim = revolve(clean, 400, "rim")
hammer(rim, 0.0007, 38)
smooth(rim)
iron_parts.append(rim)
for k in range(56):
    a = 2 * math.pi * (k + 0.5) / 56 + random.uniform(-0.004, 0.004)
    r = RW - LIP_F * 0.5
    p = Vector((r * math.cos(a), y_front(r) - IRON, r * math.sin(a)))
    iron_parts.append(rivet(p, Vector((0, -1, 0)), 0.0048))
    r = RW - LIP_B * 0.5
    p = Vector((r * math.cos(a), T / 2 + IRON, r * math.sin(a)))
    iron_parts.append(rivet(p, Vector((0, 1, 0)), 0.0040))

# 3. the boss on its flange
RF, RB, HB = 0.105, 0.068, 0.058
bp = [(0.0, y_front(0) - IRON - HB)]
for k in range(1, 25):
    r = RB * k / 24
    bp.append((r, y_front(r) - IRON - HB * (1 - (r / RB) ** 2) ** 0.62))
bp += [(RB + 0.004, y_front(RB + 0.004) - IRON - 0.002), (RF - 0.003, y_front(RF) - IRON), (RF, y_front(RF) - IRON * 0.5),
       (RF - 0.001, y_front(RF) - 0.0002), (0.0, y_front(0) - 0.0002)]
boss = revolve(bp, 160, "boss")
hammer(boss, 0.0006, 45)
smooth(boss)
iron_parts.append(boss)
for k in range(8):
    a = 2 * math.pi * (k + 0.5) / 8
    r = 0.088
    iron_parts.append(rivet(front_at(r * math.cos(a), r * math.sin(a), IRON), Vector((0, -1, 0)), 0.0055))

# 4. the bars: 4 wedges (diagonals) and 4 arrow bars with scroll curls (cardinals)
r_in, r_out = RF - 0.006, RW - LIP_F + 0.006
for k in range(8):
    a = 2 * math.pi * k / 8 + math.pi / 8 * 0  # cardinals at 0, 90, 180, 270
    d = Vector((math.cos(a), math.sin(a)))
    pts, ws = [], []
    n = 40
    wedge = k % 2 == 1
    for i in range(n + 1):
        r = r_in + (r_out - r_in) * i / n
        pts.append((d.x * r, d.y * r))
        f = i / n
        ws.append(0.012 + 0.046 * f ** 1.3 if wedge else 0.016 - 0.003 * f)
    bar = solid_ribbon(pts, ws, True, IRON, f"bar{k}")
    bevel(bar, 0.0009, 2, 30)
    hammer(bar, 0.0004, 50)
    smooth(bar)
    iron_parts.append(bar)
    for f in ((0.30, 0.62, 0.9) if wedge else (0.35, 0.7)):
        r = r_in + (r_out - r_in) * f
        iron_parts.append(rivet(front_at(d.x * r, d.y * r, IRON), Vector((0, -1, 0)), 0.006 if wedge else 0.0052))
    if not wedge:
        # arrow tip near the rim
        tip_r = r_out - 0.035
        q = Vector((-d.y, d.x))
        arrow = [(d.x * (tip_r - 0.03) + q.x * s * 0.02, d.y * (tip_r - 0.03) + q.y * s * 0.02) for s in (-1, 1)]
        tri = solid_ribbon([(d.x * (tip_r - 0.03), d.y * (tip_r - 0.03)), (d.x * (tip_r + 0.005), d.y * (tip_r + 0.005))], [0.04, 0.004], True, IRON, f"arrow{k}")
        bevel(tri, 0.0008, 2, 30)
        smooth(tri)
        iron_parts.append(tri)
        # two scroll curls off the bar, like a fleur: out from the bar, sweeping round and curling in to a knob
        for side in (-1, 1):
            curl = []
            for i in range(60):
                t = i / 59
                ang = t * math.pi * 1.55
                rad = 0.085 * (1 - 0.62 * t)
                cx, cy = 0.215, side * 0.085
                s_ = cx + rad * math.sin(ang) * 0.9 - 0.02 * t
                q_ = cy - side * rad * math.cos(ang)
                if i == 0:
                    s0, q0 = s_, q_
                curl.append((d.x * s_ + q.x * q_, d.y * s_ + q.y * q_))
            # the root joins the bar
            root = (d.x * 0.17, d.y * 0.17)
            curl = [root] + curl
            widths = [0.010 * (1 - 0.35 * i / len(curl)) for i in range(len(curl))]
            sc = solid_ribbon(curl, widths, True, IRON, f"scroll{k}{side}")
            bevel(sc, 0.0008, 2, 30)
            smooth(sc)
            iron_parts.append(sc)
            ex, ez = curl[-1]
            iron_parts.append(rivet(front_at(ex, ez, IRON), Vector((0, -1, 0)), 0.0078, flat=0.85))
# scattered rivets on the wood between the bars
for k in range(8):
    a = 2 * math.pi * (k + 0.5) / 8
    for r in (0.19, 0.30):
        a2 = a + random.uniform(-0.05, 0.05)
        iron_parts.append(rivet(front_at(r * math.cos(a2), r * math.sin(a2)), Vector((0, -1, 0)), 0.0062))

# 5. the inside: a grip on two plates, leather wrapped, and two crossed arm straps
GX = -0.15
for zc in (-0.13, 0.13):
    pts = [(GX, zc - 0.03), (GX, zc + 0.03)]
    plate = solid_ribbon(pts, [0.036, 0.036], False, IRON, "plate")
    bevel(plate, 0.0008, 2, 30)
    smooth(plate)
    iron_parts.append(plate)
    for dz in (-0.02, 0.02):
        for dx in (-0.011, 0.011):
            iron_parts.append(rivet(back_at(GX + dx, zc + dz, IRON), Vector((0, 1, 0)), 0.0036))
# the grip: a bent bar standing off the back, round section, with a spiral leather wrap over its middle
path = []
for i in range(41):
    t = i / 40
    z = -0.13 + 0.26 * t
    off = 0.008 + 0.040 * math.sin(math.pi * t) ** 0.7
    path.append(back_at(GX, z, off + IRON))
def tube(path, radius_at, segments, name):
    bm = bmesh.new()
    rings = []
    for i, p in enumerate(path):
        a = path[max(i - 1, 0)]; b = path[min(i + 1, len(path) - 1)]
        t = (b - a).normalized()
        n1 = t.cross(Vector((1, 0, 0))).normalized()
        n2 = t.cross(n1)
        ring = []
        for k in range(segments):
            ang = 2 * math.pi * k / segments
            r = radius_at(i / (len(path) - 1), ang)
            ring.append(bm.verts.new(p + (n1 * math.cos(ang) + n2 * math.sin(ang)) * r))
        rings.append(ring)
    for r0, r1 in zip(rings, rings[1:]):
        for k in range(segments):
            bm.faces.new((r0[k], r0[(k + 1) % segments], r1[(k + 1) % segments], r1[k]))
    for ring, flip in ((rings[0], False), (rings[-1], True)):
        f = bm.faces.new(ring)
        if flip:
            f.normal_flip()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_object(name, bm)
core = tube(path, lambda t, a: 0.0085, 14, "grip")
smooth(core)
iron_parts.append(core)
dense = []
for i in range(161):
    t = i / 160
    z = -0.13 + 0.26 * t
    off = 0.008 + 0.040 * math.sin(math.pi * t) ** 0.7
    dense.append(back_at(GX, z, off + IRON))
wrap = tube(dense[22:139], lambda t, a: 0.0108 + 0.0009 * math.sin(a * 1 + t * 2 * math.pi * 16), 18, "wrap")
smooth(wrap)
leather_parts.append(wrap)
# arm straps, crossed, lifted off the back in the middle
SX = 0.13
for (x0, z0, x1, z1) in ((SX - 0.07, 0.16, SX + 0.07, -0.16), (SX + 0.07, 0.16, SX - 0.07, -0.16)):
    pts, ws = [], []
    for i in range(41):
        t = i / 40
        pts.append((x0 + (x1 - x0) * t, z0 + (z1 - z0) * t))
        ws.append(0.032)
    strap = solid_ribbon(pts, ws, False, 0.0042, "strap")
    # bow it away from the wood in the middle
    for v in strap.data.vertices:
        t = (v.co.z - z0) / (z1 - z0)
        v.co.y += 0.012 * math.sin(math.pi * max(0.0, min(1.0, t))) ** 1.5
    bevel(strap, 0.0007, 2, 30)
    smooth(strap)
    leather_parts.append(strap)
    for (x, z) in ((x0, z0), (x1, z1)):
        for dd in (-0.008, 0.008):
            iron_parts.append(rivet(back_at(x + dd, z, 0.0042), Vector((0, 1, 0)), 0.0034))
iron_parts.append(rivet(back_at(SX, 0.0, 0.0042 + 0.012), Vector((0, 1, 0)), 0.0048))

# --------------------------------------------------------------------------------------------------------------- materials
def image(name):
    return bpy.data.images.load(os.path.join(TEX, name), check_existing=True)


def tex_node(nt, file, colour, proj_box=False, scale=1.0, uv=None):
    n = nt.nodes.new("ShaderNodeTexImage")
    n.image = image(file)
    n.image.colorspace_settings.name = "sRGB" if colour else "Non-Color"
    if proj_box:
        n.projection = "BOX"
        n.projection_blend = 0.25
        tc = nt.nodes.new("ShaderNodeTexCoord")
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (scale, scale, scale)
        nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
        nt.links.new(mp.outputs["Vector"], n.inputs["Vector"])
    elif uv:
        un = nt.nodes.new("ShaderNodeUVMap")
        un.uv_map = uv
        nt.links.new(un.outputs["UV"], n.inputs["Vector"])
    return n


def build_material(name, kind):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    ao = nt.nodes.new("ShaderNodeAmbientOcclusion")
    ao.samples = 24
    ao.inputs["Distance"].default_value = 0.03
    if kind == "wood":
        col = tex_node(nt, "wood-060/wood-060_color_2k.jpg", True, uv="tex")
        rough = tex_node(nt, "wood-060/wood-060_roughness_2k.jpg", False, uv="tex")
        nrm = tex_node(nt, "wood-060/wood-060_normalgl_2k.jpg", False, uv="tex")
        metal, base_mul, rough_add, edge_light = 0.0, (0.82, 0.74, 0.66), 0.08, 0.10
    elif kind == "iron":
        col = tex_node(nt, "metal-024/metal-024_color_2k.jpg", True, True, 6.0)
        rough = tex_node(nt, "metal-024/metal-024_roughness_2k.jpg", False, True, 6.0)
        nrm = tex_node(nt, "metal-024/metal-024_normalgl_2k.jpg", False, True, 6.0)
        metal, base_mul, rough_add, edge_light = 1.0, (0.62, 0.63, 0.66), 0.04, 0.35
    else:
        col = tex_node(nt, "leather-030/leather-030_color_2k.jpg", True, True, 9.0)
        rough = tex_node(nt, "leather-030/leather-030_roughness_2k.jpg", False, True, 9.0)
        nrm = tex_node(nt, "leather-030/leather-030_normalgl_2k.jpg", False, True, 9.0)
        metal, base_mul, rough_add, edge_light = 0.0, (0.95, 0.85, 0.78), 0.05, 0.18
    # colour = texture x tint x ambient occlusion, worn lighter on the sharp edges (pointiness)
    tint = nt.nodes.new("ShaderNodeMix"); tint.data_type = "RGBA"; tint.blend_type = "MULTIPLY"; tint.inputs[0].default_value = 1.0
    tint.inputs[7].default_value = (*base_mul, 1)
    nt.links.new(col.outputs["Color"], tint.inputs[6])
    occ = nt.nodes.new("ShaderNodeMix"); occ.data_type = "RGBA"; occ.blend_type = "MULTIPLY"
    nt.links.new(tint.outputs[2], occ.inputs[6])
    nt.links.new(ao.outputs["AO"], occ.inputs[7])
    occ.inputs[0].default_value = 0.85
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position, ramp.color_ramp.elements[1].position = 0.52, 0.62
    nt.links.new(geo.outputs["Pointiness"], ramp.inputs["Fac"])
    edge = nt.nodes.new("ShaderNodeMix"); edge.data_type = "RGBA"; edge.blend_type = "SCREEN"
    edge.inputs[7].default_value = (edge_light, edge_light, edge_light, 1)
    nt.links.new(ramp.outputs["Color"], edge.inputs[0])
    nt.links.new(occ.outputs[2], edge.inputs[6])
    nt.links.new(edge.outputs[2], bsdf.inputs["Base Color"])
    radd = nt.nodes.new("ShaderNodeMath"); radd.operation = "ADD"; radd.inputs[1].default_value = rough_add
    nt.links.new(rough.outputs["Color"], radd.inputs[0])
    nt.links.new(radd.outputs[0], bsdf.inputs["Roughness"])
    nm = nt.nodes.new("ShaderNodeNormalMap")
    nm.inputs["Strength"].default_value = 1.0
    nt.links.new(nrm.outputs["Color"], nm.inputs["Color"])
    nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    nt.links.new(nm.outputs["Normal"], ao.inputs["Normal"])
    bsdf.inputs["Metallic"].default_value = metal
    m["colour_socket"] = edge.name  # the base colour's last node: baked through an emission (a metal has no diffuse to bake)
    return m, metal


def join(parts, name):
    bpy.ops.object.select_all(action="DESELECT")
    for p in parts:
        p.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    ob.name = name
    return ob


wood = join(wood_parts, "wood")
iron = join(iron_parts, "iron")
leather = join(leather_parts, "leather")
for ob in (iron, leather):
    if not ob.data.uv_layers:
        ob.data.uv_layers.new(name="tex")

scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.samples = 32
scene.cycles.device = "CPU"
baked = {}
for ob, kind in ((wood, "wood"), (iron, "iron"), (leather, "leather")):
    mat, metal = build_material(f"{kind}-src", kind)
    ob.data.materials.clear()
    ob.data.materials.append(mat)
    # the bake UVs: every face its own place in one 2048 image
    bake_uv = ob.data.uv_layers.new(name="bake")
    ob.data.uv_layers.active = bake_uv
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(55), island_margin=0.004, scale_to_bounds=True)
    bpy.ops.uv.pack_islands(margin=0.003)
    bpy.ops.object.mode_set(mode="OBJECT")
    imgs = {}
    for pas in ("color", "roughness", "normal"):
        img = bpy.data.images.new(f"round-shield-{kind}-{pas}", 2048, 2048, alpha=False, float_buffer=False)
        img.colorspace_settings.name = "sRGB" if pas == "color" else "Non-Color"
        node = mat.node_tree.nodes.new("ShaderNodeTexImage")
        node.image = img
        mat.node_tree.nodes.active = node
        un = mat.node_tree.nodes.new("ShaderNodeUVMap"); un.uv_map = "bake"
        mat.node_tree.links.new(un.outputs["UV"], node.inputs["Vector"])
        kw = dict(type={"color": "EMIT", "roughness": "ROUGHNESS", "normal": "NORMAL"}[pas], margin=12, use_clear=True)
        out = next(n for n in mat.node_tree.nodes if n.type == "OUTPUT_MATERIAL")
        if pas == "color":
            emit = mat.node_tree.nodes.new("ShaderNodeEmission")
            mat.node_tree.links.new(mat.node_tree.nodes[mat["colour_socket"]].outputs[2], emit.inputs["Color"])
            mat.node_tree.links.new(emit.outputs["Emission"], out.inputs["Surface"])
        bpy.ops.object.bake(**kw)
        if pas == "color":
            mat.node_tree.links.new(mat.node_tree.nodes["Principled BSDF"].outputs["BSDF"], out.inputs["Surface"])
            mat.node_tree.nodes.remove(emit)
        imgs[pas] = img
        mat.node_tree.nodes.remove(node)
        mat.node_tree.nodes.remove(un)
    baked[ob] = (imgs, metal)
    print("BAKED", kind)

# final materials: the baked images only, the bake UVs only
for ob, (imgs, metal) in baked.items():
    m = bpy.data.materials.new(ob.name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    c = nt.nodes.new("ShaderNodeTexImage"); c.image = imgs["color"]
    r = nt.nodes.new("ShaderNodeTexImage"); r.image = imgs["roughness"]
    n = nt.nodes.new("ShaderNodeTexImage"); n.image = imgs["normal"]
    nm = nt.nodes.new("ShaderNodeNormalMap")
    nt.links.new(c.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(r.outputs["Color"], bsdf.inputs["Roughness"])
    nt.links.new(n.outputs["Color"], nm.inputs["Color"])
    nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    bsdf.inputs["Metallic"].default_value = metal
    ob.data.materials.clear()
    ob.data.materials.append(m)
    ob.data.uv_layers.remove(ob.data.uv_layers["tex"])

# stand it on its rim
for ob in (wood, iron, leather):
    ob.location.z += R
    ob.select_set(True)
bpy.context.view_layer.objects.active = wood
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
for img in bpy.data.images:
    if img.name.startswith("round-shield-"):
        img.pack()
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "round-shield-v01.blend"))

for label, size in (("2k", 2048), ("1k", 1024), ("512", 512)):
    for img in bpy.data.images:
        if img.name.startswith("round-shield-") and img.size[0] > size:
            img.scale(size, size)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, f"round-shield-{label}-v01.glb"), export_format="GLB",
                              export_draco_mesh_compression_enable=False, export_image_format="JPEG", export_jpeg_quality=92)
tris = 0
for ob in (wood, iron, leather):
    ob.data.calc_loop_triangles()
    tris += len(ob.data.loop_triangles)
print("SHIELD", tris, "triangles")
