"""Create two simple, photoreal tavern dining tables from the approved reference."""
from pathlib import Path
import math
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "content" / "models" / "tavern"
TEXTURE = ROOT / "content" / "models" / "textures" / "wt-dark-oak.jpg"
PREVIEW = OUT / "table-set-v01-preview.png"

ASSETS = (
    {"name": "table-small-v01", "length": 1.55, "width": 1.02, "height": 0.76, "planks": 4, "leg": 0.12},
    {"name": "table-messhall-v01", "length": 3.35, "width": 1.12, "height": 0.78, "planks": 5, "leg": 0.14},
)

for asset in ASSETS:
    for suffix in (".blend", ".glb"):
        target = OUT / (asset["name"] + suffix)
        if target.exists():
            raise FileExistsError(f"Refusing to overwrite existing user file: {target}")
if PREVIEW.exists():
    raise FileExistsError(f"Refusing to overwrite existing user file: {PREVIEW}")
if not TEXTURE.exists():
    raise FileNotFoundError(TEXTURE)

OUT.mkdir(parents=True, exist_ok=True)
bpy.context.preferences.filepaths.save_version = 0


def reset_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for camera in list(bpy.data.cameras):
        bpy.data.cameras.remove(camera)
    for light in list(bpy.data.lights):
        bpy.data.lights.remove(light)


def wood_material(name, tint):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    shader = nodes.get("Principled BSDF")
    shader.inputs["Roughness"].default_value = 0.82
    shader.inputs["Metallic"].default_value = 0.0
    image = bpy.data.images.load(str(TEXTURE), check_existing=True)
    image_node = nodes.new("ShaderNodeTexImage")
    image_node.image = image
    image_node.label = "Existing EmberEngine dark oak texture"
    multiply = nodes.new("ShaderNodeMixRGB")
    multiply.blend_type = "MULTIPLY"
    multiply.inputs[0].default_value = 0.78
    multiply.inputs[2].default_value = (*tint, 1.0)
    links.new(image_node.outputs["Color"], multiply.inputs[1])
    links.new(multiply.outputs["Color"], shader.inputs["Base Color"])
    return material


WOOD = [
    wood_material("Dark oak | warm brown", (0.82, 0.66, 0.48)),
    wood_material("Dark oak | deep brown", (0.72, 0.55, 0.39)),
    wood_material("Dark oak | soft variation", (0.90, 0.75, 0.57)),
]


def box(name, location, dimensions, material, bevel):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(material)
    if bevel:
        mod = obj.modifiers.new("Small worn edge bevel", "BEVEL")
        mod.width = bevel
        mod.segments = 2
        mod.profile = 0.5
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=mod.name)
        obj.modifiers.new("Weighted corner normals", "WEIGHTED_NORMAL")
        bpy.ops.object.modifier_apply(modifier="Weighted corner normals")
    return obj


