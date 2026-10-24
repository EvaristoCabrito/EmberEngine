/** A loading made of steps, each measured in its own real units (files, shaders…). The bar gives
 * every step the share of time it actually took the last time (remembered in localStorage), so it
 * moves at the pace the work really goes; inside a step it follows that step's real count. A
 * step with nothing countable (one blocking job) fills when it ends. */
export function rememberedStepMs(key: string, fallback: number[]): number[] {
  try {
    const saved = JSON.parse(localStorage.getItem(key) || "null");
    if (Array.isArray(saved) && saved.length === fallback.length && saved.every((n) => Number.isFinite(n) && n >= 0)) return saved;
  } catch { /* storage blocked */ }
  return fallback;
}

export function rememberStepMs(key: string, ms: number[]): void {
  try { localStorage.setItem(key, JSON.stringify(ms.map((n) => Math.round(n)))); } catch { /* storage blocked */ }
}

/** 0..100 for being at `step` with `done` of `total` units finished there. */
export function stepPercent(stepMs: number[], step: number, done: number, total: number): number {
  const all = stepMs.reduce((sum, ms) => sum + Math.max(1, ms), 0);
  let before = 0;
  for (let i = 0; i < Math.min(step, stepMs.length); i++) before += Math.max(1, stepMs[i]);
  const here = step < stepMs.length ? Math.max(1, stepMs[step]) : 0;
  const inside = total > 0 ? Math.min(1, done / total) : 0;
  return Math.min(100, ((before + here * inside) / all) * 100);
}

/** Times each step of one loading as it runs, so the next one can be weighted by them. */
export class StepClock {
  private readonly ms: number[];
  private step = 0;
  private since = performance.now();
  constructor(steps: number) { this.ms = new Array(steps).fill(0); }
  get current(): number { return this.step; }
  /** Move on to `step` (never back); the time since the last change belongs to the step left. */
  enter(step: number): void {
    if (step <= this.step) return;
    const now = performance.now();
    this.ms[this.step] += now - this.since;
    for (let i = this.step + 1; i < step; i++) this.ms[i] = 0;
    this.step = step;
    this.since = now;
  }
  finish(): number[] {
    this.ms[this.step] += performance.now() - this.since;
    this.since = performance.now();
    return [...this.ms];
  }
}
