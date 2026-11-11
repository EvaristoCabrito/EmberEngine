"""The Farmlands houses' kit v01 (2026-11-11; the user: "crie janelas e coloque objetos e iluminação, aumente o tamanho de todas as
casas", "crie novos objetos 3D, passe as texturas para outro agente criar").

  blender -b -t 2 -P tools/scripts/models/create_farmhouse_kit_v01.py -- [name ...] [--version v02]

Thirteen pieces for the smithy, the apothecary, the store, the inn and the villagers' house, each
content/models/farmhouse/<name>-v01.glb, all of them in farmhouse-kit-v01.blend, their measures in
build/models-tmp/farmhouse/manifest.json (register_farmhouse_kit_v01.py reads it). Never an overwrite.
1 unit = one hex radius (a human is 2.65 units).

The mesh builder is the Light House kit's (create_lighthouse_kit_v01.py: Build, the photograph materials): one builder, not a
copy. Every part carries a photograph: the farmhouse sets the texture session makes (content/models/textures/
farmhouse-<material>-v01-{color,normal,roughness}.jpg: wood, dark-wood, iron, linen, straw, clay, herbs) and, for a set that
has not arrived, the archive photograph that stood in for it (STAND_INS below); brass, glass, rope, stone and leather come
from the archives as in the Light House kit.
"""
from pathlib import Path
import json
import math
import random
import sys

HERE = Path(__file__).resolve()
_source = (HERE.parent / "create_lighthouse_kit_v01.py").read_text(encoding="utf-8")
_cut = _source.find("\n# --- 1 ·")
if _cut < 0:
    raise SystemExit("create_lighthouse_kit_v01.py: the builder's end marker ('# --- 1 ·') is gone")
exec(compile(_source[:_cut], str(HERE.parent / "create_lighthouse_kit_v01.py"), "exec"))  # ROOT, T, Rx.., image, material, Build, arc, rivets

KIT = "farmhouse"
_CLI_ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
VERSION = next((_CLI_ARGS[i + 1] for i, a in enumerate(_CLI_ARGS[:-1]) if a == "--version"), "v01")
OUT = ROOT / "content" / "models" / KIT
TMP = ROOT / "build" / "models-tmp" / KIT
TMP.mkdir(parents=True, exist_ok=True)
OUT.mkdir(parents=True, exist_ok=True)


def farm(name, kind):
    p = MT / f"farmhouse-{name}-v01-{kind}.jpg"
    return p if p.exists() else None


def photo(name, stand_in, tint=None, **rest):
    """A farmhouse texture set (tinted by `tint` when it is shared, as the wool and the blanket share the linen), or the archive
    photograph that stands in for it until the set arrives."""
    colour = farm(name, 'color')
    if colour:
        d = dict(colour=colour, normal=farm(name, 'normal'), rough=farm(name, 'roughness'))
        if tint:
            d['tint'] = tint
    else:
        d = dict(stand_in)
    d.update(rest)
    return d


STAND_INS = {
    'wood': dict(colour=acg('planks-037a', 'color'), normal=acg('planks-037a', 'normalgl'), rough=acg('planks-037a', 'roughness'), tint=(0.86, 0.80, 0.72)),
    'dark-wood': dict(colour=MT / 'wt-dark-oak.jpg', normal=acg('wood-060', 'normalgl'), rough_v=0.82, tint=(0.80, 0.74, 0.68)),
    'iron': dict(colour=acg('metal-024', 'color'), normal=acg('metal-024', 'normalgl'), rough=acg('metal-024', 'roughness'), tint=(0.36, 0.36, 0.38)),
    'linen': dict(colour=MT / 'fabric-detail-v01.jpg', rough_v=0.95, tint=(0.80, 0.76, 0.68)),
    'straw': dict(colour=MT / 'wt-burlap.jpg', rough_v=0.95, tint=(0.95, 0.80, 0.50)),
    'clay': dict(colour=MT / 'terracotta-photo-v01.png', rough_v=0.55, tint=(0.80, 0.70, 0.60)),
    'herbs': dict(colour=MT / 'mossy-ground.jpg', rough_v=0.95, tint=(0.80, 0.78, 0.55)),
}
MATERIALS = {
    'wood': photo('wood', STAND_INS['wood']),
    'dark-wood': photo('dark-wood', STAND_INS['dark-wood']),
    'iron': photo('iron', STAND_INS['iron'], metal=0.8),
    'linen': photo('linen', STAND_INS['linen']),
    'wool': photo('linen', dict(STAND_INS['linen'], tint=(1.0, 0.97, 0.90)), tint=(1.0, 0.98, 0.94)),
    'blanket': photo('linen', dict(STAND_INS['linen'], tint=(0.52, 0.30, 0.24)), tint=(0.62, 0.36, 0.28)),
    'straw': photo('straw', STAND_INS['straw']),
    'clay': photo('clay', STAND_INS['clay']),
    'bread': dict(STAND_INS['clay'], tint=(0.86, 0.62, 0.36)),  # a crust: the plain terracotta, never the green glaze
    'herbs': photo('herbs', STAND_INS['herbs']),
    'stone': dict(colour=MT / 'stone-sandstone.jpg', normal=acg('rock-030', 'normalgl'), rough_v=0.92, tint=(0.80, 0.78, 0.74)),
    'brass': dict(colour=MT / 'brass-detail-v01.jpg', normal=acg('metal-024', 'normalgl'), rough_v=0.42, metal=0.95, tint=(0.80, 0.60, 0.27), nstrength=0.5),
    'rope': dict(colour=MT / 'winch-rope-hemp-color-v01.png', normal=MT / 'winch-rope-hemp-normal-v01.png', rough=MT / 'winch-rope-hemp-roughness-v01.png', tint=(0.62, 0.55, 0.44)),
    'leather': dict(colour=acg('leather-030', 'color'), normal=acg('leather-030', 'normalgl'), rough=acg('leather-030', 'roughness'), tint=(0.62, 0.48, 0.36)),
    'paper': dict(colour=MT / 'paper-detail-v01.jpg', rough_v=0.9, tint=(0.92, 0.86, 0.72)),
    'wax': dict(colour=MT / 'paper-detail-v01.jpg', rough_v=0.5, tint=(1.0, 0.94, 0.78)),
    'green-glass': dict(colour=MT / 'dark-green-glass-photo-v01.png', rough_v=0.12, tint=(1.25, 1.3, 1.2)),
    'water': dict(colour=MT / 'dark-green-glass-photo-v01.png', rough_v=0.05, tint=(0.30, 0.34, 0.34)),
    'lamp-glow': dict(colour=MT / 'lamp-glass-v01.jpg', rough_v=0.12, emit=MT / 'lamp-glass-glow-v01.jpg', emit_strength=6.0),
    'flame': dict(colour=acg('lava-001', 'color'), rough_v=0.6, emit=acg('lava-001', 'color'), emit_strength=14.0),
}
STOOD_IN = sorted(k for k in STAND_INS if not farm(k, 'color'))
_made.clear()
# The farmhouse sets' own scale: wood, dark-wood and herbs repeat every 1.2 units; iron, linen, clay and straw every 0.6.
UV_SCALE = {k: 2.0 for k in ('iron', 'linen', 'blanket', 'clay', 'straw')
            if farm('linen' if k == 'blanket' else k, 'color')}


