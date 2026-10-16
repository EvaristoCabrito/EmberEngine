"""Engine2's two mouse cursors, taken out of their SVG wrappers as the PNGs they carry.

  python -I tools/scripts/engine2/export_cursors.py [C:/Engine2/public] [content/ui/cursors]

pointer.png = game/ui/pointer-large.svg (the arrow on every screen, hot spot 0 0), grab.png =
game/cursors/medieval-gauntlet-grab-small.svg (the gauntlet while the board or the world map is dragged, hot spot at
its centre). Both SVGs show their image at 26.25 CSS px (engine/render/DebugUi.cpp sizes them the same way).
"""
import base64
import re
import sys
from pathlib import Path

public = Path(sys.argv[1] if len(sys.argv) > 1 else "C:/Engine2/public")
out = Path(sys.argv[2] if len(sys.argv) > 2 else "content/ui/cursors")
out.mkdir(parents=True, exist_ok=True)
for name, svg in (("pointer.png", "game/ui/pointer-large.svg"), ("grab.png", "game/cursors/medieval-gauntlet-grab-small.svg")):
    text = (public / svg).read_text(encoding="utf-8")
    match = re.search(r'href="data:image/png;base64,([A-Za-z0-9+/=]+)"', text)
    if not match:
        sys.exit(f"no embedded PNG in {svg}")
    data = base64.b64decode(match.group(1))
    (out / name).write_bytes(data)
    print(f"{svg} -> {out / name} ({len(data)} bytes)")
