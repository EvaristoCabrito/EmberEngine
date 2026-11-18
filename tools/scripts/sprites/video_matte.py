"""Video -> sprite, step 1: every native frame of a stretch of a video and its alpha matte.

BiRefNet (general, 1024 x 1024, ONNX on the CPU) on each frame; the model is the one rembg 2.0.85 (the latest rembg
release) downloads to ~/.rembg/models/birefnet-general/. Writes <out>/src-NNNN.png (the video frame, RGB) and
<out>/alpha-NNNN.png (8-bit) for frames first..last (0-based, the video's own numbering). Resumable: frames already
written are skipped, so a run cut short (power) carries on where it stopped.

    python -I tools/scripts/sprites/video_matte.py <video.mp4> <out dir> <first> <last> [--threads N] [--model lite]

--model lite = BiRefNet general lite (rembg's birefnet-general-lite, same input size, a quarter of the weights; the user, 2026-10-22,
for the Giant Spider: "try again on light mode").

--device (the user, 2026-11-17: "why are we not using the graphics card at all"): cpu, dml (the graphics card through DirectML) or
auto (the default): the graphics card once build/ort-dml/GPU-OK is there (written after a test frame matched the CPU's), else the CPU.
DirectML lives in its own environment (build/ort-dml: the system one's onnxruntime files are held open by the running mattes), so a
run started with the system Python carries itself over to that environment's Python; if the card cannot run the model, the CPU.
"""
import argparse
import os
import subprocess
import sys
import time

import numpy as np
import onnxruntime as ort
from PIL import Image

MODELS = {
    "general": os.path.expanduser("~/.rembg/models/birefnet-general/birefnet-general.onnx"),
    "lite": os.path.expanduser("~/.rembg/models/birefnet-general-lite/birefnet-general-lite.onnx"),
}
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
DML_PY = os.path.join(ROOT, "build", "ort-dml", "Scripts", "python.exe")
DML_OK = os.path.join(ROOT, "build", "ort-dml", "GPU-OK")
MEAN = np.array([0.485, 0.456, 0.406], np.float32)
STD = np.array([0.229, 0.224, 0.225], np.float32)


def earlier_mattes():
    """The other video_matte.py processes started before this one (read-only: listed, never touched)."""
    me = os.getpid()
    ps = ("Get-CimInstance Win32_Process -Filter \"Name='python.exe'\" | ? { $_.CommandLine -match 'video_matte\\.py' } | "
          "% { '' + $_.ProcessId + ' ' + $_.ParentProcessId + ' ' + $_.CreationDate.ToString('yyyyMMddHHmmss.fffffff') }")
    rows = [r.split() for r in subprocess.run(["powershell", "-NoProfile", "-Command", ps], capture_output=True, text=True).stdout.split("\n")
            if len(r.split()) == 3]
    mine = next((r[2] for r in rows if r[0] == str(me)), None)
    if mine is None:
        return []
    # a matte's own card child (gpu_run) has it as parent: not another matte
    return [r for r in rows if r[0] not in (str(me), str(os.getppid())) and r[1] != str(me) and (r[2], int(r[0])) < (mine, me)]


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


FIRST_FRAME_LIMIT = 180  # s: the graphics card's run must finish its first frame by then (loading the model on it included)
CPU_FALLBACK_THREADS = 2  # only when the card fails outright (the user, 2026-11-17: "burn this video card to the max ... leave my CPU alone")


