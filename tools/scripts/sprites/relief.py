"""Relief maps for the 2D cards: per source frame, a normal map, a depth (height) map and an occlusion map, so a
card can be lit per pixel, write its own depth, parallax with the camera and shadow itself (shaders/sprites.hlsl).

    python -I tools/scripts/sprites/relief.py <frames.json> <source root> <relief dir>
           [--model <model.onnx>] [--only <set prefix>]... [--thickness 0.45] [--force]

<frames.json> is what `Cooker sprite-frames <manifest> <out.json>` writes: every clip with its frames (file, grid
cell, alpha-from-black) and the relief path the cooker will look for (`Cooker sprites ... --relief <relief dir>`).

Depth: Depth Anything V2 Small (onnx-community/depth-anything-v2-small, Apache 2.0; fetched once into
build/models/ when missing), relative inverse depth, normalised over the whole clip (so an animation does not
flicker), rounded off toward the silhouette (a quarter-circle bevel, as a body is), 1 = nearest. Normals come
from that height field at the card's thickness (height x thickness x width in pixels); occlusion is how far a
pixel lies below its surroundings at three radii. Output RGBA8 PNG, the same size as the frame:
  R, G  normal x, y (tangent space: x along the card's width, y up), 0.5 = 0
  B     occlusion (1 = open)
  A     height, 1 = nearest the viewer
Outside the art every channel repeats the nearest art pixel, so filtering and BC7 never pull in the void.
Frames already written are skipped (resume after a power cut); --force rewrites them.
"""

import argparse
import json
import os
import sys
import time
import urllib.request
from pathlib import Path

import numpy as np
from PIL import Image
from scipy import ndimage

MODEL_URL = "https://huggingface.co/onnx-community/depth-anything-v2-small/resolve/main/onnx/model.onnx"
MODEL_DEFAULT = Path("build/models/depth-anything-v2-small/model.onnx")
NET_SIDE = 518  # the longer side fed to the network (a multiple of 14 either way)
MEAN = np.array([0.485, 0.456, 0.406], dtype=np.float32)
STD = np.array([0.229, 0.224, 0.225], dtype=np.float32)


def fetch_model(path: Path) -> None:
    if path.exists():
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    print(f"downloading {MODEL_URL} -> {path}", flush=True)
    tmp = path.with_suffix(".part")
    with urllib.request.urlopen(MODEL_URL) as r, open(tmp, "wb") as out:
        total = 0
        while True:
            chunk = r.read(1 << 20)
            if not chunk:
                break
            out.write(chunk)
            total += len(chunk)
    os.replace(tmp, path)
    print(f"downloaded {total / 1e6:.1f} MB", flush=True)


