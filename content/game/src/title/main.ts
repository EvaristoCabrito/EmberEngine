import { helpLabel, renderHelp, type HelpTab } from './help';
import { getAudioSettings, setCutsceneVolume, setMuted, setMusicVolume, setSfxVolume } from '../units/sounds';
import { activeSave, configureCampaignStorage, emptySave, formatStamp, hasAnySave, isSlotEmpty, lastSaveWrite, loadBank, selectSlot, setMutedBank, slotProgress, writeSlot } from '../campaign';
import { getAudioVolumes, setCutsceneVolume as setGameCutsceneVolume, setMusicVolume as setGameMusicVolume, setSfxVolume as setGameSfxVolume } from '../game/audio';
import { getDevGfx, setDevGfx } from '../game/gfx/three/devGfx';
import { getGraphicsQuality, graphicsQualityIsCustom, setGraphicsQuality, type GraphicsQuality } from '../game/graphicsQuality';
import { getGamePreferences, setGamePreferences, subscribeGamePreferences } from '../game/gamePreferences';
import { artProgress, loadGameArt, subscribeArtProgress } from '../game/assets';
import type { SaveBank } from '../ember/types';

const $ = <T extends HTMLElement>(id: string) => document.getElementById(id) as T;
// No browser right-click menu over the game (text fields keep it for copy and paste).
document.addEventListener('contextmenu', (e) => {
  if (!(e.target as HTMLElement | null)?.closest('input, textarea, [contenteditable="true"]')) e.preventDefault();
});
const titleScreen = $('title-screen'), devControls = $('dev-controls');
const saveSlotsScreen = $('save-slots-screen');
const titleLoader = $('title-loader'), loaderFill = $('loader-fill'), loaderPercent = $('loader-percent'), loaderStatus = $('loader-status');
const menuMusic = $<HTMLAudioElement>('menu-music');
const mainMenuButtons = [...document.querySelectorAll<HTMLButtonElement>('.title-menu button')];
let saveSlotFlow: 'new' | 'load' = 'new';
let overwriteSlot: number | null = null;
try { configureCampaignStorage(window.localStorage); } catch { configureCampaignStorage(); }
let saveBank: SaveBank = loadBank();
const savedTitleLanguage = localStorage.getItem('engine2:title-language');
let language: 'pt' | 'en' = savedTitleLanguage === 'pt' || savedTitleLanguage === 'en' ? savedTitleLanguage : getGamePreferences().uiLanguage;
let helpTab: HelpTab = 'basics';
type LanguageOptions = { dialogue: 'pt' | 'en'; subtitles: 'pt' | 'en'; showSubtitles: boolean; dialogueSize: string };
const currentPreferences = getGamePreferences();
const languageOptions: LanguageOptions = {
  dialogue: currentPreferences.dialogueLanguage,
  subtitles: currentPreferences.subtitleLanguage,
  showSubtitles: currentPreferences.subtitles,
  dialogueSize: String(currentPreferences.dialogueScale),
};

const translations: Record<string, [string, string]> = {
  'loading': ['Despertando as cinzas', 'Awakening the ashes'],
};

function startMenuMusic(): void {
  const settings = getAudioSettings();
  menuMusic.volume = settings.music * Number(menuMusic.dataset.engine2MusicBase ?? 1);
  menuMusic.muted = settings.muted;
  if (settings.muted) return;
  void menuMusic.play().then(() => {
    document.removeEventListener('pointerdown', unlockMenuMusic, true);
    document.removeEventListener('keydown', unlockMenuMusic, true);
  }).catch(() => {
    document.addEventListener('pointerdown', unlockMenuMusic, true);
    document.addEventListener('keydown', unlockMenuMusic, true);
  });
}

function unlockMenuMusic(): void { startMenuMusic(); }
function stopMenuMusic(): void { menuMusic.pause(); }

function updateSoundToggle(): void {
  const isMuted = getAudioSettings().muted;
  document.querySelectorAll<HTMLButtonElement>('[data-sound-toggle]').forEach((button) => {
    button.setAttribute('aria-pressed', String(isMuted));
    button.setAttribute('aria-label', language === 'pt'
      ? (isMuted ? 'Ativar som' : 'Desativar som')
      : (isMuted ? 'Turn sound on' : 'Turn sound off'));
    button.innerHTML = isMuted
      ? '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M4 10v4h4l5 4V6l-5 4H4Z"/><path d="m17 9 5 6m0-6-5 6"/></svg>'
      : '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M4 10v4h4l5 4V6l-5 4H4Z"/><path d="M16 9a5 5 0 0 1 0 6m2-9a9 9 0 0 1 0 12"/></svg>';
  });
}

