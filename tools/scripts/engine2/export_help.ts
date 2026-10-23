import { engineRoot } from './engineRoot';
// Engine2's "Como jogar" window (GameApp.tsx HelpModal, opened by the title's Como jogar) as data for EmberEngine, in both of Engine2's languages: its five tabs (Básicos, Usos, Dano, Fórmulas, Loot)
// with every text as HelpModal writes it (Portuguese, and the English of its `uiText(..., { en })` / Engine2's
// dictionary) and every table computed by Engine2's own rules (tierUses, the skill damage rows, weaponSkillAccuracy,
// dexEscapeChance, skillGainChance, the loot weights), read-only. Also writes Engine2's interface dictionary
// (gamePreferences.ts `english`: what uiText translates when the interface is in English) for the C++ menus.
//
// HelpModal's tables live in GameApp.tsx (a React module that cannot be loaded headless): their source block
// (SKILL_SPEED_GROUPS ... SKILL_DAMAGE_NOTES_EN) is cut from the file as written, its data imports pointed at Engine2's
// modules, and run as its own module in the temp folder. gamePreferences.ts imports React: its dictionary is read from
// the source text.
//
//   node content/game/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/export_help.ts -- <out help.json> <out ui-text.json>
//
// help.json: { pt: Help, en: Help }, Help = { title, close, tabs: [{ id, label, blocks }] }. A block: { t: "list", items }
// | { t: "p", size: "sm"|"xs", muted, medium, mt, spans: [{ text, accent?, muted?, medium? }] } | { t: "h3", text, mt }
// | { t: "box", lines: [[span]], mt } | { t: "table", head, align, nowrap, rows: [[cell]], mt, headBorder, size } where a
// cell is lines of spans. mt is in rem (16 canvas units).
// ui-text.json: { english: { "<Portuguese>": "<English>" } }.

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, resolve } from 'node:path';

const E2 = engineRoot;
const PUBLIC = `${E2}/public`;
globalThis.fetch = (async (url: string) => {
  const text = readFileSync(`${PUBLIC}${url}`, 'utf8');
  return { ok: true, status: 200, json: async () => JSON.parse(text), text: async () => text, headers: new Map([['content-type', 'application/json']]) };
}) as unknown as typeof fetch;

const [outPath, dictPath] = process.argv.slice(2).filter((a) => a !== '--');
if (!outPath || !dictPath) throw new Error('usage: export_help.ts -- <out help.json> <out ui-text.json>');

// --- HelpModal's tables, cut from GameApp.tsx ---------------------------------------------------------------------------
const app = readFileSync(`${E2}/src/game/GameApp.tsx`, 'utf8');
const start = app.indexOf('const SKILL_SPEED_GROUPS');
const end = app.indexOf('function helpFormulaText');
if (start < 0 || end < 0) throw new Error('GameApp.tsx: SKILL_SPEED_GROUPS / helpFormulaText not found (HelpModal moved?)');
const block = app.slice(start, end);
const dataModules = ['./data', './frost', './dexterity', './weaponTypes', './skills', './resistances', './poison', './types'];
const imports: string[] = [];
for (const m of app.matchAll(/import\s+(type\s+)?\{([^}]*)\}\s+from\s+"([^"]+)";/g)) {
  if (!dataModules.includes(m[3])) continue;
  imports.push(`import ${m[1] ?? ''}{${m[2]}} from "${E2}/src/game/${m[3].slice(2)}.ts";`);
}
const moduleFile = resolve(tmpdir(), 'ember-help-tables.generated.ts');
writeFileSync(moduleFile, `${imports.join('\n')}\n${block}\nexport { SKILL_SPEED_GROUPS, POTION_LOOT_ROWS, POTION_LOOT_TOTAL, SKILL_DAMAGE_ROWS, SKILL_DAMAGE_NOTES_EN };\n`);
const t = await import(moduleFile.replaceAll('\\', '/'));
const data = await import(`${E2}/src/game/data.ts`);
const weaponTypes = await import(`${E2}/src/game/weaponTypes.ts`);
const dexterity = await import(`${E2}/src/game/dexterity.ts`);
const skills = await import(`${E2}/src/game/skills.ts`);
const resistances = await import(`${E2}/src/game/resistances.ts`);

