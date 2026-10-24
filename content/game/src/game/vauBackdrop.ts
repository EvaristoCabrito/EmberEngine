/** Shared world-space framing of O Vau's painted river and camera limits. */
export function vauBackdropBounds(tile: number, cols: number, viewW: number, viewH: number, imageRatio: number) {
  const boardWidth = tile * Math.sqrt(3) * cols;
  const width = Math.max(boardWidth * 1.35, viewW, viewH * imageRatio);
  const height = width / imageRatio;
  // Match the river in map rows 5–6 to the water band 56% down the panorama.
  const centerY = tile * (2.4 + 1.5 * 5.5 + 1) - 0.06 * height;
  return { width, height, left: (boardWidth - width) / 2, top: centerY - height / 2 };
}