function applyLanguage(next: 'pt' | 'en'): void {
  language = next;
  document.documentElement.lang = next === 'pt' ? 'pt-BR' : 'en';
  document.querySelectorAll<HTMLElement>('[data-pt][data-en]').forEach((element) => {
    element.textContent = element.dataset[next] ?? element.textContent ?? '';
  });
  document.querySelectorAll<HTMLElement>('[data-label-pt][data-label-en]').forEach((element) => {
    element.setAttribute('aria-label', element.dataset[next === 'pt' ? 'labelPt' : 'labelEn'] ?? '');
  });
  $<HTMLSelectElement>('language-select').value = next;
  renderHelpPanel();
  updateGraphicsControls();
  updateSoundToggle();
  try { localStorage.setItem('engine2:title-language', next); } catch { /* The menu still works for this page. */ }
  setGamePreferences({ uiLanguage: next });
  loaderStatus.textContent = translations.loading[next === 'pt' ? 0 : 1];
}

// Ember's TitleLoader embers: x %, delay s, duration s. They ride the fill, never ahead of it.
const LOADER_EMBERS = [
  { x: 8, d: 0, t: 3.4 }, { x: 19, d: 1.1, t: 4.1 }, { x: 31, d: 2.2, t: 3.7 }, { x: 44, d: 0.5, t: 4.4 },
  { x: 57, d: 1.7, t: 3.5 }, { x: 68, d: 2.9, t: 4.0 }, { x: 79, d: 0.9, t: 3.8 }, { x: 90, d: 2.4, t: 4.3 },
];
const loaderEmbers = Array.from(titleLoader.querySelectorAll<HTMLElement>('.loader-embers i'));
loaderEmbers.forEach((ember, i) => {
  ember.style.animationDelay = `${LOADER_EMBERS[i].d}s`;
  ember.style.animationDuration = `${LOADER_EMBERS[i].t}s`;
});

function setProgress(value: number): void {
  const pct = Math.max(0, Math.min(100, Math.round(value)));
  loaderFill.style.width = `${pct}%`;
  loaderEmbers.forEach((ember, i) => { ember.style.left = `${Math.min(LOADER_EMBERS[i].x, Math.max(2, pct))}%`; });
  loaderPercent.textContent = `${pct}%`;
  titleLoader.setAttribute('aria-valuenow', String(pct));
}

function setScreen(screen: 'title' | 'save-slots' | 'dev'): void {
  titleScreen.hidden = screen !== 'title';
  saveSlotsScreen.hidden = screen !== 'save-slots';
  devControls.hidden = screen !== 'dev';
  if (screen === 'title' || screen === 'save-slots') startMenuMusic();
  else stopMenuMusic();
  if (screen === 'dev') $('dev-title').focus?.();
  if (screen === 'title') $('game-title').focus?.();
  if (screen === 'save-slots') $('save-slots-title').focus?.();
}

function updateContinueButton(): void {
  $('continue-campaign').hidden = !hasAnySave(saveBank);
}

function formatSaveDate(timestamp: number): string {
  try {
    return new Intl.DateTimeFormat(language === 'pt' ? 'pt-BR' : 'en-US', {
      timeZone: 'America/Sao_Paulo', dateStyle: 'medium', timeStyle: 'short',
    }).format(new Date(timestamp));
  } catch { return formatStamp(timestamp); }
}

