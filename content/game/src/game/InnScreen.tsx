import { FARMLANDS_POTIONS, isFarmlandsSmithWeapon, type FarmlandsService } from "./farmlands";
import { useEffect, useMemo, useRef, useState } from "react";
import { CUTSCENE_SUBTITLES, syncSubtitles } from "./cutsceneSubtitles";
import { ChevronLeft, Volume2, VolumeX } from "lucide-react";
import { Button } from "@/components/ui/button";
import { ALL_HERO_NAMES, BAG_MAX, CLASSES, EQUIPMENT, LOCKPICK_PRICE, POTION_CARRY_MAX, POTION_PRICE, RATION_STACK_MAX, RATIONS_ICON, RATIONS_PRICE, WEAPON_MAX_ENH, WEAPONS, equipmentIcon, equipmentTooltip, equipmentTypeSlotName, heroRecruited, isPlayableClassForDisplay, isPouch, lockpickTooltip, partyBagHasRoom, partyPouchId, potionTooltip, pouchIcon, weaponDiceLabel, weaponEnhCost, weaponIcon, weaponPower, weaponRangeLabel, weaponSellValue, weaponTooltip, potionLabel } from "./data";
import { ItemTip, PartyInventoryOverlay } from "./InventoryScreens";
import { portraitFor } from "./assets";
import type { Bag, ClassId, EquipSlot, PotionId, SaveData, SpriteId } from "./types";
import { GoldAmount } from "./GoldAmount";
import { playTheme, sfxPlay, stopMusic, unlockAudio } from "./audio";
import { fullness, INN_MEAL_PRICE } from "./hunger";
import { HungerBar } from "./HungerBar";
import { questProgress, questStatus, questsFor } from "./quests";
import { useGamePreferences } from "./gamePreferences";
import { weaponMagicSummary } from "./data";

const BAG_ICON = pouchIcon(null);
export const HEALER_CAST_PRICE = 5;
export const HEALER_AILMENT_PRICE = 7;
export const NIGHT_REST_PRICE = 8;

interface HealerTarget {
  name: string;
  hp: number;
  maxHp: number;
  sprite: SpriteId;
  diseased: boolean;
  poisoned: boolean;
}

const NPCS = [
  {
    id: "brue",
    name: "Brue",
    role: "Estalajadeiro",
    portrait: "/game/portraits/brue.png",
    talk: "O fogo ainda pega. As camas, não. Gold compra o que sobrou da adega. Salazar pode levar frasco — não precisa, mas o copo não recusa.",
    shop: true,
  },
  {
    id: "mudo",
    name: "O Mudo",
    role: "Viajante",
    portrait: "/game/portraits/mudo.png",
    talk: "Ele não fala. Nunca. Carrega uma pequena tábua de madeira e um pedaço de giz, e usa-a para responder quando não pode simplesmente apontar ou ir embora. Na tábua, uma frase aparece repetidamente: “Não fique parado por muito tempo.” Ninguém sabe de onde ele veio. Conhece estradas que não aparecem em mapa algum e parece sempre estar a caminho de algum lugar. Quando perguntam quanto custa uma bebida na velha adega, ele escreve apenas: “Ainda tem preço.”",
    shop: false,
  },
  {
    id: "suspicious",
    name: "Cliente Suspeito",
    role: "Viajante",
    portrait: "/game/portraits/1beca9c9-789c-4b44-8930-c53b88c982ac.jpg",
    talk: "Ele mantém o rosto meio escondido e observa quem entra e quem sai. Parece escolher cada palavra antes de pronunciá-la.",
    shop: false,
  },
  {
    id: "porao",
    name: "A Hóspede",
    role: "Porão",
    portrait: "/game/portraits/porao.png",
    talk: "Não subo. O chão me conhece. Tragam histórias, não luz. Se Brue ainda mede Gold, o mundo não acabou.",
    shop: false,
  },
  {
    id: "merchant",
    name: "Rambarton",
    role: "Mercador Itinerante",
    portrait: "/game/portraits/traveling-merchant.png",
    talk: "Poções, rações e gazuas. Levo pouco, mas escolhi bem.",
    shop: true,
  },
] as const;

const POTION_ORDER: PotionId[] = ["weak", "mid", "potent", "disease", "manaSmall", "manaMid", "manaLarge"];
const MERCHANT_POTION_ORDER: PotionId[] = ["weak", "mid", "manaSmall"];

const ICONS: Record<PotionId, string> = {
  weak: "/game/icons/potion-weak.png",
  mid: "/game/icons/potion-mid.png",
  potent: "/game/icons/potion-potent.png",
  disease: "/game/icons/potion-disease.png",
  manaSmall: "/game/icons/potion-manaSmall.png?v=ds2",
  manaMid: "/game/icons/potion-manaMid.png?v=ds2",
  manaLarge: "/game/icons/potion-manaLarge.png?v=ds2",
};

const EMPTY_CART: Record<PotionId, number> = { weak: 0, mid: 0, potent: 0, disease: 0, manaSmall: 0, manaMid: 0, manaLarge: 0 };

