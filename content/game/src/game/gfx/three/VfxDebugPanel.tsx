import { useEffect, useRef, useState, type MutableRefObject } from "react";
import * as THREE from "three";
import { EffectComposer } from "three/examples/jsm/postprocessing/EffectComposer.js";
import { RenderPass } from "three/examples/jsm/postprocessing/RenderPass.js";
import { UnrealBloomPass } from "three/examples/jsm/postprocessing/UnrealBloomPass.js";
import { OutputPass } from "three/examples/jsm/postprocessing/OutputPass.js";
import { DEFAULT_FIRE_EMITTER, ParticleEmitter, loadFireFlipbook, type FireEmitterSettings } from "./ThreeVfxSystem";
import { PIXEL_ELEMENT_PRESETS, ProceduralElementEmitter, pixelDefaults, pixelPreset, type PixelElement, type PixelElementSettings } from "./ProceduralElementEmitter";
import { DEFAULT_IMPACT_SETTINGS, FireballImpactVFX, getActiveImpactSettings, setActiveImpactSettings, type ImpactSettings } from "./FireballImpactVFX";
import { DEFAULT_PHANTASMAL_FORCE_SETTINGS, getActivePhantasmalForceSettings, PhantasmalForceVFX, setActivePhantasmalForceSettings, type PhantasmalForceSettings } from "./PhantasmalForceVFX";
import { BlessVFX, DEFAULT_BLESS_VFX_SETTINGS, getActiveBlessVfxSettings, setActiveBlessVfxSettings, type BlessVfxSettings } from "./BlessVFX";
import { DEFAULT_MAGIC_MISSILE_V2_SETTINGS, getActiveMagicMissileV2Settings, MagicMissileV2VFX, setActiveMagicMissileV2Settings, type MagicMissileV2Settings } from "./MagicMissileV2VFX";
import { DEFAULT_WEB_OF_DREAMS_VFX_SETTINGS, getActiveWebOfDreamsVfxSettings, setActiveWebOfDreamsVfxSettings, WebOfDreamsVFX, type WebOfDreamsVfxSettings } from "./WebOfDreamsVFX";
import { BurningHandsV2VFX, DEFAULT_BURNING_HANDS_V2_SETTINGS, getActiveBurningHandsV2Settings, setActiveBurningHandsV2Settings, type BurningHandsV2Settings } from "./BurningHandsV2VFX";
import { BurningHandsV3VFX, DEFAULT_BURNING_HANDS_V3_SETTINGS, getActiveBurningHandsV3Settings, setActiveBurningHandsV3Settings, type BurningHandsV3Settings } from "./BurningHandsV3VFX";
import { CleaveSweepVFX, DEFAULT_VARREDURA_SETTINGS, getActiveVarreduraSettings, setActiveVarreduraSettings, VarreduraVFX, type VarreduraSettings } from "./VarreduraVFX";

type PreviewMode = "flame" | `pixel-${Exclude<PixelElement, "fire">}` | `pixel-v2-${PixelElement}` | `pixel-v3-${PixelElement}` | "impact" | "phantasmal" | "bless" | "magic-missile-v2" | "web-of-dreams" | "burning-hands-v2" | "burning-hands-v3" | "varredura-v2" | "cleave-sweep-v2";

const pixelModeElement = (mode: PreviewMode): PixelElement | null =>
  mode.startsWith("pixel-v2-") || mode.startsWith("pixel-v3-") ? mode.slice(9) as PixelElement : mode.startsWith("pixel-") ? mode.slice(6) as Exclude<PixelElement, "fire"> : null;
const pixelModePresetId = (mode: PreviewMode): string | undefined => {
  const element = pixelModeElement(mode);
  const version = mode.startsWith("pixel-v3-") ? "v3_" : mode.startsWith("pixel-v2-") ? "v2_" : "";
  return element ? `procedural_pixel_${version}${element}` : undefined;
};
const pixelModeVersion = (mode: PreviewMode): 1 | 2 | 3 => mode.startsWith("pixel-v3-") ? 3 : mode.startsWith("pixel-v2-") ? 2 : 1;

type PreviewState = {
  settings: FireEmitterSettings;
  pixelSettings: PixelElementSettings;
  impactSettings: ImpactSettings;
  phantasmalSettings: PhantasmalForceSettings;
  blessSettings: BlessVfxSettings;
  magicMissileV2Settings: MagicMissileV2Settings;
  webDreamSettings: WebOfDreamsVfxSettings;
  burningHandsSettings: BurningHandsV2Settings;
  burningHandsV3Settings: BurningHandsV3Settings;
  varreduraSettings: VarreduraSettings;
  mode: PreviewMode;
  playing: boolean;
  looping: boolean;
  bloomEnabled: boolean;
};

type PreviewControls = {
  restart: () => void;
  frameStep: () => void;
  setPhantasmalSettings: (settings: PhantasmalForceSettings) => void;
  setBlessSettings: (settings: BlessVfxSettings) => void;
  setMagicMissileV2Settings: (settings: MagicMissileV2Settings) => void;
  setWebDreamSettings: (settings: WebOfDreamsVfxSettings) => void;
  setBurningHandsSettings: (settings: BurningHandsV2Settings) => void;
  setBurningHandsV3Settings: (settings: BurningHandsV3Settings) => void;
  setVarreduraSettings: (settings: VarreduraSettings) => void;
  releaseWebDreamPreview: () => void;
};

type ImpactSlider = { key: keyof ImpactSettings; label: string; min: number; max: number; step: number };
const IMPACT_GROUPS: { title: string; sliders: ImpactSlider[] }[] = [
  { title: "Flash de impacto", sliders: [
    { key: "flashIntensity", label: "Intensidade do flash", min: 2, max: 32, step: 0.5 },
    { key: "flashSize", label: "Tamanho do flash", min: 0.2, max: 1.8, step: 0.01 },
    { key: "flashDuration", label: "Duração do flash", min: 0.02, max: 0.15, step: 0.005 },
    { key: "flashOffset", label: "Atraso do flash", min: 0, max: 0.12, step: 0.005 },
  ] },
  { title: "Explosão principal", sliders: [
    { key: "primaryCount", label: "Partículas de fogo", min: 12, max: 120, step: 1 },
    { key: "primarySize", label: "Tamanho das chamas", min: 0.25, max: 1.5, step: 0.01 },
    { key: "primarySpeed", label: "Velocidade radial", min: 0.6, max: 7, step: 0.1 },
    { key: "primaryDrag", label: "Arrasto do fogo", min: 0.5, max: 10, step: 0.1 },
    { key: "primaryLife", label: "Duração do fogo", min: 0.15, max: 0.8, step: 0.01 },
    { key: "verticalSpread", label: "Abertura vertical", min: 0.2, max: 2.5, step: 0.01 },
    { key: "primaryEmission", label: "Emissão principal", min: 0.5, max: 5, step: 0.1 },
    { key: "primaryOffset", label: "Atraso do fogo", min: 0, max: 0.18, step: 0.005 },
  ] },
  { title: "Chamas secundárias e radiais", sliders: [
    { key: "secondaryCount", label: "Chamas secundárias", min: 0, max: 64, step: 1 },
    { key: "secondarySize", label: "Tamanho secundário", min: 0.3, max: 1.8, step: 0.01 },
    { key: "secondarySpeed", label: "Velocidade secundária", min: 0.2, max: 3.5, step: 0.05 },
    { key: "buoyancy", label: "Convecção ascendente", min: 0, max: 2, step: 0.01 },
    { key: "secondaryLife", label: "Duração secundária", min: 0.25, max: 1.1, step: 0.01 },
    { key: "secondaryOffset", label: "Atraso secundário", min: 0, max: 0.3, step: 0.005 },
    { key: "radialCount", label: "Línguas no chão", min: 0, max: 96, step: 1 },
    { key: "radialSpeed", label: "Velocidade no chão", min: 0.4, max: 5, step: 0.1 },
    { key: "radialOffset", label: "Atraso radial", min: 0, max: 0.25, step: 0.005 },
  ] },
  { title: "Faíscas e brasas", sliders: [
    { key: "sparkCount", label: "Quantidade de faíscas", min: 0, max: 200, step: 1 },
    { key: "sparkSpeed", label: "Velocidade das faíscas", min: 0.5, max: 12, step: 0.1 },
    { key: "sparkGravity", label: "Gravidade das faíscas", min: 0.5, max: 15, step: 0.1 },
    { key: "sparkDrag", label: "Arrasto das faíscas", min: 0, max: 3, step: 0.05 },
    { key: "sparkLife", label: "Duração das faíscas", min: 0.15, max: 1.2, step: 0.01 },
    { key: "sparkOffset", label: "Atraso das faíscas", min: 0, max: 0.2, step: 0.005 },
    { key: "emberCount", label: "Quantidade de brasas", min: 0, max: 72, step: 1 },
    { key: "emberSpeed", label: "Velocidade das brasas", min: 0.3, max: 5, step: 0.1 },
    { key: "emberGravity", label: "Gravidade das brasas", min: 0.5, max: 10, step: 0.1 },
    { key: "emberLife", label: "Duração das brasas", min: 0.2, max: 1.5, step: 0.01 },
    { key: "emberOffset", label: "Atraso das brasas", min: 0, max: 0.35, step: 0.005 },
  ] },
  { title: "Fumaça", sliders: [
    { key: "smokeCount", label: "Quantidade de fumaça", min: 0, max: 64, step: 1 },
    { key: "smokeSize", label: "Tamanho da fumaça", min: 0.3, max: 2, step: 0.01 },
    { key: "smokeRise", label: "Elevação da fumaça", min: 0.1, max: 2, step: 0.01 },
    { key: "smokeExpansion", label: "Expansão da fumaça", min: 0.1, max: 1.5, step: 0.01 },
    { key: "smokeOpacity", label: "Opacidade da fumaça", min: 0, max: 0.8, step: 0.01 },
    { key: "smokeLife", label: "Duração da fumaça", min: 0.4, max: 1.8, step: 0.01 },
    { key: "smokeOffset", label: "Atraso da fumaça", min: 0, max: 0.5, step: 0.005 },
  ] },
  { title: "Luz e animação", sliders: [
    { key: "peakLight", label: "Pico de luz", min: 0, max: 40, step: 0.5 },
    { key: "lightRadius", label: "Raio da luz", min: 1, max: 8, step: 0.1 },
    { key: "lightDecay", label: "Decaimento da luz", min: 0.3, max: 1.5, step: 0.01 },
    { key: "flipbookFps", label: "FPS do flipbook", min: 6, max: 36, step: 1 },
  ] },
];

type FireNumericKey = { [K in keyof FireEmitterSettings]-?: NonNullable<FireEmitterSettings[K]> extends number ? K : never }[keyof FireEmitterSettings];
const SLIDERS: { key: FireNumericKey; label: string; min: number; max: number; step: number }[] = [
  { key: "flipbookFps", label: "Taxa do flipbook", min: 6, max: 36, step: 1 },
  { key: "particleCount", label: "Quantidade de chamas", min: 12, max: 72, step: 1 },
  { key: "particleScale", label: "Escala da chama", min: 0.55, max: 1.5, step: 0.01 },
  { key: "velocity", label: "Velocidade de subida", min: 0.25, max: 1.6, step: 0.01 },
  { key: "drag", label: "Arrasto do ar", min: 0.2, max: 3, step: 0.01 },
  { key: "turbulence", label: "Turbulência", min: 0, max: 2, step: 0.01 },
  { key: "gravity", label: "Gravidade", min: 0, max: 1.2, step: 0.01 },
  { key: "coreIntensity", label: "Emissão da chama", min: 0.5, max: 3.5, step: 0.01 },
  { key: "lightIntensity", label: "Luz no terreno", min: 0, max: 12, step: 0.1 },
  { key: "lightRadius", label: "Alcance da luz", min: 1, max: 6, step: 0.1 },
];

