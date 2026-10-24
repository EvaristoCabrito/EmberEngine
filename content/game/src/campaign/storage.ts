/** Storage belongs to the host. Campaign logic can run without a browser. */
export interface CampaignStorage {
  getItem(key: string): string | null;
  setItem(key: string, value: string): void;
}
export let campaignStorage: CampaignStorage | undefined;
let listener: ((name: string, detail?: unknown) => void) | undefined;
export function configureCampaignStorage(storage?: CampaignStorage, onEvent?: (name: string, detail?: unknown) => void): void {
  campaignStorage = storage;
  listener = onEvent;
}
export function emitCampaignEvent(name: string, detail?: unknown): void { listener?.(name, detail); }
export function memoryCampaignStorage(initial: Record<string, string> = {}): CampaignStorage {
  const values = new Map(Object.entries(initial));
  return { getItem: key => values.get(key) ?? null, setItem: (key, value) => { values.set(key, value); } };
}