export function InnScreen({
  onUseRation,
  onUseRationAll,
  onBuyMeal,
  onBuyMealAll,
  onOpenStatus,
  bags,
  ember,
  muted,
  weapons,
  equipped,
  heroClass,
  save,
  test,
  onMute,
  onLeave,
  onPay,
  onBuyRations,
  onBuyWeapon,
  onBuyEquipment,
  onEquipWeapon,
  onEquipItem,
  onUsePotion,
  onDiscardWeapon,
  onDiscardEquipment,
  onDiscardRation,
  onDiscardBagItem,
  onUpgradeWeapon,
  onSellWeapon,
  onSeenSmithIntro,
  onAcceptQuest,
  onTurnInQuest,
  onTalkToNpc,
  onPassNight,
  questOffered,
  farmlandsService,
  startInSmith = false,
  startInHealer = false,
  startInMerchant = false,
  startInMerchantGear = false,
  merchantBackdrop,
  healerTargets = [],
  onHealerCast,
  onHealerCureAilments,
}: {
  /** Inn quests (see quests.ts): accept an offered quest / hand in a finished one for its
   * Gold reward. Each returns whether it actually applied. */
  onAcceptQuest?: (questId: string) => boolean;
  onTurnInQuest?: (questId: string) => boolean;
  /** Called whenever an NPC is opened, so progression can note who has been talked to and
   * which of their quests have been learned of (see progression.ts). */
  onTalkToNpc?: (npcId: string) => void;
  /** Spends the per-person overnight fee, advances one day, and restores the party. */
  onPassNight?: (heroes: string[]) => { day: number; healed: number } | false;
  /** Whether a not-yet-accepted quest is on offer right now (its own availability condition).
   * Omitted means every quest is offered. */
  questOffered?: (questId: string) => boolean;
  /** Opened by talking to Vargan in the walkable Inn: goes straight to the smith (intro
   * video first, if not seen yet), and leaving the smith leaves this screen entirely. */
  farmlandsService?: FarmlandsService;
  startInSmith?: boolean;
  /** Opened by talking to Curandeiro Ancião in the walkable Inn. */
  startInHealer?: boolean;
  /** Opens the limited roadside merchant inventory instead of the inn menus. */
  startInMerchant?: boolean;
  /** Roadside equipment selection priced between 320 and 1300 Gold. */
  startInMerchantGear?: boolean;
  /** Roadside or mountain-market scene for the merchant's shop. */
  merchantBackdrop?: string;
  healerTargets?: HealerTarget[];
  /** Applies one paid Cura Média cast and returns the HP actually restored. */
  onHealerCast?: (hero: string) => number | false;
  /** Cures every selected party member's persistent disease/poison and returns count. */
  onHealerCureAilments?: (heroes: string[]) => number | false;
  bags: Record<string, Bag>;
  onUseRation: (hero: string) => void;
  onUseRationAll?: (heroes: string[]) => number;
  onBuyMeal: (hero: string) => boolean;
  /** Feeds every hero currently shown in the Adega's hero row in one go, cheapest way to
   * clear hunger for the whole party — same per-hero price and 120% cap as Comer, just
   * skips whoever's already full or has no Gold left by the time their turn comes.
   * Returns how many actually ate, for the note text. */
  onBuyMealAll: (heroes: string[]) => number;
  onOpenStatus: (hero: string) => void;
  ember: number;
  muted: boolean;
  weapons: Record<string, number>;
  equipped: Record<string, string>;
  heroClass: Record<string, ClassId>;
  save: SaveData;
  /** Test/review mode — the Smith stocks every weapon, item, and piece of gear in the game. */
  test?: boolean;
  onMute: () => void;
  onLeave: () => void;
  onPay: (hero: string, cart: Record<PotionId, number>, lockpicks: number) => boolean;
  /** Party-wide, unlike onPay's per-hero cart — rations don't belong to one hero. */
  onBuyRations: (qty: number) => boolean;
  onBuyWeapon: (hero: string, weaponId: string) => boolean;
  onBuyEquipment: (itemId: string) => boolean;
  onEquipWeapon: (hero: string, weaponId: string) => void;
  onEquipItem?: (hero: string, slot: EquipSlot, itemId: string | null) => void;
  onUsePotion?: (hero: string, kind: PotionId) => void;
  onDiscardWeapon?: (weaponId: string) => void;
  onDiscardEquipment?: (itemId: string) => void;
  onDiscardRation?: () => void;
  onDiscardBagItem?: (hero: string, kind: PotionId | "lockpick") => void;
  onUpgradeWeapon: (weaponId: string) => boolean;
  onSellWeapon: (weaponId: string) => number | false;
  onSeenSmithIntro: () => void;
}) {
  const [view, setView] = useState<"npc" | "smith" | "healer">(farmlandsService?.kind === "smith" ? "smith" : startInMerchantGear ? "smith" : startInHealer ? "healer" : startInSmith && save.seenSmithIntro ? "smith" : "npc");
  // Esc leaves, same as the Sair button (an open dialog closes on Esc by itself first).
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key !== "Escape" || e.defaultPrevented || document.querySelector("dialog[open]") || (e.target as HTMLElement | null)?.closest("input, textarea, select")) return;
      e.preventDefault();
      onLeave();
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [onLeave]);
  const [smithIntro, setSmithIntro] = useState(startInSmith && !farmlandsService && !save.seenSmithIntro);
  const [npc, setNpc] = useState<(typeof NPCS)[number]>(startInMerchant || startInMerchantGear ? NPCS[NPCS.length - 1] : NPCS[0]);
  const npcOptions = startInMerchant || startInMerchantGear ? [NPCS[NPCS.length - 1]] : NPCS.slice(0, -1);
  const [hero, setHero] = useState<string>("Kael");
  useEffect(() => {
    onTalkToNpc?.(npc.id);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [npc.id]);
  const [cart, setCart] = useState<Record<PotionId, number>>({ ...EMPTY_CART });
  const [lockpickQty, setLockpickQty] = useState(0);
  const [rationsQty, setRationsQty] = useState(0);
  const [editingRationsQty, setEditingRationsQty] = useState(false);
  const [rationsQtyDraft, setRationsQtyDraft] = useState("0");
  const [rationsNote, setRationsNote] = useState<string | null>(null);
  const [note, setNote] = useState<string | null>(null);
  const [restNote, setRestNote] = useState<string | null>(null);
  const [invView, setInvView] = useState<"doll" | "pack" | null>(null);
  useEffect(() => {
    if (!note) return;
    const timer = window.setTimeout(() => setNote(null), 2200);
    return () => window.clearTimeout(timer);
  }, [note]);
  useEffect(() => {
    if (!rationsNote) return;
    const timer = window.setTimeout(() => setRationsNote(null), 2200);
    return () => window.clearTimeout(timer);
  }, [rationsNote]);
  useEffect(() => {
    if (!restNote) return;
    const timer = window.setTimeout(() => setRestNote(null), 3500);
    return () => window.clearTimeout(timer);
  }, [restNote]);
  const bag = bags[hero] ?? { mid: 0, weak: 0, potent: 0, disease: 0, manaSmall: 0, manaMid: 0, manaLarge: 0, lockpick: 0 };
  // Same roster the Adega's own hero-selector row shows — Todos feeds exactly whoever
  // Comer could already feed one at a time, never a hero outside that list.
  const partyRoster = useMemo(
    () =>
      ALL_HERO_NAMES.filter((name) => test || heroRecruited(name, save.completed, save.flags)),
    [test, save.completed, save.flags],
  );
  const mealTargets = partyRoster.filter((name) => fullness(save.heroHunger[name]) < 120 && (save.unitHp[name] ?? 1) > 0);
  const mealAllCost = mealTargets.length * INN_MEAL_PRICE;

  const total = useMemo(
    () => POTION_ORDER.reduce((n, kind) => n + cart[kind] * POTION_PRICE[kind], 0) + lockpickQty * LOCKPICK_PRICE,
    [cart, lockpickQty],
  );
  const items = POTION_ORDER.reduce((n, kind) => n + cart[kind], 0) + lockpickQty;
  const remain = ember - total;

  const add = (kind: PotionId, delta: number) => {
    setNote(null);
    setCart((prev) => {
      const next = Math.max(0, (prev[kind] ?? 0) + delta);
      const have = bag[kind] ?? 0;
      const cap = Math.max(0, POTION_CARRY_MAX[kind] - have);
      return { ...prev, [kind]: Math.min(next, cap) };
    });
  };

  const addLockpick = (delta: number) => {
    setNote(null);
    setLockpickQty((prev) => {
      const next = Math.max(0, prev + delta);
      const cap = Math.max(0, BAG_MAX - (bag.lockpick ?? 0));
      return Math.min(next, cap);
    });
  };

  const pay = () => {
    if (items <= 0) {
      setNote("Nada no copo.");
      return;
    }
    if (remain < 0) {
      setNote(`Faltam ${-remain} Gold.`);
      return;
    }
    const ok = onPay(hero, cart, lockpickQty);
    if (!ok) {
      setNote(farmlandsService ? `${farmlandsService.npcName}: falta Gold ou espaço.` : "Brue recusou. Gold ou espaço.");
      return;
    }
    setCart({ ...EMPTY_CART });
    setLockpickQty(0);
    setNote(farmlandsService?.kind === "general" ? "Pago. As gazuas foram para a mochila." : "Pago. Os frascos foram para o saco.");
  };

  // Party-wide, so it's a separate small purchase rather than folded into pay()'s
  // per-hero cart — there's no hero to charge it to.
  const buyRations = () => {
    if (rationsQty <= 0) {
      setRationsNote("Nada no carrinho.");
      return;
    }
    const ok = onBuyRations(rationsQty);
    if (!ok) {
      setRationsNote(farmlandsService ? `${farmlandsService.npcName}: falta Gold ou espaço na mochila.` : "Brue recusou. Gold ou espaço na mochila.");
      return;
    }
    setRationsQty(0);
    setRationsNote("Pago. As rações foram para a mochila.");
  };

  const finishSmithIntro = () => {
    setSmithIntro(false);
    onSeenSmithIntro();
    setView("smith");
    if (!muted) playTheme("inn");
  };

  const enterSmith = () => {
    unlockAudio();
    sfxPlay.ui();
    if (save.seenSmithIntro) {
      setView("smith");
      return;
    }
    stopMusic();
    setSmithIntro(true);
  };

  if (smithIntro) {
    return <SmithIntroScreen muted={muted} onMute={onMute} onSkip={finishSmithIntro} />;
  }

  if (view === "smith") {
    return (
      <SmithPanel
        ember={ember}
        muted={muted}
        weapons={weapons}
        equipped={equipped}
        heroClass={heroClass}
        save={save}
        test={test}
        onMute={onMute}
        farmlandsService={farmlandsService}
        merchantGear={startInMerchantGear}
        merchantBackdrop={merchantBackdrop}
        onBack={startInSmith || startInMerchantGear ? onLeave : () => setView("npc")}
        onBuyWeapon={onBuyWeapon}
        onBuyEquipment={onBuyEquipment}
        onEquipWeapon={onEquipWeapon}
        onEquipItem={onEquipItem}
        onUsePotion={onUsePotion}
        onDiscardWeapon={onDiscardWeapon}
        onDiscardEquipment={onDiscardEquipment}
        onDiscardRation={onDiscardRation}
        onDiscardBagItem={onDiscardBagItem}
        onOpenStatus={onOpenStatus}
        onUpgradeWeapon={onUpgradeWeapon}
        onSellWeapon={onSellWeapon}
      />
    );
  }

  if (view === "healer") {
    return (
      <HealerServicePanel
        ember={ember}
        muted={muted}
        targets={healerTargets}
        onLeave={onLeave}
        onMute={onMute}
        onCast={onHealerCast ?? (() => false)}
        onCureAilments={onHealerCureAilments ?? (() => false)}
      />
    );
  }

  return (
    <section className="shop-surface relative h-dvh min-h-0 flex flex-col overflow-hidden bg-bg">
      <img src={farmlandsService?.backdrop ?? (startInMerchant ? merchantBackdrop ?? "/game/assets/merchant-road-background-001.jpg" : "/game/assets/brief-estalagem.jpg")} alt="" className="absolute inset-0 h-full w-full object-cover" />
      <div className="absolute inset-0 bg-gradient-to-t from-bg/80 via-bg/25 to-bg/10" />
      <header className="relative z-10 flex items-center gap-3 px-4 pt-[max(1rem,env(safe-area-inset-top))] pb-3">
        {/* Back/exit always sits at the far left, across every screen, so it never gets lost. */}
        <button type="button" onClick={onLeave} className="h-10 px-3 rounded-md ember-chip text-xs uppercase tracking-[0.14em]">
          Sair
        </button>
        <div className="min-w-0 flex-1">
          <p className="text-xs ember-kicker">{farmlandsService ? "Farmlands" : startInMerchant ? (startInMerchantGear ? "Entre os picos nevados" : "Parada na estrada") : "Pousada à margem da cinza"}</p>
          <h1 className="font-display text-2xl leading-none ember-title">{farmlandsService?.title ?? (startInMerchant ? "Rambarton - Mercador Itinerante" : "A Estalagem do Osso Seco")}</h1>
        </div>
        <button
          type="button"
          onClick={() => setInvView("pack")}
          className="h-12 px-3 rounded-md ember-chip text-xs uppercase tracking-[0.14em] flex items-center gap-2"
        >
          <img src={BAG_ICON} alt="" className="size-8 shrink-0 rounded-sm object-contain" />
          Mochila
        </button>
        <button
          type="button"
          onClick={() => setInvView("doll")}
          className="h-12 px-3 rounded-md ember-chip text-xs uppercase tracking-[0.14em]"
        >
          Equipar
        </button>
        {!startInMerchant && !farmlandsService && (<button
          type="button"
          onClick={enterSmith}
          className="h-10 px-3 rounded-md ember-chip text-xs uppercase tracking-[0.14em]"
        >
          Ferreiro
        </button>)}
        <p className="text-sm ember-chip rounded-md px-2 py-1"><GoldAmount amount={ember} /></p>
        <button type="button" onClick={onMute} className="size-10 grid place-items-center rounded-md ember-chip" aria-label="Som">
          {muted ? <VolumeX className="size-4" /> : <Volume2 className="size-4" />}
        </button>
      </header>
      <div className="relative z-10 flex-1 min-h-0 overflow-y-auto p-4 flex flex-col gap-3 max-w-lg mx-auto w-full">
        <div className="grid grid-cols-3 gap-2">
          {(farmlandsService ? [] : npcOptions).map((n) => (
            <button
              key={n.id}
              type="button"
              onClick={() => setNpc(n)}
              className={`ember-slot overflow-hidden text-left${npc.id === n.id ? " is-last" : ""}`}
            >
              <img src={n.portrait} alt="" className="w-full aspect-[2/3] object-cover" />
              <p className="px-2 py-1 text-xs font-medium truncate ember-chip">{n.name}</p>
            </button>
          ))}
        </div>
        <div className="relative ember-panel p-3 flex gap-3">
          <img src={farmlandsService?.portrait ?? npc.portrait} alt="" className="h-24 w-16 object-cover rounded-md shrink-0" />
          <div className="min-w-0">
            <p className="text-sm font-medium">
              {farmlandsService ? `${farmlandsService.npcName} · ${farmlandsService.role}` : npc.id === "merchant" ? `${npc.name} - ${npc.role}` : `${npc.name} · ${npc.role}`}
            </p>
            <p className="mt-1 text-sm leading-relaxed text-fg/90">{farmlandsService?.talk ?? npc.talk}</p>
          </div>
        </div>
        {npc.id === "brue" && !startInMerchant && (
          <div className="relative ember-panel p-3 flex flex-col gap-2">
            <p className="text-xs ember-kicker">Descanso · uma noite</p>
            <p className="text-sm leading-relaxed text-fg/90">Dormir na estalagem avança um dia, recupera 75% do HP perdido e restaura todos os usos de feitiço.</p>
            <p className="text-sm tabular-nums">Prévia do custo · {partyRoster.length} pessoas × {NIGHT_REST_PRICE} Gold = <GoldAmount amount={partyRoster.length * NIGHT_REST_PRICE} /></p>
            <p className="text-xs text-muted">Após dormir: {Math.max(0, ember - partyRoster.length * NIGHT_REST_PRICE)} Gold · Dia {save.gameClock} → {save.gameClock + 1}</p>
            {restNote && <p className="text-sm text-accent">{restNote}</p>}
            <Button
              className="ember-btn ember-btn-primary"
              disabled={!onPassNight || partyRoster.length === 0 || ember < partyRoster.length * NIGHT_REST_PRICE}
              onClick={() => {
                const result = onPassNight?.(partyRoster);
                if (!result) {
                  setRestNote("Brue recusou. Falta Gold para todo o grupo.");
                  return;
                }
                sfxPlay.ui();
                setRestNote(`Dia ${result.day}. O grupo recuperou ${result.healed} HP e todos os feitiços foram restaurados.`);
              }}
            >
              Passar a Noite · {partyRoster.length * NIGHT_REST_PRICE} Gold
            </Button>
          </div>
        )}
        {(farmlandsService || npc.id === "merchant" ? [] : questsFor(npc.id)).map((quest) => {
          const status = questStatus(save, quest);
          if (status === "available" && questOffered && !questOffered(quest.id)) return null;
          const { have, total } = questProgress(save, quest);
          return (
            <div key={quest.id} className="relative ember-panel p-3 flex flex-col gap-2">
              <p className="text-xs ember-kicker">
                Missão · {status === "done" ? "concluída" : status === "available" ? "oferecida" : status === "ready" ? "pronta para entregar" : "em andamento"}
              </p>
              <p className="text-sm font-medium">{quest.title}</p>
              <p className="text-sm leading-relaxed text-fg/90">
                {status === "available" ? quest.offer : status === "active" ? quest.active : status === "ready" ? quest.ready : quest.done}
              </p>
              {status !== "done" && <p className="text-xs text-muted">Onde: {quest.place}</p>}
              {status !== "available" && status !== "done" && (
                <p className="text-xs text-muted tabular-nums">
                  {quest.kind === "kill" ? (have >= total ? "Alvo abatido" : "Alvo ainda vivo") : quest.kind === "recruit" ? (have >= total ? "Oficial resgatado" : "Oficial ainda cativo") : `Encontrados ${have} / ${total}`}
                </p>
              )}
              {status !== "done" && (
                <div className="flex flex-wrap items-center gap-x-1 text-xs text-muted">
                  <span>Recompensa:</span>
                  <GoldAmount amount={quest.reward} />
                  {quest.rewardPotions.map((kind) => (
                    <span key={kind} className="break-words">+ {potionLabel(kind)}</span>
                  ))}
                </div>
              )}
              {status === "available" && (
                <Button
                  className="ember-btn ember-btn-primary"
                  onClick={() => {
                    if (onAcceptQuest?.(quest.id)) sfxPlay.ui();
                  }}
                >
                  Aceitar missão
                </Button>
              )}
              {status === "ready" && (
                <Button
                  className="ember-btn ember-btn-primary"
                  onClick={() => {
                    if (onTurnInQuest?.(quest.id)) sfxPlay.ui();
                  }}
                >
                  Entregar · {quest.reward} Gold
                </Button>
              )}
            </div>
          );
        })}
        {npc.shop && farmlandsService?.kind !== "rest" && (
          <div className="shop-panel relative ember-panel p-3 flex flex-col gap-2">
            <p className="text-xs ember-kicker">{startInMerchant ? "Estoque limitado · escolha o que levar" : "Adega · quem leva"}</p>
            {farmlandsService?.kind !== "provisions" && (<>
            <div className="flex flex-wrap gap-1">
              {partyRoster.map((name) => (
                <Button
                  key={name}
                  className={`shop-hero-selector ember-btn ember-btn-sm ${hero === name ? "ember-btn-primary" : "ember-btn-ghost"}`}
                  size="sm"
                  variant={hero === name ? undefined : "quiet"}
                  onClick={() => {
                    setHero(name);
                    setCart({ ...EMPTY_CART });
                    setLockpickQty(0);
                    setNote(null);
                  }}
                >
                  {name}
                </Button>
              ))}
            </div>
            <div className="flex flex-col gap-1">
              <div className="ember-slot p-3">
                <p className="text-sm">Refeição para {hero} · {INN_MEAL_PRICE} Gold</p>
                <p className="text-xs text-muted">Enche a saciedade até 120% · bônus de 20%</p>
                <HungerBar name={hero} value={save.heroHunger[hero]} />
                <div className="flex gap-1.5 mt-2">
                  <Button className="flex-1 ember-btn ember-btn-sm ember-btn-primary" disabled={ember < INN_MEAL_PRICE || fullness(save.heroHunger[hero]) >= 120 || (save.unitHp[hero] ?? 1) <= 0} onClick={() => setNote(onBuyMeal(hero) ? `${hero} comeu. Saciedade: 120%.` : "Falta Gold ou o personagem já está satisfeito.")}>
                    Comer · {INN_MEAL_PRICE} Gold
                  </Button>
                  <Button
                    className="flex-1 ember-btn ember-btn-sm ember-btn-ghost"
                    variant="quiet"
                    disabled={mealTargets.length === 0 || ember < INN_MEAL_PRICE}
                    onClick={() => {
                      const fed = onBuyMealAll(partyRoster);
                      setNote(
                        fed === 0
                          ? "Ninguém comeu. Falta Gold ou já estão satisfeitos."
                          : fed === mealTargets.length
                            ? "Todos comeram. Saciedade: 120%."
                            : `${fed} comeram por ${fed * INN_MEAL_PRICE} Gold · Gold não deu pros demais.`,
                      );
                    }}
                  >
                    Todos · {mealAllCost} Gold
                  </Button>
                </div>
                <p className="text-xs text-muted tabular-nums">
                  Prévia: {mealTargets.length} {mealTargets.length === 1 ? "pessoa" : "pessoas"} podem comer · total {mealAllCost} Gold
                </p>
              </div>
              {(farmlandsService?.kind === "potions" ? FARMLANDS_POTIONS : farmlandsService ? [] : startInMerchant ? MERCHANT_POTION_ORDER : POTION_ORDER).map((kind) => {
                const price = POTION_PRICE[kind];
                const have = bag[kind] ?? 0;
                const qty = cart[kind] ?? 0;
                return (
                  <ItemTip key={kind} text={potionTooltip(kind)} className="block">
                    <div className="tavern-item-window flex items-center gap-2 rounded-md border border-border px-2 py-1.5">
                      <img src={ICONS[kind]} alt="" className="size-6 rounded-sm object-cover bg-black" />
                      <span className="flex-1 text-sm min-w-0">
                        {potionLabel(kind)}
                        <span className="block text-[11px] text-muted tabular-nums">
                          comprar {qty} · tem {have} / {POTION_CARRY_MAX[kind]} · {price} Gold
                        </span>
                      </span>
                      <button type="button" className="size-8 grid place-items-center rounded-md border border-border bg-surface-2" onClick={() => add(kind, -1)} disabled={qty <= 0}>
                        −
                      </button>
                      <span className="w-6 text-center text-sm tabular-nums">{qty}</span>
                      <button
                        type="button"
                        className="size-8 grid place-items-center rounded-md border border-border bg-surface-2"
                        onClick={() => add(kind, 1)}
                        disabled={have + qty >= POTION_CARRY_MAX[kind]}
                      >
                        +
                      </button>
                    </div>
                  </ItemTip>
                );
              })}
              {(!farmlandsService || farmlandsService.kind === "general") && (<>
              <ItemTip text={lockpickTooltip()} className="block">
                <div className="tavern-item-window flex items-center gap-2 rounded-md border border-border px-2 py-1.5">
                  <img src="/game/icons/lockpick.png" alt="" className="size-6 rounded-sm object-cover bg-black" />
                  <span className="flex-1 text-sm min-w-0">
                    Gazua
                    <span className="block text-[11px] text-muted tabular-nums">
                      comprar {lockpickQty} · tem {bag.lockpick ?? 0} / {BAG_MAX} · {LOCKPICK_PRICE} Gold
                    </span>
                  </span>
                  <button type="button" className="size-8 grid place-items-center rounded-md border border-border bg-surface-2" onClick={() => addLockpick(-1)} disabled={lockpickQty <= 0}>
                    −
                  </button>
                  <span className="w-6 text-center text-sm tabular-nums">{lockpickQty}</span>
                  <button
                    type="button"
                    className="size-8 grid place-items-center rounded-md border border-border bg-surface-2"
                    onClick={() => addLockpick(1)}
                    disabled={(bag.lockpick ?? 0) + lockpickQty >= BAG_MAX}
                  >
                    +
                  </button>
                </div>
              </ItemTip>
              </>)}
            </div>
            <p className={`text-sm tabular-nums ${remain < 0 ? "text-danger" : "text-muted"}`}>
              Conta {total} Gold · restam {remain}
            </p>
            {note && <p className="text-sm text-accent">{note}</p>}
            <div className="flex gap-2">
              <Button className="flex-1 ember-btn ember-btn-sm ember-btn-primary" disabled={items <= 0} onClick={pay}>
                Pagar
              </Button>
              <Button variant="quiet" className="ember-btn ember-btn-sm ember-btn-ghost" onClick={() => { setCart({ ...EMPTY_CART }); setNote(null); }}>
                Limpar
              </Button>
            </div>

            </>)}
            {farmlandsService?.kind !== "potions" && (<>
            <p className="mt-2 text-xs ember-kicker">Rações · para toda a party</p>
            <ItemTip text="Alimenta o grupo inteiro por um dia cada, no mapa. Empilha até 30 por espaço na mochila." className="block">
              <div className="tavern-item-window flex items-center gap-2 rounded-md border border-border px-2 py-1.5">
                <img src={RATIONS_ICON} alt="" className="size-6 rounded-sm object-cover bg-black" />
                <span className="flex-1 text-sm min-w-0">
                  Rações
                  <span className="block text-[11px] text-muted tabular-nums">
                    comprar {rationsQty} · tem {save.rations} · {RATIONS_PRICE} Gold cada
                  </span>
                </span>
                <button type="button" className="size-8 grid place-items-center rounded-md border border-border bg-surface-2" onClick={() => setRationsQty((q) => Math.max(0, q - 1))} disabled={rationsQty <= 0}>
                  −
                </button>
                {editingRationsQty ? (
                  <input
                    type="number"
                    inputMode="numeric"
                    min={0}
                    max={RATION_STACK_MAX * 20}
                    autoFocus
                    value={rationsQtyDraft}
                    onFocus={(e) => e.currentTarget.select()}
                    onChange={(e) => setRationsQtyDraft(e.target.value)}
                    onBlur={() => {
                      const parsed = Math.max(0, Math.min(RATION_STACK_MAX * 20, Math.floor(Number(rationsQtyDraft) || 0)));
                      setRationsQty(parsed);
                      setEditingRationsQty(false);
                    }}
                    onKeyDown={(e) => {
                      if (e.key === "Enter") e.currentTarget.blur();
                      if (e.key === "Escape") {
                        setRationsQtyDraft(String(rationsQty));
                        setEditingRationsQty(false);
                      }
                    }}
                    className="w-12 text-center text-sm tabular-nums bg-bg border border-border rounded-md"
                  />
                ) : (
                  <button
                    type="button"
                    className="w-6 text-center text-sm tabular-nums"
                    onClick={() => {
                      setRationsQtyDraft(String(rationsQty));
                      setEditingRationsQty(true);
                    }}
                    aria-label="Digitar quantidade de rações"
                  >
                    {rationsQty}
                  </button>
                )}
                <button
                  type="button"
                  className="size-8 grid place-items-center rounded-md border border-border bg-surface-2"
                  onClick={() => setRationsQty((q) => Math.min(q + 1, RATION_STACK_MAX * 20))}
                  disabled={!partyBagHasRoom(save, Math.ceil((save.rations + rationsQty + 1) / RATION_STACK_MAX) - Math.ceil(save.rations / RATION_STACK_MAX), test)}
                >
                  +
                </button>
              </div>
            </ItemTip>
            {rationsNote && <p className="text-sm text-accent">{rationsNote}</p>}
            <Button className="w-full ember-btn ember-btn-primary" disabled={rationsQty <= 0 || ember < rationsQty * RATIONS_PRICE} onClick={buyRations}>
              Comprar {rationsQty > 0 ? `(${rationsQty * RATIONS_PRICE} Gold)` : ""}
            </Button>
            </>)}
          </div>
        )}
      </div>
      <div className="relative z-10 p-4 pt-0 pb-[max(1rem,env(safe-area-inset-bottom))] max-w-lg mx-auto w-full">
        <Button variant="ghost" className="w-full ember-btn ember-btn-ghost" onClick={onLeave}>
          <ChevronLeft className="size-4" /> {farmlandsService ? "Voltar à casa" : "Sair da Adega"}
        </Button>
      </div>
      {invView && (
        <PartyInventoryOverlay
          heroName={hero}
          classId={heroClass[hero] ?? "swordsman"}
          save={save}
          test={test}
          onUseRation={onUseRation}
          onUseRationAll={onUseRationAll}
          onOpenStatus={(h) => {
            // The status sheet renders behind this overlay (both share z-40, and this one
            // mounts later) — close it first or the "Status" button looks like it does
            // nothing while it's actually opening right behind the Mochila/Equipar screen.
            setInvView(null);
            onOpenStatus(h);
          }}
          onClose={() => setInvView(null)}
          onEquipWeapon={onEquipWeapon}
          onEquipItem={onEquipItem}
          onUsePotion={onUsePotion}
          onDiscardWeapon={onDiscardWeapon}
          onDiscardEquipment={onDiscardEquipment}
          onDiscardRation={onDiscardRation}
          onDiscardBagItem={onDiscardBagItem}
          initialView={invView === "pack" ? "backpack" : "equipment"}
        />
      )}
    </section>
  );
}

