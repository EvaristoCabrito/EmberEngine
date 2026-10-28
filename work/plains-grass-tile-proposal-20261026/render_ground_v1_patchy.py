import bpy
import math
import random

OUT = r"C:\EmberEngine\work\plains-grass-tile-proposal-20261026\plains-grass-ground-v1-patchy-preview.png"
SOURCE = r"C:\EmberEngine\content\assets\game\ground-engine2-v4\plains\color.png"
random.seed(2601026)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)

ground_mat = bpy.data.materials.new("Terrain texture base")
ground_mat.use_nodes = True
bsdf = ground_mat.node_tree.nodes.get("Principled BSDF")
tex = ground_mat.node_tree.nodes.new("ShaderNodeTexImage")
tex.image = bpy.data.images.load(SOURCE, check_existing=True)
tex.interpolation = "Linear"
ground_mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
bsdf.inputs["Roughness"].default_value = 0.94
bpy.ops.mesh.primitive_plane_add(size=1, location=(0.5, 0.5, 0))
plane = bpy.context.object
plane.name = "Patchy prairie tile preview"
plane.data.materials.append(ground_mat)

palette = [(0.045, 0.14, 0.022, 1), (0.07, 0.19, 0.028, 1), (0.09, 0.21, 0.034, 1)]
materials = []
for i, color in enumerate(palette):
    m = bpy.data.materials.new(f"Low meadow green {i + 1}")
    m.use_nodes = True
    m.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value = color
    m.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = 0.92
    materials.append(m)

verts, faces, face_slots = [], [], []
for patch in range(145):
    root_x, root_y = random.random(), random.random()
    direction = random.random() * math.tau
    patch_slot = random.randrange(len(materials))
    for _ in range(random.randint(2, 4)):
        angle = direction + random.uniform(-0.55, 0.55)
        length = random.uniform(0.065, 0.13)
        width = random.uniform(0.0014, 0.0027)
        bend = random.uniform(0.65, 0.95)
        dx, dy = math.cos(angle) * length * bend, math.sin(angle) * length * bend
        height = random.uniform(0.009, 0.024)
        slot = patch_slot if random.random() < 0.82 else random.randrange(len(materials))
        xs = [0, -1, 1] if root_x < 0.15 or root_x > 0.85 else [0]
        ys = [0, -1, 1] if root_y < 0.15 or root_y > 0.85 else [0]
        for ox in xs:
            for oy in ys:
                base = len(verts)
                for step in range(4):
                    t = step / 3
                    x, y = root_x + ox + dx * t, root_y + oy + dy * t
                    half = width * (1 - t) ** 0.8
                    sx, sy = -math.sin(angle) * half, math.cos(angle) * half
                    z = 0.002 + height * t
                    verts += [(x + sx, y + sy, z), (x - sx, y - sy, z)]
                for step in range(3):
                    a, b = base + step * 2, base + (step + 1) * 2
                    faces += [(a, b, a + 1), (a + 1, b, b + 1)]
                    face_slots += [slot, slot]

mesh = bpy.data.meshes.new("Scattered short prairie blades")
mesh.from_pydata(verts, [], faces)
for m in materials:
    mesh.materials.append(m)
mesh.update()
grass = bpy.data.objects.new("Sparse irregular grass patches", mesh)
bpy.context.collection.objects.link(grass)
for poly, slot in zip(mesh.polygons, face_slots):
    poly.material_index = slot

light_data = bpy.data.lights.new("Daylight", "AREA")
light = bpy.data.objects.new("Daylight", light_data)
bpy.context.collection.objects.link(light)
light.location = (0.28, 0.68, 1.3)
light_data.energy = 60
light_data.size = 1.0
camera_data = bpy.data.cameras.new("Texture camera")
camera = bpy.data.objects.new("Texture camera", camera_data)
bpy.context.collection.objects.link(camera)
camera.location = (0.5, 0.5, 2.0)
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
bpy.ops.render.render(write_still=True)