class Farm(Build):
    def lathe(self, prof, key, segs=24, xf=None, us=0.8, vs=0.8, smooth=True):
        k = UV_SCALE.get(key, 1.0)
        return super().lathe(prof, key, segs, xf, us * k, vs * k, smooth)

    def tube(self, pts, r, key, sides=8, closed=False, caps=True, us=1.0, vs=0.8, soft=None):
        k = UV_SCALE.get(key, 1.0)
        return super().tube(pts, r, key, sides, closed, caps, us * k, vs * k, soft)

    def box(self, size, key, xf=None, bevel=0.012, us=0.8):
        return super().box(size, key, xf, bevel, us * UV_SCALE.get(key, 1.0))


# --- small shared parts ------------------------------------------------------------------------------------------------------
def jar(b, x, y, z, r=0.10, h=0.26, key='clay', cover=None, segs=14):
    """A potter's jar standing at (x, y, z): belly, neck, a rolled lip; `cover` ties a cloth over the mouth."""
    b.lathe([(0, 0), (r * 0.62, 0), (r * 0.95, h * 0.22), (r, h * 0.50), (r * 0.80, h * 0.80), (r * 0.58, h * 0.90), (r * 0.66, h * 0.97),
             (r * 0.60, h), (0, h)], key, segs, T(x, y, z))
    if cover:
        b.lathe([(0, h * 1.04), (r * 0.5, h * 1.03), (r * 0.72, h * 0.97), (r * 0.76, h * 0.86), (r * 0.70, h * 0.84)], cover, segs, T(x, y, z))
        b.lathe([(r * 0.70, h * 0.88), (r * 0.74, h * 0.88), (r * 0.74, h * 0.905), (r * 0.70, h * 0.905)], 'rope', segs, T(x, y, z))


def bottle(b, x, y, z, r=0.055, h=0.30, key='green-glass'):
    b.lathe([(0, 0), (r, 0), (r, h * 0.55), (r * 0.42, h * 0.74), (r * 0.36, h * 0.95), (r * 0.46, h * 0.96), (r * 0.46, h), (0, h)], key, 12, T(x, y, z))
    b.lathe([(0, h), (r * 0.34, h), (r * 0.34, h * 1.07), (0, h * 1.07)], 'wood', 8, T(x, y, z))  # the cork


def bowl(b, x, y, z, r=0.14, h=0.07, key='clay'):
    b.lathe([(0, 0), (r * 0.45, 0), (r * 0.85, h * 0.55), (r, h), (r * 0.93, h), (r * 0.78, h * 0.55), (r * 0.40, h * 0.22), (0, h * 0.22)], key, 16, T(x, y, z))


def jug(b, x, y, z, r=0.12, h=0.36, key='clay', turn=0.0):
    b.lathe([(0, 0), (r * 0.68, 0), (r, h * 0.26), (r, h * 0.52), (r * 0.56, h * 0.78), (r * 0.48, h * 0.90), (r * 0.62, h), (r * 0.52, h),
             (r * 0.38, h * 0.90), (0, h * 0.88)], key, 16, T(x, y, z))
    m = T(x, y, z) @ Rz(turn)
    b.tube([m @ p for p in arc((r * 0.92, 0, h * 0.62), h * 0.24, -80, 95, 9, 'xz')], r * 0.11, key, sides=6)


def candle(b, x, y, z, h=0.16):
    b.lathe([(0, 0), (0.07, 0), (0.075, 0.012), (0.03, 0.022), (0, 0.022)], 'iron', 12, T(x, y, z))  # the pan
    b.lathe([(0, 0.02), (0.022, 0.02), (0.024, h), (0.012, h + 0.008), (0, h + 0.004)], 'wax', 10, T(x, y, z))
    b.lathe([(0, h + 0.006), (0.012, h + 0.02), (0.009, h + 0.045), (0, h + 0.075)], 'flame', 8, T(x, y, z))


def hammer(b, m, length=0.52, head=(0.20, 0.075, 0.075)):
    """A smith's hammer in the frame m: the handle along local X from the origin, the head at its far end."""
    b.tube([m @ Vector((0, 0, 0)), m @ Vector((length, 0, 0))], [0.017, 0.02], 'dark-wood', sides=8)
    b.box((head[1], head[0], head[2]), 'iron', m @ T(length - 0.02, 0, 0), bevel=0.008)


def tongs(b, m, length=0.70):
    """Blacksmith's tongs in the frame m: two crossed reins along local X, the rivet a quarter from the jaws."""
    for s in (-1, 1):
        b.tube([m @ Vector((0, s * 0.035, 0)), m @ Vector((length * 0.74, -s * 0.004, 0)), m @ Vector((length * 0.80, -s * 0.03, 0)),
                m @ Vector((length, -s * 0.012, 0))], [0.009, 0.011, 0.012, 0.01], 'iron', sides=6)
    b.lathe([(0, -0.016), (0.016, -0.016), (0.016, 0.016), (0, 0.016)], 'iron', 8, m @ T(length * 0.76, 0, 0))


def hoop(b, r, z, key='iron', w=0.03, out=0.012, segs=20, xf=None):
    b.lathe([(r - 0.004, z - w), (r + out, z - w), (r + out, z + w), (r - 0.004, z + w)], key, segs, xf)


# --- 1 · the anvil on its stump ------------------------------------------------------------------------------------------------
def anvil():
    b = Farm("farmhouse-anvil", 101)
    b.lathe([(0, 0), (0.40, 0), (0.42, 0.04), (0.39, 0.30), (0.37, 0.58), (0.35, 0.62), (0, 0.62)], 'dark-wood', 18, vs=1.2)
    hoop(b, 0.378, 0.47, w=0.03, segs=18)
    rivets(b, 0.392, 0.47, 6, 'iron', 0.014)
    z = 0.62
    b.box((0.52, 0.32, 0.10), 'iron', T(0, 0, z + 0.05), bevel=0.02)       # the feet
    b.box((0.26, 0.20, 0.20), 'iron', T(0, 0, z + 0.20), bevel=0.03)       # the waist
    b.box((0.60, 0.25, 0.15), 'iron', T(-0.02, 0, z + 0.375), bevel=0.012)  # the body and its face
    top = z + 0.45
    b.lathe([(0.105, 0), (0.095, 0.10), (0.06, 0.24), (0.025, 0.34), (0, 0.37)], 'iron', 14, T(0.28, 0, top - 0.105) @ Ry(90))  # the horn
    b.box((0.17, 0.22, 0.07), 'iron', T(-0.39, 0, top - 0.035), bevel=0.01)  # the heel
    b.box((0.035, 0.035, 0.02), 'iron', T(-0.40, 0.05, top + 0.002), bevel=0)  # the hardy hole's rim
    for sx in (-1, 1):  # the staples that hold it to the stump
        b.tube([(sx * 0.20, 0.13, z + 0.10), (sx * 0.30, 0.20, z + 0.02), (sx * 0.31, 0.21, z - 0.10)], 0.012, 'iron', sides=6)
        b.tube([(sx * 0.20, -0.13, z + 0.10), (sx * 0.30, -0.20, z + 0.02), (sx * 0.31, -0.21, z - 0.10)], 0.012, 'iron', sides=6)
    hammer(b, T(-0.26, -0.40, top + 0.04) @ Rz(62))
    tongs(b, T(0.47, 0.10, 0.02) @ Ry(-100) @ Rz(8), 0.86)
    # a horseshoe on the stump's top, by the foot
    b.tube([Vector((-0.24, -0.20, z + 0.014)) + Vector((0.075 * cos(a), 0.085 * sin(a), 0)) for a in [math.radians(40 + 28 * i) for i in range(11)]],
           0.013, 'iron', sides=6)
    return b.finish(), dict(name='Bigorna no cepo 3D', footprint='one', blocks=True)