const PHANTASMAL_SLIDERS: { key: keyof PhantasmalForceSettings; label: string; min: number; max: number; step: number; integer?: boolean }[] = [
  { key: "radius", label: "Raio do campo", min: 0.7, max: 3.2, step: 0.05 },
  { key: "tendrilCount", label: "Quantidade de tendrils", min: 4, max: 14, step: 1, integer: true },
  { key: "tendrilThickness", label: "Espessura dos tendrils", min: 0.01, max: 0.09, step: 0.002 },
  { key: "tendrilTurbulence", label: "Turbulência dos tendrils", min: 0, max: 0.5, step: 0.01 },
  { key: "splineNoise", label: "Ruído das curvas", min: 0, max: 0.7, step: 0.01 },
  { key: "particleCount", label: "Quantidade de partículas", min: 80, max: 520, step: 10, integer: true },
  { key: "particleAttraction", label: "Atração das partículas", min: 0.5, max: 5, step: 0.1 },
  { key: "spiralStrength", label: "Força espiral", min: 0, max: 3.5, step: 0.05 },
  { key: "compressionDuration", label: "Duração da compressão", min: 0.08, max: 0.24, step: 0.01 },
  { key: "coreSize", label: "Tamanho do núcleo", min: 0.04, max: 0.28, step: 0.01 },
  { key: "coreEmission", label: "Emissão do núcleo", min: 1, max: 14, step: 0.25 },
  { key: "buildupLight", label: "Luz de preparação", min: 0, max: 8, step: 0.1 },
  { key: "impactLight", label: "Luz do impacto", min: 1, max: 36, step: 0.5 },
  { key: "lightRadius", label: "Alcance da luz", min: 1, max: 8, step: 0.1 },
  { key: "shellRadius", label: "Raio da onda", min: 0.7, max: 3.5, step: 0.05 },
  { key: "shellSpeed", label: "Velocidade da onda", min: 3, max: 18, step: 0.25 },
  { key: "distortionStrength", label: "Distorção da onda", min: 0, max: 0.5, step: 0.01 },
  { key: "residualLifetime", label: "Duração residual", min: 0.2, max: 1.2, step: 0.02 },
];

type BlessNumericKey = { [K in keyof BlessVfxSettings]: BlessVfxSettings[K] extends number ? K : never }[keyof BlessVfxSettings];
type BlessToggleKey = { [K in keyof BlessVfxSettings]: BlessVfxSettings[K] extends boolean ? K : never }[keyof BlessVfxSettings];
const BLESS_SLIDERS: { key: BlessNumericKey; label: string; min: number; max: number; step: number; integer?: boolean }[] = [
  { key: "waveSpeed", label: "Velocidade da onda", min: 0.5, max: 1.8, step: 0.05 },
  { key: "waveRadius", label: "Raio da onda", min: 0.75, max: 1.15, step: 0.01 },
  { key: "waveHeight", label: "Altura da onda", min: 0.1, max: 0.8, step: 0.01 },
  { key: "waveThickness", label: "Espessura da onda", min: 0.04, max: 0.25, step: 0.01 },
  { key: "waveTurbulence", label: "Turbulência da onda", min: 0, max: 0.5, step: 0.01 },
  { key: "casterParticles", label: "Partículas do conjurador", min: 8, max: 64, step: 1, integer: true },
  { key: "allyParticles", label: "Partículas por aliado", min: 8, max: 48, step: 1, integer: true },
  { key: "strandCount", label: "Fios por aliado", min: 4, max: 10, step: 1, integer: true },
  { key: "strandThickness", label: "Espessura dos fios", min: 0.01, max: 0.06, step: 0.002 },
  { key: "strandHeight", label: "Altura dos fios", min: 0.35, max: 1.4, step: 0.02 },
  { key: "strandCurvature", label: "Curvatura dos fios", min: 0, max: 1.1, step: 0.02 },
  { key: "absorptionSpeed", label: "Velocidade de absorção", min: 0.5, max: 1.8, step: 0.05 },
  { key: "emissive", label: "Emissão HDR", min: 0, max: 10, step: 0.1 },
  { key: "casterLight", label: "Luz do conjurador", min: 0, max: 16, step: 0.25 },
  { key: "casterLightRadius", label: "Raio da luz central", min: 1, max: 9, step: 0.1 },
  { key: "allyLight", label: "Luz por aliado", min: 0, max: 8, step: 0.1 },
  { key: "allyLightRadius", label: "Raio da luz dos aliados", min: 0.5, max: 5, step: 0.1 },
  { key: "lightDecay", label: "Decaimento da luz", min: 0.4, max: 1.6, step: 0.05 },
  { key: "duration", label: "Duração total", min: 1, max: 2.4, step: 0.05 },
];

type MagicMissileNumericKey = { [K in keyof MagicMissileV2Settings]: MagicMissileV2Settings[K] extends number ? K : never }[keyof MagicMissileV2Settings];
type MagicMissileToggleKey = { [K in keyof MagicMissileV2Settings]: MagicMissileV2Settings[K] extends boolean ? K : never }[keyof MagicMissileV2Settings];
const MAGIC_MISSILE_V2_SLIDERS: { key: MagicMissileNumericKey; label: string; min: number; max: number; step: number; integer?: boolean }[] = [
  { key: "missileScale", label: "Escala do projétil", min: 0.08, max: 0.55, step: 0.01 },
  { key: "projectileSpeed", label: "Velocidade (hex/s)", min: 2, max: 12, step: 0.25 },
  { key: "acceleration", label: "Aceleração", min: 0.5, max: 3.5, step: 0.05 },
  { key: "trajectoryCurvature", label: "Curvatura", min: 0, max: 1.4, step: 0.02 },
  { key: "trajectoryHeight", label: "Altura da trajetória", min: 0, max: 1.8, step: 0.02 },
  { key: "seekingStrength", label: "Busca do alvo", min: 0, max: 1, step: 0.02 },
  { key: "turbulence", label: "Turbulência", min: 0, max: 0.5, step: 0.01 },
  { key: "shellDistortion", label: "Distorção da carapaça", min: 0, max: 1, step: 0.02 },
  { key: "trailLength", label: "Comprimento da trilha", min: 0.1, max: 0.8, step: 0.01 },
  { key: "trailThickness", label: "Espessura da trilha", min: 0.02, max: 0.2, step: 0.005 },
  { key: "trailFragmentation", label: "Fragmentação da trilha", min: 0, max: 1, step: 0.02 },
  { key: "particleCount", label: "Fragmentos por conjuração", min: 0, max: 80, step: 1, integer: true },
  { key: "emissive", label: "Emissão HDR", min: 0, max: 12, step: 0.1 },
  { key: "travelLightIntensity", label: "Luz em voo", min: 0, max: 14, step: 0.25 },
  { key: "travelLightRadius", label: "Raio da luz em voo", min: 0.5, max: 8, step: 0.1 },
  { key: "impactSize", label: "Tamanho do impacto", min: 0.1, max: 0.9, step: 0.01 },
  { key: "impactLightIntensity", label: "Luz do impacto", min: 0, max: 32, step: 0.5 },
  { key: "impactLightRadius", label: "Raio da luz de impacto", min: 0.5, max: 8, step: 0.1 },
  { key: "finalImpactMultiplier", label: "Multiplicador final", min: 1, max: 1.5, step: 0.01 },
];

type WebDreamNumericKey = { [K in keyof WebOfDreamsVfxSettings]: WebOfDreamsVfxSettings[K] extends number ? K : never }[keyof WebOfDreamsVfxSettings];
type WebDreamToggleKey = { [K in keyof WebOfDreamsVfxSettings]: WebOfDreamsVfxSettings[K] extends boolean ? K : never }[keyof WebOfDreamsVfxSettings];
const WEB_DREAM_SLIDERS: { key: WebDreamNumericKey; label: string; min: number; max: number; step: number; integer?: boolean }[] = [
  { key: "radius", label: "Raio do campo", min: 0.5, max: 2, step: 0.05 },
  { key: "seedCount", label: "Nós de ancoragem", min: 4, max: 12, step: 1, integer: true },
  { key: "filamentCount", label: "Fios da rede", min: 6, max: 40, step: 1, integer: true },
  { key: "targetFilamentCount", label: "Fios por alvo", min: 2, max: 12, step: 1, integer: true },
  { key: "filamentThickness", label: "Espessura dos fios", min: 0.004, max: 0.04, step: 0.001 },
  { key: "curvature", label: "Curvatura", min: 0, max: 1.2, step: 0.02 },
  { key: "verticalSpread", label: "Profundidade 3D", min: 0.1, max: 1.8, step: 0.05 },
  { key: "displacement", label: "Ondulação espacial", min: 0, max: 0.2, step: 0.005 },
  { key: "formationSpeed", label: "Velocidade de formação", min: 0.3, max: 2, step: 0.05 },
  { key: "pulseSpeed", label: "Velocidade do pulso", min: 0.2, max: 3, step: 0.05 },
  { key: "pulseBrightness", label: "Brilho do pulso", min: 0, max: 5, step: 0.1 },
  { key: "nodeCount", label: "Nós visíveis", min: 2, max: 12, step: 1, integer: true },
  { key: "nodeSize", label: "Tamanho dos nós", min: 0.025, max: 0.18, step: 0.005 },
  { key: "tightening", label: "Contração ao prender", min: 0, max: 0.7, step: 0.02 },
  { key: "centralLightIntensity", label: "Luz central", min: 0, max: 12, step: 0.2 },
  { key: "centralLightRadius", label: "Raio da luz central", min: 0.5, max: 10, step: 0.1 },
  { key: "secondaryLightCount", label: "Luzes secundárias", min: 0, max: 3, step: 1, integer: true },
  { key: "secondaryLightIntensity", label: "Intensidade secundária", min: 0, max: 8, step: 0.1 },
  { key: "secondaryLightRadius", label: "Raio secundário", min: 0.5, max: 8, step: 0.1 },
  { key: "bindingFlashIntensity", label: "Clarão ao prender", min: 0, max: 20, step: 0.25 },
  { key: "sustainedLightIntensity", label: "Luz sustentada", min: 0, max: 3, step: 0.05 },
  { key: "seed", label: "Semente", min: 1, max: 999999, step: 1, integer: true },
];