def load_frame(root: Path, frame: dict) -> np.ndarray:
    """The frame as straight-alpha RGBA8, exactly as tools/SpriteCooker.cpp loadFrame cuts it."""
    im = Image.open(root / frame["file"]).convert("RGBA")
    w, h = im.size
    cols, rows, cell = frame.get("cols", 1), frame.get("rows", 1), frame.get("cell", 0)
    cx, cy = cell % cols, cell // cols
    x0, x1 = cx * w // cols, cx * w // cols + w // cols
    y0, y1 = cy * h // rows, cy * h // rows + h // rows
    px = np.asarray(im, dtype=np.uint8)[y0:y1, x0:x1].copy()
    if frame.get("black", False):
        rgb = px[..., :3].astype(np.int32)
        a = np.minimum(255, 4 * rgb.max(axis=2))
        safe = np.maximum(a, 1)[..., None]
        px[..., :3] = np.where(a[..., None] > 0, np.minimum(255, (rgb * 255 + safe // 2) // safe), 0).astype(np.uint8)
        px[..., 3] = a.astype(np.uint8)
    return px


class Depth:
    def __init__(self, model: Path):
        import onnxruntime as ort

        opts = ort.SessionOptions()
        opts.intra_op_num_threads = max(1, (os.cpu_count() or 4))
        self.session = ort.InferenceSession(str(model), opts, providers=["CPUExecutionProvider"])
        self.input = self.session.get_inputs()[0].name
        self.output = self.session.get_outputs()[0].name

    def __call__(self, rgb: np.ndarray) -> np.ndarray:
        """Relative inverse depth (bigger = nearer) at the image's own size."""
        h, w = rgb.shape[:2]
        scale = NET_SIDE / max(h, w)
        nh, nw = max(14, int(round(h * scale / 14)) * 14), max(14, int(round(w * scale / 14)) * 14)
        im = Image.fromarray(rgb).resize((nw, nh), Image.BICUBIC)
        x = (np.asarray(im, dtype=np.float32) / 255.0 - MEAN) / STD
        x = np.ascontiguousarray(x.transpose(2, 0, 1)[None])
        y = self.session.run([self.output], {self.input: x})[0]
        y = np.asarray(y, dtype=np.float32).reshape(y.shape[-2], y.shape[-1])
        back = Image.fromarray(y).resize((w, h), Image.BICUBIC)
        return np.asarray(back, dtype=np.float32)


def nearest_fill(values: np.ndarray, mask: np.ndarray) -> np.ndarray:
    """Every pixel outside `mask` takes the nearest inside pixel's value (channels last)."""
    if mask.all():
        return values
    if not mask.any():
        return values
    idx = ndimage.distance_transform_edt(~mask, return_distances=False, return_indices=True)
    return values[idx[0], idx[1]]


def relief_of(frames_rgba: list, depths: list, thickness: float) -> list:
    """The clip's relief images from its frames and their raw depths (height normalised over the clip)."""
    masks = [f[..., 3] > 0 for f in frames_rgba]
    inside = np.concatenate([d[m] for d, m in zip(depths, masks) if m.any()]) if any(m.any() for m in masks) else None
    if inside is None or inside.size < 16:
        lo, hi = 0.0, 1.0
    else:
        lo, hi = np.percentile(inside, 1.0), np.percentile(inside, 99.0)
        if hi - lo < 1e-6:
            hi = lo + 1.0
    out = []
    for px, d, m in zip(frames_rgba, depths, masks):
        h, w = m.shape
        height = np.clip((d - lo) / (hi - lo), 0.0, 1.0).astype(np.float32)
        if m.any():
            ys, xs = np.nonzero(m)
            bw, bh = xs.max() - xs.min() + 1, ys.max() - ys.min() + 1
        else:
            bw, bh = w, h
        # A quarter-circle bevel toward the silhouette: a body is round where the art ends.
        dist = ndimage.distance_transform_edt(m).astype(np.float32)
        r = max(3.0, 0.07 * min(bw, bh))
        t = np.clip(dist / r, 0.0, 1.0)
        bevel = np.sqrt(np.clip(t * (2.0 - t), 0.0, 1.0))
        height = height * (0.15 + 0.85 * bevel)
        height = np.where(m, height, 0.0).astype(np.float32)
        height = nearest_fill(height[..., None], m)[..., 0]
        # Normals from the height field in pixels: height x the card's thickness (a share of its width).
        scale = thickness * float(bw)
        smooth = ndimage.gaussian_filter(height, 0.8)
        gx = ndimage.sobel(smooth, axis=1) / 8.0 * scale
        gy = ndimage.sobel(smooth, axis=0) / 8.0 * scale
        n = np.stack([-gx, gy, np.ones_like(gx)], axis=-1)  # image rows run down: +y is up the card
        n /= np.linalg.norm(n, axis=-1, keepdims=True)
        # Occlusion: how far below its surroundings a pixel lies, at three radii.
        ao = np.ones_like(height)
        for sigma, weight in ((2.0, 0.5), (6.0, 0.3), (14.0, 0.2)):
            around = ndimage.gaussian_filter(height, sigma)
            ao -= weight * np.clip((around - height) * scale / (sigma * 1.5), 0.0, 1.0)
        ao = np.clip(ao, 0.25, 1.0)
        rgba = np.empty((h, w, 4), dtype=np.float32)
        rgba[..., 0] = n[..., 0] * 0.5 + 0.5
        rgba[..., 1] = n[..., 1] * 0.5 + 0.5
        rgba[..., 2] = ao
        rgba[..., 3] = height
        rgba = nearest_fill(rgba, m)
        out.append(np.clip(rgba * 255.0 + 0.5, 0, 255).astype(np.uint8))
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("frames")
    ap.add_argument("root")
    ap.add_argument("out")
    ap.add_argument("--model", default=str(MODEL_DEFAULT))
    ap.add_argument("--only", action="append", default=[])
    ap.add_argument("--thickness", type=float, default=0.45)
    ap.add_argument("--force", action="store_true")
    args = ap.parse_args()

    root, out, model = Path(args.root), Path(args.out), Path(args.model)
    with open(args.frames, "r", encoding="utf-8") as f:
        clips = json.load(f)["clips"]
    if args.only:
        clips = [c for c in clips if any(c["set"].startswith(p) for p in args.only)]
    fetch_model(model)
    depth = Depth(model)

    def written(path: Path) -> bool:
        """A map already there and readable (a power cut can leave a file of the right size full of zeros)."""
        if not path.exists():
            return False
        try:
            Image.open(path).verify()
            return True
        except Exception:  # noqa: BLE001
            return False

    todo = [c for c in clips if args.force or any(not written(out / fr["relief"]) for fr in c["frames"])]
    print(f"{len(clips)} clips, {sum(len(c['frames']) for c in clips)} frames; {len(todo)} clips to do", flush=True)
    start = time.time()
    done_frames = 0
    for i, clip in enumerate(todo):
        t0 = time.time()
        try:
            frames = [load_frame(root, fr) for fr in clip["frames"]]
        except Exception as e:  # noqa: BLE001
            print(f"  {clip['set']}/{clip['clip']}: cannot read a frame ({e})", flush=True)
            continue
        depths = []
        for px in frames:
            a = px[..., 3:4].astype(np.float32) / 255.0
            rgb = (px[..., :3].astype(np.float32) * a + 118.0 * (1.0 - a)).astype(np.uint8)  # over neutral grey
            depths.append(depth(rgb))
        reliefs = relief_of(frames, depths, args.thickness)
        for fr, img in zip(clip["frames"], reliefs):
            path = out / fr["relief"]
            path.parent.mkdir(parents=True, exist_ok=True)
            tmp = path.with_suffix(".part.png")
            Image.fromarray(img, "RGBA").save(tmp, compress_level=1)
            os.replace(tmp, path)
        done_frames += len(frames)
        el = time.time() - start
        print(f"  [{i + 1}/{len(todo)}] {clip['set']}/{clip['clip']}: {len(frames)} frames in {time.time() - t0:.1f}s "
              f"({done_frames / max(el, 1e-6):.2f} frames/s)", flush=True)
    print(f"done: {done_frames} frames in {time.time() - start:.0f}s", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