# --- 2 · the quench tub ----------------------------------------------------------------------------------------------------------
def quench_tub():
    b = Farm("farmhouse-quench-tub", 102)
    b.lathe([(0, 0), (0.44, 0), (0.50, 0.30), (0.52, 0.62), (0.475, 0.62), (0.455, 0.30), (0.42, 0.08), (0, 0.08)], 'wood', 16, smooth=False, us=1.1, vs=1.0)
    hoop(b, 0.468, 0.14, segs=16)
    hoop(b, 0.511, 0.50, segs=16)
    rivets(b, 0.48, 0.14, 1, 'iron', 0.014)
    rivets(b, 0.523, 0.50, 1, 'iron', 0.014, phase=0.3)
    b.lathe([(0, 0.53), (0.25, 0.53), (0.462, 0.53)], 'water', 16)
    b.tube([(0.10, 0.12, 0.10), (0.34, 0.44, 1.00)], 0.014, 'iron', sides=6)        # a bar cooling in the water
    b.tube([(-0.05, -0.20, 0.10), (-0.22, -0.50, 0.92), (-0.25, -0.56, 0.96)], [0.012, 0.012, 0.02], 'iron', sides=6)
    tongs(b, T(-0.58, 0.10, 0.655) @ Rz(-6), 1.16)                          # the tongs laid across the rim
    # the ladle hooked on the rim
    b.tube([(0.50, -0.14, 0.66), (0.535, -0.15, 0.64), (0.54, -0.15, 0.30)], 0.009, 'iron', sides=6)
    bowl(b, 0.54, -0.15, 0.20, 0.07, 0.06, 'iron')
    return b.finish(), dict(name='Tina de têmpera 3D', footprint='one', blocks=True)


# --- 3 · the smith's tool rack ---------------------------------------------------------------------------------------------------
def tool_rack():
    b = Farm("farmhouse-tool-rack", 103)
    W, H = 0.64, 2.0
    for sx in (-1, 1):
        b.beam((sx * W, 0, 0.06), (sx * W, 0, H), 0.09, 0.09, 'dark-wood', up=(1, 0, 0))
        b.beam((sx * W, -0.30, 0.04), (sx * W, 0.26, 0.04), 0.11, 0.08, 'dark-wood')
        b.beam((sx * W, -0.24, 0.10), (sx * W, -0.02, 0.48), 0.06, 0.05, 'dark-wood')
    b.beam((-W - 0.08, 0, H + 0.04), (W + 0.08, 0, H + 0.04), 0.11, 0.08, 'dark-wood')
    for i in range(5):  # the back boards
        b.box((2 * W - 0.06, 0.03, 0.225), 'wood', T(0, 0.02, 0.92 + i * 0.235), bevel=0.006)
    b.box((2 * W - 0.06, 0.34, 0.04), 'wood', T(0, -0.12, 0.66), bevel=0.008)  # the shelf
    for sx in (-1, 1):
        b.beam((sx * (W - 0.05), -0.26, 0.64), (sx * (W - 0.05), 0.0, 0.40), 0.04, 0.04, 'dark-wood')
    y = -0.045
    pegs = [(-0.48, 1.86), (-0.24, 1.86), (0.0, 1.86), (0.25, 1.86), (0.48, 1.86), (-0.40, 1.30), (0.38, 1.36), (0.05, 1.32)]
    for px, pz in pegs:
        b.tube([(px, 0.01, pz), (px, -0.10, pz + 0.015)], 0.012, 'dark-wood', sides=6)
    hammer(b, T(-0.48, y, 1.30) @ Ry(-90) @ Rx(90), 0.56, (0.22, 0.08, 0.08))               # the sledge
    hammer(b, T(-0.24, y, 1.44) @ Ry(-90) @ Rx(90), 0.42, (0.16, 0.06, 0.06))               # the hand hammer
    tongs(b, T(0.0, y - 0.01, 1.87) @ Ry(90) @ Rx(90), 0.78)
    tongs(b, T(0.25, y - 0.01, 1.87) @ Ry(90) @ Rx(90) @ Rz(4), 0.66)
    b.box((0.045, 0.014, 0.50), 'iron', T(0.48, y, 1.60), bevel=0.004)             # the rasp
    b.lathe([(0, 0), (0.02, 0), (0.022, 0.12), (0, 0.12)], 'dark-wood', 8, T(0.48, y, 1.85 - 0.01))
    for px, pz in ((-0.40, 1.30), (0.38, 1.36), (0.05, 1.32)):                      # horseshoes on their pegs
        b.tube([Vector((px, y - 0.01, pz - 0.095)) + Vector((0.085 * cos(a), 0, 0.10 * sin(a))) for a in [math.radians(-50 + 28 * i) for i in range(11)]],
               0.014, 'iron', sides=6)
    # on the shelf: iron bars, a mallet, a pot of nails
    for i, (bx, by) in enumerate(((-0.34, -0.14), (-0.33, -0.06), (-0.30, -0.10))):
        b.box((0.46, 0.055, 0.035), 'iron', T(bx, by, 0.70 + (0.036 if i == 2 else 0)) @ Rz(4 * i - 4), bevel=0.004)
    b.lathe([(0, 0), (0.075, 0), (0.08, 0.16), (0, 0.16)], 'wood', 12, T(0.20, -0.14, 0.76) @ Ry(90) @ T(0, 0, -0.08))
    b.tube([(0.20, -0.14, 0.76), (0.22, -0.40, 0.74)], 0.016, 'dark-wood', sides=8)
    jar(b, 0.46, -0.12, 0.68, 0.075, 0.15)
    # the leather apron hung on a post
    b.box((0.04, 0.34, 0.74), 'leather', T(W + 0.065, -0.02, 1.36) @ Ry(3), bevel=0.012)
    b.tube([(W + 0.02, -0.02, 1.76), (W + 0.10, -0.02, 1.78)], 0.012, 'iron', sides=6)
    return b.finish(), dict(name='Painel de ferramentas do ferreiro 3D', footprint='one', blocks=True)


