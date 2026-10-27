import bpy
from mathutils import Vector

obj = bpy.data.objects.get("tall-grass-hex-v01")
if obj is None:
    raise RuntimeError("Expected supplied tall-grass-v01 model")

center = obj.location.copy()
bpy.ops.mesh.primitive_plane_add(size=4.0, location=(center.x, center.y, -0.03))
plane = bpy.context.object
plane.name = "Neutral preview ground"
mat = bpy.data.materials.new("Muted green preview surface")
mat.diffuse_color = (0.12, 0.18, 0.07, 1.0)
mat.use_nodes = True
mat.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value = (0.12, 0.18, 0.07, 1.0)
mat.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = 0.9
plane.data.materials.append(mat)

light_data = bpy.data.lights.new("Large softbox", "AREA")
light = bpy.data.objects.new("Large softbox", light_data)
bpy.context.collection.objects.link(light)
light.location = (center.x - 2.3, center.y - 1.5, 5.5)
light_data.energy = 850
light_data.size = 4.0

camera_data = bpy.data.cameras.new("Preview camera")
camera = bpy.data.objects.new("Preview camera", camera_data)
bpy.context.collection.objects.link(camera)
camera.location = (center.x + 0.1, center.y - 3.2, center.z + 2.5)
direction = center - camera.location
camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
camera_data.type = "ORTHO"
camera_data.ortho_scale = 3.5
bpy.context.scene.camera = camera

scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.samples = 24
scene.render.resolution_x = 1024
scene.render.resolution_y = 1024
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
scene.render.film_transparent = False
scene.render.filepath = r"C:\EmberEngine\work\plains-grass-tile-proposal-20261026\source-grass-clump-inspection.png"
scene.view_settings.view_transform = "AgX"
bpy.ops.render.render(write_still=True)
