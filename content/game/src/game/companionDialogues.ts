import { AFFINITY_HEROES, affinityScore, changeAffinity, type AffinityHero, type AffinityScores } from "./affinity";
import type { DialogLine, DialogReply, DialogTree, SpriteId } from "./types";

export const COMPANION_TIERS = [0, 25, 50, 80, 100] as const;
export const COMPANION_TIER_NAMES = ["First impressions", "Common ground", "Confidences", "A promise", "Chosen family"] as const;
export type ConversationMemory = Record<string, -3 | 0 | 3>;
const portraits: Record<AffinityHero, SpriteId> = { Kael: "kaelFinal", Neera: "neera", Voss: "voss", Salazar: "salazar", Aldric: "aldric", Malrec: "malrec" };

// Each entry is a conversation arc for one pair. The two disclosures are authored
// separately: changing leader changes who speaks, what they reveal and their replies.
type Arc = { heroes: readonly [AffinityHero, AffinityHero]; stages: readonly (readonly [string, string])[] };
export const COMPANION_ARCS: readonly Arc[] = [
  { heroes: ["Kael", "Neera"], stages: [
    ["You count exits before you look at faces, Neera. I count who might need carrying through them. Perhaps we need both habits.", "Kael, when you go first, you hide the road from the rest of us. Leave me a sight line. Being behind you doesn't mean I need sheltering."],
    ["I used to mistake your silence for agreement. Now I listen for the breath before you object. Tell me when I'm taking us too far.", "You actually stopped when I said the path was wrong. Most people with a sword that big make listening look like defeat. I noticed."],
    ["There are nights I keep watch because I don't trust myself asleep. You notice, and you never announce it to everyone. Thank you for that.", "I can judge a moving target without thinking. Asking somebody to stay is harder. With you, I always turn it into advice about the road."],
    ["If I start calling every risk my responsibility, remind me you chose this road too. I want you beside me, not somewhere I can keep safe and forget to hear.", "Let me take the first watch tonight. Not because you ordered it. Because I know the look you get when you're too tired to admit it."],
    ["When I imagine a day without fighting, you are still there. No orders. No debt. Just the two of us deciding where to go.", "I used to choose a camp by its exits. Now I find the place where you'll sit. I still know how to leave, Kael. I just want to stay."],
  ] },
  { heroes: ["Kael", "Voss"], stages: [
    ["I don't need you to make your magic sound harmless, Voss. I need to know where to stand when you use it.", "If you want a warning before a spell, Kael, give me time to speak. Courage doesn't make you immune to being in the way."],
    ["You explain a spell like you're bracing for an accusation. I asked because I want to understand what we're trusting you with.", "You've started asking what a spell costs me. That's a better question than how many enemies it can kill."],
    ["I fear a decision made too quickly more than an enemy I can see. Sometimes I ask you for certainty you couldn't possibly have. That isn't fair.", "I sometimes rehearse a joke while I'm afraid. If it sounds cruel, it may be because I don't know how to say I need help."],
    ["Promise me you'll tell me if the next spell asks too much of you. I won't spend a friend's life to save my pride as a leader.", "If your hand shakes before the next fight, you can show me. I won't turn it into a lesson, or pretend mine never does."],
    ["You're one of the few people who can tell me I'm wrong without making me feel alone. I want that voice in my life after the road ends.", "When there is no enemy left to impress, I'd still like to argue with you over a bad meal. That is my unusually sincere invitation."],
  ] },
  { heroes: ["Kael", "Salazar"], stages: [
    ["You have no potions to spare, Salazar. Tell me what you need before I ask you to mend another wound.", "Kael, I can close a wound. I cannot make your body forget you were hurt. Let that matter when you decide who goes first."],
    ["I keep thanking you when somebody survives. I should also ask how you are when your hands finally stop glowing.", "You waited while I treated someone who couldn't help us. I was prepared to argue for that time. I'm glad I didn't have to."],
    ["Sometimes I count those I brought back and those I didn't, as if the first number could excuse the second. Do you ever stop counting?", "After the ritual, silence didn't feel peaceful. It felt like something waiting to begin again. Your footsteps outside the tent helped."],
    ["You don't owe us endless strength because we found you alive. If you need to stop, I will stand with you when you say it.", "Let me care for you before you collapse, Kael. Accepting it isn't asking me to carry all your pain. It's trusting me with a little."],
    ["You helped me believe survival could be more than another obligation. I hope we learn what living looks like together.", "I no longer listen to your footsteps to make sure the silence is safe. I listen because a friend is coming home."],
  ] },
  { heroes: ["Kael", "Aldric"], stages: [
    ["I don't expect obedience just because I'm in front, Aldric. If you see a flaw in an order, say it while there's time to change it.", "A spear holds a line. It doesn't decide whether the line should be held. I need to know you understand the difference, Kael."],
    ["You check the ground before agreeing to a plan. I used to hear doubt. Now I hear somebody trying to bring everyone back.", "You let me change the formation without turning it into a contest. That's how a group starts feeling like a company worth staying with."],
    ["It's easier to give an order than admit I want someone to tell me it'll work. I won't dress that need up as your duty.", "I polish the spear when I don't know what to say. Straight edges are easier than uncertain loyalties. You're learning to spot the difference."],
    ["If holding the line means abandoning you behind it, we'll find another line. I mean that as a friend, not a commander.", "I will stand beside you when the others question a choice. But I won't call every choice right. You deserve better than that."],
    ["There is room beside me even when you put the spear down. You don't have to earn your place every morning.", "For once, staying feels like a decision I make for myself. If there is a table at the end of this road, keep me a chair."],
  ] },
  { heroes: ["Neera", "Voss"], stages: [
    ["Voss, your sparks make every shadow move. Warn me before you light the whole field. I prefer my targets where I left them.", "Neera, your arrows arrive with less conversation than my spells. I envy the simplicity. I do not envy having to retrieve them."],
    ["I watched you spare a spell when one arrow would do. That's the first time your restraint impressed me more than your magic.", "You don't ask whether I can hit something. You ask whether I can leave a clear lane for you. It's oddly pleasant being trusted precisely."],
    ["I can spend a whole evening saying nothing and still be exhausted by people. You seem to understand when a joke is enough company.", "With you, I don't have to fill every silence. I thought that would feel like being ignored. It feels like being allowed to rest."],
    ["If the jokes stop working, you can sit beside me without them. I won't make you explain yourself before you're ready.", "If you ask for solitude, I won't mistake it for rejection. If you ask for company, I won't make a performance of being wanted."],
    ["I know your laugh before I see you now. Strange thing to use as a landmark. Keep making it; I like knowing where home is.", "I'd like to show you something beautiful with magic, without it needing to be useful. You'd be the first person I wouldn't feel foolish asking."],
  ] },
  { heroes: ["Neera", "Salazar"], stages: [
    ["Salazar, I can cover you while you heal. Tell me which side you need clear. Don't spend breath asking whether I mind.", "You keep your distance in a fight, Neera. I won't drag you into danger to make healing convenient. Just tell me when you are hurt."],
    ["You don't ask me to smile when you check a wound. It sounds like a small kindness until somebody finally offers it.", "You watched the door while I rested and didn't call attention to it. Quiet kindness is still kindness. I wanted you to know I saw it."],
    ["When you're hurting, I always offer to keep watch. Sometimes it's because I don't know what else I can give. Is sitting here enough?", "Some days being touched surprises me, even when I know the hand is kind. You ask first. That gives me something the ritual took away."],
    ["You can say no to helping someone, including me. I'll still be here when you're done being the person everybody needs.", "You don't have to invent a wound to ask me to stay. And I don't have to invent a treatment. We can just keep each other company."],
    ["With you, silence doesn't feel like a hiding place anymore. It feels like a room somebody left open for me.", "When I think about peace, I picture you setting down the bow and still feeling safe. I want to be there when that becomes ordinary."],
  ] },
  { heroes: ["Neera", "Aldric"], stages: [
    ["Aldric, lower the spear when you cross my sight line. I'll warn you before an arrow passes your shoulder. We can start with that.", "You choose where to stand as carefully as I do, Neera. We should compare our ground before we compare our kills."],
    ["You leave an opening for my shot without looking back. I didn't realize how much I wanted someone to remember I was there.", "You cover the gap when I step forward. I used to check every time. Now I can listen for the bowstring and keep my eyes ahead."],
    ["I notice you sit facing the door even when I'm watching it. You can give me that worry for an evening, if you want.", "I find it easier to trust you in battle than ask you to sit with me afterward. That sounds absurd when I say it aloud."],
    ["If you put the spear down, I won't leave you unguarded. But I'd rather we both learned how to rest at the same time.", "Tell me when I'm protecting you instead of listening to you. I don't want a habit that keeps me comfortable to make you smaller."],
    ["I'd like a morning when neither of us checks the door first. Until then, I'm glad we can take turns.", "I used to think companionship meant someone holding the other flank. Now I know it can mean someone beside me when there is no flank to hold."],
  ] },
  { heroes: ["Voss", "Salazar"], stages: [
    ["Salazar, if I ask about your healing, it is curiosity, not a test of your faith. Tell me when curiosity becomes an intrusion.", "Voss, I don't need your magic to resemble mine to respect it. I do need you to respect the person underneath mine."],
    ["You answered my question without trying to convert me. I may have been preparing an argument you never intended to have.", "You asked permission before studying the light. I expected a debate about miracles. You offered me a choice instead."],
    ["Knowledge is a refuge I can usually explain. Fear isn't. When I start lecturing at midnight, you can ask which one I'm actually feeling.", "I worry that if I cannot heal someone, I will have nothing left to offer. You stayed to talk on a day I had no strength for magic."],
    ["I won't explain your suffering back to you. If an answer would help, I'll search. If a friend would help, I'll stay.", "Your doubt doesn't wound me. Your honesty comforts me. Don't pretend certainty because you think it's what I need to hear."],
    ["I don't need to settle every question between us. I'd rather keep the person who makes asking them feel safe.", "We may never name the light the same way. We have learned how to stand in it together. That matters more to me."],
  ] },
  { heroes: ["Voss", "Aldric"], stages: [
    ["Aldric, a spell is not a spear with better lighting. If you want predictable timing, ask me before you build a plan around it.", "Voss, I don't distrust what I can't wield. I distrust a plan that assumes no one needs to understand it."],
    ["You asked for limits rather than guarantees. That is a rarer courtesy than you might think, and a more useful one.", "You explained what could go wrong without hiding it behind cleverness. I can work with risk. I can't work with being kept in the dark."],
    ["I worry you find me unreliable because I make light of things. The joke is often the part of me that hasn't frozen yet.", "I envy the way you change a plan in an instant. Sometimes I keep defending an old decision because admitting fear feels worse."],
    ["If I sound certain when I shouldn't, ask again. I'd rather disappoint your expectations than earn your trust under false terms.", "I'll give you room to improvise. Promise you'll tell me when you need me to stop holding a position and come stand with you."],
    ["I thought we'd spend this whole journey proving the other wrong. Instead you've become the person I want beside me when neither of us knows.", "When you say you have an idea, I don't brace myself anymore. I listen. It took us a long time to make those two things different."],
  ] },
  { heroes: ["Salazar", "Aldric"], stages: [
    ["Aldric, let me see the wound before you call it minor. I'm not questioning your courage. I'm asking you to stop spending it on silence.", "Salazar, tell me where you need space to work. I can hold that space without deciding who deserves your help."],
    ["You move the wounded gently even when you're in a hurry. I trust a person more for that than for a speech about honor.", "You don't treat a scar as a failure. I hadn't realized how often I was measuring myself that way until you stopped me."],
    ["I sometimes feel guilty for being tired of caring. You don't ask me to be grateful every moment. That makes it easier to be honest.", "I mend straps that don't need mending when I feel useless. You let me help without pretending the straps are the important part."],
    ["I can offer care without asking for your obedience. You can accept it without becoming a debt I expect you to repay.", "If you need someone to stand outside the tent, I'll stand there. If you need someone inside, ask. I won't choose for you."],
    ["You have become someone I can ask for help before I find words for the hurt. That is a quiet kind of home.", "I'd like to sit beside you somewhere no one is waiting to be healed. We can find out what we talk about when everyone is safe."],
  ] },
  { heroes: ["Kael", "Malrec"], stages: [
    ["Malrec, I can work with someone who keeps their counsel. I cannot work with someone who leaves the group blind to a danger.", "Kael, you ask where I stand before you ask what I can summon. Sensible. A creature's obedience says little about its caller."],
    ["You warned us when silence would have been easier for you. I won't pretend that answers every question. It answers one that matters.", "You let a warning remain a warning, without demanding a confession to go with it. I can give you more if you don't seize all of it."],
    ["I won't ask you to promise a future here. For the time we share, I'd rather know what you need than keep guessing at what you're hiding.", "I can trust you with a watch, Kael. I am not ready to trust anyone with every reason I stay awake. Don't mistake one for the other."],
  ] },
  { heroes: ["Neera", "Malrec"], stages: [
    ["Your summons change the sound of a camp, Malrec. Tell me which noises belong to you before I put an arrow into one.", "You watch my hands more than my creatures, Neera. Good. The danger worth noticing usually comes before the creature."],
    ["You noticed the hidden path and let me check it myself. I prefer that to being told I ought to trust your instincts.", "You let a silence stand without trying to corner an answer. I find myself saying more when I don't have to defend saying less."],
    ["If you need distance, say so. I know what it costs to have every silence treated as proof against you.", "You know how to leave without making a spectacle. If I tell you I need an evening alone, will you let it mean only that?"],
  ] },
  { heroes: ["Voss", "Malrec"], stages: [
    ["Malrec, I have questions about your summons. I can keep them technical if you can keep the answers honest.", "Curiosity can be an appetite, Voss. Ask one question at a time. Let me decide whether the next answer belongs to you."],
    ["You corrected my theory instead of flattering it. Irritating. Useful. I suppose I should thank you for the second part.", "You admitted a gap in your knowledge without pretending it was a joke. That makes an exchange possible."],
    ["Sometimes I ask about magic because asking about the person feels too dangerous. This time, I'm asking whether you're all right.", "Knowing how a bond works does not tell you why someone needs it. I can explain the first. Leave me some time for the second."],
  ] },
  { heroes: ["Salazar", "Malrec"], stages: [
    ["Malrec, I won't ask you to defend your magic before I treat a wound. I will ask you to tell me what the wound needs.", "Salazar, keep your judgment separate from your hands and we may understand each other. I don't need absolution to accept a bandage."],
    ["You sat through the treatment without making pain into a contest. Thank you. Neither of us has anything to prove that way.", "You asked before touching me. I was prepared for an argument about trust, and you made it unnecessary."],
    ["Care doesn't require a confession. You can tell me only what you want to tell me. I know the difference between a person and a debt.", "I am willing to let you help. That is not the same as asking you to save me. Can we leave the words at that?"],
  ] },
  { heroes: ["Aldric", "Malrec"], stages: [
    ["Malrec, I need to know whether a summon can cross the line I'm holding. A practical question deserves a practical answer.", "Aldric, you trust a formation more readily than a person. At least a formation tells you when it has changed."],
    ["You kept the space we agreed on. I don't demand friendship as proof of reliability. I do notice reliability when it's offered.", "You kept your promise without asking me to admire it. That is easier to respect than the speeches people usually attach."],
    ["You don't owe me a pledge to stay forever. Tell me what we can count on today, and I will answer in kind.", "I can give you my word for the watch ahead. Don't stretch it into an oath for a lifetime. A small honest promise should be enough."],
  ] },
];