type BurningHandsNumericKey = { [K in keyof BurningHandsV2Settings]: BurningHandsV2Settings[K] extends number ? K : never }[keyof BurningHandsV2Settings];
type BurningHandsToggleKey = { [K in keyof BurningHandsV2Settings]: BurningHandsV2Settings[K] extends boolean ? K : never }[keyof BurningHandsV2Settings];
type BurningHandsV3ToggleKey = { [K in keyof BurningHandsV3Settings]: BurningHandsV3Settings[K] extends boolean ? K : never }[keyof BurningHandsV3Settings];
type VarreduraNumericKey = { [K in keyof VarreduraSettings]: VarreduraSettings[K] extends number ? K : never }[keyof VarreduraSettings];
type VarreduraToggleKey = { [K in keyof VarreduraSettings]: VarreduraSettings[K] extends boolean ? K : never }[keyof VarreduraSettings];
const VARREDURA_SLIDERS: { key: VarreduraNumericKey; label: string; min: number; max: number; step: number; integer?: boolean }[] = [
  { key: "angle", label: "Abertura do arco", min: 0.8, max: 3.1, step: 0.05 },
  { key: "radius", label: "Alcance em hexes", min: 1, max: 5, step: 0.1 },
  { key: "waveSpeed", label: "Velocidade de propagação", min: 0.25, max: 2.5, step: 0.05 },
  { key: "thickness", label: "Espessura da onda", min: 0.08, max: 1.1, step: 0.02 },
  { key: "height", label: "Altura da lâmina", min: 0.1, max: 2.5, step: 0.05 },
  { key: "leadingEdge", label: "Definição da borda", min: 0.01, max: 0.2, step: 0.005 },
  { key: "turbulence", label: "Turbulência", min: 0, max: 1, step: 0.02 },
  { key: "breakup", label: "Fragmentação da onda", min: 0, max: 1, step: 0.02 },
  { key: "trailLength", label: "Rastro de pressão", min: 0.2, max: 3, step: 0.05 },
  { key: "trailThickness", label: "Espessura do rastro", min: 0.02, max: 0.6, step: 0.01 },
  { key: "dustAmount", label: "Quantidade de poeira", min: 0, max: 2, step: 0.05 },
  { key: "debrisCount", label: "Detritos", min: 0, max: 120, step: 1, integer: true },
  { key: "debrisVelocity", label: "Velocidade dos detritos", min: 0.1, max: 4, step: 0.05 },
  { key: "impactSize", label: "Tamanho do impacto", min: 0.05, max: 1, step: 0.02 },
  { key: "spearLight", label: "Intensidade da luz da lâmina", min: 0, max: 12, step: 0.25 },
  { key: "spearRadius", label: "Alcance da luz da lâmina", min: 0.2, max: 5, step: 0.1 },
  { key: "impactLight", label: "Intensidade da luz de impacto", min: 0, max: 16, step: 0.25 },
  { key: "impactRadius", label: "Alcance da luz de impacto", min: 0.2, max: 5, step: 0.1 },
  { key: "residualDuration", label: "Duração residual", min: 0, max: 1.5, step: 0.05 },
  { key: "seed", label: "Semente procedural", min: 1, max: 999999, step: 1, integer: true },
];
const BURNING_HANDS_SLIDERS: { key: BurningHandsNumericKey; label: string; min: number; max: number; step: number; integer?: boolean }[] = [
  { key: "tongueCount", label: "Densidade de chamas", min: 8, max: 16, step: 1, integer: true },
  { key: "flameLength", label: "Alcance do jato", min: 0.6, max: 1.5, step: 0.05 },
  { key: "flameWidth", label: "Abertura da labareda", min: 0.65, max: 1.4, step: 0.05 },
  { key: "turbulence", label: "Turbulência orgânica", min: 0, max: 0.45, step: 0.01 },
  { key: "lightIntensity", label: "Intensidade das luzes", min: 0, max: 30, step: 0.5 },
  { key: "lightRadius", label: "Alcance das luzes", min: 1, max: 9, step: 0.1 },
];