function HealerServicePanel({
  ember,
  muted,
  targets,
  onLeave,
  onMute,
  onCast,
  onCureAilments,
}: {
  ember: number;
  muted: boolean;
  targets: HealerTarget[];
  onLeave: () => void;
  onMute: () => void;
  onCast: (hero: string) => number | false;
  onCureAilments: (heroes: string[]) => number | false;
}) {
  const [casts, setCasts] = useState<Record<string, number>>({});
  const [healing, setHealing] = useState<{ hero: string; amount: number } | null>(null);
  const [busy, setBusy] = useState(false);
  const [note, setNote] = useState<string | null>(null);
  const castRef = useRef(onCast);
  useEffect(() => {
    castRef.current = onCast;
  }, [onCast]);

  const neededCasts = (target: HealerTarget) => {
    if (target.hp <= 0 || target.hp >= target.maxHp) return 0;
    const perCast = Math.max(1, Math.ceil(target.maxHp * 0.25));
    return Math.min(4, Math.ceil((target.maxHp - target.hp) / perCast));
  };
  const selectedPlan = targets.flatMap((target) => {
    const count = Math.min(neededCasts(target), Math.max(0, casts[target.name] ?? 0));
    return count > 0 ? [{ hero: target.name, count }] : [];
  });
  const allPlan = targets.flatMap((target) => {
    const count = neededCasts(target);
    return count > 0 ? [{ hero: target.name, count }] : [];
  });
  const planCost = (plan: { hero: string; count: number }[]) => plan.reduce((sum, item) => sum + item.count * HEALER_CAST_PRICE, 0);
  const selectedCost = planCost(selectedPlan);
  const allCost = planCost(allPlan);
  const ailmentTargets = targets.filter((target) => target.diseased || target.poisoned);
  const ailmentCost = ailmentTargets.length * HEALER_AILMENT_PRICE;

  const treat = async (plan: { hero: string; count: number }[], cost: number) => {
    if (busy || plan.length === 0) return;
    if (cost > ember) {
      setNote(`São necessários ${cost} Gold; você tem ${ember}.`);
      return;
    }
    setNote(null);
    setBusy(true);
    for (const item of plan) {
      for (let index = 0; index < item.count; index++) {
        const amount = castRef.current(item.hero);
        if (amount === false || amount <= 0) {
          setNote("O Curandeiro não conseguiu completar a cura.");
          setBusy(false);
          setHealing(null);
          return;
        }
        setHealing({ hero: item.hero, amount });
        await new Promise<void>((resolve) => window.setTimeout(resolve, 650));
      }
    }
    setHealing(null);
    setCasts({});
    setBusy(false);
  };

  return (
    <section className="shop-surface relative h-dvh min-h-0 flex flex-col overflow-hidden bg-bg">
      <img src="/game/assets/brief-estalagem.jpg" alt="" className="absolute inset-0 h-full w-full object-cover" />
      <div className="absolute inset-0 bg-gradient-to-t from-bg/85 via-bg/45 to-bg/25" />
      <header className="relative z-10 flex items-center gap-3 px-4 pt-[max(1rem,env(safe-area-inset-top))] pb-3">
        <button type="button" onClick={onLeave} disabled={busy} className="h-10 px-3 rounded-md ember-chip text-xs uppercase tracking-[0.14em] disabled:opacity-50">Voltar</button>
        <div className="min-w-0 flex-1">
          <p className="text-xs ember-kicker">Curandeiro Ancião</p>
          <h1 className="font-display text-2xl leading-none ember-title">Cura Média</h1>
        </div>
        <p className="text-sm ember-chip rounded-md px-2 py-1"><GoldAmount amount={ember} /></p>
        <button type="button" onClick={onMute} className="size-10 grid place-items-center rounded-md ember-chip" aria-label="Som">
          {muted ? <VolumeX className="size-4" /> : <Volume2 className="size-4" />}
        </button>
      </header>
      <div className="relative z-10 flex-1 min-h-0 overflow-y-auto p-4 flex flex-col gap-3 max-w-xl mx-auto w-full">
        <div className="ember-panel p-3 flex flex-col gap-1">
          <p className="text-sm leading-relaxed">“Uma Cura Média restaura 25% do HP máximo. Cinco Gold por conjuração; no máximo quatro por pessoa.”</p>
          <p className="text-xs text-muted">Cada conjuração mostra a Cura Média sobre o personagem tratado.</p>
        </div>
        <div className="ember-panel p-3 flex flex-col gap-2">
          <p className="text-sm font-medium">Curar doenças e venenos</p>
          <p className="text-xs text-muted">
            {ailmentTargets.length > 0
              ? `Tratamento de ${ailmentTargets.map((target) => target.name).join(", ")} · ${ailmentTargets.length} ${ailmentTargets.length === 1 ? "pessoa" : "pessoas"} × ${HEALER_AILMENT_PRICE} Gold.`
              : "Ninguém do grupo está doente ou envenenado."}
          </p>
          <p className="text-sm tabular-nums">Prévia do custo · <GoldAmount amount={ailmentCost} /> · após o tratamento: {Math.max(0, ember - ailmentCost)} Gold</p>
          <Button className="ember-btn ember-btn-ghost" variant="quiet" disabled={busy || ailmentTargets.length === 0 || ailmentCost > ember} onClick={() => {
            const count = onCureAilments(ailmentTargets.map((target) => target.name));
            setNote(count === false ? "O Curandeiro não conseguiu tratar o grupo." : count === 0 ? "Ninguém precisava de tratamento." : `${count} ${count === 1 ? "pessoa foi tratada" : "pessoas foram tratadas"}.`);
          }}>Tratar doenças e venenos · {ailmentCost} Gold</Button>
        </div>
        {targets.map((target) => {
          const allowed = neededCasts(target);
          const chosen = Math.min(allowed, Math.max(0, casts[target.name] ?? 0));
          const perCast = Math.max(1, Math.ceil(target.maxHp * 0.25));
          const previewHp = Math.min(target.maxHp, target.hp + perCast * chosen);
          const portrait = portraitFor(target.sprite);
          const isHealing = healing?.hero === target.name;
          return (
            <div key={target.name} className="relative ember-panel p-3 flex items-center gap-3">
              <div className={`relative size-16 shrink-0 rounded-full ${isHealing ? "ring-2 ring-amber-200 shadow-[0_0_24px_rgba(255,211,105,0.95)]" : ""}`}>
                <img src={portrait.src} alt="" style={{ objectPosition: portrait.position }} className={`h-full w-full rounded-full ${portrait.framed ? "object-cover" : "object-contain"}`} />
                {isHealing && <div className="absolute -inset-2 rounded-full bg-[radial-gradient(circle,rgba(255,238,153,0.48),rgba(255,199,61,0.12)_55%,transparent_72%)] animate-pulse pointer-events-none" />}
              </div>
              <div className="min-w-0 flex-1">
                <p className="font-medium">{target.name}</p>
                <p className="text-sm tabular-nums">HP {target.hp} / {target.maxHp}{chosen > 0 && ` → ${previewHp} / ${target.maxHp}`}</p>
                {(target.diseased || target.poisoned) && <p className="text-xs text-danger">{[target.diseased ? "Doente" : "", target.poisoned ? "Envenenado" : ""].filter(Boolean).join(" · ")}</p>}
                {target.hp <= 0 && <p className="text-xs text-muted">Caído · não pode receber cura</p>}
                {isHealing && <p className="text-xs text-amber-200 animate-pulse">Cura Média · +{healing.amount} HP</p>}
              </div>
              <div className="flex flex-col items-center gap-1">
                <p className="text-[0.65rem] uppercase tracking-wider text-muted">Conjurações</p>
                <div className="flex items-center gap-2">
                  <Button size="sm" variant="quiet" className="ember-btn ember-btn-sm" disabled={busy || chosen <= 0} onClick={() => setCasts((prev) => ({ ...prev, [target.name]: chosen - 1 }))}>−</Button>
                  <span className="min-w-5 text-center tabular-nums">{chosen}/4</span>
                  <Button size="sm" variant="quiet" className="ember-btn ember-btn-sm" disabled={busy || chosen >= allowed || chosen >= 4} onClick={() => setCasts((prev) => ({ ...prev, [target.name]: chosen + 1 }))}>+</Button>
                </div>
                <span className="text-xs text-muted tabular-nums">{chosen * HEALER_CAST_PRICE} Gold</span>
              </div>
            </div>
          );
        })}
        <div className="ember-panel p-3 flex flex-col gap-2">
          <p className="text-sm">Prévia da seleção · {selectedPlan.reduce((sum, item) => sum + item.count, 0)} conjurações · <GoldAmount amount={selectedCost} /></p>
          <p className="text-xs text-muted">Após a seleção: {Math.max(0, ember - selectedCost)} Gold</p>
          {note && <p className="text-sm text-amber-200">{note}</p>}
          <div className="flex flex-wrap gap-2">
            <Button className="flex-1 ember-btn ember-btn-primary" disabled={busy || selectedPlan.length === 0 || selectedCost > ember} onClick={() => void treat(selectedPlan, selectedCost)}>Curar seleção · {selectedCost} Gold</Button>
            <Button className="flex-1 ember-btn ember-btn-ghost" variant="quiet" disabled={busy || allPlan.length === 0 || allCost > ember} onClick={() => void treat(allPlan, allCost)}>Curar todos · {allCost} Gold</Button>
          </div>
          <p className="text-xs text-muted">Curar todos: {allPlan.reduce((sum, item) => sum + item.count, 0)} conjurações necessárias · {allCost} Gold</p>
        </div>
      </div>
    </section>
  );
}