def build_table(spec):
    length, width, height, count, leg = (
        spec["length"], spec["width"], spec["height"], spec["planks"], spec["leg"]
    )
    objects = []
    top_thickness = 0.085
    gap = 0.006
    plank_width = (width - gap * (count - 1)) / count
    top_bottom = height - top_thickness
    top_center = height - top_thickness / 2

    # Broad, continuous oak boards run along the length of the tabletop.
    for index in range(count):
        y = -width / 2 + plank_width / 2 + index * (plank_width + gap)
        objects.append(box(
            f"Top plank {index + 1:02d}",
            (0.0, y, top_center + (0.001 if index % 2 else 0.0)),
            (length, plank_width, top_thickness), WOOD[index % len(WOOD)], 0.007,
        ))

    apron_height = 0.105
    apron_thickness = 0.075
    apron_z = top_bottom - apron_height / 2 + 0.008
    # Plain apron rails sit just below the top, between the four square legs.
    for side in (-1, 1):
        objects.append(box("Long apron rail", (0, side * (width / 2 - leg * 0.52), apron_z),
                           (length - leg * 1.55, apron_thickness, apron_height), WOOD[1], 0.009))
        objects.append(box("Low long stretcher", (0, side * (width / 2 - leg * 0.56), 0.225),
                           (length - leg * 1.5, 0.072, 0.095), WOOD[1], 0.009))
    for end in (-1, 1):
        objects.append(box("End apron rail", (end * (length / 2 - leg * 0.52), 0, apron_z),
                           (apron_thickness, width - leg * 1.55, apron_height), WOOD[1], 0.009))
        objects.append(box("Low end stretcher", (end * (length / 2 - leg * 0.56), 0, 0.225),
                           (0.072, width - leg * 1.5, 0.095), WOOD[1], 0.009))

    leg_height = top_bottom - 0.012
    for x_side in (-1, 1):
        for y_side in (-1, 1):
            objects.append(box("Square oak leg",
                               (x_side * (length / 2 - leg / 2), y_side * (width / 2 - leg / 2), leg_height / 2),
                               (leg, leg, leg_height), WOOD[0], 0.009))

    # Join the simple boards into a single game-ready mesh while keeping
    # separate material slots for subtle natural plank-to-plank variation.
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    table = bpy.context.object
    table.name = spec["name"]

    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(island_margin=0.006)
    bpy.ops.object.mode_set(mode="OBJECT")
    bpy.context.scene.cursor.location = (0.0, 0.0, 0.0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    bpy.context.scene.unit_settings.system = "METRIC"
    table["Asset"] = "Tavern dining table"
    table["Variant"] = "Small four-person table" if "small" in spec["name"] else "Long mess-hall table"
    table["Real world height (m)"] = height
    return table


def save_asset(spec):
    reset_scene()
    table = build_table(spec)
    bpy.ops.file.pack_all()
    blend_path = OUT / (spec["name"] + ".blend")
    glb_path = OUT / (spec["name"] + ".glb")
    bpy.ops.object.select_all(action="DESELECT")
    table.select_set(True)
    bpy.context.view_layer.objects.active = table
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))
    bpy.ops.export_scene.gltf(
        filepath=str(glb_path), export_format="GLB", use_selection=True,
        export_apply=True, export_texcoords=True, export_normals=True,
        export_materials="EXPORT", export_image_format="AUTO", export_yup=True,
    )
    return table


def point_camera(camera, target):
    direction = target - camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def render_pair():
    reset_scene()
    small = build_table(ASSETS[0])
    small.location.x = -1.9
    large = build_table(ASSETS[1])
    large.location.x = 2.55

    bpy.ops.mesh.primitive_plane_add(size=200, location=(0, 0, -0.012))
    floor = bpy.context.object
    floor.name = "Temporary preview floor"
    floor_mat = bpy.data.materials.new("Temporary neutral preview ground")
    floor_mat.diffuse_color = (0.24, 0.235, 0.225, 1)
    floor.data.materials.append(floor_mat)

    world = bpy.context.scene.world or bpy.data.worlds.new("Preview world")
    bpy.context.scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.42, 0.42, 0.42, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.45

    bpy.ops.object.camera_add(location=(5.7, -7.4, 4.4))
    camera = bpy.context.object
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 8.6
    point_camera(camera, Vector((0.75, 0, 0.37)))
    bpy.context.scene.camera = camera

    lights = [
        ((-3.0, -4.0, 6.0), 900, 5.0),
        ((4.0, 1.0, 5.0), 700, 4.0),
        ((0.0, 4.0, 3.0), 450, 3.0),
    ]
    for location, power, size in lights:
        bpy.ops.object.light_add(type="AREA", location=location)
        light = bpy.context.object
        light.data.energy = power
        light.data.shape = "DISK"
        light.data.size = size
        point_camera(light, Vector((0.75, 0, 0.35)))

    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 32
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 900
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(PREVIEW)
    scene.view_settings.view_transform = "AgX"
    scene.render.film_transparent = False
    bpy.ops.render.render(write_still=True)


for asset_spec in ASSETS:
    save_asset(asset_spec)
render_pair()
print("Created tavern tables and preview:")
for asset_spec in ASSETS:
    print(OUT / (asset_spec["name"] + ".blend"))
    print(OUT / (asset_spec["name"] + ".glb"))
print(PREVIEW)
