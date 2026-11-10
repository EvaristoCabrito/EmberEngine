"""Stake barricade (cheval de frise), one hex wide: bark logs, sharpened stakes, hemp lashings.

Headless: blender.exe -b -P tools/scripts/models/create_stake_barricade_v01.py
1 unit = one hex radius (a human is 2.65 units); the row spacing of the hexes is sqrt(3) units, so three of these
side by side close a three-hex barricade wall. Textures: the Poly Haven bark and ambientCG wood already in
content/textures, the hemp rope in content/models/textures.
"""
from pathlib import Path
import math
import random
import bmesh
import bpy
from mathutils import Matrix, Vector, noise

ROOT = Path(__file__).resolve().parents[3]
NAME = "stake-barricade-v01"
OUT = ROOT / "content" / "models" / "defenses"
PREVIEW = ROOT / "work" / f"{NAME}-preview.png"
TEX = ROOT / "content" / "textures"
BARK = TEX / "polyhaven" / "bark_brown_02"
WOOD = TEX / "ambientcg" / "wood-060"
ROPE = ROOT / "content" / "models" / "textures"
TEXTURE_SIZE = 1024
SMALL = ROOT / "build" / "models-tmp" / NAME
SMALL.mkdir(parents=True, exist_ok=True)

for target in (OUT / f"{NAME}.blend", OUT / f"{NAME}.glb"):
    if target.exists():
        raise FileExistsError(f"Refusing to overwrite existing file: {target}")
OUT.mkdir(parents=True, exist_ok=True)
bpy.context.preferences.filepaths.save_version = 0
rng = random.Random(20261110)


def reset_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for block in (bpy.data.cameras, bpy.data.lights):
        for item in list(block):
            block.remove(item)


def image(path, colour):
    small = SMALL / (Path(path).stem + f"-{TEXTURE_SIZE}.jpg")
    if not small.exists():
        src = bpy.data.images.load(str(path))
        src.scale(TEXTURE_SIZE, TEXTURE_SIZE)
        src.filepath_raw = str(small)
        src.file_format = "JPEG"
        src.save()
        bpy.data.images.remove(src)
    img = bpy.data.images.load(str(small), check_existing=True)
    img.colorspace_settings.name = "sRGB" if colour else "Non-Color"
    return img


def material(name, colour, normal, rough=None, arm=None, tint=(1, 1, 1), normal_strength=1.0):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    shader = nodes.get("Principled BSDF")
    shader.inputs["Metallic"].default_value = 0.0
    tex = nodes.new("ShaderNodeTexImage")
    tex.image = image(colour, True)
    if tint != (1, 1, 1):
        mix = nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.blend_type = "MULTIPLY"
        mix.inputs["Factor"].default_value = 1.0
        mix.inputs["B"].default_value = (*tint, 1.0)
        links.new(tex.outputs["Color"], mix.inputs["A"])
        links.new(mix.outputs["Result"], shader.inputs["Base Color"])
    else:
        links.new(tex.outputs["Color"], shader.inputs["Base Color"])
    ntex = nodes.new("ShaderNodeTexImage")
    ntex.image = image(normal, False)
    nmap = nodes.new("ShaderNodeNormalMap")
    nmap.inputs["Strength"].default_value = normal_strength
    links.new(ntex.outputs["Color"], nmap.inputs["Color"])
    links.new(nmap.outputs["Normal"], shader.inputs["Normal"])
    if arm:
        atex = nodes.new("ShaderNodeTexImage")
        atex.image = image(arm, False)
        sep = nodes.new("ShaderNodeSeparateColor")
        links.new(atex.outputs["Color"], sep.inputs["Color"])
        links.new(sep.outputs["Green"], shader.inputs["Roughness"])
    elif rough:
        rtex = nodes.new("ShaderNodeTexImage")
        rtex.image = image(rough, False)
        links.new(rtex.outputs["Color"], shader.inputs["Roughness"])
    return mat