// --- gamePreferences.ts: the English dictionary and uiText --------------------------------------------------------------
const prefsSource = readFileSync(`${E2}/src/game/gamePreferences.ts`, 'utf8');
const dictStart = prefsSource.indexOf('const english: Record<string, string> = {');
const dictEnd = prefsSource.indexOf('\n};', dictStart);
if (dictStart < 0 || dictEnd < 0) throw new Error('gamePreferences.ts: the english dictionary not found');
const english = new Function(`return ${prefsSource.slice(prefsSource.indexOf('{', dictStart), dictEnd + 2)};`)() as Record<string, string>;
type Lang = 'pt' | 'en';
// uiText(text, { en }) with the interface in `lang` (translatedText: an empty translation falls back).
const uiText = (lang: Lang, text: string, en?: string) => (lang === 'en' ? (en?.trim() ? en : english[text] ?? text) : text);
// GameApp helpFormulaText's English.
const formulaEn = (v: string) => v.replaceAll('dano de arma', 'weapon damage').replaceAll('armadura', 'armor').replaceAll('arma', 'weapon')
  .replaceAll('cura', 'healing').replaceAll('centro', 'center').replaceAll('respingo', 'splash').replaceAll('raio', 'radius')
  .replaceAll('% de DEX', '% DEX').replaceAll('% dano', '% damage').replaceAll('dano', 'damage').replaceAll('alcance', 'range')
  .replaceAll('rodadas', 'rounds').replaceAll('Veja a progressão completa ao lado.', 'See the full progression beside it.');
const helpFormulaText = (lang: Lang, v: string) => uiText(lang, v, formulaEn(v));

// --- blocks ---------------------------------------------------------------------------------------------------------------
type Span = { text: string; accent?: boolean; muted?: boolean; medium?: boolean };
type Cell = Span[][];
const s = (text: string, extra: Partial<Span> = {}): Span => ({ text, ...extra });
const p = (spans: Span[] | string, opts: { size?: 'sm' | 'xs'; muted?: boolean; medium?: boolean; mt?: number } = {}) =>
  ({ t: 'p', size: opts.size ?? 'sm', muted: opts.muted ?? false, medium: opts.medium ?? false, mt: opts.mt ?? 0, spans: typeof spans === 'string' ? [s(spans)] : spans });
const h3 = (text: string, mt = 0) => ({ t: 'h3', text, mt });
const box = (lines: Span[][], mt = 0.5) => ({ t: 'box', lines, mt });
const cell = (v: string | number, extra: Partial<Span> = {}): Cell => [[s(String(v), extra)]];
const table = (head: string[], align: string[], rows: Cell[][], opts: { mt?: number; headBorder?: boolean; size?: string; nowrap?: number[] } = {}) =>
  ({ t: 'table', head, align, rows, mt: opts.mt ?? 0, headBorder: opts.headBorder ?? false, size: opts.size ?? 'xs', nowrap: opts.nowrap ?? [] });