function renderSaveSlots(): void {
  const list = $('save-slots-list');
  list.replaceChildren();
  saveBank.slots.forEach((slot, index) => {
    const empty = isSlotEmpty(slot);
    const row = document.createElement('div');
    row.setAttribute('role', 'listitem');
    const button = document.createElement('button');
    button.type = 'button';
    const last = index === saveBank.lastSlot && hasAnySave(saveBank) && !empty;
    const confirming = overwriteSlot === index;
    button.className = `save-slot${last ? ' is-last' : ''}`;
    button.disabled = saveSlotFlow === 'load' && empty;
    button.setAttribute('aria-label', `${empty ? (language === 'pt' ? 'Espaço vazio' : 'Empty slot') : slotProgress(slot).title}, ${language === 'pt' ? 'espaço' : 'slot'} ${index + 1}${last ? (language === 'pt' ? ', último usado' : ', last used') : ''}`);

    const meta = document.createElement('span');
    meta.className = 'save-slot-meta';
    const name = document.createElement('strong');
    name.className = 'save-slot-kicker';
    name.textContent = `${language === 'pt' ? 'Espaço' : 'Slot'} ${index + 1}`;
    meta.append(name);
    if (last) {
      const lastUsed = document.createElement('span');
      lastUsed.className = 'save-slot-last';
      lastUsed.textContent = language === 'pt' ? 'Último usado' : 'Last used';
      meta.append(lastUsed);
    }
    const title = document.createElement('span');
    title.className = 'save-slot-name';
    const progress = document.createElement('span');
    progress.className = 'save-slot-progress';
    const summary = slotProgress(slot);
    title.textContent = empty ? (language === 'pt' ? 'Vazio' : 'Empty') : summary.title;
    progress.textContent = empty
      ? (language === 'pt' ? 'Nenhuma campanha salva' : 'No saved campaign')
      : summary.detail;
    button.append(meta, title, progress);
    if (!empty && slot) {
      const date = document.createElement('span');
      date.className = 'save-slot-date';
      date.textContent = formatSaveDate(slot.updatedAt);
      button.append(date);
    }
    if (confirming) {
      const confirm = document.createElement('span');
      confirm.className = 'save-slot-confirm';
      confirm.textContent = language === 'pt' ? 'Toque de novo para substituir.' : 'Select again to overwrite.';
      button.append(confirm);
    }
    button.addEventListener('click', () => {
      if (saveSlotFlow === 'new' && !empty && !confirming) {
        overwriteSlot = index;
        renderSaveSlots();
        return;
      }
      chooseSaveSlot(index);
    });
    row.append(button);
    list.append(row);
  });
}

function openSaveSlots(flow: 'new' | 'load'): void {
  saveSlotFlow = flow;
  overwriteSlot = null;
  $('save-slots-title').textContent = language === 'pt'
    ? (flow === 'new' ? 'Nova campanha' : 'Carregar campanha')
    : (flow === 'new' ? 'New campaign' : 'Load campaign');
  $('save-slots-intro').textContent = language === 'pt'
    ? (flow === 'new' ? 'Escolha um espaço. Um espaço ocupado será substituído.' : 'O último usado vem marcado. Escolha uma campanha para continuar.')
    : (flow === 'new' ? 'Choose a slot. An occupied slot will be overwritten.' : 'The last used slot is marked. Choose a campaign to continue.');
  $('save-slots-status').textContent = '';
  renderSaveSlots();
  setScreen('save-slots');
}

function chooseSaveSlot(index: number): void {
  const slotNumber = index + 1;
  let launchMode: 'new' | 'continue';
  if (saveSlotFlow === 'new') {
    if (!isSlotEmpty(saveBank.slots[index] ?? null) && overwriteSlot !== index) return;
    saveBank = writeSlot(saveBank, index, { ...emptySave(saveBank.muted), pendingMission: 'vau', mapMode: 'rpg' });
    launchMode = 'new';
    overwriteSlot = null;
    if (!lastSaveWrite().ok) {
      $('save-slots-status').textContent = language === 'pt' ? 'Não foi possível gravar o espaço neste navegador.' : 'This browser could not save the slot.';
      return;
    }
    $('save-slots-status').textContent = language === 'pt'
      ? `Campanha criada e salva no espaço ${slotNumber}. Use Continuar para selecionar esse progresso.`
      : `Campaign created and saved in slot ${slotNumber}. Use Continue to select this progress.`;
  } else {
    if (isSlotEmpty(saveBank.slots[index] ?? null)) return;
    saveBank = selectSlot(saveBank, index);
    launchMode = 'continue';
    if (!lastSaveWrite().ok) {
      $('save-slots-status').textContent = language === 'pt' ? 'Não foi possível selecionar o espaço neste navegador.' : 'This browser could not select the slot.';
      return;
    }
    const summary = slotProgress(activeSave(saveBank));
    $('save-slots-status').textContent = language === 'pt'
      ? `Espaço ${slotNumber} carregado: ${summary.title} · ${summary.detail}.`
      : `Slot ${slotNumber} loaded: ${summary.title} · ${summary.detail}.`;
  }
  updateContinueButton();
  try { sessionStorage.removeItem("engine2:game-launched"); } catch { /* storage blocked */ }
  window.location.assign(`/game.html?start=${launchMode}`);
}

