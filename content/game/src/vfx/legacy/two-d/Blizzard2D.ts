import { createProgram } from "./glutil";
export type BlizzardCell = { x: number; y: number };
export type BlizzardAnchor = { x: number; y: number; tile: number };
export interface Blizzard2DConfig {
  cells: BlizzardCell[];
  duration?: number;
  intensity?: number;
  particleDensity?: number;
  windStrength?: number;
  seed?: number;
  groundScaleY?: number;
  onImpact?: (cell: BlizzardCell, dx: number, dy: number, strength: number) => void;
}
type Mote = {
  cell: number;
  dx: number;
  dy: number;
  layer: number;
  kind: number;
  phase: number;
  speed: number;
  size: number;
  turn: number;
  lastCycle: number;
};
const VS = `#version 300 es
precision highp float;layout(location=0)in vec2 aQuad;layout(location=1)in vec2 aCenter;layout(location=2)in vec2 aSize;layout(location=3)in vec4 aData;uniform vec2 uResolution;out vec2 vUv;out vec4 vData;
void main(){vUv=aQuad*.5+.5;vData=aData;float c=cos(aData.x),s=sin(aData.x);vec2 p=mat2(c,-s,s,c)*(aQuad*aSize)+aCenter;gl_Position=vec4(p.x/uResolution.x*2.-1.,1.-p.y/uResolution.y*2.,0,1);}`;
const FS = `#version 300 es
precision highp float;in vec2 vUv;in vec4 vData;uniform sampler2D uIce;uniform sampler2D uMist;uniform float uTime;uniform sampler2D uArea;uniform vec2 uResolution;out vec4 fragColor;
void main(){vec2 p=vUv*2.-1.;float kind=vData.z; if(kind>3.5){vec2 screenUv=vec2(gl_FragCoord.x/uResolution.x,1.-gl_FragCoord.y/uResolution.y);float area=texture(uArea,screenUv).a;vec2 iceUv=fract(screenUv*vec2(3.3,2.8));vec4 floorIce=texture(uMist,(vec2(1.,2.)+iceUv)/4.);float a=area*vData.y*floorIce.a*.32;fragColor=vec4(mix(floorIce.rgb,vec3(.72,.9,.96),.45),a);return;}float frame=floor(vData.w);vec2 cell=vec2(mod(frame,4.),floor(frame/4.));vec4 tex;
if(kind<.5){vec2 uv=vUv;uv.x+=sin(uv.y*9.+uTime*1.3+frame)*.045;uv.y+=sin(uv.x*7.-uTime*.8)*.055;
tex=texture(uMist,(cell+clamp(uv,.005,.995))/4.);float ribbon=pow(max(0.,1.-p.x*p.x),.5)*pow(max(0.,1.-abs(p.y)),.7);tex.a*=ribbon;tex.rgb=mix(tex.rgb,vec3(.13,.58,.83),.25);}
else if(kind<1.5){vec2 uv=vec2(.49+vUv.x*.12,.035+vUv.y*.66);tex=texture(uIce,(cell+uv)/4.);tex.a*=1.-smoothstep(.82,.99,vUv.y+sin(vUv.x*13.)*.025);}
else if(kind<2.5){float d=length(p);float a=1.-smoothstep(.1,.85,d);tex=vec4(.76,.94,1.,a);}
else {tex=texture(uIce,(cell+clamp(vUv,.002,.998))/4.);}
tex.a*=vData.y;if(tex.a<.006)discard;fragColor=tex;}`;
function rng(seed: number) {
  let s = seed >>> 0 || 1;
  return () => {
    s ^= s << 13;
    s ^= s >>> 17;
    s ^= s << 5;
    return (s >>> 0) / 4294967296;
  };
}
/** Shared 2D instanced sprite pass, rendered into EffectsRenderer's existing alpha/bloom FBO. */
export class Blizzard2D {
  private readonly program: WebGLProgram;
  private readonly vao: WebGLVertexArrayObject;
  private readonly quad: WebGLBuffer;
  private readonly instances: WebGLBuffer;
  private readonly ice: WebGLTexture;
  private readonly mist: WebGLTexture;
  private readonly area: WebGLTexture;
  private readonly mask = document.createElement("canvas");
  private maskKey = "";
  private readonly motes: Mote[] = [];
  private readonly data: Float32Array;
  private age = 0;
  private loaded = 0;
  private disposed = false;
  readonly duration: number;
  readonly seed: number;
  constructor(
    private gl: WebGL2RenderingContext,
    readonly config: Blizzard2DConfig,
  ) {
    if (!config.cells.length) throw new Error("Blizzard requires resolved affected cells");
    this.duration = Math.max(2, config.duration ?? 8);
    this.seed = config.seed ?? 38171;
    const random = rng(this.seed);
    const density = Math.max(0.2, Math.min(3, config.particleDensity ?? 1));
    const count = Math.min(1800, Math.ceil(config.cells.length * 95 * density));
    for (let i = 0; i < count; i++) {
      // Uniform triangle-fan sampling of the game's actual pointy-top hex: adjacent edges join.
      const edge = Math.floor(random() * 6);
      const a = ((edge * 60 - 30) * Math.PI) / 180,
        b = a + Math.PI / 3;
      const r = Math.sqrt(random()) * 0.94,
        t = random();
      const dx = r * ((1 - t) * Math.cos(a) + t * Math.cos(b)),
        dy = r * ((1 - t) * Math.sin(a) + t * Math.sin(b));
      const kind = i % 24 < 5 ? 0 : i % 24 < 7 ? 1 : 2;
      this.motes.push({
        cell: Math.floor(random() * config.cells.length),
        dx,
        dy,
        layer: i % 3,
        kind,
        phase: random(),
        speed: 0.6 + random() * 0.9,
        size: random(),
        turn: random() * 6.28,
        lastCycle: -1,
      });
    }
    for (const mote of [...this.motes]) if (mote.kind === 1) this.motes.push({ ...mote, kind: 3 });
    this.data = new Float32Array(this.motes.length * 8);
    this.program = createProgram(gl, VS, FS);
    this.vao = gl.createVertexArray()!;
    this.quad = gl.createBuffer()!;
    this.instances = gl.createBuffer()!;
    gl.bindVertexArray(this.vao);
    gl.bindBuffer(gl.ARRAY_BUFFER, this.quad);
    gl.bufferData(
      gl.ARRAY_BUFFER,
      new Float32Array([-1, -1, 1, -1, -1, 1, -1, 1, 1, -1, 1, 1]),
      gl.STATIC_DRAW,
    );
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
    gl.bindBuffer(gl.ARRAY_BUFFER, this.instances);
    gl.bufferData(gl.ARRAY_BUFFER, this.data.byteLength, gl.DYNAMIC_DRAW);
    for (const [index, size, offset] of [
      [1, 2, 0],
      [2, 2, 8],
      [3, 4, 16],
    ]) {
      gl.enableVertexAttribArray(index);
      gl.vertexAttribPointer(index, size, gl.FLOAT, false, 32, offset);
      gl.vertexAttribDivisor(index, 1);
    }
    gl.bindVertexArray(null);
    this.ice = this.load("/game/fx/blizzard-impact-reference.png");
    this.mist = this.load("/game/fx/blizzard-wind-reference.png");
    this.area = gl.createTexture()!;
    gl.bindTexture(gl.TEXTURE_2D, this.area);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  }
  private load(url: string) {
    const gl = this.gl;
    const tex = gl.createTexture()!;
    gl.bindTexture(gl.TEXTURE_2D, tex);
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
    const img = new Image();
    img.onload = () => {
      if (this.disposed) return;
      gl.bindTexture(gl.TEXTURE_2D, tex);
      gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, img);
      this.loaded++;
    };
    img.src = url;
    return tex;
  }
  get finished() {
    return this.age >= this.duration;
  }
  get ready() {
    return this.loaded === 2;
  }
  draw(
    dt: number,
    width: number,
    height: number,
    getAnchor: (cell: BlizzardCell) => BlizzardAnchor,
  ) {
    if (this.disposed) return;
    this.age = Math.min(this.duration, this.age + Math.max(0, dt));
    if (!this.ready || this.finished) return;
    const age = this.age;
    const gather = Math.min(1, age / 1.2);
    const fade = Math.min(1, (this.duration - age) / 1.6);
    const intensity = Math.max(0, this.config.intensity ?? 1);
    const wind = this.config.windStrength ?? 1;
    this.motes.forEach((m, i) => {
      const a = getAnchor(this.config.cells[m.cell]);
      const cycle = age * m.speed * 0.55 + m.phase,
        f = cycle - Math.floor(cycle);
      let x = a.x + m.dx * a.tile,
        y = a.y + m.dy * a.tile * (this.config.groundScaleY ?? 1);
      let sx = 0,
        sy = 0,
        alpha = gather * fade,
        angle = 0;
      if (m.kind === 1 && Math.floor(cycle) > m.lastCycle) {
        if (m.lastCycle >= 0) this.config.onImpact?.(this.config.cells[m.cell], m.dx, m.dy, fade);
        m.lastCycle = Math.floor(cycle);
      }
      const gust = Math.sin(age * 1.5 + m.turn + m.layer) * wind;
      if (m.kind === 0) {
        y -= a.tile * (0.35 + m.layer * 0.65);
        x += gust * a.tile * 0.23;
        sx = a.tile * (0.68 + m.size * 0.55);
        sy = a.tile * (0.18 + m.size * 0.17);
        y -= sy * 0.8;
        angle = -0.25 + gust * 0.15;
        alpha *= 0.24;
      } else if (m.kind === 1) {
        y -= a.tile * (1 - f) * (1.8 + m.layer * 0.26);
        x += gust * a.tile * 0.12 * (1 - f);
        sx = a.tile * (0.07 + m.size * 0.07);
        sy = a.tile * (0.18 + m.size * 0.17);
        y -= sy * 0.8;
        angle =
          Math.PI +
          Math.atan2(
            0.12 *
              (1.5 * Math.cos(age * 1.5 + m.turn + m.layer) * wind * (1 - f) -
                gust * m.speed * 0.55),
            m.speed * 0.55 * (1.8 + m.layer * 0.26),
          );
        alpha *= 0.92;
      } else if (m.kind === 2) {
        y -= a.tile * (1 - f) * (1.9 + m.layer * 0.3);
        x += gust * a.tile * 0.14 * (1 - f);
        sx = a.tile * (0.009 + m.size * 0.016);
        sy = sx * (1 + m.layer * 0.3);
        angle = -0.2;
        alpha *= 0.45 + m.layer * 0.18;
      } else {
        sx = a.tile * (0.35 + m.size * 0.24);
        sy = a.tile * (0.33 + m.size * 0.25);
        y -= sy * 0.85;
        alpha *= f < 0.28 ? 1 : 0;
      }
      this.data.set(
        [
          x,
          y,
          sx,
          sy,
          angle,
          alpha * intensity,
          m.kind,
          m.kind === 3
            ? Math.min(15, Math.floor((f / 0.28) * 16))
            : m.kind === 1
              ? 3
              : Math.floor((age * 18 + m.phase * 16) % 16),
        ],
        i * 8,
      );
    });
    const gl = this.gl;
    gl.useProgram(this.program);
    gl.bindVertexArray(this.vao);
    gl.bindBuffer(gl.ARRAY_BUFFER, this.instances);
    gl.bufferSubData(gl.ARRAY_BUFFER, 0, this.data);
    gl.uniform2f(gl.getUniformLocation(this.program, "uResolution"), width, height);
    gl.uniform1f(gl.getUniformLocation(this.program, "uTime"), age);
    gl.activeTexture(gl.TEXTURE2);
    gl.bindTexture(gl.TEXTURE_2D, this.ice);
    gl.uniform1i(gl.getUniformLocation(this.program, "uIce"), 2);
    gl.activeTexture(gl.TEXTURE3);
    gl.bindTexture(gl.TEXTURE_2D, this.mist);
    gl.uniform1i(gl.getUniformLocation(this.program, "uMist"), 3);
    gl.activeTexture(gl.TEXTURE4);
    gl.bindTexture(gl.TEXTURE_2D, this.area);
    const maskKey = JSON.stringify([width, height, ...this.config.cells.map(getAnchor)]);
    if (maskKey !== this.maskKey) {
      this.maskKey = maskKey;
      this.mask.width = width;
      this.mask.height = height;
      const ctx = this.mask.getContext("2d")!;
      ctx.fillStyle = "white";
      ctx.strokeStyle = "white";
      ctx.lineWidth = 1;
      for (const cell of this.config.cells) {
        const a = getAnchor(cell);
        ctx.beginPath();
        for (let i = 0; i < 6; i++) {
          const angle = ((i * 60 - 30) * Math.PI) / 180;
          const x = a.x + Math.cos(angle) * a.tile,
            y = a.y + Math.sin(angle) * a.tile * (this.config.groundScaleY ?? 1);
          if (i) ctx.lineTo(x, y);
          else ctx.moveTo(x, y);
        }
        ctx.closePath();
        ctx.fill();
        ctx.stroke();
      }
      gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, this.mask);
    }
    gl.uniform1i(gl.getUniformLocation(this.program, "uArea"), 4);
    gl.enable(gl.BLEND);
    gl.blendFuncSeparate(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA, gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
    const floor = new Float32Array([
      width / 2,
      height / 2,
      width / 2,
      height / 2,
      0,
      gather * fade * intensity,
      4,
      0,
    ]);
    gl.bufferSubData(gl.ARRAY_BUFFER, 0, floor);
    gl.drawArraysInstanced(gl.TRIANGLES, 0, 6, 1);
    gl.bufferSubData(gl.ARRAY_BUFFER, 0, this.data);
    gl.drawArraysInstanced(gl.TRIANGLES, 0, 6, this.motes.length);
    gl.bindVertexArray(null);
  }
  dispose() {
    if (this.disposed) return;
    this.disposed = true;
    const gl = this.gl;
    gl.deleteTexture(this.area);
    gl.deleteTexture(this.ice);
    gl.deleteTexture(this.mist);
    gl.deleteBuffer(this.quad);
    gl.deleteBuffer(this.instances);
    gl.deleteVertexArray(this.vao);
    gl.deleteProgram(this.program);
  }
}
