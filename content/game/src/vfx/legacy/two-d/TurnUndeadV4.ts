import type { WebGL2DRenderer } from "./WebGL2DRenderer";
import type { DivineLightCell } from "./TurnUndeadV1";
export const TURN_UNDEAD_V4_DURATION = .3;

/** One transient area-wide flash using the existing healing aura's hot cream core,
 * gold falloff and additive blend. Each draw samples the same single light field;
 * the exact affected cells clip it, without per-hex lights or a persistent zone. */
export function drawTurnUndeadV4(ctx: WebGL2DRenderer, cells: readonly DivineLightCell[], elapsed: number): void {
  if(!cells.length || elapsed<=0 || elapsed>=TURN_UNDEAD_V4_DURATION)return;
  const fade=Math.min(1,elapsed/.02)*Math.pow(1-elapsed/TURN_UNDEAD_V4_DURATION,1.3);
  const cx=cells.reduce((v,p)=>v+p.x,0)/cells.length,cy=cells.reduce((v,p)=>v+p.y,0)/cells.length;
  const radius=Math.max(...cells.flatMap(cell=>cell.corners.map(([x,y])=>Math.hypot(x-cx,y-cy))));
  const path=(cell:DivineLightCell)=>{ctx.beginPath();cell.corners.forEach(([x,y],i)=>i?ctx.lineTo(x,y):ctx.moveTo(x,y));ctx.closePath();};
  ctx.save();ctx.globalCompositeOperation="lighter";
  const light=ctx.createRadialGradient(cx,cy,0,cx,cy,radius*1.15);
  light.addColorStop(0,`rgba(255,250,220,${.98*fade})`);
  light.addColorStop(.35,`rgba(255,232,160,${.8*fade})`);
  light.addColorStop(.72,`rgba(255,210,90,${.42*fade})`);
  light.addColorStop(1,'rgba(255,210,90,0)');
  cells.forEach((cell,index)=>{
    ctx.save();path(cell);ctx.clip();ctx.fillStyle=light;path(cell);ctx.fill();
    // Sparse healing-style motes scattered through one flash, never an orbit or a ring.
    for(let i=0;i<2;i++){
      const seed=index*13.17+i*73.51;
      const r=Math.max(...cell.corners.map(([x,y])=>Math.hypot(x-cell.x,y-cell.y)));
      const x=cell.x+Math.sin(seed)*r*.7,y=cell.y+Math.cos(seed*1.7)*r*.6-elapsed*r*.3;
      ctx.strokeStyle=`rgba(255,250,230,${fade*.85})`;ctx.lineWidth=1.5;
      ctx.beginPath();ctx.moveTo(x,y);ctx.lineTo(x,y-r*.055);ctx.stroke();
    }
    ctx.restore();
  });ctx.restore();
}
