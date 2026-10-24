/**
 * HD-2D geometry test (Dev Controls → "Cena 3D — teste HD-2D"). A standalone Three.js scene,
 * separate from the battle renderer: one small section of "O Vau" built as real 3D geometry —
 * hex prisms with the map's own tile art on top, a clearly raised plateau, a real standing wall
 * block — with an existing 2D character sprite standing inside it as an upright billboard, a
 * real Sun and Moon (THREE.DirectionalLight) that move across the sky, and a real
 * THREE.PointLight above a brazier. An orbit camera lets the world be viewed from any side.
 *
 * World units: 1 = one hex radius. Y is up (standard Three.js), the board lies on the XZ plane,
 * +Z is "down the screen" in the tactical view.
 */
import { useEffect, useRef, useState } from "react";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import { loadGameArt } from "../../assets";
import { BattleEngine } from "../../engine";
import { missionById } from "../../mapstore";
import { TERRAIN } from "../../data";

const SQRT3 = Math.sqrt(3);
const GROUND_TOP = 0.3;
const PLATEAU_TOP = 1.4;
const HILL_EXTRA = 0.4;
const CHARACTER_HEIGHT = 1.9;
const BRAZIER_HEIGHT = 1.3;
const KAEL_FRAMES = 36;
const kaelFrameSrc = (i: number) => `/game/sprites/Kael_Final/kael-final-002/${i + 1}.png?v=kael-final-002`;

export type Hd2dView = "tactical" | "side" | "top";

export interface Hd2dApi {
  /** PointLight position: x/z on the board, y = height above the world origin. */
  setLight(x: number, y: number, z: number): void;
  getLight(): { x: number; y: number; z: number };
  /** 0..1 through one day: 0.25 sunrise, 0.5 midday, 0.75 sunset, 0/1 midnight. */
  setTimeOfDay(t: number): void;
  getTimeOfDay(): number;
  /** Whether the sun and moon move on their own. */
  setCycle(on: boolean): void;
  setView(view: Hd2dView): void;
  renderNow(): void;
  dispose(): void;
}

function loadImage(src: string): Promise<HTMLImageElement> {
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => resolve(img);
    img.onerror = reject;
    img.src = src;
  });
}

function srgbTexture(img: HTMLImageElement): THREE.Texture {
  const tex = new THREE.Texture(img);
  tex.colorSpace = THREE.SRGBColorSpace;
  tex.anisotropy = 4;
  tex.needsUpdate = true;
  return tex;
}

/** Hex prism, pointy-top as seen from above, base at y=0 and top at y=height. Group 0 = top
 * and bottom caps (UVs span the tile image), group 1 = the vertical sides. */
function hexPrism(height: number): THREE.BufferGeometry {
  const shape = new THREE.Shape();
  for (let i = 0; i < 6; i++) {
    const a = ((60 * i + 30) * Math.PI) / 180;
    const x = Math.cos(a);
    const y = Math.sin(a);
    if (i === 0) shape.moveTo(x, y);
    else shape.lineTo(x, y);
  }
  shape.closePath();
  const geo = new THREE.ExtrudeGeometry(shape, { depth: height, bevelEnabled: false });
  // Extrusion runs along +Z; stand it up so it rises along +Y (shape y ends up on -Z, i.e. the
  // top of the tile image points "up the screen" in the tactical view).
  geo.rotateX(-Math.PI / 2);
  return geo;
}

/** Upright 2D sprite quad, feet at y=0, that casts a shadow of its real alpha silhouette. */
function spriteBillboard(tex: THREE.Texture, height: number, aspect: number, castShadow: boolean) {
  const geo = new THREE.PlaneGeometry(height * aspect, height);
  geo.translate(0, height / 2, 0);
  const mat = new THREE.MeshStandardMaterial({ map: tex, alphaTest: 0.5, side: THREE.DoubleSide, roughness: 1, metalness: 0 });
  const mesh = new THREE.Mesh(geo, mat);
  mesh.castShadow = castShadow;
  mesh.receiveShadow = true;
  mesh.customDepthMaterial = new THREE.MeshDepthMaterial({ depthPacking: THREE.RGBADepthPacking, map: tex, alphaTest: 0.5 });
  mesh.customDistanceMaterial = new THREE.MeshDistanceMaterial({ map: tex, alphaTest: 0.5 });
  return { mesh, mat };
}