function mountVfxPreview(canvas: HTMLCanvasElement, state: MutableRefObject<PreviewState>, controls: MutableRefObject<PreviewControls | null>): () => void {
  let disposed = false;
  let raf = 0;
  let last = performance.now();
  let simulationClock = 0;
  let emitter: ParticleEmitter | null = null;
  let pixelEmitter: ProceduralElementEmitter | null = null;
  let activePixelElement: string | null = null;
  let pixelClock = 0;
  let impact: FireballImpactVFX | null = null;
  let phantasmal: PhantasmalForceVFX | null = null;
  let bless: BlessVFX | null = null;
  let magicMissileV2: MagicMissileV2VFX | null = null;
  let webOfDreams: WebOfDreamsVFX | null = null;
  let burningHands: BurningHandsV2VFX | null = null;
  let burningHandsV3: BurningHandsV3VFX | null = null;
  let varredura: VarreduraVFX | null = null;
  let cleaveSweep: CleaveSweepVFX | null = null;
  let varreduraLayer: THREE.Group | null = null;
  let flipbook: THREE.Texture | null = null;
  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: false, powerPreference: "high-performance" });
  renderer.setPixelRatio(1);
  renderer.setClearColor(0x100e0b, 1);
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1;
  renderer.shadowMap.enabled = true;
  renderer.shadowMap.type = THREE.PCFShadowMap;

  const camera = new THREE.PerspectiveCamera(37, 1, 0.1, 30);
  camera.position.set(2.25, 2.75, 3.35);
  camera.lookAt(0, 0.48, 0);

  const baseScene = new THREE.Scene();
  baseScene.background = new THREE.Color(0x100e0b);
  baseScene.add(new THREE.HemisphereLight(0xbfc6d2, 0x17120d, 0.62));
  const key = new THREE.DirectionalLight(0xf0e5d2, 1.35);
  key.position.set(-3, 6, 4);
  key.castShadow = true;
  key.shadow.mapSize.set(512, 512);
  key.shadow.camera.left = -3;
  key.shadow.camera.right = 3;
  key.shadow.camera.top = 3;
  key.shadow.camera.bottom = -3;
  baseScene.add(key);
  baseScene.add(key.target);
  const pixelLight = new THREE.PointLight(0xffffff, 0, 4, 2);
  pixelLight.position.set(0, 0.8, 0);
  baseScene.add(pixelLight);

  const ground = new THREE.Mesh(
    new THREE.PlaneGeometry(200, 200),
    new THREE.MeshStandardMaterial({ color: 0x211c17, roughness: 1, metalness: 0 }),
  );
  ground.rotation.x = -Math.PI / 2;
  ground.position.y = -0.18;
  ground.receiveShadow = true;
  baseScene.add(ground);

  // A single battlefield hex is the fixed emitter anchor. The flat center and broken stone rim
  // give the warm point light real surfaces to illuminate, while the surrounding plane keeps
  // the light falloff legible in the preview.
  const hex = new THREE.Mesh(
    new THREE.CylinderGeometry(1.22, 1.22, 0.34, 6, 1, false),
    new THREE.MeshStandardMaterial({ color: 0x51473b, roughness: 0.94, metalness: 0.02 }),
  );
  hex.position.y = 0;
  hex.castShadow = true;
  hex.receiveShadow = true;
  baseScene.add(hex);
  const top = new THREE.Mesh(
    new THREE.CircleGeometry(1.18, 6),
    new THREE.MeshStandardMaterial({ color: 0x453b31, roughness: 0.98, metalness: 0 }),
  );
  top.rotation.x = -Math.PI / 2;
  top.rotation.z = Math.PI / 6;
  top.position.y = 0.174;
  top.receiveShadow = true;
  baseScene.add(top);

  const stoneMat = new THREE.MeshStandardMaterial({ color: 0x62594e, roughness: 0.89, metalness: 0.03 });
  const stoneMat2 = new THREE.MeshStandardMaterial({ color: 0x38332d, roughness: 0.97, metalness: 0.01 });
  const stones = new THREE.Group();
  let seed = 423;
  const rand = () => { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 4294967296; };
  for (let i = 0; i < 28; i++) {
    const angle = rand() * Math.PI * 2;
    const radius = 0.24 + rand() * 0.87;
    const stone = new THREE.Mesh(new THREE.DodecahedronGeometry(0.5, 0), i % 3 === 0 ? stoneMat2 : stoneMat);
    stone.position.set(Math.cos(angle) * radius, 0.19 + rand() * 0.035, Math.sin(angle) * radius);
    stone.scale.set(0.1 + rand() * 0.14, 0.035 + rand() * 0.028, 0.08 + rand() * 0.13);
    stone.rotation.set(rand() * 0.2, rand() * Math.PI, rand() * 0.2);
    stone.castShadow = true;
    stone.receiveShadow = true;
    stones.add(stone);
  }
  baseScene.add(stones);

  // Keep the spell preview focused on the effect and battlefield surface; do not add a
  // placeholder combatant/model to the FX lab.
  const phantasmalTarget = new THREE.Vector3(0, 0.78, 0);
  phantasmal = new PhantasmalForceVFX(baseScene);
  phantasmal.setSettings(state.current.phantasmalSettings);
  phantasmal.hide();
  bless = new BlessVFX(baseScene);
  bless.setSettings(state.current.blessSettings);
  bless.hide();
  magicMissileV2 = new MagicMissileV2VFX(baseScene);
  magicMissileV2.setSettings(state.current.magicMissileV2Settings);
  magicMissileV2.hide();
  webOfDreams = new WebOfDreamsVFX(baseScene);
  webOfDreams.setSettings(state.current.webDreamSettings);
  const previewWebOfDreams = () => {
    webOfDreams?.resetPreview();
    state.current.playing = true;
  };
  const previewBless = () => bless?.castSpell({
    id: "bless-preview",
    center: new THREE.Vector3(0, 0.18, 0.08),
    radiusWorld: 2.1,
    allies: [
      { id: "preview-near", position: new THREE.Vector3(0.56, -0.1, 0.05), distanceHexes: 1 },
      { id: "preview-mid", position: new THREE.Vector3(-0.92, 0.42, 0.1), distanceHexes: 2 },
      { id: "preview-edge", position: new THREE.Vector3(0.42, -1.16, 0.12), distanceHexes: 3 },
    ],
    onApply: () => {},
    onComplete: () => { state.current.playing = state.current.looping; },
  });
  const previewMagicMissileV2 = (target = new THREE.Vector3(0.95, 0.82, 0.12)) => magicMissileV2?.castSpell({
    id: "magic-missile-v2-preview",
    origin: new THREE.Vector3(-1.15, 1.18, 0.1),
    target,
    missileCount: 1,
    onImpact: () => {},
    onComplete: () => { state.current.playing = state.current.looping; },
  });
  const previewBurningHandsV2 = () => {
    burningHands?.dispose();
    burningHands = new BurningHandsV2VFX(baseScene, {
      id: "burning-hands-v2-preview",
      origin: new THREE.Vector3(-0.08, 0.23, 0),
      direction: new THREE.Vector2(0.34, 0.94).normalize(),
      length: 1.9,
      width: 1.55,
      worldScale: 1,
      settings: state.current.burningHandsSettings,
      onRelease: () => {},
      onComplete: () => {
        if (state.current.looping) previewBurningHandsV2();
        else state.current.playing = false;
      },
    });
    // The battle renderer uses XY as the map plane with Z as height; the FX Lab stage uses XZ with Y up.
    burningHands.group.rotation.x = -Math.PI / 2;
  };
  const previewBurningHandsV3 = () => {
    burningHandsV3?.dispose();
    burningHandsV3 = new BurningHandsV3VFX(baseScene, {
      id: "burning-hands-v3-preview",
      origin: new THREE.Vector3(-0.08, 0.23, 0),
      direction: new THREE.Vector2(0.34, 0.94).normalize(),
      length: 1.9,
      width: 1.55,
      worldScale: 1,
      settings: state.current.burningHandsV3Settings,
      onRelease: () => {},
      onComplete: () => {
        if (state.current.looping) previewBurningHandsV3();
        else state.current.playing = false;
      },
    });
    burningHandsV3.group.rotation.x = -Math.PI / 2;
  };
  const ensureVarreduraLayer = () => {
    if (varreduraLayer) return varreduraLayer;
    varreduraLayer = new THREE.Group();
    // Battle effects use XY as the map plane and Z as height; the lab ground is XZ/Y-up.
    varreduraLayer.rotation.x = -Math.PI / 2;
    fireScene.add(varreduraLayer);
    return varreduraLayer;
  };
  const previewVarredura = (origin = new THREE.Vector3(0, 0, 0.15)) => {
    varredura?.dispose();
    cleaveSweep?.dispose();
    const targets = [
      [2.55, 0], [1.8, 1.8], [0, 2.55], [-1.8, 1.8],
      [-2.55, 0], [-1.8, -1.8], [0, -2.55], [1.8, -1.8],
    ].map(([x, y], index) => ({ id: `preview-target-${index}`, position: new THREE.Vector3(origin.x + x!, origin.y + y!, 0.2) }));
    varredura = new VarreduraVFX(ensureVarreduraLayer(), origin, targets, 1);
    state.current.playing = true;
  };
  const previewCleaveSweep = (origin = new THREE.Vector3(-1.35, 0, 0.15)) => {
    cleaveSweep?.dispose();
    varredura?.dispose();
    const targets = [0.9, 1.75, 2.55].map((x, index) => ({ id: `preview-cleave-target-${index}`, position: new THREE.Vector3(origin.x + x, origin.y + (index === 1 ? 0.4 : -0.45), 0.2) }));
    cleaveSweep = new CleaveSweepVFX(ensureVarreduraLayer(), origin, targets, 1, { ...state.current.varreduraSettings });
    state.current.playing = true;
  };

  const baseTarget = new THREE.WebGLRenderTarget(1, 1, { depthBuffer: true, stencilBuffer: false });
  baseTarget.depthTexture = new THREE.DepthTexture(1, 1, THREE.UnsignedIntType);
  baseTarget.depthTexture.format = THREE.DepthFormat;
  baseTarget.depthTexture.minFilter = THREE.NearestFilter;
  baseTarget.depthTexture.magFilter = THREE.NearestFilter;
  const overlayTarget = new THREE.WebGLRenderTarget(1, 1, { depthBuffer: false, stencilBuffer: false });

  const fireScene = new THREE.Scene();
  const compositeScene = new THREE.Scene();
  const compositeCamera = new THREE.OrthographicCamera(-1, 1, 1, -1, 0, 1);
  const compositeMaterial = new THREE.ShaderMaterial({
    uniforms: { uBase: { value: baseTarget.texture }, uFire: { value: overlayTarget.texture } },
    vertexShader: `varying vec2 vUv; void main(){ vUv=uv; gl_Position=vec4(position.xy,0.0,1.0); }`,
    fragmentShader: `uniform sampler2D uBase; uniform sampler2D uFire; varying vec2 vUv; void main(){ vec4 base=texture2D(uBase,vUv); vec4 fire=texture2D(uFire,vUv); gl_FragColor=vec4(base.rgb*(1.0-fire.a)+fire.rgb,1.0); }`,
    depthTest: false,
    depthWrite: false,
    toneMapped: false,
  });
  const compositeQuad = new THREE.Mesh(new THREE.PlaneGeometry(2, 2), compositeMaterial);
  compositeScene.add(compositeQuad);
  const composer = new EffectComposer(renderer);
  composer.addPass(new RenderPass(compositeScene, compositeCamera));
  const bloomPass = new UnrealBloomPass(new THREE.Vector2(1, 1), 0.92, 0.34, 0.7);
  composer.addPass(bloomPass);
  composer.addPass(new OutputPass());

  const resize = () => {
    const w = Math.max(1, canvas.clientWidth);
    const h = Math.max(1, canvas.clientHeight);
    const dpr = Math.min(window.devicePixelRatio || 1, 1.5);
    const pw = Math.max(1, Math.floor(w * dpr));
    const ph = Math.max(1, Math.floor(h * dpr));
    renderer.setSize(pw, ph, false);
    canvas.width = pw;
    canvas.height = ph;
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
    baseTarget.setSize(pw, ph);
    overlayTarget.setSize(pw, ph);
    composer.setSize(pw, ph);
    if (emitter) (emitter.material.uniforms.uViewport!.value as THREE.Vector2).set(pw, ph);
  };
  resize();
  const observer = new ResizeObserver(resize);
  observer.observe(canvas);

  controls.current = {
    restart: () => {
      if (pixelModeElement(state.current.mode)) { pixelClock = 0; pixelEmitter?.dispose(); pixelEmitter = null; activePixelElement = null; return; }
      if (state.current.mode === "phantasmal" && phantasmal) {
        phantasmal.restartAt(phantasmalTarget, 1, { onComplete: () => { state.current.playing = state.current.looping; } });
        return;
      }
      if (state.current.mode === "bless") { previewBless(); return; }
      if (state.current.mode === "magic-missile-v2") { previewMagicMissileV2(); return; }
      if (state.current.mode === "web-of-dreams") { previewWebOfDreams(); return; }
      if (state.current.mode === "burning-hands-v2") { previewBurningHandsV2(); return; }
      if (state.current.mode === "burning-hands-v3") { previewBurningHandsV3(); return; }
      if (state.current.mode === "varredura-v2") { previewVarredura(); return; }
      if (state.current.mode === "cleave-sweep-v2") { previewCleaveSweep(); return; }
      if (state.current.mode === "impact" && impact) { impact.restart(state.current.impactSettings); return; }
      if (!emitter) return;
      simulationClock = 0;
      emitter.reset(state.current.settings, true);
    },
    frameStep: () => {
      if (pixelModeElement(state.current.mode)) {
        pixelClock += 1 / 12;
        pixelEmitter?.update(1 / 12, 1, pixelClock, 0, -0.13);
        return;
      }
      if (state.current.mode === "phantasmal" && phantasmal) { phantasmal.update(1 / 24); return; }
      if (state.current.mode === "bless" && bless) { bless.update(1 / 24); return; }
      if (state.current.mode === "magic-missile-v2" && magicMissileV2) { magicMissileV2.update(1 / 24); return; }
      if (state.current.mode === "web-of-dreams") { webOfDreams?.previewAt(new THREE.Vector3(0, 0.8, 0), 1, 1.9, [{ id: "preview-target", position: new THREE.Vector3(0, 0.78, 0) }], 1 / 24); return; }
      if (state.current.mode === "burning-hands-v2") { burningHands?.update(1 / 24); return; }
      if (state.current.mode === "burning-hands-v3") { burningHandsV3?.update(1 / 24); return; }
      if (state.current.mode === "varredura-v2") { varredura?.update(1 / 24); return; }
      if (state.current.mode === "cleave-sweep-v2") { cleaveSweep?.update(1 / 24); return; }
      if (state.current.mode === "impact" && impact) { impact.update(1 / Math.max(1, state.current.impactSettings.flipbookFps), state.current.impactSettings, state.current.looping, camera); return; }
      if (!emitter) return;
      const s = state.current.settings;
      emitter.update(1 / Math.max(1, s.flipbookFps), s, state.current.looping);
      simulationClock += 1 / Math.max(1, s.flipbookFps);
      if (flipbook) emitter.material.uniforms.uFlipbook!.value = flipbook;
    },
    setPhantasmalSettings: (next) => phantasmal?.setSettings(next),
    setBlessSettings: (next) => bless?.setSettings(next),
    setMagicMissileV2Settings: (next) => magicMissileV2?.setSettings(next),
    setWebDreamSettings: (next) => webOfDreams?.setSettings(next),
    setBurningHandsSettings: (next) => burningHands?.setSettings(next),
    setBurningHandsV3Settings: (next) => burningHandsV3?.setSettings(next),
    setVarreduraSettings: (next) => { state.current.varreduraSettings = next; if (state.current.mode === "cleave-sweep-v2") previewCleaveSweep(); },
    releaseWebDreamPreview: () => webOfDreams?.releasePreview(),
  };

  void loadFireFlipbook().then((texture) => {
    if (disposed) { texture.dispose(); return; }
    flipbook = texture;
    emitter = new ParticleEmitter(texture, baseTarget.depthTexture!, {
      sceneNear: camera.near,
      sceneFar: camera.far,
      viewport: new THREE.Vector2(canvas.width, canvas.height),
      intensity: state.current.settings.coreIntensity,
    });
    emitter.reset(state.current.settings, true);
    emitter.mesh.position.set(0, 0.13, 0);
    fireScene.add(emitter.mesh);
    emitter.light.position.set(0, 0.55, 0);
    emitter.light.castShadow = false;
    emitter.light.shadow.mapSize.set(512, 512);
    emitter.light.shadow.camera.near = 0.1;
    emitter.light.shadow.camera.far = 8;
    baseScene.add(emitter.light);
    impact = new FireballImpactVFX(texture, baseTarget.depthTexture!, camera.near, camera.far, new THREE.Vector2(canvas.width, canvas.height));
    impact.setOrigin(new THREE.Vector3(0, 0.19, 0));
    impact.mesh.visible = false;
    impact.flash.visible = false;
    fireScene.add(impact.mesh, impact.flash);
    baseScene.add(impact.light);
    resize();
  });

  const raycaster = new THREE.Raycaster();
  const pointer = new THREE.Vector2();
  const placeImpact = (event: PointerEvent) => {
    if (state.current.mode === "varredura-v2" || state.current.mode === "cleave-sweep-v2") {
      const rect = canvas.getBoundingClientRect();
      pointer.set(((event.clientX - rect.left) / rect.width) * 2 - 1, -((event.clientY - rect.top) / rect.height) * 2 + 1);
      raycaster.setFromCamera(pointer, camera);
      const hit = raycaster.intersectObject(top, false)[0];
      if (!hit) return;
      const origin = new THREE.Vector3(hit.point.x, hit.point.z, 0.15);
      if (state.current.mode === "varredura-v2") previewVarredura(origin);
      else previewCleaveSweep(origin);
      return;
    }
    if (state.current.mode === "phantasmal" && phantasmal) {
      const rect = canvas.getBoundingClientRect();
      pointer.set(((event.clientX - rect.left) / rect.width) * 2 - 1, -((event.clientY - rect.top) / rect.height) * 2 + 1);
      raycaster.setFromCamera(pointer, camera);
      const hit = raycaster.intersectObject(top, false)[0];
      if (!hit) return;
      const position = hit.point.clone().setY(0.78);
      phantasmal.restartAt(position, 1, { onComplete: () => { state.current.playing = state.current.looping; } });
      state.current.playing = true;
      return;
    }
    if (state.current.mode === "magic-missile-v2" && magicMissileV2) {
      const rect = canvas.getBoundingClientRect();
      pointer.set(((event.clientX - rect.left) / rect.width) * 2 - 1, -((event.clientY - rect.top) / rect.height) * 2 + 1);
      raycaster.setFromCamera(pointer, camera);
      const hit = raycaster.intersectObject(top, false)[0];
      if (!hit) return;
      previewMagicMissileV2(hit.point.clone().setY(0.82));
      state.current.playing = true;
      return;
    }
    if (state.current.mode !== "impact" || !impact) return;
    const rect = canvas.getBoundingClientRect();
    pointer.set(((event.clientX - rect.left) / rect.width) * 2 - 1, -((event.clientY - rect.top) / rect.height) * 2 + 1);
    raycaster.setFromCamera(pointer, camera);
    const hit = raycaster.intersectObject(top, false)[0];
    if (!hit) return;
    impact.setOrigin(hit.point);
    impact.restart(state.current.impactSettings);
    state.current.playing = true;
  };
  canvas.addEventListener("pointerdown", placeImpact);

  const animate = (now: number) => {
    if (disposed) return;
    raf = requestAnimationFrame(animate);
    const dt = Math.min(0.05, Math.max(0, (now - last) / 1000));
    last = now;
    if (state.current.mode === "varredura-v2") {
      camera.position.set(0, 8.5, 12.5);
      camera.lookAt(0, 0.15, 0);
    } else if (state.current.mode === "phantasmal") {
      camera.position.set(3.2, 3.1, 5.8);
      camera.lookAt(0, 0.62, 0);
    } else if (state.current.mode === "bless" || state.current.mode === "magic-missile-v2") {
      camera.position.set(0.15, 2.7, 5.7);
      camera.lookAt(0, 0.15, 0);
    } else if (state.current.mode === "web-of-dreams") {
      camera.position.set(0.2, 2.45, 5.4);
      camera.lookAt(0, 0.72, 0);
    } else {
      camera.position.set(2.25, 2.75, 3.35);
      camera.lookAt(0, 0.48, 0);
    }
    bloomPass.enabled = state.current.mode === "bless" ? state.current.blessSettings.bloom : state.current.mode === "magic-missile-v2" ? state.current.magicMissileV2Settings.bloom : state.current.mode === "burning-hands-v2" ? state.current.burningHandsSettings.bloom : state.current.mode === "burning-hands-v3" ? state.current.burningHandsV3Settings.bloom : state.current.bloomEnabled;
    if (state.current.mode !== "phantasmal") phantasmal?.hide();
    else if (phantasmal && state.current.playing) phantasmal.update(dt);
    if (state.current.mode !== "bless") bless?.hide();
    else if (bless && state.current.playing) bless.update(dt);
    if (state.current.mode !== "magic-missile-v2") magicMissileV2?.hide();
    else if (magicMissileV2 && state.current.playing) magicMissileV2.update(dt);
    if (state.current.mode !== "web-of-dreams") webOfDreams?.resetPreview();
    else if (webOfDreams && state.current.playing) webOfDreams.previewAt(
      new THREE.Vector3(0, 0.8, 0), 1, 1.9,
      [{ id: "preview-target", position: new THREE.Vector3(0, 0.78, 0) }], dt,
    );
    if (state.current.mode !== "burning-hands-v2") burningHands?.dispose();
    else if (burningHands && state.current.playing) burningHands.update(dt);
    if (state.current.mode !== "burning-hands-v3") burningHandsV3?.dispose();
    else if (burningHandsV3 && state.current.playing) burningHandsV3.update(dt);
    if (state.current.mode !== "varredura-v2") { varredura?.dispose(); varredura = null; }
    else if (varredura && state.current.playing) { varredura.update(dt); if (varredura.finished) { if (state.current.looping) previewVarredura(); else state.current.playing=false; } }
    if (state.current.mode !== "cleave-sweep-v2") { cleaveSweep?.dispose(); cleaveSweep = null; }
    else if (cleaveSweep && state.current.playing) { cleaveSweep.update(dt); if (cleaveSweep.finished) { if (state.current.looping) previewCleaveSweep(); else state.current.playing = false; } }
    const chosenPixel = pixelModeElement(state.current.mode);
    const chosenPixelPresetId = pixelModePresetId(state.current.mode);
    if (chosenPixel && chosenPixelPresetId) {
      if (!pixelEmitter || activePixelElement !== chosenPixelPresetId) {
        pixelEmitter?.dispose();
        pixelEmitter = new ProceduralElementEmitter(fireScene, pixelPreset(chosenPixel, chosenPixelPresetId), state.current.pixelSettings);
        activePixelElement = chosenPixelPresetId;
        pixelClock = 0;
      }
      if (state.current.playing) pixelClock += dt;
      pixelEmitter.setSettings(state.current.pixelSettings);
      pixelEmitter.update(state.current.playing ? dt : 0, 1, pixelClock, 0, -0.13);
      const sample = pixelEmitter.getLightSample(0, 0, 1);
      if (sample) {
        const peak = Math.max(...sample.rgb, 1e-6);
        pixelLight.color.setRGB(sample.rgb[0]/peak, sample.rgb[1]/peak, sample.rgb[2]/peak);
        pixelLight.intensity = peak * 35;
        pixelLight.distance = sample.r * 1.5;
      } else pixelLight.intensity = 0;
    } else {
      pixelEmitter?.dispose();
      pixelEmitter = null;
      activePixelElement = null;
      pixelLight.intensity = 0;
    }
    if (emitter) {
      const s = state.current.settings;
      emitter.material.uniforms.uSceneDepth!.value = baseTarget.depthTexture;
      emitter.material.uniforms.uViewport!.value.set(canvas.width, canvas.height);
      emitter.material.uniforms.uIntensity!.value = s.coreIntensity;
      if (state.current.mode === "flame") {
        emitter.mesh.visible = true;
        emitter.light.visible = true;
        if (impact) { impact.mesh.visible = false; impact.flash.visible = false; impact.light.visible = false; }
        if (state.current.playing) {
          emitter.update(dt, s, state.current.looping);
          simulationClock += dt;
          if (!state.current.looping && emitter.particles.slice(0, s.particleCount).every((p) => p.age >= p.life)) {
            state.current.playing = false;
            emitter.light.intensity = 0;
          }
        } else {
          emitter.light.intensity = s.lightIntensity;
          emitter.light.distance = s.lightRadius;
          emitter.setFrame(s);
        }
        if (state.current.looping && simulationClock > 30) simulationClock %= 30;
      } else {
        emitter.mesh.visible = false;
        emitter.light.visible = false;
        if (state.current.mode === "impact" && impact) {
          impact.mesh.visible = true;
          impact.light.visible = true;
          const fx = state.current.impactSettings;
          if (state.current.playing) impact.update(dt, fx, state.current.looping, camera);
          else impact.update(0, fx, state.current.looping, camera);
        } else {
          if (impact) { impact.mesh.visible = false; impact.flash.visible = false; impact.light.visible = false; }
        }
      }
    }

    renderer.setRenderTarget(baseTarget);
    renderer.clear(true, true, true);
    renderer.render(baseScene, camera);
    renderer.setRenderTarget(overlayTarget);
    renderer.setClearColor(0x000000, 0);
    renderer.clear(true, true, true);
    renderer.render(fireScene, camera);
    renderer.setRenderTarget(null);
    composer.render(dt);
  };
  raf = requestAnimationFrame(animate);

  return () => {
    disposed = true;
    cancelAnimationFrame(raf);
    observer.disconnect();
    controls.current = null;
    canvas.removeEventListener("pointerdown", placeImpact);
    emitter?.dispose();
    pixelEmitter?.dispose();
    impact?.dispose();
    phantasmal?.dispose();
    bless?.dispose();
    magicMissileV2?.dispose();
    webOfDreams?.dispose();
    burningHands?.dispose();
    varredura?.dispose();
    cleaveSweep?.dispose();
    if (varreduraLayer) fireScene.remove(varreduraLayer);
    flipbook?.dispose();
    renderer.dispose();
    composer.dispose();
    baseTarget.dispose();
    overlayTarget.dispose();
    (compositeQuad.geometry as THREE.BufferGeometry).dispose();
    compositeMaterial.dispose();
    ground.geometry.dispose();
    (ground.material as THREE.Material).dispose();
    hex.geometry.dispose();
    (hex.material as THREE.Material).dispose();
    top.geometry.dispose();
    (top.material as THREE.Material).dispose();
    for (const stone of stones.children) {
      if (stone instanceof THREE.Mesh) stone.geometry.dispose();
    }
    stoneMat.dispose();
    stoneMat2.dispose();
  };
}

