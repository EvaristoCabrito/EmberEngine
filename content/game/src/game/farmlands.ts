import { WEAPONS } from "./data";
import type { PotionId } from "./types";

export interface FarmlandsService {
  kind: "smith" | "potions" | "provisions" | "general" | "rest";
  title: string;
  npcName: string;
  role: string;
  talk: string;
  backdrop: string;
  portrait?: string;
}

const backdrop = "/game/assets/farmlands-service-background-013.png";
export const FARMLANDS_SERVICES: Record<string, FarmlandsService> = {
  "farmlands-smith": { kind: "smith", title: "Ferraria de Duarte", npcName: "Duarte", role: "Ferreiro", talk: "O ferro lembra a mão que o tocou. Tenho armas para quem ainda precisa atravessar estes campos.", backdrop: "/game/assets/farmlands-smith-background-013.png" },
  "farmlands-apothecary": { kind: "potions", title: "Botica de Lia", npcName: "Lia", role: "Boticária", talk: "Trago remédios para feridas, doença e cansaço mágico. Escolha quem vai levar os frascos.", backdrop: "/game/assets/farmlands-apothecary-background-014.jpg" },
  "farmlands-tavern": { kind: "provisions", title: "A Taberna dos Campos", npcName: "Bento", role: "Taberneiro", talk: "Posso preparar provisões para a estrada. Não atravessem os campos de estômago vazio.", backdrop },
  "farmlands-general-store": { kind: "general", title: "Armazém dos Campos", npcName: "Bento", role: "Mercador", talk: "Provisões e gazuas. Não pergunte de onde vieram; pergunte quanto tempo vão durar.", backdrop: "/game/assets/farmlands-general-store-background-015.png", portrait: "/game/sprites/ratCatcher/1.png" },
  "farmlands-inn": { kind: "rest", title: "A Pequena Pousada", npcName: "Marta", role: "Hospedeira", talk: "Há camas limpas e um lugar para conversar. Podem descansar antes de seguir viagem.", backdrop: "/game/assets/farmlands-inn-background-015.png", portrait: "/game/sprites/mothKeeper/1.png" },
};

export const FARMLANDS_INTERIOR_NAMES: Record<string, string> = { ...Object.fromEntries(Object.entries(FARMLANDS_SERVICES).map(([id, service]) => [id, service.title])), "farmlands-villagers": "Casa dos Moradores" };

export const FARMLANDS_POTIONS: PotionId[] = ["weak", "mid", "potent", "disease", "manaSmall", "manaMid", "manaLarge"];

/** Rungs 3–7: middle-tier through the first high-tier weapons; no legendary stock. */
export function isFarmlandsSmithWeapon(id: string): boolean {
  const weapon = WEAPONS[id];
  return !!weapon && weapon.price >= 180 && weapon.price <= 1200;
}

export function isFarmlandsConnector(from: string, to: string | undefined): boolean {
  return !!to && ((from === "farmlands" && !!FARMLANDS_INTERIOR_NAMES[to]) || (!!FARMLANDS_INTERIOR_NAMES[from] && to === "farmlands"));
}

export function farmlandsDoorLabel(target: string): string {
  return target === "farmlands" ? "Sair para os campos" : `Entrar em ${FARMLANDS_INTERIOR_NAMES[target] ?? "Farmlands"}`;
}


