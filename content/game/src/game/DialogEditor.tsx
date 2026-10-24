import { useState } from "react";
import { AFFINITY_HEROES } from "./affinity";
import { X } from "lucide-react";
import { Button } from "@/components/ui/button";
import type { DialogLine, DialogReply, DialogTree, SpriteId } from "./types";

function randomId(prefix: string): string {
  return `${prefix}-${Math.random().toString(36).slice(2, 9)}`;
}

function emptyLine(): DialogLine {
  return { id: randomId("line"), speaker: "", text: "" };
}

/** The Map Editor's authoring UI for one DialogTree — mounted as a modal from
 * MapEditorScreen for all three attachment points (mission intro, mission outro, one per
 * neutral NPC spawn). Changes stay in a local draft until Save is pressed. */
export function DialogEditor({
  title,
  tree: initialTree,
  onChange: commit,
  onClose,
  portraitOptions,
}: {
  title: string;
  tree: DialogTree | undefined;
  onChange: (tree: DialogTree | undefined) => void | boolean | Promise<void | boolean>;
  onClose: () => void;
  /** Sprite options for the portrait picker — supplied by the caller so this file doesn't
   * need its own opinion on how sprites are enumerated/labeled. */
  portraitOptions: { id: SpriteId; label: string }[];
}) {
  const [tree, onChange] = useState(initialTree);
  const [saving, setSaving] = useState(false);
  const [saveError, setSaveError] = useState(false);
  const [armedDelete, setArmedDelete] = useState("");

  const updateLine = (id: string, patch: Partial<DialogLine>) => {
    if (!tree) return;
    onChange({ ...tree, lines: tree.lines.map((l) => (l.id === id ? { ...l, ...patch } : l)) });
  };
  const removeLine = (id: string) => {
    if (!tree) return;
    const lines = tree.lines.filter((l) => l.id !== id);
    if (lines.length === 0) {
      onChange(undefined);
      return;
    }
    onChange({ ...tree, startId: tree.startId === id ? lines[0]!.id : tree.startId, lines });
  };
  const addLine = () => {
    const line = emptyLine();
    if (!tree) {
      onChange({ id: randomId("tree"), startId: line.id, lines: [line] });
      return;
    }
    // Chains onto whatever line was added last, same as a player would expect a plain
    // sequence of lines to just play in order — only when that line is still a dead end
    // (no next, no branching replies already set on it), so this never overwrites a
    // deliberate branch or a "leads to" the author already chose.
    const last = tree.lines[tree.lines.length - 1];
    const lines =
      last && !last.next && !last.replies ? [...tree.lines.slice(0, -1), { ...last, next: line.id }, line] : [...tree.lines, line];
    onChange({ ...tree, lines });
  };

  return (
    <div
      className="absolute inset-0 z-50 bg-bg/85 flex items-center justify-center p-4"
      onClick={(e) => {
        if (e.target === e.currentTarget) onClose();
      }}
    >
      <div className="w-full max-w-2xl max-h-[88dvh] overflow-y-auto bg-surface border border-border rounded-xl p-5">
        <div className="flex items-center justify-between mb-4">
          <p className="font-display text-xl leading-none">{title}</p>
          <div className="flex items-center gap-2">
            <Button size="sm" disabled={saving} onClick={async () => { setSaving(true); try { if (await commit(tree) !== false) onClose(); else setSaveError(true); } catch { setSaveError(true); } finally { setSaving(false); } }}>{saving ? "Salvando…" : "Salvar"}</Button>
            {tree && (
              <button type="button" onClick={() => onChange(undefined)} className="text-xs text-danger px-2 py-1.5 rounded-md border border-border">
                Apagar diálogo
              </button>
            )}
            <button type="button" onClick={onClose} className="size-8 grid place-items-center rounded-md border border-border" aria-label="Fechar sem salvar">
              <X className="size-4" />
            </button>
          </div>
        </div>

        <p className="text-xs text-muted mb-3">As alterações só são gravadas ao clicar em Salvar. Afinidade por resposta: + = 3 pontos, 0 = neutro, − = perde 3 pontos.</p>
        {saveError && <p role="alert" className="text-sm text-danger mb-3">Não foi possível salvar. Suas alterações continuam aqui; tente novamente.</p>}
        {!tree ? (
          <>
            <p className="text-sm text-muted mb-4">Nenhum diálogo ainda.</p>
            <Button className="w-full" onClick={addLine}>
              Criar diálogo
            </Button>
          </>
        ) : (
          <>
            <label className="flex flex-col gap-1 mb-4 text-xs">
              <span className="uppercase tracking-wide text-muted">Linha inicial</span>
              <select
                className="bg-bg border border-border rounded-md px-2 py-1.5"
                value={tree.startId}
                onChange={(e) => onChange({ ...tree, startId: e.target.value })}
              >
                {tree.lines.map((l) => (
                  <option key={l.id} value={l.id}>
                    {l.speaker || "(sem nome)"} — {l.text.slice(0, 30) || "(vazio)"}
                  </option>
                ))}
              </select>
            </label>

            <div className="flex flex-col gap-3">
              {tree.lines.map((line) => (
                <DialogLineRow
                  key={line.id}
                  line={line}
                  allLines={tree.lines}
                  portraitOptions={portraitOptions}
                  onUpdate={(patch) => updateLine(line.id, patch)}
                  onRemove={() => removeLine(line.id)}
                  armedDelete={armedDelete}
                  setArmedDelete={setArmedDelete}
                />
              ))}
            </div>

            <Button variant="quiet" className="w-full mt-3" onClick={addLine}>
              + Adicionar linha
            </Button>
          </>
        )}
      </div>
    </div>
  );
}

