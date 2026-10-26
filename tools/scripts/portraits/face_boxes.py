# The face box of each portrait, for content/hero-portraits.json "face" (centre x, centre y, height as shares of the picture).
# Usage: python face_boxes.py <yunet.onnx> <sheet.png> <portrait>...   (OpenCV 5.0.0.93, opencv-python-headless; the model is
# face_detection_yunet_2023mar.onnx from github.com/opencv/opencv_zoo models/face_detection_yunet). The sheet marks each box.
import json, sys
import cv2
import numpy as np
from PIL import Image, ImageDraw

MODEL = sys.argv[1]
OUT = sys.argv[2]
files = sys.argv[3:]
det = cv2.FaceDetectorYN.create(MODEL, "", (320, 320), 0.6, 0.3, 5000)
result = {}
thumbs = []
for f in files:
    img = cv2.imdecode(np.fromfile(f, dtype=np.uint8), cv2.IMREAD_COLOR)
    h, w = img.shape[:2]
    s = 640 / max(w, h)
    small = cv2.resize(img, (round(w * s), round(h * s)))
    det.setInputSize((small.shape[1], small.shape[0]))
    _, faces = det.detect(small)
    best = None
    if faces is not None:
        best = max(faces, key=lambda r: r[2] * r[3] * r[14])
    pil = Image.open(f).convert("RGB")
    d = ImageDraw.Draw(pil)
    if best is not None:
        x, y, bw, bh = [float(v) / s for v in best[:4]]
        eyes_y = float((best[5] + best[7]) / 2) / s
        result[f] = {"cx": round((x + bw / 2) / w, 4), "cy": round((y + bh / 2) / h, 4), "h": round(bh / h, 4),
                     "eyesY": round(eyes_y / h, 4), "score": round(float(best[14]), 3), "size": [w, h]}
        d.rectangle((x, y, x + bw, y + bh), outline=(0, 255, 0), width=max(2, w // 150))
    else:
        result[f] = None
    pil.thumbnail((240, 360))
    thumbs.append(pil)
sheet = Image.new("RGB", (240 * len(thumbs), 360), (20, 20, 20))
for i, t in enumerate(thumbs):
    sheet.paste(t, (i * 240, 0))
sheet.save(OUT)
print(json.dumps(result, indent=1))
