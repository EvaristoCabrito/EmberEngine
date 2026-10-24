import type { ClassDef, DialogTree, Spawn } from "./types";

export const ENCOUNTER_NPC_IDS = ["roadCartographer","mushroomForager","bellCollector","mothKeeper","charcoalBurner","wanderingTinker","marshTrapper","ratCatcher","shadowScholar","swampFerryman","lostCourier"] as const;
export type EncounterNpcId = (typeof ENCOUNTER_NPC_IDS)[number];

export const ENCOUNTER_NPC_CLASSES = {
  roadCartographer: { id: "roadCartographer", name: "Nara, Cartógrafa das Trilhas", role: "Cartógrafa itinerante", hp: 12, atk: 1, mag: 0, def: 1, dex: 1, mov: 3, minRange: 1, maxRange: 1, sprite: "roadCartographer", size: 1, init: 8 },
  mushroomForager: { id: "mushroomForager", name: "Bento, Catador de Cogumelos", role: "Coletor desconfiado", hp: 12, atk: 1, mag: 0, def: 1, dex: 1, mov: 3, minRange: 1, maxRange: 1, sprite: "mushroomForager", size: 1, init: 8 },
  bellCollector: { id: "bellCollector", name: "Baltasar, Colecionador de Sinos", role: "Excêntrico — ouvinte do invisível", hp: 12, atk: 1, mag: 0, def: 1, dex: 1, mov: 3, minRange: 1, maxRange: 1, sprite: "bellCollector", size: 1, init: 8 },
  mothKeeper: { id: "mothKeeper", name: "Ofélia, Guardiã das Mariposas", role: "Excêntrica — guardiã noturna", hp: 12, atk: 1, mag: 0, def: 1, dex: 1, mov: 3, minRange: 1, maxRange: 1, sprite: "mothKeeper", size: 1, init: 8 },
  charcoalBurner: { id: "charcoalBurner", name: "Dário, Carvoeiro", role: "Trabalhador da floresta", hp: 12, atk: 1, mag: 0, def: 1, dex: 1, mov: 3, minRange: 1, maxRange: 1, sprite: "charcoalBurner", size: 1, init: 8 },
  wanderingTinker: { id: "wanderingTinker", name: "Ada, Funileira Ambulante", role: "Artesã de estrada", hp: 12, atk: 1, mag: 0, def: 1, dex: 1, mov: 3, minRange: 1, maxRange: 1, sprite: "wanderingTinker", size: 1, init: 8 },
  marshTrapper: { id: "marshTrapper", name: "Gaspar, Caçador dos Brejos", role: "Armadilheiro de estrada", hp: 12, atk: 1, mag: 0, def: 1, dex: 1, mov: 3, minRange: 1, maxRange: 1, sprite: "marshTrapper", size: 1, init: 8 },
  ratCatcher: { id: "ratCatcher", name: "Tereza, Caça-Ratos", role: "Excêntrica — exterminadora itinerante", hp: 12, atk: 1, mag: 0, def: 1, dex: 1, mov: 3, minRange: 1, maxRange: 1, sprite: "ratCatcher", size: 1, init: 8 },
  shadowScholar: { id: "shadowScholar", name: "Erasmo, Estudioso da Própria Sombra", role: "Excêntrico — erudito errante", hp: 12, atk: 1, mag: 0, def: 1, dex: 1, mov: 3, minRange: 1, maxRange: 1, sprite: "shadowScholar", size: 1, init: 8 },
  swampFerryman: { id: "swampFerryman", name: "Rúben, Barqueiro do Brejo", role: "Guia de travessias", hp: 12, atk: 1, mag: 0, def: 1, dex: 1, mov: 3, minRange: 1, maxRange: 1, sprite: "swampFerryman", size: 1, init: 8 },
  lostCourier: { id: "lostCourier", name: "Lia, Mensageira Perdida", role: "Mensageira obstinada", hp: 12, atk: 1, mag: 0, def: 1, dex: 1, mov: 3, minRange: 1, maxRange: 1, sprite: "lostCourier", size: 1, init: 8 },
} satisfies Record<EncounterNpcId, ClassDef>;

export const ENCOUNTER_NPC_GROWTH = {
  roadCartographer: { hp: 2, atk: 1, mag: 0, def: 1, dex: 1 },
  mushroomForager: { hp: 2, atk: 1, mag: 0, def: 1, dex: 1 },
  bellCollector: { hp: 2, atk: 1, mag: 0, def: 1, dex: 1 },
  mothKeeper: { hp: 2, atk: 1, mag: 0, def: 1, dex: 1 },
  charcoalBurner: { hp: 2, atk: 1, mag: 0, def: 1, dex: 1 },
  wanderingTinker: { hp: 2, atk: 1, mag: 0, def: 1, dex: 1 },
  marshTrapper: { hp: 2, atk: 1, mag: 0, def: 1, dex: 1 },
  ratCatcher: { hp: 2, atk: 1, mag: 0, def: 1, dex: 1 },
  shadowScholar: { hp: 2, atk: 1, mag: 0, def: 1, dex: 1 },
  swampFerryman: { hp: 2, atk: 1, mag: 0, def: 1, dex: 1 },
  lostCourier: { hp: 2, atk: 1, mag: 0, def: 1, dex: 1 },
};

