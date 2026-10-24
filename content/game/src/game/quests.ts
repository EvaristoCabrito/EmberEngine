/** Inn quests: errands the Inn's NPCs hand out (see InnScreen.tsx).
 *
 * Quest data lives HERE, keyed by mission id, rather than inside the map JSON files: the Map
 * Editor rebuilds a map's draft from its own state on every Save, so anything it doesn't know
 * about would be silently dropped the next time the author saved that map. */

import type { PotionId } from "./types";
import type { Condition, Effect } from "./progression";

export type QuestNpcId = "brue" | "mudo" | "suspicious" | "porao";

export interface QuestPickupDef {
  id: string;
  name: string;
  /** Hex the pickup lies on, in the quest mission's own board coordinates. */
  x: number;
  y: number;
}

export interface QuestDef {
  id: string;
  npc: QuestNpcId;
  title: string;
  kind: "fetch" | "kill" | "recruit";
  /** The mission (map) where the objective happens. */
  missionId: string;
  /** Human-readable place, shown in the quest text. */
  place: string;
  /** What the NPC says when offering the quest. */
  offer: string;
  /** Reminder while the quest is accepted and not finished. */
  active: string;
  /** Once every objective is met, before handing in. */
  ready: string;
  /** After the reward is paid. */
  done: string;
  /** Gold paid on hand-in. */
  reward: number;
  /** Potions handed over on top of the Gold, one each (given to whichever heroes have room). */
  rewardPotions: PotionId[];
  /** fetch: the things to pick up. */
  pickups?: QuestPickupDef[];
  /** kill: the enemy spawn name that has to die. */
  targetName?: string;
  /** The giver only offers this once the condition holds (see progression.ts). Absent means
   * always on offer. Lets one NPC gain new quests later without new dialogue code. */
  availability?: Condition;
  /** Effects applied while the quest is accepted / once it is handed in (see progression.ts),
   * e.g. { setChapter: 2 } or { setFlag: ".." }. */
  onAccept?: Effect[];
  onComplete?: Effect[];
}

export const QUESTS: QuestDef[] = [
  {
    id: "mudo-medalhoes",
    npc: "mudo",
    title: "Medalhões da velha família",
    kind: "fetch",
    missionId: "cemiterio-esquecidos-mausoleu",
    place: "Mausoléu das Velhas Famílias, no Cemitério dos Esquecidos",
    offer:
      "Na tábua, em giz: “Três medalhões ficaram no Mausoléu das Velhas Famílias. Caíram das mãos de quem os carregava. Traga-os.” Ele bate duas vezes no bolso, como quem promete pagar.",
    active: "Na tábua: “Três medalhões. Mausoléu. Ainda não.”",
    ready: "Ele vira os medalhões nas mãos, um por um, e assente devagar. Na tábua: “Era isto.”",
    done: "Na tábua, já meio apagado: “Obrigado.” E abaixo, menor: “Não fique parado por muito tempo.”",
    reward: 180,
    rewardPotions: ["potent", "manaMid"],
    pickups: [
      { id: "medalhao-1", name: "Medalhão da Família", x: 7, y: 12 },
      { id: "medalhao-2", name: "Medalhão da Família", x: 22, y: 8 },
      { id: "medalhao-3", name: "Medalhão da Família", x: 21, y: 17 },
    ],
  },
  {
    id: "suspeito-watchtower-captive",
    npc: "suspicious",
    title: "O oficial cativo",
    kind: "recruit",
    missionId: "watchtower-prison",
    place: "Torre de Vigia, nas celas do nível mais baixo",
    offer: "Ele escreve: “Há um oficial de alta patente da Companhia Carmesim preso na Torre de Vigia. Tire-o de lá. Não pergunte o nome. Não pergunte quem me contou.”",
    active: "Na tábua: “O oficial continua na Torre de Vigia. Tire-o da cela.”",
    ready: "Na tábua: “Você o trouxe vivo. Era o bastante.”",
    done: "Ele apaga a mensagem e guarda o giz. O nome e a origem da informação continuam só com ele.",
    reward: 300,
    rewardPotions: [],
    targetName: "Aldric",
    availability: { flagSet: "suspeito-watchtower-intel" },
  },
  {
    id: "porao-caliches",
    npc: "porao",
    title: "Cálices afogados",
    kind: "fetch",
    missionId: "cemiterio-esquecidos-ruinas",
    place: "Ruínas Submersas sob a capela, no Cemitério dos Esquecidos",
    offer:
      "“Desceram com a capela. Três cálices, de quando ainda se rezava ali em cima. Não quero o que os guarda, quero os cálices. Tragam-nos, e tragam histórias.”",
    active: "“Ainda não subiram. Os cálices. Ruínas sob a capela.”",
    ready: "Ela ergue os cálices contra a única vela e os vê sem beber. “Ainda lembram a água. Obrigada.”",
    done: "“Os cálices estão em paz. O chão também.”",
    reward: 240,
    rewardPotions: ["potent", "manaMid"],
    pickups: [
      { id: "calice-1", name: "Cálice Afogado", x: 9, y: 5 },
      { id: "calice-2", name: "Cálice Afogado", x: 28, y: 17 },
      { id: "calice-3", name: "Cálice Afogado", x: 12, y: 13 },
    ],
  },
  {
    id: "brue-birolho",
    npc: "brue",
    title: "O Birolho",
    kind: "kill",
    missionId: "cemiterio-esquecidos",
    place: "Terreno do Cemitério dos Esquecidos, longe da entrada",
    offer:
      "“Tem uma coisa com olhos demais rondando o cemitério, lá onde ninguém vai de propósito. Chamam de O Birolho. Comeu dois coveiros e um cavalo. Traga a morte dele e eu pago o que a adega aguenta.”",
    active: "“O Birolho ainda anda por lá. Terreno do cemitério, o canto mais longe da entrada.”",
    ready: "“Então é verdade. Ele caiu.” Brue serve um copo que não cobra.",
    done: "“Os coveiros dormem melhor. Eu também, um pouco.”",
    reward: 660,
    rewardPotions: ["potent", "manaMid"],
    targetName: "O Birolho",
  },
];

