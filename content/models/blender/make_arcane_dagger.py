"""The arcane version of the Poly Haven dagger without its sheath (ornate_medieval_dagger-v01.glb) -> -v02.glb.

  blender -b --factory-startup -P content/models/blender/make_arcane_dagger.py -- <v01.glb> <v02.glb> <texture folder>

The geometry and the UVs are v01's, untouched. New maps are made from v01's own (v01's files are not changed):
  - colour: the blade and the metal fittings blackened and cooled, the leather grip darkened towards violet;
  - emissive (new): the engravings already in the normal map (the flame on the blade's base, the sun and the scrolls of the
    guard) found where the map bends away from flat, on the blade and the guard only, glowing violet (DaggerFx's kViolet);
  - ORM and normal: v01's own images.
Which part a texel belongs to comes from the faces that cover it in the UV layout, by the face's height along the dagger
(blade / guard / grip / pommel, the heights measured on the model).
"""
import os, sys

import bpy
import numpy as np

args = sys.argv[sys.argv.index('--') + 1:]
src, out, texdir = args[0], args[1], args[2]
for p in (out,):
    if os.path.exists(p):
        raise SystemExit(f'arcane: {p} exists: bump the version instead of overwriting')
os.makedirs(texdir, exist_ok=True)

VIOLET = np.array([0.62, 0.4, 1.0], np.float32)
BLADE, GUARD, GRIP, POMMEL = 1, 2, 3, 4
# on the colour map's stored (sRGB) values: 0.66 there is ~0.40 of the light
TINT = {BLADE: (0.66, 0.66, 0.74), GUARD: (0.62, 0.60, 0.72), GRIP: (0.60, 0.50, 0.85), POMMEL: (0.62, 0.60, 0.72)}

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
objs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
if len(objs) != 1:
    raise SystemExit(f'arcane: expected one mesh in {src}, found {len(objs)}')
dag = objs[0]
mat = dag.active_material
nt = mat.node_tree
bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')


def image_into(socket_name):
    """The image node that feeds a BSDF input (through a separate-colour or normal-map node if there is one)."""
    s = bsdf.inputs[socket_name].links[0].from_node
    while s.type != 'TEX_IMAGE':
        s = next(i for i in s.inputs if i.is_linked).links[0].from_node
    return s


diff_node = image_into('Base Color')
nor_img = image_into('Normal').image
diff = diff_node.image
W, H = diff.size
if tuple(nor_img.size) != (W, H):
    raise SystemExit('arcane: colour and normal maps differ in size')


def pixels(im):
    a = np.empty(im.size[0] * im.size[1] * 4, np.float32)
    im.pixels.foreach_get(a)
    return a.reshape(im.size[1], im.size[0], 4)


# --- which part each texel belongs to: rasterise the UV triangles with their face's part
me = dag.data
me.calc_loop_triangles()
uv = np.array([l.uv[:] for l in me.uv_layers.active.data], np.float32)
region = np.zeros((H, W), np.uint8)
mw = dag.matrix_world
for tri in me.loop_triangles:
    # glTF is Y-up; the importer turns it back to Blender's Z-up, so the dagger runs along z as measured (-0.079 .. 0.267 m)
    zc = (mw @ me.polygons[tri.polygon_index].center).z
    part = BLADE if zc > 0.056 else GUARD if zc > 0.037 else GRIP if zc > -0.036 else POMMEL
    p = uv[list(tri.loops)] * np.array([W, H], np.float32) - 0.5
    x0, y0 = np.floor(p.min(0)).astype(int)
    x1, y1 = np.ceil(p.max(0)).astype(int)
    x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, W - 1), min(y1, H - 1)
    if x1 < x0 or y1 < y0:
        continue
    X, Y = np.meshgrid(np.arange(x0, x1 + 1, dtype=np.float32), np.arange(y0, y1 + 1, dtype=np.float32))
    (ax, ay), (bx, by), (cx, cy) = p
    d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
    if abs(d) < 1e-9:
        continue
    l1 = ((by - cy) * (X - cx) + (cx - bx) * (Y - cy)) / d
    l2 = ((cy - ay) * (X - cx) + (ax - cx) * (Y - cy)) / d
    inside = (l1 >= -0.02) & (l2 >= -0.02) & (1 - l1 - l2 >= -0.02)
    sub = region[y0:y1 + 1, x0:x1 + 1]
    sub[inside] = part
