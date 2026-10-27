"""The street lamp v02 (2026-10-27; the user: "can you do a 3D lamp light? i will draw any textures you need"): a square oak post
on a braced plinth, iron bands, an arm with a knee brace, a chain and a hexagonal iron lantern whose glass glows from the candle
inside. Shaped after the village's lantern-post cards (decoracoes-2d/city-lantern-post.png, lamppost.png).

Textures: the user's own photographs (Attachments, 2026-10-27) -> content/models/textures/lamp-wood-v01.jpg (mirrored both ways:
seamless), lamp-iron-v01.jpg (make_tile.py), lamp-glass-v01.jpg (one pane each face) + lamp-glass-glow-v01.jpg (the same pane
warmed by the candle: the emissive texture).

  blender -b --python content/models/blender/street_lamp_v02.py

Prints the lantern's offset from the piece's centre (the engine centres a model on its bounds): models.json light "offset".
"""
import sys, os, math, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emberlib import *  # noqa
import bmesh
from mathutils import Vector, Matrix

LX = 0.92               # the lantern hangs this far out along +x from the post's axis
PANE_LO, PANE_HI = 2.36, 2.80
PANE_R = 0.205          # the hexagon's corner radius


def wood():
    return material('lamp-wood', color='lamp-wood-v01', roughness=0.85)


def iron():
    return material('lamp-iron', color='lamp-iron-v01', roughness=0.62, metallic=0.35)


def glass():
    """The pane photograph as the colour, its warmed copy as the glow (the candle seen through it)."""
    m = material('lamp-glass', color='lamp-glass-v01', roughness=0.2)
    nt = m.node_tree
    bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
    t = nt.nodes.new('ShaderNodeTexImage')
    t.image = image('lamp-glass-glow-v01', 'sRGB')
    nt.links.new(t.outputs['Color'], bsdf.inputs['Emission Color'])
    bsdf.inputs['Emission Strength'].default_value = 10.0
    return m


def grain_uv(o, su, sv):
    """Planar per face like box_uv, with the image's width over `su` units and its height over `sv` units, the height along z
    on the side faces: the grain runs up the piece as it stands when this is called."""
    me = o.data
    bm = bmesh.new()
    bm.from_mesh(me)
    uv = bm.loops.layers.uv.verify()
    for f in bm.faces:
        n = f.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        for l in f.loops:
            p = l.vert.co
            if ax == 0:
                u, v = p.y * (1 if n.x > 0 else -1) / su, p.z / sv
            elif ax == 1:
                u, v = p.x * (-1 if n.y > 0 else 1) / su, p.z / sv
            else:
                u, v = p.x / su, p.y / su
            l[uv].uv = (u, v)
    bm.to_mesh(me)
    bm.free()


def beam(name, w, d, a, b, mat, uv=None):
    """A timber (or iron bar) w x d in section from a to b: built standing, unwrapped standing, then turned into place."""
    a, b = Vector(a), Vector(b)
    length = (b - a).length
    o = box(name, (w, d, length), mat=mat)
    if uv:
        uv(o)
    rot = Vector((0, 0, 1)).rotation_difference((b - a).normalized()).to_matrix().to_4x4()
    o.data.transform(Matrix.Translation((a + b) * 0.5) @ rot)
    return o


def hex_panes(name, r, z0, z1, mat):
    """Six quads round the lantern, each carrying the whole pane image (corners at angles k x 60 deg, as lathe(segs=6) puts them)."""
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new('UVMap')
    pts = [(r * math.cos(k * math.pi / 3), r * math.sin(k * math.pi / 3)) for k in range(6)]
    for k in range(6):
        # counter-clockwise along the ring, then up: the face's normal points out of the lantern
        (x0, y0), (x1, y1) = pts[k], pts[(k + 1) % 6]
        vs = [bm.verts.new((LX + x0, y0, z0)), bm.verts.new((LX + x1, y1, z0)), bm.verts.new((LX + x1, y1, z1)), bm.verts.new((LX + x0, y0, z1))]
        f = bm.faces.new(vs)
        for l, c in zip(f.loops, ((0, 0), (1, 0), (1, 1), (0, 1))):
            l[uv].uv = c
    o = bpy.data.objects.new(name, bpy.data.meshes.new(name))
    bpy.context.collection.objects.link(o)
    bm.to_mesh(o.data)
    bm.free()
    o.data.materials.append(mat)
    return o


