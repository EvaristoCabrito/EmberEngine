/** The title belongs to index.html. Gameplay must never mount another title screen. */
export const TITLE_PAGE = "/";
const GAME_STARTS = new Set(["new", "continue", "test", "map"]);

export function shouldReturnToTitle(start: string | null, reloaded: boolean, editorResume: boolean): boolean {
  return !editorResume && (reloaded || !GAME_STARTS.has(start ?? ""));
}

export function returnToTitle(location: Pick<Location, "replace"> = window.location): void {
  location.replace(TITLE_PAGE);
}
