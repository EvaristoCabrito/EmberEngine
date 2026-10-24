import assert from "node:assert/strict";
import { test } from "node:test";
import { elevationFaces } from "./elevationSteps.ts";

test("flat ground has no terrace faces", () => {
  assert.deepEqual(elevationFaces(0, [0, 0, 0, 0, 0, 0]), []);
});
test("equal-height neighbors form a plateau without internal cliff faces", () => {
  assert.deepEqual(elevationFaces(4, [4, 4, 4, 4, 4, 4]), []);
});
test("only lower neighbors expose faces, with tiers from the height difference", () => {
  const faces = elevationFaces(5, [5, 3, 0, 6, 5, 4]);
  assert.deepEqual(faces.map(face => [face.edge, face.steps]), [[1, 2], [2, 5], [5, 1]]);
  assert(faces.every(face => face.depth > 0 && face.depth <= 0.3));
});
test("board edges expose ground-facing terraces and extreme levels keep bounded relief", () => {
  const faces = elevationFaces(12, []);
  assert.equal(faces.length, 6);
  assert(faces.every(face => face.depth === 0.3 && face.steps === 6));
  assert.deepEqual(elevationFaces(NaN, []), []);
});
