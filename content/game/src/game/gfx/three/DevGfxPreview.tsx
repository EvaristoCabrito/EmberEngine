import { useEffect, useRef } from "react";
import * as THREE from "three";
import { getDevGfx } from "./devGfx";

const SHADOW_RADIUS_HARD = 1;
const SHADOW_RADIUS_SOFT = 4;

/** Turns a slice of the unit's own cutout into a flat multiply-blend decal: a generic radial
 * gradient reads as a uniform ellipse no matter what's standing on it, with the same soft falloff
 * everywhere — a real contact shadow is sharp exactly at the touching surface and only softens in
 * the gap right next to it. `(sx,sy,sw,sh)` crops the source image (in its own pixel space) before
 * tinting, so the caller can grab just the feet/hem slice instead of the whole body. Method:
 * composite the cropped slice in a flat dark tint onto a white background — "source-in" recolors
 * every opaque pixel to `tint` while keeping the original alpha as a mask, then drawing that over
 * solid white flattens it to lerp(white, tint, alpha). That's exactly what multiply blending
 * needs: white outside the silhouette is a no-op, tint inside actually darkens, and the source
 * art's own edge antialiasing survives as the (tight, 1-2px) fade between them — not a hand-tuned
 * blur radius standing in for one. */
function makeSilhouetteShadowTexture(img: CanvasImageSource, sx: number, sy: number, sw: number, sh: number, tint = "rgb(12,10,9)"): THREE.CanvasTexture {
  const mask = document.createElement("canvas");
  mask.width = sw;
  mask.height = sh;
  const mctx = mask.getContext("2d")!;
  mctx.drawImage(img, sx, sy, sw, sh, 0, 0, sw, sh);
  mctx.globalCompositeOperation = "source-in";
  mctx.fillStyle = tint;
  mctx.fillRect(0, 0, sw, sh);

  const out = document.createElement("canvas");
  out.width = sw;
  out.height = sh;
  const octx = out.getContext("2d")!;
  octx.fillStyle = "white";
  octx.fillRect(0, 0, sw, sh);
  octx.drawImage(mask, 0, 0);
  return new THREE.CanvasTexture(out);
}

/** The wide, faint ambient-falloff pass that sits *under* the sharp silhouette core — real
 * contact shadows aren't uniformly soft, but the light that does bounce into the gap just past
 * the contact seam still falls off gradually, so this stays a plain gradient on purpose (it's the
 * "layer a slightly wider, softer, lighter pass underneath" step, not the shadow's shape). */
function makeAmbientFalloffTexture(): THREE.CanvasTexture {
  const c = document.createElement("canvas");
  c.width = c.height = 64;
  const ctx = c.getContext("2d")!;
  const g = ctx.createRadialGradient(32, 32, 0, 32, 32, 32);
  g.addColorStop(0, "rgb(150,140,128)");
  g.addColorStop(1, "rgb(255,255,255)");
  ctx.fillStyle = g;
  ctx.fillRect(0, 0, 64, 64);
  return new THREE.CanvasTexture(c);
}

/** Kael's real 36-frame idle sheet (kaelFinal — see assets.ts's spriteFrameSrc/loadGameArt for
 * the folder and cache-bust), animated the way the battle plays a long sheet: 36 frames over
 * LONG_ANIM_SECONDS, ping-ponging (see BattleEngine.idleFrame). */
const PREVIEW_FRAMES = 36;
const PREVIEW_FRAME_SRC = (i: number) => `/game/sprites/Kael_Final/kael-final-002/${i + 1}.png?v=kael-final-002`;
const PREVIEW_ANIM_SECONDS = 3;

/** Builds the live preview scene on `canvas` and returns a disposer. Kept as a plain function
 * (no React) so it can also be driven directly from a Playwright QA script the same way
 * scripts/shadow-qa.mjs drives ThreeBattleRenderer, instead of only being inspectable by eye
 * through the Dev Controls screen. */
