import assert from "node:assert/strict";
import { test } from "node:test";
import { sampleWaterPatches, type WaterPatch } from "./gfx/three/ThreeWater.ts";

const square = (x: number, size: number): WaterPatch => ({ x, y: 6, size, shape: "square", level: 0.5 });
test("small water strokes preserve the full-size water underneath", () => {
  const large = square(5, 1), small = square(5, 0.25);
  const before = sampleWaterPatches(5.7, 6, [large]);
  assert.deepEqual(sampleWaterPatches(5.7, 6, [large, small]), before);
});
test("closely spaced strokes join independently of hex centers", () => {
  const patches = [square(5.1, 0.25), square(5.4, 0.25)];
  assert.equal(sampleWaterPatches(5.25, 6, patches).coverage, 1);
  assert.equal(sampleWaterPatches(5.25, 6, patches).level, 0.5);
});
test("an added stroke extends the existing water surface", () => {
  const large = square(5, 1), extension = square(6.4, 0.25);
  assert.equal(sampleWaterPatches(6.4, 6, [large]).coverage, 0);
  assert.equal(sampleWaterPatches(6.4, 6, [large, extension]).coverage, 1);
});