export function VfxDebugPanel() {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const runtimeRef = useRef<PreviewControls | null>(null);
  const [settings, setSettings] = useState(DEFAULT_FIRE_EMITTER);
  const [pixelSettings, setPixelSettings] = useState<PixelElementSettings>(() => pixelDefaults("frost"));
  const [impactSettings, setImpactSettings] = useState(DEFAULT_IMPACT_SETTINGS);
  const [phantasmalSettings, setPhantasmalSettings] = useState(() => getActivePhantasmalForceSettings());
  const [blessSettings, setBlessSettings] = useState(() => getActiveBlessVfxSettings());
  const [magicMissileV2Settings, setMagicMissileV2Settings] = useState(() => getActiveMagicMissileV2Settings());
  const [webDreamSettings, setWebDreamSettings] = useState(() => getActiveWebOfDreamsVfxSettings());
  const [burningHandsSettings, setBurningHandsSettings] = useState(() => getActiveBurningHandsV2Settings());
  const [burningHandsV3Settings, setBurningHandsV3Settings] = useState(() => getActiveBurningHandsV3Settings());
  const [varreduraSettings, setVarreduraSettings] = useState(() => getActiveVarreduraSettings());
  const [mode, setMode] = useState<PreviewMode>("flame");
  const [playing, setPlaying] = useState(true);
  const [looping, setLooping] = useState(true);
  const [bloomEnabled, setBloomEnabled] = useState(false);
  const stateRef = useRef<PreviewState>({ settings, pixelSettings, impactSettings, phantasmalSettings, blessSettings, magicMissileV2Settings, webDreamSettings, burningHandsSettings, burningHandsV3Settings, varreduraSettings, mode, playing, looping, bloomEnabled });
  stateRef.current = { settings, pixelSettings, impactSettings, phantasmalSettings, blessSettings, magicMissileV2Settings, webDreamSettings, burningHandsSettings, burningHandsV3Settings, varreduraSettings, mode, playing, looping, bloomEnabled };

  const switchMode = (next: PreviewMode) => {
    const element = pixelModeElement(next);
    if (element) { const defaults = pixelDefaults(element, pixelModeVersion(next)); stateRef.current.pixelSettings = defaults; setPixelSettings(defaults); }
    stateRef.current.mode = next;
    stateRef.current.playing = true;
    setMode(next);
    setPlaying(true);
    runtimeRef.current?.restart();
  };

  const updateImpactSetting = (key: keyof ImpactSettings, value: number) => {
    const next = { ...stateRef.current.impactSettings, [key]: value };
    stateRef.current.impactSettings = next;
    setActiveImpactSettings(next);
    stateRef.current.playing = true;
    setImpactSettings(next);
    setPlaying(true);
    runtimeRef.current?.restart();
  };

  const updatePhantasmalSetting = (key: keyof PhantasmalForceSettings, value: number) => {
    const next = { ...stateRef.current.phantasmalSettings, [key]: value };
    stateRef.current.phantasmalSettings = next;
    setActivePhantasmalForceSettings(next);
    runtimeRef.current?.setPhantasmalSettings(next);
    setPhantasmalSettings(next);
  };

  const updateBlessSetting = <K extends keyof BlessVfxSettings>(key: K, value: BlessVfxSettings[K]) => {
    const next = { ...stateRef.current.blessSettings, [key]: value };
    stateRef.current.blessSettings = next;
    setActiveBlessVfxSettings(next);
    runtimeRef.current?.setBlessSettings(next);
    setBlessSettings(next);
  };

  const updateMagicMissileV2Setting = <K extends keyof MagicMissileV2Settings>(key: K, value: MagicMissileV2Settings[K]) => {
    const next = { ...stateRef.current.magicMissileV2Settings, [key]: value };
    stateRef.current.magicMissileV2Settings = next;
    setActiveMagicMissileV2Settings(next);
    runtimeRef.current?.setMagicMissileV2Settings(next);
    setMagicMissileV2Settings(next);
  };

  const updateWebDreamSetting = <K extends keyof WebOfDreamsVfxSettings>(key: K, value: WebOfDreamsVfxSettings[K]) => {
    const next = { ...stateRef.current.webDreamSettings, [key]: value };
    stateRef.current.webDreamSettings = next;
    setActiveWebOfDreamsVfxSettings(next);
    runtimeRef.current?.setWebDreamSettings(next);
    setWebDreamSettings(next);
  };

  const updateBurningHandsSetting = <K extends keyof BurningHandsV2Settings>(key: K, value: BurningHandsV2Settings[K]) => {
    const next = { ...stateRef.current.burningHandsSettings, [key]: value };
    stateRef.current.burningHandsSettings = next;
    setActiveBurningHandsV2Settings(next);
    runtimeRef.current?.setBurningHandsSettings(next);
    setBurningHandsSettings(next);
  };

  const updateBurningHandsV3Setting = <K extends keyof BurningHandsV3Settings>(key: K, value: BurningHandsV3Settings[K]) => {
    const next = { ...stateRef.current.burningHandsV3Settings, [key]: value };
    stateRef.current.burningHandsV3Settings = next;
    setActiveBurningHandsV3Settings(next);
    runtimeRef.current?.setBurningHandsV3Settings(next);
    setBurningHandsV3Settings(next);
  };

  const updateVarreduraSetting = <K extends keyof VarreduraSettings>(key:K,value:VarreduraSettings[K])=>{
    const next={...stateRef.current.varreduraSettings,[key]:value};stateRef.current.varreduraSettings=next;setActiveVarreduraSettings(next);setVarreduraSettings(next);runtimeRef.current?.setVarreduraSettings(next);
  };

  const restart = () => { stateRef.current.playing = true; setPlaying(true); runtimeRef.current?.restart(); };

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const savedImpactSettings = getActiveImpactSettings();
    const savedPhantasmalSettings = getActivePhantasmalForceSettings();
    const savedBlessSettings = getActiveBlessVfxSettings();
    const savedMagicMissileV2Settings = getActiveMagicMissileV2Settings();
    const savedWebDreamSettings = getActiveWebOfDreamsVfxSettings();
    const savedBurningHandsSettings = getActiveBurningHandsV2Settings();
    const savedBurningHandsV3Settings = getActiveBurningHandsV3Settings();
    const savedVarreduraSettings = getActiveVarreduraSettings();
    stateRef.current.impactSettings = savedImpactSettings;
    stateRef.current.phantasmalSettings = savedPhantasmalSettings;
    stateRef.current.blessSettings = savedBlessSettings;
    stateRef.current.magicMissileV2Settings = savedMagicMissileV2Settings;
    stateRef.current.webDreamSettings = savedWebDreamSettings;
    stateRef.current.burningHandsSettings = savedBurningHandsSettings;
    stateRef.current.burningHandsV3Settings = savedBurningHandsV3Settings;
    stateRef.current.varreduraSettings = savedVarreduraSettings;
    setImpactSettings(savedImpactSettings);
    setPhantasmalSettings(savedPhantasmalSettings);
    setBlessSettings(savedBlessSettings);
    setMagicMissileV2Settings(savedMagicMissileV2Settings);
    setWebDreamSettings(savedWebDreamSettings);
    setBurningHandsSettings(savedBurningHandsSettings);
    setBurningHandsV3Settings(savedBurningHandsV3Settings);
    setVarreduraSettings(savedVarreduraSettings);
    return mountVfxPreview(canvas, stateRef, runtimeRef);
  }, []);

  return (
    <section className="flex flex-col gap-3 rounded-xl border border-border bg-bg/40 p-4" aria-label="VFX debug editor">
      <div>
        <p className="text-sm uppercase tracking-[0.14em] text-muted">Laboratório VFX · etapa {mode === "flame" ? "01" : mode === "phantasmal" ? "03" : mode === "bless" ? "04" : mode === "magic-missile-v2" ? "05" : mode === "web-of-dreams" ? "06" : mode === "burning-hands-v2" || mode === "burning-hands-v3" ? "07" : mode === "varredura-v2" ? "08" : mode === "cleave-sweep-v2" ? "09" : "02"}</p>
        <label className="mt-2 flex flex-col gap-1 text-sm">
          <span className="text-muted">Efeito</span>
          <select aria-label="Selecionar efeito VFX" value={mode} onChange={(event) => switchMode(event.target.value as PreviewMode)} className="min-h-11 rounded-md border border-border bg-bg px-3 py-2 font-display text-lg text-fg focus:border-accent focus:outline-none">
            <option value="flame">Emissor de fogo estacionário</option>
            {PIXEL_ELEMENT_PRESETS.filter((entry) => entry.version >= 2 || entry.element !== "fire").map((entry) => <option key={entry.id} value={`pixel-${entry.version >= 2 ? `v${entry.version}-` : ""}${entry.element}`}>{entry.label} · estacionário</option>)}
            <option value="impact">Explosão de impacto Fireball · original</option>
            <option value="phantasmal">Força Fantasmal · 3D compressão espectral</option>
            <option value="bless">Bless · onda dourada 3D e luz real</option>
          <option value="magic-missile-v2">Míssil Mágico V2 · projétil arcano 3D</option>
            <option value="web-of-dreams">Web of Dreams · rede 3D persistente</option>
            <option value="burning-hands-v2">Burning Hands V2 · leque de fogo 3D</option>
            <option value="burning-hands-v3">Burning Hands V3 · leque de fogo 3D</option>
            <option value="varredura-v2">Varredura V2 · onda de choque circular</option>
            <option value="cleave-sweep-v2">Cleave · varredura da lança 3D</option>
          </select>
        </label>
        <p className="text-sm text-muted mt-1">{mode === "flame" ? "Chama contínua ancorada em um hex de batalha." : pixelModeElement(mode) ? "Emissor elemental de 16 quadros, fragmentos instanciados e luz real no terreno. Os ajustes também podem ser usados no editor de mapas." : mode === "phantasmal" ? "Força 3D que envolve o alvo, comprime energia espectral para dentro e libera uma onda real no espaço. Clique no hex para reposicionar." : mode === "bless" ? "Bless reúne energia no conjurador, propaga a onda por três hexes e envolve cada aliado na ordem em que ela chega. A luz real e o bônus são os mesmos usados no combate." : mode === "magic-missile-v2" ? "Um projétil arcano 3D se forma junto ao conjurador, ilumina o campo, percorre uma curva visível e colapsa no alvo. Cada disparo da magia recebe seu próprio efeito. Clique no tabuleiro para trocar o alvo." : mode === "web-of-dreams" ? "Fios volumétricos crescem ao redor do alvo, ligam nós de energia e se contraem ao prendê-lo. Teste profundidade, geometria e luzes dinâmicas reais abaixo." : mode === "burning-hands-v2" ? "As mãos acendem, comprimem o fogo e liberam um leque largo de línguas volumétricas. As luzes reais percorrem o cone; o efeito usa os hexes já resolvidos pelo combate." : mode === "burning-hands-v3" ? "Preview V3 separado no FX Lab; não altera o efeito de batalha V2." : mode === "varredura-v2" ? "Uma frente de choque circular se expande em 360° a partir do centro e ilumina os alvos quando os alcança. Clique no chão para reposicionar o centro." : mode === "cleave-sweep-v2" ? "Um golpe direcional de lança varre a área de Cleave, com fragmentos e impactos iluminados em sequência. Ajuste geometria e luzes abaixo; clique no chão para reposicionar." : "Clique no hex para posicionar e repetir a explosão original. Câmera fixa; sem projétil ou AOE. Ajustes salvos automaticamente neste navegador e aplicados às próximas conjurações de Fireball."}</p>
      </div>
      <canvas ref={canvasRef} onPointerDown={() => { if (mode !== "flame") { stateRef.current.playing = true; setPlaying(true); } }} className={`w-full h-80 rounded-lg border border-border bg-black/40 ${mode !== "flame" ? "cursor-crosshair" : ""}`} aria-label="3D spell effect preview" />
      <div className="grid grid-cols-2 gap-2">
        <button type="button" onClick={restart} className="min-h-11 rounded-md border border-accent bg-accent/10 px-3 py-2 font-medium hover:bg-accent/20">Reproduzir</button>
        <button type="button" onClick={() => { stateRef.current.playing = false; setPlaying(false); }} className="min-h-11 rounded-md border border-border px-3 py-2 hover:border-accent">Pausar</button>
        <button type="button" onClick={restart} className="min-h-11 rounded-md border border-border px-3 py-2 hover:border-accent">Reiniciar</button>
        <button type="button" onClick={() => runtimeRef.current?.frameStep()} className="min-h-11 rounded-md border border-border px-3 py-2 hover:border-accent">Avançar 1 quadro</button>
        <button type="button" role="switch" aria-checked={looping} onClick={() => { const next = !looping; stateRef.current.looping = next; setLooping(next); }} className={`col-span-2 min-h-11 rounded-md border px-3 py-2 ${looping ? "border-accent text-fg" : "border-border text-muted"}`}>{looping ? "Loop ativado" : "Loop desativado"}</button>
      </div>
      {mode === "flame" ? <>
        <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
          {SLIDERS.map(({ key, label, min, max, step }) => (
            <label key={key} className="flex flex-col gap-1 rounded-md border border-border px-3 py-2">
              <span className="flex items-center justify-between gap-2 text-sm"><span>{label}</span><output className="tabular-nums text-muted">{(settings[key] ?? DEFAULT_FIRE_EMITTER[key] ?? min).toFixed(key === "particleCount" || key === "flipbookFps" ? 0 : 2)}</output></span>
              <input aria-label={label} type="range" min={min} max={max} step={step} value={settings[key] ?? DEFAULT_FIRE_EMITTER[key] ?? min} onChange={(event) => setSettings((s) => ({ ...s, [key]: Number(event.target.value) }))} />
            </label>
          ))}
        </div>
        <button type="button" onClick={() => { const defaults = { ...DEFAULT_FIRE_EMITTER }; setSettings(defaults); stateRef.current.settings = defaults; restart(); }} className="min-h-11 rounded-md border border-border px-3 py-2 hover:border-accent">Restaurar valores padrão</button>
      </> : pixelModeElement(mode) ? <>
        <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
          {([
            ["scale","Escala",0.4,2.5,0.05],["intensity","Intensidade",0.2,2.5,0.05],["particleCount","Fragmentos",12,96,1],
            ["density","Densidade",0.25,2.5,0.05],["spawnRate","Taxa de emissão",0.25,2.5,0.05],
            ["lifetime","Vida",0.4,5,0.1],["velocity","Velocidade",0.1,2,0.05],["spread","Abertura",0.1,1.2,0.05],
            ["turbulence","Turbulência",0,1.5,0.05],["emissive","Emissão HDR",0,5,0.1],
            ["lightIntensity","Luz real",0,3,0.05],["lightRadius","Raio da luz",0.3,5,0.1],
            ["flickerAmount","Oscilação",0,1,0.05],["animationSpeed","Velocidade da animação",0.2,3,0.05],
            ["seed","Semente",1,999999,1]
          ] as [keyof PixelElementSettings,string,number,number,number][]).map(([key,label,min,max,step]) =>
            <label key={key} className="flex flex-col gap-1 rounded-md border border-border px-3 py-2">
              <span className="flex items-center justify-between gap-2 text-sm"><span>{label}</span><output>{pixelSettings[key]}</output></span>
              <input aria-label={label} type="range" min={min} max={max} step={step} value={pixelSettings[key] as number} onChange={(event) => { const next={...stateRef.current.pixelSettings,[key]:Number(event.target.value)}; stateRef.current.pixelSettings=next; setPixelSettings(next); }} />
            </label>)}
        </div>
        <div className="flex flex-wrap gap-2">
          <button type="button" onClick={() => { const next={...pixelSettings,visualsEnabled:!pixelSettings.visualsEnabled,lightEnabled:true};stateRef.current.pixelSettings=next;setPixelSettings(next); }} className="min-h-11 rounded-md border border-border px-3 py-2">{pixelSettings.visualsEnabled ? "Teste: só luz real" : "Mostrar emissor"}</button>
          <button type="button" onClick={() => { const element=pixelModeElement(mode); if(!element)return; const defaults=pixelDefaults(element,pixelModeVersion(mode));stateRef.current.pixelSettings=defaults;setPixelSettings(defaults);restart(); }} className="min-h-11 rounded-md border border-border px-3 py-2">Restaurar valores padrão</button>
        </div>
      </> : mode === "impact" ? <>
        <div className="grid grid-cols-2 gap-2">
          <button type="button" onClick={() => { const next = { ...stateRef.current.impactSettings, seed: Math.floor(Math.random() * 999999) + 1 }; stateRef.current.impactSettings = next; setImpactSettings(next); setActiveImpactSettings(next); restart(); }} className="min-h-11 rounded-md border border-accent bg-accent/10 px-3 py-2 hover:bg-accent/20">Sortear semente · {impactSettings.seed}</button>
          <button type="button" onClick={() => { const defaults = { ...DEFAULT_IMPACT_SETTINGS }; stateRef.current.impactSettings = defaults; setImpactSettings(defaults); setActiveImpactSettings(defaults); restart(); }} className="min-h-11 rounded-md border border-border px-3 py-2 hover:border-accent">Restaurar valores padrão</button>
        </div>
        {IMPACT_GROUPS.map((group, groupIndex) => <details key={group.title} open={groupIndex < 2} className="rounded-lg border border-border p-3">
          <summary className="cursor-pointer font-medium">{group.title}</summary>
          <div className="mt-3 grid grid-cols-1 gap-2 sm:grid-cols-2">
            {group.sliders.map(({ key, label, min, max, step }) => <label key={key} className="flex flex-col gap-1 rounded-md border border-border px-3 py-2">
              <span className="flex items-center justify-between gap-2 text-sm"><span>{label}</span><output className="tabular-nums text-muted">{impactSettings[key].toFixed(key === "primaryCount" || key === "secondaryCount" || key === "radialCount" || key === "sparkCount" || key === "emberCount" || key === "smokeCount" || key === "flipbookFps" ? 0 : 2)}</output></span>
              <input aria-label={label} type="range" min={min} max={max} step={step} value={impactSettings[key]} onChange={(event) => updateImpactSetting(key, Number(event.target.value))} />
            </label>)}
          </div>
        </details>)}
      </> : mode === "phantasmal" ? <>
        <div className="grid grid-cols-2 gap-2">
          <button type="button" onClick={() => updatePhantasmalSetting("seed", Math.floor(Math.random() * 999999) + 1)} className="min-h-11 rounded-md border border-accent bg-accent/10 px-3 py-2 hover:bg-accent/20">Sortear semente · {phantasmalSettings.seed}</button>
          <button type="button" onClick={() => { const defaults = { ...DEFAULT_PHANTASMAL_FORCE_SETTINGS }; stateRef.current.phantasmalSettings = defaults; setPhantasmalSettings(defaults); setActivePhantasmalForceSettings(defaults); runtimeRef.current?.setPhantasmalSettings(defaults); restart(); }} className="min-h-11 rounded-md border border-border px-3 py-2 hover:border-accent">Restaurar valores padrão</button>
        </div>
        <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
          {PHANTASMAL_SLIDERS.map(({ key, label, min, max, step, integer }) => (
            <label key={key} className="flex flex-col gap-1 rounded-md border border-border px-3 py-2">
              <span className="flex items-center justify-between gap-2 text-sm"><span>{label}</span><output className="tabular-nums text-muted">{integer ? Math.round(phantasmalSettings[key]) : phantasmalSettings[key].toFixed(2)}</output></span>
              <input aria-label={label} type="range" min={min} max={max} step={step} value={phantasmalSettings[key]} onChange={(event) => updatePhantasmalSetting(key, Number(event.target.value))} />
            </label>
          ))}
        </div>
      </> : mode === "magic-missile-v2" ? <>
        <div className="grid grid-cols-2 gap-2">
          <button type="button" onClick={() => updateMagicMissileV2Setting("seed", Math.floor(Math.random() * 999999) + 1)} className="min-h-11 rounded-md border border-accent bg-accent/10 px-3 py-2 hover:bg-accent/20">Sortear semente · {magicMissileV2Settings.seed}</button>
          <button type="button" onClick={() => { const defaults = { ...DEFAULT_MAGIC_MISSILE_V2_SETTINGS }; stateRef.current.magicMissileV2Settings = defaults; setMagicMissileV2Settings(defaults); setActiveMagicMissileV2Settings(defaults); runtimeRef.current?.setMagicMissileV2Settings(defaults); restart(); }} className="min-h-11 rounded-md border border-border px-3 py-2 hover:border-accent">Restaurar valores padrão</button>
        </div>
        <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
          {MAGIC_MISSILE_V2_SLIDERS.map(({ key, label, min, max, step, integer }) => (
            <label key={key} className="flex flex-col gap-1 rounded-md border border-border px-3 py-2">
              <span className="flex items-center justify-between gap-2 text-sm"><span>{label}</span><output className="tabular-nums text-muted">{integer ? Math.round(magicMissileV2Settings[key]) : magicMissileV2Settings[key].toFixed(2)}</output></span>
              <input aria-label={label} type="range" min={min} max={max} step={step} value={magicMissileV2Settings[key]} onChange={(event) => updateMagicMissileV2Setting(key, Number(event.target.value))} />
            </label>
          ))}
        </div>
        <div className="grid grid-cols-2 gap-2">
          {([ ["geometry", "Geometria 3D"], ["trails", "Trilha 3D"], ["particles", "Fragmentos instanciados"], ["distortion", "Distorção espacial"], ["emissiveEnabled", "Emissão HDR"], ["lights", "Luzes dinâmicas reais"], ["bloom", "Bloom"] ] as [MagicMissileToggleKey, string][]).map(([key, label]) => (
            <label key={key} className="flex min-h-11 items-center justify-between rounded-md border border-border px-3 py-2 text-sm"><span>{label}</span><input type="checkbox" checked={magicMissileV2Settings[key]} onChange={(event) => updateMagicMissileV2Setting(key, event.target.checked)} /></label>
          ))}
        </div>
      </> : mode === "bless" ? <>
        <div className="grid grid-cols-2 gap-2">
          <button type="button" onClick={() => updateBlessSetting("seed", Math.floor(Math.random() * 999999) + 1)} className="min-h-11 rounded-md border border-accent bg-accent/10 px-3 py-2 hover:bg-accent/20">Sortear semente · {blessSettings.seed}</button>
          <button type="button" onClick={() => { const defaults = { ...DEFAULT_BLESS_VFX_SETTINGS }; stateRef.current.blessSettings = defaults; setBlessSettings(defaults); setActiveBlessVfxSettings(defaults); runtimeRef.current?.setBlessSettings(defaults); restart(); }} className="min-h-11 rounded-md border border-border px-3 py-2 hover:border-accent">Restaurar valores padrão</button>
        </div>
        <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
          {BLESS_SLIDERS.map(({ key, label, min, max, step, integer }) => (
            <label key={key} className="flex flex-col gap-1 rounded-md border border-border px-3 py-2">
              <span className="flex items-center justify-between gap-2 text-sm"><span>{label}</span><output className="tabular-nums text-muted">{integer ? Math.round(blessSettings[key]) : blessSettings[key].toFixed(2)}</output></span>
              <input aria-label={label} type="range" min={min} max={max} step={step} value={blessSettings[key]} onChange={(event) => updateBlessSetting(key, Number(event.target.value))} />
            </label>
          ))}
        </div>
        <div className="grid grid-cols-2 gap-2">
          {([ ["geometry", "Geometria 3D"], ["particles", "Partículas"], ["emissiveEnabled", "Emissivo"], ["lights", "Luzes dinâmicas"], ["bloom", "Bloom"] ] as [BlessToggleKey, string][]).map(([key, label]) => (
            <label key={key} className="flex min-h-11 items-center justify-between rounded-md border border-border px-3 py-2 text-sm"><span>{label}</span><input type="checkbox" checked={blessSettings[key]} onChange={(event) => updateBlessSetting(key, event.target.checked)} /></label>
          ))}
        </div>
      </> : mode === "web-of-dreams" ? <>
        <div className="grid grid-cols-2 gap-2">
          <button type="button" onClick={() => updateWebDreamSetting("seed", Math.floor(Math.random() * 999999) + 1)} className="min-h-11 rounded-md border border-accent bg-accent/10 px-3 py-2 hover:bg-accent/20">Sortear semente · {webDreamSettings.seed}</button>
          <button type="button" onClick={() => { const defaults = { ...DEFAULT_WEB_OF_DREAMS_VFX_SETTINGS }; stateRef.current.webDreamSettings = defaults; setWebDreamSettings(defaults); setActiveWebOfDreamsVfxSettings(defaults); runtimeRef.current?.setWebDreamSettings(defaults); restart(); }} className="min-h-11 rounded-md border border-border px-3 py-2 hover:border-accent">Restaurar valores padrão</button>
          <button type="button" onClick={() => { updateWebDreamSetting("geometry", false); updateWebDreamSetting("nodes", false); updateWebDreamSetting("particles", false); updateWebDreamSetting("distortion", false); updateWebDreamSetting("emissive", false); updateWebDreamSetting("lights", true); restart(); }} className="col-span-2 min-h-11 rounded-md border border-accent bg-accent/10 px-3 py-2 hover:bg-accent/20">Teste: somente luz dinâmica real</button>
          <button type="button" onClick={() => { runtimeRef.current?.releaseWebDreamPreview(); stateRef.current.playing = true; setPlaying(true); }} className="col-span-2 min-h-11 rounded-md border border-border px-3 py-2 hover:border-accent">Soltar a rede e ver a luz desaparecer</button>
        </div>
        <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
          {WEB_DREAM_SLIDERS.map(({ key, label, min, max, step, integer }) => (
            <label key={key} className="flex flex-col gap-1 rounded-md border border-border px-3 py-2">
              <span className="flex items-center justify-between gap-2 text-sm"><span>{label}</span><output className="tabular-nums text-muted">{integer ? Math.round(webDreamSettings[key]) : webDreamSettings[key].toFixed(2)}</output></span>
              <input aria-label={label} type="range" min={min} max={max} step={step} value={webDreamSettings[key]} onChange={(event) => updateWebDreamSetting(key, Number(event.target.value))} />
            </label>
          ))}
        </div>
        <div className="grid grid-cols-2 gap-2">
          {([["geometry", "Fios 3D"], ["nodes", "Nós de energia"], ["particles", "Partículas"], ["distortion", "Distorção espacial"], ["emissive", "Emissão HDR"], ["lights", "Luzes dinâmicas reais"]] as [WebDreamToggleKey, string][]).map(([key, label]) => (
            <label key={key} className="flex min-h-11 items-center justify-between rounded-md border border-border px-3 py-2 text-sm"><span>{label}</span><input type="checkbox" checked={webDreamSettings[key]} onChange={(event) => updateWebDreamSetting(key, event.target.checked)} /></label>
          ))}
        </div>
      </> : mode === "burning-hands-v2" ? <>
        <div className="grid grid-cols-2 gap-2">
          <button type="button" onClick={() => { updateBurningHandsSetting("visuals", false); updateBurningHandsSetting("distortion", false); updateBurningHandsSetting("emissive", false); updateBurningHandsSetting("lights", true); updateBurningHandsSetting("bloom", false); }} className="col-span-2 min-h-11 rounded-md border border-accent bg-accent/10 px-3 py-2 hover:bg-accent/20">Teste: somente luz dinâmica real</button>
          <button type="button" onClick={() => { updateBurningHandsSetting("visuals", true); updateBurningHandsSetting("distortion", true); updateBurningHandsSetting("emissive", true); updateBurningHandsSetting("lights", false); updateBurningHandsSetting("bloom", false); }} className="col-span-2 min-h-11 rounded-md border border-border px-3 py-2 hover:border-accent">Teste: fogo sem luzes nem bloom</button>
          <button type="button" onClick={() => { const defaults = { ...DEFAULT_BURNING_HANDS_V2_SETTINGS }; stateRef.current.burningHandsSettings = defaults; setActiveBurningHandsV2Settings(defaults); runtimeRef.current?.setBurningHandsSettings(defaults); setBurningHandsSettings(defaults); restart(); }} className="col-span-2 min-h-11 rounded-md border border-border px-3 py-2 hover:border-accent">Restaurar valores padrão</button>
        </div>
        <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
          {BURNING_HANDS_SLIDERS.map(({ key, label, min, max, step, integer }) => (
            <label key={key} className="flex flex-col gap-1 rounded-md border border-border px-3 py-2">
              <span className="flex items-center justify-between gap-2 text-sm"><span>{label}</span><output className="tabular-nums text-muted">{integer ? Math.round(burningHandsSettings[key]) : burningHandsSettings[key].toFixed(2)}</output></span>
              <input aria-label={label} type="range" min={min} max={max} step={step} value={burningHandsSettings[key]} onChange={(event) => updateBurningHandsSetting(key, Number(event.target.value))} />
            </label>
          ))}
        </div>
        <div className="grid grid-cols-2 gap-2">
          {([["visuals", "Fogo flipbook 3D"], ["distortion", "Turbulência do fogo"], ["emissive", "Emissão HDR"], ["lights", "Luzes dinâmicas reais"], ["bloom", "Bloom"]] as [BurningHandsToggleKey, string][]).map(([key, label]) => (
            <label key={key} className="flex min-h-11 items-center justify-between rounded-md border border-border px-3 py-2 text-sm"><span>{label}</span><input type="checkbox" checked={burningHandsSettings[key]} onChange={(event) => updateBurningHandsSetting(key, event.target.checked)} /></label>
          ))}
        </div>
      </> : mode === "burning-hands-v3" ? <>
        <div className="grid grid-cols-2 gap-2">
          <button type="button" onClick={() => { const defaults = { ...DEFAULT_BURNING_HANDS_V3_SETTINGS }; stateRef.current.burningHandsV3Settings = defaults; setActiveBurningHandsV3Settings(defaults); runtimeRef.current?.setBurningHandsV3Settings(defaults); setBurningHandsV3Settings(defaults); restart(); }} className="col-span-2 min-h-11 rounded-md border border-border px-3 py-2 hover:border-accent">Restaurar valores padrão do V3</button>
        </div>
        <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
          {BURNING_HANDS_SLIDERS.map(({ key, label, min, max, step, integer }) => (
            <label key={key} className="flex flex-col gap-1 rounded-md border border-border px-3 py-2">
              <span className="flex items-center justify-between gap-2 text-sm"><span>{label}</span><output className="tabular-nums text-muted">{integer ? Math.round(burningHandsV3Settings[key]) : burningHandsV3Settings[key].toFixed(2)}</output></span>
              <input aria-label={label} type="range" min={min} max={max} step={step} value={burningHandsV3Settings[key]} onChange={(event) => updateBurningHandsV3Setting(key, Number(event.target.value))} />
            </label>
          ))}
        </div>
        <div className="grid grid-cols-2 gap-2">
          {([ ["visuals", "Fogo flipbook 3D"], ["distortion", "Turbulência do fogo"], ["emissive", "Emissão HDR"], ["lights", "Luzes dinâmicas reais"], ["bloom", "Bloom"] ] as [BurningHandsV3ToggleKey, string][]).map(([key, label]) => (
            <label key={key} className="flex min-h-11 items-center justify-between rounded-md border border-border px-3 py-2 text-sm"><span>{label}</span><input type="checkbox" checked={burningHandsV3Settings[key]} onChange={(event) => updateBurningHandsV3Setting(key, event.target.checked)} /></label>
          ))}
        </div>
      </> : mode === "cleave-sweep-v2" ? <>
        <div className="grid grid-cols-2 gap-2">
          <button type="button" onClick={() => { const defaults={...DEFAULT_VARREDURA_SETTINGS}; stateRef.current.varreduraSettings=defaults; setActiveVarreduraSettings(defaults); setVarreduraSettings(defaults); runtimeRef.current?.setVarreduraSettings(defaults); }} className="col-span-2 min-h-11 rounded-md border border-border px-3 py-2 hover:border-accent">Restaurar valores padrão</button>
          <button type="button" onClick={() => { updateVarreduraSetting("geometryEnabled",false); updateVarreduraSetting("debrisEnabled",false); updateVarreduraSetting("lightEnabled",true); restart(); }} className="min-h-11 rounded-md border border-accent bg-accent/10 px-3 py-2 hover:bg-accent/20">Teste: só luz dinâmica</button>
          <button type="button" onClick={() => { updateVarreduraSetting("geometryEnabled",true); updateVarreduraSetting("debrisEnabled",true); updateVarreduraSetting("lightEnabled",false); restart(); }} className="min-h-11 rounded-md border border-border px-3 py-2 hover:border-accent">Teste: só geometria</button>
          <button type="button" onClick={() => updateVarreduraSetting("seed", Math.floor(Math.random()*999999)+1)} className="col-span-2 min-h-11 rounded-md border border-border px-3 py-2 hover:border-accent">Nova variação procedural · semente {varreduraSettings.seed}</button>
        </div>
        <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
          {VARREDURA_SLIDERS.map(({key,label,min,max,step,integer}) => (
            <label key={key} className="flex flex-col gap-1 rounded-md border border-border px-3 py-2">
              <span className="flex items-center justify-between gap-2 text-sm"><span>{label}</span><output className="tabular-nums text-muted">{integer?Math.round(varreduraSettings[key]):varreduraSettings[key].toFixed(2)}</output></span>
              <input aria-label={label} type="range" min={min} max={max} step={step} value={varreduraSettings[key]} onChange={(event)=>updateVarreduraSetting(key,Number(event.target.value))}/>
            </label>
          ))}
        </div>
        <div className="grid grid-cols-2 gap-2">
          {([["geometryEnabled","Lâmina e onda 3D"],["debrisEnabled","Detritos no rastro"],["lightEnabled","Luzes dinâmicas reais"]] as [VarreduraToggleKey,string][]).map(([key,label])=>(
            <label key={key} className="flex min-h-11 items-center justify-between rounded-md border border-border px-3 py-2 text-sm"><span>{label}</span><input type="checkbox" checked={varreduraSettings[key]} onChange={(event)=>updateVarreduraSetting(key,event.target.checked)}/></label>
          ))}
        </div>
      </> : null}
      {mode !== "bless" && mode !== "magic-missile-v2" && mode !== "burning-hands-v2" && mode !== "burning-hands-v3" && <label className="flex min-h-11 items-center justify-between rounded-md border border-border px-3 py-2 text-sm"><span>Bloom de pós-processamento</span><input type="checkbox" checked={bloomEnabled} onChange={(event) => { stateRef.current.bloomEnabled = event.target.checked; setBloomEnabled(event.target.checked); }} /></label>}
      <p className="text-xs leading-relaxed text-muted">{mode === "flame" ? "Flipbook com 16 quadros · partículas instanciadas · suavização por profundidade · luz real no terreno" : pixelModeElement(mode) ? "Flipbook elemental com 16 quadros · fragmentos instanciados · luz real no terreno" : mode === "phantasmal" ? "Tendril meshes com profundidade real · partículas instanciadas · PointLight violeta com sombras · semente determinística; ajustes persistem e valem no combate" : mode === "bless" ? "Onda radius-3 · chegada sincronizada por aliado · PointLights reais no caster e na equipe · as configurações persistem e também regem conjurações de combate" : mode === "magic-missile-v2" ? "Charge prolongado · projétil de escala mundial · spline 3D e trail procedural · luzes pontuais reais com sombras no caster, em voo e no impacto · uma ocorrência por disparo" : mode === "burning-hands-v2" ? "V2 continua disponível exatamente no seu slot de batalha." : mode === "burning-hands-v3" ? "Entrada de preview independente no FX Lab; V2 e o combate permanecem intactos." : mode === "varredura-v2" ? "Frente circular 3D completa · expansão radial uniforme · luzes e impactos em torno de todo o perímetro" : mode === "cleave-sweep-v2" ? "Golpe direcional 3D · fragmentos no rastro · impactos iluminados na ordem dos alvos · parâmetros visuais persistem" : "Timeline de impacto original · partículas em um draw call · mesma semente reproduz o mesmo padrão · bloom começa desligado para avaliar a estrutura"}</p>
    </section>
  );
}