function showDialog(title: string, message: string, showTestLink = false): void {
  $('dialog-title').textContent = title;
  $('dialog-message').textContent = message;
  $('dialog-test-link').hidden = !showTestLink;
  $<HTMLDialogElement>('notice-dialog').showModal();
}

function renderHelpPanel(): void {
  const tabs: HelpTab[] = ['basics', 'uses', 'damage', 'formulas', 'loot'];
  const tabRoot = $('help-tabs');
  const content = $('help-content');
  if (!tabRoot || !content) return;
  tabRoot.innerHTML = tabs.map((tab) => `<button type="button" role="tab" aria-selected="${tab === helpTab}" class="help-tab${tab === helpTab ? ' on' : ''}" data-help-tab="${tab}">${helpLabel(tab, language)}</button>`).join('');
  content.innerHTML = renderHelp(language, helpTab);
}

function persistLanguageOptions(): void {
  setGamePreferences({
    dialogueLanguage: languageOptions.dialogue,
    subtitleLanguage: languageOptions.subtitles,
    subtitles: languageOptions.showSubtitles,
    dialogueScale: Number(languageOptions.dialogueSize),
  });
}

function updateGraphicsControls(): void {
  const quality = getGraphicsQuality();
  const gfx = getDevGfx();
  const custom = graphicsQualityIsCustom(gfx);
  document.querySelectorAll<HTMLButtonElement>('[data-graphics-quality]').forEach((button) => {
    button.setAttribute('aria-pressed', String(!custom && button.dataset.graphicsQuality === quality));
  });
  const values: Record<string, boolean> = {
    'gfx-real-shadows': gfx.realShadows,
    'gfx-soft-shadows': gfx.softShadows,
    'gfx-local-lights': gfx.localLights,
    'gfx-atmospheric-fx': gfx.atmosphericFx,
    'gfx-contact-shadows': gfx.contactShadows,
  };
  Object.entries(values).forEach(([id, checked]) => { $<HTMLInputElement>(id).checked = checked; });
  document.querySelectorAll<HTMLElement>('[data-gfx-control]').forEach((label) => {
    const name = label.dataset.gfxControl;
    label.hidden = (name === 'realShadows' && quality === 'low')
      || (name === 'softShadows' && quality !== 'medium')
      || (name === 'contactShadows' && quality !== 'high');
    if (name === 'contactShadows') $<HTMLInputElement>('gfx-contact-shadows').disabled = !gfx.realShadows;
  });
  const customLabel = $('graphics-custom');
  customLabel.hidden = !custom;
  customLabel.textContent = language === 'pt' ? `Personalizada · ${gfx.shadowResolution}` : `Custom · ${gfx.shadowResolution}`;
}

$('test-mode-open').addEventListener('click', () => { window.location.href = '/game.html?start=test'; });
document.querySelectorAll<HTMLButtonElement>('[data-back-title]').forEach((button) => button.addEventListener('click', () => setScreen('title')));
$('new-campaign').addEventListener('click', () => openSaveSlots('new'));
$('continue-campaign').addEventListener('click', () => openSaveSlots('load'));
$('dialog-test-link').addEventListener('click', () => { $<HTMLDialogElement>('notice-dialog').close(); window.location.href = '/game.html?start=test'; });
document.querySelectorAll<HTMLButtonElement>('[data-close-dialog]').forEach((button) => button.addEventListener('click', () => $<HTMLDialogElement>('notice-dialog').close()));
$('notice-dialog').addEventListener('click', (event) => { if (event.target === $('notice-dialog')) $<HTMLDialogElement>('notice-dialog').close(); });