# --- 4 · the grindstone ----------------------------------------------------------------------------------------------------------
def grindstone():
    b = Farm("farmhouse-grindstone", 104)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.beam((sx * 0.46, sy * 0.30, 0), (sx * 0.34, sy * 0.21, 0.74), 0.075, 0.075, 'dark-wood')
        b.beam((sx * 0.40, -0.25, 0.30), (sx * 0.40, 0.25, 0.30), 0.05, 0.06, 'dark-wood')
    for sy in (-1, 1):
        b.beam((-0.50, sy * 0.21, 0.77), (0.50, sy * 0.21, 0.77), 0.08, 0.09, 'dark-wood')
        b.box((0.12, 0.10, 0.05), 'iron', T(0, sy * 0.21, 0.835), bevel=0.006)  # the bearing straps
    axis = T(0, 0, 0.86) @ Rx(90)
    b.lathe([(0.045, -0.065), (0.37, -0.065), (0.385, -0.05), (0.385, 0.05), (0.37, 0.065), (0.045, 0.065)], 'stone', 28, axis, us=1.0, vs=1.0)
    b.tube([(0, -0.32, 0.86), (0, 0.36, 0.86)], 0.022, 'iron')
    b.tube([(0, 0.36, 0.86), (0, 0.36, 0.66), (0, 0.50, 0.66)], 0.016, 'iron', sides=6)
    b.lathe([(0, 0), (0.028, 0), (0.032, 0.08), (0.028, 0.16), (0, 0.16)], 'dark-wood', 10, T(0, 0.42, 0.66) @ Rx(-90))
    # the water trough under the wheel
    b.box((0.62, 0.20, 0.03), 'wood', T(0, 0, 0.40), bevel=0.006)
    for sy in (-1, 1):
        b.box((0.62, 0.025, 0.13), 'wood', T(0, sy * 0.10, 0.46), bevel=0.006)
    for sx in (-1, 1):
        b.box((0.025, 0.20, 0.13), 'wood', T(sx * 0.31, 0, 0.46), bevel=0.006)
        b.beam((sx * 0.31, -0.21, 0.42), (sx * 0.31, 0.21, 0.42), 0.04, 0.04, 'dark-wood')
    b.box((0.58, 0.17, 0.01), 'water', T(0, 0, 0.50), bevel=0)
    # a knife waiting across the rails, a whetstone
    b.tube([(-0.36, -0.34, 0.827), (-0.36, 0.0, 0.827), (-0.37, 0.28, 0.827)], [0.005, 0.015, 0.017], 'iron', sides=4)
    b.tube([(-0.37, 0.28, 0.827), (-0.38, 0.44, 0.827)], 0.017, 'dark-wood', sides=8)
    b.box((0.16, 0.05, 0.03), 'stone', T(0.36, 0.21, 0.83), bevel=0.006)
    return b.finish(), dict(name='Rebolo de amolar 3D', footprint='one', blocks=True)


# --- 5 · the shop counter (two hexes) ---------------------------------------------------------------------------------------------
def shop_counter():
    b = Farm("farmhouse-shop-counter", 105)
    L, D, H = 2.90, 0.70, 1.14
    b.box((L, D, 0.08), 'dark-wood', T(0, 0, 0.04), bevel=0.01)
    n = 9
    for i in range(n):  # the front: upright boards
        w = (L - 0.10) / n
        b.box((w - 0.012, 0.035, H - 0.10), 'wood', T(-L / 2 + 0.05 + w * (i + 0.5), -D / 2 + 0.03, 0.08 + (H - 0.10) / 2), bevel=0.006)
    for z in (0.20, H - 0.08):  # the rails over them
        b.box((L - 0.04, 0.03, 0.09), 'dark-wood', T(0, -D / 2 + 0.005, z), bevel=0.006)
    for sx in (-1, 1):
        b.box((0.04, D - 0.04, H - 0.10), 'wood', T(sx * (L / 2 - 0.03), 0, 0.08 + (H - 0.10) / 2), bevel=0.006)
        b.beam((sx * (L / 2 - 0.03), -D / 2 + 0.03, 0.08), (sx * (L / 2 - 0.03), -D / 2 + 0.03, H - 0.02), 0.09, 0.09, 'dark-wood', up=(1, 0, 0))
    b.box((L - 0.12, D - 0.10, 0.035), 'wood', T(0, 0.03, 0.56), bevel=0.006)   # the shelf behind
    for i in range(3):  # the top: three long planks
        b.box((L + 0.10, (D + 0.14) / 3 - 0.008, 0.065), 'wood', T(0, -(D + 0.14) / 3 + i * (D + 0.14) / 3, H + 0.012), bevel=0.01)
    top = H + 0.045
    # the scales
    sx0 = -0.92
    b.lathe([(0, 0), (0.10, 0), (0.105, 0.02), (0.04, 0.04), (0.02, 0.06), (0, 0.06)], 'brass', 16, T(sx0, 0, top))
    b.tube([(sx0, 0, top + 0.05), (sx0, 0, top + 0.50)], 0.012, 'brass', sides=8)
    b.tube([(sx0 - 0.25, 0, top + 0.47), (sx0, 0, top + 0.50), (sx0 + 0.25, 0, top + 0.44)], 0.009, 'brass', sides=6)
    for s, drop in ((-1, 0.47), (1, 0.44)):
        px, pz = sx0 + s * 0.25, top + drop - 0.27
        b.lathe([(0, 0), (0.05, 0.004), (0.095, 0.022), (0.10, 0.035), (0.09, 0.035), (0.05, 0.016), (0, 0.012)], 'brass', 14, T(px, 0, pz))
        for k in range(3):
            a = math.radians(90 + 120 * k)
            b.tube([(px, 0, top + drop), (px + 0.092 * cos(a), 0.092 * sin(a), pz + 0.034)], 0.0035, 'brass', sides=4, caps=False)
    for i, r in enumerate((0.035, 0.028, 0.022)):  # the weights
        b.lathe([(0, 0), (r, 0), (r, r * 1.3), (r * 0.4, r * 1.5), (r * 0.5, r * 1.9), (0, r * 1.9)], 'iron', 10, T(sx0 + 0.22 + i * 0.085, -0.20, top))
    # the ledger, a sack of grain with its scoop, jars, a candle
    b.box((0.36, 0.27, 0.035), 'leather', T(-0.18, -0.04, top + 0.018) @ Rz(-8), bevel=0.006)
    b.box((0.33, 0.24, 0.03), 'paper', T(-0.18, -0.04, top + 0.05) @ Rz(-8), bevel=0.004)
    b.box((0.34, 0.012, 0.012), 'leather', T(-0.16, -0.04, top + 0.068) @ Rz(-52), bevel=0)
    b.lathe([(0, 0), (0.15, 0), (0.19, 0.06), (0.19, 0.20), (0.15, 0.29), (0.11, 0.31), (0.13, 0.36), (0.12, 0.365), (0.095, 0.32), (0.09, 0.27)],
            'linen', 14, T(0.42, 0.03, top))
    b.lathe([(0, 0.288), (0.05, 0.282), (0.092, 0.272)], 'bread', 14, T(0.42, 0.03, top))  # the grain
    b.tube([(0.44, -0.02, top + 0.27), (0.60, -0.20, top + 0.42)], 0.012, 'dark-wood', sides=6)
    jar(b, 0.92, 0.08, top, 0.11, 0.30, cover='linen')
    jar(b, 1.16, -0.06, top, 0.08, 0.20)
    bottle(b, 1.10, 0.18, top, 0.05, 0.28)
    candle(b, 0.10, 0.20, top, 0.20)
    # under the counter: crocks on the shelf
    jar(b, -0.90, 0.10, 0.58, 0.14, 0.34)
    jar(b, -0.50, 0.12, 0.58, 0.12, 0.30, cover='linen')
    jar(b, 0.70, 0.10, 0.58, 0.14, 0.36)
    b.box((0.42, 0.30, 0.24), 'wood', T(0.12, 0.10, 0.70), bevel=0.008)
    return b.finish(), dict(name='Balcão de loja 3D', footprint='pair', blocks=True)


