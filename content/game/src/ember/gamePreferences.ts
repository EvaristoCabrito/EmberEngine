import { useSyncExternalStore } from "react";
export type GameLanguage = "pt" | "en";
export type Translations = Partial<Record<GameLanguage, string>>;
export interface GamePreferences { uiLanguage: GameLanguage; dialogueLanguage: GameLanguage; subtitleLanguage: GameLanguage; subtitles: boolean; dialogueScale: number; }
const KEY = "emberash:preferences:v1";
const defaults: GamePreferences = { uiLanguage: "pt", dialogueLanguage: "pt", subtitleLanguage: "pt", subtitles: true, dialogueScale: 1 };
let current = { ...defaults };
try {
 const saved = JSON.parse(localStorage.getItem(KEY) || "{}");
 for (const key of ["uiLanguage", "dialogueLanguage"] as const) if (saved[key] === "pt" || saved[key] === "en") current[key] = saved[key];
 if (saved.subtitleLanguage === "pt" || saved.subtitleLanguage === "en") current.subtitleLanguage = saved.subtitleLanguage;
 if (typeof saved.subtitles === "boolean") current.subtitles = saved.subtitles;
 if ([1, 1.15, 1.3].includes(saved.dialogueScale)) current.dialogueScale = saved.dialogueScale;
} catch { /* Defaults when storage is unavailable. */ }
const listeners = new Set<() => void>();
export const getGamePreferences = () => current;
export function setGamePreferences(patch: Partial<GamePreferences>) {
 current = { ...current, ...patch };
 try { localStorage.setItem(KEY, JSON.stringify(current)); } catch { /* Session still works. */ }
 listeners.forEach(listener => listener());
}
export function subscribeGamePreferences(listener: () => void) { listeners.add(listener); return () => { listeners.delete(listener); }; }
export function useGamePreferences() { return useSyncExternalStore(subscribeGamePreferences, getGamePreferences, getGamePreferences); }
/** Existing authored text remains the fallback until its translation is supplied. */
export function translatedText(original: string, translations: Translations | undefined, language: GameLanguage): string { return translations?.[language]?.trim() ? translations[language]! : original; }
const english: Record<string, string> = {
 "Personalizada": "Custom",
 "Oclusão ambiente sempre ativa. Suavidade das sombras é uma preferência visual independente.": "Ambient occlusion stays enabled. Shadow softness is an independent visual preference.",
 "Ajustes manuais personalizam o preset selecionado.": "Manual adjustments customize the selected preset.",
 "Poção de Vida": "Health Potion", "Poção de Mana": "Mana Potion", "Espada Curta": "Short Sword", "Espada Longa": "Long Sword", "Adaga": "Dagger", "Arco Curto": "Short Bow", "Arco Longo": "Long Bow",
 "Curar Doença Leve": "Cure Minor Disease", "Criar Comida e Água": "Create Food and Water", "Fôlego Renovado": "Second Wind", "Aura de Proteção": "Aura of Protection", "Presença Intimidante": "Intimidating Presence", "Ira Divina": "Divine Wrath", "Investida de Ombro": "Shoulder Smash", "Debandada": "Stampede", "Investida Perfurante": "Piercing Thrust", "Varredura": "Sweep", "Rasteira": "Trip", "Corte Duplo": "Double Strike", "Investida Touro": "Bull Rush", "Golpe do Carrasco": "Executioner Strike", "Golpe de Escudo": "Shield Bash", "Tiro Múltiplo": "Multi Shot", "Força Fantasmal": "Phantasmal Force", "Teia dos Sonhos": "Web of Dreams", "Invocar Familiar Maior": "Summon Greater Familiar", "Invocar Familiar Titã": "Summon Titan Familiar", "Invocar Familiar Radiante": "Summon Radiant Familiar", "Invocar Cão Zumbi": "Summon Zombie Hound",
 "Feiticeiro": "Sorcerer", "Cultista Ancestral": "Ancient Cultist", "Golem Ancião": "Elder Golem", "Lanceiro": "Lancer", "Conjurador": "Conjurer", "Cão Zumbi": "Zombie Hound", "Cavaleiro Pesado": "Heavy Knight", "Elementalista": "Elementalist", "Bruxo": "Warlock", "Arcanista": "Arcanist", "Bispo": "Bishop", "Patrulheiro": "Ranger", "Sentinela": "Sentinel", "Templário": "Templar",
 "Opções": "Options", "Fechar": "Close", "Gráficos": "Graphics", "Áudio": "Audio", "Idiomas": "Languages", "Acessibilidade": "Accessibility",
 "Qualidade gráfica": "Graphics quality", "Baixa": "Low", "Média": "Medium", "Alta": "High",
 "Sombras mais leves": "Lighter shadows", "FX atmosféricos": "Atmospheric FX", "Baixa: sem sombras ou FX atmosféricos. Média: sombras 2048 sem contato. Alta: sombras 4096 com contato.": "Low: no shadows or atmospheric FX. Medium: 2048 shadows without contact shadows. High: 4096 shadows with contact shadows.",
 "Ajusta resolução, sombras e iluminação. Mantém os modelos e as regras do jogo.": "Adjusts resolution, shadows and lighting. Keeps models and gameplay rules unchanged.",
 "Nova campanha": "New campaign", "Continuar": "Continue", "Como jogar": "How to play", "Carregando…": "Loading…", "Modo teste": "Test mode", "Táticas em cinzas": "Tactics in ashes",
 "Básicos": "Basics", "Usos": "Uses", "Dano": "Damage", "Loot": "Loot", "Toque numa aliada para ver movimento (azul) e ataque (vermelho).": "Select an ally to see movement (blue) and attack (red) ranges.",
 "Toque num inimigo para ver HP, alcance e a área vermelha de perigo.": "Select an enemy to see HP, range, and its red danger zone.", "Golpe de arma: AT − DF, dentro do alcance da ficha.": "Weapon attack: AT − DF, within the range shown on the unit card.",
 "Magia ofensiva: dano dos dados reduzido pela resistência elemental, no alcance da magia.": "Offensive spell: dice damage reduced by elemental resistance, within the spell's range.", "Todo mundo tem AT, MAG, DF, DEX, Mov e Alc. Nada fica de fora da ficha.": "Every unit has AT, MAG, DF, DEX, Move, and Range. All stats are shown on its card.",
 "Terreno alto (barranco, tronco morto, casa abandonada): +2 de dano. A arqueira também ganha +1 de alcance. No alto, outro hex alto na frente não corta a flecha.": "High ground (ridge, dead tree, abandoned house): +2 damage. The archer also gains +1 range. From high ground, another high hex ahead does not block arrows.",
 "Barricada (estacas, 3 hexes): ninguém passa. De trás você atira. Projéteis não acertam quem está atrás.": "Barricade (three hexes of stakes): units cannot pass through it. You can shoot from behind it, and projectiles do not hit units behind it.", "Depois de mover, dois cliques no personagem = Esperar e passa ao próximo.": "After moving, click the unit twice to Wait and pass to the next unit.",
 "Cada classe tem uma velocidade de conjuração — ela decide quantos usos de cada tier (1 a 5) a classe tem em cada nível. As tabelas abaixo mostram os números exatos, nível a nível.": "Each class has a casting speed, which determines how many uses of each tier (1 to 5) it gets at each level. The tables below show the exact numbers for every level.",
 "Conjuração Rápida": "Fast Casting", "Conjuração Média": "Medium Casting", "Conjuração Lenta": "Slow Casting", "Nv": "Lv", "Ataque normal": "Normal attack", "metade do seu ATK (ou MAG, se for conjurador) + dados da arma + terreno − metade da DEF do alvo (resistência elemental, contra magia). Metades não contam: arredonda pra baixo. Mínimo 1 de dano.": "half your ATK (or MAG for a caster) + weapon dice + terrain − half the target's DEF (elemental resistance against magic). Fractions round down. Minimum 1 damage.",
 "Magia": "Spell", "a mesma conta, com a sua metade de MAG multiplicada pelo peso da magia e os dados dela no lugar da arma. Todo peso é maior que 1, e o resultado nunca fica abaixo de um ataque normal — conjurar sempre vale mais que bater.": "uses the same calculation, with half your MAG multiplied by the spell's multiplier and its dice in place of the weapon dice. Every multiplier is greater than 1, and the result is never lower than a normal attack.",
 "Por isso a tabela abaixo mostra a fórmula por MAG, não por nível: uma magia cresce junto com quem conjura, não numa tabela própria.": "The table below shows the formula by MAG rather than level: a spell scales with its caster instead of using its own level table.",
 "Habilidade": "Skill", "Classe": "Class", "Fórmula": "Formula", "Efeito": "Effect", "Nv1": "Lv 1", "Nv7": "Lv 7", "Nv14": "Lv 14", "MAG 10": "MAG 10", "MAG 20": "MAG 20", "MAG 40": "MAG 40",
 "Chances de drop, do jeito que estão programadas agora.": "Current item drop chances.", "Poções em baú (por baú)": "Chest potions (per chest)", "Todo baú dá Gold + uma poção garantida (sorteada abaixo) + uma chance separada de item. Se quem abriu já estiver no máximo daquela poção (5), ela passa para o próximo personagem que vai agir; se todos estiverem cheios, é descartada.": "Every chest gives Gold, one guaranteed potion (selected below), and a separate chance for an item. If the opener already has the maximum of that potion (5), it goes to the next unit to act; if everyone is full, it is discarded.",
 "Poção": "Potion", "Chance": "Chance", "Gold:": "Gold:", "por baú.": "per chest.", "Gold e item de baú": "Chest gold and items", "Chance extra de arma ou equipamento:": "Extra chance of a weapon or equipment:", "Drop ao matar inimigo": "Enemy kill drops", "Inimigo comum:": "Common enemy:", "de chance de largar uma arma.": "chance to drop a weapon.", "Chefes nomeados (drop garantido): sempre largam arma ou equipamento ao morrer.": "Named bosses (guaranteed drop) always drop a weapon or equipment when killed.", "Toda arma/equipamento largado é sorteado por preço — quanto mais caro, mais raro — e limitado ao nível de itens da missão atual, então cada trecho da campanha só solta o que faz sentido pra ele.": "Dropped weapons and equipment are selected by price—the more expensive, the rarer—and capped by the current mission's item level, so each part of the campaign only drops gear that fits.", "Entendi": "Got it",
 "Seis sobreviventes. Um tabuleiro de guerra. Cada casa conta.": "Six survivors. A battlefield. Every square counts.", "Próximo": "Next", "Pular": "Skip", "Deite o telefone": "Rotate your phone",
 "Ativar som": "Enable sound", "Silenciar": "Mute", "Música": "Music", "Efeitos sonoros": "Sound effects", "Vídeos": "Videos",
 "Sombras": "Shadows", "Sombras suaves": "Soft shadows", "Sombras de contato": "Contact shadows", "Oclusão ambiente": "Ambient occlusion", "Luzes locais": "Local lights", "Tela cheia": "Fullscreen",
 "Interface e nomes": "Interface and names", "Diálogos": "Dialogue", "Legendas": "Subtitles", "Idioma das legendas": "Subtitle language", "Português (Brasil)": "Portuguese (Brazil)", "Exibir legendas": "Show subtitles", "Tamanho do diálogo": "Dialogue text size", "Normal": "Normal", "Grande": "Large", "Muito grande": "Extra large",
 "Traduções ausentes usam o texto original. As escolhas são independentes.": "Missing translations use the original text. Each language choice is independent.",
 "Alterações salvas automaticamente.": "Changes are saved automatically.", "Tela cheia indisponível neste navegador.": "Fullscreen is unavailable in this browser.",
 "Bola De Fogo": "Fireball", "Míssil Mágico": "Magic Missile", "Cura Menor": "Minor Heal", "Cura Média": "Medium Heal", "Cura Leve": "Light Heal", "Relâmpago": "Lightning", "Choque": "Shock", "Dreno de Vida": "Life Drain", "Mãos Flamejantes": "Burning Hands", "Tiro Longo": "Long Shot", "Tiro Perfurante": "Piercing Shot", "Invocar Familiar": "Summon Familiar", "Veneno Cáustico": "Caustic Venom", "Veneno Menor": "Minor Venom",
 "Guerreiro": "Warrior", "Arqueira": "Archer", "Mago Negro": "Black Mage", "Curandeiro": "Healer", "Soldado": "Soldier", "Piqueiro": "Pikeman", "Piqueiro Base": "Base Pikeman", "Besteiro Base": "Base Crossbowman", "Besteiro Salteador": "Brigand Crossbowman", "Besteiro": "Crossbowman", "Capitão": "Captain", "Zumbi": "Zombie", "Boi Morto-vivo": "Undead Ox", "Troll da caverna": "Cave Troll", "Cão de guerra": "War Dog", "Paladino": "Paladin", "Clérigo": "Cleric", "Ladino": "Rogue", "Assassino": "Assassin", "Necromante": "Necromancer"
};
/** Presentation only: never changes IDs, authored names, saves or combat rules. */
export function uiText(text: string, translations?: Translations): string { return translatedText(current.uiLanguage === "en" ? english[text] ?? text : text, translations, current.uiLanguage); }