function SmithPanel({
  ember,
  muted,
  weapons,
  equipped,
  heroClass,
  save,
  test,
  farmlandsService,
  merchantGear = false,
  merchantBackdrop,
  onMute,
  onBack,
  onBuyWeapon,
  onBuyEquipment,
  onEquipWeapon,
  onEquipItem,
  onUsePotion,
  onDiscardWeapon,
  onDiscardEquipment,
  onDiscardRation,
  onDiscardBagItem,
  onOpenStatus,
  onUpgradeWeapon,
  onSellWeapon,
}: {
  ember: number;
  muted: boolean;
  weapons: Record<string, number>;
  equipped: Record<string, string>;
  heroClass: Record<string, ClassId>;
  save: SaveData;
  test?: boolean;
  farmlandsService?: FarmlandsService;
  merchantGear?: boolean;
  merchantBackdrop?: string;
  onMute: () => void;
  onBack: () => void;
  onBuyWeapon: (hero: string, weaponId: string) => boolean;
  onBuyEquipment: (itemId: string) => boolean;
  onEquipWeapon: (hero: string, weaponId: string) => void;
  onEquipItem?: (hero: string, slot: EquipSlot, itemId: string | null) => void;
  onUsePotion?: (hero: string, kind: PotionId) => void;
  onDiscardWeapon?: (weaponId: string) => void;
  onDiscardEquipment?: (itemId: string) => void;
  onDiscardRation?: () => void;
  onDiscardBagItem?: (hero: string, kind: PotionId | "lockpick") => void;
  onOpenStatus: (hero: string) => void;
  onUpgradeWeapon: (weaponId: string) => boolean;
  onSellWeapon: (weaponId: string) => number | false;
}) {
  const [hero, setHero] = useState<string>("Kael");
  const [note, setNote] = useState<string | null>(null);
  const [invView, setInvView] = useState<"doll" | "pack" | null>(null);
  useEffect(() => {
    if (!note) return;
    const timer = window.setTimeout(() => setNote(null), 2200);
    return () => window.clearTimeout(timer);
  }, [note]);

  const classId = heroClass[hero];
  // Purchases still enter the shared Mochila, but the selected hero is the compatibility
  // filter: Vargan never offers that hero a weapon they cannot wield. This stays on in
  // test mode too — it's how per-class gear gets reviewed, one hero tab at a time.
  // Test mode only drops the OTHER stock-limiting rules below (price floor, half-stock
  // slicing, pouch exclusion) so nothing compatible with the selected hero is hidden.
  const pool = useMemo(
    () =>
      Object.values(WEAPONS)
        .filter((weapon) => merchantGear ? weapon.price >= 320 && weapon.price <= 1300 : test || weapon.price > 0)
        .filter((weapon) => !farmlandsService || isFarmlandsSmithWeapon(weapon.id))
        .filter((weapon) => weapon.usableBy.includes(classId))
        .sort((a, b) => weaponPower(a) - weaponPower(b)),
    [classId, test, merchantGear, farmlandsService],
  );
  const smithEquipment = useMemo(() => {
    if (farmlandsService) return [];
    const bySlot = new Map<string, (typeof EQUIPMENT)[string][]>();
    for (const item of Object.values(EQUIPMENT)) {
      if (merchantGear && ((item.price ?? 0) < 320 || (item.price ?? 0) > 1300)) continue;
      if (!test && (item.price ?? 0) <= 0) continue;
      if (!test && isPouch(item.id)) continue;
      if (item.slot === "ring1" || item.slot === "ring2") continue;
      if (item.usableBy && !item.usableBy.includes(classId)) continue;
      const key = item.slot;
      bySlot.set(key, [...(bySlot.get(key) ?? []), item]);
    }
    return [...bySlot.values()].flatMap((items) => {
      const ranked = items.sort((a, b) => (a.price ?? 0) - (b.price ?? 0));
      return test || merchantGear ? ranked : ranked.slice(0, Math.ceil(ranked.length / 2));
    }).sort((a, b) => (a.price ?? 0) - (b.price ?? 0));
  }, [classId, test, merchantGear, farmlandsService]);
  const smithRings = useMemo(
    () =>
      Object.values(EQUIPMENT)
        .filter(() => !farmlandsService)
        .filter((item) => item.slot === "ring1" || item.slot === "ring2")
        .filter((item) => merchantGear ? (item.price ?? 0) >= 320 && (item.price ?? 0) <= 1300 : test || (item.price ?? 0) > 0)
        .filter((item) => !item.usableBy || item.usableBy.includes(classId))
        .sort((a, b) => (a.price ?? 0) - (b.price ?? 0)),
    [classId, test, merchantGear, farmlandsService],
  );
  const equippedId = equipped[hero];
  const equippedWeapon = equippedId ? WEAPONS[equippedId] : null;
  const equippedEnh = equippedId ? (weapons[equippedId] ?? 0) : 0;
  const owned = pool.filter((w) => weapons[w.id] != null && w.id !== equippedId);
  const notOwned = pool.filter((w) => weapons[w.id] == null);

  const buy = (weaponId: string) => {
    setNote(null);
    if (!partyBagHasRoom(save, 1, test)) {
      setNote("Mochila cheia.");
      return;
    }
    if (!onBuyWeapon(hero, weaponId)) {
      setNote(farmlandsService ? `${farmlandsService.npcName}: falta Gold.` : merchantGear ? "Elias recusou. Falta Gold." : "Vargan recusou. Falta Gold.");
      return;
    }
    sfxPlay.purchase();
    setNote(`${WEAPONS[weaponId]!.name} comprada. Equipe no saco quando quiser.`);
  };
  const buyEquipment = (itemId: string) => {
    setNote(null);
    if (!partyBagHasRoom(save, 1, test)) {
      setNote("Mochila cheia.");
      return;
    }
    if (!onBuyEquipment(itemId)) {
      setNote(farmlandsService ? `${farmlandsService.npcName}: falta Gold.` : merchantGear ? "Elias recusou. Falta Gold." : "Vargan recusou. Falta Gold.");
      return;
    }
    sfxPlay.purchase();
    setNote(`${EQUIPMENT[itemId]!.name} foi colocada na Mochila.`);
  };

  const equip = (weaponId: string) => {
    setNote(null);
    onEquipWeapon(hero, weaponId);
  };

  const upgrade = () => {
    if (!equippedId) return;
    setNote(null);
    if (!onUpgradeWeapon(equippedId)) {
      setNote(farmlandsService ? `${farmlandsService.npcName}: falta Gold ou já está no máximo.` : merchantGear ? "Elias recusou. Falta Gold ou já está no máximo." : "Vargan recusou. Falta Gold ou já está no máximo.");
      return;
    }
    setNote(`${equippedWeapon?.name} aprimorada.`);
  };

  const sell = (weaponId: string) => {
    setNote(null);
    const value = onSellWeapon(weaponId);
    if (value === false) return;
    setNote(`${WEAPONS[weaponId]!.name} vendida por ${value} Gold.`);
  };

  const nextEnhCost = equippedEnh < WEAPON_MAX_ENH ? weaponEnhCost(equippedEnh + 1) : null;
  const bagFull = !partyBagHasRoom(save, 1, test);

  return (
    <section className="shop-surface relative h-dvh min-h-0 flex flex-col overflow-hidden bg-bg">
      <img src={farmlandsService?.backdrop ?? (merchantGear ? merchantBackdrop ?? "/game/assets/merchant-snow-market-background-001.jpg" : "/game/ui/smith-background.jpg")} alt="" className="absolute inset-0 h-full w-full object-cover object-left" />
      {merchantGear && <img src="/game/decorations/merchant-covered-cart-001.png" alt="" className="pointer-events-none absolute z-[1] bottom-0 left-0 w-[min(72vw,1020px)] max-h-[105vh] object-contain object-bottom drop-shadow-2xl" />}
      <header className="relative z-10 flex items-center gap-3 px-4 pt-[max(1rem,env(safe-area-inset-top))] pb-3">
        {/* Back always sits at the far left, across every screen, so it never gets lost. */}
        <button type="button" onClick={onBack} className="h-10 px-3 rounded-md ember-chip text-xs uppercase tracking-[0.14em]">
          <ChevronLeft className="size-4 inline -mt-0.5" /> Voltar
        </button>
        <div className="min-w-0 flex-1">
          <p className="text-xs ember-kicker">{farmlandsService ? "Armas · 180–1200 Gold" : merchantGear ? "Equipamentos de viagem · 320–1300 Gold" : "A forja no porão"}</p>
          <h1 className="font-display text-2xl leading-none ember-title">{farmlandsService?.title ?? (merchantGear ? "Equipamento para os Picos" : "Vargan, o Ferreiro")}</h1>
        </div>
        <button
          type="button"
          onClick={() => setInvView("pack")}
          className="h-12 px-3 rounded-md ember-chip text-xs uppercase tracking-[0.14em] flex items-center gap-2"
        >
          <img src={BAG_ICON} alt="" className="size-8 shrink-0 rounded-sm object-contain" />
          Mochila
        </button>
        <button
          type="button"
          onClick={() => setInvView("doll")}
          className="h-12 px-3 rounded-md ember-chip text-xs uppercase tracking-[0.14em]"
        >
          Equipar
        </button>
        <p className="text-sm ember-chip rounded-md px-2 py-1"><GoldAmount amount={ember} /></p>
        <button type="button" onClick={onMute} className="size-10 grid place-items-center rounded-md ember-chip" aria-label="Som">
          {muted ? <VolumeX className="size-4" /> : <Volume2 className="size-4" />}
        </button>
      </header>
      {note && (
        <div className="pointer-events-none absolute left-1/2 top-20 z-30 -translate-x-1/2 ember-plate is-accent px-4 py-2 text-sm" role="status" aria-live="polite">
          {note}
        </div>
      )}
      <div className="relative z-10 flex-1 min-h-0 overflow-y-auto p-4 flex flex-col gap-3 max-w-lg ml-auto w-full">
        <div className="shop-panel relative ember-panel p-3">
          <p className="text-sm leading-relaxed text-fg/90">
            {farmlandsService?.talk ?? "“Aço, sangue, alma — tudo é forjado.” Ele não fala mais que isso. Aponta pra bigorna e espera você escolher."}
          </p>
        </div>
        <div className="shop-panel relative ember-panel p-3 flex flex-col gap-2">
          <p className="text-xs ember-kicker">Equipamento exibido</p>
          <div className="flex flex-wrap gap-1">
            {ALL_HERO_NAMES
              .filter((name) => test || heroRecruited(name, save.completed, save.flags))
              .map((name) => (
              <Button
                key={name}
                className={`shop-hero-selector ember-btn ember-btn-sm ${hero === name ? "ember-btn-primary" : "ember-btn-ghost"}`}
                size="sm"
                variant={hero === name ? undefined : "quiet"}
                onClick={() => {
                  setHero(name);
                  setNote(null);
                }}
              >
                {name}
              </Button>
            ))}
          </div>

          <p className="text-xs ember-kicker mt-2">Equipada</p>
          {equippedWeapon ? (
            <ItemTip text={weaponTooltip(equippedWeapon, equippedEnh)} className="block">
              <div className="flex items-center gap-2 rounded-md border border-accent px-2 py-1.5">
                <span className="grid size-20 shrink-0 place-items-center rounded-sm bg-black">
                  <img src={weaponIcon(equippedWeapon.id)} alt="" className="size-full object-contain" />
                </span>
                <span className="flex-1 text-sm min-w-0">
                  {equippedWeapon.name} {equippedEnh > 0 ? `+${equippedEnh}` : ""}
                  <span className="block text-[11px] text-muted tabular-nums">
                    {weaponDiceLabel(equippedWeapon.id)} {equippedEnh > 0 ? `+ ${equippedEnh} aprimoro` : ""} · {weaponRangeLabel(equippedWeapon.id)}
                  </span>
                  <span className="block text-[10px] uppercase tracking-wide text-muted">Mão principal</span>
                </span>
                <div className="flex flex-col gap-1">
                  <Button size="sm" className="ember-btn ember-btn-sm ember-btn-primary" disabled={nextEnhCost == null || ember < nextEnhCost} onClick={upgrade}>
                    {nextEnhCost == null ? "Máx." : `+1 · ${nextEnhCost} Gold`}
                  </Button>
                  <Button size="sm" variant="quiet" className="ember-btn ember-btn-sm ember-btn-ghost" onClick={() => onEquipWeapon(hero, "")}>
                    Desequipar
                  </Button>
                  <Button size="sm" variant="quiet" className="ember-btn ember-btn-sm ember-btn-ghost" onClick={() => sell(equippedWeapon.id)}>
                    Vender · {weaponSellValue(equippedWeapon.id, equippedEnh)} Gold
                  </Button>
                </div>
              </div>
            </ItemTip>
          ) : (
            <p className="text-sm text-muted">Nenhuma arma equipada ainda.</p>
          )}

          {owned.length > 0 && (
            <>
              <p className="text-xs ember-kicker mt-2">No saco</p>
              <div className="flex flex-col gap-1">
                {owned.map((w) => (
                  <ItemTip key={w.id} text={weaponTooltip(w, weapons[w.id] ?? 0)} className="block">
                    <div className="flex items-center gap-2 rounded-md border border-border px-2 py-1.5">
                      <span className="grid size-16 shrink-0 place-items-center rounded-sm bg-black">
                        <img src={weaponIcon(w.id)} alt="" className="size-full object-contain" />
                      </span>
                      <span className="flex-1 text-sm min-w-0">
                        {w.name}
                        <span className="block text-[11px] text-muted tabular-nums">
                          {weaponDiceLabel(w.id)} · {weaponRangeLabel(w.id)}
                        </span>
                        <span className="block text-[10px] uppercase tracking-wide text-muted">Mão principal</span>
                        {w.bonusClass && isPlayableClassForDisplay(w.bonusClass) && (
                          <span
                            className={`block text-[11px] tabular-nums ${w.bonusClass === classId ? "text-accent" : "text-muted"}`}
                          >
                            +10% dano · {CLASSES[w.bonusClass].name}
                          </span>
                        )}
                      </span>
                      <Button size="sm" variant="quiet" className="ember-btn ember-btn-sm ember-btn-ghost" onClick={() => equip(w.id)}>
                        Equipar
                      </Button>
                      <Button size="sm" variant="quiet" className="ember-btn ember-btn-sm ember-btn-ghost" onClick={() => sell(w.id)}>
                        Vender · {weaponSellValue(w.id, weapons[w.id] ?? 0)} Gold
                      </Button>
                    </div>
                  </ItemTip>
                ))}
              </div>
            </>
          )}

          {notOwned.length > 0 && (
            <>
              <p className="text-xs ember-kicker mt-2">Na bancada</p>
              <div className="flex flex-col gap-1">
                {notOwned.map((w) => (
                  <ItemTip key={w.id} text={weaponTooltip(w)} className="block">
                    <div className="flex items-center gap-2 rounded-md border border-border px-2 py-1.5">
                      <span className="grid size-16 shrink-0 place-items-center rounded-sm bg-black">
                        <img src={weaponIcon(w.id)} alt="" className="size-full object-contain" />
                      </span>
                      <span className="flex-1 text-sm min-w-0">
                        {w.name}
                        <span className="block text-[11px] text-muted tabular-nums">
                          {w.recommendedLevel != null && `Nv. ${w.recommendedLevel} · `}{weaponDiceLabel(w.id)} · {weaponRangeLabel(w.id)} · {w.price} Gold
                        </span>
                        {w.magic && <span className="block text-[11px] text-muted">{weaponMagicSummary(w)}</span>}
                        <span className="block text-[10px] uppercase tracking-wide text-muted">Mão principal</span>
                        {w.bonusClass && isPlayableClassForDisplay(w.bonusClass) && (
                          <span
                            className={`block text-[11px] tabular-nums ${w.bonusClass === classId ? "text-accent" : "text-muted"}`}
                          >
                            +10% dano · {CLASSES[w.bonusClass].name}
                          </span>
                        )}
                      </span>
                      <Button size="sm" className="ember-btn ember-btn-sm ember-btn-primary" disabled={bagFull || ember < w.price} onClick={() => buy(w.id)}>
                        Comprar
                      </Button>
                    </div>
                  </ItemTip>
                ))}
              </div>
            </>
          )}
          <p className="text-xs ember-kicker mt-2">Anéis</p>
          <p className="text-[11px] text-muted">Cabem nos dois dedos da paper doll. Compra dois se quiser testar os dois espaços.</p>
          <div className="grid grid-cols-2 gap-1.5">
            {smithRings.map((item) => (
              <ItemTip key={item.id} text={equipmentTooltip(item)} className="block">
                <div className="flex h-full items-center gap-2 rounded-md border border-border px-2 py-1.5">
                  <span className="grid size-16 shrink-0 place-items-center rounded-sm bg-black">
                    <img src={equipmentIcon(item.id)} alt="" className="size-full object-contain" />
                  </span>
                  <span className="min-w-0 flex-1 text-xs">
                    <span className="block truncate">{item.name}</span>
                    <span className="block text-[10px] text-muted">{equipmentTypeSlotName(item)} · {item.price ?? 0} Gold</span>
                  </span>
                  <Button size="sm" className="ember-btn ember-btn-sm ember-btn-primary" disabled={bagFull || ember < (item.price ?? 0)} onClick={() => buyEquipment(item.id)}>Comprar</Button>
                </div>
              </ItemTip>
            ))}
          </div>
          <p className="text-xs ember-kicker mt-2">Armaduras e acessórios</p>
          <p className="text-[11px] text-muted">Estoque básico da estalagem. As peças superiores pertencem ao ferreiro da cidade.</p>
          <div className="grid grid-cols-2 gap-1.5">
            {smithEquipment.map((item) => (
              <ItemTip key={item.id} text={equipmentTooltip(item)} className="block">
                <div className="flex h-full items-center gap-2 rounded-md border border-border px-2 py-1.5">
                  <span className="grid size-16 shrink-0 place-items-center rounded-sm bg-black">
                    <img src={equipmentIcon(item.id)} alt="" className="size-full object-contain" />
                  </span>
                  <span className="min-w-0 flex-1 text-xs">
                    <span className="block truncate">{item.name}</span>
                    <span className="block text-[10px] text-muted">{equipmentTypeSlotName(item)} · {item.price ?? 0} Gold</span>
                  </span>
                  <Button size="sm" className="ember-btn ember-btn-sm ember-btn-primary" disabled={bagFull || ember < (item.price ?? 0)} onClick={() => buyEquipment(item.id)}>Comprar</Button>
                </div>
              </ItemTip>
            ))}
          </div>
          {note && <p className="text-sm text-accent">{note}</p>}
        </div>
      </div>
      {invView && (
        <PartyInventoryOverlay
          heroName={hero}
          classId={heroClass[hero] ?? "swordsman"}
          save={save}
          test={test}
          onOpenStatus={(h) => {
            // Same reasoning as the NPC panel's overlay: close this one first so the status
            // sheet (same z-40 layer, mounted earlier in the tree) isn't hidden behind it.
            setInvView(null);
            onOpenStatus(h);
          }}
          onClose={() => setInvView(null)}
          onEquipWeapon={onEquipWeapon}
          onEquipItem={onEquipItem}
          onUsePotion={onUsePotion}
          onDiscardWeapon={onDiscardWeapon}
          onDiscardEquipment={onDiscardEquipment}
          onDiscardRation={onDiscardRation}
          onDiscardBagItem={onDiscardBagItem}
          initialView={invView === "pack" ? "backpack" : "equipment"}
        />
      )}
    </section>
  );
}

