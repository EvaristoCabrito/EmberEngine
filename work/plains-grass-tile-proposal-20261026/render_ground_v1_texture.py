import bpy
import math
import random

OUT = r"C:\EmberEngine\work\plains-grass-tile-proposal-20261026\plains-grass-ground-v1-textured-preview.png"
SOURCE = r"C:\EmberEngine\content\assets\game\ground-engine2-v4\plains\color.png"
BLEND = r"C:\EmberEngine\work\plains-grass-tile-proposal-20261026\plains-grass-ground-v1-textured-preview.blend"
random.seed(20261026)

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)

mat = bpy.data.materials.new("Existing game plains texture reference")
mat.use_nodes = True
nodes, links = mat.node_tree.nodes, mat.node_tree.links
bsdf = nodes.get("Principled BSDF")
image = bpy.data.images.load(SOURCE, check_existing=True)
image_node = nodes.new("ShaderNodeTexImage")
image_node.image = image
image_node.interpolation = "Linear"
links.new(image_node.outputs["Color"], bsdf.inputs["Base Color"])
bsdf.inputs["Roughness"].default_value = 0.94

bpy.ops.mesh.primitive_plane_add(size=1, location=(0.5, 0.5, 0))
ground = bpy.context.object
ground.name = "New meadow ground tile proposal"
ground.data.materials.append(mat)

palette = [
    (0.045, 0.19, 0.035, 1),
    (0.075, 0.25, 0.045, 1),
    (0.11, 0.29, 0.05, 1),
    (0.07, 0.22, 0.055, 1),
]
leaf_mats = []
for i, color in enumerate(palette):
    leaf = bpy.data.materials.new(f"Wild grass leaf {i + 1}")
    leaf.diffuse_color = color
    leaf.use_nodes = True
    leaf.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value = color
    leaf.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = 0.9
    leaf_mats.append(leaf)

vertices, faces, face_mats = [], [], []
cluster_count = 520
for cluster in range(cluster_count):
    root_x, root_y = random.random(), random.random()
    cluster_angle = random.random() * math.tau
    cluster_material = random.randrange(len(leaf_mats))
    for _ in range(random.randint(4, 7)):
        angle = cluster_angle + random.uniform(-0.85, 0.85)
        length = random.uniform(0.10, 0.22)
        width = random.uniform(0.0026, 0.0055)
        lean = random.uniform(0.55, 0.88)
        # Low, naturally bent blades leave the underlying textured earth visible.
        offset_x = math.cos(angle) * length * lean
        offset_y = math.sin(angle) * length * lean
        z_height = random.uniform(0.018, 0.052)
        material_index = cluster_material if random.random() < 0.72 else random.randrange(len(leaf_mats))
        wrap_x = [0, -1, 1] if root_x < 0.23 or root_x > 0.77 else [0]
        wrap_y = [0, -1, 1] if root_y < 0.23 or root_y > 0.77 else [0]
        for tile_x in wrap_x:
            for tile_y in wrap_y:
                base = len(vertices)
                for i in range(5):
                    t = i / 4
                    cx = root_x + tile_x + offset_x * t
                    cy = root_y + tile_y + offset_y * t
                    half_width = width * (1 - t) ** 0.85
                    side_x = -math.sin(angle) * half_width
                    side_y = math.cos(angle) * half_width
                    z = 0.004 + z_height * t
                    vertices.extend([
                        (cx + side_x, cy + side_y, z),
                        (cx - side_x, cy - side_y, z),
                    ])
                for i in range(4):
                    a, b = base + 2 * i, base + 2 * (i + 1)
                    faces.append((a, b, a + 1))
                    faces.append((a + 1, b, b + 1))
                    face_mats.extend([material_index, material_index])

mesh = bpy.data.meshes.new("Low prairie grass clusters")
mesh.from_pydata(vertices, [], faces)
for leaf in leaf_mats:
    mesh.materials.append(leaf)
mesh.update()
grass = bpy.data.objects.new("Patchy short prairie grass", mesh)
bpy.context.collection.objects.link(grass)
for polygon, material_index in zip(mesh.polygons, face_mats):
    polygon.material_index = material_index

light_data = bpy.data.lights.new("Soft natural daylight", "AREA")
light = bpy.data.objects.new("Soft natural daylight", light_data)
bpy.context.collection.objects.link(light)
light.location = (0.32, 0.66, 1.8)
light_data.energy = 100
light_data.size = 1.2

camera_data = bpy.data.cameras.new("Tile camera")
camera = bpy.data.objects.new("Tile camera", camera_data)
bpy.context.collection.objects.link(camera)
camera.location = (0.5, 0.5, 2.4)
camera.rotation_euler = (0, 0, 0)
camera_data.type = "ORTHO"
camera_data.ortho_scale = 1.0
bpy.context.scene.camera = camera

scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.samples = 32
scene.render.resolution_x = 1024
scene.render.resolution_y = 1024
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
scene.render.film_transparent = False
scene.view_settings.view_transform = "AgX"
scene.render.filepath = OUT
bpy.ops.wm.save_as_mainfile(filepath=BLEND)
bpy.ops.render.render(write_still=True)
