"""Proxy-quality numbers from Sandbox --compare images (build/compare by default).

For each width: silhouette agreement with the full-detail reference (intersection over union of the
covered pixels) and the mean / 95th-percentile angle between normals on pixels both cover, for the
geometric normals and the shading normals (normal map applied). Usage: python compare_meshes.py [dir]
"""
import math
import sys
from pathlib import Path

from PIL import Image


def load(path):
    img = Image.open(path).convert('RGB')
    return img.size, list(img.get_flattened_data() if hasattr(img, "get_flattened_data") else img.getdata())


def normal(rgb):
    x, y, z = (c / 255.0 * 2.0 - 1.0 for c in rgb)
    length = math.sqrt(x * x + y * y + z * z) or 1.0
    return x / length, y / length, z / length


def compare(ref_path, test_path):
    (w, h), ref = load(ref_path)
    _, test = load(test_path)
    covered_ref = [p != (0, 0, 0) for p in ref]
    covered_test = [p != (0, 0, 0) for p in test]
    both = [i for i in range(w * h) if covered_ref[i] and covered_test[i]]
    union = sum(1 for i in range(w * h) if covered_ref[i] or covered_test[i])
    iou = len(both) / union if union else 1.0
    angles = []
    for i in both:
        a, b = normal(ref[i]), normal(test[i])
        angles.append(math.degrees(math.acos(max(-1.0, min(1.0, sum(p * q for p, q in zip(a, b)))))))
    angles.sort()
    mean = sum(angles) / len(angles) if angles else 0.0
    p95 = angles[int(0.95 * (len(angles) - 1))] if angles else 0.0
    return sum(covered_ref), iou, mean, p95


def main():
    folder = Path(sys.argv[1] if len(sys.argv) > 1 else 'build/compare')
    print(f"{'width':>6} {'variant':>9} {'pixels':>7} {'silhouette IoU':>15} {'geometric normal':>22} {'shading normal':>22}")
    for width in (400, 200, 100, 50, 25):
        for variant in ('original', 'proxy'):
            px, iou, gm, g95 = compare(folder / f'{width}px_reference_geometric.png', folder / f'{width}px_{variant}_geometric.png')
            _, _, sm, s95 = compare(folder / f'{width}px_reference_shading.png', folder / f'{width}px_{variant}_shading.png')
            print(f"{width:>5}px {variant:>9} {px:>7} {iou:>15.3f} {gm:>8.1f} deg (p95 {g95:>5.1f}) {sm:>8.1f} deg (p95 {s95:>5.1f})")


if __name__ == '__main__':
    main()