export async function mountHd2dTest(canvas: HTMLCanvasElement): Promise<Hd2dApi> {
  const art = await loadGameArt();
  const mission = missionById("vau")!;
  const engine = new BattleEngine(mission, art, { hp: {}, levels: {} } as unknown as ConstructorParameters<typeof BattleEngine>[2], 7);
  const spawn = mission.playerSpawns[0]!;

  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
  renderer.outputColorSpace = THREE.SRGBColorSpace;
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.shadowMap.enabled = true;
  renderer.shadowMap.type = THREE.PCFShadowMap;

  const scene = new THREE.Scene();
  const camera = new THREE.PerspectiveCamera(35, 1, 0.1, 200);
  const controls = new OrbitControls(camera, canvas);
  controls.enableDamping = true;

  // --- Ground: a 9x7 section of the map, each hex a real prism with its tile art on top. ---
  const COLS = 9;
  const ROWS = 7;
  const col0 = Math.max(0, spawn.x - 3);
  const row0 = Math.max(0, spawn.y - 3);
  const center = { x: SQRT3 * (COLS / 2), z: 1.5 * (ROWS / 2) };
  const sideMat = new THREE.MeshStandardMaterial({ color: 0x4a3a2c, roughness: 1 });
  const tileMats = new Map<string, THREE.MeshStandardMaterial>();
  const heightAt = new Map<string, number>();
  const cellPos = (c: number, r: number) => ({ x: SQRT3 * (c + 0.5 * ((row0 + r) & 1) + 0.5) - center.x, z: 1.5 * r + 1 - center.z });
  const prismCache = new Map<number, THREE.BufferGeometry>();
  // The raised plateau: a clearly elevated block of cells on the right side of the section.
  const plateau = new Set(["6,1", "7,1", "6,2", "7,2", "8,2", "7,3"]);
  for (let r = 0; r < ROWS; r++) {
    for (let c = 0; c < COLS; c++) {
      const mc = col0 + c;
      const mr = row0 + r;
      if (mc >= engine.cols || mr >= engine.rows) continue;
      const id = engine.tiles[mr * engine.cols + mc]!;
      if (id === "void") continue;
      const variant = engine.tileVariants[mr * engine.cols + mc] ?? 0;
      const img = art.tiles[id]?.[variant] ?? art.tiles[id]?.[0];
      const key = `${id}:${variant}`;
      let top = tileMats.get(key);
      if (!top) {
        top = new THREE.MeshStandardMaterial({ roughness: 1, metalness: 0, color: img ? 0xffffff : 0x556b3a });
        if (img) {
          const tex = srgbTexture(img);
          tex.repeat.set(0.5, 0.5);
          tex.offset.set(0.5, 0.5);
          top.map = tex;
        }
        tileMats.set(key, top);
      }
      const h = plateau.has(`${c},${r}`) ? PLATEAU_TOP : GROUND_TOP + (TERRAIN[id]?.height ? HILL_EXTRA : 0);
      heightAt.set(`${c},${r}`, h);
      let geo = prismCache.get(h);
      if (!geo) prismCache.set(h, (geo = hexPrism(h)));
      const mesh = new THREE.Mesh(geo, [top, sideMat]);
      const p = cellPos(c, r);
      mesh.position.set(p.x, 0, p.z);
      mesh.castShadow = true;
      mesh.receiveShadow = true;
      scene.add(mesh);
    }
  }
  const surface = (c: number, r: number) => heightAt.get(`${c},${r}`) ?? GROUND_TOP;

  // --- A real standing wall: 2 units tall, casts and receives shadows. ---
  const wallPos = cellPos(2, 4);
  const wall = new THREE.Mesh(new THREE.BoxGeometry(3.4, 2.0, 0.35), new THREE.MeshStandardMaterial({ color: 0x86807a, roughness: 0.9 }));
  wall.position.set(wallPos.x + 0.4, GROUND_TOP + 1.0, wallPos.z + 0.75);
  wall.castShadow = true;
  wall.receiveShadow = true;
  scene.add(wall);

  // --- The 2D character, standing upright on the ground. ---
  const kaelImgs = await Promise.all(Array.from({ length: KAEL_FRAMES }, (_, i) => loadImage(kaelFrameSrc(i))));
  const kaelTex = kaelImgs.map(srgbTexture);
  const kael = spriteBillboard(kaelTex[0]!, CHARACTER_HEIGHT, kaelImgs[0]!.naturalWidth / kaelImgs[0]!.naturalHeight, true);
  const kaelCell = { c: 3, r: 2 };
  const kp = cellPos(kaelCell.c, kaelCell.r);
  kael.mesh.position.set(kp.x, surface(kaelCell.c, kaelCell.r), kp.z);
  scene.add(kael.mesh);

  // --- Brazier sprite + a real PointLight above its bowl. ---
  const brazierImg = await loadImage("/game/decorations/city-brazier.png");
  const brazier = spriteBillboard(srgbTexture(brazierImg), BRAZIER_HEIGHT, brazierImg.naturalWidth / brazierImg.naturalHeight, false);
  const bp = cellPos(3, 3);
  brazier.mesh.position.set(bp.x + 0.6, surface(3, 3), bp.z);
  scene.add(brazier.mesh);

  const fire = new THREE.PointLight(0xff9a4a, 14, 12, 2);
  fire.castShadow = true;
  fire.shadow.mapSize.set(1024, 1024);
  fire.shadow.camera.near = 0.05;
  fire.shadow.bias = -0.002;
  fire.position.set(brazier.mesh.position.x, brazier.mesh.position.y + BRAZIER_HEIGHT * 0.85, brazier.mesh.position.z);
  scene.add(fire);
  // The light's own position made visible: a small glowing core (does not block light).
  const core = new THREE.Mesh(new THREE.SphereGeometry(0.09, 16, 12), new THREE.MeshBasicMaterial({ color: 0xffd9a0 }));
  core.castShadow = false;
  fire.add(core);

  // --- Sun, Moon and sky fill. ---
  const sun = new THREE.DirectionalLight(0xfff0d8, 3.4);
  const moon = new THREE.DirectionalLight(0x9fb4ff, 0.8);
  for (const l of [sun, moon]) {
    l.castShadow = true;
    l.shadow.mapSize.set(2048, 2048);
    const cam = l.shadow.camera as THREE.OrthographicCamera;
    cam.left = -14;
    cam.right = 14;
    cam.top = 14;
    cam.bottom = -14;
    cam.near = 0.5;
    cam.far = 80;
    l.shadow.bias = -0.0005;
    l.shadow.normalBias = 0.02;
    l.target.position.set(0, 0, 0);
    scene.add(l, l.target);
  }
  const sky = new THREE.HemisphereLight(0xcfd8ff, 0x3a2e22, 0.6);
  scene.add(sky);
  const daySky = new THREE.Color(0x8fa6c4);
  const nightSky = new THREE.Color(0x0b0f1a);
  scene.background = daySky.clone();

  // Faint floor grid under everything, to read depth in the debug views.
  const grid = new THREE.GridHelper(40, 40, 0x333333, 0x222222);
  grid.position.y = -0.01;
  scene.add(grid);

  let timeOfDay = 0.4;
  let cycle = true;
  const DAY_SECONDS = 90;
  /** Sun path: rises in the east (+X), arcs high over the board, sets in the west (-X); the
   * Moon follows the opposite half of the same arc. */
  const skyPos = (angle: number) => new THREE.Vector3(Math.cos(angle) * 30, Math.sin(angle) * 26, -Math.sin(angle) * 10 - 6);
  const applySky = () => {
    const a = (timeOfDay - 0.25) * Math.PI * 2;
    sun.position.copy(skyPos(a));
    moon.position.copy(skyPos(a + Math.PI));
    const sunUp = THREE.MathUtils.clamp(Math.sin(a) * 4, 0, 1);
    const moonUp = THREE.MathUtils.clamp(Math.sin(a + Math.PI) * 4, 0, 1);
    sun.intensity = 3.4 * sunUp;
    moon.intensity = 0.8 * moonUp;
    sun.castShadow = sunUp > 0.01;
    moon.castShadow = moonUp > 0.01;
    sky.intensity = 0.25 + 0.9 * sunUp;
    (scene.background as THREE.Color).copy(nightSky).lerp(daySky, sunUp);
  };

  const setView = (view: Hd2dView) => {
    if (view === "tactical") camera.position.set(0, 14, 13);
    else if (view === "side") camera.position.set(19, 1.6, 0);
    else camera.position.set(0, 24, 0.01);
    controls.target.set(0, 0.6, 0);
    controls.update();
  };
  setView("tactical");

  const resize = () => {
    const w = canvas.clientWidth || canvas.width;
    const h = canvas.clientHeight || canvas.height;
    renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
    renderer.setSize(w, h, false);
    camera.aspect = w / Math.max(1, h);
    camera.updateProjectionMatrix();
  };
  resize();
  const ro = new ResizeObserver(resize);
  ro.observe(canvas);

  const faceCamera = (m: THREE.Object3D) => {
    m.rotation.y = Math.atan2(camera.position.x - m.position.x, camera.position.z - m.position.z);
  };
  let frame = 0;
  let dir = 1;
  let frameClock = 0;
  const renderNow = () => {
    applySky();
    faceCamera(kael.mesh);
    faceCamera(brazier.mesh);
    renderer.render(scene, camera);
  };

  let last = performance.now();
  let raf = 0;
  const loop = (now: number) => {
    const dt = Math.min(0.1, (now - last) / 1000);
    last = now;
    if (cycle) timeOfDay = (timeOfDay + dt / DAY_SECONDS) % 1;
    frameClock += dt;
    if (frameClock >= 1 / 12) {
      frameClock = 0;
      frame += dir;
      if (frame >= KAEL_FRAMES - 1 || frame <= 0) dir = -dir;
      const tex = kaelTex[frame]!;
      kael.mat.map = tex;
      (kael.mesh.customDepthMaterial as THREE.MeshDepthMaterial).map = tex;
      (kael.mesh.customDistanceMaterial as THREE.MeshDistanceMaterial).map = tex;
    }
    fire.intensity = 14 * (1 + 0.06 * Math.sin(now * 0.011) + 0.04 * Math.sin(now * 0.023));
    controls.update();
    renderNow();
    raf = requestAnimationFrame(loop);
  };
  raf = requestAnimationFrame(loop);

  return {
    setLight(x, y, z) {
      fire.position.set(x, y, z);
    },
    getLight() {
      return { x: fire.position.x, y: fire.position.y, z: fire.position.z };
    },
    setTimeOfDay(t) {
      timeOfDay = ((t % 1) + 1) % 1;
    },
    getTimeOfDay() {
      return timeOfDay;
    },
    setCycle(on) {
      cycle = on;
    },
    setView,
    renderNow,
    dispose() {
      cancelAnimationFrame(raf);
      ro.disconnect();
      controls.dispose();
      renderer.dispose();
    },
  };
}