# --- 6 · the remedy shelf --------------------------------------------------------------------------------------------------------
def remedy_shelf():
    b = Farm("farmhouse-remedy-shelf", 106)
    W, D, H = 1.46, 0.42, 2.28
    for sx in (-1, 1):
        b.box((0.055, D, H), 'dark-wood', T(sx * (W / 2 - 0.03), 0, H / 2), bevel=0.008)
    b.box((W - 0.05, 0.025, H - 0.06), 'wood', T(0, D / 2 - 0.02, H / 2), bevel=0)
    levels = (0.10, 0.64, 1.13, 1.60, 2.02)
    for z in levels + (H - 0.02,):
        b.box((W - 0.07, D - 0.02, 0.04), 'wood', T(0, 0, z - 0.02), bevel=0.006)
    b.box((W + 0.08, D + 0.06, 0.05), 'dark-wood', T(0, 0, H + 0.02), bevel=0.012)
    rng = b.rng
    y = -0.03
    for i, x in enumerate((-0.44, 0.0, 0.44)):                    # the big crocks
        jar(b, x, y, levels[0], 0.17, 0.42, cover='linen' if i != 1 else None)
    for i, x in enumerate((-0.54, -0.28, 0.0, 0.27, 0.53)):       # cloth-covered jars
        jar(b, x, y + rng.uniform(-0.03, 0.03), levels[1], rng.uniform(0.095, 0.115), rng.uniform(0.28, 0.36), cover='linen')
    for i, x in enumerate((-0.56, -0.42, -0.28, -0.13, 0.02, 0.17)):  # bottles
        bottle(b, x, y + rng.uniform(-0.05, 0.06), levels[2], rng.uniform(0.045, 0.06), rng.uniform(0.26, 0.36))
    bowl(b, 0.44, y, levels[2], 0.13, 0.10)                         # the mortar and its pestle
    b.tube([(0.42, y, levels[2] + 0.05), (0.56, y - 0.06, levels[2] + 0.22)], [0.02, 0.013], 'stone', sides=8)
    for i, x in enumerate((-0.52, -0.33, -0.15, 0.03)):            # small pots
        jar(b, x, y + rng.uniform(-0.04, 0.04), levels[3], rng.uniform(0.07, 0.085), rng.uniform(0.17, 0.22), cover='linen' if i % 2 else None)
    b.box((0.36, 0.24, 0.16), 'wood', T(0.42, y, levels[3] + 0.08), bevel=0.008)   # the box of roots
    b.box((0.30, 0.20, 0.03), 'paper', T(0.42, y, levels[3] + 0.175) @ Rz(6), bevel=0.004)
    for i, x in enumerate((-0.50, -0.08)):                         # herb bundles lying down, a jug, a candle
        b.lathe([(0, 0), (0.03, 0.02), (0.085, 0.24), (0.07, 0.34), (0, 0.38)], 'herbs', 8, T(x - 0.16, y, levels[4] + 0.09) @ Ry(84) @ Rz(40 * i))
        b.lathe([(0.03, 0.02), (0.036, 0.02), (0.036, 0.045), (0.03, 0.045)], 'rope', 8, T(x - 0.16, y, levels[4] + 0.09) @ Ry(84))
    jug(b, 0.28, y, levels[4], 0.075, 0.17, turn=40)
    candle(b, 0.52, y - 0.05, levels[4], 0.10)
    return b.finish(), dict(name='Estante de remédios 3D', footprint='one', blocks=True)


# --- 7 · the herb drying rack ----------------------------------------------------------------------------------------------------
def herb_rack():
    b = Farm("farmhouse-herb-rack", 107)
    W, H = 0.66, 2.02
    for sx in (-1, 1):
        b.beam((sx * W, 0, 0.05), (sx * W, 0, H), 0.075, 0.075, 'dark-wood', up=(1, 0, 0))
        b.beam((sx * W, -0.36, 0.04), (sx * W, 0.36, 0.04), 0.10, 0.08, 'dark-wood')
        for sy in (-1, 1):
            b.beam((sx * W, sy * 0.28, 0.09), (sx * W, sy * 0.02, 0.52), 0.05, 0.05, 'dark-wood')
    b.beam((-W, 0, 0.26), (W, 0, 0.26), 0.05, 0.06, 'dark-wood')
    rng = b.rng
    for zbar, n in ((H - 0.06, 6), (1.42, 5)):
        b.tube([(-W - 0.10, 0, zbar), (W + 0.10, 0, zbar)], 0.026, 'wood', sides=8)
        for i in range(n):
            x = -W + 0.14 + (2 * W - 0.28) * i / (n - 1) + rng.uniform(-0.03, 0.03)
            yb = rng.uniform(-0.03, 0.03)
            s = rng.uniform(0.82, 1.18)
            m = T(x, yb, zbar - 0.035) @ Rz(rng.uniform(0, 360)) @ Rx(rng.uniform(-7, 7))
            b.tube([(x, yb, zbar + 0.028), (x, yb, zbar - 0.05)], 0.006, 'rope', sides=4, caps=False)
            b.lathe([(0, 0), (0.022 * s, -0.012), (0.028 * s, -0.06), (0.034 * s, -0.10)], 'rope', 8, m)       # the tied stalks
            b.lathe([(0.034 * s, -0.10), (0.085 * s, -0.24 * s), (0.115 * s, -0.36 * s), (0.10 * s, -0.44 * s), (0.05 * s, -0.50 * s), (0, -0.52 * s)],
                    'herbs', 9, m, smooth=False)
    # a basket of cut herbs at its foot, a sickle hung on a post
    b.lathe([(0, 0), (0.22, 0), (0.29, 0.12), (0.31, 0.27), (0.285, 0.27), (0.265, 0.13), (0.20, 0.03), (0, 0.03)], 'straw', 16, T(0.14, -0.42, 0), us=0.8, vs=0.8)
    b.tube([Vector((0.14, -0.42, 0.27)) + Vector((0.297 * cos(a), 0.297 * sin(a), 0)) for a in [math.radians(360 * i / 20) for i in range(20)]],
           0.02, 'straw', sides=6, closed=True, us=0.5, vs=1.5)
    b.sphere(0.25, 'herbs', T(0.14, -0.42, 0.22), segs=12, rings=6, squash=0.42)
    b.tube([Vector((-W - 0.05, -0.02, 1.14)) + Vector((0, 0.16 * cos(a), 0.16 * sin(a))) for a in [math.radians(-100 + 22 * i) for i in range(11)]],
           [0.004 + 0.0012 * i for i in range(11)], 'iron', sides=4)
    b.tube([(-W - 0.05, -0.02 + 0.16 * cos(math.radians(120)), 1.14 + 0.16 * sin(math.radians(120))), (-W - 0.05, -0.20, 1.44)], 0.016, 'dark-wood', sides=8)
    return b.finish(), dict(name='Varal de ervas secas 3D', footprint='one', blocks=True)