export function mountDevGfxPreview(canvas: HTMLCanvasElement): () => void {
  let raf = 0;
  let disposed = false;

  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: false });
  renderer.setClearColor(0x1a140e, 1);
  renderer.shadowMap.enabled = true;
  // Same fallback as ThreeBattleRenderer: PCFSoftShadowMap was removed from this Three.js
  // version, so PCFShadowMap + a manual radius is the soft/hard knob here too.
  renderer.shadowMap.type = THREE.PCFShadowMap;

  const camera = new THREE.PerspectiveCamera(32, 1, 0.1, 50);
  camera.position.set(0, 3.1, 5.6);
  camera.lookAt(0, 0.7, 0);

  const resize = () => {
    const w = canvas.clientWidth || 280;
    const h = canvas.clientHeight || 180;
    renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
    renderer.setSize(w, h, false);
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
  };
  resize();
  const resizeObserver = new ResizeObserver(resize);
  resizeObserver.observe(canvas);

  const scene = new THREE.Scene();

  // MeshLambertMaterial, not MeshStandardMaterial — same choice ThreeBattleRenderer makes for
  // terrain (see its materialCache comment): Standard's specular lobe clips to solid white with
  // no tone mapping active, which was washing out the contact shadow sitting right on top of it.
  const ground = new THREE.Mesh(
    new THREE.CircleGeometry(3, 48),
    new THREE.MeshLambertMaterial({ color: 0x4a3c2a }),
  );
  ground.rotation.x = -Math.PI / 2;
  ground.receiveShadow = true;
  scene.add(ground);

  scene.add(new THREE.HemisphereLight(0xfff0d6, 0x140f0a, 0.6));

  // Parented under a pivot that spins slowly, so the shadow it casts sweeps across the ground
  // instead of sitting frozen — that motion is what makes hard vs. soft edges easy to spot.
  const sunPivot = new THREE.Group();
  scene.add(sunPivot);
  const sun = new THREE.DirectionalLight(0xfff0d6, 1.8);
  sun.position.set(2.6, 4.2, 1.6);
  sun.castShadow = true;
  sun.shadow.mapSize.set(1024, 1024);
  const shadowCam = sun.shadow.camera as THREE.OrthographicCamera;
  shadowCam.left = -2.5;
  shadowCam.right = 2.5;
  shadowCam.top = 2.5;
  shadowCam.bottom = -2.5;
  shadowCam.near = 0.1;
  shadowCam.far = 12;
  sunPivot.add(sun);
  sunPivot.add(sun.target);

  // Flat billboard quad + a separate alpha-tested, colorWrite:false copy that casts the real
  // shadow — same two-mesh split ThreeBattleRenderer uses for units (see its unitEntries
  // comment): the visible copy stays soft-edged/alpha-blended, the caster copy is a hard cutout
  // so the shadow map sees the sprite's actual silhouette instead of a filled rectangle.
  const bodyGroup = new THREE.Group();
  scene.add(bodyGroup);
  // footprintRig sits at the character's feet; footprintCore's LOCAL position inside it gets
  // biased opposite the light every frame (the "key light pushes the darkest core to one side"
  // rule) while footprintSoft stays centered — only the sharp part is directional, the ambient
  // falloff isn't.
  const footprintRig = new THREE.Group();
  footprintRig.position.y = 0.012;
  scene.add(footprintRig);
  let footprintCore: THREE.Mesh | null = null;
  // Every idle frame, in order; the body/caster/contact core swap to the current one each tick.
  const frames: THREE.Texture[] = [];
  const coreFrames: THREE.CanvasTexture[] = [];
  let visibleMat: THREE.MeshBasicMaterial | null = null;
  let casterMat: THREE.MeshBasicMaterial | null = null;
  let coreMat: THREE.MeshBasicMaterial | null = null;
  let shownFrame = -1;
  const startTime = performance.now();
  const sliceFrac = 0.12;
  const coreFor = (i: number): THREE.CanvasTexture => {
    if (!coreFrames[i]) {
      const img = frames[i]!.image as HTMLImageElement;
      const sliceH = img.height * sliceFrac;
      const t = makeSilhouetteShadowTexture(img, 0, img.height - sliceH, img.width, sliceH, "rgb(8,6,5)");
      t.colorSpace = THREE.SRGBColorSpace;
      t.generateMipmaps = false;
      t.minFilter = THREE.LinearFilter;
      t.magFilter = THREE.LinearFilter;
      coreFrames[i] = t;
    }
    return coreFrames[i]!;
  };

  const loader = new THREE.TextureLoader();
  let loaded = 0;
  for (let i = 0; i < PREVIEW_FRAMES; i++) {
    loader.load(PREVIEW_FRAME_SRC(i), (t) => {
      if (disposed) return;
      t.colorSpace = THREE.SRGBColorSpace;
      // Same as ThreeBattleRenderer's unitTextureFor — mipmapped sampling on a cutout sprite
      // bleeds the fully-transparent pixels' stored (often white) RGB into the alpha edge, which
      // showed up here as a bright white fringe along the cape/robe hem. Plain LinearFilter with
      // no mip chain samples the real edge pixels only, no bleed.
      t.generateMipmaps = false;
      t.minFilter = THREE.LinearFilter;
      t.magFilter = THREE.LinearFilter;
      t.wrapS = THREE.ClampToEdgeWrapping;
      t.wrapT = THREE.ClampToEdgeWrapping;
      frames[i] = t;
      if (++loaded === PREVIEW_FRAMES) buildFigure(frames[0]!);
    });
  }

  const buildFigure = (tex: THREE.Texture) => {
    const img0 = tex.image as HTMLImageElement;
    const aspect = img0.width / img0.height;
    const height = 1.7;
    const geo = new THREE.PlaneGeometry(height * aspect, height);

    visibleMat = new THREE.MeshBasicMaterial({ map: tex, transparent: true, depthWrite: false });
    const visible = new THREE.Mesh(geo, visibleMat);
    visible.position.y = height / 2;
    bodyGroup.add(visible);

    casterMat = new THREE.MeshBasicMaterial({ map: tex, alphaTest: 0.5, colorWrite: false, depthWrite: false, side: THREE.DoubleSide });
    const caster = new THREE.Mesh(geo, casterMat);
    caster.position.y = height / 2;
    caster.castShadow = true;
    bodyGroup.add(caster);

    // Wide, faint ambient pass first — sits centered under the feet, never moves. This is the
    // gradual bounce-light falloff a real contact shadow still has a little of, just not as its
    // whole shape.
    const soft = new THREE.Mesh(
      new THREE.PlaneGeometry(height * aspect * 1.15, height * 0.4),
      new THREE.MeshBasicMaterial({ map: makeAmbientFalloffTexture(), blending: THREE.MultiplyBlending, premultipliedAlpha: true, depthWrite: false, transparent: true }),
    );
    soft.rotation.x = -Math.PI / 2;
    footprintRig.add(soft);

    // The sharp core: only the bottom slice of the sprite (feet + hem, not the whole squashed
    // body) — that slice's own cutout IS the exact contact silhouette, alphaTest-cut with no
    // extra blur so it stays a crisp seam instead of another soft ellipse.
    coreMat = new THREE.MeshBasicMaterial({ map: coreFor(0), alphaTest: 0.5, blending: THREE.MultiplyBlending, premultipliedAlpha: true, depthWrite: false, transparent: true });
    footprintCore = new THREE.Mesh(new THREE.PlaneGeometry(height * aspect, height * sliceFrac), coreMat);
    footprintCore.rotation.x = -Math.PI / 2;
    footprintCore.position.y = 0.003;
    footprintRig.add(footprintCore);
    shownFrame = 0;
  };

  const worldSunPos = new THREE.Vector3();
  const CORE_LIGHT_BIAS = 0.14;

  const animate = () => {
    if (disposed) return;
    const gfx = getDevGfx();
    sun.castShadow = gfx.realShadows;
    sun.shadow.radius = gfx.softShadows ? SHADOW_RADIUS_SOFT : SHADOW_RADIUS_HARD;
    footprintRig.visible = gfx.contactShadows;
    if (footprintCore) {
      sun.getWorldPosition(worldSunPos);
      const dx = worldSunPos.x;
      const dz = worldSunPos.z;
      const len = Math.hypot(dx, dz) || 1;
      footprintCore.position.x = -(dx / len) * CORE_LIGHT_BIAS;
      footprintCore.position.z = -(dz / len) * CORE_LIGHT_BIAS;
    }
    if (shownFrame >= 0 && visibleMat && casterMat && coreMat) {
      // Same ping-pong idle as BattleEngine.idleFrame for a long sheet.
      const cycle = PREVIEW_FRAMES * 2 - 2;
      const x = Math.floor(((performance.now() - startTime) / 1000) * (PREVIEW_FRAMES / PREVIEW_ANIM_SECONDS)) % cycle;
      const f = x < PREVIEW_FRAMES ? x : cycle - x;
      if (f !== shownFrame) {
        visibleMat.map = casterMat.map = frames[f]!;
        coreMat.map = coreFor(f);
        shownFrame = f;
      }
    }
    sunPivot.rotation.y += 0.006;
    renderer.render(scene, camera);
    raf = requestAnimationFrame(animate);
  };
  raf = requestAnimationFrame(animate);

  return () => {
    disposed = true;
    cancelAnimationFrame(raf);
    resizeObserver.disconnect();
    renderer.dispose();
    ground.geometry.dispose();
    (ground.material as THREE.Material).dispose();
    for (const child of bodyGroup.children) {
      if (child instanceof THREE.Mesh) {
        child.geometry.dispose();
        (child.material as THREE.MeshBasicMaterial).dispose();
      }
    }
    for (const t of frames) t?.dispose();
    for (const t of coreFrames) t?.dispose();
    for (const child of footprintRig.children) {
      if (child instanceof THREE.Mesh) {
        child.geometry.dispose();
        const mat = child.material as THREE.MeshBasicMaterial;
        if (mat !== coreMat) mat.map?.dispose(); // the core's maps are coreFrames, freed above
        mat.dispose();
      }
    }
  };
}

/** Tiny live 3D scene for the Dev Controls screen — a standing figure on a ground disc, lit the
 * same way ThreeBattleRenderer lights a battle (PCFShadowMap sun + a separate contact-shadow
 * footprint), orbiting slowly so the cast shadow sweeps across the ground and its edge quality is
 * actually visible. Reads getDevGfx() every frame, same as the real renderer, so flipping a switch
 * in the panel updates this preview immediately with no need to open a fight to compare. */
export function DevGfxPreview() {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    return mountDevGfxPreview(canvas);
  }, []);

  return (
    <canvas
      ref={canvasRef}
      className="w-full h-44 rounded-xl border border-border bg-black/40"
      aria-label="Prévia ao vivo das sombras"
    />
  );
}