const conversations: Record<EncounterNpcId, readonly string[]> = {
  "roadCartographer": [
    "Este mapa diz que há uma ponte adiante. A bússola discorda. Eu costumo confiar na que não cobra tinta.",
    "Por que a bússola discorda?",
    "Aqui a agulha segue alguma coisa enterrada. Uma ponte cai uma vez; uma direção errada derruba a viagem inteira.",
    "Qual caminho você recomenda?",
    "Pela margem baixa. Procure as pedras claras e mantenha o rio à direita. Se ouvir alguém chamar do mato, confira primeiro se trouxe companhia."
  ],
  "mushroomForager": [
    "Não toque nos roxos. Eles são ótimos para ver estrelas... mesmo ao meio-dia.",
    "Os outros são seguros?",
    "Os marrons, sim. Os brancos dependem. Dos que falam, eu não como nenhum: acho falta de educação.",
    "Você mora aqui?",
    "Morar é uma palavra forte. Tenho um teto de folhas e vizinhos que não pedem açúcar. Já tive coisa pior."
  ],
  "bellCollector": [
    "Silêncio. Este sino acabou de responder a uma pergunta que ninguém fez.",
    "O que ele disse?",
    "Que faltava uma voz no bosque. Talvez a minha. Talvez a sua. Prefiro não conferir sozinho.",
    "Você coleciona sinos?",
    "Coleciono os intervalos entre eles. Os sinos só ajudam a carregar."
  ],
  "mothKeeper": [
    "Elas não procuram a luz. Procuram o que a luz tenta esconder.",
    "Por que carrega mariposas?",
    "Cada uma voltou de uma casa vazia. Enquanto alguém se lembrar dos moradores, eu mantenho a porta aberta.",
    "Elas conhecem o caminho?",
    "Melhor que nós. Mas não peça para guiá-lo: algumas saudades levam longe demais."
  ],
  "charcoalBurner": [
    "O fogo faz o serviço. Eu só passo o dia convencendo a madeira a não discutir.",
    "Viu gente armada por aqui?",
    "Vi botas limpas entrando e botas ensanguentadas saindo. Gente honesta costuma deixar a lama fazer o trabalho primeiro.",
    "Como se ganha a vida aqui?",
    "Um saco de carvão de cada vez. A cidade quer calor, mas prefere esquecer de onde ele vem."
  ],
  "wanderingTinker": [
    "Se faz barulho, tem conserto. Quando fica quieto demais é que eu começo a cobrar por preocupação.",
    "Pode consertar essa chaleira?",
    "Posso. Ela perdeu uma alça e a dignidade, mas ainda guarda água melhor que muita gente guarda segredo.",
    "O que viu na estrada?",
    "Uma carroça sem roda, três mentiras bem vestidas e um homem negociando com uma árvore. A árvore parecia levar vantagem."
  ],
  "marshTrapper": [
    "Cuidado onde pisa. Minha armadilha pega coelho; a dos outros pega gente.",
    "Quem armou o caminho?",
    "Mãos apressadas. O nó foi feito para ser puxado de longe. Se encontrar corda atravessando a trilha, procure o dono antes de cortar.",
    "Caçou alguma coisa estranha?",
    "Uma vez segui pegadas que terminavam no alto de um pinheiro. Desde então só caço o que sei cozinhar."
  ],
  "ratCatcher": [
    "Esta gaiola está vazia. Não quer dizer que o serviço acabou.",
    "Há ratos aqui?",
    "Onde há saco de comida, há rato. Onde há palácio, há rato com título. Eu cobro igual.",
    "Por que olha tanto a gaiola?",
    "Ontem eu a fechei vazia. Hoje alguém roeu a tranca por dentro. Vai dizer que também não olharia?"
  ],
  "shadowScholar": [
    "Minha sombra está atrasada outra vez. Veja: eu já terminei este capítulo e ela ainda está na margem.",
    "Você está discutindo com a sombra?",
    "Estou corrigindo sua interpretação. É uma diferença acadêmica muito importante para quem convive com ela o dia inteiro.",
    "O livro fala sobre o quê?",
    "Sobre a distância entre o que vemos e o que entendemos. O autor morreu antes do último capítulo. Muito conveniente."
  ],
  "swampFerryman": [
    "A água parece rasa porque quer que você desça primeiro.",
    "Onde fica a travessia?",
    "Perto das pedras largas, ao sul. Solte a correia da mochila antes de entrar. Se a corrente puxar, deixe a carga ir.",
    "Você conhece este rio?",
    "Conheço o que ele mostra. O resto eu respeito. É assim que um barqueiro chega a velho."
  ],
  "lostCourier": [
    "A carta está seca. As botas não. Considero isso uma vitória profissional.",
    "Está perdida?",
    "Estou fora do caminho. Perdida seria se eu tivesse desistido. Há uma diferença, e ela é tudo o que tenho agora.",
    "Para quem é a carta?",
    "Para alguém que ainda está esperando. O selo não é meu para abrir; a promessa é minha para cumprir."
  ]
};

export function encounterNpcDialog(id: EncounterNpcId): DialogTree {
  const [start, question1, answer1, question2, answer2] = conversations[id];
  const speaker = ENCOUNTER_NPC_CLASSES[id].name.split(",")[0];
  return { id: `encounter-${id}`, startId: "start", lines: [
    { id: "start", speaker, portrait: id, text: start, replies: [{ text: question1, next: "answer1" }, { text: question2, next: "answer2" }, { text: "Boa viagem.", next: null }] },
    { id: "answer1", speaker, portrait: id, text: answer1 },
    { id: "answer2", speaker, portrait: id, text: answer2 },
  ] };
}

export function encounterNpcSpawn(id: EncounterNpcId, x: number, y: number): Spawn {
  return { name: ENCOUNTER_NPC_CLASSES[id].name.split(",")[0], classId: id, x, y, dialog: encounterNpcDialog(id) };
}
