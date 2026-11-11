"""Offline review renders of the Farmlands houses kit exported GLBs (what the game will cook), one picture each (the Light House preview with this kit paths).

  blender -b -t 2 -P tools/scripts/models/render_farmhouse_kit_preview.py [-- name ...]

Writes work/farmhouse-kit-v01-preview/<name>.png (Cycles on the CPU, few samples, denoised: light on the machine).
"""
from pathlib import Path
import math
import sys
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
SRC = ROOT / "content" / "models" / "farmhouse"
OUT = ROOT / "work" / "farmhouse-kit-v01-preview"
OUT.mkdir(parents=True, exist_ok=True)
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
names = argv or sorted(p.stem for p in SRC.glob("*-v02.glb"))


def point(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()


for name in names:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(SRC / f"{name}.glb"))
    objs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    pts = [o.matrix_world @ Vector(c) for o in objs for c in o.bound_box]
    lo = Vector([min(p[i] for p in pts) for i in range(3)])
    hi = Vector([max(p[i] for p in pts) for i in range(3)])
    centre, span = (lo + hi) / 2, max(hi - lo)
    bpy.ops.mesh.primitive_plane_add(size=80, location=(0, 0, lo.z))
    floor = bpy.context.object
    fmat = bpy.data.materials.new("Preview ground")
    fmat.diffuse_color = (0.16, 0.155, 0.15, 1)
    floor.data.materials.append(fmat)
    world = bpy.data.worlds.new("Preview world")
    bpy.context.scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.42, 0.45, 0.5, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.55
    bpy.ops.object.camera_add(location=centre + Vector((1.0, -1.25, 0.72)) * span * 1.6)
    camera = bpy.context.object
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = span * 1.5
    point(camera, centre)
    bpy.context.scene.camera = camera
    for offset, power in (((1.2, -0.8, 1.6), 900), ((-1.4, -0.6, 0.9), 320), ((0.2, 1.5, 1.0), 260)):
        bpy.ops.object.light_add(type="AREA", location=centre + Vector(offset) * span)
        light = bpy.context.object
        light.data.energy = power * span * span / 9
        light.data.size = span
        point(light, centre)
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 24
    scene.cycles.use_denoising = True
    scene.render.resolution_x = scene.render.resolution_y = 560
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(OUT / f"{name}.png")
    bpy.ops.render.render(write_still=True)
    print("PREVIEW", name)
