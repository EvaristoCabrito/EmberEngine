"""The two grave markers again as v02, without the earth mound under the stone (the user, 2026-10-20: on the cemetery's
uneven ground the mound showed as a black disc, or a white one under snow, cut by the board). v01 stays as it was.

  blender -b --python content/models/blender/graves_v02.py
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emberlib import *  # noqa
import kit_extra
import kit_frost

reports = []
for kit, name, build in (
        ('extra', 'grave-marker', lambda: kit_extra.grave_marker(mound=False)),
        ('frost', 'snowy-grave-marker', lambda: kit_frost.g(kit_frost.snow_cap(kit_extra.grave_marker(seed=515, mound=False), 0.6, 515, 0.3)))):
    reset()
    reports.append(export(build(), kit, name, version='v02'))
    print(f'[ok] {kit}/{name}-v02: {reports[-1]["triangles"]} tris, {reports[-1]["size"]}')
print('JSON_BEGIN' + json.dumps({'assets': reports}) + 'JSON_END')
