import { useGamePreferences, translatedText, uiText } from "./gamePreferences";
import { useState } from "react";
import { Button } from "@/components/ui/button";
import { portraitFor } from "./assets";
import type { DialogAction, DialogLine, DialogReply, DialogTree, SpriteId } from "./types";

const speakerKey = (name: string) => name.normalize("NFD").replace(/[\u0300-\u036f]/g, "").toLowerCase().trim();
const speakerPortraits: Record<string, SpriteId> = {
  kael: "kaelFinal", neera: "neera", voss: "voss", salazar: "salazar", aldric: "aldric", malrec: "malrec",
  brue: "brue", "o mudo": "mudinho", "a hospede": "crazyLady", "mercador itinerante": "travelingMerchant",
};

export function dialoguePortraitFor(line: DialogLine, tree: DialogTree, characters: { name: string; sprite: SpriteId }[] = []) {
  const speaker = speakerKey(line.speaker);
  const character = characters.find(c => speakerKey(c.name) === speaker)
    ?? characters.find(c => speakerKey(c.name).split(",")[0] === speaker.split(",")[0]);
  const sameSpeaker = tree.lines.find(l => l.portrait && speakerKey(l.speaker) === speaker);
  const sprite = speakerPortraits[speaker] ?? line.portrait ?? character?.sprite ?? sameSpeaker?.portrait;
  return speaker === "vargan" ? { src: "/game/portraits/vargan.png", framed: true } : sprite ? portraitFor(sprite) : null;
}

/** The Battle Dialog System's runtime popup — shared by the mission intro/outro and every
 * NPC conversation. Always starts at `tree.startId`; a plain line advances via its own
 * `next` (an "Ok" button), a branching line shows one button per reply, each with its own
 * `next`. Either ends the tree (closes the popup) when the line/reply it followed has no
 * `next`. No click-outside-to-dismiss — same as the chest-loot/promotion popups, a
 * conversation only advances when the player deliberately presses a button. */
export function DialogOverlay({ tree, onClose, onAction, onReply, characters = [] }: { tree: DialogTree; onClose: () => void; onAction?: (action: DialogAction) => void; onReply?: (reply: DialogReply) => void; characters?: { name: string; sprite: SpriteId }[] }) {
  const prefs = useGamePreferences();
  const [lineId, setLineId] = useState(tree.startId);
  const line = tree.lines.find((l) => l.id === lineId);
  // Missing line id (a hand-edited/corrupt tree) closes rather than soft-locking the battle.
  if (!line) {
    onClose();
    return null;
  }
  const advance = (next: string | null | undefined) => {
    if (!next) {
      onClose();
      return;
    }
    setLineId(next);
  };
  const portrait = dialoguePortraitFor(line, tree, characters);
  return (
    <div className="absolute inset-0 z-50 ember-veil flex items-end sm:items-center justify-center p-4">
      <div className="relative w-full max-w-xl max-h-[90dvh] overflow-y-auto ember-panel p-5 flex gap-4">
        {portrait && (
          <img
            src={portrait.src}
            alt={line.speaker}
            style={{ objectPosition: portrait.position }}
            className={portrait.framed ? "h-20 w-14 sm:h-24 sm:w-20 object-cover rounded-lg border border-border shrink-0" : "h-16 w-14 sm:h-20 sm:w-20 object-contain shrink-0"}
          />
        )}
        <div className="flex-1 min-w-0" style={{ fontSize: `${prefs.dialogueScale}rem` }}>
          <p className="text-xs ember-kicker">{translatedText(line.speaker, line.speakerTranslations, prefs.dialogueLanguage)}</p>
          <p className="mt-1 text-[1em] leading-relaxed text-fg whitespace-pre-line">{translatedText(line.text, line.translations, prefs.dialogueLanguage)}</p>
          <div className="mt-4 flex flex-col gap-2">
            {line.replies && line.replies.length > 0 ? (
              line.replies.map((reply, i) => (
                <Button
                  key={i}
                  variant="quiet"
                  className="w-full h-auto min-h-11 whitespace-normal py-2 text-left justify-start leading-snug ember-btn ember-btn-sm ember-btn-ghost"
                  onClick={() => {
                    onReply?.(reply);
                    if (reply.action && onAction) {
                      onClose();
                      onAction(reply.action);
                      return;
                    }
                    advance(reply.next);
                  }}
                >
                  {translatedText(reply.text, reply.translations, prefs.dialogueLanguage)}
                </Button>
              ))
            ) : (
              <Button className="w-full ember-btn ember-btn-primary" onClick={() => advance(line.next)}>
                {uiText(line.next ? "Próximo" : "Ok")}
              </Button>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