/** Full-screen test screen with the scene and a few controls. */
export function Hd2dTestScreen({ onBack }: { onBack: () => void }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const apiRef = useRef<Hd2dApi | null>(null);
  const [ready, setReady] = useState(false);
  const [cycle, setCycleState] = useState(true);
  const [light, setLightState] = useState({ x: 0, y: 0, z: 0 });
  const [hour, setHour] = useState(0.4);

  useEffect(() => {
    let disposed = false;
    const canvas = canvasRef.current;
    if (!canvas) return;
    void mountHd2dTest(canvas).then((api) => {
      if (disposed) return api.dispose();
      apiRef.current = api;
      (window as Window & { __hd2d?: Hd2dApi }).__hd2d = api;
      setLightState(api.getLight());
      setReady(true);
    });
    const timer = window.setInterval(() => {
      if (apiRef.current) setHour(apiRef.current.getTimeOfDay());
    }, 250);
    return () => {
      disposed = true;
      window.clearInterval(timer);
      apiRef.current?.dispose();
      apiRef.current = null;
    };
  }, []);

  const moveLight = (patch: Partial<typeof light>) => {
    const next = { ...light, ...patch };
    setLightState(next);
    apiRef.current?.setLight(next.x, next.y, next.z);
  };
  const clock = `${String(Math.floor(hour * 24)).padStart(2, "0")}:${String(Math.floor((hour * 24 * 60) % 60)).padStart(2, "0")}`;

  return (
    <section className="fixed inset-0 z-50 bg-black">
      <canvas ref={canvasRef} className="absolute inset-0 h-full w-full" />
      <div className="absolute left-4 top-4 flex w-72 flex-col gap-3 rounded-xl border border-border bg-bg/85 p-4 text-sm">
        <div className="flex items-center justify-between">
          <span className="font-display text-xl">Cena 3D — teste HD-2D</span>
          <button type="button" onClick={onBack} className="rounded-md border border-border px-2 py-1">Voltar</button>
        </div>
        {!ready && <p className="text-muted">Carregando…</p>}
        <p className="text-muted">Arraste para girar a câmera, role para zoom.</p>
        <div className="flex gap-2">
          {(["tactical", "side", "top"] as const).map((v) => (
            <button key={v} type="button" onClick={() => apiRef.current?.setView(v)} className="flex-1 rounded-md border border-border px-2 py-1 hover:border-accent">
              {v === "tactical" ? "Tática" : v === "side" ? "Lateral" : "Topo"}
            </button>
          ))}
        </div>
        <div className="flex items-center justify-between">
          <span>Hora do dia: {clock}</span>
          <button
            type="button"
            onClick={() => {
              const on = !cycle;
              setCycleState(on);
              apiRef.current?.setCycle(on);
            }}
            className="rounded-md border border-border px-2 py-1"
          >
            {cycle ? "Pausar sol/lua" : "Mover sol/lua"}
          </button>
        </div>
        <p className="font-display text-base">Fogo (PointLight)</p>
        {(
          [
            ["x", "Leste / oeste", -8, 8],
            ["z", "Norte / sul", -6, 6],
            ["y", "Altura", 0.2, 6],
          ] as const
        ).map(([k, label, min, max]) => (
          <label key={k} className="flex flex-col gap-1">
            <span>{label}</span>
            <input type="range" min={min} max={max} step={0.05} value={light[k]} onChange={(e) => moveLight({ [k]: Number(e.target.value) })} />
          </label>
        ))}
      </div>
    </section>
  );
}
