"""Video -> sprite, step 1, the high-resolution matte: every native frame of a stretch of a video and its alpha, from BiRefNet HR
matting (ZhengPeng7/BiRefNet_HR-matting, trained at 2048 x 2048 to give each pixel its own opacity: teeth, the gaps between them,
drool and hair come out as they are, not as hard blocks of figure or wall). The user, 2026-11-17: "a new tool that actually works".

Writes <out>/src-NNNN.png (the video frame, RGB) and <out>/alpha-NNNN.png (8-bit), the same files as video_matte.py, so
video_sprite.py cuts from either. Resumable: frames already written are skipped. Runs in its own Python (build/hrmatte-venv: PyTorch
on the CPU); the model is read from build/models/birefnet-hr-matting.

    build/hrmatte-venv/Scripts/python.exe tools/scripts/sprites/hr_matte.py <video.mp4> <out dir> <first> <last> [--crop x0,y0,x1,y1]
        [--size 2048] [--threads 12]

--crop: the model sees only this part of the frame (the figure gets the whole 2048 px); the alpha is written at the frame's size,
clear outside the crop.
"""
import argparse
import os
import subprocess
import time

import numpy as np
import torch
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
MODEL = os.path.join(ROOT, "build", "models", "birefnet-hr-matting")
MEAN = np.array([0.485, 0.456, 0.406], np.float32)
STD = np.array([0.229, 0.224, 0.225], np.float32)


def video_size(path):
    out = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries", "stream=width,height",
                          "-of", "csv=p=0", path], capture_output=True, text=True, check=True).stdout.strip()
    w, h = out.split(",")[:2]
    return int(w), int(h)


def read_frames(path, first, last):
    """Every frame first..last, decoded in order (no seeking, so the numbering is the decoder's own)."""
    w, h = video_size(path)
    p = subprocess.Popen(["ffmpeg", "-v", "error", "-i", path, "-f", "rawvideo", "-pix_fmt", "rgb24", "-"], stdout=subprocess.PIPE)
    size = w * h * 3
    i = 0
    try:
        while i <= last:
            buf = p.stdout.read(size)
            if len(buf) < size:
                break
            if i >= first:
                yield i, np.frombuffer(buf, np.uint8).reshape(h, w, 3)
            i += 1
    finally:
        p.stdout.close()
        p.wait()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("video")
    ap.add_argument("out")
    ap.add_argument("first", type=int)
    ap.add_argument("last", type=int)
    ap.add_argument("--crop", default="")
    ap.add_argument("--size", type=int, default=2048)
    ap.add_argument("--threads", type=int, default=12)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    torch.set_num_threads(a.threads)
    from transformers import AutoModelForImageSegmentation
    model = AutoModelForImageSegmentation.from_pretrained(MODEL, trust_remote_code=True)
    model.float()  # its weights ship in half precision; the CPU runs full precision
    model.eval()
    t0 = time.time()
    done = 0
    for i, rgb in read_frames(a.video, a.first, a.last):
        src = os.path.join(a.out, f"src-{i:04d}.png")
        alpha = os.path.join(a.out, f"alpha-{i:04d}.png")
        if os.path.exists(alpha) and os.path.exists(src):
            continue
        im = Image.fromarray(rgb)
        box = tuple(int(v) for v in a.crop.split(",")) if a.crop else (0, 0, im.width, im.height)
        part = im.crop(box)
        x = np.asarray(part.resize((a.size, a.size), Image.BICUBIC), np.float32) / 255
        x = torch.from_numpy(((x - MEAN) / STD).transpose(2, 0, 1)[None].copy())
        with torch.inference_mode():
            y = model(x)[-1].sigmoid()[0, 0].numpy()
        m = Image.fromarray((np.clip(y, 0, 1) * 255 + 0.5).astype(np.uint8)).resize(part.size, Image.BICUBIC)
        if a.crop:
            full = Image.new("L", im.size, 0)
            full.paste(m, box[:2])
            m = full
        im.save(src)
        m.save(alpha + ".tmp.png")
        os.replace(alpha + ".tmp.png", alpha)  # the alpha last: a frame counts as done only when both are whole
        done += 1
        print(f"{a.out}: frame {i} ({done} done, {(time.time() - t0) / done:.1f} s a frame)", flush=True)


if __name__ == "__main__":
    main()
