import { useEffect, useRef, useState } from "react";
import { AFFINITY_HEROES, affinityScore, type AffinityHero } from "./affinity";
import { buildCompanionDialogue, companionTopics, COMPANION_TIER_NAMES } from "./companionDialogues";
import { DialogOverlay } from "./DialogOverlay";
import { portraitFor } from "./assets";
import type { DialogReply, DialogTree, SaveData, SpriteId } from "./types";

const portraits: Record<AffinityHero, SpriteId> = { Kael: "kaelFinal", Neera: "neera", Voss: "voss", Salazar: "salazar", Aldric: "aldric", Malrec: "malrec" };

export function CompanionConversations({ save, leader, heroes, onLeader, onReply, onClose }: {
  save: SaveData; leader: AffinityHero; heroes: readonly AffinityHero[];
  onLeader: (hero: AffinityHero) => void; onReply: (reply: DialogReply, leader: AffinityHero) => void; onClose: () => void;
}) {
  const [companion, setCompanion] = useState<AffinityHero | null>(heroes.find(h => h !== leader) ?? null);
  const [tree, setTree] = useState<DialogTree | null>(null);
  const modal = useRef<HTMLDivElement>(null);
  useEffect(() => { modal.current?.focus(); }, []);
  const target = companion !== leader && companion && heroes.includes(companion) ? companion : heroes.find(h => h !== leader) ?? null;
  return <div ref={modal} tabIndex={-1} className="fixed inset-0 z-[70] ember-veil flex items-center justify-center p-3 outline-none" role="dialog" aria-modal="true" aria-label="Companion conversations" onKeyDown={event => {
    event.stopPropagation();
    if (event.key === "Escape") { event.preventDefault(); if (tree) setTree(null); else onClose(); }
    if (event.key === "Tab") {
      const controls = Array.from(event.currentTarget.querySelectorAll<HTMLElement>(tree ? ".absolute.inset-0 button:not(:disabled)" : "button:not(:disabled), select"));
      const index = controls.indexOf(document.activeElement as HTMLElement);
      if (controls.length && (index === -1 || (event.shiftKey ? index === 0 : index === controls.length - 1))) {
        event.preventDefault();
        controls[event.shiftKey ? controls.length - 1 : 0].focus();
      }
    }
  }}>
    <section className="ember-panel w-full max-w-2xl max-h-[90dvh] overflow-y-auto p-5">
      <div className="flex justify-between items-start gap-3">
        <div><h2 className="font-display text-xl">Companion conversations</h2><p className="text-sm text-muted">Speak as {leader}. Each companion has a different relationship with every leader.</p></div>
        <button type="button" className="ember-btn ember-btn-sm" onClick={onClose}>Close</button>
      </div>
      <label className="block mt-4 text-sm">Party leader
        <select className="ml-2 bg-bg border border-border rounded p-2" value={leader} onChange={e => { setTree(null); onLeader(e.target.value as AffinityHero); }}>
          {AFFINITY_HEROES.filter(h => heroes.includes(h)).map(h => <option key={h}>{h}</option>)}
        </select>
      </label>
      <div className="flex flex-wrap gap-2 my-4" aria-label="Choose a companion">
        {heroes.filter(h => h !== leader).map(h => <button type="button" key={h} aria-pressed={target === h} className={`ember-btn ember-btn-sm ${target === h ? "ember-btn-primary" : ""}`} onClick={() => setCompanion(h)}>{h}</button>)}
      </div>
      {target ? <>
        <div className="flex gap-3 items-center mb-3">
          <img className="w-14 h-16 rounded object-cover" src={portraitFor(portraits[target]).src} style={{ objectPosition: portraitFor(portraits[target]).position }} alt={target} />
          <div><h3 className="font-display text-lg">{leader} &amp; {target}</h3><p className="text-sm text-muted">Affinity: {affinityScore(save.affinityScores, leader, target).toLocaleString(undefined, { maximumFractionDigits: 1 })}/100</p></div>
        </div>
        <div className="flex flex-col gap-2">
          {companionTopics(leader, target, save.affinityScores, save.companionConversations).map(topic => <button type="button" key={topic.tier} disabled={!topic.unlocked} className="ember-slot p-3 text-left disabled:opacity-50" onClick={() => setTree(buildCompanionDialogue(leader, target, topic.tier, save.affinityScores, save.companionConversations))}>
            <span className="block text-xs text-muted">{COMPANION_TIER_NAMES[topic.tier]} · {topic.threshold} affinity</span>
            <span className="block">{topic.label}</span>
            <span className="block text-xs text-muted">{!topic.unlocked ? "Keep building this relationship to unlock" : topic.resolved ? "Revisit conversation" : "New conversation"}</span>
          </button>)}
        </div>
        <p className="mt-3 text-xs text-muted">You can revisit earlier conversations. Only the first response changes affinity. Leaving a topic for later does not resolve it.</p>
      </> : <p className="text-muted">Conversations become available when another companion joins the party.</p>}
    </section>
    {tree && <DialogOverlay key={tree.id} tree={tree} onClose={() => setTree(null)} onReply={reply => onReply(reply, leader)} />}
  </div>;
}
