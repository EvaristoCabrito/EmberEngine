import { createProgram, createUnitQuad, bindAttrib } from "./glutil";
import type { BlizzardCell, BlizzardAnchor } from "./Blizzard2D";
export interface Frost2DConfig {
  origin: BlizzardCell;
  cells: BlizzardCell[];
  seed?: number;
  onImpact?: (cell: BlizzardCell) => void;
}
const V = `#version 300 es
precision highp float;layout(location=0)in vec2 a_pos;uniform vec2 uCenter;uniform vec2 uSize;uniform vec2 uResolution;uniform float uAngle;out vec2 vUv;void main(){vUv=a_pos*.5+.5;float c=cos(uAngle),s=sin(uAngle);vec2 p=uCenter+mat2(c,-s,s,c)*(a_pos*uSize);gl_Position=vec4(p.x/uResolution.x*2.-1.,1.-p.y/uResolution.y*2.,0.,1.);}`;
const F = `#version 300 es
precision highp float;in vec2 vUv;uniform sampler2D uSprite;uniform float uAlpha;uniform float uMode;uniform vec3 uColor;out vec4 fragColor;void main(){
if(uMode>.5){float d=length(vUv*2.-1.);float a=1.-smoothstep(0.,1.,d);a*=a*uAlpha;if(a<.005)discard;fragColor=vec4(uColor,a);return;}
vec2 uv=vec2(.025+vUv.x*.95,.32+vUv.y*.32);vec3 col=texture(uSprite,uv).rgb;float a=smoothstep(.035,.22,max(col.r,max(col.g,col.b)))*uAlpha;if(a<.005)discard;fragColor=vec4(col,a);}`;
/** Ember's short forward ice jet (the shard and its splinters, unchanged) in the existing raw
 * WebGL2 alpha/bloom passes, plus ice glints trailing the shard and, on every struck hex, a cold
 * mist and a burst of ice sparkles (soft discs, uMode 1). Its light is the overlay's impact pulse. */