function helpFor(lang: Lang) {
  const L = (pt: string, en: string) => uiText(lang, pt, en);  // uiText(pt, { en })
  const U = (pt: string) => uiText(lang, pt);                     // uiText(pt): Engine2's dictionary
  const numberLocale = lang === 'en' ? 'en-US' : 'pt-BR';

  const basicos = [{
    t: 'list',
    items: [
      L('Toque numa aliada para ver movimento (azul) e ataque (vermelho).', 'Select an ally to see movement (blue) and attack (red) ranges.'),
      L('Toque num inimigo para ver HP, alcance e a área vermelha de perigo.', 'Select an enemy to see HP, range, and its red danger zone.'),
      L('Todo mundo tem AT, MAG, DF, DEX, Mov e Alc. Nada fica de fora da ficha.', 'Every unit has ATK, MAG, DEF, DEX, Move, and Range. All stats are shown on its card.'),
      L('Terreno alto: +10% do ATK ou MAG (arredondado); armas de alcance ganham +1 de alcance. Flechas disparadas do alto passam por outro hex alto.', 'High ground: +10% ATK or MAG (rounded); ranged weapons gain +1 range. Arrows fired from high ground pass over another high hex.'),
      L('Barricada (estacas, 3 hexes): ninguém passa. De trás você atira. Projéteis não acertam quem está atrás.', 'Barricade (three hexes of stakes): units cannot pass through it. You can shoot from behind it, and projectiles do not hit units behind it.'),
      L('Após um ataque, o alvo pode contra-atacar se estiver vivo, não estiver atordoado e conseguir alcançar quem atacou. A prévia mostra chance e dano do contra-ataque.', 'After an attack, the target can counter if alive, not stunned, and able to reach the attacker. The preview shows counter hit chance and damage.'),
      L('Veneno causa dano no início do turno da vítima até ser curado. Sangramento causa 1D8 por ação; mover causa isso no máximo uma vez por turno. Doença reduz os atributos em 10%.', 'Poison deals damage at the start of the victim’s turn until cured. Bleeding deals 1D8 per action; movement triggers it at most once per turn. Disease reduces stats by 10%.'),
      L('Atacar uma fera neutra acorda todas as feras vivas da mesma espécie; elas começam a agir na rodada seguinte.', 'Attacking a neutral beast provokes every living beast of the same species; they start acting next round.'),
      L('Depois de mover, dois cliques no personagem = Esperar e passa ao próximo.', 'After moving, click the unit twice to Wait and pass to the next unit.'),
    ],
  }];

  const usos: unknown[] = [
    p(L('Cada classe tem uma velocidade de conjuração — ela decide quantos usos de cada tier (1 a 5) a classe tem em cada nível. As tabelas abaixo mostram os números exatos, nível a nível.', 'Each class has a casting speed, which determines how many uses of each tier (1 to 5) it gets at each level. The tables below show the exact numbers for every level.'), { muted: true }),
  ];
  for (const g of t.SKILL_SPEED_GROUPS as { label: string; classes: string; classId: string; maxTier: number }[]) {
    usos.push(p([s(U(g.label), { medium: true }), s(` · ${g.classes.split(', ').map((name) => U(name)).join(', ')}`, { muted: true })], { mt: 1.25 }));
    const tiers = Array.from({ length: g.maxTier }, (_, i) => i + 1);
    usos.push(table([U('Nv'), ...tiers.map((x) => `T${x}`)], ['l', ...tiers.map(() => 'r')],
      Array.from({ length: data.MAX_LEVEL as number }, (_, i) => i + 1).map((level) => [cell(level, { muted: true }), ...tiers.map((x) => cell(data.tierUses(g.classId, x, level)))]),
      { size: 'xs-tight' }));
  }

  const damageRows = (t.SKILL_DAMAGE_ROWS as {
    name: string; cls: string; tier: number; formula: string | ((x: number) => string); param?: 'level';
    progression?: { levels: string; range: number; size: number; duration: number; damage: string }[]; note: string;
  }[]).map((row) => {
    let formula: Cell;
    if (row.progression) {
      formula = row.progression.map((step) => [s(`${L('Níveis', 'Levels')} ${step.levels}: ${L('alcance', 'range')} ${step.range}, ${L('área de', 'area of')} ${step.size} ${L('hexes', 'hexes')}, ${L('duração', 'duration')} ${step.duration} ${L('rodadas', 'rounds')}, ${L('dano', 'damage')} ${step.damage}`)]);
    } else if (typeof row.formula === 'string') {
      formula = [[s(helpFormulaText(lang, row.formula))]];
    } else if (row.param === 'level') {
      const f = row.formula;
      formula = [[s(`${U('Nv1')}: ${helpFormulaText(lang, f(1))}`), s(` · ${U('Nv7')}: ${helpFormulaText(lang, f(7))}`, { muted: true }), s(` · ${U('Nv14')}: ${helpFormulaText(lang, f(14))}`, { muted: true })]];
    } else {
      const f = row.formula;
      formula = [[s(`${U('MAG 10')}: ${helpFormulaText(lang, f(10))}`), s(` · ${U('MAG 20')}: ${helpFormulaText(lang, f(20))}`, { muted: true }), s(` · ${U('MAG 40')}: ${helpFormulaText(lang, f(40))}`, { muted: true })]];
    }
    const className = (data.CLASSES as Record<string, { name: string }>)[row.cls]?.name ?? row.cls;
    const note = uiText(lang, row.note, (t.SKILL_DAMAGE_NOTES_EN as Record<string, string>)[row.name]);
    return [cell(U(row.name), { medium: true }), cell(U(className), { muted: true }), cell(`T${row.tier}`, { muted: true }), formula, cell(note, { muted: true })];
  });
  const dano = [
    p([s(L('Ataque normal', 'Normal attack'), { accent: true }), s(' = ' + L('metade do seu ATK (ou MAG, se for conjurador) + dados da arma + terreno − metade da DEF do alvo (resistência elemental, contra magia). Metades não contam: arredonda pra baixo. Mínimo 1 de dano.', "half your ATK (or MAG for a caster) + weapon dice + terrain − half the target's DEF (elemental resistance against magic). Fractions round down. Minimum 1 damage."))]),
    p([s(L('Magia', 'Spell'), { accent: true }), s(' = ' + L('a mesma conta, com a sua metade de MAG multiplicada pelo peso da magia e os dados dela no lugar da arma. Todo peso é maior que 1, e o resultado nunca fica abaixo de um ataque normal — conjurar sempre vale mais que bater.', "Uses the same calculation, with half your MAG multiplied by the spell's multiplier and its dice in place of the weapon dice. Every multiplier is greater than 1, and the result is never lower than a normal attack."))], { mt: 0.5 }),
    p(L('As fórmulas mostram o dano pelo MAG do conjurador. Ice Storm ocupa 3 hexes nos níveis 8–11 e 7 hexes a partir do nível 12, sem novos aumentos de área. Alcance, duração e dano continuam a progredir; os valores aparecem em cada faixa de nível.', "The formulas show damage by the caster's MAG. Ice Storm covers 3 hexes at levels 8–11 and 7 hexes from level 12 onward, with no further area increases. Range, duration, and damage continue to scale; values are shown for each level range."), { size: 'xs', muted: true, mt: 0.5 }),
    table([U('Habilidade'), U('Classe'), 'Tier', U('Fórmula'), U('Efeito')], ['l', 'l', 'c', 'l', 'l'], damageRows, { mt: 1.25, nowrap: [0, 1] }),
  ];

  const pct = (v: number) => `${v}%`;
  const resistPt: Record<string, string> = { fire: 'Fogo', lightning: 'Relâmpago', ice: 'Gelo', arcane: 'Arcano', darkness: 'Escuridão', holy: 'Sagrado', poison: 'Veneno', ember: 'Brasa' };
  const resistEn = resistances.RESISTANCE_LABELS as Record<string, string>;
  const formulas = [
    h3(L('Proficiência com armas', 'Weapon Proficiency')),
    p(L('Cada personagem só treina os tipos de arma permitidos para sua classe. A proficiência vai de 0 a 100: cada ponto acrescenta 1 ponto percentual à precisão e 1% aos dados da arma.', 'Each character trains only weapon types allowed by their class. Proficiency ranges from 0 to 100: every point adds 1 percentage point to accuracy and 1% to the weapon dice.'), { size: 'xs', muted: true, mt: 0.5 }),
    box([
      [s(L('Precisão final', 'Final accuracy'), { accent: true }), s(': ' + L('limite entre 0% e 100% de (75% + proficiência da arma + bônus de acerto − DEX do defensor)', 'Clamp (75% + weapon proficiency + hit bonus − defender DEX) to 0%–100%.'))],
      [s(L('Multiplicador dos dados', 'Weapon-dice multiplier'), { accent: true }), s(': ' + L('1 + (proficiência da arma ÷ 100)', '1 + (weapon proficiency ÷ 100)'))],
    ]),
    table([L('Proficiência', 'Proficiency'), L('Precisão base', 'Base accuracy'), L('Multiplicador dos dados', 'Weapon-dice multiplier')], ['l', 'r', 'r'],
      [0, 1, 20, 50, 100].map((v) => [cell(v), cell(pct(weaponTypes.weaponSkillAccuracy(v))), cell(`×${weaponTypes.weaponSkillDamageMultiplier(v).toFixed(2)}`)]), { mt: 0.5, headBorder: true }),
    p(L('Exemplo de precisão: proficiência 20, bônus de acerto 0 e defensor com DEX 15 → 75 + 20 − 15 = 80% de chance. Em 100 pontos de proficiência, a precisão base chega a 175%; o limite final ainda é aplicado depois da DEX do defensor.', 'Accuracy example: proficiency 20, no hit bonus, defender DEX 15 → 75 + 20 − 15 = 80% chance. At 100 proficiency, base accuracy reaches 175%; the final cap is applied after subtracting defender DEX.'), { size: 'xs', muted: true, mt: 0.5 }),

    h3(L('Destreza (DEX)', 'Dexterity (DEX)'), 1.5),
    box([
      [s(L('Precisão contra você', 'Accuracy against you'), { accent: true }), s(': ' + L('chance do ataque − sua DEX, limitado entre 0% e 100%.', 'attack chance − your DEX, clamped to 0%–100%.'))],
      [s(L('Fuga', 'Escape'), { accent: true }), s(': ' + L('mínimo entre 100% e (60% + DEX ÷ 3)', 'the lower of 100% and (60% + DEX ÷ 3)'))],
    ]),
    table(['DEX', L('Chance de fuga', 'Escape chance')], ['l', 'r'],
      [0, 1, 3, 15, 30, 60, 120].map((dex) => [cell(dex), cell(`${Number(dexterity.dexEscapeChance(dex).toFixed(2)).toLocaleString(numberLocale)}%`)]), { mt: 0.5, headBorder: true }),
    p(L('Fugir exige que o personagem ativo alcance um hex da borda do campo. A chance é calculada pela DEX de quem tenta fugir; falhar consome o turno.', "To flee, the active character must reach a battlefield edge hex. The chance uses the fleeing character's DEX; a failed attempt uses their turn."), { size: 'xs', muted: true, mt: 0.5 }),

    h3(L('Resistências elementais', 'Elemental Resistances'), 1.5),
    box([
      [s(L('Resistência total', 'Total resistance'), { accent: true }), s(': ' + L('bônus da classe + perícia de resistência + bônus de equipamento', 'class bonus + resistance skill + equipment bonus'))],
      [s(L('Resistência efetiva', 'Effective resistance'), { accent: true }), s(': ' + L('limite entre −50% e 100% de (resistência − penalidade do efeito − MAG do atacante ÷ 2)', 'Clamp (resistance − effect penalty − attacker MAG ÷ 2) to −50%–100%.'))],
      [s(L('Dano depois da resistência', 'Damage after resistance'), { accent: true }), s(': ' + L('arredondar para baixo [dano base × (1 − resistência efetiva ÷ 100)]', 'floor [base damage × (1 − effective resistance ÷ 100)]'))],
    ]),
    table([L('Perícia', 'Skill'), L('Dano que reduz', 'Damage reduced')], ['l', 'l'],
      (resistances.RESISTANCE_ELEMENTS as string[]).map((e) => [cell(L(`${resistPt[e]} Resistência`, `${resistEn[e]} Resistance`)), cell(L(resistPt[e], resistEn[e]))]), { mt: 0.5, headBorder: true }),
    p(L('Cada ponto de resistência reduz 1 ponto percentual do dano daquele elemento. MAG 40 penetra 20 pontos. Exemplo: resistência 70 − 20 = 50% efetiva; um golpe de 30 causa 15. Resistências são independentes: Resistência a Fogo não reduz Relâmpago, Gelo, Arcano, Escuridão, Sagrado, Veneno ou Brasa.', 'Each resistance point reduces damage from its element by 1 percentage point. MAG 40 penetrates 20 points. Example: resistance 70 − 20 = 50% effective; a 30-damage hit deals 15. Resistances are independent: Fire Resistance does not reduce Lightning, Ice, Arcane, Darkness, Holy, Poison, or Ember.'), { size: 'xs', muted: true, mt: 0.5 }),

    h3(L('Veneno', 'Poison'), 1.5),
    box([
      [s(L('Resistência efetiva a veneno', 'Effective Poison Resistance'), { accent: true }), s(': ' + L('limite entre −50% e 100% de (Resistência a Veneno − penalidade do veneno − MAG do atacante ÷ 2)', 'Clamp (Poison Resistance − poison penalty − attacker MAG ÷ 2) to −50%–100%.'))],
      [s(L('Chance de aplicação após acertar', 'Application chance after a hit'), { accent: true }), s(': ' + L('limite entre 0% e 100% de (100% − resistência efetiva)', 'Clamp (100% − effective resistance) to 0%–100%.'))],
      [s(L('Dano por turno', 'Damage per turn'), { accent: true }), s(': ' + L('arredondar ao inteiro mais próximo [dano rolado × (1 − resistência efetiva ÷ 100)]', 'round to the nearest integer [rolled damage × (1 − effective resistance ÷ 100)]'))],
    ]),
    p(L('Exemplo: Resistência a Veneno 70, penalidade do veneno 20 e MAG atacante 40 → 70 − 20 − 20 = 30% efetiva. Uma rolagem de dano 30 causa 30 × 70% = 21. Com Resistência 71, a resistência efetiva é 31%; o dano antes do arredondamento é 20,7.', 'Example: Poison Resistance 70, poison penalty 20, attacker MAG 40 → 70 − 20 − 20 = 30% effective. A rolled tick of 30 deals 30 × 70% = 21. At Resistance 71, effective resistance is 31%; damage before rounding is 20.7.'), { size: 'xs', muted: true, mt: 0.5 }),
    p(L('A penalidade e os dados do tick dependem do veneno aplicado. Veneno causa dano no início do turno da vítima e permanece até ser curado. Dano acima do limite de resistência pode chegar a 150% do valor rolado; a chance de aplicação continua limitada a 100%.', "The resistance penalty and tick dice depend on the applied poison. Poison ticks at the start of the victim's turn and remains until cured. Damage can reach 150% of the roll at the resistance floor; application chance remains capped at 100%."), { size: 'xs', muted: true, mt: 0.5 }),

    h3(L('Aprendizado das perícias', 'Skill Progression'), 1.5),
    box([
      [s(L('Chance de ganhar perícia', 'Skill-gain chance'), { accent: true }), s(': ' + L('(100 − valor atual da perícia) ÷ 100; máximo 100, mínimo 0.', '(100 − current skill value) ÷ 100; minimum 0, maximum 100.'))],
      [s(L('Limite', 'Cap'), { accent: true }), s(`: 100. ${L('Resistências ganham normalmente', 'Resistance skills normally gain')} +${(skills.SKILL_GAIN as number).toLocaleString(numberLocale)} ${L('por sucesso de treino.', 'per successful training attempt.')}`)],
    ]),
    table([L('Valor atual', 'Current value'), L('Chance de ganho', 'Gain chance')], ['l', 'r'],
      [0, 50, 90, 99, skills.SKILL_CAP].map((v: number) => [cell(v), cell(`${skills.skillGainChance(v) * 100}%`)]), { mt: 0.5, headBorder: true }),
    p(L('Perícia de arma pode treinar ao atacar inimigos com aquele tipo de arma, inclusive quando erra. Resistência treina ao usar ou sofrer dano do elemento correspondente; em área, cada inimigo atingido permite uma tentativa. Magias dos familiares treinam a resistência correspondente do conjurador.', "Weapon proficiency can train by attacking enemies with that weapon type, including misses. A resistance trains when you use or take damage from its element; each enemy hit by an area attack gives one gain attempt. Familiar spells train the summoner's matching resistance."), { size: 'xs', muted: true, mt: 0.5 }),
    p(L('Contra inimigo 5 níveis abaixo: o ganho é +0,05. Com diferença de 10 níveis: não há ganho. No treino de estrada, escolha uma perícia por personagem: resistência recebe +0,1 a cada 12 horas; arma recebe +1 a cada 120 horas. A contagem restante é mantida ao trocar de perícia.', 'Against an enemy 5 levels below: gain is +0.05. At a 10-level difference: no gain. For road training, choose one skill per character: resistance gains +0.1 every 12 hours; weapon proficiency gains +1 every 120 hours. Remaining time carries over when switching skills.'), { size: 'xs', muted: true, mt: 0.5 }),
  ];

  const chest = data.CHEST_LOOT as { emberBase: number; emberDice: number; gearChance: number };
  const loot = [
    p(U('Chances de drop, do jeito que estão programadas agora.'), { muted: true }),
    p(U('Poções em baú (por baú)'), { medium: true, mt: 1.25 }),
    p(U('Todo baú dá Gold + uma poção garantida (sorteada abaixo) + uma chance separada de item. Se quem abriu já estiver no máximo daquela poção (5), ela passa para o próximo personagem que vai agir; se todos estiverem cheios, é descartada.'), { size: 'xs', muted: true }),
    table([U('Poção'), U('Chance')], ['l', 'r'],
      (t.POTION_LOOT_ROWS as { name: string; weight: number }[]).map((r) => [cell(U(r.name)), cell(`${((r.weight / t.POTION_LOOT_TOTAL) * 100).toFixed(0)}%`)]), { mt: 0.5, size: 'xs-tight' }),
    p(U('Gold e item de baú'), { medium: true, mt: 1.25 }),
    p(`${U('Gold:')} ${chest.emberBase}–${chest.emberBase + chest.emberDice - 1} ${U('por baú.')}`, { muted: true, mt: 0.25 }),
    p(`${U('Chance extra de arma ou equipamento:')} ${Math.round(chest.gearChance * 100)}%.`, { muted: true, mt: 0.25 }),
    p(U('Drop ao matar inimigo'), { medium: true, mt: 1.25 }),
    p(`${U('Inimigo comum:')} ${((data.KILL_DROP_CHANCE as number) * 100).toFixed(0)}% ${U('de chance de largar uma arma.')}`, { muted: true, mt: 0.25 }),
    p(U('Chefes nomeados (drop garantido): sempre largam arma ou equipamento ao morrer.'), { muted: true, mt: 0.25 }),
    p(U('Toda arma/equipamento largado é sorteado por preço — quanto mais caro, mais raro — e limitado ao nível de itens da missão atual, então cada trecho da campanha só solta o que faz sentido pra ele.'), { size: 'xs', muted: true, mt: 1.25 }),
  ];

  return {
    title: U('Como jogar'),
    close: U('Entendi'),
    tabs: [
      { id: 'basicos', label: U('Básicos'), blocks: basicos },
      { id: 'tabelas', label: U('Usos'), blocks: usos },
      { id: 'dano', label: U('Dano'), blocks: dano },
      { id: 'formulas', label: L('Fórmulas', 'Formulas'), blocks: formulas },
      { id: 'loot', label: U('Loot'), blocks: loot },
    ],
    skillRows: damageRows.length,
  };
}

const pt = helpFor('pt'), en = helpFor('en');
const out = { source: 'Engine2 src/game/GameApp.tsx HelpModal', pt, en };
mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify(out, null, 1) + '\n');
mkdirSync(dirname(dictPath), { recursive: true });
writeFileSync(dictPath, JSON.stringify({ source: 'Engine2 src/game/gamePreferences.ts english', english }, null, 1) + '\n');
console.log(`help: ${pt.tabs.map((x) => `${x.label} ${x.blocks.length}`).join(', ')}; ${pt.skillRows} skill rows; en ${en.tabs.map((x) => x.label).join(' / ')} -> ${outPath}`);
console.log(`ui-text: ${Object.keys(english).length} English entries -> ${dictPath}`);