function DialogLineRow({
  line,
  allLines,
  portraitOptions,
  onUpdate,
  onRemove,
  armedDelete,
  setArmedDelete,
}: {
  line: DialogLine;
  allLines: DialogLine[];
  portraitOptions: { id: SpriteId; label: string }[];
  onUpdate: (patch: Partial<DialogLine>) => void;
  onRemove: () => void;
  armedDelete: string;
  setArmedDelete: (key: string) => void;
}) {
  const branching = !!line.replies;
  const deleteKey = `line:${line.id}`;
  const armed = armedDelete === deleteKey;
  return (
    <div className="rounded-lg border border-border bg-bg/50 p-3 flex flex-col gap-2">
      <div className="flex items-center gap-1.5">
        <input
          className="flex-1 min-w-0 bg-bg border border-border rounded-md px-1.5 py-1 text-sm"
          placeholder="Quem fala"
          value={line.speaker}
          onChange={(e) => onUpdate({ speaker: e.target.value })}
        />
        <select
          className="bg-bg border border-border rounded-md px-1.5 py-1 text-xs"
          value={line.portrait ?? ""}
          onChange={(e) => onUpdate({ portrait: (e.target.value || undefined) as SpriteId | undefined })}
        >
          <option value="">Sem retrato</option>
          {portraitOptions.map((p) => (
            <option key={p.id} value={p.id}>
              {p.label}
            </option>
          ))}
        </select>
        <button
          type="button"
          onClick={() => {
            if (armed) {
              onRemove();
              setArmedDelete("");
            } else {
              setArmedDelete(deleteKey);
            }
          }}
          className={`px-1.5 text-xs shrink-0 ${armed ? "text-danger font-bold" : "text-danger"}`}
        >
          {armed ? "Apagar?" : <X className="size-3.5" />}
        </button>
      </div>
      <textarea
        className="w-full bg-bg border border-border rounded-md px-2 py-1.5 text-sm min-h-16"
        placeholder="Texto da linha"
        value={line.text}
        onChange={(e) => onUpdate({ text: e.target.value })}
      />
      <label className="flex items-center gap-1.5 text-xs text-muted">
        <input
          type="checkbox"
          checked={branching}
          onChange={(e) => {
            if (e.target.checked) onUpdate({ replies: [{ text: "" }], next: undefined });
            else onUpdate({ replies: undefined });
          }}
        />
        Respostas ramificadas
      </label>
      {branching ? (
        <div className="flex flex-col gap-1.5 pl-3 border-l-2 border-border">
          {(line.replies ?? []).map((reply, i) => (
            <ReplyRow
              key={i}
              reply={reply}
              index={i}
              total={line.replies!.length}
              allLines={allLines}
              onUpdate={(patch) => {
                const replies = [...line.replies!];
                replies[i] = { ...replies[i]!, ...patch };
                onUpdate({ replies });
              }}
              onMove={(dir) => {
                const replies = [...line.replies!];
                const j = i + dir;
                if (j < 0 || j >= replies.length) return;
                [replies[i], replies[j]] = [replies[j]!, replies[i]!];
                onUpdate({ replies });
              }}
              onRemove={() => {
                const replies = line.replies!.filter((_, idx) => idx !== i);
                onUpdate({ replies: replies.length ? replies : [{ text: "" }] });
              }}
            />
          ))}
          <button
            type="button"
            onClick={() => onUpdate({ replies: [...(line.replies ?? []), { text: "" }] })}
            className="text-xs text-muted self-start px-1"
          >
            + resposta
          </button>
        </div>
      ) : (
        <label className="flex flex-col gap-1 text-xs">
          <span className="uppercase tracking-wide text-muted">Leva a</span>
          <select
            className="bg-bg border border-border rounded-md px-1.5 py-1"
            value={line.next ?? ""}
            onChange={(e) => onUpdate({ next: e.target.value || null })}
          >
            <option value="">(fim do diálogo)</option>
            {allLines
              .filter((l) => l.id !== line.id)
              .map((l) => (
                <option key={l.id} value={l.id}>
                  {l.speaker || "(sem nome)"} — {l.text.slice(0, 30) || "(vazio)"}
                </option>
              ))}
          </select>
        </label>
      )}
    </div>
  );
}

