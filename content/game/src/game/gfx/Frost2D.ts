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
precision highp float;in vec2 vUv;uniform sampler2D uSprite;uniform float uAlpha;out vec4 fragColor;void main(){vec2 uv=vec2(.025+vUv.x*.95,.32+vUv.y*.32);vec3 col=texture(uSprite,uv).rgb;float a=smoothstep(.035,.22,max(col.r,max(col.g,col.b)))*uAlpha;if(a<.005)discard;fragColor=vec4(col,a);}`;
/** A short forward ice jet in the existing raw WebGL2 alpha/bloom passes. */
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
    return this.age >= 1.5;
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
      fade = Math.min(1, (1.5 - this.age) / 0.4);
    gl.useProgram(this.program);
    bindAttrib(gl, this.quad, 0, 2);
    gl.uniform2f(gl.getUniformLocation(this.program, "uResolution"), w, h);
    gl.activeTexture(gl.TEXTURE2);
    gl.bindTexture(gl.TEXTURE_2D, this.texture);
    gl.uniform1i(gl.getUniformLocation(this.program, "uSprite"), 2);
    gl.enable(gl.BLEND);
    gl.blendFuncSeparate(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA, gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
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
    this.config.cells.forEach((cell, i) => {
      const a = get(cell),
        arrival = (i + 1) / this.config.cells.length;
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