# --- 8 · the farm table with its benches (two hexes) -------------------------------------------------------------------------------
def farm_table():
    b = Farm("farmhouse-farm-table", 108)
    L, H = 2.70, 1.06
    for i in range(3):
        b.box((L, 0.275, 0.06), 'wood', T(0, -0.28 + i * 0.28, H), bevel=0.012)
    for sx in (-1, 1):
        x = sx * 1.02
        b.beam((x, -0.40, 0), (x, -0.13, H - 0.04), 0.09, 0.075, 'dark-wood')
        b.beam((x, 0.40, 0), (x, 0.13, H - 0.04), 0.09, 0.075, 'dark-wood')
        b.beam((x, -0.40, H - 0.065), (x, 0.40, H - 0.065), 0.08, 0.07, 'dark-wood')
        b.beam((x, -0.30, 0.42), (x, 0.30, 0.42), 0.06, 0.06, 'dark-wood')
    b.beam((-1.10, 0, 0.42), (1.10, 0, 0.42), 0.07, 0.10, 'dark-wood')
    for sx in (-1, 1):  # the stretcher's pegs
        b.box((0.03, 0.03, 0.18), 'dark-wood', T(sx * 1.09, 0, 0.42), bevel=0.004)
    for sy in (-1, 1):  # the benches
        yb = sy * 0.71
        b.box((L - 0.20, 0.24, 0.05), 'wood', T(0, yb, 0.585), bevel=0.012)
        for sx in (-1, 1):
            b.box((0.055, 0.22, 0.56), 'dark-wood', T(sx * 0.98, yb, 0.28), bevel=0.008)
        b.beam((-0.98, yb, 0.30), (0.98, yb, 0.30), 0.04, 0.07, 'dark-wood')
    top = H + 0.03
    # supper: a loaf on its board with the knife, bowls, a jug, mugs, a candle
    b.box((0.44, 0.28, 0.025), 'dark-wood', T(-0.62, 0.04, top + 0.012) @ Rz(12), bevel=0.006)
    b.sphere(0.15, 'bread', T(-0.64, 0.05, top + 0.085), segs=14, rings=8, squash=0.50, us=1.6)
    b.box((0.24, 0.022, 0.006), 'iron', T(-0.44, -0.06, top + 0.03) @ Rz(30), bevel=0)
    b.tube([(-0.55, -0.12, top + 0.032), (-0.64, -0.17, top + 0.032)], 0.011, 'dark-wood', sides=6)
    bowl(b, -0.10, -0.22, top, 0.13, 0.065)
    bowl(b, 0.34, 0.22, top, 0.13, 0.065)
    b.lathe([(0, 0.014), (0.05, 0.03), (0.092, 0.05)], 'bread', 12, T(-0.10, -0.22, top))  # porridge
    for mx, my in ((0.10, 0.20), (0.62, -0.20)):  # wooden mugs
        b.lathe([(0, 0), (0.05, 0), (0.055, 0.13), (0.045, 0.13), (0.04, 0.02), (0, 0.02)], 'wood', 10, T(mx, my, top))
        b.tube([T(mx, my, top) @ p for p in arc((0.05, 0, 0.065), 0.04, -75, 75, 6, 'xz')], 0.008, 'wood', sides=6)
    jug(b, 0.82, 0.10, top, 0.11, 0.33, turn=-30)
    candle(b, 0.02, 0.0, top, 0.18)
    b.box((0.28, 0.20, 0.012), 'linen', T(1.10, -0.16, top + 0.006) @ Rz(-14), bevel=0)
    return b.finish(), dict(name='Mesa de fazenda com bancos 3D', footprint='pair', blocks=True)


# --- 9 · the spinning wheel --------------------------------------------------------------------------------------------------------
def spinning_wheel():
    b = Farm("farmhouse-spinning-wheel", 109)
    b.box((1.16, 0.25, 0.06), 'wood', T(0, 0, 0.60) @ Ry(-7), bevel=0.012)   # the bench, rising towards the wheel
    b.tube([(-0.44, 0, 0.52), (-0.60, 0, 0)], [0.034, 0.026], 'dark-wood', sides=8)
    for sy in (-1, 1):
        b.tube([(0.42, sy * 0.08, 0.63), (0.56, sy * 0.30, 0)], [0.034, 0.026], 'dark-wood', sides=8)
    cx, cz, R = 0.30, 1.19, 0.46
    for sy in (-1, 1):  # the wheel's uprights
        b.tube([(cx + 0.02, sy * 0.085, 0.62), (cx, sy * 0.085, cz + 0.08)], 0.026, 'dark-wood', sides=8)
        b.sphere(0.034, 'dark-wood', T(cx, sy * 0.085, cz + 0.10), segs=8, rings=6)
    b.tube(arc((cx, 0, cz), R, 0, 360, 32, 'xz')[:-1], 0.032, 'wood', sides=8, closed=True, vs=1.4)
    b.lathe([(0, -0.07), (0.045, -0.07), (0.06, -0.02), (0.06, 0.02), (0.045, 0.07), (0, 0.07)], 'dark-wood', 12, T(cx, 0, cz) @ Rx(90))
    for i in range(10):
        a = math.radians(36 * i + 9)
        p0 = Vector((cx + 0.05 * cos(a), 0, cz + 0.05 * sin(a)))
        p1 = Vector((cx + (R - 0.02) * cos(a), 0, cz + (R - 0.02) * sin(a)))
        b.tube([p0, p0.lerp(p1, 0.5), p1], [0.014, 0.019, 0.012], 'dark-wood', sides=6)
    b.tube([(cx, -0.14, cz), (cx, 0.20, cz)], 0.012, 'iron', sides=6)
    b.tube([(cx, 0.20, cz), (cx + 0.09, 0.20, cz - 0.02)], 0.01, 'iron', sides=6)                     # the crank
    b.tube([(cx + 0.09, 0.20, cz - 0.02), (0.40, 0.15, 0.12)], 0.008, 'dark-wood', sides=6)           # the footman
    b.box((0.46, 0.15, 0.025), 'wood', T(0.30, 0.13, 0.08) @ Ry(-8), bevel=0.006)                     # the treadle
    b.tube([(0.50, -0.26, 0.05), (0.50, 0.26, 0.05)], 0.016, 'dark-wood', sides=6)
    # the flyer: the maidens, the bobbin with its spun thread, the drive band from the wheel
    fx, fz = -0.40, 0.92
    for sy in (-1, 1):
        b.tube([(fx, sy * 0.095, 0.55), (fx, sy * 0.095, fz + 0.07)], 0.02, 'dark-wood', sides=8)
        b.sphere(0.028, 'dark-wood', T(fx, sy * 0.095, fz + 0.09), segs=8, rings=6)
    ax = T(fx, 0, fz) @ Rx(90)
    b.lathe([(0.012, -0.085), (0.05, -0.085), (0.05, -0.07), (0.018, -0.07), (0.018, 0.045), (0.05, 0.045), (0.05, 0.06), (0.012, 0.06)], 'wood', 12, ax)
    b.lathe([(0.018, -0.065), (0.036, -0.05), (0.04, 0.0), (0.034, 0.035), (0.018, 0.042)], 'wool', 12, ax, us=3.0, vs=3.0)
    b.tube([(fx, -0.10, fz), (fx, 0.12, fz)], 0.007, 'iron', sides=6)
    b.tube([(cx, 0.0, cz + R + 0.03), (fx, 0.0, fz + 0.05), (fx - 0.045, 0.0, fz), (fx, 0.0, fz - 0.05), (cx, 0.0, cz - R - 0.03)], 0.005, 'rope', sides=4, caps=False)
    # the distaff with its wool, the yarn running to the flyer, a basket of fleece
    b.tube([(-0.52, 0.09, 0.56), (-0.60, 0.12, 1.62)], [0.02, 0.012], 'dark-wood', sides=8)
    b.sphere(0.13, 'wool', T(-0.595, 0.118, 1.46), segs=12, rings=8, squash=1.55, us=2.5)
    b.tube([(-0.58, 0.07, 1.36), (-0.50, 0.03, 1.10), (fx, 0.02, fz + 0.04)], 0.004, 'wool', sides=4, caps=False)
    b.lathe([(0, 0), (0.17, 0), (0.22, 0.10), (0.235, 0.20), (0.215, 0.20), (0.20, 0.10), (0.15, 0.025), (0, 0.025)], 'straw', 14, T(-0.10, -0.42, 0), us=0.8, vs=0.8)
    b.sphere(0.19, 'wool', T(-0.10, -0.42, 0.17), segs=12, rings=6, squash=0.5, us=2.5)
    return b.finish(), dict(name='Roda de fiar 3D', footprint='one', blocks=True)