function SmithIntroScreen({ onSkip }: { muted: boolean; onMute: () => void; onSkip: () => void }) {
  const ref = useRef<HTMLVideoElement>(null);
  // Every cutscene starts with sound on, whatever the game's mute toggle says; the button
  // here only affects this video and never carries over to the rest of the game.
  const [muted, setLocalMuted] = useState(false);
  const prefs = useGamePreferences();
  const subtitleTracks = CUTSCENE_SUBTITLES["/game/smith-intro.mp4"];
  const selectedSubtitle = prefs.subtitles ? subtitleTracks?.[prefs.subtitleLanguage] : undefined;
  useEffect(() => {
    syncSubtitles(ref.current?.textTracks, prefs.subtitles, prefs.subtitleLanguage);
  }, [prefs.subtitles, prefs.subtitleLanguage, selectedSubtitle]);
  const [portrait, setPortrait] = useState(
    () => typeof window !== "undefined" && window.matchMedia("(max-width: 720px) and (orientation: portrait)").matches,
  );
  useEffect(() => {
    const mq = window.matchMedia("(max-width: 720px) and (orientation: portrait)");
    const sync = () => setPortrait(mq.matches);
    sync();
    mq.addEventListener("change", sync);
    return () => mq.removeEventListener("change", sync);
  }, []);
  useEffect(() => {
    const el = ref.current;
    if (!el) return;
    el.muted = false;
    const syncSound = () => setLocalMuted(el.muted);
    el.addEventListener("volumechange", syncSound);
    const kick = () => {
      void el.play().catch(() => {
        el.muted = true;
        void el.play().catch(() => {});
      });
    };
    kick();
    el.addEventListener("canplay", kick);
    const t = window.setTimeout(onSkip, 22000);
    return () => {
      el.removeEventListener("canplay", kick);
      el.removeEventListener("volumechange", syncSound);
      window.clearTimeout(t);
    };
  }, [onSkip]);
  return (
    <section className="relative h-dvh w-dvw bg-black overflow-hidden">
      <div className="cutscene-stage">
        <video ref={ref} src="/game/smith-intro.mp4" playsInline autoPlay preload="auto" onEnded={onSkip} onError={onSkip}>
          {selectedSubtitle && <track key={selectedSubtitle} kind="subtitles" src={selectedSubtitle} srcLang={prefs.subtitleLanguage === "pt" ? "pt-BR" : "en"} label={prefs.subtitleLanguage === "pt" ? "Português (Brasil)" : "English"} default onLoad={() => syncSubtitles(ref.current?.textTracks, prefs.subtitles, prefs.subtitleLanguage)} />}
        </video>
      </div>
      {portrait && (
        <p className="pointer-events-none absolute inset-x-0 top-[max(0.75rem,env(safe-area-inset-top))] text-center text-[11px] tracking-[0.16em] uppercase text-muted">
          Deite o telefone
        </p>
      )}
      <div className="absolute right-3 top-[max(0.75rem,env(safe-area-inset-top))] z-20 p-2">
        <button
          type="button"
          className="grid size-9 place-items-center rounded bg-black/40 text-white/90"
          aria-label={muted ? "Ativar som" : "Silenciar"}
          aria-pressed={!muted}
          onClick={() => {
            const video = ref.current;
            if (!video) return;
            video.muted = !video.muted;
            setLocalMuted(video.muted);
            if (!video.muted) void video.play().catch(() => {});
          }}
        >
          {muted ? <VolumeX className="size-4" /> : <Volume2 className="size-4" />}
        </button>
      </div>
      <div className="absolute inset-x-0 bottom-0 z-10 p-5 pb-[max(1.25rem,env(safe-area-inset-bottom))] flex justify-end">
        <Button size="md" variant="ghost" onClick={onSkip}>
          Pular
        </Button>
      </div>
    </section>
  );
}
