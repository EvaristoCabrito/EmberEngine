// Engine2's map editor page: Ember's own MapEditorScreen, mounted the way Ember's GameApp mounts it.
import '@/styles.css';
import { createRoot } from 'react-dom/client';
import { configureCampaignStorage } from '../../campaign/storage';
import type { GameArt } from '../../ember/types';
import { MapEditorScreen, clearEditorResume, readEditorResume } from './MapEditorScreen';

// Ember keeps editor drafts, saved versions and Locais in the browser; the campaign port asks the
// host for that storage.
configureCampaignStorage(window.localStorage);

let editorDraft = readEditorResume();

createRoot(document.getElementById('root')!).render(
  <MapEditorScreen
    // Ember handed its preview the loaded game art; Engine2's 3D map loads its own on demand.
    art={{} as GameArt}
    initialDraft={editorDraft}
    onDraftChange={(d) => { editorDraft = d; }}
    // Ember's back button returns to the Test mode menu.
    onBack={() => { clearEditorResume(); location.href = '/game.html?start=test'; }}
    onPlaytest={() => {
      window.alert('Testar: a batalha ainda não está conectada ao Engine2. O mapa continua aberto no editor.');
    }}
  />,
);
