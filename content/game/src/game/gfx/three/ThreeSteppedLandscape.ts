import * as THREE from "three";
import type { LandscapeSurface } from "./ThreeLandscape";

/** Solid terraces: each authored cell has a level top, and only height differences
 * expose vertical rock faces. Equal-height neighbors share an uninterrupted plateau. */
export function buildSteppedHexLandscape(
  bounds: { minX: number; minY: number; maxX: number; maxY: number },
  cells: { x: number; y: number; height: number }[], radius: number, baseDepth: number,
): LandscapeSurface {
  const positions: number[] = [], uv: number[] = [], top: number[] = [], sides: number[] = [];
  const edges = new Map<string, { a: number[]; b: number[]; height: number }[]>();
  const vertex = (x: number, y: number, z: number, u: number, v: number) => {
    const index = positions.length / 3;
    positions.push(x, y, z); uv.push(u, v); return index;
  };
  const groundVertex = (x: number, y: number, z: number) => vertex(x, y, z,
    (x-bounds.minX)/(bounds.maxX-bounds.minX), (y-bounds.minY)/(bounds.maxY-bounds.minY));
  const pointKey = (p: number[]) => `${Math.round(p[0]*1e5)},${Math.round(p[1]*1e5)}`;
  for (const cell of cells) {
    const center = groundVertex(cell.x, cell.y, cell.height);
    const ring = Array.from({ length: 6 }, (_, i) => {
      const angle = (i*60-30)*Math.PI/180;
      return [cell.x+Math.cos(angle)*radius, cell.y+Math.sin(angle)*radius];
    });
    const indices = ring.map(p => groundVertex(p[0], p[1], cell.height));
    for (let i=0; i<6; i++) {
      top.push(center, indices[i], indices[(i+1)%6]);
      const a=ring[i], b=ring[(i+1)%6], key=[pointKey(a), pointKey(b)].sort().join(":");
      const edge=edges.get(key) ?? []; edge.push({a,b,height:cell.height}); edges.set(key,edge);
    }
  }
  for (const adjoining of edges.values()) {
    const high=adjoining.reduce((a,b)=>a.height>b.height?a:b);
    const low=adjoining.length===1 ? -baseDepth : Math.min(...adjoining.map(e=>e.height));
    if (high.height-low<1e-6) continue;
    const {a,b}=high, width=Math.hypot(b[0]-a[0],b[1]-a[1])/radius;
    const start=vertex(a[0],a[1],high.height,0,high.height/radius);
    vertex(b[0],b[1],high.height,width,high.height/radius);
    vertex(a[0],a[1],low,0,low/radius);
    vertex(b[0],b[1],low,width,low/radius);
    sides.push(start,start+2,start+1,start+1,start+2,start+3);
  }
  const geometry=new THREE.BufferGeometry();
  geometry.setAttribute("position",new THREE.Float32BufferAttribute(positions,3));
  geometry.setAttribute("uv",new THREE.Float32BufferAttribute(uv,2));
  geometry.setIndex([...top,...sides]); geometry.addGroup(0,top.length,0); geometry.addGroup(top.length,sides.length,1);
  geometry.computeVertexNormals(); geometry.computeBoundingSphere();
  const centers=new Map(cells.map(c=>[pointKey([c.x,c.y]),c]));
  const heightAt=(x:number,y:number) => {
    // Restrict the search to the closest three rows and columns of the staggered grid.
    const row=Math.round((-y/radius-3.4)/1.5);
    let nearest: typeof cells[number] | undefined, distance=Infinity;
    for(let r=row-1;r<=row+1;r++) {
      const col=Math.round(x/(radius*Math.sqrt(3))-0.5*(r&1)-0.5);
      for(let c=col-1;c<=col+1;c++) {
        const cell=centers.get(pointKey([radius*Math.sqrt(3)*(c+0.5*(r&1)+0.5),-radius*(3.4+1.5*r)]));
        if(!cell) continue;
        const d=(x-cell.x)**2+(y-cell.y)**2;
        if(d<distance){distance=d;nearest=cell;}
      }
    }
    return nearest?.height ?? 0;
  };
  return {geometry,heightAt,...bounds};
}
