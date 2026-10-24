"""Create one simple geometric chair to match the new tavern table set."""
from pathlib import Path
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "content" / "models" / "tavern"
TEXTURE = ROOT / "content" / "models" / "textures" / "wt-dark-oak.jpg"
NAME = "chair-v03"
PREVIEW = OUT / f"{NAME}-preview.png"
BLEND = OUT / f"{NAME}.blend"
GLB = OUT / f"{NAME}.glb"

for target in (PREVIEW, BLEND, GLB):
    if target.exists():
        raise FileExistsError(f"Refusing to overwrite existing user file: {target}")
if not TEXTURE.exists():
    raise FileNotFoundError(TEXTURE)

OUT.mkdir(parents=True, exist_ok=True)
bpy.context.preferences.filepaths.save_version = 0


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for camera in list(bpy.data.cameras):
        bpy.data.cameras.remove(camera)
    for light in list(bpy.data.lights):
        bpy.data.lights.remove(light)


def wood_material(name, tint):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
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
    return mat


WOOD = [
    wood_material("Dark oak | warm brown", (0.82, 0.66, 0.48)),
    wood_material("Dark oak | deep brown", (0.72, 0.55, 0.39)),
    wood_material("Dark oak | soft variation", (0.90, 0.75, 0.57)),
]


def box(name, location, dimensions, material, bevel=0.006):
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
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=mod.name)
        mod = obj.modifiers.new("Weighted corner normals", "WEIGHTED_NORMAL")
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


def build_chair():
    width, depth, total_height = 0.46, 0.48, 1.00
    seat_height, seat_thickness = 0.47, 0.07
    leg = 0.062
    x_positions = (-width / 2 + leg / 2, width / 2 - leg / 2)
    front_y = -depth / 2 + leg / 2
    back_y = depth / 2 - leg / 2
    objects = []

    # Plain three-board seat, with a small gap between the oak planks.
    board_count, gap = 3, 0.005
    board_depth = (depth - gap * (board_count - 1)) / board_count
    for index in range(board_count):
        y = -depth / 2 + board_depth / 2 + index * (board_depth + gap)
        objects.append(box(f"Seat plank {index + 1:02d}", (0, y, seat_height - seat_thickness / 2),
                           (width, board_depth, seat_thickness), WOOD[index % 3], 0.006))

    # Front legs stop at the seat; the rear legs continue into the backrest.
    for x in x_positions:
        objects.append(box("Front square leg", (x, front_y, (seat_height - seat_thickness) / 2),
                           (leg, leg, seat_height - seat_thickness), WOOD[0], 0.006))
        objects.append(box("Rear back post", (x, back_y, total_height / 2),
                           (leg, leg, total_height), WOOD[1], 0.006))

    # Simple aprons and low stretchers make a sturdy, unornamented tavern chair.
    apron_z = seat_height - seat_thickness - 0.055
    for side in (-1, 1):
        y = side * (depth / 2 - leg / 2)
        objects.append(box("Seat side apron", (0, y, apron_z),
                           (width - leg * 1.35, leg * 0.78, 0.095), WOOD[1], 0.006))
        objects.append(box("Low side stretcher", (0, y, 0.205),
                           (width - leg * 1.1, leg * 0.70, 0.075), WOOD[1], 0.005))
    for y in (front_y, back_y):
        objects.append(box("Seat cross apron", (0, y, apron_z),
                           (width - leg * 1.4, leg * 0.78, 0.095), WOOD[1], 0.006))
    objects.append(box("Front foot stretcher", (0, front_y + 0.01, 0.205),
                       (width - leg * 1.1, leg * 0.70, 0.075), WOOD[1], 0.005))

    # Three plain horizontal boards form a simple, comfortable back.
    back_y_center = back_y + 0.012
    for index, z in enumerate((0.66, 0.80, 0.94)):
        rail_height = 0.075 if index < 2 else 0.085
        objects.append(box(f"Backrest rail {index + 1:02d}", (0, back_y_center, z),
                           (width - leg * 1.25, 0.052, rail_height), WOOD[index % 3], 0.006))

    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    chair = bpy.context.object
    chair.name = NAME
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(island_margin=0.006)
    bpy.ops.object.mode_set(mode="OBJECT")
    bpy.context.scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    bpy.context.scene.unit_settings.system = "METRIC"
    chair["Asset"] = "Tavern dining chair"
    chair["Matches"] = "table-small-v01 and table-messhall-v01"
    chair["Real world height (m)"] = total_height
    return chair


chair = build_chair()
bpy.ops.file.pack_all()
bpy.ops.object.select_all(action="DESELECT")
chair.select_set(True)
bpy.context.view_layer.objects.active = chair
bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
bpy.ops.export_scene.gltf(
    filepath=str(GLB), export_format="GLB", use_selection=True,
    export_apply=True, export_texcoords=True, export_normals=True,
    export_materials="EXPORT", export_image_format="AUTO", export_yup=True,
)

# Temporary studio render for review; lights, camera, and floor are not saved
# into the editable model or exported game asset.
clear_scene()
chair = build_chair()
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

bpy.ops.object.camera_add(location=(1.9, -2.7, 1.85))
camera = bpy.context.object
camera.data.type = "ORTHO"
camera.data.ortho_scale = 1.85
camera.rotation_euler = (Vector((0, 0, 0.48)) - camera.location).to_track_quat("-Z", "Y").to_euler()
bpy.context.scene.camera = camera
for location, power, size in (((-2, -3, 4), 700, 3), ((3, 1, 3), 500, 2.5), ((0, 3, 2), 300, 2)):
    bpy.ops.object.light_add(type="AREA", location=location)
    light = bpy.context.object
    light.data.energy = power
    light.data.shape = "DISK"
    light.data.size = size
    light.rotation_euler = (Vector((0, 0, 0.45)) - light.location).to_track_quat("-Z", "Y").to_euler()

scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.samples = 32
scene.render.resolution_x = 1000
scene.render.resolution_y = 1000
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.filepath = str(PREVIEW)
scene.view_settings.view_transform = "AgX"
bpy.ops.render.render(write_still=True)
print(f"Created {BLEND}, {GLB}, and {PREVIEW}")
