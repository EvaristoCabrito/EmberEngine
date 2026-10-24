/** One displayed release version for the normal title and gameplay build. */
export const APP_VERSION = "0.034.4";
export const DISPLAY_VERSION = APP_VERSION;

/** Used during both development and production HTML transforms, before the first paint. */
export function renderReleaseVersion(html: string): string {
  return html.replaceAll("%APP_DISPLAY_VERSION%", DISPLAY_VERSION);
}
