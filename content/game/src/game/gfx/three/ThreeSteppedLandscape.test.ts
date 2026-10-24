import { test } from "node:test";
import assert from "node:assert/strict";
import { buildSteppedHexLandscape } from "./ThreeSteppedLandscape.ts";

test("terraces preserve exact top heights and expose only unequal shared edges", () => {
  const radius=10, bounds={minX:0,minY:-60,maxX:60,maxY:0};
  const left={x:Math.sqrt(3)*radius*.5,y:-3.4*radius,height:13};
  const right={x:Math.sqrt(3)*radius*1.5,y:left.y,height:26};
  const surface=buildSteppedHexLandscape(bounds,[left,right],radius,4.5);
  assert.equal(surface.heightAt(left.x,left.y),13);
  assert.equal(surface.heightAt(right.x,right.y),26);
  const position=surface.geometry.getAttribute("position"), index=surface.geometry.index!;
  for(let i=0;i<surface.geometry.groups[0].count;i+=3) {
    const heights=[0,1,2].map(k=>position.getZ(index.getX(i+k)));
    assert.equal(heights[0],heights[1]); assert.equal(heights[1],heights[2]);
  }
  // Ten exterior faces and one full-height internal step, two triangles per face.
  assert.equal(surface.geometry.groups[1].count,66);
  const plateau=buildSteppedHexLandscape(bounds,[left,{...right,height:13}],radius,4.5);
  assert.equal(plateau.geometry.groups[1].count,60);
  surface.geometry.dispose(); plateau.geometry.dispose();
});