// Como jogar and Opções open Ember's own windows (src/title/emberMenus.tsx).
$('help-open').addEventListener('click', () => {
  void import('./emberMenus').then(({ openHelp }) => openHelp(() => $('help-open').focus()));
});
/** Ember's Options writes the game's audio volumes and preferences: the title follows them. */
function syncTitleAudio(): void {
  const gameAudio = getAudioVolumes();
  setMusicVolume(gameAudio.music);
  setSfxVolume(gameAudio.sfx);
  setCutsceneVolume(gameAudio.cutscene);
  menuMusic.volume = gameAudio.music * Number(menuMusic.dataset.engine2MusicBase ?? 1);
}
subscribeGamePreferences(() => {
  const prefs = getGamePreferences();
  Object.assign(languageOptions, { dialogue: prefs.dialogueLanguage, subtitles: prefs.subtitleLanguage, showSubtitles: prefs.subtitles, dialogueSize: String(prefs.dialogueScale) });
  if (prefs.uiLanguage !== language) applyLanguage(prefs.uiLanguage);
});
function toggleTitleMute(): void {
  const next = !getAudioSettings().muted;
  setMuted(next);
  saveBank = setMutedBank(saveBank, next);
  updateSoundToggle();
  if (!titleScreen.hidden || !saveSlotsScreen.hidden) startMenuMusic();
}
$('options-open').addEventListener('click', () => {
  syncTitleAudio();
  document.addEventListener('input', syncTitleAudio);
  void import('./emberMenus').then(({ openOptions }) => openOptions({
    muted: () => getAudioSettings().muted,
    onMute: toggleTitleMute,
    onClosed: () => { document.removeEventListener('input', syncTitleAudio); syncTitleAudio(); $('options-open').focus(); },
  }));
});
renderHelpPanel();
$('help-tabs').addEventListener('click', (event) => {
  const button = (event.target as HTMLElement).closest<HTMLButtonElement>('[data-help-tab]');
  if (!button) return;
  helpTab = button.dataset.helpTab as HelpTab;
  renderHelpPanel();
});
document.querySelectorAll<HTMLButtonElement>('[data-close-help]').forEach((button) => button.addEventListener('click', () => $<HTMLDialogElement>('help-dialog').close()));
$('help-dialog').addEventListener('click', (event) => { if (event.target === $('help-dialog')) $<HTMLDialogElement>('help-dialog').close(); });
$('options-dialog').addEventListener('toggle', () => {
  if (!$<HTMLDialogElement>('options-dialog').open) return;
  const audio = getAudioSettings();
  const gameAudio = getAudioVolumes();
  setMusicVolume(gameAudio.music);
  setSfxVolume(gameAudio.sfx);
  setCutsceneVolume(gameAudio.cutscene);
  menuMusic.volume = gameAudio.music * Number(menuMusic.dataset.engine2MusicBase ?? 1);
  $<HTMLInputElement>('mute-sound').checked = audio.muted;
  for (const [key, id] of [['music', 'music-volume'], ['sfx', 'sfx-volume'], ['cutscene', 'cutscene-volume']] as const) {
    const slider = $<HTMLInputElement>(id);
    slider.value = String(gameAudio[key]);
    $<HTMLOutputElement>(`${id}-value`).value = `${Math.round(gameAudio[key] * 100)}%`;
  }
  $<HTMLSelectElement>('dialogue-language').value = languageOptions.dialogue;
  $<HTMLSelectElement>('subtitle-language').value = languageOptions.subtitles;
  $<HTMLInputElement>('show-subtitles').checked = languageOptions.showSubtitles;
  $<HTMLSelectElement>('dialogue-size').value = languageOptions.dialogueSize;
  updateGraphicsControls();
});
document.querySelectorAll<HTMLButtonElement>('[data-close-options]').forEach((button) => button.addEventListener('click', () => $<HTMLDialogElement>('options-dialog').close()));
$('options-dialog').addEventListener('click', (event) => { if (event.target === $('options-dialog')) $<HTMLDialogElement>('options-dialog').close(); });
$('language-select').addEventListener('change', (event) => applyLanguage((event.target as HTMLSelectElement).value as 'pt' | 'en'));
$('dialogue-language').addEventListener('change', (event) => { languageOptions.dialogue = (event.target as HTMLSelectElement).value as 'pt' | 'en'; persistLanguageOptions(); });
$('subtitle-language').addEventListener('change', (event) => { languageOptions.subtitles = (event.target as HTMLSelectElement).value as 'pt' | 'en'; persistLanguageOptions(); });
$('show-subtitles').addEventListener('change', (event) => { languageOptions.showSubtitles = (event.target as HTMLInputElement).checked; persistLanguageOptions(); });
$('dialogue-size').addEventListener('change', (event) => { languageOptions.dialogueSize = (event.target as HTMLSelectElement).value; persistLanguageOptions(); });
$('mute-sound').addEventListener('change', (event) => {
  const next = (event.target as HTMLInputElement).checked;
  setMuted(next);
  saveBank = setMutedBank(saveBank, next);
  updateSoundToggle();
  if (!getAudioSettings().muted && (!titleScreen.hidden || !saveSlotsScreen.hidden)) startMenuMusic();
});
document.querySelectorAll<HTMLButtonElement>('[data-sound-toggle]').forEach((button) => button.addEventListener('click', () => {
  const next = !getAudioSettings().muted;
  setMuted(next);
  saveBank = setMutedBank(saveBank, next);
  updateSoundToggle();
  if (!titleScreen.hidden || !saveSlotsScreen.hidden) startMenuMusic();
}));
for (const [id, setter] of [['music-volume', setMusicVolume], ['sfx-volume', setSfxVolume], ['cutscene-volume', setCutsceneVolume]] as const) {
  $<HTMLInputElement>(id).addEventListener('input', (event) => {
    const value = Number((event.target as HTMLInputElement).value);
    setter(value);
    if (id === 'music-volume') setGameMusicVolume(value);
    else if (id === 'sfx-volume') setGameSfxVolume(value);
    else setGameCutsceneVolume(value);
    $<HTMLOutputElement>(`${id}-value`).value = `${Math.round(value * 100)}%`;
  });
}
document.querySelectorAll<HTMLButtonElement>('[data-graphics-quality]').forEach((button) => button.addEventListener('click', () => {
  setGraphicsQuality(button.dataset.graphicsQuality as GraphicsQuality);
  updateGraphicsControls();
}));
for (const [id, key] of [
  ['gfx-real-shadows', 'realShadows'], ['gfx-soft-shadows', 'softShadows'], ['gfx-local-lights', 'localLights'],
  ['gfx-atmospheric-fx', 'atmosphericFx'], ['gfx-contact-shadows', 'contactShadows'],
] as const) {
  $<HTMLInputElement>(id).addEventListener('change', (event) => {
    setDevGfx({ [key]: (event.target as HTMLInputElement).checked });
    updateGraphicsControls();
  });
}
$<HTMLButtonElement>('fullscreen-toggle').addEventListener('click', async () => {
  const status = $('fullscreen-error');
  try {
    if (document.fullscreenElement) await document.exitFullscreen();
    else await document.documentElement.requestFullscreen();
    status.hidden = true;
  } catch {
    status.hidden = false;
    status.textContent = language === 'pt' ? 'Tela cheia indisponível neste navegador.' : 'Fullscreen is unavailable in this browser.';
  }
});

