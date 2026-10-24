import { CHEST_LOOT, KILL_DROP_CHANCE, MAX_LEVEL, POTIONS, POTION_LOOT_WEIGHT, SPELL_TIER, tierUses, type SpellTier } from "../ember/data";
import { dexEscapeChance } from "../campaign/dexterity";
import { skillGainChance, SKILL_GAIN } from "../campaign/skills";
import { weaponSkillAccuracy, weaponSkillDamageMultiplier } from "../campaign/weaponTypes";
import type { ClassId } from "../ember/types";

export type HelpLanguage = "pt" | "en";
export type HelpTab = "basics" | "uses" | "damage" | "formulas" | "loot";

const CLASS_NAMES: Record<string, string> = {
  mage: "Mago", conjurer: "Conjurador", healer: "Curandeiro", elementalist: "Elementalista", sorcerer: "Feiticeiro", bishop: "Bispo",
  archer: "Arqueira", warlock: "Bruxo", necromancer: "Necromante", cleric: "Clérigo", paladin: "Paladino", assassin: "Assassino", templar: "Templário",
  swordsman: "Guerreiro", lancer: "Lanceiro", aldric: "Aldric", heavyKnight: "Cavaleiro Pesado", ranger: "Patrulheiro", sentinel: "Sentinela",
};
const CLASS_NAMES_EN: Record<string, string> = {
  mage: "Mage", conjurer: "Conjurer", healer: "Healer", elementalist: "Elementalist", sorcerer: "Sorcerer", bishop: "Bishop",
  archer: "Archer", warlock: "Warlock", necromancer: "Necromancer", cleric: "Cleric", paladin: "Paladin", assassin: "Assassin", templar: "Templar",
  swordsman: "Warrior", lancer: "Lancer", aldric: "Aldric", heavyKnight: "Heavy Knight", ranger: "Ranger", sentinel: "Sentinel",
};

const groups = [
  { label: ["Conjuração Rápida", "Fast casting"], classes: ["mage", "conjurer", "healer", "elementalist", "sorcerer", "bishop"], sample: "mage", tiers: 10 },
  { label: ["Conjuração Média", "Medium casting"], classes: ["archer", "warlock", "necromancer", "cleric", "paladin", "assassin", "templar"], sample: "archer", tiers: 8 },
  { label: ["Conjuração Lenta", "Slow casting"], classes: ["swordsman", "lancer", "aldric", "heavyKnight", "ranger", "sentinel"], sample: "swordsman", tiers: 6 },
] as const;

const basics: Record<HelpLanguage, string[]> = {
  pt: [
    "Toque numa aliada para ver movimento (azul) e ataque (vermelho).",
    "Toque num inimigo para ver HP, alcance e a área vermelha de perigo.",
    "Todo mundo tem AT, MAG, DF, DEX, Mov e Alc. Nada fica de fora da ficha.",
    "Terreno alto: +10% do ATK ou MAG (arredondado); armas de alcance ganham +1 de alcance. Flechas disparadas do alto passam por outro hex alto.",
    "Barricada (estacas, 3 hexes): ninguém passa. De trás você atira. Projéteis não acertam quem está atrás.",
    "Após um ataque, o alvo pode contra-atacar se estiver vivo, não estiver atordoado e conseguir alcançar quem atacou. A prévia mostra chance e dano do contra-ataque.",
    "Veneno causa dano no início do turno da vítima até ser curado. Sangramento causa 1D8 por ação; mover causa isso no máximo uma vez por turno. Doença reduz os atributos em 10%.",
    "Atacar uma fera neutra acorda todas as feras vivas da mesma espécie; elas começam a agir na rodada seguinte.",
    "Depois de mover, dois cliques no personagem = Esperar e passa ao próximo.",
  ],
  en: [
    "Select an ally to see movement (blue) and attack (red) ranges.",
    "Select an enemy to see HP, range, and its red danger zone.",
    "Every unit has ATK, MAG, DEF, DEX, Move, and Range. All stats are shown on its card.",
    "High ground: +10% ATK or MAG (rounded); ranged weapons gain +1 range. Arrows fired from high ground pass over another high hex.",
    "Barricade (three hexes of stakes): units cannot pass through it. You can shoot from behind it, and projectiles do not hit units behind it.",
    "After an attack, the target can counter if alive, not stunned, and able to reach the attacker. The preview shows counter hit chance and damage.",
    "Poison deals damage at the start of the victim's turn until cured. Bleeding deals 1D8 per action; movement triggers it at most once per turn. Disease reduces stats by 10%.",
    "Attacking a neutral beast provokes every living beast of the same species; they start acting next round.",
    "After moving, click the unit twice to Wait and pass to the next unit.",
  ],
};