function ReplyRow({
  reply,
  index,
  total,
  allLines,
  onUpdate,
  onMove,
  onRemove,
}: {
  reply: DialogReply;
  index: number;
  total: number;
  allLines: DialogLine[];
  onUpdate: (patch: Partial<DialogReply>) => void;
  onMove: (dir: -1 | 1) => void;
  onRemove: () => void;
}) {
  return (
    <div className="flex flex-wrap items-center gap-1.5 text-xs">
      <input
        className="flex-1 min-w-0 bg-bg border border-border rounded-md px-1.5 py-1"
        placeholder="Texto da resposta"
        value={reply.text}
        onChange={(e) => onUpdate({ text: e.target.value })}
      />
      <select
        className="bg-bg border border-border rounded-md px-1.5 py-1"
        value={reply.next ?? ""}
        onChange={(e) => onUpdate({ next: e.target.value || null })}
      >
        <option value="">(fim)</option>
        {allLines.map((l) => (
          <option key={l.id} value={l.id}>
            {l.speaker || "(sem nome)"} — {l.text.slice(0, 20) || "(vazio)"}
          </option>
        ))}
      </select>
      <select aria-label="Personagem que responde" className="bg-bg border border-border rounded-md px-1 py-1" value={reply.affinity?.from ?? "Kael"} onChange={e => onUpdate({ affinity: { from: e.target.value, to: reply.affinity?.to ?? "Neera", delta: reply.affinity?.delta ?? 0 } })}>
        {AFFINITY_HEROES.map(hero => <option key={hero}>{hero}</option>)}
      </select>
      <select aria-label="Personagem cuja afinidade muda" className="bg-bg border border-border rounded-md px-1 py-1" value={reply.affinity?.to ?? "Neera"} onChange={e => onUpdate({ affinity: { from: reply.affinity?.from ?? "Kael", to: e.target.value, delta: reply.affinity?.delta ?? 0 } })}>
        {AFFINITY_HEROES.map(hero => <option key={hero}>{hero}</option>)}
      </select>
      <select aria-label="Bônus de afinidade da resposta" title="+ dá 3 pontos; − perde 3 pontos" className="bg-bg border border-border rounded-md px-1 py-1" value={reply.affinity?.delta ?? 0} onChange={e => onUpdate({ affinity: { from: reply.affinity?.from ?? "Kael", to: reply.affinity?.to ?? "Neera", delta: Number(e.target.value) as -3 | 0 | 3 } })}>
        <option value={3}>+ (+3)</option><option value={0}>0</option><option value={-3}>− (−3)</option>
      </select>
      <button type="button" disabled={index === 0} onClick={() => onMove(-1)} className="px-1 disabled:opacity-30">
        ↑
      </button>
      <button type="button" disabled={index === total - 1} onClick={() => onMove(1)} className="px-1 disabled:opacity-30">
        ↓
      </button>
      <button type="button" onClick={onRemove} className="text-danger px-1" aria-label="Remover resposta">
        <X className="size-3.5" />
      </button>
    </div>
  );
}