def gpu_run(cmd):
    """The run on the graphics card (DirectML's own Python), watched: its lines passed on; stopped only when it has no frame done
    within FIRST_FRAME_LIMIT or fails (never for being slow: the card works flat out, the CPU stays free). True = it finished the
    whole stretch. Never two at once: this process only waits while it runs, and frames it finished are kept (the CPU skips them)."""
    import re
    import threading
    child = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
    first = threading.Event()

    def watch():
        for line in child.stdout:
            print(line.rstrip(), flush=True)
            m = re.search(r"\((\d+) done, ([\d.]+) s a frame\)", line)
            if m:
                first.set()

    t = threading.Thread(target=watch, daemon=True)
    t.start()
    if not first.wait(FIRST_FRAME_LIMIT):
        print(f"graphics card: no frame in {FIRST_FRAME_LIMIT} s", flush=True)
        child.kill()
    code = child.wait()
    t.join(5)
    return code == 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("video")
    ap.add_argument("out")
    ap.add_argument("first", type=int)
    ap.add_argument("last", type=int)
    ap.add_argument("--threads", type=int, default=12)
    ap.add_argument("--model", choices=sorted(MODELS), default="general")
    ap.add_argument("--device", choices=["auto", "cpu", "dml"], default="auto")
    # --crop x0,y0,x1,y1 (the user, 2026-11-17, the Cão Zumbi's idle: "cutting over"): the model sees only this part of the frame,
    # so a figure much smaller than the frame gets the model's whole 1024 px (the teeth no longer a few pixels); the alpha is written
    # at the frame's size, clear outside the crop
    ap.add_argument("--crop", default="")
    a = ap.parse_args()
    # one matte at a time (the user, 2026-11-17: "move on to 12 cores in a single process as soon as possible"): however many queue
    # lanes call it, a matte waits until every video_matte started before it has ended (first come, first served), then only does the
    # frames still missing, so two never run together and none is done twice
    os.makedirs(a.out, exist_ok=True)
    while earlier_mattes():
        time.sleep(30)
    # build/video-cut/matte-threads.txt (the user's to set, read when the matte's turn comes): the CPU threads, whatever the queue
    # started with, so a change never needs a running queue stopped (2026-11-17: "dont stop any processes ever")
    setting = os.path.join(ROOT, "build", "video-cut", "matte-threads.txt")
    if os.path.exists(setting) and open(setting).read().strip().isdigit():
        a.threads = int(open(setting).read().strip())
    # a stretch already whole (a queue run again after a stop): nothing to load, nothing to do
    frames = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries", "stream=nb_frames", "-of", "csv=p=0",
                             a.video], capture_output=True, text=True).stdout.strip()
    last = min(a.last, int(frames) - 1) if frames.isdigit() else a.last
    if all(os.path.exists(os.path.join(a.out, f"{k}-{i:04d}.png")) for i in range(a.first, last + 1) for k in ("src", "alpha")):
        print(f"{a.out}: frames {a.first}-{last} already done", flush=True)
        return
    if a.device == "auto":
        a.device = "dml" if os.path.exists(DML_OK) else "cpu"
    if a.device == "dml" and "DmlExecutionProvider" not in ort.get_available_providers():
        if os.path.exists(DML_PY) and os.path.normcase(os.path.abspath(sys.executable)) != os.path.normcase(DML_PY):
            if gpu_run([DML_PY, "-I", os.path.abspath(__file__), *sys.argv[1:], "--device", "dml"]):
                return
            print(f"{a.out}: the graphics card failed; the rest on the CPU at {CPU_FALLBACK_THREADS} threads", flush=True)
            a.threads = min(a.threads, CPU_FALLBACK_THREADS)
        a.device = "cpu"
    os.makedirs(a.out, exist_ok=True)
    so = ort.SessionOptions()
    so.intra_op_num_threads = 1 if a.device == "dml" else a.threads  # on the card: the CPU only for the small steps around it
    sess = None
    if a.device == "dml":
        so.enable_mem_pattern = False
        so.execution_mode = ort.ExecutionMode.ORT_SEQUENTIAL
        try:
            sess = ort.InferenceSession(MODELS[a.model], so, providers=["DmlExecutionProvider", "CPUExecutionProvider"])
            print(f"{a.out}: on the graphics card (DirectML)", flush=True)
        except Exception as e:  # the card cannot hold the model: the CPU (the watching run carries on there)
            print(f"{a.out}: graphics card failed ({str(e)[:120]})", flush=True)
            sys.exit(3)
    if sess is None:
        sess = ort.InferenceSession(MODELS[a.model], so, providers=["CPUExecutionProvider"])
    name = sess.get_inputs()[0].name
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
        x = np.asarray(part.resize((1024, 1024), Image.BICUBIC), np.float32) / 255
        x = ((x - MEAN) / STD).transpose(2, 0, 1)[None]
        y = sess.run(None, {name: x})[0][0, 0]
        m = 1 / (1 + np.exp(-np.clip(y, -40, 40)))
        m = Image.fromarray((m * 255 + 0.5).astype(np.uint8)).resize(part.size, Image.BICUBIC)
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