def log(bm, uv, start, end, radius, sharp_end=False, sharp_start=False, sides=12, seed=0.0, taper=0.9):
    """A slightly crooked bark log from start to end; a sharpened end is pale carved wood (material 1)."""
    start, end = Vector(start), Vector(end)
    axis = end - start
    length = axis.length
    z = axis.normalized()
    x = z.orthogonal().normalized()
    y = z.cross(x)
    tip = radius * (3.0 + 1.6 * abs(noise.noise(Vector((seed, 1.0, 4.0)))))
    stations = [0.0]
    body_from = tip if sharp_start else 0.0
    body_to = length - tip if sharp_end else length
    if sharp_start:
        stations.append(body_from)
    steps = max(3, int((body_to - body_from) / 0.3))
    for i in range(1, steps + 1):
        stations.append(body_from + (body_to - body_from) * i / steps)
    if sharp_end:
        stations.append(length)
    rings = []
    for t in stations:
        pointed = (sharp_start and t == 0.0) or (sharp_end and t == length)
        r = radius * (1.0 - (1.0 - taper) * t / length)
        bend = Vector((noise.noise(Vector((seed, t * 0.9, 0.0))), noise.noise(Vector((seed + 7.3, t * 0.9, 1.0))), 0.0)) * radius * 0.55
        centre = start + z * t + x * bend.x + y * bend.y
        if pointed:
            centre += (x * noise.noise(Vector((seed, 3.1, 9.0))) + y * noise.noise(Vector((seed, 5.7, 2.0)))) * radius * 0.9
        ring = []
        for s in range(sides):
            a = 2 * math.pi * s / sides
            wobble = 1.0 + 0.11 * noise.noise(Vector((seed + math.cos(a) * 1.7, seed + math.sin(a) * 1.7, t * 2.3)))
            rr = radius * 0.10 if pointed else r * wobble
            ring.append(bm.verts.new(centre + (x * math.cos(a) + y * math.sin(a)) * rr))
        rings.append((t, ring, pointed))
    girth = 2 * math.pi * radius
    for i in range(len(rings) - 1):
        (t0, r0, p0), (t1, r1, p1) = rings[i], rings[i + 1]
        carved = p0 or p1
        for s in range(sides):
            n = (s + 1) % sides
            face = bm.faces.new((r0[s], r0[n], r1[n], r1[s]))
            face.material_index = 1 if carved else 0
            face.smooth = not carved
            u0, u1 = s / sides, (s + 1) / sides
            scale = 0.55 if carved else 0.42
            coords = ((u0 * girth * scale * 2.2, t0 * scale), (u1 * girth * scale * 2.2, t0 * scale),
                      (u1 * girth * scale * 2.2, t1 * scale), (u0 * girth * scale * 2.2, t1 * scale))
            for loop, co in zip(face.loops, coords):
                loop[uv].uv = (co[0] + seed * 0.37, co[1] + seed * 0.61)
    for t, ring, pointed in (rings[0], rings[-1]):
        order = ring if t != 0.0 else list(reversed(ring))
        cap = bm.faces.new(order)
        cap.material_index = 1
        cap.smooth = False
        centre = sum((v.co for v in ring), Vector()) / sides
        for loop in cap.loops:
            d = loop.vert.co - centre
            loop[uv].uv = (0.5 + d.dot(x) / (radius * 3.0), 0.5 + d.dot(y) / (radius * 3.0))


def lashing(bm, uv, centre, axis, radius, turns=6, thickness=0.013):
    """Hemp rope wound a few turns round a joint (material 2)."""
    axis = Vector(axis).normalized()
    x = axis.orthogonal().normalized()
    y = axis.cross(x)
    segments, sides = 14 * turns, 6
    pitch = thickness * 1.9
    rings = []
    for i in range(segments + 1):
        a = 2 * math.pi * i / 14
        along = (i / 14 - turns / 2) * pitch
        r = radius + thickness * 0.7 + 0.002 * math.sin(i * 1.7)
        p = Vector(centre) + axis * along + (x * math.cos(a) + y * math.sin(a)) * r
        tangent = (-x * math.sin(a) + y * math.cos(a)).normalized()
        out = (x * math.cos(a) + y * math.sin(a)).normalized()
        side = tangent.cross(out)
        rings.append([bm.verts.new(p + (out * math.cos(b) + side * math.sin(b)) * thickness)
                      for b in (2 * math.pi * k / sides for k in range(sides))])
    for i in range(segments):
        for k in range(sides):
            n = (k + 1) % sides
            face = bm.faces.new((rings[i][k], rings[i][n], rings[i + 1][n], rings[i + 1][k]))
            face.material_index = 2
            face.smooth = True
            v0, v1 = i * 0.22, (i + 1) * 0.22
            for loop, co in zip(face.loops, ((k / sides, v0), ((k + 1) / sides, v0), ((k + 1) / sides, v1), (k / sides, v1))):
                loop[uv].uv = co
    for ring in (list(reversed(rings[0])), rings[-1]):
        bm.faces.new(ring).material_index = 2