# --- 10 · the cradle ---------------------------------------------------------------------------------------------------------------
def cradle():
    b = Farm("farmhouse-cradle", 110)
    for sx in (-1, 1):  # the rockers
        b.tube(arc((sx * 0.40, 0, 0.62), 0.62, -122, -58, 10, 'yz'), 0.036, 'dark-wood', sides=8, vs=1.2)
        b.beam((sx * 0.40, -0.20, 0.10), (sx * 0.40, 0.20, 0.10), 0.05, 0.14, 'dark-wood')
    b.box((0.98, 0.46, 0.03), 'wood', T(0, 0, 0.19), bevel=0.006)
    for sy in (-1, 1):
        for i in range(2):
            b.box((0.98, 0.028, 0.15), 'wood', T(0, sy * (0.235 + 0.03 * i), 0.275 + 0.155 * i) @ Rx(-sy * 10), bevel=0.006)
    # the head board (taller, with a cut heart of light) and the foot board
    b.box((0.03, 0.56, 0.52), 'wood', T(-0.49, 0, 0.44), bevel=0.012)
    b.box((0.034, 0.50, 0.07), 'dark-wood', T(-0.49, 0, 0.72), bevel=0.02)
    b.box((0.03, 0.56, 0.36), 'wood', T(0.49, 0, 0.36), bevel=0.012)
    for sx, h in ((-1, 0.74), (1, 0.56)):
        for sy in (-1, 1):
            b.tube([(sx * 0.49, sy * 0.27, 0.16), (sx * 0.49, sy * 0.29, h)], 0.026, 'dark-wood', sides=8)
            b.sphere(0.036, 'dark-wood', T(sx * 0.49, sy * 0.29, h + 0.02), segs=8, rings=6)
    b.box((0.90, 0.42, 0.09), 'straw', T(0, 0, 0.25), bevel=0.03, us=0.8)                 # the straw tick
    b.box((0.88, 0.41, 0.03), 'linen', T(0, 0, 0.305), bevel=0.012)
    b.sphere(0.14, 'linen', T(-0.30, 0, 0.34), segs=12, rings=8, squash=0.42)     # the pillow
    b.box((0.56, 0.50, 0.05), 'blanket', T(0.16, 0, 0.345), bevel=0.02)                   # the blanket, folded back
    b.box((0.10, 0.50, 0.03), 'linen', T(-0.10, 0, 0.375), bevel=0.012)
    return b.finish(), dict(name='Berço de embalar 3D', footprint='one', blocks=True)


# --- 11 · the oil lantern on its post (lights the room) ------------------------------------------------------------------------------
def oil_lantern():
    b = Farm("farmhouse-oil-lantern", 111)
    H = 1.92
    b.beam((-0.36, 0, 0.045), (0.36, 0, 0.045), 0.10, 0.09, 'dark-wood')
    b.beam((0, -0.36, 0.045), (0, 0.36, 0.045), 0.10, 0.09, 'dark-wood')
    b.beam((0, 0, 0.09), (0, 0, H), 0.085, 0.085, 'dark-wood', up=(1, 0, 0))
    for a in (0, 90, 180, 270):
        c, s = cos(math.radians(a)), sin(math.radians(a))
        b.beam((0.27 * c, 0.27 * s, 0.10), (0.03 * c, 0.03 * s, 0.46), 0.05, 0.05, 'dark-wood', up=(-s, c, 0))
    ax = 0.34
    b.beam((-0.06, 0, H - 0.06), (ax + 0.06, 0, H - 0.06), 0.07, 0.07, 'dark-wood')
    b.beam((0.03, 0, H - 0.40), (ax - 0.04, 0, H - 0.10), 0.045, 0.045, 'dark-wood', up=(0, 1, 0))
    b.tube([(ax, 0, H - 0.09), (ax, 0, H - 0.15), (ax + 0.025, 0, H - 0.18), (ax, 0, H - 0.21), (ax - 0.02, 0, H - 0.19)], 0.008, 'iron', sides=6)
    z0 = H - 0.21 - 0.36  # the lantern's foot
    m = T(ax, 0, z0)
    b.tube([m @ p for p in arc((0, 0, 0.325), 0.035, 0, 360, 12, 'xz')[:-1]], 0.006, 'iron', sides=6, closed=True)  # the ring
    b.lathe([(0, 0), (0.105, 0), (0.108, 0.012), (0.10, 0.028), (0, 0.028)], 'iron', 16, m)                       # the oil pan
    b.lathe([(0.03, 0.028), (0.045, 0.04), (0.03, 0.055), (0.012, 0.06), (0, 0.06)], 'brass', 12, m)              # the burner
    b.lathe([(0.086, 0.028), (0.094, 0.10), (0.086, 0.20)], 'lamp-glow', 16, m, us=3.0, vs=3.0)                   # the glass
    for k in range(4):
        a = math.radians(45 + 90 * k)
        b.tube([m @ Vector((0.10 * cos(a), 0.10 * sin(a), 0.02)), m @ Vector((0.105 * cos(a), 0.105 * sin(a), 0.11)), m @ Vector((0.10 * cos(a), 0.10 * sin(a), 0.21))],
               0.007, 'iron', sides=6)
    b.lathe([(0, 0.20), (0.108, 0.20), (0.112, 0.212), (0.06, 0.255), (0.035, 0.27), (0.03, 0.29), (0, 0.292)], 'iron', 16, m)  # the cap
    b.lathe([(0, 0.062), (0.016, 0.085), (0.011, 0.125), (0, 0.165)], 'flame', 8, m)
    # the oil can and the wick trimmer at the foot
    jug(b, -0.22, -0.20, 0.0, 0.085, 0.24, 'brass', turn=-140)
    return b.finish(), dict(name='Lanterna a óleo em poste 3D', footprint='one', blocks=True,
                            light={"color": [1.0, 0.70, 0.36], "intensity": 9, "radius": 5.2, "flicker": 0.4, "minHeight": 1.45})


# --- 12 · the washstand --------------------------------------------------------------------------------------------------------------
def washstand():
    b = Farm("farmhouse-washstand", 112)
    W, D, H = 0.84, 0.52, 0.98
    b.box((W, D, 0.045), 'wood', T(0, 0, H), bevel=0.012)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.beam((sx * (W / 2 - 0.05), sy * (D / 2 - 0.05), 0), (sx * (W / 2 - 0.05), sy * (D / 2 - 0.05), H - 0.02), 0.055, 0.055, 'dark-wood', up=(1, 0, 0))
        b.beam((sx * (W / 2 - 0.05), -D / 2 + 0.05, H - 0.09), (sx * (W / 2 - 0.05), D / 2 - 0.05, H - 0.09), 0.03, 0.10, 'dark-wood')
    for sy in (-1, 1):
        b.beam((-W / 2 + 0.05, sy * (D / 2 - 0.05), H - 0.09), (W / 2 - 0.05, sy * (D / 2 - 0.05), H - 0.09), 0.03, 0.10, 'dark-wood')
    b.box((W - 0.12, D - 0.12, 0.03), 'wood', T(0, 0, 0.30), bevel=0.006)
    b.box((W, 0.03, 0.30), 'wood', T(0, D / 2 - 0.015, H + 0.17), bevel=0.012)          # the splash board
    top = H + 0.023
    b.lathe([(0, 0.01), (0.09, 0), (0.19, 0.035), (0.235, 0.105), (0.243, 0.11), (0.232, 0.12), (0.18, 0.055), (0.09, 0.025), (0, 0.03)], 'clay', 20, T(-0.08, -0.01, top))
    b.lathe([(0, 0.09), (0.11, 0.09), (0.215, 0.09)], 'water', 20, T(-0.08, -0.01, top))
    jug(b, 0.10, 0.02, 0.315, 0.13, 0.40, turn=20)
    b.box((0.13, 0.085, 0.045), 'wax', T(0.30, -0.14, top + 0.022) @ Rz(15), bevel=0.014)   # the soap
    # the towel over the rail at the side, a comb
    b.tube([(W / 2 + 0.01, -D / 2 + 0.08, H - 0.20), (W / 2 + 0.07, -D / 2 + 0.08, H - 0.20), (W / 2 + 0.07, D / 2 - 0.08, H - 0.20), (W / 2 + 0.01, D / 2 - 0.08, H - 0.20)],
           0.012, 'dark-wood', sides=6)
    b.box((0.014, 0.26, 0.46), 'linen', T(W / 2 + 0.085, 0, H - 0.42), bevel=0)
    b.box((0.014, 0.26, 0.30), 'linen', T(W / 2 + 0.055, 0, H - 0.34), bevel=0)
    b.tube([(W / 2 + 0.07, -0.13, H - 0.19), (W / 2 + 0.07, 0.13, H - 0.19)], 0.02, 'linen', sides=6)
    b.box((0.16, 0.05, 0.01), 'dark-wood', T(-0.28, -0.19, top + 0.005) @ Rz(-20), bevel=0)
    return b.finish(), dict(name='Lavatório 3D', footprint='one', blocks=True)