async function loadTitleResources(): Promise<void> {
  setProgress(0);
  // Ember's title load: the real game art (FX, backdrops, portraits…) loads here, so the bar shows
  // true progress and the game page (Test mode, campaign) opens from the browser cache.
  // Kept monotonic and below 100% until loading has truly finished, as in Ember's useArtLoadProgress.
  let peak = 0;
  const unsubscribe = subscribeArtProgress(() => {
    peak = Math.max(peak, Math.min(artProgress(), 0.97));
    setProgress(peak * 100);
  });
  const background = new Promise<void>((resolve) => {
    const image = new Image();
    image.onload = image.onerror = () => resolve();
    image.src = '/game/title/title-bg.jpg';
    if (image.complete) resolve();
  });
  // game.html's boot loading screen art, cached here so it shows at once when the game opens.
  const bootArt = new Promise<void>((resolve, reject) => {
    const image = new Image();
    image.onload = () => { void image.decode().then(resolve, reject); };
    image.onerror = () => reject(new Error('Unable to preload the castle loading screen.'));
    image.src = '/game/ui/LoadingSCreen.jpg?v=2';
  });
  // Also kept in a named Cache Storage cache (the HTTP cache evicts/revalidates at will), so
  // game.html takes it from there, complete, at once. game.html reads the same cache name.
  const bootArtCache = (async () => {
    try {
      const cache = await caches.open('ember-loading-art-v1');
      if (!(await cache.match('/game/ui/LoadingSCreen.jpg?v=2'))) await cache.add('/game/ui/LoadingSCreen.jpg?v=2');
    } catch { /* no Cache Storage (e.g. a private window): game.html fetches it itself */ }
  })();
  await Promise.all([background, bootArt, bootArtCache, loadGameArt().catch(() => undefined)]);
  unsubscribe();
  setProgress(100);
  titleLoader.classList.add('is-ready');
  mainMenuButtons.forEach((button) => { button.disabled = false; });
  window.setTimeout(() => titleLoader.classList.add('is-leaving'), 900);
  window.setTimeout(() => titleLoader.remove(), 1900);
}

applyLanguage(language);
updateContinueButton();
updateSoundToggle();
startMenuMusic();
void loadTitleResources();
// Migrate old links to the removed Test Menu straight to the debug map flow.
if (new URLSearchParams(location.search).get('screen') === 'test') window.location.replace('/game.html?start=test');