def street_lamp():
    W, I, G = wood(), iron(), glass()
    post_uv = lambda o: grain_uv(o, 1.6, 4.0)
    parts = [beam('plinth', 0.7, 0.7, (0, 0, 0), (0, 0, 0.12), W, lambda o: grain_uv(o, 1.6, 1.6)),
             beam('foot', 0.4, 0.4, (0, 0, 0.12), (0, 0, 0.54), W, post_uv),
             beam('post', 0.26, 0.26, (0, 0, 0.12), (0, 0, 3.78), W, post_uv),
             beam('top', 0.32, 0.32, (0, 0, 3.78), (0, 0, 3.85), W, post_uv)]
    for k in range(4):  # the four braces from the plinth's edge up to the post
        c, s = math.cos(k * math.pi / 2), math.sin(k * math.pi / 2)
        parts.append(beam(f'brace{k}', 0.09, 0.09, (0.33 * c, 0.33 * s, 0.13), (0.14 * c, 0.14 * s, 0.8), W, post_uv))
    finial = cylinder('finial', 0.15, 0.0, 0.18, loc=(0, 0, 3.94), rot=(0, 0, math.pi / 4), segs=4, mat=I)
    box_uv(finial, 0.5)
    parts.append(finial)
    # iron bands round the post, a rivet on each face
    for z, h in ((0.58, 0.07), (1.55, 0.07), (3.05, 0.07), (3.42, 0.22), (3.66, 0.07)):
        band = box(f'band{z}', (0.29, 0.29, h), loc=(0, 0, z), mat=I)
        box_uv(band, 0.5)
        parts.append(band)
        for k in range(4):
            c, s = math.cos(k * math.pi / 2), math.sin(k * math.pi / 2)
            parts.append(sphere(f'rivet{z}{k}', 0.022, (0.146 * c, 0.146 * s, z), subdiv=1, mat=I))
    # the arm through the post, its knee brace, the iron collar where the chain hangs
    parts.append(beam('arm', 0.15, 0.15, (-0.2, 0, 3.42), (1.12, 0, 3.42), W, post_uv))
    parts.append(beam('knee', 0.1, 0.1, (0.13, 0, 2.86), (0.64, 0, 3.36), W, post_uv))
    collar = box('collar', (0.07, 0.175, 0.175), loc=(LX, 0, 3.42), mat=I)
    box_uv(collar, 0.5)
    parts.append(collar)
    # hook and chain: links turning a quarter each
    parts.append(torus('hook', 0.05, 0.014, loc=(LX, 0, 3.31), rot=(math.pi / 2, 0, 0), mat=I, segs=12, ring=6))
    for i, z in enumerate((3.22, 3.14, 3.06)):
        parts.append(torus(f'link{i}', 0.042, 0.011, loc=(LX, 0, z), rot=(math.pi / 2, 0, math.pi / 2 * ((i + 1) % 2)), mat=I, segs=10, ring=6))
    # the lantern: cap, ring, six panes in an iron frame, tray, the drop under it
    parts.append(lathe('cap', [(0, 0), (0.27, 0), (0.27, 0.03), (0.09, 0.17), (0.05, 0.2), (0, 0.2)], segs=6, loc=(LX, 0, PANE_HI), mat=I))
    parts.append(torus('ring', 0.035, 0.011, loc=(LX, 0, PANE_HI + 0.22), rot=(math.pi / 2, 0, 0), mat=I, segs=10, ring=6))
    parts.append(hex_panes('glass', PANE_R, PANE_LO, PANE_HI, G))
    for k in range(6):
        a = k * math.pi / 3
        parts.append(cylinder(f'bar{k}', 0.016, 0.016, PANE_HI - PANE_LO + 0.02, loc=(LX + 0.21 * math.cos(a), 0.21 * math.sin(a), (PANE_LO + PANE_HI) / 2), segs=6, mat=I))
    for z in (PANE_LO + 0.01, (PANE_LO + PANE_HI) / 2, PANE_HI - 0.01):
        parts.append(lathe(f'frame{z}', [(0.19, -0.012), (0.218, -0.012), (0.218, 0.012), (0.19, 0.012), (0.19, -0.012)], segs=6, loc=(LX, 0, z), mat=I))
    parts.append(lathe('tray', [(0, 0), (0.25, 0), (0.25, 0.05), (0, 0.05)], segs=6, loc=(LX, 0, PANE_LO - 0.05), mat=I))
    parts.append(lathe('drop', [(0, 0), (0.03, 0.06), (0.12, 0.16), (0.12, 0.18), (0, 0.18)], segs=6, loc=(LX, 0, PANE_LO - 0.23), mat=I))
    for p in parts:
        if p.name.startswith(('cap', 'ring', 'frame', 'tray', 'drop', 'hook', 'link', 'bar', 'rivet')):
            box_uv(p, 0.5)
    for p in parts:  # one UV layer, one name: join keeps layers apart by name (the panes' own went to a second layer)
        for extra in list(p.data.uv_layers)[1:]:
            p.data.uv_layers.remove(extra)
        if p.data.uv_layers:
            p.data.uv_layers[0].name = 'UVMap'
    o = join(parts, 'street-lamp')
    smooth(o, 40)
    xs = [v.co.x for v in o.data.vertices]
    ys = [v.co.y for v in o.data.vertices]
    centre = ((min(xs) + max(xs)) * 0.5, (min(ys) + max(ys)) * 0.5)
    ground(o)
    # the lantern's centre from the piece's centre, in the engine's frame (glTF: x = Blender x, z = -Blender y)
    light = {'offset': [round(LX - centre[0], 3), round(centre[1], 3)], 'minHeight': round((PANE_LO + PANE_HI) / 2, 3)}
    print('LIGHT_BEGIN' + json.dumps(light) + 'LIGHT_END')
    return o


if __name__ == '__main__':
    reset()
    o = street_lamp()
    r = export(o, 'city', 'street-lamp', version='v02')
    print(f'[ok] city/street-lamp v02: {r["triangles"]} tris, {r["size"]}')
    print('JSON_BEGIN' + json.dumps(r) + 'JSON_END')
