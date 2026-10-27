import bpy
import math
from pathlib import Path

ROOT = Path(r"C:\EmberEngine")
OUT = ROOT / "work/plains-random-panorama-proposal-20261026"
TREE = ROOT / "content/models/forest/mature-tree-v04-photo.glb"
TEXTURE_1 = ROOT / "content/game/public/game/ground-variant-tiles/plains-meadow-natural-v01.png"
TEXTURE_2 = ROOT / "content/game/public/game/ground-variant-tiles/plains-meadow-natural-v02.png"
TEXTURE_3 = ROOT / "content/game/public/game/ground-variant-tiles/plains-meadow-scrub-rare-v01.png"
WATER_TEXTURE = ROOT / "content/game/public/game/ground-engine2-v4/shallow-water/color.png"


def clear_objects():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)


def image_material(name, path, repeat_x, repeat_y, roughness=0.95):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    shader = nodes.get("Principled BSDF")
    shader.inputs["Roughness"].default_value = roughness
    image = nodes.new("ShaderNodeTexImage")
    image.image = bpy.data.images.load(str(path), check_existing=True)
    image.extension = "REPEAT"
    uv = nodes.new("ShaderNodeTexCoord")
    mapping = nodes.new("ShaderNodeMapping")
    mapping.inputs["Scale"].default_value = (repeat_x, repeat_y, 1)
    links.new(uv.outputs["UV"], mapping.inputs["Vector"])
    links.new(mapping.outputs["Vector"], image.inputs["Vector"])
    links.new(image.outputs["Color"], shader.inputs["Base Color"])
    return mat


def terrain_z(x, y, config):
    def smooth_hill(start, peak, end, height, phase):
        if y <= start or y >= end:
            return 0.0
        if y < peak:
            t = (y - start) / (peak - start)
            shape = math.sin(t * math.pi * 0.5) ** 2
        else:
            t = (y - peak) / (end - peak)
            shape = math.cos(t * math.pi * 0.5) ** 2
        cross = 0.84 + 0.10 * math.sin(x * 0.014 + phase) + 0.06 * math.sin(x * 0.031 + phase * 1.8)
        return height * shape * cross
    return (
        smooth_hill(56, 105, 177, config["ridge1"], config["phase"])
        + smooth_hill(150, 205, 275, config["ridge2"], config["phase"] + 1.1)
        + 0.12 * math.sin(x * 0.025 + config["phase"]) * max(0.0, min(1.0, (y + 15) / 40))
    )