def build():
    bark = material("Barricade bark | Poly Haven bark_brown_02", BARK / "bark_brown_02_diff_2k.jpg",
                    BARK / "bark_brown_02_nor_gl_2k.jpg", arm=BARK / "bark_brown_02_arm_2k.jpg", tint=(0.78, 0.74, 0.70))
    carved = material("Barricade carved wood | ambientCG Wood060", WOOD / "wood-060_color_2k.jpg",
                      WOOD / "wood-060_normalgl_2k.jpg", rough=WOOD / "wood-060_roughness_2k.jpg", tint=(0.60, 0.56, 0.50))
    rope = material("Barricade hemp lashing", ROPE / "winch-rope-hemp-color-v01.png",
                    ROPE / "winch-rope-hemp-normal-v01.png", rough=ROPE / "winch-rope-hemp-roughness-v01.png",
                    tint=(0.50, 0.44, 0.35))
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")
    half = math.sqrt(3) / 2 + 0.03          # just past the hex's half width, so neighbours overlap
    rail_z, rail_r = 1.02, 0.105

    # The spine: one heavy log along the row, a lighter one low on the enemy side.
    log(bm, uv, (-half, 0.02, rail_z), (half, -0.02, rail_z + 0.03), rail_r, seed=1.0, taper=0.93)
    log(bm, uv, (-half + 0.05, 0.47, 0.36), (half - 0.04, 0.50, 0.33), 0.07, seed=2.0, taper=0.95)

    # Two trestles of crossed logs carry the spine; their tops are sharpened too.
    for i, tx in enumerate((-0.62, 0.60)):
        lean = 0.62 + 0.04 * i
        log(bm, uv, (tx - 0.09, -lean, 0.0), (tx - 0.09, lean * 0.80, 1.80), 0.088, sharp_end=True, seed=3.0 + i)
        log(bm, uv, (tx + 0.09, lean, 0.0), (tx + 0.09, -lean * 0.78, 1.74), 0.085, sharp_end=True, seed=5.0 + i)
        lashing(bm, uv, (tx, 0.0, rail_z + 0.02), (1, 0, 0), rail_r + 0.012, turns=8)

    # The stakes: planted behind, leaning over the spine at the enemy, alternating in height and angle.
    xs = (-0.80, -0.40, -0.21, 0.0, 0.20, 0.39, 0.81)
    for i, sx in enumerate(xs):
        jitter = rng.uniform(-0.035, 0.035)
        reach = rng.uniform(0.42, 0.72)
        back = rng.uniform(0.78, 0.96)
        r = rng.uniform(0.058, 0.072)
        # the stake rests on top of the spine: its line from the ground behind passes over (y=0, z=rest)
        rest = rail_z + rail_r + r * 0.55
        y0, y1 = -back, reach
        z1 = rest * (y1 - y0) / (0.0 - y0)
        log(bm, uv, (sx + jitter, y0, 0.0), (sx - jitter, y1, z1), r, sharp_end=True, seed=10.0 + i)
        if i % 2 == 0:
            direction = Vector((-2 * jitter, y1 - y0, z1)).normalized()
            lashing(bm, uv, (sx, 0.0, rest), direction, r, turns=5)

    # Two short stakes pointing the other way, so it reads as a barricade from behind as well.
    for i, sx in enumerate((-0.30, 0.31)):
        log(bm, uv, (sx, 0.40, 0.0), (sx + 0.03, -0.70, 1.42 + 0.08 * i), 0.06, sharp_end=True, seed=20.0 + i)

    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bm.normal_update()
    mesh = bpy.data.meshes.new(NAME)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(NAME, mesh)
    bpy.context.collection.objects.link(obj)
    for mat in (bark, carved, rope):
        mesh.materials.append(mat)
    obj["Asset"] = "Stake barricade, one hex"
    return obj


def point(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()


def render(obj):
    copies = []
    for k in (-1, 1):
        twin = obj.copy()
        twin.location.x = k * math.sqrt(3)
        bpy.context.collection.objects.link(twin)
        copies.append(twin)
    bpy.ops.mesh.primitive_plane_add(size=60, location=(0, 0, 0))
    floor = bpy.context.object
    fmat = bpy.data.materials.new("Preview ground")
    fmat.diffuse_color = (0.20, 0.19, 0.17, 1)
    floor.data.materials.append(fmat)
    world = bpy.context.scene.world or bpy.data.worlds.new("Preview world")
    bpy.context.scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.5, 0.5, 0.52, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.5
    bpy.ops.object.camera_add(location=(5.2, 6.4, 5.6))
    camera = bpy.context.object
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 7.0
    point(camera, (0, 0, 0.8))
    bpy.context.scene.camera = camera
    for location, power in (((4, 5, 7), 1400), ((-5, 3, 4), 500), ((0, -5, 3), 350)):
        bpy.ops.object.light_add(type="AREA", location=location)
        light = bpy.context.object
        light.data.energy = power
        light.data.size = 4.0
        point(light, (0, 0, 0.8))
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 48
    scene.render.resolution_x, scene.render.resolution_y = 1600, 900
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(PREVIEW)
    bpy.ops.render.render(write_still=True)
    for extra in copies + [floor]:
        bpy.data.objects.remove(extra)


reset_scene()
barricade = build()
bpy.ops.file.pack_all()
bpy.ops.object.select_all(action="DESELECT")
barricade.select_set(True)
bpy.context.view_layer.objects.active = barricade
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / f"{NAME}.blend"))
bpy.ops.export_scene.gltf(
    filepath=str(OUT / f"{NAME}.glb"), export_format="GLB", use_selection=True, export_apply=True,
    export_texcoords=True, export_normals=True, export_tangents=True, export_materials="EXPORT",
    export_image_format="JPEG", export_yup=True,
)
bounds = [barricade.matrix_world @ Vector(c) for c in barricade.bound_box]
size = [max(b[i] for b in bounds) - min(b[i] for b in bounds) for i in range(3)]
print(f"BARRICADE size x={size[0]:.3f} y={size[1]:.3f} z={size[2]:.3f} triangles={sum(len(p.vertices) - 2 for p in barricade.data.polygons)}")
render(barricade)
print("BARRICADE done", OUT / f"{NAME}.glb", PREVIEW)