const voice: Record<AffinityHero, { topics: readonly string[]; ask: readonly string[]; welcome: string; respect: string; boundary: string }> = {
  Kael: { topics: ["Carrying responsibility", "Learning to listen", "The cost of keeping watch", "Standing beside each other", "A life beyond the road"], ask: ["How do we keep everyone safe?", "Do you trust the way I lead?", "What keeps you awake?", "Can we share the weight?", "Where do you see us when this is over?"], welcome: "Then let me be honest too. I don't need you to carry everything. I need to know we can ask each other for help.", respect: "Fair. We can start with what we can promise today. I'll remember you didn't ask me to be someone else.", boundary: "Don't turn what I told you into another order I have to obey. I can give you trust. You still have to choose what to do with it." },
  Neera: { topics: ["Space to take a shot", "Quiet acts of trust", "Asking someone to stay", "Company without conditions", "A place worth staying"], ask: ["How do you want me to stand beside you?", "What have you noticed about us?", "Would you rather have quiet or company?", "What would help you feel safe with me?", "What makes somewhere feel like home?"], welcome: "You heard what I meant, not just what I managed to say. Stay a while. We don't have to fill the whole silence.", respect: "That I can work with. You leave me room to choose, and I won't make you guess where I stand.", boundary: "I told you something because I chose to. It wasn't permission to decide what I should feel. Give me back that space." },
  Voss: { topics: ["Magic and its limits", "Questions worth asking", "The joke beneath the fear", "Honesty without certainty", "Wonder without a battle"], ask: ["What should I understand about your magic?", "Can I ask what that costs you?", "What is hiding behind the joke?", "What can we promise without pretending?", "What would you make if it didn't have to be useful?"], welcome: "Well. You've made the clever answer unnecessary. The honest one is that I'm glad you're here.", respect: "An answer with limits. I appreciate that more than a grand assurance. We can keep talking without solving everything tonight.", boundary: "You don't get to call it honesty when you're using my words to pin me down. I offered a conversation, not a demonstration." },
  Salazar: { topics: ["Care without a debt", "The person behind the healing", "A silence after the ritual", "Permission to need help", "Peace without being needed"], ask: ["What do you need while you care for us?", "How are you when the healing stops?", "Would you like me to sit with you?", "Can we help without owing each other?", "What would a peaceful day look like?"], welcome: "Thank you for asking rather than deciding. I can say what I need more easily when I know no is an answer you'll accept.", respect: "We don't have to mend every hurt in one evening. A little honest company is something I can accept.", boundary: "My care is not permission to reach into every part of me. Please don't make me defend a boundary just because I trusted you with a hurt." },
  Aldric: { topics: ["Holding the right line", "Reliability without an oath", "The habit of staying guarded", "A place without duty", "Putting down the spear"], ask: ["What do you need to trust a plan?", "Have we learned to rely on each other?", "What happens when there is no line to hold?", "Will you let me stand beside you?", "Who do you want to be when duty is done?"], welcome: "I can accept that. Not as an order, and not as payment. Because you've given me a reason to want to stay.", respect: "A promise we can keep is enough for tonight. I don't need you to decorate it with an oath.", boundary: "I didn't offer you obedience. If my trust only matters when I agree with you, we haven't understood each other." },
  Malrec: { topics: ["Practical limits", "A little earned trust", "A promise for today"], ask: ["What should we know before you summon?", "What can we trust each other with?", "What can we promise for the time we have?"], welcome: "Then we understand the size of the promise. Small does not mean false. I can give you that much.", respect: "Good. Let an answer be enough without prying out the next one. There may be another conversation in that.", boundary: "You were offered a little trust, not ownership of the rest. Press harder and you'll have less than you began with." },
};