def build_surface(name, xmin, xmax, ymin, ymax, nx, ny, zfn, mat):
    vertices, faces, uvs = [], [], []
    for j in range(ny + 1):
        v = j / ny
        y = ymin + (ymax - ymin) * v
        for i in range(nx + 1):
            u = i / nx
            x = xmin + (xmax - xmin) * u
            vertices.append((x, y, zfn(x, y)))
            uvs.append((u, v))
    stride = nx + 1
    for j in range(ny):
        for i in range(nx):
            a = j * stride + i
            faces.append((a, a + 1, a + stride + 1, a + stride))
    mesh = bpy.data.meshes.new(name + " mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(mat)
    uv = mesh.uv_layers.new(name="UVMap")
    for poly in mesh.polygons:
        for loop_index in poly.loop_indices:
            uv.data[loop_index].uv = uvs[mesh.loops[loop_index].vertex_index]
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def add_trees(config):
    bpy.ops.import_scene.gltf(filepath=str(TREE))
    source = [o for o in bpy.context.selected_objects if o.type == "MESH" and "mature tree" in o.name.lower()]
    if not source:
        raise RuntimeError("Photo tree meshes were not imported")
    x_positions = config["trees"]
    scales = [0.82, 1.0, 0.74, 1.08, 0.9, 0.78, 0.96]
    for i, x in enumerate(x_positions):
        y = 62 + (i % 3) * 7
        z = terrain_z(x, y, config)
        for original in source:
            duplicate = original.copy()
            duplicate.data = original.data
            duplicate.name = f"Far meadow tree {i + 1} · {original.name}"
            bpy.context.collection.objects.link(duplicate)
            duplicate.location = (x, y, z)
            scale = scales[i % len(scales)]
            duplicate.scale = (scale, scale, scale)
            duplicate.rotation_euler.z = (i % 2) * 0.25
    for original in source:
        bpy.data.objects.remove(original, do_unlink=True)


def add_river(mat, config):
    verts, faces, uvs = [], [], []
    count = 100
    for i in range(count + 1):
        u = i / count
        x = -120 + 240 * u
        center_y = 27 + 12 * (0.5 + 0.5 * math.sin(u * math.tau + 0.5))
        width = 2.4 + 1.4 * math.sin(u * math.pi)
        for side in (-1, 1):
            y = center_y + side * width
            z = terrain_z(x, y, config) + 0.035
            verts.append((x, y, z))
            uvs.append((u * 5, 0 if side < 0 else 1))
    for i in range(count):
        a = i * 2
        faces.append((a, a + 2, a + 3, a + 1))
    mesh = bpy.data.meshes.new("Narrow ford water mesh")
    mesh.from_pydata(verts, [], faces)
    mesh.materials.append(mat)
    uv = mesh.uv_layers.new(name="UVMap")
    for poly in mesh.polygons:
        for li in poly.loop_indices:
            uv.data[li].uv = uvs[mesh.loops[li].vertex_index]
    obj = bpy.data.objects.new("Narrow winding ford", mesh)
    bpy.context.collection.objects.link(obj)


def setup_camera_and_render(config):
    camera_data = bpy.data.cameras.new("Long panorama camera")
    camera = bpy.data.objects.new("Long panorama camera", camera_data)
    bpy.context.collection.objects.link(camera)
    camera.location = (0, 0, 1.6)
    camera.rotation_euler = (math.radians(88), 0, 0)
    camera_data.type = "PERSP"
    camera_data.lens = 20
    bpy.context.scene.camera = camera

    light_data = bpy.data.lights.new("Soft field daylight", "AREA")
    light = bpy.data.objects.new("Soft field daylight", light_data)
    bpy.context.collection.objects.link(light)
    light.location = (-15, 5, 48)
    light_data.energy = 150000
    light_data.shape = "DISK"
    light_data.size = 38

    world = bpy.context.scene.world
    world.color = (0.42, 0.42, 0.42)
    world.use_nodes = True
    world.node_tree.nodes.get("Background").inputs["Color"].default_value = (0.42, 0.42, 0.42, 1)
    world.node_tree.nodes.get("Background").inputs["Strength"].default_value = 0.35

    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 24
    scene.render.resolution_x = 3072
    scene.render.resolution_y = 1024
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.film_transparent = True
    scene.view_settings.view_transform = "AgX"
    scene.render.filepath = str(OUT / config["file"])
    bpy.ops.render.render(write_still=True)


def build(config):
    clear_objects()
    ground = image_material("Continuous prairie ground", config["ground"], 42, 48)
    water = image_material("Shallow river surface", WATER_TEXTURE, 3, 1)
    build_surface("Continuous rolling grassland", -260, 260, -15, 280, 180, 105,
                  lambda x, y: terrain_z(x, y, config), ground)
    if config.get("river"):
        add_river(water, config)
    if config.get("stones"):
        stone = bpy.data.materials.new("Weathered fieldstone")
        stone.use_nodes = True
        shader = stone.node_tree.nodes.get("Principled BSDF")
        shader.inputs["Base Color"].default_value = (0.22, 0.24, 0.19, 1)
        shader.inputs["Roughness"].default_value = 0.97
        for x, y, size in [(-20, 25, 1.25), (22, 39, 0.95), (2, 53, 0.72)]:
            bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=1, location=(x, y, terrain_z(x, y, config) + size * 0.48))
            rock = bpy.context.object
            rock.name = "Low weathered fieldstone"
            rock.scale = (size, size * 0.68, size * 0.52)
            rock.data.materials.append(stone)
    add_trees(config)
    setup_camera_and_render(config)


for item in [
    {"file": "plains-panorama-01-v02-preview.png", "ground": TEXTURE_1, "ridge1": 12, "ridge2": 7, "phase": 0.4, "trees": [-58, -38, -16, 15, 39, 57]},
    {"file": "plains-panorama-02-v02-preview.png", "ground": TEXTURE_2, "ridge1": 11, "ridge2": 9, "phase": 2.2, "trees": [-62, -41, -20, 22, 43, 62], "river": True},
    {"file": "plains-panorama-03-v02-preview.png", "ground": TEXTURE_3, "ridge1": 16, "ridge2": 6, "phase": 4.4, "trees": [-56, -34, 28, 54], "stones": True},
]:
    build(item)
