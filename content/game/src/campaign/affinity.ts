export const AFFINITY_HEROES = ["Kael", "Neera", "Voss", "Salazar", "Aldric", "Malrec"] as const;
export type AffinityHero = typeof AFFINITY_HEROES[number];
export type AffinityScores = Record<string, number>;
export function affinityBonus(points: number): number {
  return points >= 90 ? 0.08 : points >= 50 ? 0.05 : points >= 25 ? 0.02 : 0;
}
export function canUseAffinityDuo(scores: AffinityScores | undefined, a: AffinityHero, b: AffinityHero): boolean {
  return a !== b && affinityScore(scores, a, b) >= 80;
}
export function canUseAffinityUltimate(scores: AffinityScores | undefined, heroes: readonly [AffinityHero, AffinityHero, AffinityHero]): boolean {
  const [a, b, c] = heroes;
  return new Set(heroes).size === 3 && affinityScore(scores, a, b) === 100 && affinityScore(scores, a, c) === 100 && affinityScore(scores, b, c) === 100;
}
export function affinityPair(a: string, b: string): string {
  return [a, b].sort().join("|");
}
export function affinityScore(scores: AffinityScores | undefined, a: string, b: string): number {
  return scores?.[affinityPair(a, b)] ?? 0;
}
export function affinityGrade(points: number): string {
  return points >= 100 ? "Alta" : points >= 80 ? "Boa" : points >= 50 ? "Média" : points >= 25 ? "Regular" : "Ruim";
}
export function cleanAffinityScores(raw: unknown): AffinityScores {
  if (!raw || typeof raw !== "object" || Array.isArray(raw)) return {};
  const result: AffinityScores = {};
  for (const [pair, score] of Object.entries(raw)) {
    const heroes = pair.split("|");
    if (heroes.length !== 2 || heroes[0] === heroes[1] || !heroes.every(hero => AFFINITY_HEROES.includes(hero as AffinityHero))) continue;
    if (typeof score === "number" && Number.isFinite(score)) result[affinityPair(heroes[0], heroes[1])] = Math.max(0, Math.min(100, Math.round(score * 10) / 10));
  }
  return result;
}
export function changeAffinity(scores: AffinityScores | undefined, a: AffinityHero, b: AffinityHero, delta: number): AffinityScores {
  if (a === b || !Number.isFinite(delta)) return { ...scores };
  return { ...scores, [affinityPair(a, b)]: Math.max(0, Math.min(100, Math.round((affinityScore(scores, a, b) + delta) * 10) / 10)) };
}