const spells: [string, number, string, string][] = [
  ["bless", 1, "Curandeiro", "Healer"], ["magicMissile", 1, "Mago", "Mage"], ["longShot", 1, "Arqueira", "Archer"], ["provoke", 1, "Guerreiro", "Warrior"],
  ["cureMinor", 1, "Curandeiro", "Healer"], ["doubleStrike", 1, "Guerreiro", "Warrior"], ["piercingThrust", 1, "Lanceiro", "Lancer"], ["summonFamiliar", 1, "Conjurador", "Conjurer"],
  ["bloodyShot", 2, "Arqueira", "Archer"], ["lightning", 2, "Mago", "Mage"], ["piercing", 2, "Arqueira", "Archer"], ["cureWounds", 2, "Curandeiro", "Healer"], ["cleave", 2, "Guerreiro", "Warrior"],
  ["sweep", 2, "Lanceiro", "Lancer"], ["trip", 3, "Lanceiro", "Lancer"], ["webOfDreams", 2, "Conjurador", "Conjurer"], ["fireball", 3, "Mago", "Mage"], ["iceStorm", 4, "Mago", "Mage"],
  ["frost", 2, "Mago", "Mage"], ["lightningTier3", 5, "Elementalista", "Elementalist"], ["cureDisease", 3, "Clérigo", "Cleric"], ["causticVenom", 5, "Mago", "Mage"], ["warp", 3, "Mago", "Mage"],
  ["divineBolt", 4, "Curandeiro", "Healer"], ["multiShot", 3, "Arqueira", "Archer"], ["secondWind", 3, "Paladino", "Paladin"], ["cureLight", 4, "Paladino", "Paladin"],
  ["auraOfProtection", 5, "Paladino", "Paladin"], ["divineWrath", 6, "Paladino", "Paladin"], ["shoulderSmash", 4, "Cavaleiro Pesado", "Heavy Knight"],
  ["intimidatingPresence", 5, "Cavaleiro Pesado", "Heavy Knight"], ["stampede", 6, "Cavaleiro Pesado", "Heavy Knight"], ["bullRush", 1, "Guerreiro", "Warrior"],
  ["shieldBash", 2, "Guerreiro", "Warrior"], ["executionerStrike", 3, "Guerreiro", "Warrior"], ["poisonBreath", 1, "Mago", "Mage"], ["burningHands", 2, "Mago", "Mage"],
  ["createFoodAndWater", 4, "Curandeiro", "Healer"], ["turnUndead", 3, "Clérigo", "Cleric"], ["summonFamiliar2", 2, "Conjurador", "Conjurer"],
  ["summonFamiliar4", 3, "Conjurador", "Conjurer"], ["summonFamiliar3", 4, "Conjurador", "Conjurer"], ["summonZombieDog", 5, "Conjurador", "Conjurer"],
];
const spellNames: Record<string, [string, string]> = {
  bless: ["Bênção", "Bless"], magicMissile: ["Míssil Mágico", "Magic Missile"], longShot: ["Tiro Longo", "Long Shot"], provoke: ["Provocar", "Provoke"],
  cureMinor: ["Cura Menor", "Minor Heal"], doubleStrike: ["Golpe Duplo", "Double Strike"], piercingThrust: ["Investida Perfurante", "Piercing Thrust"], summonFamiliar: ["Invocar Familiar", "Summon Familiar"],
  bloodyShot: ["Tiro Sangrento", "Bloody Shot"], lightning: ["Relâmpago", "Lightning"], piercing: ["Tiro Perfurante", "Piercing Shot"], cureWounds: ["Cura Média", "Medium Heal"], cleave: ["Corte Amplo", "Cleave"],
  sweep: ["Varredura", "Sweep"], trip: ["Rasteira", "Trip"], webOfDreams: ["Teia dos Sonhos", "Web of Dreams"], fireball: ["Bola de Fogo", "Fireball"], iceStorm: ["Tempestade de Gelo", "Ice Storm"],
  frost: ["Gelo", "Frost"], lightningTier3: ["Relâmpago T3", "Lightning T3"], cureDisease: ["Curar Doença", "Cure Disease"], causticVenom: ["Veneno Cáustico", "Caustic Venom"],
  divineBolt: ["Raio Divino", "Divine Bolt"], multiShot: ["Tiro Múltiplo", "Multi Shot"], secondWind: ["Segundo Fôlego", "Second Wind"], cureLight: ["Cura Leve", "Light Heal"],
  warp: ["Warp", "Warp"],
  auraOfProtection: ["Aura de Proteção", "Aura of Protection"], divineWrath: ["Ira Divina", "Divine Wrath"], shoulderSmash: ["Ombro Esmagador", "Shoulder Smash"],
  intimidatingPresence: ["Presença Intimidante", "Intimidating Presence"], stampede: ["Debandada", "Stampede"], bullRush: ["Investida do Touro", "Bull Rush"], shieldBash: ["Golpe de Escudo", "Shield Bash"],
  executionerStrike: ["Golpe do Carrasco", "Executioner Strike"], poisonBreath: ["Sopro Venenoso", "Poison Breath"], burningHands: ["Mãos Flamejantes", "Burning Hands"],
  createFoodAndWater: ["Criar Comida e Água", "Create Food and Water"], turnUndead: ["Afastar Mortos-vivos", "Turn Undead"], summonFamiliar2: ["Invocar Familiar Maior", "Summon Greater Familiar"],
  summonFamiliar4: ["Invocar Familiar Radiante", "Summon Radiant Familiar"], summonFamiliar3: ["Invocar Familiar de Fogo", "Summon Fire Familiar"], summonZombieDog: ["Invocar Cão Zumbi", "Summon Zombie Hound"],
};
const spellDetails: Record<string, [string, string]> = {
  bless: ["Raio 1; aumenta a precisão em 1% por nível até +10%; duração cresce com o nível.", "Radius 1; hit chance rises 1% per level up to +10%; duration scales with level."],
  magicMissile: ["Nunca erra. Lança 1 míssil, 2 no nível 3 e 3 no nível 6; cada um atinge um alvo.", "Never misses. Fires 1 missile, 2 at level 3, and 3 at level 6; each hits one target."],
  longShot: ["Alcance 7; os dados aumentam em níveis específicos.", "Range 7; damage dice increase at level breakpoints."],
  provoke: ["Sem dano: atrai a inimizade dos inimigos atingidos; a área cresce com o nível.", "Deals no damage: draws the enmity of affected enemies; area grows with level."],
  cureMinor: ["Cura um aliado; o valor escala com MAG.", "Heals one ally; healing scales with MAG."],
  doubleStrike: ["Ataca duas vezes; cada acerto rola seu próprio bônus.", "Attacks twice; each hit rolls its own bonus."],
  piercingThrust: ["Ataca em linha; o segundo alvo recebe metade do dano.", "Strikes in a line; the second target takes half damage."],
  summonFamiliar: ["Invoca um aliado que pode lançar Míssil Mágico por conta própria.", "Summons an ally that can cast Magic Missile on its own."],
  bloodyShot: ["Causa dano de arma e sangramento; a duração aumenta em níveis específicos.", "Deals weapon damage and bleeding; duration increases at level breakpoints."],
  lightning: ["Atravessa cobertura e barricadas; pode ecoar para um alvo adjacente.", "Pierces cover and barricades; can echo to an adjacent target."],
  piercing: ["Multiplica o dano da arma; o multiplicador cresce com o nível.", "Multiplies weapon damage; the multiplier grows with level."],
  cureWounds: ["Cura um aliado; o valor escala com MAG.", "Heals one ally; healing scales with MAG."],
  cleave: ["Atinge vários hexes e causa dano extra a criaturas grandes.", "Hits multiple hexes and deals extra damage to large creatures."],
  sweep: ["Atinge inimigos próximos e os empurra para trás.", "Hits nearby enemies and knocks them back."],
  trip: ["Causa sangramento e reduz atributos pelo resto da batalha.", "Causes bleeding and reduces stats for the rest of the battle."],
  webOfDreams: ["Pode dormir inimigos e limitar o movimento dentro da área.", "Can put enemies to sleep and restrict movement inside its area."],
  fireball: ["Dano de fogo em área; o raio vem da habilidade.", "Area fire damage; radius is defined by the skill."],
  iceStorm: ["Dano de gelo por turno na área; permanece no campo por várias rodadas.", "Ice damage each turn inside the area; remains on the field for multiple rounds."],
  frost: ["Linha de gelo que cresce com o nível; pode atingir aliados.", "A line of ice that grows with level; can hit allies."],
  lightningTier3: ["Relâmpago de alto tier; atravessa cobertura e pode ecoar.", "High-tier lightning; pierces cover and can echo."],
  cureDisease: ["Cura doença e veneno.", "Cures disease and poison."],
  causticVenom: ["Veneno em área, com dano central e respingo; aplica veneno persistente.", "Area venom with center and splash damage; applies persistent poison."],
  divineBolt: ["Dano sagrado em área ao redor do alvo.", "Area holy damage around the target."],
  multiShot: ["Atinge vários alvos; o número de alvos cresce com o nível.", "Hits multiple targets; target count increases with level."],
  secondWind: ["Passiva: cura automaticamente quando o HP fica baixo.", "Passive: automatically heals when HP is low."],
  cureLight: ["Cura um aliado; usa cargas próprias do Paladino.", "Heals one ally; uses the Paladin's own charges."],
  auraOfProtection: ["Reduz dano recebido por aliados próximos por várias rodadas.", "Reduces damage taken by nearby allies for several rounds."],
  divineWrath: ["Linha sagrada mirando para frente; não atinge aliados.", "Aimed holy line; never hits allies."],
  shoulderSmash: ["Ataque em arco que empurra os alvos; requer estar sem escudo.", "Arc attack that knocks targets back; requires no shield."],
  intimidatingPresence: ["Aumenta o dano recebido por inimigos próximos.", "Increases damage taken by nearby enemies."],
  stampede: ["Investida em linha reta; pode atingir aliados.", "Straight-line charge; can hit allies."],
  bullRush: ["Investida que desloca o alvo.", "A charge that displaces its target."],
  shieldBash: ["Ataque com escudo que pode interromper ou atordoar.", "Shield attack that can interrupt or stun."],
  executionerStrike: ["Golpe de execução com dano aumentado.", "Execution strike with increased damage."],
  poisonBreath: ["Cone venenoso; aplica veneno aos alvos atingidos.", "Poison cone; applies poison to affected targets."],
  burningHands: ["Cone de fogo em curta distância.", "Short-range cone of fire."],
  createFoodAndWater: ["Cria provisões para a companhia.", "Creates provisions for the party."],
  turnUndead: ["Dano sagrado e medo; mortos-vivos fogem por duas rodadas.", "Holy damage and fear; undead flee for two rounds."],
  warp: ["Aprendido no nível 7. Em masmorras, abre um portal perto do Mago e outro perto da entrada por 3 rodadas (4 no nível 15; 5 no nível 23). Aliados precisam alcançar uma abertura para atravessar; inimigos de tamanho humano também podem segui-los. No mapa-múndi, permite viajar para cidades visitadas.", "Learned at level 7. In dungeons, opens one portal near the Mage and another near the entrance for 3 rounds (4 at level 15; 5 at level 23). Allies must reach an opening to cross; human-sized enemies can follow them through. On the world map, Warp travels to visited cities."],
  summonFamiliar2: ["Invoca um familiar maior que pode usar duas magias.", "Summons a greater familiar that can use two spells."],
  summonFamiliar4: ["Invoca um familiar radiante que pode usar duas magias.", "Summons a radiant familiar that can use two spells."],
  summonFamiliar3: ["Invoca um familiar que pode lançar Bola de Fogo.", "Summons a familiar that can cast Fireball."],
  summonZombieDog: ["Invoca um cão zumbi aliado.", "Summons an allied zombie hound."],
};

