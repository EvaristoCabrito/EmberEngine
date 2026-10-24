export interface DecorationAnchor {
  /** Opaque base's horizontal 5th–95th percentile span, normalized to the image. */
  u0: number;
  u1: number;
  /** Lowest opaque pixel row, normalized to the image. */
  v: number;
}

const anchorCache = new WeakMap<HTMLImageElement, DecorationAnchor | null>();

/** Measures the actual visible base rather than trusting transparent crop padding. This lets
 * refreshed cutouts sit on the same map point even when their cleaner exports use new margins. */
export function decorationAnchor(img: HTMLImageElement): DecorationAnchor | null {
  if (anchorCache.has(img)) return anchorCache.get(img)!;
  let result: DecorationAnchor | null = null;
  try {
    const scale = Math.min(1, 256 / Math.max(img.naturalWidth, img.naturalHeight));
    const w = Math.max(1, Math.round(img.naturalWidth * scale));
    const h = Math.max(1, Math.round(img.naturalHeight * scale));
    const canvas = document.createElement("canvas");
    canvas.width = w;
    canvas.height = h;
    const context = canvas.getContext("2d", { willReadFrequently: true });
    if (!context) throw new Error("Canvas unavailable");
    context.drawImage(img, 0, 0, w, h);
    const pixels = context.getImageData(0, 0, w, h).data;
    let bottom = -1;
    for (let y = h - 1; y >= 0 && bottom < 0; y--) {
      for (let x = 0; x < w; x++) if (pixels[(y * w + x) * 4 + 3]! > 128) { bottom = y; break; }
    }
    if (bottom >= 0) {
      const top = Math.max(0, bottom - Math.max(1, Math.round(h * 0.04)));
      const columns = new Float64Array(w);
      let total = 0;
      for (let y = top; y <= bottom; y++) for (let x = 0; x < w; x++) {
        const alpha = pixels[(y * w + x) * 4 + 3]!;
        if (alpha > 128) { columns[x] += alpha; total += alpha; }
      }
      let accumulated = 0;
      let x0 = 0;
      let x1 = w - 1;
      for (let x = 0; x < w; x++) {
        const previous = accumulated;
        accumulated += columns[x]!;
        if (previous < total * 0.05 && accumulated >= total * 0.05) x0 = x;
        if (previous < total * 0.95 && accumulated >= total * 0.95) x1 = x;
      }
      result = { u0: x0 / w, u1: (x1 + 1) / w, v: (bottom + 1) / h };
    }
  } catch {
    result = null;
  }
  anchorCache.set(img, result);
  return result;
}
