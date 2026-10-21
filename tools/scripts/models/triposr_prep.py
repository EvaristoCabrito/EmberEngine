"""Alpha PNG -> TripoSR input (content/models/README.md, Image to 3D): square, foreground at 85 % of the side, grey 0.5 background."""
import sys
import numpy as np
from PIL import Image

src, dst = sys.argv[1], sys.argv[2]
im = Image.open(src).convert("RGBA")
a = np.asarray(im)[:, :, 3]
ys, xs = np.nonzero(a > 8)
im = im.crop((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1))
side = int(max(im.size) / 0.85)
sq = Image.new("RGBA", (side, side), (0, 0, 0, 0))
sq.paste(im, ((side - im.size[0]) // 2, (side - im.size[1]) // 2))
f = np.asarray(sq).astype(np.float32) / 255
rgb = f[:, :, :3] * f[:, :, 3:4] + (1 - f[:, :, 3:4]) * 0.5
Image.fromarray((rgb * 255).astype(np.uint8)).resize((512, 512), Image.LANCZOS).save(dst)
print(src, "->", dst, sq.size)
