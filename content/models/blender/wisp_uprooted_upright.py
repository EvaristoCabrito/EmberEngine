"""The Wisp Forest's uprooted dead tree standing upright (v03, additive: v01 / v02 untouched).

The same recipe as kit_wisp_revisited.py's 'uprooted-leaning' (seed, height, trunk, roots, limbs) with the
trunk's lean 0, so the tree stands straight on its hex.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit_wisp_revisited as kit  # noqa: E402
from emberlib import reset, export  # noqa: E402


def build():
    reset(); mats = kit._materials()
    obj = kit._tree((104, 7.8, .55, 0.0, 'lean'), mats)
    print('[ok] tree uprooted-leaning upright', export(obj, kit.KIT, 'uprooted-leaning', 'v03'))


if __name__ == '__main__': build()