# the islands' padding takes the part of its nearest covered texel (no seam of the old colour at an island's border)
for _ in range(10):
    empty = region == 0
    if not empty.any():
        break
    grow = region.copy()
    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        sh = np.roll(np.roll(region, dy, 0), dx, 1)
        take = empty & (grow == 0) & (sh != 0)
        grow[take] = sh[take]
    region = grow

# --- colour
c = pixels(diff)
tint = np.ones((H, W, 3), np.float32)
for part, t in TINT.items():
    tint[region == part] = t
c[..., :3] *= tint

# --- emissive: the engravings, where the normal map bends away from flat, on the blade and the guard
n = pixels(nor_img)
bend = np.hypot(n[..., 0] - 0.5, n[..., 1] - 0.5) * 2.0
mask = np.clip((bend - 0.18) / 0.22, 0.0, 1.0)
mask[(region != BLADE) & (region != GUARD)] = 0.0
# one soft texel around each line (a glow, not a hard cut)
soft = mask.copy()
for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
    soft = np.maximum(soft, np.roll(np.roll(mask, dy, 0), dx, 1) * 0.5)
emit = np.ones((H, W, 4), np.float32)
emit[..., :3] = soft[..., None] * VIOLET

stem = os.path.splitext(os.path.basename(out))[0]


def new_image(name, data, colour):
    path = os.path.join(texdir, f'{name}.png')
    if os.path.exists(path):
        raise SystemExit(f'arcane: {path} exists: bump the version instead of overwriting')
    im = bpy.data.images.new(name, W, H, alpha=False, float_buffer=False)
    im.colorspace_settings.name = 'sRGB' if colour else 'Non-Color'
    im.pixels.foreach_set(data.ravel())
    im.file_format = 'PNG'
    im.save(filepath=os.path.abspath(path))
    if not os.path.exists(path):
        raise SystemExit(f'arcane: {path} was not written')
    bpy.data.images.remove(im)
    im = bpy.data.images.load(os.path.abspath(path))   # the model takes the file as written (the exporter embeds its bytes)
    im.colorspace_settings.name = 'sRGB' if colour else 'Non-Color'
    return im


# a byte image's pixels are its stored (sRGB) values, read and written alike: the same encoding in and out
diff_new = new_image(f'{stem}-diff', c, True)
emit_new = new_image(f'{stem}-emissive', emit, True)
region_dbg = np.ones((H, W, 4), np.float32)
region_dbg[..., :3] = (region / 4.0)[..., None]
new_image(f'{stem}-parts', region_dbg, False)   # which texel is which part (a record, not used by the model)
print(f'arcane: engraving texels {int((mask > 0.5).sum())} on blade + guard; parts blade {int((region == BLADE).sum())} '
      f'guard {int((region == GUARD).sum())} grip {int((region == GRIP).sum())} pommel {int((region == POMMEL).sum())} '
      f'uncovered {int((region == 0).sum())}')

diff_node.image = diff_new
em = nt.nodes.new('ShaderNodeTexImage')
em.image = emit_new
nt.links.new(em.outputs['Color'], bsdf.inputs['Emission Color'])
bsdf.inputs['Emission Strength'].default_value = 3.0
mat.name = stem
bpy.ops.export_scene.gltf(filepath=out, export_format='GLB', export_apply=True, export_image_format='AUTO')
print(f'arcane: wrote {out}')