const HTML_ESCAPE: Record<string, string> = { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" };
const html = (value: string) => value.replace(/[&<>"']/g, (ch) => HTML_ESCAPE[ch]!);
function makeTable(headers: string[], rows: (string | number)[][]): string {
  return `<div class="help-table-wrap"><table class="help-table"><thead><tr>${headers.map((h) => `<th>${html(h)}</th>`).join("")}</tr></thead><tbody>${rows.map((row) => `<tr>${row.map((cell) => `<td>${html(String(cell))}</td>`).join("")}</tr>`).join("")}</tbody></table></div>`;
}
const p = (text: string, cls = "") => `<p${cls ? ` class="${cls}"` : ""}>${text}</p>`;
const list = (items: string[]) => `<ul class="help-list">${items.map((item) => `<li>${html(item)}</li>`).join("")}</ul>`;

function usesTab(lang: HelpLanguage): string {
  const rows: string[] = [];
  const levels = Array.from({ length: MAX_LEVEL }, (_, i) => i + 1);
  for (const group of groups) {
    const classes = group.classes.map((id) => (lang === "pt" ? CLASS_NAMES[id] : CLASS_NAMES_EN[id])).join(", ");
    rows.push(`<h3>${group.label[lang === "pt" ? 0 : 1]}</h3>${p(classes, "help-class-list")}`);
    rows.push(makeTable([lang === "pt" ? "Nv" : "Lv", ...Array.from({ length: group.tiers }, (_, i) => `T${i + 1}`)], levels.map((level) => [level, ...Array.from({ length: group.tiers }, (_, i) => tierUses(group.sample as ClassId, (i + 1) as SpellTier, level))])));
  }
  return p(lang === "pt" ? "Cada classe tem uma velocidade de conjuração. Ela determina quantos usos de cada tier (1 a 10) ficam disponíveis em cada nível. As classes de cada grupo compartilham a mesma tabela." : "Each class has a casting speed. It determines how many uses of each tier (1 to 10) are available at each level. Classes in each group share the same table.") + rows.join("");
}

function formulasTab(lang: HelpLanguage): string {
  const weaponValues = [0, 1, 20, 50, 100];
  const dexValues = [0, 1, 3, 15, 30, 60, 120];
  const skillValues = [0, 50, 90, 99, 100];
  const resistances = lang === "pt" ? ["Fogo", "Relâmpago", "Gelo", "Arcano", "Escuridão", "Sagrado", "Veneno", "Brasa"] : ["Fire", "Lightning", "Ice", "Arcane", "Darkness", "Holy", "Poison", "Ember"];
  return [
    `<h3>${lang === "pt" ? "Proficiência com armas" : "Weapon proficiency"}</h3>`,
    p(lang === "pt" ? "Cada personagem treina os tipos de arma permitidos para sua classe. A proficiência vai de 0 a 100: cada ponto acrescenta 1 ponto percentual à precisão e 1% aos dados da arma." : "Each character trains the weapon types allowed by their class. Proficiency ranges from 0 to 100: each point adds 1 percentage point to accuracy and 1% to weapon dice.", "help-note"),
    p(`<b>${lang === "pt" ? "Precisão final" : "Final accuracy"}</b>: ${lang === "pt" ? "limite entre 0% e 100% de (75% + proficiência + bônus de acerto − DEX do defensor)" : "Clamp (75% + proficiency + hit bonus − defender DEX) to 0%–100%."}<br><b>${lang === "pt" ? "Multiplicador dos dados" : "Weapon-dice multiplier"}</b>: 1 + (proficiency ÷ 100)`, "help-equation"),
    makeTable([lang === "pt" ? "Proficiência" : "Proficiency", lang === "pt" ? "Precisão base" : "Base accuracy", lang === "pt" ? "Multiplicador dos dados" : "Weapon-dice multiplier"], weaponValues.map((v) => [v, `${Math.min(100, weaponSkillAccuracy(v))}%`, `×${weaponSkillDamageMultiplier(v).toFixed(2)}`])),
    p(lang === "pt" ? "Exemplo: proficiência 20, bônus 0 e defensor com DEX 15 → 75 + 20 − 15 = 80% de chance. A precisão base pode passar de 100%; o limite final é aplicado depois da DEX." : "Example: proficiency 20, no hit bonus, defender DEX 15 → 75 + 20 − 15 = 80% chance. Base accuracy can exceed 100%; the final cap is applied after subtracting DEX."),
    `<h3>${lang === "pt" ? "Destreza (DEX)" : "Dexterity (DEX)"}</h3>`,
    p(lang === "pt" ? "Precisão contra você = chance do ataque − sua DEX, limitada entre 0% e 100%. Fuga = mínimo entre 100% e (60% + DEX ÷ 3)." : "Accuracy against you = attack chance − your DEX, clamped to 0%–100%. Escape = the lower of 100% and (60% + DEX ÷ 3).", "help-equation"),
    makeTable(["DEX", lang === "pt" ? "Chance de fuga" : "Escape chance"], dexValues.map((v) => [v, `${dexEscapeChance(v).toLocaleString(lang === "pt" ? "pt-BR" : "en-US", { maximumFractionDigits: 2 })}%`])),
    p(lang === "pt" ? "Fugir exige alcançar um hex da borda do campo. A chance usa a DEX de quem tenta fugir; falhar consome o turno." : "To flee, the active character must reach a battlefield edge hex. The chance uses the fleeing character's DEX; a failed attempt uses their turn."),
    `<h3>${lang === "pt" ? "Resistências elementais" : "Elemental resistances"}</h3>`,
    p(lang === "pt" ? "Resistência total = bônus da classe + perícia de resistência + bônus de equipamento. Resistência efetiva = resistência − penalidade do efeito − MAG do atacante ÷ 2, limitada entre −50% e 100%. Dano final = arredondar para baixo [dano base × (1 − resistência efetiva ÷ 100)]." : "Total resistance = class bonus + resistance skill + equipment bonus. Effective resistance = resistance − effect penalty − attacker MAG ÷ 2, clamped to −50%–100%. Final damage = floor [base damage × (1 − effective resistance ÷ 100)].", "help-equation"),
    makeTable([lang === "pt" ? "Perícia" : "Skill", lang === "pt" ? "Reduz este dano" : "Reduces this damage"], resistances.map((v) => [`${v} ${lang === "pt" ? "Resistência" : "Resistance"}`, v])),
    p(lang === "pt" ? "Cada ponto reduz 1 ponto percentual do dano daquele elemento. MAG 40 penetra 20 pontos. As resistências são independentes." : "Each point reduces damage from that element by 1 percentage point. MAG 40 penetrates 20 points. Resistances are independent."),
    `<h3>${lang === "pt" ? "Veneno" : "Poison"}</h3>`,
    p(lang === "pt" ? "Chance de aplicação depois de acertar = 100% − resistência efetiva, limitada entre 0% e 100%. O dano por turno é arredondado ao inteiro mais próximo após aplicar a resistência. O veneno causa dano no início do turno da vítima e permanece até ser curado." : "Application chance after a hit = 100% − effective resistance, clamped to 0%–100%. Tick damage is rounded to the nearest integer after resistance. Poison ticks at the start of the victim's turn and remains until cured."),
    `<h3>${lang === "pt" ? "Aprendizado das perícias" : "Skill progression"}</h3>`,
    p(lang === "pt" ? `Chance de ganhar perícia = (100 − valor atual) ÷ 100. O limite é 100; resistências ganham +${SKILL_GAIN} por sucesso.` : `Skill-gain chance = (100 − current value) ÷ 100. The cap is 100; resistances gain +${SKILL_GAIN} per successful check.`),
    makeTable([lang === "pt" ? "Valor atual" : "Current value", lang === "pt" ? "Chance de ganho" : "Gain chance"], skillValues.map((v) => [v, `${Math.round(skillGainChance(v) * 100)}%`])),
    p(lang === "pt" ? "Proficiência pode treinar ao atacar com aquela arma, inclusive quando erra. Resistências treinam ao usar ou sofrer dano do elemento. No treino de estrada, resistência recebe +0,1 a cada 12 horas; arma recebe +1 a cada 120 horas." : "Weapon proficiency can train by attacking with that weapon, including on a miss. Resistances train when you use or take damage from their element. On road training, resistance gains +0.1 every 12 hours; weapon proficiency gains +1 every 120 hours."),
  ].join("");
}

function damageTab(lang: HelpLanguage): string {
  const names = spells.filter(([id]) => SPELL_TIER[id as keyof typeof SPELL_TIER] !== undefined);
  const rows = names.map(([id, fallbackTier, ptClass, enClass]) => {
    const label = spellNames[id]?.[lang === "pt" ? 0 : 1] ?? id;
    const tier = SPELL_TIER[id as keyof typeof SPELL_TIER] ?? fallbackTier;
    return [label, lang === "pt" ? ptClass : enClass, `T${tier}`, spellDetails[id]?.[lang === "pt" ? 0 : 1] ?? "—"];
  });
  return p(lang === "pt" ? "Ataque normal = metade do ATK (ou MAG para conjurador) + dados da arma + terreno − metade da DEF do alvo. Arredonde as metades para baixo; o dano mínimo é 1." : "Normal attack = half ATK (or MAG for a caster) + weapon dice + terrain − half the target's DEF. Round halves down; minimum damage is 1.", "help-equation")
    + p(lang === "pt" ? "Magia usa a mesma conta, com metade da MAG multiplicada pelo peso da magia e os dados dela no lugar da arma. Resistência elemental é aplicada ao dano mágico." : "Spells use the same calculation, with half MAG multiplied by the spell's multiplier and its dice in place of the weapon. Elemental resistance is applied to magic damage.", "help-equation")
    + p(lang === "pt" ? "As fórmulas de dano e os efeitos abaixo vêm das habilidades atualmente disponíveis no jogo." : "The damage formulas and effects below cover the abilities currently available in the game.")
    + makeTable([lang === "pt" ? "Habilidade" : "Skill", lang === "pt" ? "Classe" : "Class", "Tier", lang === "pt" ? "Efeito" : "Effect"], rows);
}

function lootTab(lang: HelpLanguage): string {
  const total = Object.values(POTION_LOOT_WEIGHT).reduce((a, b) => a + b, 0);
  const potionRows = Object.entries(POTION_LOOT_WEIGHT).map(([id, weight]) => {
    const name = POTIONS[id as keyof typeof POTIONS].name;
    const translated: Record<string, string> = { "Poção Média": "Medium Potion", "Poção Fraca": "Weak Potion", "Poção De Cura Potente": "Potent Healing Potion", "Poção De Curar Doenças": "Disease Cure Potion", "Poção De Mana Pequena": "Small Mana Potion", "Poção De Mana Média": "Medium Mana Potion", "Poção De Mana Grande": "Large Mana Potion" };
    return [lang === "pt" ? name : (translated[name] ?? name), `${((weight / total) * 100).toFixed(0)}%`];
  });
  return p(lang === "pt" ? "Chances de drop atuais. Todo baú dá Gold, uma poção garantida sorteada pela tabela e uma chance separada de item. Se quem abriu já estiver no máximo daquela poção (5), ela passa para o próximo personagem que vai agir; se todos estiverem cheios, é descartada." : "Current drop chances. Every chest gives Gold, one guaranteed potion selected from the table, and a separate chance for an item. If the opener already has the maximum of that potion (5), it passes to the next unit to act; if everyone is full, it is discarded.")
    + `<h3>${lang === "pt" ? "Poções em baú (por baú)" : "Chest potions (per chest)"}</h3>`
    + makeTable([lang === "pt" ? "Poção" : "Potion", lang === "pt" ? "Chance" : "Chance"], potionRows)
    + `<h3>${lang === "pt" ? "Gold e item de baú" : "Chest gold and gear"}</h3>`
    + makeTable([lang === "pt" ? "Baú" : "Chest", lang === "pt" ? "Gold" : "Gold", lang === "pt" ? "Chance de equipamento" : "Gear chance"], [
      [lang === "pt" ? "Pequeno" : "Small", `${CHEST_LOOT.emberBase}–${CHEST_LOOT.emberBase + CHEST_LOOT.emberDice - 1}`, `${Math.round(CHEST_LOOT.gearChance * 100)}%`],
      [lang === "pt" ? "Médio" : "Medium", `${CHEST_LOOT.betterEmberBase}–${CHEST_LOOT.betterEmberBase + CHEST_LOOT.betterEmberDice - 1}`, `${Math.round(CHEST_LOOT.betterGearChance * 100)}%`],
      [lang === "pt" ? "Grande" : "Large", `${CHEST_LOOT.bestEmberBase}–${CHEST_LOOT.bestEmberBase + CHEST_LOOT.bestEmberDice - 1}`, `${Math.round(CHEST_LOOT.bestGearChance * 100)}%`],
    ])
    + `<h3>${lang === "pt" ? "Drop ao matar inimigo" : "Enemy kill drops"}</h3>`
    + makeTable([lang === "pt" ? "Inimigo" : "Enemy", lang === "pt" ? "Chance de arma/equipamento" : "Weapon/equipment chance"], [[lang === "pt" ? "Inimigo comum" : "Common enemy", `${(KILL_DROP_CHANCE * 100).toFixed(0)}%`], [lang === "pt" ? "Chefe nomeado" : "Named boss", lang === "pt" ? "Garantido" : "Guaranteed"]])
    + p(lang === "pt" ? "Armas e equipamentos largados são sorteados por preço (os mais caros são mais raros) e limitados ao nível de itens da missão atual." : "Dropped weapons and equipment are selected by price (more expensive gear is rarer) and capped by the current mission's item level.");
}

export function renderHelp(lang: HelpLanguage, tab: HelpTab): string {
  if (tab === "basics") return list(basics[lang]);
  if (tab === "uses") return usesTab(lang);
  if (tab === "damage") return damageTab(lang);
  if (tab === "formulas") return formulasTab(lang);
  return lootTab(lang);
}

export function helpLabel(tab: HelpTab, lang: HelpLanguage): string {
  const labels: Record<HelpTab, [string, string]> = {
    basics: ["Básicos", "Basics"], uses: ["Usos", "Uses"], damage: ["Dano", "Damage"], formulas: ["Fórmulas", "Formulas"], loot: ["Loot", "Loot"],
  };
  return labels[tab][lang === "pt" ? 0 : 1];
}
