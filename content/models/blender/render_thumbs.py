"""Editor thumbnails of every 3D decoration (the user, 2026-10-28: "como é que eu vou encontrar qualquer coisa no editor se não tem
nem preview"): each prop the Map Editor's "3D Decorations" tab lists (editor-catalog.json: architectureOrder, decoMeta model3d +
props3d) is imported alone, framed in a three-quarter view from above and rendered on a clear background into
content/ui/thumbs3d/<id>.png, which MapEditorStage's palette draws in place of the bare "3D".

    blender -b --python content/models/blender/render_thumbs.py                 # every prop without a thumbnail yet
    blender -b --python content/models/blender/render_thumbs.py -- --force      # every prop again
    blender -b --python content/models/blender/render_thumbs.py -- id,id        # just these
"""
import json
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Vector

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
OUT = os.path.join(ROOT, "content", "ui", "thumbs3d")
SIZE = 256        # the saved thumbnail
RENDER = 512      # rendered larger, then cropped to the model and scaled down
YAW, PITCH = math.radians(35), math.radians(28)   # the view: turned 35 deg, looking 28 deg down


def model_file(url):
    # the catalog's /game/... URL: content/assets first (the old props and trees), then EmberEngine's library (/game/ember/... =
    # content/models/...)
    for path in (os.path.join(ROOT, "content", "assets", url.lstrip("/")),
                 os.path.join(ROOT, "content", "models", url[len("/game/ember/"):]) if url.startswith("/game/ember/") else ""):
        if path and os.path.exists(path):
            return path
    return None


def props():
    cat = json.load(open(os.path.join(ROOT, "content", "data", "editor-catalog.json"), encoding="utf-8"))
    e, meta = cat["editor"], cat["editor"]["decoMeta"]
    out = []
    for pid in e["architectureOrder"]:
        m = meta.get(pid) or {}
        if not m.get("model3d") or not m.get("props3d"):
            continue
        model = (cat["decorations"].get(pid) or {}).get("model") or {}
        if model.get("file"):
            out.append((pid, model["file"]))
    return out


def setup():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    for engine in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT", "CYCLES"):
        try:
            scene.render.engine = engine
            break
        except TypeError:
            continue
    scene.render.resolution_x = scene.render.resolution_y = RENDER
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("thumb")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.55, 0.58, 0.62, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.9
    scene.world = world
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3.5
    sun.data.angle = math.radians(8)
    sun.rotation_euler = (math.radians(40), 0, math.radians(-30))
    scene.collection.objects.link(sun)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.lens = 50
    cam.data.clip_start, cam.data.clip_end = 0.01, 10000
    scene.collection.objects.link(cam)
    scene.camera = cam
    return scene, cam


def frame(cam, objs):
    lo, hi = Vector((1e9, 1e9, 1e9)), Vector((-1e9, -1e9, -1e9))
    for o in objs:
        if o.type != "MESH":
            continue
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            lo = Vector(map(min, lo, w))
            hi = Vector(map(max, hi, w))
    view = Vector((math.sin(YAW) * math.cos(PITCH), -math.cos(YAW) * math.cos(PITCH), math.sin(PITCH)))
    right = (-view).cross(Vector((0, 0, 1))).normalized()
    up = right.cross(-view)
    corners = [Vector((x, y, z)) for x in (lo.x, hi.x) for y in (lo.y, hi.y) for z in (lo.z, hi.z)]
    centre = (lo + hi) / 2
    # the box's middle on screen, then the distance at which every corner fits the lens (a tall tree fills the frame's height)
    xs, ys = [(c - centre).dot(right) for c in corners], [(c - centre).dot(up) for c in corners]
    centre += right * (max(xs) + min(xs)) / 2 + up * (max(ys) + min(ys)) / 2
    tan = math.tan(cam.data.angle / 2)
    dist = max(max(abs((c - centre).dot(right)), abs((c - centre).dot(up))) / tan + (c - centre).dot(view) for c in corners) * 1.06
    cam.location = centre + view * max(dist, 1e-3)
    cam.rotation_euler = (-view).to_track_quat("-Z", "Y").to_euler()


def tighten(path):
    # the render cropped to its own opaque pixels, centred in a square with a small margin, back at SIZE (a model's box can be far
    # larger than what shows: hidden parts, a ground card)
    img = bpy.data.images.load(path)
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)
    ys, xs = np.nonzero(px[..., 3] > 0.02)
    if len(xs):
        cx, cy = (xs.min() + xs.max() + 1) / 2, (ys.min() + ys.max() + 1) / 2
        side = int(math.ceil(max(xs.max() - xs.min() + 1, ys.max() - ys.min() + 1) * 1.08))
        sq = np.zeros((side, side, 4), dtype=np.float32)
        x0, y0 = int(round(cx - side / 2)), int(round(cy - side / 2))
        sx0, sy0, sx1, sy1 = max(x0, 0), max(y0, 0), min(x0 + side, w), min(y0 + side, h)
        sq[sy0 - y0:sy1 - y0, sx0 - x0:sx1 - x0] = px[sy0:sy1, sx0:sx1]
        out = bpy.data.images.new("thumb-crop", side, side, alpha=True)
        out.pixels[:] = sq.ravel()
        out.scale(SIZE, SIZE)
        out.filepath_raw = path
        out.file_format = "PNG"
        out.save()
        bpy.data.images.remove(out)
    bpy.data.images.remove(img)


def clear(objs):
    for o in objs:
        bpy.data.objects.remove(o, do_unlink=True)
    for coll in (bpy.data.meshes, bpy.data.materials, bpy.data.images, bpy.data.textures, bpy.data.armatures):
        for block in list(coll):
            if block.users == 0:
                coll.remove(block)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    force = "--force" in argv
    only = set(",".join(a for a in argv if not a.startswith("--")).split(",")) - {""}
    os.makedirs(OUT, exist_ok=True)
    scene, cam = setup()
    done = failed = 0
    for pid, url in props():
        if only and pid not in only:
            continue
        dst = os.path.join(OUT, pid + ".png")
        if os.path.exists(dst) and not force and not only:
            continue
        path = model_file(url)
        if not path:
            print(f"thumb {pid}: model not found ({url})")
            failed += 1
            continue
        before = set(bpy.data.objects)
        try:
            bpy.ops.import_scene.gltf(filepath=path)
        except Exception as err:  # a broken file is reported, the rest still render
            print(f"thumb {pid}: import failed: {err}")
            clear([o for o in bpy.data.objects if o not in before])
            failed += 1
            continue
        objs = [o for o in bpy.data.objects if o not in before]
        frame(cam, objs)
        scene.render.filepath = dst
        bpy.ops.render.render(write_still=True)
        clear(objs)
        tighten(dst)
        done += 1
        print(f"thumb {pid} -> {os.path.relpath(dst, ROOT)}")
    print(f"thumbs: {done} rendered, {failed} failed")


main()
