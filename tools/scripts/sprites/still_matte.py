"""A photographed prop on a plain studio sweep -> a PNG with alpha for decoracoes-2d/ (the user's art, e.g. Attachments/MistyCave).

BiRefNet general (the same ONNX model as video_matte.py, rembg's birefnet-general, on the CPU) gives the matte at 1024 x 1024,
resized back to the photo. The sweep's colour is taken from the photo's border (its median), and the partly clear edge pixels
are unmixed from it (colour = (pixel - (1 - a) * sweep) / a), so no grey fringe stays on the silhouette. The result is trimmed
to the prop's box plus a margin; the photo's own pixels are kept (no resampling of the prop).

    python -I tools/scripts/sprites/still_matte.py <photo> <out.png> [--margin 8] [--threads 12] [--model lite]
"""
import argparse
import os

import numpy as np
import onnxruntime as ort
from PIL import Image

MODELS = {
    "general": os.path.expanduser("~/.rembg/models/birefnet-general/birefnet-general.onnx"),
    "lite": os.path.expanduser("~/.rembg/models/birefnet-general-lite/birefnet-general-lite.onnx"),
}
MEAN = np.array([0.485, 0.456, 0.406], np.float32)
STD = np.array([0.229, 0.224, 0.225], np.float32)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("photo")
    ap.add_argument("out")
    ap.add_argument("--margin", type=int, default=8)
    ap.add_argument("--threads", type=int, default=12)
    ap.add_argument("--model", choices=sorted(MODELS), default="general")
    a = ap.parse_args()

    im = Image.open(a.photo).convert("RGB")
    so = ort.SessionOptions()
    so.intra_op_num_threads = a.threads
    sess = ort.InferenceSession(MODELS[a.model], so, providers=["CPUExecutionProvider"])
    x = np.asarray(im.resize((1024, 1024), Image.BICUBIC), np.float32) / 255
    x = ((x - MEAN) / STD).transpose(2, 0, 1)[None]
    y = sess.run(None, {sess.get_inputs()[0].name: x})[0][0, 0]
    m = 1 / (1 + np.exp(-np.clip(y, -40, 40)))
    alpha = np.asarray(Image.fromarray((m * 255 + 0.5).astype(np.uint8)).resize(im.size, Image.BICUBIC), np.float32) / 255

    rgb = np.asarray(im, np.float32) / 255
    border = np.concatenate([rgb[:8].reshape(-1, 3), rgb[-8:].reshape(-1, 3), rgb[:, :8].reshape(-1, 3), rgb[:, -8:].reshape(-1, 3)])
    sweep = np.median(border, axis=0)
    a3 = alpha[..., None]
    unmixed = np.where(a3 > 0.02, (rgb - (1 - a3) * sweep) / np.maximum(a3, 0.02), rgb)
    rgb = np.where((a3 > 0.02) & (a3 < 0.98), np.clip(unmixed, 0, 1), rgb)
    alpha = np.where(alpha < 0.02, 0, alpha)

    ys, xs = np.nonzero(alpha > 0.04)
    if len(xs) == 0:
        raise SystemExit(f"{a.photo}: the matte found nothing")
    h, w = alpha.shape
    x0, x1 = max(0, xs.min() - a.margin), min(w, xs.max() + 1 + a.margin)
    y0, y1 = max(0, ys.min() - a.margin), min(h, ys.max() + 1 + a.margin)
    out = np.dstack([rgb, alpha])[y0:y1, x0:x1]
    Image.fromarray((out * 255 + 0.5).astype(np.uint8), "RGBA").save(a.out)
    print(f"{a.out}: {x1 - x0} x {y1 - y0} (sweep {np.round(sweep * 255).astype(int).tolist()})")


if __name__ == "__main__":
    main()
