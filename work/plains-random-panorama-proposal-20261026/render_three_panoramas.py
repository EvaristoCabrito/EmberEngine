import bpy
import math
from pathlib import Path

ROOT = Path(r"C:\EmberEngine")
OUT = ROOT / "work/plains-random-panorama-proposal-20261026"
TEXTURE_1 = ROOT / "content/game/public/game/ground-variant-tiles/plains-meadow-natural-v01.png"
TEXTURE_2 = ROOT / "content/game/public/game/ground-variant-tiles/plains-meadow-natural-v02.png"
TEXTURE_3 = ROOT / "content/game/public/game/ground-variant-tiles/plains-meadow-scrub-rare-v01.png"
HILL_TEXTURE = ROOT / "content/game/public/game/ground-engine2-v4/hill/color.png"
WATER_TEXTURE = ROOT / "content/game/public/game/ground-engine2-v4/shallow-water/color.png"


def remove_scene_objects():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)


def textured_material(name, image_path, repeat_x, repeat_y, roughness=0.94):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    shader = nodes.get("Principled BSDF")
    shader.inputs["Roughness"].default_value = roughness
    tex = nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(str(image_path), check_existing=True)
    tex.extension = "REPEAT"
    uv = nodes.new("ShaderNodeTexCoord")
    mapping = nodes.new("ShaderNodeMapping")
    mapping.inputs["Scale"].default_value = (repeat_x, repeat_y, 1)
    links.new(uv.outputs["UV"], mapping.inputs["Vector"])
    links.new(mapping.outputs["Vector"], tex.inputs["Vector"])
    links.new(tex.outputs["Color"], shader.inputs["Base Color"])
    return mat


def make_grid(name, xmin, xmax, ymin, ymax, nx, ny, z_fn, mat):
    vertices, faces, uvs = [], [], []
    for j in range(ny + 1):
        v = j / ny
        y = ymin + (ymax - ymin) * v
        for i in range(nx + 1):
            u = i / nx
            x = xmin + (xmax - xmin) * u
            vertices.append((x, y, z_fn(x, y, u, v)))
            uvs.append((u, v))
    stride = nx + 1
    for j in range(ny):
        for i in range(nx):
            a = j * stride + i
            faces.append((a, a + 1, a + stride + 1, a + stride))
    mesh = bpy.data.meshes.new(name + " mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(mat)
    uv_layer = mesh.uv_layers.new(name="UVMap")
    for poly in mesh.polygons:
        for loop_index in poly.loop_indices:
            uv_layer.data[loop_index].uv = uvs[mesh.loops[loop_index].vertex_index]
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def ridge(name, y0, y1, peak, phase, mat, shape=1.0):
    def z_fn(x, y, _u, v):
        base = max(0.0, math.sin(math.pi * v)) ** 0.8
        broad = 0.72 + 0.18 * math.sin(x * 0.012 + phase) + 0.10 * math.sin(x * 0.027 + phase * 1.7)
        shoulders = 1.0 + 0.16 * math.sin(v * math.pi * 2 + phase)
        return max(0.0, peak * base * broad * shoulders * shape)
    return make_grid(name, -260, 260, y0, y1, 160, 40, z_fn, mat)


def add_river(mat):
    def z_fn(x, y, _u, _v):
        return 0.018 + 0.002 * math.sin(x * 0.07)
    return make_grid("Shallow river across the ford", -130, 130, 28, 45, 100, 8, z_fn, mat)


def scene(config):
    remove_scene_objects()
    ground = textured_material("Prairie ground texture", config["ground"], 42, 48)
    hills = textured_material("Distant hill surface", HILL_TEXTURE, 8, 8)
    water = textured_material("Ford water", WATER_TEXTURE, 4, 1.5)
    make_grid("Wide grassland", -260, 260, -15, 270, 120, 100, lambda _x, _y, _u, _v: 0.0, ground)
    ridge("Near rolling ridge", 68, 165, config["ridge1"], config["phase"], hills)
    ridge("Far rolling ridge", 150, 260, config["ridge2"], config["phase"] + 1.2, hills, 0.72)
    if config.get("river"):
        add_river(water)

    if config.get("stones"):
        stone = bpy.data.materials.new("Weathered fieldstone")
        stone.diffuse_color = (0.22, 0.25, 0.21, 1)
        stone.use_nodes = True
        stone.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value = (0.22, 0.25, 0.21, 1)
        stone.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = 0.96
        for x, y, scale in [(-18, 24, 1.3), (23, 36, 0.9), (2, 48, 0.7)]:
            bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=1, location=(x, y, 0.65 * scale))
            rock = bpy.context.object
            rock.name = "Weathered stone in prairie"
            rock.scale = (0.8 * scale, 0.52 * scale, 0.65 * scale)
            rock.data.materials.append(stone)

    camera_data = bpy.data.cameras.new("Long panorama camera")
    camera = bpy.data.objects.new("Long panorama camera", camera_data)
    bpy.context.collection.objects.link(camera)
    camera.location = (0, 0, 1.55)
    camera.rotation_euler = (math.radians(88), 0, 0)
    camera_data.type = "PERSP"
    camera_data.lens = 20
    bpy.context.scene.camera = camera

    light_data = bpy.data.lights.new("Broad daylight", "AREA")
    light = bpy.data.objects.new("Broad daylight", light_data)
    bpy.context.collection.objects.link(light)
    light.location = (-24, 3, 52)
    light_data.energy = 150000
    light_data.shape = "DISK"
    light_data.size = 45

    world = bpy.context.scene.world
    world.color = (0.42, 0.42, 0.42)
    world.use_nodes = True
    world.node_tree.nodes.get("Background").inputs["Color"].default_value = (0.42, 0.42, 0.42, 1)
    world.node_tree.nodes.get("Background").inputs["Strength"].default_value = 0.35

    render = bpy.context.scene.render
    render.engine = "CYCLES"
    bpy.context.scene.cycles.samples = 24
    render.resolution_x = 3072
    render.resolution_y = 1024
    render.resolution_percentage = 100
    render.image_settings.file_format = "PNG"
    render.image_settings.color_mode = "RGBA"
    render.film_transparent = True
    bpy.context.scene.view_settings.view_transform = "AgX"
    render.filepath = str(OUT / config["file"])
    bpy.ops.render.render(write_still=True)


for config in [
    {"file": "plains-panorama-01-preview.png", "ground": TEXTURE_1, "ridge1": 18, "ridge2": 10, "phase": 0.3},
    {"file": "plains-panorama-02-preview.png", "ground": TEXTURE_2, "ridge1": 15, "ridge2": 13, "phase": 2.2, "river": True},
    {"file": "plains-panorama-03-preview.png", "ground": TEXTURE_3, "ridge1": 21, "ridge2": 8, "phase": 4.4, "stones": True},
]:
    scene(config)
