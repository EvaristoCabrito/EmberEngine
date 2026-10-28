"""The Meshy stone hearth (E05): the user's generated mesh (Downloads/Meshy_AI_Stone_Hearth_Glow_*.blend: one mesh,
391k triangles, no UVs, no material) catalogued as content/models/meshy/stone-hearth-v01: scaled from metres to
world units (1 unit = 0.68 m), stood on z = 0, a triplanar unwrap, Ember's grey masonry with soot inside the
firebox. The original file is read only.

  blender -b <the Meshy .blend> --python content/models/blender/export_hearth.py
"""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
from emberlib import *  # noqa
from mathutils import Vector

src = [o for o in bpy.data.objects if o.type == 'MESH']
assert len(src) == 1, src
o = src[0]
o.scale = (1 / 0.68,) * 3
apply_transform(o)
o.data.materials.clear()
o.data.materials.append(M('masonry'))
ground(o)
box_uv(o, 1.3)
# soot: the recessed firebox (the faces deep inside the front opening)
me = o.data
xs = [v.co.x for v in me.vertices]; ys = [v.co.y for v in me.vertices]; zs = [v.co.z for v in me.vertices]
cx, cy = (min(xs) + max(xs)) * 0.5, (min(ys) + max(ys)) * 0.5
depth = max(ys) - min(ys)
set_material_where(o, M('masonry-crypt'), lambda c, n: abs(c.y - cy) < depth * 0.22 and abs(c.x - cx) < 0.9 and c.z < max(zs) * 0.7 and c.z > 0.08)
smooth(o, 40)
o.name = o.data.name = 'stone-hearth'
r = export(o, 'meshy', 'stone-hearth')
print('JSON_BEGIN' + __import__('json').dumps({'kit': 'meshy', 'assets': [r], 'errors': []}) + 'JSON_END')