// A careful, practical promise lands better with these listeners than a broad
// emotional invitation. This is directed: Aldric speaking to Voss is not Neera
// speaking to Voss, even at the same affinity score.
const practicalTrust = new Set(["Kael:Malrec", "Neera:Malrec", "Voss:Malrec", "Salazar:Malrec", "Aldric:Malrec", "Malrec:Aldric", "Malrec:Voss", "Voss:Aldric", "Aldric:Voss"]);
const repliesByLeader: Record<AffinityHero, readonly [string, string, string]> = {
  Kael: ["I can listen without making this another burden. Tell me only what you choose.", "Give me a promise we can keep today. I'll give you the same.", "If we're going to stand together, I need you to tell me everything."],
  Neera: ["I can stay, or give you space. Tell me which feels right.", "We don't have to name this yet. Let's show up for each other tomorrow.", "You keep watching for an exit. How am I supposed to trust that?"],
  Voss: ["No theory, no clever answer. I'd like to understand you, if you'll let me.", "Let's be precise about what we can promise. Nothing larger than we mean.", "You're leaving out the important part. I can't understand you if you won't explain it."],
  Salazar: ["You don't owe me a confession. I can sit here and listen.", "A little care today is enough. Neither of us needs to promise forever.", "Keeping this inside is hurting you. You need to let me help."],
  Aldric: ["I can put duty aside and listen as a friend. You decide how much to say.", "One honest promise for the next watch. We can build from there.", "I can't stand beside someone who keeps part of the truth from me."],
  Malrec: ["I know what it costs to be asked for more than you can give. I won't ask that.", "Let a small promise be enough. I can keep mine if you can keep yours.", "You want my trust, but you're still deciding what I get to know."],
};

