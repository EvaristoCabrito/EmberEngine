import assert from "node:assert/strict";
import { test } from "node:test";
import { advanceProgression, applyEffects, currentChapter, evaluate, missionAccess, type MissionGate, type ProgressSave, type Trigger } from "./progression.ts";

const base = (over: Partial<ProgressSave> = {}): ProgressSave => ({ completed: [], ...over });

test("ALL / ANY / NOT combine leaf conditions", () => {
  const save = base({ completed: ["vau"], flags: ["a"] });
  assert.equal(evaluate({ all: [{ missionCompleted: "vau" }, { flagSet: "a" }] }, save), true);
  assert.equal(evaluate({ all: [{ missionCompleted: "vau" }, { flagSet: "b" }] }, save), false);
  assert.equal(evaluate({ any: [{ flagSet: "b" }, { flagSet: "a" }] }, save), true);
  assert.equal(evaluate({ not: { flagSet: "b" } }, save), true);
  assert.equal(evaluate(null, save), true, "no condition means always true");
});

test("quest stages: 0 not taken, 1 accepted, 2 ready, 3 done", () => {
  const id = "mudo-medalhoes";
  assert.equal(evaluate({ questActive: id }, base()), false);
  const accepted = base({ questsActive: [id] });
  assert.equal(evaluate({ questActive: id }, accepted), true);
  assert.equal(evaluate({ questStage: [id, 2] }, accepted), false);
  const ready = base({ questsActive: [id], questItems: ["mudo-medalhoes:medalhao-1", "mudo-medalhoes:medalhao-2", "mudo-medalhoes:medalhao-3"] });
  assert.equal(evaluate({ questStage: [id, 2] }, ready), true);
  assert.equal(evaluate({ questCompleted: id }, ready), false);
  const done = base({ questsDone: [id] });
  assert.equal(evaluate({ questCompleted: id }, done), true);
  assert.equal(evaluate({ questDiscovered: id }, done), true);
  assert.equal(evaluate({ questDiscovered: id }, base({ questsDiscovered: [id] })), true);
  assert.equal(evaluate({ questDiscovered: "no-such-quest" }, base()), false);
});

test("items, party members and talked-to NPCs come from the save/extras", () => {
  const save = base({ npcTalked: ["brue"] });
  assert.equal(evaluate({ npcTalkedTo: "brue" }, save), true);
  assert.equal(evaluate({ npcTalkedTo: "mudo" }, save), false);
  assert.equal(evaluate({ itemOwned: "adaga" }, save, { items: ["adaga"] }), true);
  assert.equal(evaluate({ itemOwned: "adaga" }, save), false);
  assert.equal(evaluate({ partyMemberPresent: "Neera" }, save, { party: ["Kael", "Neera"] }), true);
});

test("a mission with no gate is unaffected; a gated one is hidden, revealed, then available", () => {
  const gates: Record<string, MissionGate> = {
    "wisp-03": { visible: { questDiscovered: "mudo-medalhoes" }, unlock: { questStage: ["mudo-medalhoes", 1] } },
  };
  assert.equal(missionAccess("vau", base(), {}, false, gates), "available");
  assert.equal(missionAccess("wisp-03", base(), {}, false, gates), "hidden");
  assert.equal(missionAccess("wisp-03", base({ questsDiscovered: ["mudo-medalhoes"] }), {}, false, gates), "revealed");
  assert.equal(missionAccess("wisp-03", base({ questsActive: ["mudo-medalhoes"] }), {}, false, gates), "available");
  assert.equal(missionAccess("wisp-03", base({ completed: ["wisp-03"] }), {}, false, gates), "completed");
  assert.equal(missionAccess("wisp-03", base(), {}, true, gates), "available", "debug mode opens everything");
});

test("a mission with only an unlock rule is visible (revealed) until unlocked", () => {
  const gates: Record<string, MissionGate> = { m: { unlock: { chapterReached: 2 } } };
  assert.equal(missionAccess("m", base(), {}, false, gates), "revealed");
  assert.equal(missionAccess("m", base({ chapter: 2 }), {}, false, gates), "available");
});

test("Cemetery and Watchtower campaigns stay visible and locked until their quests are accepted", () => {
  const cases = [
    ["cemiterio-esquecidos", "brue-birolho"],
    ["watchtower-gate-floor", "suspeito-watchtower-captive"],
  ] as const;
  for (const [missionId, questId] of cases) {
    assert.equal(missionAccess(missionId, base()), "revealed", `${missionId} is visible but locked from the start`);
    assert.equal(
      missionAccess(missionId, base({ questsDiscovered: [questId] })),
      "revealed",
      `${missionId} is visible but locked after the quest is offered`,
    );
    assert.equal(
      missionAccess(missionId, base({ questsActive: [questId] })),
      "available",
      `${missionId} opens when its quest is accepted`,
    );
  }
});

test("chapters: explicit, and via effects; never goes backwards", () => {
  assert.equal(currentChapter(base()), 1);
  assert.equal(currentChapter(base({ chapter: 3 })), 3);
  assert.equal(applyEffects(base({ chapter: 3 }), [{ setChapter: 2 }]).chapter, 3);
  assert.equal(applyEffects(base(), [{ setChapter: 2 }]).chapter, 2);
});

test("triggers fire effects, are idempotent, and can chain", () => {
  const triggers: Trigger[] = [
    { when: { missionCompleted: "thebridge" }, do: [{ setFlag: "left-bridge" }] },
    { when: { flagSet: "left-bridge" }, do: [{ setChapter: 2 }] },
    { when: { chapterReached: 2 }, do: [{ discoverQuest: "brue-birolho" }] },
  ];
  const before = base({ completed: ["thebridge"] });
  const after = advanceProgression(before, {}, triggers);
  assert.deepEqual(after.flags, ["left-bridge"]);
  assert.equal(after.chapter, 2);
  assert.deepEqual(after.questsDiscovered, ["brue-birolho"]);
  assert.equal(advanceProgression(after, {}, triggers), after, "a settled save comes back unchanged");
  assert.equal(advanceProgression(base(), {}, triggers).chapter, undefined, "nothing fires without its condition");
});

test("quest onComplete effects run once the quest is done", () => {
  // No shipped quest declares effects yet; a finished quest with none must leave the save alone.
  const done = base({ questsDone: ["mudo-medalhoes"] });
  assert.equal(advanceProgression(done, {}, []), done);
});