export class Frost2D {
  private program: WebGLProgram;
  private quad: WebGLBuffer;
  private texture: WebGLTexture;
  private age = 0;
  private loaded = false;
  private disposed = false;
  private hits = new Set<number>();
  constructor(
    private gl: WebGL2RenderingContext,
    readonly config: Frost2DConfig,
  ) {
    this.program = createProgram(gl, V, F);
    this.quad = createUnitQuad(gl);
    this.texture = gl.createTexture()!;
    gl.bindTexture(gl.TEXTURE_2D, this.texture);
    gl.texImage2D(
      gl.TEXTURE_2D,
      0,
      gl.RGBA,
      1,
      1,
      0,
      gl.RGBA,
      gl.UNSIGNED_BYTE,
      new Uint8Array([0, 0, 0, 0]),
    );
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    const image = new Image();
    image.onload = () => {
      if (this.disposed) return;
      gl.bindTexture(gl.TEXTURE_2D, this.texture);
      gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, image);
      this.loaded = true;
    };
    image.src = "/game/icons/frost.jpg";
  }
  get finished() {
    // the shard is gone at 1.5 s; the last hex's mist fades out by 2.2 s
    return this.age >= 2.2;
  }
  draw(dt: number, w: number, h: number, get: (cell: BlizzardCell) => BlizzardAnchor) {
    if (this.disposed) return;
    this.age += Math.max(0, dt);
    if (!this.loaded || this.finished || !this.config.cells.length) return;
    const gl = this.gl,
      from = get(this.config.origin),
      to = get(this.config.cells.at(-1)!);
    const dx = to.x - from.x,
      dy = to.y - from.y,
      angle = -Math.atan2(dy, dx);
    const progress = Math.min(1, Math.max(0, (this.age - 0.1) / 0.65)),
      fade = Math.max(0, Math.min(1, (1.5 - this.age) / 0.4));
    gl.useProgram(this.program);
    bindAttrib(gl, this.quad, 0, 2);
    gl.uniform2f(gl.getUniformLocation(this.program, "uResolution"), w, h);
    gl.activeTexture(gl.TEXTURE2);
    gl.bindTexture(gl.TEXTURE_2D, this.texture);
    gl.uniform1i(gl.getUniformLocation(this.program, "uSprite"), 2);
    gl.enable(gl.BLEND);
    gl.blendFuncSeparate(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA, gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
    const mode = gl.getUniformLocation(this.program, "uMode"), color = gl.getUniformLocation(this.program, "uColor");
    /** A soft round glow (mist puff or ice sparkle) instead of the shard sprite. */
    const disc = (x: number, y: number, r: number, rgb: [number, number, number], alpha: number) => {
      if (alpha <= 0.005) return;
      gl.uniform1f(mode, 1);
      gl.uniform3f(color, rgb[0], rgb[1], rgb[2]);
      gl.uniform2f(gl.getUniformLocation(this.program, "uCenter"), x, y);
      gl.uniform2f(gl.getUniformLocation(this.program, "uSize"), r, r);
      gl.uniform1f(gl.getUniformLocation(this.program, "uAngle"), 0);
      gl.uniform1f(gl.getUniformLocation(this.program, "uAlpha"), alpha);
      gl.drawArrays(gl.TRIANGLES, 0, 6);
      gl.uniform1f(mode, 0);
    };
    const MIST: [number, number, number] = [0.78, 0.88, 0.96], ICE: [number, number, number] = [0.86, 0.95, 1];
    // a seeded jitter per particle, stable from frame to frame
    const rand = (n: number) => { const s = Math.sin(n * 12.9898 + (this.config.seed ?? 1) * 0.37) * 43758.5453; return s - Math.floor(s); };
    const sprite = (x: number, y: number, sx: number, sy: number, a: number, alpha: number) => {
      gl.uniform2f(gl.getUniformLocation(this.program, "uCenter"), x, y);
      gl.uniform2f(gl.getUniformLocation(this.program, "uSize"), sx, sy);
      gl.uniform1f(gl.getUniformLocation(this.program, "uAngle"), a);
      gl.uniform1f(gl.getUniformLocation(this.program, "uAlpha"), alpha);
      gl.drawArrays(gl.TRIANGLES, 0, 6);
    };
    sprite(
      from.x + dx * progress - from.tile * 0.4 * Math.cos(angle),
      from.y + dy * progress + from.tile * 0.4 * Math.sin(angle) - from.tile * 0.25,
      from.tile * 0.72,
      from.tile * 0.23,
      angle,
      fade,
    );
    // ice glints trailing the shard while it flies
    if (progress > 0 && progress < 1) for (let k = 1; k <= 12; k++) {
      const back = progress - k * 0.035;
      if (back <= 0) break;
      const jx = (rand(k) - 0.5) * from.tile * 0.3, jy = (rand(k + 50) - 0.5) * from.tile * 0.2;
      disc(from.x + dx * back + jx, from.y + dy * back - from.tile * 0.25 + jy, from.tile * (0.1 - k * 0.005), ICE, fade * (1 - k / 13) * 0.9);
    }
    this.config.cells.forEach((cell, i) => {
      const a = get(cell),
        arrival = (i + 1) / this.config.cells.length;
      if (progress >= arrival) {
        const t = this.age - 0.1 - arrival * 0.65;
        // cold mist rising off the struck hex
        for (let m = 0; m < 4; m++) {
          const life = Math.max(0, 1 - t / 1.25);
          disc(a.x + (rand(i * 7 + m) - 0.5) * a.tile * 0.6, a.y - a.tile * (0.15 + t * 0.4 + m * 0.1), a.tile * (0.4 + t * 0.5 + m * 0.08), MIST, Math.sqrt(life) * 0.7);
        }
        // a burst of ice sparkles thrown up and falling back
        for (let s = 0; s < 14; s++) {
          const ang = rand(i * 31 + s) * Math.PI * 2, speed = 0.4 + rand(i * 17 + s) * 0.6;
          const life = Math.max(0, 1 - t / 0.9);
          disc(a.x + Math.cos(ang) * speed * t * a.tile, a.y - a.tile * 0.2 - Math.abs(Math.sin(ang)) * speed * t * a.tile * 1.1 + t * t * a.tile * 0.9, a.tile * (0.07 + rand(s + 90) * 0.05), ICE, life);
        }
      }
      if (progress >= arrival && !this.hits.has(i)) {
        this.hits.add(i);
        this.config.onImpact?.(cell);
      }
      if (progress >= arrival) {
        const t = this.age - 0.1 - arrival * 0.65;
        for (let j = 0; j < 5; j++) {
          const phase = j * 2.4 + (this.config.seed ?? 1) * 0.01;
          sprite(
            a.x + Math.cos(phase) * t * a.tile * 0.45,
            a.y - a.tile * 0.15 + Math.sin(phase) * t * a.tile * 0.25,
            a.tile * 0.12,
            a.tile * 0.05,
            phase,
            Math.max(0, 1 - t * 1.4) * fade * 0.8,
          );
        }
      }
    });
  }
  dispose() {
    if (this.disposed) return;
    this.disposed = true;
    this.gl.deleteTexture(this.texture);
    this.gl.deleteBuffer(this.quad);
    this.gl.deleteProgram(this.program);
  }
}