export function questById(id: string): QuestDef | undefined {
  return QUESTS.find((quest) => quest.id === id);
}

export function questsFor(npc: QuestNpcId): QuestDef[] {
  return QUESTS.filter((quest) => quest.npc === npc);
}

/** Extra levels a named boss spawn gets over its mission's normal enemy level (see
 * spawnUnit in engine.ts). The engine's own per-5-levels stat boost then applies on top, so
 * the bonus raises both level and stats. Keyed by spawn name, not stored in the map JSON, for
 * the same reason quest data lives here. */
export const NOTORIOUS_LEVEL_BONUS: Record<string, number> = {
  "O Birolho": 8,
};

/** The slice of a save the quest helpers read. */
export interface QuestSaveView {
  questsActive?: string[];
  questsDone?: string[];
  questItems?: string[];
  questKills?: string[];
  flags?: string[];
}

export type QuestStatus = "available" | "active" | "ready" | "done";

/** 0 = not taken, 1 = accepted, 2 = objectives met (ready to hand in), 3 = done — the
 * numbers progression.ts conditions (questStage / questActive / questCompleted) speak in. */
export function questStage(save: QuestSaveView, quest: QuestDef): number {
  const status = questStatus(save, quest);
  return status === "done" ? 3 : status === "ready" ? 2 : status === "active" ? 1 : 0;
}

export function pickupKey(quest: QuestDef, pickup: QuestPickupDef): string {
  return `${quest.id}:${pickup.id}`;
}

export function questProgress(save: QuestSaveView, quest: QuestDef): { have: number; total: number } {
  if (quest.kind === "kill") return { have: (save.questKills ?? []).includes(quest.targetName ?? "") ? 1 : 0, total: 1 };
  if (quest.kind === "recruit") return { have: (save.flags ?? []).includes(`recruited:${quest.targetName ?? ""}`) ? 1 : 0, total: 1 };
  const total = quest.pickups?.length ?? 0;
  const held = new Set(save.questItems ?? []);
  return { have: (quest.pickups ?? []).filter((pickup) => held.has(pickupKey(quest, pickup))).length, total };
}

export function questStatus(save: QuestSaveView, quest: QuestDef): QuestStatus {
  if ((save.questsDone ?? []).includes(quest.id)) return "done";
  if (!(save.questsActive ?? []).includes(quest.id)) return "available";
  const { have, total } = questProgress(save, quest);
  return have >= total ? "ready" : "active";
}

/** Pickups that should be lying on a mission's map right now: only for accepted, unfinished
 * quests, and never one already collected. */
export function activePickupsFor(save: QuestSaveView, missionId: string): { key: string; name: string; x: number; y: number }[] {
  const held = new Set(save.questItems ?? []);
  const active = new Set(save.questsActive ?? []);
  const done = new Set(save.questsDone ?? []);
  const out: { key: string; name: string; x: number; y: number }[] = [];
  for (const quest of QUESTS) {
    if (quest.kind !== "fetch" || quest.missionId !== missionId || !active.has(quest.id) || done.has(quest.id)) continue;
    for (const pickup of quest.pickups ?? []) {
      const key = pickupKey(quest, pickup);
      if (!held.has(key)) out.push({ key, name: pickup.name, x: pickup.x, y: pickup.y });
    }
  }
  return out;
}