export function companionSceneId(leader: AffinityHero, companion: AffinityHero, tier: number): string {
  return `companion:${leader}:${companion}:${tier}`;
}
export function companionMaxTier(a: AffinityHero, b: AffinityHero): number { return a === "Malrec" || b === "Malrec" ? 2 : 4; }
export function cleanConversationMemory(raw: unknown): ConversationMemory {
  if (!raw || typeof raw !== "object" || Array.isArray(raw)) return {};
  return Object.fromEntries(Object.entries(raw).filter(([id, value]) => {
    const [prefix, a, b, tier] = id.split(":");
    return prefix === "companion" && id.split(":").length === 4 && AFFINITY_HEROES.includes(a as AffinityHero) && AFFINITY_HEROES.includes(b as AffinityHero)
      && a !== b && /^[0-4]$/.test(tier) && Number(tier) <= companionMaxTier(a as AffinityHero, b as AffinityHero) && (value === -3 || value === 0 || value === 3);
  })) as ConversationMemory;
}
export function companionTopics(leader: AffinityHero, companion: AffinityHero, scores?: AffinityScores, memory?: ConversationMemory) {
  if (leader === companion) return [];
  const points = affinityScore(scores, leader, companion);
  return COMPANION_TIERS.slice(0, companionMaxTier(leader, companion) + 1).map((threshold, tier) => ({
    tier, threshold, label: voice[companion].topics[tier], unlocked: points >= threshold || memory?.[companionSceneId(leader, companion, tier)] !== undefined,
    resolved: memory?.[companionSceneId(leader, companion, tier)] !== undefined,
  }));
}

