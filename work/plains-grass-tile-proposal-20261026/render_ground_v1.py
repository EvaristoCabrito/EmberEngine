import bpy
import math
import random
from mathutils import Vector


OUT = r"C:\EmberEngine\work\plains-grass-tile-proposal-20261026\plains-grass-ground-v1-preview.png"
BLEND = r"C:\EmberEngine\work\plains-grass-tile-proposal-20261026\plains-grass-ground-v1-preview.blend"

random.seed(14026)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)

def material(name, color, roughness=0.88):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1.0)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    return mat


soil = material("Soft meadow base", (0.055, 0.095, 0.027))
leaf_mats = [
    material("Grass deep green", (0.035, 0.16, 0.026)),
    material("Grass meadow green", (0.055, 0.24, 0.034)),
    material("Grass sunlit green", (0.105, 0.31, 0.045)),
    material("Grass olive green", (0.13, 0.25, 0.04)),
    material("Grass cool green", (0.035, 0.20, 0.058)),
]

bpy.ops.mesh.primitive_plane_add(size=1.0, location=(0.5, 0.5, -0.012))
ground = bpy.context.object
ground.name = "Meadow soil base"
ground.data.materials.append(soil)

# A dense, periodic field of narrow, tapered 3D leaf blades. Copies crossing an
# edge are wrapped to the opposite side so this square can repeat as a texture.
vertices = []
faces = []
face_materials = []
blade_count = 8200
for _ in range(blade_count):
    x = random.random()
    y = random.random()
    height = random.uniform(0.15, 0.36)
    width = random.uniform(0.006, 0.012)
    angle = random.random() * math.tau
    lean_angle = angle + random.uniform(-0.8, 0.8)
    lean = random.uniform(0.025, 0.095)
    material_index = random.randrange(len(leaf_mats))
    phases = [0, -1, 1] if x < 0.12 or x > 0.88 else [0]
    phases_y = [0, -1, 1] if y < 0.12 or y > 0.88 else [0]
    for ox in phases:
        for oy in phases_y:
            base = len(vertices)
            for segment in range(6):
                t = segment / 5
                taper = (1.0 - t) ** 0.78
                curve = t * t
                cx = x + ox + math.cos(lean_angle) * lean * curve
                cy = y + oy + math.sin(lean_angle) * lean * curve
                half = width * taper * (0.78 + 0.22 * math.sin(t * math.pi))
                side_x = -math.sin(angle) * half
                side_y = math.cos(angle) * half
                z = 0.002 + height * t
                # A narrow folded ridge gives each leaf a real, readable surface.
                vertices.extend([
                    (cx + side_x, cy + side_y, z),
                    (cx, cy, z + 0.0025 * math.sin(t * math.pi)),
                    (cx - side_x, cy - side_y, z),
                ])
            for segment in range(5):
                a = base + segment * 3
                b = a + 3
                faces.extend([(a, b, a + 1), (a + 1, b, b + 1), (a + 1, b + 1, a + 2), (a + 2, b + 1, b + 2)])
                face_materials.extend([material_index] * 4)

mesh = bpy.data.meshes.new("Tapered meadow blades")
mesh.from_pydata(vertices, [], faces)
mesh.materials.clear()
for mat in leaf_mats:
    mesh.materials.append(mat)
mesh.update()
blades = bpy.data.objects.new("Meadow grass blades", mesh)
bpy.context.collection.objects.link(blades)
for poly, mat_index in zip(mesh.polygons, face_materials):
    poly.material_index = mat_index

light_data = bpy.data.lights.new("Soft daylight", "AREA")
light = bpy.data.objects.new("Soft daylight", light_data)
bpy.context.collection.objects.link(light)
light.location = (0.35, 0.65, 2.4)
light_data.energy = 175
light_data.size = 1.4

camera_data = bpy.data.cameras.new("Tile camera")
camera = bpy.data.objects.new("Tile camera", camera_data)
bpy.context.collection.objects.link(camera)
camera.location = (0.5, 0.5, 2.7)
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
scene.world.color = (0.22, 0.22, 0.22)
scene.camera.data.lens = 50

bpy.ops.wm.save_as_mainfile(filepath=BLEND)
bpy.ops.render.render(write_still=True)