# --- 13 · the linen chest ------------------------------------------------------------------------------------------------------------
def linen_chest():
    b = Farm("farmhouse-linen-chest", 113)
    W, D, H = 1.22, 0.56, 0.52
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box((0.10, 0.10, 0.08), 'dark-wood', T(sx * (W / 2 - 0.07), sy * (D / 2 - 0.07), 0.04), bevel=0.01)
    for i in range(3):  # the boards of the front and the back
        for sy in (-1, 1):
            b.box((W, 0.035, H / 3 - 0.006), 'wood', T(0, sy * (D / 2 - 0.018), 0.08 + H / 6 + i * H / 3), bevel=0.006)
    for sx in (-1, 1):
        b.box((0.035, D - 0.07, H), 'wood', T(sx * (W / 2 - 0.018), 0, 0.08 + H / 2), bevel=0.006)
    b.box((W - 0.06, D - 0.06, 0.03), 'wood', T(0, 0, 0.10), bevel=0)
    lid = 0.08 + H
    for i in range(3):  # the lid: three boards, a little crowned
        b.box((W + 0.05, (D + 0.05) / 3 - 0.006, 0.05), 'wood', T(0, -(D + 0.05) / 3 + i * (D + 0.05) / 3, lid + 0.025 + (0.016 if i == 1 else 0)), bevel=0.012)
    for sx in (-1, 1):  # iron straps down the front and over the lid, the hasp
        b.box((0.055, 0.012, H + 0.03), 'iron', T(sx * 0.36, -D / 2 - 0.004, 0.08 + H / 2), bevel=0.004)
        b.box((0.055, D + 0.06, 0.012), 'iron', T(sx * 0.36, 0, lid + 0.072), bevel=0.004)
        rivets(b, 0, 0, 1, 'iron', 0.014, T(sx * 0.36, -D / 2 - 0.012, 0.20))
        rivets(b, 0, 0, 1, 'iron', 0.014, T(sx * 0.36, -D / 2 - 0.012, 0.46))
    b.box((0.11, 0.014, 0.14), 'iron', T(0, -D / 2 - 0.005, lid - 0.07), bevel=0.004)
    b.tube([Vector((0, -D / 2 - 0.02, lid - 0.10)) + Vector((0.03 * cos(a), 0, 0.035 * sin(a))) for a in [math.radians(180 + 20 * i) for i in range(10)]], 0.008, 'iron', sides=6)
    for sx in (-1, 1):  # rope handles on the ends
        b.tube([Vector((sx * (W / 2 + 0.005), 0, 0.42)) + Vector((sx * 0.07 * sin(a), 0.12 * cos(a), -0.07 * sin(a))) for a in [math.radians(18 * i) for i in range(11)]],
               0.016, 'rope', sides=6, vs=3.0)
    # folded linen and a blanket on the lid
    top = lid + 0.07
    b.box((0.56, 0.40, 0.05), 'linen', T(-0.22, 0.02, top + 0.025) @ Rz(4), bevel=0.018)
    b.box((0.52, 0.37, 0.05), 'linen', T(-0.23, 0.02, top + 0.075) @ Rz(-3), bevel=0.018)
    b.box((0.48, 0.36, 0.07), 'blanket', T(-0.22, 0.02, top + 0.135) @ Rz(6), bevel=0.025)
    candle(b, 0.50, 0.02, lid + 0.066, 0.14)
    return b.finish(), dict(name='Arca de roupa 3D', footprint='one', blocks=True)


ASSETS = {'anvil': anvil, 'quench-tub': quench_tub, 'tool-rack': tool_rack, 'grindstone': grindstone, 'shop-counter': shop_counter,
          'remedy-shelf': remedy_shelf, 'herb-rack': herb_rack, 'farm-table': farm_table, 'spinning-wheel': spinning_wheel, 'cradle': cradle,
          'oil-lantern': oil_lantern, 'washstand': washstand, 'linen-chest': linen_chest}


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    force = '--replace-unreleased' in argv  # only while v01 has never been in a map or a pack
    wanted = [a for i, a in enumerate(argv) if not a.startswith('--') and (i == 0 or argv[i - 1] != '--version')] or list(ASSETS)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    manifest_path = TMP / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding='utf-8')) if manifest_path.exists() else {}
    if STOOD_IN:
        print("FARMHOUSE stand-ins (the farmhouse texture set has not arrived):", ", ".join(STOOD_IN))
    made = []
    for key in wanted:
        glb = OUT / f"{key}-{VERSION}.glb"
        if glb.exists() and not force:
            raise FileExistsError(f"Refusing to overwrite existing file: {glb}")
        obj, info = ASSETS[key]()
        for mat in obj.data.materials:
            mat.name = mat.name.replace("Lighthouse ", "Farmhouse ")
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.export_scene.gltf(filepath=str(glb), export_format="GLB", use_selection=True, export_apply=True, export_texcoords=True,
                                  export_normals=True, export_tangents=True, export_materials="EXPORT", export_image_format="JPEG", export_yup=True)
        bounds = [Vector(c) for c in obj.bound_box]
        size = [max(p[i] for p in bounds) - min(p[i] for p in bounds) for i in range(3)]
        tris = len(obj.data.polygons)
        manifest[key] = dict(info, key=key, file=f"{key}-{VERSION}.glb", height=round(size[2], 3), width=round(max(size[0], size[1]), 3),
                             depth=round(min(size[0], size[1]), 3), triangles=tris, materials=[m.name for m in obj.data.materials],
                             stand_ins=STOOD_IN)
        print(f"FARMHOUSE {key}: x={size[0]:.3f} y={size[1]:.3f} z={size[2]:.3f} triangles={tris} materials={len(obj.data.materials)}")
        made.append(obj)
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    if len(wanted) == len(ASSETS):
        for i, obj in enumerate(made):
            obj.location.x = (i - len(made) / 2) * 4.0
        blend = OUT / f"farmhouse-kit-{VERSION}.blend"
        if not blend.exists() or force:
            bpy.ops.file.pack_all()
            bpy.ops.wm.save_as_mainfile(filepath=str(blend))
    print("FARMHOUSE done", ", ".join(wanted))


main()
