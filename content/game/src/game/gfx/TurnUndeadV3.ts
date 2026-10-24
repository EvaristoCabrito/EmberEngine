import type { WebGL2DRenderer } from "./WebGL2DRenderer";
import type { DivineLightCell } from "./TurnUndeadV1";

export const TURN_UNDEAD_V3_DURATION = .5;
function hexPath(ctx: WebGL2DRenderer, cell: DivineLightCell, scale = 1): void {
  ctx.beginPath();cell.corners.forEach(([x,y],i)=>{const px=cell.x+(x-cell.x)*scale,py=cell.y+(y-cell.y)*scale;i?ctx.lineTo(px,py):ctx.moveTo(px,py);});ctx.closePath();
}

/** Separate, unapproved V3 flash preview. Uses drawPotionBurst's soft additive aura,
 * orbiting motes and rising particles, with the healing FX's cream/gold palette.
 * The area is masked by real hex cells; original healing/potion FX remain untouched. */
export function drawTurnUndeadV3(ctx: WebGL2DRenderer, cells: readonly DivineLightCell[], elapsed: number): void {
  if(elapsed<0 || elapsed>=TURN_UNDEAD_V3_DURATION)return;
  const k=elapsed/TURN_UNDEAD_V3_DURATION;
  const fade=Math.min(1,elapsed/.025)*Math.pow(Math.max(0,1-elapsed/.32),1.5);
  if(fade<=0)return;
  ctx.save();ctx.globalCompositeOperation="lighter";
  for(let index=0;index<cells.length;index++){
    const cell=cells[index]!;const cx=cell.x,cy=cell.y;
    const tile=Math.max(...cell.corners.map(([x,y])=>Math.hypot(x-cx,y-cy)));
    const seed=index*1.791;const pulse=.88+.12*Math.abs(Math.sin(elapsed*7+seed));
    ctx.save();hexPath(ctx,cell);ctx.clip();
    // Same soft, hot-center light falloff as the existing healing and potion aura.
    const aura=ctx.createRadialGradient(cx,cy-tile*.12,0,cx,cy-tile*.12,tile*1.45);
    aura.addColorStop(0,`rgba(255,250,220,${.72*fade*pulse})`);
    aura.addColorStop(.35,`rgba(255,210,90,${.38*fade})`);
    aura.addColorStop(1,'rgba(255,210,90,0)');
    ctx.fillStyle=aura;hexPath(ctx,cell);ctx.fill();
    // Potion's twelve orbiting motes, in the priest's existing divine-light colors.
    for(let i=0;i<12;i++){
      const ang=seed+i*.52+elapsed*(2.8+(i%3)*.4);
      const rad=tile*(.18+(i%4)*.06)*(.85+Math.sin(elapsed*6+i)*.12);
      const px=cx+Math.cos(ang)*rad,py=cy+Math.sin(ang)*rad*.72-tile*.08*Math.sin(elapsed*5+i);
      const r=tile*(.04+(i%3)*.012)*fade;
      ctx.fillStyle=i%3===0?`rgba(255,250,220,${.9*fade})`:`rgba(255,210,90,${.75*fade})`;
      ctx.shadowColor=`rgba(255,220,140,${.8*fade})`;ctx.shadowBlur=tile*.14;
      ctx.beginPath();ctx.arc(px,py,Math.max(.8,r),0,Math.PI*2);ctx.fill();
    }
    ctx.shadowBlur=0;
    // The short impact flash follows the exact affected hexes and is gone by 0.32 s.
    const ground=ctx.createRadialGradient(cx,cy,0,cx,cy,tile);
    ground.addColorStop(0,`rgba(255,248,220,${.4*fade})`);ground.addColorStop(1,'rgba(255,210,90,0)');
    ctx.fillStyle=ground;hexPath(ctx,cell,Math.min(1,.28+k*.9));ctx.fill();
    for(let i=0;i<8;i++){
      const rise=((seed+i*.21+k*1.2)%1);
      ctx.fillStyle=`rgba(255,250,220,${(.7-rise*.4)*fade})`;
      ctx.beginPath();ctx.arc(cx+Math.sin(seed+i*2)*tile*.22,cy+tile*.4-rise*tile*1.05,Math.max(.5,tile*.028*fade),0,Math.PI*2);ctx.fill();
    }
    ctx.restore();
  }
  ctx.restore();
}
