"""Video -> sprite, step 1: every native frame of a stretch of a video and its alpha matte.

BiRefNet (general, 1024 x 1024, ONNX on the CPU) on each frame; the model is the one rembg 2.0.85 (the latest rembg
release) downloads to ~/.rembg/models/birefnet-general/. Writes <out>/src-NNNN.png (the video frame, RGB) and
<out>/alpha-NNNN.png (8-bit) for frames first..last (0-based, the video's own numbering). Resumable: frames already
written are skipped, so a run cut short (power) carries on where it stopped.

    python -I tools/scripts/sprites/video_matte.py <video.mp4> <out dir> <first> <last> [--threads N]
"""
import argparse
import os
import subprocess
import time

import numpy as np
import onnxruntime as ort
from PIL import Image

MODEL = os.path.expanduser("~/.rembg/models/birefnet-general/birefnet-general.onnx")
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
    p = subprocess.Popen(["ffmpeg", "-v", "error", "-i", path, "-f", "rawvideo", "-pix_fmt", "rgb24", "-"],
                         stdout=subprocess.PIPE)
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
    ap.add_argument("--threads", type=int, default=12)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    so = ort.SessionOptions()
    so.intra_op_num_threads = a.threads
    sess = ort.InferenceSession(MODEL, so, providers=["CPUExecutionProvider"])
    name = sess.get_inputs()[0].name
    t0 = time.time()
    done = 0
    for i, rgb in read_frames(a.video, a.first, a.last):
        src = os.path.join(a.out, f"src-{i:04d}.png")
        alpha = os.path.join(a.out, f"alpha-{i:04d}.png")
        if os.path.exists(alpha) and os.path.exists(src):
            continue
        im = Image.fromarray(rgb)
        x = np.asarray(im.resize((1024, 1024), Image.BICUBIC), np.float32) / 255
        x = ((x - MEAN) / STD).transpose(2, 0, 1)[None]
        y = sess.run(None, {name: x})[0][0, 0]
        m = 1 / (1 + np.exp(-np.clip(y, -40, 40)))
        m = Image.fromarray((m * 255 + 0.5).astype(np.uint8)).resize(im.size, Image.BICUBIC)
        im.save(src)
        m.save(alpha + ".tmp.png")
        os.replace(alpha + ".tmp.png", alpha)  # the alpha last: a frame counts as done only when both are whole
        done += 1
        print(f"{a.out}: frame {i} ({done} done, {(time.time() - t0) / done:.1f} s a frame)", flush=True)


if __name__ == "__main__":
    main()