export function buildCompanionDialogue(leader: AffinityHero, companion: AffinityHero, tier: number, scores?: AffinityScores, memory?: ConversationMemory): DialogTree | null {
  if (!Number.isInteger(tier) || !companionTopics(leader, companion, scores, memory).find(t => t.tier === tier)?.unlocked) return null;
  const arc = COMPANION_ARCS.find(a => a.heroes.includes(leader) && a.heroes.includes(companion));
  if (!arc) return null;
  const sceneId = companionSceneId(leader, companion, tier);
  const speakerIndex = arc.heroes.indexOf(companion);
  const disclosure = arc.stages[tier][speakerIndex];
  const invitation = arc.stages[tier][1 - speakerIndex];
  const v = voice[companion];
  const isReplay = memory?.[sceneId] !== undefined;
  const prefersPractical = practicalTrust.has(`${leader}:${companion}`);
  const previous = tier > 0 ? memory?.[companionSceneId(leader, companion, tier - 1)] : undefined;
  const continuity = previous === -3 ? "Last time, you pushed me further than I wanted to go. I'm willing to try again, but I need you to listen this time.\n\n"
    : previous === 3 ? "I've been thinking about our last conversation. You gave me a reason to say a little more.\n\n" : "";
  const choices = repliesByLeader[leader];
  const reply = (text: string, next: string, delta: -3 | 0 | 3): DialogReply => ({ text, next,
    companionScene: sceneId, affinity: { from: leader, to: companion, delta: isReplay ? 0 : delta } });
  const line = (id: string, speaker: AffinityHero, text: string, next?: string): DialogLine => ({ id, speaker, portrait: portraits[speaker], text, next });
  return { id: sceneId, startId: "ask", lines: [
    line("ask", leader, v.ask[tier], "confide"),
    { ...line("confide", companion, continuity + disclosure), replies: [
      reply(choices[0], "open", prefersPractical ? 0 : 3),
      reply(choices[1], "steady", prefersPractical ? 3 : 0),
      reply(choices[2], "boundary", -3),
      { text: "We can leave this here for now.", next: "leave" },
    ] },
    line("open", companion, prefersPractical ? "I hear you. An invitation is something I can consider, but a little time is what I need first." : v.welcome, "answer"),
    line("answer", leader, invitation, "close"),
    line("close", companion, tier >= 3 ? "I'm glad we said that aloud. Come find me again when you want to talk." : "That's enough for tonight. We can keep learning each other without rushing it."),
    line("steady", companion, v.respect, prefersPractical ? "answer" : undefined),
    line("boundary", companion, v.boundary),
    line("leave", companion, "All right. Another time, if we both want to."),
  ] };
}

/** Resolving a directed chapter is permanent; replaying never awards or removes points. */
export function resolveCompanionReply(scores: AffinityScores | undefined, memory: ConversationMemory | undefined, reply: DialogReply, leader: AffinityHero) {
  const cleaned = cleanConversationMemory(memory);
  const af = reply.affinity;
  const id = reply.companionScene;
  if (!id || !af || cleaned[id] !== undefined || af.from !== leader || !AFFINITY_HEROES.includes(af.to as AffinityHero)) return { scores: { ...scores }, memory: cleaned };
  const companion = af.to as AffinityHero;
  const tier = Number(id.split(":")[3]);
  if (id !== companionSceneId(leader, companion, tier) || !companionTopics(leader, companion, scores).find(t => t.tier === tier)?.unlocked) return { scores: { ...scores }, memory: cleaned };
  // Match the existing leader catch-up bonus; losses are never amplified.
  const delta = af.delta > 0 ? Math.round(af.delta * (2 - affinityScore(scores, leader, companion) / 100) * 100) / 100 : af.delta;
  return { scores: changeAffinity(scores, leader, companion, delta), memory: { ...cleaned, [id]: af.delta } };
}
