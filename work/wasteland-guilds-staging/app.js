const GUILD_FORMS = {
  moonwarden: [['Guerreiro de machado e escudo'], ['Lobisomem de machado e escudo'], ['Grande Lobo']],
  vanguard: [['Vanguard'], ['Swordsmaster', 'WarBringer']],
};

const BUILDINGS = [
  { id: 'farm', name: 'Fazenda', glyph: 'F', cost: 115, wood: 6, detail: 'Alho, trigo, suínos, ovos, leite ou hortaliças. Escolha o que plantar e melhore até o nível 3.', gold: 0.55, grain: 0.225, people: 0 },
  { id: 'mill', name: 'Moinho', glyph: 'M', cost: 155, wood: 10, detail: 'Mói o trigo em farinha. Trigo cru não alimenta ninguém. As pás giram; a vila, nem tanto.', gold: 0.4, grain: 0, people: 0 },
  { id: 'bakery', name: 'Padaria', glyph: 'P', cost: 165, wood: 10, detail: 'Assa pão, queijo ou quiche lorraine com o que tem no depósito. Você escolhe a fornada; o forno não discute. Melhora até o nível 3.', gold: 0.4, grain: 0, people: 0 },
  { id: 'emberLab', name: 'Laboratório de Ember', glyph: 'E', cost: 220, wood: 14, detail: 'Refina em Ember os Dead Remains que os heróis trazem dos covis. Quanto melhor a grade, mais Ember. Ninguém pergunta de onde vieram.', gold: 0, grain: 0, people: 0 },
  { id: 'butcher', name: 'Açougue', glyph: 'A', cost: 190, wood: 12, detail: 'Transforma suínos, frango e carne de vaca em alimentos. Melhora até o nível 2.', gold: 0, grain: 0, people: 0 },
  { id: 'hut', name: 'Casebre Pequeno', glyph: 'C', cost: 60, wood: 4, detail: 'Moradia para 2 camponeses. Amplia para Casebre Grande, com 5. Eles não tinham pra onde ir mesmo.', gold: 0, grain: 0, people: 0, housing: 2 },
  { id: 'depot', name: 'Depósito de Provisões', glyph: 'D', cost: 70, wood: 6, detail: 'Guarda mais 30 unidades de comida, armas e itens. Amplia para 45, 70 e 100. Os ratos aprovam.', gold: 0, grain: 0, people: 0 },
  { id: 'lumber', name: 'Cabana do Lenhador', glyph: 'L', cost: 80, wood: 2, detail: 'Coloque de 1 a 3 camponeses para cortar lenha: 0,05, 0,15 ou 0,25 de madeira por segundo.', gold: 0, grain: 0, people: 0 },
  { id: 'quarry', name: 'Pedreira', glyph: 'P', cost: 95, wood: 6, detail: 'Dois trabalhadores cortam pedra de uma reserva finita; nível 2 amplia a equipe e o estoque.', gold: 0, grain: 0, people: 0 },
  { id: 'mine', name: 'Campo de Mineração', glyph: 'M', cost: 120, wood: 8, detail: 'Dois trabalhadores extraem rocha bruta de um veio escolhido e entregam cargas à City.', gold: 0, grain: 0, people: 0 },
  { id: 'blacksmith', name: 'Blacksmith', glyph: 'F', cost: 260, wood: 18, detail: 'Processa hematita, malaquita, galena e quartzo; nível 2 libera aço cementado.', gold: 0, grain: 0, people: 0 },
  { id: 'market', name: 'Mercado', glyph: 'M', cost: 100, wood: 6, detail: 'Venda o que sobra e compre o que falta. Os mercadores cobram o dobro pra vender e pagam a metade pra comprar.', gold: 0, grain: 0, people: 0 },
  { id: 'tavern', name: 'Taverna', glyph: 'T', cost: 145, wood: 9, detail: 'Uma cama para um herói. Também recebe visitantes e serve bebida, que rende imposto.', gold: 1.15, grain: 0, people: 0 },
  { id: 'hotel', name: 'Hotel de Estrada', glyph: 'H', cost: 170, wood: 11, detail: 'Mais 4 camas para heróis. Os lençóis são trocados uma vez por viúva.', gold: 0.85, grain: 0, people: 0 },
  { id: 'warrior', name: 'Guilda de Guerreiros', glyph: 'W', cost: 185, wood: 11, detail: 'Recruta Warriors. Seguram a linha de frente na força.', heroClass: 'Warrior', gold: 0.3, grain: 0, people: 1 },
  { id: 'archer', name: 'Salão de Arqueiros', glyph: 'A', cost: 170, wood: 10, detail: 'Recruta Archers. Decidem a luta de longe.', heroClass: 'Archer', gold: 0.3, grain: 0, people: 1 },
  { id: 'lancer', name: 'Pátio dos Lanceiros', glyph: 'L', cost: 175, wood: 11, detail: 'Recruta Lancers. A lança chega antes da espada.', heroClass: 'Lancer', gold: 0.3, grain: 0, people: 1 },
  { id: 'blackMage', name: 'Torre dos Magos', glyph: 'M', cost: 210, wood: 12, detail: 'Recruta Black Mages. Magia que explode em área. Eles também morrem fácil.', heroClass: 'Black Mage', gold: 0.2, grain: 0, people: 1 },
  { id: 'conjurer', name: 'Casa dos Conjuradores', glyph: 'C', cost: 200, wood: 12, detail: 'Recruta Conjurers. Chegam sozinhos e lutam acompanhados.', heroClass: 'Conjurer', gold: 0.2, grain: 0, people: 1 },
  { id: 'healer', name: 'Guilda de Curandeiros', glyph: 'H', cost: 180, wood: 10, detail: 'Recruta Healers. Mantêm os heróis vivos. Por um tempo.', heroClass: 'Healer', gold: 0.25, grain: 0, people: 1 },
  { id: 'moonwarden', name: 'Guilda MoonWarden', glyph: 'M', cost: 185, wood: 11, detail: 'Seu morador evolui de guerreiro a lobisomem e, por fim, a Grande Lobo.', heroClass: 'MoonWarden', gold: 0.3, grain: 0, people: 1 },
  { id: 'vanguard', name: 'Guilda dos Vanguards', glyph: 'V', cost: 185, wood: 11, detail: 'Seu Vanguard escolhe entre Swordsmaster e WarBringer no nível 2.', heroClass: 'Vanguard', gold: 0.3, grain: 0, people: 1 },
  { id: 'armory', name: 'Arsenal do Domínio', glyph: 'E', cost: 165, wood: 9, detail: 'Heróis e camponeses compram equipamento aqui com o próprio ouro. O ouro vem pro cofre.', gold: 0.15, grain: 0, people: 0 },
];
// Toda construção consome pedra adicional, calculada a partir da madeira necessária.
const buildingStoneCost = item => Math.ceil(item.wood / 2);
BUILDINGS.find(building => building.id === 'lumber').detail += ' Pode ser construida em City ou Wilds.';
BUILDINGS.find(building => building.id === 'quarry').detail += ' O prédio é o local de extração. City permite uma pedreira de baixa reserva; Wilds não tem limite.';
BUILDINGS.find(building => building.id === 'mine').detail += ' Apenas em Wilds, no deposito de metais marcado.';
const FARM_TYPES = [
  // grain: produção por segundo nos níveis 1, 2 e 3 da fazenda.
  { id: 'garlic', name: 'Alho', art: 'farm-garlic', icon: 'icon-market-garlic-photo-v01.png', grain: [0.075, 0.125, 0.2], gold: 0.9 },
  { id: 'wheat', name: 'Trigo', art: 'farm-wheat-v02', icon: 'icon-market-wheat-photo-v01.png', grain: [0.1125, 0.1875, 0.3], gold: 0.55 },
  { id: 'bacon', name: 'Suínos', foodId: 'pork', art: 'farm-bacon', icon: 'farm-pigs-animal-v01.png', grain: [0.005, 0.0083, 0.0132], gold: 0.75 },
  { id: 'eggs', name: 'Galinheiro', foodId: 'eggs', secondary: 'chicken', art: 'farm-eggs', icon: 'farm-chickens-animal-v01.png', grain: [0.1, 0.166, 0.264], gold: 0.65 },
  { id: 'milk', name: 'Leite e carne de vaca', foodId: 'milk', secondary: 'beef', art: 'farm-milk', icon: 'farm-cow-animal-v01.png', grain: [0.1025, 0.1725, 0.275], gold: 0.6 },
  { id: 'potato', name: 'Batatas', art: 'farm-vegetables', icon: 'potato-v01.png', grain: [0.075, 0.125, 0.2], gold: 0.7, group: 'vegetables' },
  { id: 'carrot', name: 'Cenouras', art: 'farm-vegetables', icon: 'carrot-v01.png', grain: [0.075, 0.125, 0.2], gold: 0.7, group: 'vegetables' },
  { id: 'tomato', name: 'Tomates', art: 'farm-vegetables', icon: 'tomato-v01.png', grain: [0.075, 0.125, 0.2], gold: 0.7, group: 'vegetables' },
];
const FARM_OUTPUT_MULTIPLIER = 2;
const farmType = id => FARM_TYPES.find(type => type.id === id) || FARM_TYPES[1];
const FARM_ANIMAL_RATE = 0.1; // produção secundária de carne nas fazendas de ovos e leite
const farmFoodId = building => farmType(building.produce).foodId || farmType(building.produce).id;
const farmSecondaryRate = building => farmType(building.produce).secondary ? farmOutput(building) * FARM_ANIMAL_RATE : 0;
const FISHING_RATE = 0.035; // peixe por segundo por pescador
const WILDLIFE_MAP_SET = 1; // os Wilds acessíveis no fluxo atual do jogo
const FISHING_SPOTS = [
  { site: 'wilds-stream-west-map01', type: 'fishing', name: 'Riacho das Wilds · trecho oeste', mapId: 'wilds', mapSet: 1, x: 17, y: 68, stock: 0 },
  { site: 'wilds-stream-east-map01', type: 'fishing', name: 'Riacho das Wilds · trecho leste', mapId: 'wilds', mapSet: 1, x: 47, y: 59, stock: 0 },
  { site: 'wilds-stream-north', type: 'fishing', name: 'Riacho das Wilds · trecho norte', mapId: 'wilds', mapSet: 2, x: 25, y: 42, stock: 0 },
  { site: 'wilds-stream-south', type: 'fishing', name: 'Riacho das Wilds · trecho sul', mapId: 'wilds', mapSet: 2, x: 68, y: 67, stock: 0 },
];
const fishingStock = spot => state.fishingStock?.[spot.site] || 0;
const WILDLIFE_SPAWN_MAX = 5; // até cinco animais novos por dia; pode sair zero
const WILDLIFE_STAY_CAP = 12; // limite de animais vivos nas Wilds
const WILDLIFE_STATS = { boar: { hp: 75, atk: 9, mag: 0, def: 5, dex: 9 }, elk: { hp: 110, atk: 12, mag: 0, def: 7, dex: 7 } };
const WILDLIFE_MEAT = { boar: 'boarMeat', elk: 'elkMeat' };
const WILDLIFE_NAMES = { boar: 'Javali', elk: 'Alce' };
const WILDLIFE_ART = { boar: 'wild-boar', elk: 'elk' };
const HUNTING_SKILL_BONUS = 0.5; // cada ponto dá 0,5% mais dano ao caçar
const HUNT_MEAT_DROP = { boar: [2, 4], elk: [3, 5] }; // mínimo e máximo de carne por caça
// A colheita fica na fazenda (até FARM_STOCK_CAP) até um camponês carregar CARRY_LOAD por viagem para o depósito.
const CARRY_LOAD = 5;
const FARM_STOCK_CAP = 10;
// Indústrias (moinho, padaria e açougue) acumulam 30 produtos no nível 1, 50 no nível 2 e 70 no nível 3.
const INDUSTRY_STOCK_CAP = [30, 50, 70];
const industryCap = building => INDUSTRY_STOCK_CAP[Math.min(building?.level || 0, INDUSTRY_STOCK_CAP.length - 1)];
// Comida é leve: qualquer alimento (trigo, farinha, pão, carne, peixe) vai 10 por viagem; a habilidade de carga sobe isso
// até 30 no nível máximo (100), e o equipamento de carga soma, sem passar de 30. Pedra, minério e metal seguem carryLoad.
const FOOD_LOAD = 10;
const FOOD_LOAD_MAX = 30;
const foodLoad = actor => Math.min(FOOD_LOAD_MAX, FOOD_LOAD + Math.floor(skillOf(actor, 'hauling') / 100 * (FOOD_LOAD_MAX - FOOD_LOAD)) + gearBonus(actor, 'carry'));
const MANOR_DOOR = { x: 42, y: 39 };
// Fazendas e moinhos sobem até o nível 3. O moinho melhorado mói mais rápido, sem exagero.
const PRODUCER_LEVELS = [{ gold: 0, wood: 0 }, { gold: 180, wood: 12 }, { gold: 320, wood: 20 }];
const MILL_RATE = [0.225, 0.3, 0.4]; // trigo moído em farinha por segundo, por nível
// Padaria: o camponês busca os ingredientes no depósito, o forno transforma 1 de cada em 1 do produto
// e ele leva a fornada de volta. O produto alimenta o mesmo que os ingredientes somados e ocupa menos espaço.
const BAKERY_RECIPES = [
  { id: 'bread', name: 'Pão', needs: ['flour'] },
  { id: 'cheese', name: 'Queijo', needs: ['flour', 'milk'] },
  { id: 'quiche', name: 'Quiche Lorraine', needs: ['flour', 'milk', 'bacon'] },
];
const BAKE_RATE = [0.15, 0.2, 0.27]; // fornadas por segundo, por nível
const BUTCHER_RATE = [0.12, 0.16]; // unidades processadas por segundo nos níveis 1 e 2
const BUTCHER_MAX_LEVEL = 2;
const BUTCHER_RECIPES = [
  { id: 'bacon', name: 'Bacon', needs: ['pork'], feeds: 1, art: 'farm-bacon', icon: 'icon-market-bacon-photo-v01.png' },
  { id: 'sausage', name: 'Linguiça', needs: ['pork'], feeds: 1, art: 'sausage' },
  { id: 'nuggets', name: 'Chicken Nuggets', needs: ['chicken'], feeds: 1, art: 'chicken-nuggets' },
  { id: 'wings', name: 'Buffalo Wings', needs: ['chicken'], feeds: 1, art: 'buffalo-wings' },
  { id: 'hamburger', name: 'Hambúrguer', needs: ['beef'], feeds: 1, art: 'hamburger' },
  { id: 'steak', name: 'Filé', needs: ['beef'], feeds: 1, art: 'steak' },
  { id: 'parmesanSteak', name: 'Filé à Parmegiana', needs: ['beef', 'cheese'], feeds: 3, art: 'parmesan-steak' },
  { id: 'roastBoar', name: 'Javali Assado', needs: ['boarMeat'], feeds: 2, art: 'roast-boar', icon: 'roast-boar-v01.png' },
  { id: 'garlicBoar', name: 'Javali ao Alho', needs: ['boarMeat', 'garlic'], feeds: 4, art: 'garlic-boar', icon: 'garlic-boar-v01.png' },
  { id: 'roastElk', name: 'Alce Assado', needs: ['elkMeat'], feeds: 2, art: 'roast-elk', icon: 'roast-elk-v01.png' },
  { id: 'elkStew', name: 'Ensopado de Alce', needs: ['elkMeat', 'milk', 'potato'], feeds: 4, art: 'elk-stew', icon: 'elk-stew-v01.png' },
];
const BUTCHER_ART = Object.fromEntries(BUTCHER_RECIPES.map(recipe => [recipe.id, recipe.art]));
const butcherRecipe = id => BUTCHER_RECIPES.find(recipe => recipe.id === id) || BUTCHER_RECIPES[0];
const butcherActiveRecipe = butcher => butcherRecipe(butcher?.produce);
const butcherRate = building => BUTCHER_RATE[Math.min(BUTCHER_RATE.length - 1, producerLevel(building))];
const BAKED_ART = { bread: 'bread', cheese: 'cheese', quiche: 'quiche' };
const bakeryRecipe = id => BAKERY_RECIPES.find(recipe => recipe.id === id) || BAKERY_RECIPES[0];
// Construções com nível de produção (melhoria até o nível 3) e camponês fixo.
const PRODUCER_TYPES = ['farm', 'mill', 'bakery', 'emberLab', 'butcher', 'blacksmith'];
// Laboratório de Ember: Dead Remains refinados por segundo, por nível. Cada grade rende um tanto de Ember.
const LAB_RATE = [0.025, 0.035, 0.05];
const labRate = building => LAB_RATE[producerLevel(building)];
const BLACKSMITH_RATE = [0.025, 0.05]; // lotes refinados por segundo, níveis 1 e 2
const BLACKSMITH_RECIPES = [
  { id: 'iron', name: 'Reduzir hematita e consolidar ferro', needs: { ironOre: 20 }, makes: { iron: 4.4625 }, fuel: 6, work: 120, minLevel: 1, assay: 350000, recovery: .75 * .85, residue: 'slag' },
  { id: 'copper', name: 'Reduzir malaquita e fundir cobre', needs: { copperOre: 20 }, makes: { copper: .476 }, fuel: 3.5, work: 80, minLevel: 1, assay: 35000, recovery: .8 * .85, residue: 'wasteRock' },
  { id: 'arsenicalCopper', name: 'Processar cobre arsenical regional', needs: { arsenicalCopperOre: 20 }, makes: { arsenicalCopper: .476 }, fuel: 4, work: 90, minLevel: 1, assay: 35000, recovery: .8 * .85, residue: 'wasteRock' },
  { id: 'nativeCopper', name: 'Separar e preparar cobre nativo', needs: { nativeCopper: 1 }, makes: { copper: .9 }, fuel: 0, work: 35, minLevel: 1 },
  { id: 'tin', name: 'Reduzir cassiterita e fundir estanho', needs: { tinOre: 20 }, makes: { tin: .476 }, fuel: 4, work: 90, minLevel: 1, assay: 150000, recovery: .8 * .85, residue: 'wasteRock' },
  { id: 'bronze', name: 'Fundir lote de bronze', needs: { copper: 4, tin: 1 }, makes: { bronze: 5 }, fuel: 2, work: 70, minLevel: 1 },
  { id: 'silver', name: 'Fundir galena e cupelar prata', needs: { silverOre: 40 }, makes: { silver: .0144 }, fuel: 5, work: 110, minLevel: 1, assay: 500, recovery: .8 * .9, residue: 'leadOxide' },
  { id: 'gold', name: 'Lavar quartzo e fundir ouro', needs: { goldOre: 100 }, makes: { gold: .0008232 }, fuel: .25, work: 80, water: true, minLevel: 1, assay: 12, recovery: .7 * .98, residue: 'wasteRock' },
  { id: 'castIron', name: 'Fundir ferro de alto carbono', needs: { ironOre: 40 }, makes: { castIron: .35 }, fuel: 8, work: 180, minLevel: 2, assay: 350000, recovery: .55, residue: 'slag' },
  { id: 'steel', name: 'Cementar lâminas de ferro', needs: { iron: 1 }, makes: { steel: .9 }, fuel: .5, work: 45, minLevel: 2, residue: 'slag' },
  { id: 'crucibleSteel', name: 'Fundir aço de cadinho', needs: { iron: 4 }, makes: { crucibleSteel: 3 }, fuel: 1, work: 120, minLevel: 2, residue: 'slag' },
  { id: 'meteoriteIron', name: 'Preparar fragmentos de ferro meteórico', needs: { meteoriteFragments: 1 }, makes: { meteoriteIron: .8 }, fuel: 0, work: 45, minLevel: 2 },
  { id: 'charcoal', name: 'Queimar carvão vegetal', needs: { timber: 10 }, makes: { charcoal: 2 }, fuel: 0, work: 60, minLevel: 1 },
];
const METAL_STOCK = ['ironOre', 'copperOre', 'arsenicalCopperOre', 'tinOre', 'nativeCopper', 'meteoriteFragments', 'silverOre', 'goldOre', 'iron', 'steel', 'crucibleSteel', 'castIron', 'copper', 'arsenicalCopper', 'tin', 'bronze', 'meteoriteIron', 'silver', 'gold'];
const ORE_STOCK = new Set(['ironOre', 'copperOre', 'arsenicalCopperOre', 'tinOre', 'nativeCopper', 'meteoriteFragments', 'silverOre', 'goldOre']);
const METAL_NAMES = { ironOre: 'Rocha hematítica (kg)', copperOre: 'Rocha malaquítica (kg)', arsenicalCopperOre: 'Minério regional de cobre arsenical (kg)', tinOre: 'Cassiterita (kg)', nativeCopper: 'Fragmentos de cobre nativo (kg)', meteoriteFragments: 'Fragmentos de meteorito (kg)', silverOre: 'Galena argentífera (kg)', goldOre: 'Quartzo aurífero (kg)', iron: 'Ferro trabalhado (kg)', steel: 'Aço para corte (kg)', crucibleSteel: 'Aço de cadinho (kg)', castIron: 'Ferro fundido (kg)', copper: 'Cobre (kg)', arsenicalCopper: 'Cobre arsenical (kg)', tin: 'Estanho (kg)', bronze: 'Bronze (kg)', meteoriteIron: 'Ferro meteórico (kg)', silver: 'Prata (kg)', gold: 'Ouro (kg)', charcoal: 'Carvão vegetal (kg)', timber: 'Madeira' };
const metalStockTotal = () => METAL_STOCK.reduce((sum, id) => sum + (state.metals?.[id] || 0), 0);
const metalStorageUnits = () => metalStockTotal() / 100;
const blacksmithRate = building => BLACKSMITH_RATE[producerLevel(building)];
const blacksmithRecipe = building => BLACKSMITH_RECIPES.find(recipe => recipe.id === building?.produce && recipe.minLevel <= (producerLevel(building) + 1)) || BLACKSMITH_RECIPES[0];
// Arte por nível: quando houver desenho do upgrade, liste aqui. Ex.: 'farm-garlic': [null, 'farm-garlic-l2', 'farm-garlic-l3']
const LEVEL_ART = {
  'farm-garlic': [null, 'farm-garlic-l2', 'farm-garlic-l3'],
  'farm-wheat-v02': [null, 'farm-wheat-v02-l2', 'farm-wheat-v02-l3'],
  'farm-bacon': [null, 'farm-bacon-l2', 'farm-bacon-l3'],
  'farm-eggs': [null, 'farm-eggs-l2', 'farm-eggs-l3'],
  'farm-milk': [null, 'farm-milk-l2', 'farm-milk-l3'],
  'farm-vegetables': [null, 'farm-vegetables-l2', 'farm-vegetables-l3'],
  'mill-body': [null, 'mill-body-l2', 'mill-body-l3'],
  bakery: [null, 'bakery-l2', 'bakery-l3'],
  'ember-lab': [null, 'ember-lab-l2', 'ember-lab-l3'],
  butcher: [null, 'butcher-l2'], blacksmith: [null, 'blacksmith-l2'],
};
const producerLevel = building => Math.min(building?.type === 'blacksmith' ? BLACKSMITH_RATE.length - 1 : PRODUCER_LEVELS.length - 1, building?.level || 0);
const farmOutput = building => farmType(building.produce).grain[producerLevel(building)] * FARM_OUTPUT_MULTIPLIER;
const millRate = building => MILL_RATE[producerLevel(building)];
const bakeRate = building => BAKE_RATE[producerLevel(building)];
const levelArt = (art, building) => LEVEL_ART[art]?.[producerLevel(building)] || art;
const bakeryActiveRecipe = bakery => {
  const recipe = bakeryRecipe(bakery?.produce);
  return recipe.id === 'quiche' && producerLevel(bakery) < 1 ? bakeryRecipe('bread') : recipe;
};
const rateNumber = value => value.toFixed(2).replace('.', ',');

const PRESTIGE = {
  Warrior: ['Paladin', 'Heavy Knight'], Archer: ['Ranger', 'Assassin'], Lancer: ['Sentinel', 'Templar'],
  'Black Mage': ['Elementalist', 'Warlock'], Conjurer: ['Sorcerer', 'Necromancer'], Healer: ['Bishop', 'Cleric'],
};
const GUILD_FOR = { Warrior: 'warrior', Archer: 'archer', Lancer: 'lancer', 'Black Mage': 'blackMage', Conjurer: 'conjurer', Healer: 'healer' };
const CLASS_ART = { Warrior: 'warrior', Archer: 'archer', Lancer: 'lancer', 'Black Mage': 'black-mage', Conjurer: 'conjurer', Healer: 'healer', Peasant: 'peasant' };
const PORTRAIT_ART = { Warrior: 'warrior', Archer: 'archer', Lancer: 'lancer', 'Black Mage': 'black-mage', Conjurer: 'conjurer', Healer: 'healer' };
const PRESTIGE_ART = { Paladin: 'paladin', 'Heavy Knight': 'heavy-knight', Ranger: 'ranger', Assassin: 'assassin', Sentinel: 'sentinel', Templar: 'templar', Elementalist: 'elementalist', Warlock: 'warlock', Sorcerer: 'sorcerer', Necromancer: 'necromancer', Bishop: 'bishop', Cleric: 'cleric' };
const GUILD_HERO_ART = {
  MoonWarden: 'moonwarden-human',
  'Lobisomem de machado e escudo': 'moonwarden-werewolf',
  'Grande Lobo': 'moonwarden-great-wolf',
  Vanguard: 'vanguard',
  Swordsmaster: 'swordsmaster',
  WarBringer: 'warbringer',
};
const guildHeroArt = actor => GUILD_HERO_ART[actor.guildForm || actor.className];
const actorArt = actor => guildHeroArt(actor) || PRESTIGE_ART[actor.prestige] || actor.spriteId || (actor.kind === 'skeleton' ? 'skeleton-v02' : actor.kind === 'zombie' ? 'zombie-v01' : actor.kind === 'wolf' ? 'mordavian-wolf-v01' : actor.kind === 'goblin' ? 'mordavian-goblin-v01' : actor.kind === 'troll' ? 'mordavian-troll-v01' : null) || CLASS_ART[actor.className];
const actorPortrait = actor => guildHeroArt(actor) || PRESTIGE_ART[actor.prestige] || actor.portraitId || PORTRAIT_ART[actor.className] || 'male-peasant';
const actorSheet = actor => {
  if (actor.action === 'build') return 'build';
  if (actor.action === 'idle') return 'idle';
  if (actor.className === 'Peasant' && actor.action === 'work') return 'work';
  if (actor.className === 'Peasant' && actor.action === 'collect') return 'action';
  if (actor.className === 'Peasant' && actor.action === 'fight') return 'fight';
  if (actor.action === 'action' && (!actor.prestige || guildHeroArt(actor))) return 'action';
  return 'walk';
};
// Nomes, manias e falas vindos do Herança de Cinzas original.
const FIRST_NAMES = ['Vasile','Mirela','Bogdan','Ileana','Radu','Doina','Grigore','Anca','Toma','Zoia','Petru','Lenuța','Costel','Viorica','Mircea','Sanda','Dorin','Floarea','Ion','Rodica'];
const EPITHETS = ['Três Dedos','Sem Sorte','do Brejo','da Vala','Meio Surdo','Olho Torto','de Ninguém','Dente Mole','do Poço'];
const TRAITS = {
  'Covarde': 'Foge na primeira ferida.', 'Ganancioso': 'Só aceita recompensa gorda.', 'Bêbado': 'Passa a noite na taverna, aconteça o que acontecer.',
  'Amaldiçoado': 'Os monstros preferem ele.', 'Valente': 'Nunca foge. Isso não é um elogio.', 'Honesto': 'Nenhum defeito conhecido. Suspeito.', 'Teimoso': 'Não foge. Não escuta. É você.',
};
const pick = list => list[Math.floor(Math.random() * list.length)];
function heroName() {
  const taken = new Set(state.heroes.map(hero => hero.name));
  for (let i = 0; i < 40; i++) { const name = `${pick(FIRST_NAMES)} ${pick(EPITHETS)}`; if (!taken.has(name)) return name; }
  return `${pick(FIRST_NAMES)} ${pick(EPITHETS)} ${state.heroes.length + 1}`;
}
const PROMO_MSG = {
  Paladin: n => `${n} virou Paladin. Agora reza antes de bater.`, 'Heavy Knight': n => `${n} virou Heavy Knight. A armadura pesa mais que ele.`,
  Ranger: n => `${n} virou Ranger. Agora erra de mais longe.`, Assassin: n => `${n} virou Assassin. Ninguém mais sabe o nome verdadeiro dele.`,
  Sentinel: n => `${n} virou Sentinel. Não sai de perto de casa nem pra beber.`, Templar: n => `${n} virou Templar. A lança foi benta. Ele, mais ou menos.`,
  Elementalist: n => `${n} virou Elementalist. Tirem o trigo de perto.`, Warlock: n => `${n} virou Warlock. Ninguém perguntou o preço.`,
  Sorcerer: n => `${n} virou Sorcerer. Os cães agora vêm em dupla.`, Necromancer: n => `${n} virou Necromancer. O cemitério ganhou um sindicato.`,
  Bishop: n => `${n} virou Bishop. Já pediu o dízimo.`, Cleric: n => `${n} virou Cleric. Cura com uma mão e bate com a outra.`,
};
const NIGHT_MSG = ['Primeira noite. Dizem que a primeira é a mais fácil. Dizem muita coisa.', 'Anoiteceu. Tranque as galinhas. Você não tem galinhas.',
  'Noite três. Os mortos já sabem o seu endereço.', 'Anoiteceu. O padre recomenda rezar. O padre está bêbado.', 'Noite cinco. A cinza está mais grossa. Isso não é cinza.',
  'Mais uma noite. Ou a última. Depende de você.', 'Anoiteceu. Alguém da família veio visitar.'];
const LOSS_MSG = ['Um camponês morreu. Os outros pediram aumento.', 'Um camponês foi levado. A colheita atrasou.', 'Perdemos um camponês. Ele tinha nome. Ninguém lembra qual.'];

const isDone = building => !building.underConstruction && !building.ruined;
// Vida das construções. Monstros atacam (damageBuilding); camponeses consertam gastando madeira.
// Cada nível de melhoria dá +50% de vida. Em ruínas, a construção para até ser consertada.
const BASE_HP = { farm: 400, mill: 480, bakery: 440, emberLab: 600, hut: 320, depot: 600, lumber: 360, quarry: 440, mine: 480, market: 480, tavern: 560, hotel: 640, armory: 800 };
const GUILD_HP = 720;
const REPAIR_RATE = 8; // vida por segundo de jogo, antes da habilidade e das ferramentas
const REPAIR_HP_PER_WOOD = 20; // cada madeira devolve esta vida
// O casarão também apanha: vida própria mais a Resistência (RES) do herdeiro x 3. Se cair, o herdeiro morre.
const MANOR_BASE_HP = 1000;
const MANOR = () => state.manor || (state.manor = { site: 'manor', type: 'manor', x: MANOR_DOOR.x, y: MANOR_DOOR.y, damage: 0, ruined: false, builderId: null });
const repairSites = () => [...state.buildings, MANOR()];
const buildSiteOf = actor => actor.buildSite != null ? repairSites().find(building => building.site === actor.buildSite && needsBuilder(building)) || null : null;
function buildingLevel(building) {
  if (building.type === 'hut') return building.big ? 1 : 0;
  if (building.tier != null || building.branch) return building.tier ?? 2;
  return building.level || 0;
}
const buildingMaxHp = building => building.type === 'manor' ? MANOR_BASE_HP + heirStat('RES') * 3 : Math.round((BASE_HP[building.type] ?? GUILD_HP) * (1 + 0.5 * buildingLevel(building)));
const buildingHp = building => Math.max(0, Math.round(buildingMaxHp(building) - (building.damage || 0)));
const needsBuilder = building => !!building.underConstruction || (building.damage || 0) > 0;
function damageBuilding(building, amount) {
  if (!building || building.underConstruction) return;
  building.damage = Math.min(buildingMaxHp(building), (building.damage || 0) + amount);
  if (building.damage >= buildingMaxHp(building) && !building.ruined) {
    building.ruined = true;
    if (building.type === 'manor') { manorFell(); return; }
    say(`${siteLabel(building)} virou ruína. Os camponeses vão juntar os pedaços.`);
  }
  assignBuilders();
  renderSites();
  renderManorHp();
  if (state.selectedSite === building.site) renderPanel();
}
const guildHousing = () => state.buildings.filter(building => BUILDINGS.some(item => item.id === building.type && item.heroClass) && isDone(building)).length;
const lodgingCapacity = () => state.buildings.filter(building => isDone(building) && building.type === 'hotel').length * 4
  + state.buildings.filter(building => isDone(building) && building.type === 'tavern').length;
// O casarão abriga dois heróis. Começa com um; a cada dia, se houver cama, chega outro de uma das seis classes base.
const MANOR_HERO_SLOTS = 2;
const BASE_HERO_CLASSES = ['Warrior', 'Archer', 'Lancer', 'Black Mage', 'Conjurer', 'Healer'];
const manorHeroes = () => state.heroes.filter(hero => hero.alive && (hero.home === 'manor' || (hero.home == null && hero.homeSite == null && hero.guildSite == null && hero.recruitedFromSite == null))).length;
const heroCapacity = () => MANOR_HERO_SLOTS + guildHousing() + lodgingCapacity();
const freeLodging = () => {
  return state.buildings.find(building => isDone(building)
    && ((building.type === 'hotel' && state.heroes.filter(hero => hero.alive && hero.homeSite === building.site).length < 4)
      || (building.type === 'tavern' && !state.heroes.some(hero => hero.alive && hero.homeSite === building.site)))) || null;
};
// Construção livre: vai onde o jogador colocar. Só não pode sair do mapa nem encostar em outra construção.
const BUILD_SPACING = 12.5;
const BUILDING_ART = {
  bakery: 'bakery', emberLab: 'ember-lab', butcher: 'butcher', blacksmith: 'blacksmith',
  farm: 'farm-wheat', mill: 'mill-body', hut: 'hut', depot: 'depot', lumber: 'lumber', quarry: 'quarry-l1', mine: 'mine-l1', market: 'market', tavern: 'tavern', hotel: 'hotel', warrior: 'guild', archer: 'archer', lancer: 'lancer',
  blackMage: 'tower', conjurer: 'conjurer', healer: 'healer', armory: 'armory', moonwarden: 'guild-moonwarden', vanguard: 'guild-vanguard',
};
const PHOTOREAL_BUILDING_ART = new Set([
  'farm-garlic', 'farm-wheat', 'farm-wheat-v02', 'farm-bacon', 'farm-eggs', 'farm-milk',
  'farm-garlic-l2', 'farm-garlic-l3', 'farm-wheat-v02-l2', 'farm-wheat-v02-l3',
  'farm-bacon-l2', 'farm-bacon-l3', 'farm-eggs-l2', 'farm-eggs-l3', 'farm-milk-l2', 'farm-milk-l3',
  'farm-vegetables', 'farm-vegetables-l2', 'farm-vegetables-l3',
  'mill-body-l2', 'mill-body-l3',
  'mill-body', 'mill-sails', 'hut', 'hut-big', 'depot', 'lumber', 'market',
  'tavern', 'hotel', 'guild', 'archer', 'lancer', 'tower', 'conjurer', 'healer', 'armory', 'manor', 'bakery', 'ember-lab', 'butcher', 'blacksmith', 'bakery-l2', 'bakery-l3', 'ember-lab-l2', 'ember-lab-l3', 'butcher-l2', 'blacksmith-l2',
]);
// Artes de obra: o sprite final existente continua sendo a fase completa.
// Para adicionar uma construção nova, inclua a chave aqui e os arquivos -early/-mid.
const CONSTRUCTION_STAGE_ART = new Set([
  'farm-garlic', 'farm-wheat', 'farm-wheat-v02', 'farm-bacon', 'farm-eggs', 'farm-milk',
  'farm-garlic-l2', 'farm-garlic-l3', 'farm-wheat-v02-l2', 'farm-wheat-v02-l3',
  'farm-bacon-l2', 'farm-bacon-l3', 'farm-eggs-l2', 'farm-eggs-l3', 'farm-milk-l2', 'farm-milk-l3',
  'farm-vegetables', 'farm-vegetables-l2', 'farm-vegetables-l3',
  'mill-body', 'mill-body-l2', 'mill-body-l3', 'hut', 'hut-big', 'depot', 'lumber', 'market', 'tavern',
  'hotel', 'guild', 'archer', 'lancer', 'tower', 'conjurer', 'healer', 'armory', 'bakery', 'bakery-l2', 'bakery-l3', 'ember-lab', 'ember-lab-l2', 'ember-lab-l3', 'butcher', 'butcher-l2', 'blacksmith', 'blacksmith-l2',
]);
const BUILDING_ART_ASPECTS = {
  'farm-garlic': 1.5, 'farm-wheat': 1.5, 'farm-wheat-v02': 1.5, 'farm-bacon': 1.5, 'farm-eggs': 1.5, 'farm-milk': 1.5,
  'farm-garlic-l2': 1.5, 'farm-garlic-l3': 1.5, 'farm-wheat-v02-l2': 1.5, 'farm-wheat-v02-l3': 1.5,
  'farm-bacon-l2': 1.5, 'farm-bacon-l3': 1.5, 'farm-eggs-l2': 1.5, 'farm-eggs-l3': 1.5, 'farm-milk-l2': 1.5, 'farm-milk-l3': 1.5,
  'farm-vegetables': 1.5, 'farm-vegetables-l2': 1.5, 'farm-vegetables-l3': 1.5,
  'mill-body': 1105 / 1424, 'mill-body-l2': 1105 / 1424, 'mill-body-l3': 1105 / 1424, 'mill-sails': 1312 / 1199, hut: 1.5, 'hut-big': 1.5,
  depot: 2, lumber: 1.5, market: 2, tavern: 1.5, hotel: 2, guild: 1.5,
  archer: 1199 / 1312, lancer: 1.5, tower: 1202 / 1308, conjurer: 1312 / 1199, bakery: 1.5, 'ember-lab': 1200 / 1003,
  'bakery-l2': 1.5, 'bakery-l3': 1.5, 'ember-lab-l2': 1200 / 1003, 'ember-lab-l3': 1200 / 1003, butcher: 1.5, 'butcher-l2': 1.5, blacksmith: 1.5, 'blacksmith-l2': 1.5,
  healer: 1.5, armory: 1.5, manor: 1.5,
  'quarry-l1': 1, 'quarry-l2': 1, 'mine-l1': 1, 'mine-l2': 1,
};
const PIT_BUILDING_SHEETS = {
  'quarry-l1': 'public/art/mining-quarry-v01/quarry-level-1-construction-board-v02.png',
  'quarry-l2': 'public/art/mining-quarry-v01/quarry-level-2-construction-board-v01.png',
  'mine-l1': 'public/art/mining-quarry-v01/mine-level-1-construction-board-v01.png',
  'mine-l2': 'public/art/mining-quarry-v01/mine-level-2-construction-board-v01.png',
};
function pitSpriteSheet(art, constructionStage = null, building = null) {
  const match = /^(quarry|mine)-l[12]$/.exec(art || '');
  if (!match) return null;
  const type = match[1];
  const level = building?.underConstruction && building.pendingUpgrade?.kind === 'pit' ? 2 : Number(art.slice(-1));
  const sheet = PIT_BUILDING_SHEETS[`${type}-l${level}`];
  if (!sheet) return null;
  const stage = building?.underConstruction && building.pendingUpgrade?.kind === 'pit'
    ? ((building.progress || 0) < BUILD_TIME * 0.4 ? 'early' : 'mid')
    : constructionStage;
  return { sheet, position: stage === 'early' ? '0%' : stage === 'mid' ? '50%' : '100%' };
}
function buildingImageStyle(art, constructionStage = null, building = null) {
  const sprite = pitSpriteSheet(art, constructionStage, building);
  return sprite
    ? `background-image:url('${sprite.sheet}');background-size:300% 100%;background-position:${sprite.position} 50%;background-repeat:no-repeat;`
    : `background-image:url('${buildingImage(art, constructionStage, building)}')`;
}
function applyBuildingImage(element, art, constructionStage = null, building = null) {
  const sprite = pitSpriteSheet(art, constructionStage, building);
  element.style.backgroundImage = `url('${sprite?.sheet || buildingImage(art, constructionStage, building)}')`;
  if (sprite) {
    element.style.backgroundSize = '300% 100%';
    element.style.backgroundPosition = `${sprite.position} 50%`;
    element.style.backgroundRepeat = 'no-repeat';
  } else {
    element.style.removeProperty('background-size');
    element.style.removeProperty('background-position');
    element.style.removeProperty('background-repeat');
  }
}
const buildingImage = (art, constructionStage = null, building = null) => {
  if (building?.underConstruction && building.pendingUpgrade?.kind === 'producer') {
    const artId = building.type === 'farm' ? farmType(building.produce).art : BUILDING_ART[building.type];
    const targetArt = LEVEL_ART[artId]?.[producerLevel(building) + 1];
    if (targetArt) {
      if (!CONSTRUCTION_STAGE_ART.has(targetArt)) return buildingImage(targetArt, null);
      if ((building.progress || 0) < BUILD_TIME / 3) return buildingImage(art, null);
      if ((building.progress || 0) < BUILD_TIME * 2 / 3) return `public/art/buildings-construction-dark-v01/${targetArt}-early.webp`;
      return `public/art/buildings-construction-dark-v01/${targetArt}-mid.webp`;
    }
  }
  if (art === 'armory' && constructionStage && building?.armoryUpgradeTo >= 2 && building.armoryUpgradeTo <= 4) {
    return `public/art/armory-upgrade-phases-v01/armory-upgrade-to-level-0${building.armoryUpgradeTo}-${constructionStage}-v01.png`;
  }
  if (constructionStage && CONSTRUCTION_STAGE_ART.has(art)) {
    return `public/art/buildings-construction-dark-v01/${art}-${constructionStage}.webp`;
  }
  if (!PHOTOREAL_BUILDING_ART.has(art)) return `public/art/${art}-v01.webp`;
  if (options.buildingArtStyle === 'color') {
    return `public/art/buildings-color-v02/${art}.webp`;
  }
  return `public/art/buildings-photoreal-v03-runtime/${art}.webp`;
};
const PATH_NODES = [
  { x: 42, y: 39, links: [1] },                 // Entrada do casarão
  { x: 53, y: 54, links: [0, 2, 3] },           // Cruzamento central
  { x: 41, y: 58, links: [1, 4] },
  { x: 62, y: 56, links: [1, 6, 10] },
  { x: 40, y: 68, links: [2, 5] },
  { x: 40, y: 80, links: [4, 12] },
  { x: 62, y: 81, links: [3] },
  { x: 90, y: 52, links: [8, 11] },
  { x: 91, y: 29, links: [7, 9] },
  { x: 85, y: 21, links: [8] },                 // Ruínas do covil
  { x: 62, y: 67, links: [3, 11] },             // Contorno sul do lote direito
  { x: 90, y: 67, links: [10, 7] },
  { x: 42, y: 99, links: [5, 13] },
  { x: 54, y: 107, links: [12, 14] },
  { x: 51, y: 121, links: [13, 15] },
  { x: 39, y: 137, links: [14, 16] },
  { x: 45, y: 153, links: [15, 17] },
  { x: 56, y: 171, links: [16, 18] },
  { x: 44, y: 190, links: [17] },               // Final da estrada velha
];
const PATROLS = {
  Warrior: [0, 3, 6, 1, 5],
  Healer: [0, 2, 5, 1, 6],
  other: [0, 1, 3, 6, 2],
};

const GAME_VERSION = 'Version 0.13.21';
const SAVE_SLOTS = 3;
const SAVE_KEY = slot => `mordavia.save.${slot}`;
const OPTIONS_KEY = 'mordavia.options';
const DEBUG_KEY = 'mordavia.debug';

function readStore(key, fallback) {
  try { const raw = localStorage.getItem(key); return raw ? { ...fallback, ...JSON.parse(raw) } : { ...fallback }; } catch { return { ...fallback }; }
}
function writeStore(key, value) {
  try { localStorage.setItem(key, JSON.stringify(value)); return true; } catch { return false; }
}

// Opções do jogador e do debug ficam salvas no navegador.
const options = readStore(OPTIONS_KEY, { startZoom: 0, names: 'selected', autosave: 3, buildingArtStyle: 'dark' });
if (options.zoomVersion !== 3 || ![0, 1, 2].includes(options.startZoom)) { options.startZoom = 0; options.zoomVersion = 3; }
// Som: 1 ligado, 0 desligado. Vale para a música (ver setMusic) e para todo som futuro.
if (![0, 1].includes(options.sound)) options.sound = 1;
if (!['dark', 'color'].includes(options.buildingArtStyle)) options.buildingArtStyle = 'dark';
// A configuração só escolhe o estoque concedido ao criar uma partida nova.
const debug = readStore(DEBUG_KEY, { startingStock: 'standard' });
if (!['standard', 'full'].includes(debug.startingStock)) debug.startingStock = debug.testMode ? 'full' : 'standard';
delete debug.testMode;
// Estoque de teste: só quantidade. Reposto ao entrar na partida e a cada segundo (pedido do dono: "maxed out e sem gastar").
// Os custos são cobrados normalmente; o saldo só não acaba. Pedra fica em 40 kg porque ocupa o depósito.
const TEST_STOCK = { gold: 1e9, timber: 1e9, stone: 40 };
function grantStartingStock(target) {
  if (debug.startingStock !== 'full') return;
  Object.entries(TEST_STOCK).forEach(([id, qty]) => { target[id] = Math.max(target[id] || 0, qty); });
}

// Começo de um jogo novo. Regra: sem construção, sem ninguém. O casarão traz um camponês e um Warrior;
// outros heróis saem de guildas, taverna ou hotel; camponeses só chegam pelo casarão e pelos casebres.
// Bolsas: salário diário do trabalhador e o que cada um traz ao chegar.
const WORKER_WAGE = 5;
const WORKER_START_PURSE = 5;
const HERO_START_PURSE = 15;
function newGameState(heir = null) {
  return {
  gold: 240, grain: 58, timber: 200, stone: 200, marble: 0, stoneDeposits: {}, quarryMaterials: { wallingKg: 0, lintels: [], dimensionBlocks: [], aggregateKg: 0, settsKg: 0, roofingSlateKg: 0, roofingSlatesKg: 0, millstones: [], quicklimeKg: 0, limePuttyKg: 0, mortarKg: 0, plasterKg: 0 }, metal: 0, charcoal: 0, metals: Object.fromEntries(METAL_STOCK.map(id => [id, 0])), craftedWeapons: [], oreLots: [], oreGrades: {}, mineralDeposits: {}, residues: {}, extractionTools: [
      { id: 'starter-iron-pick-1', type: 'mine', edge: 'iron', durability: 40, maxDurability: 40, assignedTo: null },
      { id: 'starter-iron-pick-2', type: 'mine', edge: 'iron', durability: 40, maxDurability: 40, assignedTo: null },
      { id: 'starter-quarry-pick-1', type: 'quarry', edge: 'iron', durability: 40, maxDurability: 40, assignedTo: null },
      { id: 'starter-quarry-pick-2', type: 'quarry', edge: 'iron', durability: 40, maxDurability: 40, assignedTo: null },
    ], remains: { D: 0, C: 0, B: 0, A: 0, nekomata: 0, nekomataWild: 0 }, remainsPiles: [], fishingStock: Object.fromEntries(FISHING_SPOTS.map(spot => [spot.site, 0])),
    food: { wheat: 0, flour: 40, garlic: 18, bacon: 0, pork: 0, chicken: 0, beef: 0, boarMeat: 0, elkMeat: 0, eggs: 0, milk: 0, bread: 0, cheese: 0, quiche: 0, sausage: 0, nuggets: 0, wings: 0, hamburger: 0, steak: 0, parmesanSteak: 0, roastBoar: 0, garlicBoar: 0, roastElk: 0, elkStew: 0, potato: 0, carrot: 0, tomato: 0, fish: 0 }, items: [], ration: 2, diet: { wheat: false, flour: false, garlic: true, bacon: true, pork: false, chicken: false, beef: false, boarMeat: false, elkMeat: false, eggs: true, milk: true, bread: true, cheese: true, quiche: true, sausage: true, nuggets: true, wings: true, hamburger: true, steak: true, parmesanSteak: true, roastBoar: true, garlicBoar: true, roastElk: true, elkStew: true, potato: true, carrot: true, tomato: true, fish: true }, happiness: 50, moodTarget: 50, moodParts: null, favor: 50, demands: [], nextDemandAt: 600, lastRaidDay: -1, bounties: [], bountyAmount: 25,
    speed: 1, minutes: 380, ticks: 0, manorArrivalDay: 0, selectedBuild: null, selectedSite: null, selectedFarmType: 'wheat', taxPercent: 10, tavernTab: 0, nextBuildingId: 1,
    tab: 'build', won: false, goalStep: 0,
    bandits: [], wildMeatPiles: [], wildlifeSpawnDay: Math.floor(380 / 1440), nextRaidAt: 720,
    heir, heirLevel: 1, heirXp: 0, heirAttributePoints: 0, heirAttributes: { CHA: 0, MAG: 0, INT: 0, WIS: 0, RES: 0 }, equipment: BASIC_EQUIPMENT(), playSeconds: 0,
    buildings: [],
    mapSet: 1,
    lairs: [
      { id: 'cemetery-l1', name: 'Cemiterio level 1', kind: 'cemetery', level: 1, art: 'mordavia-cemetery-l1-v01.png', mapId: 'city', x: 44, y: 190, active: true },
      { id: 'wolf-den-1', name: 'Covil de lobos 1', kind: 'wolf-den', level: 1, art: 'mordavia-wolf-den-l1-v01.png', mapId: 'wilds', x: 82, y: 173, active: true, nextSpawnAt: 380 + 90 },
      { id: 'wolf-den-2', name: 'Covil de lobos 2', kind: 'wolf-den', level: 1, art: 'mordavia-wolf-den-l1-v01.png', mapId: 'wilds', x: 91, y: 184, active: true, nextSpawnAt: 380 + 420 },
      { id: 'cemetery-l2', name: 'Cemiterio level 2', kind: 'cemetery', level: 2, art: 'mordavia-cemetery-l2-v01.png', mapId: 'wilds', x: 44, y: 190, active: false },
      { id: 'wolf-den-l2', name: 'Covil de lobos level 2', kind: 'wolf-den', level: 2, art: 'mordavia-wolf-den-l2-v01.png', mapId: 'wilds', x: 82, y: 173, active: false, nextScenario: true },
      { id: 'nekomata-den-l1', name: 'Covil da Nekomata · nível 1', kind: 'nekomata-den', level: 1, art: 'nekomata-lair-photo-candidate-v02.png', mapId: 'wilds', mapSet: 1, x: 24, y: 55, active: true, nextSpawnAt: 380 + 360 },
      { id: 'nekomata-den-l2', name: 'Covil da Nekomata · nível 2', kind: 'nekomata-den', level: 2, art: 'nekomata-lair-photo-v04.png', mapId: 'wilds', mapSet: 2, x: 34, y: 118, active: true, nextSpawnAt: 380 + 360 },
      { id: 'goblin-den-l1', name: 'Covil goblin level 1', kind: 'goblin-den', level: 1, art: 'mordavia-goblin-den-l1-v01.png', mapId: 'wilds', x: 82, y: 24, active: true, nextSpawnAt: 380 + 200 },
      { id: 'goblin-den-l2', name: 'Covil goblin level 2', kind: 'goblin-den', level: 2, art: 'mordavia-goblin-den-l2-v01.png', mapId: 'wilds', x: 34, y: 118, active: false, nextScenario: true },
      { id: 'troll-den-l1', name: 'Covil de trolls level 1', kind: 'troll-den', level: 1, art: 'mordavia-troll-den-l1-v01.png', mapId: 'wilds', x: 90, y: 34, active: true, nextSpawnAt: 380 + 300 },
      { id: 'troll-den-l2', name: 'Covil de trolls level 2', kind: 'troll-den', level: 2, art: 'mordavia-troll-den-l1-v01.png', mapId: 'wilds', x: 62, y: 162, active: false, nextScenario: true },
    ],
    heroes: [
      { id: 'warrior-manor', name: 'Bogdan Sem Sorte', className: 'Warrior', trait: 'Valente', level: 1, xp: 0, x: 42, y: 39, alive: true, home: 'manor', purse: HERO_START_PURSE, gear: { weapon: 'rustySword' } },
    ],
    workers: [
      { id: 'peasant-1', name: 'Ion do Poço', level: 1, xp: 0, className: 'Peasant', spriteId: 'peasant', portraitId: 'male-peasant', x: 42, y: 39, alive: true, workTrip: false, home: 'manor', purse: WORKER_START_PURSE },
      { id: 'peasant-2', name: 'Rodica do Poço', level: 1, xp: 0, className: 'Peasant', spriteId: 'female-peasant', portraitId: 'female-peasant', x: 42.6, y: 39, alive: true, workTrip: false, home: 'manor', purse: WORKER_START_PURSE },
    ],
    selectedActor: null,
  };
}

let state = newGameState();
let selectedLairId = null;
let selectedFishingSpotId = null;
const game = { running: false, slot: null, lastSaved: 0 };
let arrivalClockState = state;
let arrivalClock = { workers: state.minutes, heroes: state.minutes };

function resetArrivalClock() {
  arrivalClockState = state;
  arrivalClock = { workers: state.minutes, heroes: state.minutes };
}
function arrivalIntervalMinutes(kind) {
  const chaPoints = Math.max(0, Number(state.heirAttributes?.CHA) || 0);
  const baseMinutes = kind === 'workers' ? 120 : 240;
  return baseMinutes / (1 + chaPoints / 100);
}
function arrivalIsDue(kind) {
  if (arrivalClockState !== state) resetArrivalClock();
  if (state.minutes - arrivalClock[kind] < arrivalIntervalMinutes(kind)) return false;
  arrivalClock[kind] = state.minutes;
  return true;
}

const $ = (selector, root = document) => root.querySelector(selector);
const $$ = (selector, root = document) => [...root.querySelectorAll(selector)];
const format = n => Math.floor(n).toLocaleString('pt-BR');
const formatHudAmount = value => {
  const amount = Number(value) || 0, absolute = Math.abs(amount), sign = amount < 0 ? '−' : '';
  if (absolute >= 999_500_000) return `${sign}${(absolute / 1_000_000_000).toFixed(1).replace('.', ',')}B`;
  if (absolute >= 10_000_000) return `${sign}${Math.floor(absolute / 1_000_000)}M`;
  if (absolute >= 1_000_000) return `${sign}${(absolute / 1_000_000).toFixed(1).replace('.', ',')}M`;
  if (absolute >= 10_000) return `${sign}${Math.floor(absolute / 1_000)}k`;
  if (absolute >= 1_000) return `${sign}${(absolute / 1_000).toFixed(1).replace('.', ',')}k`;
  return format(amount);
};
const setHudAmount = (selector, value) => {
  const element = $(selector), exact = format(value);
  element.textContent = formatHudAmount(value);
  element.title = exact;
  element.setAttribute('aria-label', exact);
};
const formatMass = n => Number(n || 0).toLocaleString('pt-BR', { maximumFractionDigits: 4 });
const formatMetalAmount = (id, kg) => ['copper', 'silver', 'gold'].includes(id) && Math.abs(kg) < 1 ? `${formatMass(kg * 1000)} g` : `${formatMass(kg)} kg`;

// A câmera percorre o mundo sem alterar as coordenadas dos lotes e personagens.
// Zoom do jogo: ampla preenche a tela; normal aproxima 30%; próxima aproxima 60%.
const ZOOM_LEVELS = [1, 1.3, 1.6];
const MAP_WORLD_HEIGHT = 2624;
const MAP_RATIO = MAP_WORLD_HEIGHT / 1199;
const WORLD_Y_UNITS = 200 * MAP_WORLD_HEIGHT / 2452;
const Y_TO_X_SCALE = MAP_WORLD_HEIGHT / WORLD_Y_UNITS * 100 / 1199;
let currentMap = 'city';
let currentMapSet = 1;
const camera = { x: 0.5, y: 0, zoom: 0, drag: null };
const clampCamera = value => Math.max(0, Math.min(1, value));

function cameraBounds() {
  const scene = $('#map-scene');
  const world = $('#map-world');
  return {
    x: Math.max(0, world.offsetWidth * ZOOM_LEVELS[camera.zoom] - scene.clientWidth),
    y: Math.max(0, world.offsetHeight * ZOOM_LEVELS[camera.zoom] - scene.clientHeight),
  };
}

// O mundo cobre a moldura inteira: a largura é a maior entre a da moldura e a necessária para preencher a altura.
// Assim o zoom 1 (distante) nunca deixa faixas vazias, em qualquer formato de janela.
function sizeWorld() {
  const scene = $('#map-scene');
  const world = $('#map-world');
  world.style.width = `${Math.max(scene.clientWidth, scene.clientHeight / MAP_RATIO)}px`;
}

// Centraliza a câmera no casarão (cerca de 42% x 39% da arte).
function homeCamera() {
  const scene = $('#map-scene');
  const world = $('#map-world');
  camera.x = 0.5;
  camera.y = 0;
  renderCamera();
}

function renderCamera() {
  const bounds = cameraBounds();
  const scene = $('#map-scene');
  const world = $('#map-world');
  const zoom = ZOOM_LEVELS[camera.zoom];
  // Quando o mundo cabe inteiro na cena (zoom distante), ele fica centralizado em vez de colado à borda.
  const slackX = Math.max(0, scene.clientWidth - world.offsetWidth * zoom) / 2;
  const slackY = Math.max(0, scene.clientHeight - world.offsetHeight * zoom) / 2;
  world.style.setProperty('--zoom', zoom);
  world.style.setProperty('--camera-x', `${slackX - bounds.x * camera.x}px`);
  world.style.setProperty('--camera-y', `${slackY - bounds.y * camera.y}px`);
}

function setZoom(index) {
  if (index === camera.zoom || !ZOOM_LEVELS[index]) return;
  const scene = $('#map-scene');
  const world = $('#map-world');
  const oldBounds = cameraBounds();
  const oldScale = ZOOM_LEVELS[camera.zoom];
  const oldSlackX = Math.max(0, (scene.clientWidth - world.offsetWidth * oldScale) / 2);
  const oldSlackY = Math.max(0, (scene.clientHeight - world.offsetHeight * oldScale) / 2);
  const centerX = (scene.clientWidth / 2 - oldSlackX + oldBounds.x * camera.x) / (world.offsetWidth * oldScale);
  const centerY = (scene.clientHeight / 2 - oldSlackY + oldBounds.y * camera.y) / (world.offsetHeight * oldScale);
  camera.zoom = index;
  const bounds = cameraBounds();
  const newScale = ZOOM_LEVELS[index];
  const newSlackX = Math.max(0, (scene.clientWidth - world.offsetWidth * newScale) / 2);
  const newSlackY = Math.max(0, (scene.clientHeight - world.offsetHeight * newScale) / 2);
  camera.x = bounds.x ? clampCamera((newSlackX + centerX * world.offsetWidth * newScale - scene.clientWidth / 2) / bounds.x) : 0.5;
  camera.y = bounds.y ? clampCamera((newSlackY + centerY * world.offsetHeight * newScale - scene.clientHeight / 2) / bounds.y) : 0;
  $$('.zoom-btn').forEach(button => {
    const active = Number(button.dataset.zoom) === index;
    button.classList.toggle('active', active);
    button.setAttribute('aria-pressed', String(active));
  });
  renderCamera();
}

function focusRegion(name) {
  if (name === 'manor') { homeCamera(); return; }
  if (name === 'wilds') { switchMap('wilds'); return; }
  const scene = $('#map-scene');
  const world = $('#map-world');
  const bounds = cameraBounds();
  const desiredCenter = world.offsetHeight * ZOOM_LEVELS[camera.zoom] * 0.72;
  camera.y = bounds.y ? clampCamera((desiredCenter - scene.clientHeight / 2) / bounds.y) : 0;
  renderCamera();
}

function switchMap(name) {
  currentMap = name === 'wilds' ? 'wilds' : 'city';
  // A disponibilidade das construções depende da região. Sempre mostre o catálogo
  // ao trocar de mapa para que Wilds não pareça sem opções quando outra aba estava aberta.
  state.tab = 'build';
  $$('.tab').forEach(tab => {
    const active = tab.dataset.tab === state.tab;
    tab.classList.toggle('active', active);
    tab.setAttribute('aria-selected', String(active));
  });
  const wilds = currentMap === 'wilds';
  $('#map-world').dataset.map = currentMap;
  $('#map-world').dataset.mapSet = String(currentMapSet);
  const art = currentMapSet === 2
    ? (wilds ? ['mordavia-map02-wilds-v01.png', 'mordavia-map02-wilds-trees-v01.png'] : ['mordavia-map02-city-v01.png', 'mordavia-map02-city-trees-v01.png'])
    : (wilds ? ['mordavia-wilds-map.webp', 'mordavia-wilds-forest-layer.png'] : ['mordavia-map.webp', 'mordavia-forest-layer.png']);
  $('.map-art').src = `public/art/${art[0]}`;
  $('.tree-foreground').src = `public/art/${art[1]}`;
  $('.map-art').alt = wilds ? 'Mapa dos Wilds' : 'Mapa de City';
  const button = $('#camera-wilds');
  button.textContent = wilds ? 'CITY' : 'WILDS';
  button.title = wilds ? 'Voltar para City' : 'Abrir Wilds';
  button.setAttribute('aria-label', button.title);
  camera.x = 0.5;
  camera.y = 0;
  state.selectedSite = null;
  state.demolishArmed = null;
  selectedLairId = null;
  selectedFishingSpotId = null;
  sizeWorld();
  renderCamera();
  renderLairs();
  renderHeroes();
  renderPlaces();
  renderFishingSpots();
  renderSites();
  renderPanel();
  updateHud();
  updateManorInventoryTrigger();
}

function updateManorInventoryTrigger() {
  const trigger = $('#manor-inventory-trigger');
  if (trigger) trigger.hidden = currentMap !== 'city';
}

function openManorInventory() {
  if (!game.slot) {
    say('Carregue uma partida para abrir o inventário do Casarão.');
    return;
  }
  renderManorInventory();
  $('#inventory-layer').hidden = false;
  $('#inventory-close').focus();
}

function inventorySection(title, rows) {
  return `<section class="inventory-section"><h3>${title}</h3>${rows.length ? `<div class="inventory-rows">${[...rows].sort((a, b) => a[0].localeCompare(b[0], 'pt-BR')).map(([name, amount, source, shown]) => `<div class="inventory-row${amount > 0 ? '' : ' empty'}"><b>${name}</b><strong>${shown ?? format(amount)}</strong><small>${source}</small></div>`).join('')}</div>` : '<p class="inventory-empty">Nada guardado.</p>'}</section>`;
}
function renderManorInventory() {
  const keepFocus = document.activeElement?.id === 'inventory-close';
  const used = storedUnits();
  const items = (state.items || []).map(id => { const item = craftedWeapon(id); return [item?.name || String(id), 1, item ? `Forjada · ${WEAPON_MATERIAL_LABELS[item.materialId]} · estrutura ${formatMass(item.structuralCondition)}/${formatMass(item.structuralMax)}${item.sharp ? ` · gume ${formatMass(item.edgeCondition)}/${formatMass(item.edgeMax)}` : ''}` : 'Arma ou item']; });
  const foodRows = FOOD_TYPES.map(food => [food.name, state.food?.[food.id] || 0, food.raw ? 'Matéria-prima' : food.baked || food.meat ? 'Produzido no domínio' : 'Provisão']);
  const metalRows = METAL_STOCK.map(id => {
    const lots = (state.oreLots || []).filter(lot => lot.id === id && lot.quantityKg > 1e-6);
    const source = ORE_STOCK.has(id) ? (lots.length ? lots.map(lot => `${oreName(lot.mineral)} · ${formatMass(lot.quantityKg)} kg · ${lot.gradePpm} ppm · veio ${lot.seamId || 'local'}`).join(' / ') : 'Rocha ou achado entregue do posto') : 'Metal manufaturado';
    return [METAL_NAMES[id], state.metals?.[id] || 0, source, formatMetalAmount(id, state.metals?.[id] || 0)];
  });
  const remainsRows = [...REMAINS_GRADES.map(grade => [`Dead Remains ${grade.id}`, state.remains?.[grade.id] || 0, 'Guardado · matéria-prima do Laboratório de Ember']), ['Nekomata Remains · covil', state.remains?.nekomata || 0, 'Guardado · material especial'], ['Nekomata Remains · individual', state.remains?.nekomataWild || 0, 'Guardado · material especial']];
  const workRows = state.buildings.filter(isDone).flatMap(building => {
    const name = buildingName(building);
    const crew = state.workers.filter(worker => worker.alive && worker.buildSite == null && (worker.workplace ? worker.workplace.site : worker.jobSite) === building.site).length;
    if (building.type === 'farm') {
      const type = farmType(building.produce);
      const rows = [[`${foodName(farmFoodId(building))} · ${name}`, building.stock || 0, `Colhido · esperando transporte · ${crew} trabalhador${crew === 1 ? '' : 'es'} · produção ${rateNumber(farmOutput(building) * (1 + crewWork(building)))}/s`]];
      if (type.secondary) rows.push([`${foodName(type.secondary)} · ${name}`, building.secondaryStock || 0, `Produção animal · esperando transporte · ${rateNumber(farmSecondaryRate(building) * (1 + crewWork(building)))}/s`]);
      return rows;
    }
    if (building.type === 'mill') return [['Trigo · Moinho', building.input || 0, `Aguardando moagem · ${crew} trabalhador${crew === 1 ? '' : 'es'} no Moinho · capacidade ${rateNumber(millRate(building) * (1 + crewWork(building)))}/s`], ['Farinha · Moinho', building.stock || 0, `Moída · aguardando transporte · capacidade ${rateNumber(millRate(building) * (1 + crewWork(building)))}/s`]];
    if (building.type === 'bakery') return [[`Insumos · ${name}`, Object.values(building.pantry || {}).reduce((sum, amount) => sum + amount, 0), 'Ingredientes aguardando preparo'], [`${foodName(building.stockFood || building.produce || 'bread')} · ${name}`, building.stock || 0, 'Pronto · aguardando transporte']];
    if (building.type === 'butcher') return [[`Insumos · ${name}`, Object.values(building.pantry || {}).reduce((sum, amount) => sum + amount, 0), 'Ingredientes aguardando preparo'], [`${foodName(building.stockFood || building.produce || 'bacon')} · ${name}`, building.stock || 0, 'Pronto · aguardando transporte']];
    if (building.type === 'mine') { const seam = selectedSeam(building); const rate = pitRate(building); return [[`Extração · ${name}`, rate, `${seam ? oreName(seam.mineral) : 'sem veio'} · ${seam ? (seam.remainingGrams / 1000).toFixed(1) : '0'} kg restantes`, `${fineRate(rate)}/s`]]; }
    if (building.type === 'blacksmith') { const recipe = blacksmithRecipe(building), rate = blacksmithRate(building) * (1 + crewWork(building)); const inputs = Object.entries(recipe.needs).map(([id, qty]) => `${METAL_NAMES[id] || id} ${formatMass(state.metals[id] || 0)}/${formatMass(qty)}`).join(' · '); return [[`Produção · ${name}`, rate, `${recipe.name} · ${inputs} · carvão ${formatMass(state.charcoal)}/${formatMass(recipe.fuel)} kg`, `${fineRate(rate)}/s`]]; }
    return [];
  }).concat(state.workers.filter(worker => worker.alive && worker.carrying?.food).map(worker => [`Em transporte · ${foodName(worker.carrying.food)}`, worker.carrying.qty, `${worker.name} levando ao depósito`]));
  const rows = [
    inventorySection('Moedas e materiais', [['Ouro', state.gold || 0, 'Cofre'], ['Madeira', state.timber || 0, 'Lenhadores'], ['Pedra de Construção (kg)', state.stone || 0, 'Pedreira · obras comuns'], ['Mármore (kg)', state.marble || 0, 'Pedreira nível 2 · mansão'], ['Carvão vegetal (kg)', state.charcoal || 0, 'Blacksmith'], ['Ember', state.ember || 0, 'Laboratório de Ember']]),
    inventorySection('Metais', metalRows),
    inventorySection('Provisões', foodRows),
    inventorySection('Produção nos prédios', workRows),
    inventorySection('Restos e equipamento', [...remainsRows, ['Armas e itens', items.length, items.map(([name]) => name).join(', ')]])
  ];
  $('#inventory-content').innerHTML = `<header class="inventory-head"><div><small>CASARÃO</small><h2>Inventário</h2><p>Estoque do depósito e produção que ainda está nos prédios.</p></div><button type="button" class="inventory-close" id="inventory-close" aria-label="Fechar inventário">×</button></header><div class="inventory-capacity"><b>Espaço usado no depósito</b><strong>${format(used)} / ${format(storageCap())}</strong><div class="inventory-capacity-bar"><i style="width:${Math.min(100, Math.round(used / storageCap() * 100))}%"></i></div></div><div class="inventory-grid">${rows.join('')}</div>`;
  $('#inventory-close').addEventListener('click', closeManorInventory);
  if (keepFocus) $('#inventory-close').focus();
}
function closeManorInventory() {
  $('#inventory-layer').hidden = true;
  $('#manor-inventory-trigger').focus();
}

function moveCamera(dx, dy) {
  const bounds = cameraBounds();
  if (bounds.x) camera.x = clampCamera(camera.x - dx / bounds.x);
  if (bounds.y) camera.y = clampCamera(camera.y - dy / bounds.y);
  renderCamera();
}

function initCamera() {
  const scene = $('#map-scene');
  const endDrag = event => {
    if (camera.drag?.id !== event.pointerId) return;
    camera.dragMoved = camera.drag.moved;
    camera.drag = null;
    scene.classList.remove('dragging');
    if (scene.hasPointerCapture(event.pointerId)) scene.releasePointerCapture(event.pointerId);
  };
  scene.addEventListener('pointerdown', event => {
    camera.dragMoved = false;
    if (camera.drag || event.button !== 0 || event.target.closest('button')) return;
    camera.drag = { id: event.pointerId, x: event.clientX, y: event.clientY, startX: event.clientX, startY: event.clientY, moved: false };
    try { scene.setPointerCapture(event.pointerId); } catch {}
    scene.classList.add('dragging');
  });
  scene.addEventListener('pointermove', event => {
    if (camera.drag?.id !== event.pointerId) return;
    if (Math.hypot(event.clientX - camera.drag.startX, event.clientY - camera.drag.startY) > 5) camera.drag.moved = true;
    if (!camera.drag.moved) return;
    moveCamera(event.clientX - camera.drag.x, event.clientY - camera.drag.y);
    camera.drag.x = event.clientX;
    camera.drag.y = event.clientY;
  });
  scene.addEventListener('pointerup', endDrag);
  scene.addEventListener('pointercancel', endDrag);
  scene.addEventListener('lostpointercapture', endDrag);
  scene.addEventListener('wheel', event => {
    event.preventDefault();
    const unit = event.deltaMode === 1 ? 18 : event.deltaMode === 2 ? scene.clientHeight : 1;
    moveCamera(-event.deltaX * unit, -event.deltaY * unit);
  }, { passive: false });
  document.addEventListener('keydown', event => {
    const step = 48;
    const moves = { ArrowLeft: [step, 0], ArrowRight: [-step, 0], ArrowUp: [0, step], ArrowDown: [0, -step] };
    if (!moves[event.key]) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    moveCamera(...moves[event.key]);
  }, true);
  $('#camera-home')?.addEventListener('click', () => {
    focusRegion('manor');
  });
  $('#camera-road')?.addEventListener('click', () => focusRegion('road'));
  $('#camera-wilds').addEventListener('click', () => switchMap(currentMap === 'city' ? 'wilds' : 'city'));
  $$('.zoom-btn').forEach(button => button.addEventListener('click', () => setZoom(Number(button.dataset.zoom))));
  window.addEventListener('resize', () => { sizeWorld(); renderCamera(); });
  sizeWorld();
  homeCamera();
}

// Mensagens ficam na tela até o jogador clicar. Repetida vira contador; acima de TOAST_MAX, sai a mais antiga.
// Todas vão também para o log da partida (state.messageLog), que guarda as últimas MESSAGE_LOG_MAX.
const TOAST_MAX = 3;
const MESSAGE_LOG_MAX = 200;
function logMessage(message) {
  if (!Array.isArray(state.messageLog)) state.messageLog = [];
  const last = state.messageLog[state.messageLog.length - 1];
  if (last && last.text === message) { last.n = (last.n || 1) + 1; last.t = state.minutes || 0; return; }
  state.messageLog.push({ t: state.minutes || 0, text: message });
  if (state.messageLog.length > MESSAGE_LOG_MAX) state.messageLog.splice(0, state.messageLog.length - MESSAGE_LOG_MAX);
}
const TOAST_SECONDS = 15;   // cada aviso some sozinho depois disso
function dismissToastLater(item) {
  clearTimeout(item.dismissTimer);
  item.dismissTimer = setTimeout(() => { item.remove(); updateToastClear(); }, TOAST_SECONDS * 1000);
}
function say(message, event = message) {
  logMessage(message);
  const toast = $('#toast');
  const last = [...toast.querySelectorAll('.toast-msg')].pop();
  if (last && last.dataset.text === message) {
    last.dataset.count = String(Number(last.dataset.count || 1) + 1);
    last.querySelector('em').textContent = `×${last.dataset.count}`;
    dismissToastLater(last);
  } else {
    const item = document.createElement('button');
    item.type = 'button';
    item.className = 'toast-msg';
    item.dataset.text = message;
    item.title = 'Clique para fechar';
    item.innerHTML = '<span></span><em></em>';
    item.querySelector('span').textContent = message;
    item.addEventListener('click', () => { item.remove(); updateToastClear(); });
    toast.insertBefore(item, toast.querySelector('.toast-clear'));
    dismissToastLater(item);
    while (toast.querySelectorAll('.toast-msg').length > TOAST_MAX) toast.querySelector('.toast-msg').remove();
  }
  updateToastClear();
  $('#event-line').textContent = event;
}
function updateToastClear() {
  const toast = $('#toast');
  let clear = toast.querySelector('.toast-clear');
  const count = toast.querySelectorAll('.toast-msg').length;
  if (count < 3) { clear?.remove(); return; }
  if (!clear) {
    clear = document.createElement('button');
    clear.type = 'button';
    clear.className = 'toast-clear';
    clear.textContent = 'Fechar todas';
    clear.addEventListener('click', () => { toast.querySelectorAll('.toast-msg').forEach(item => item.remove()); updateToastClear(); });
    toast.append(clear);
  }
}

const WORKER_UPKEEP = 0.075; // provisões por segundo, por trabalhador (4x mais devagar que os 0,3 antigos)
// Mercado: sempre aberto. Sem a construção, só a troca simples de trigo, alho e madeira, 25% pior:
// vende por 75% do preço e compra por 125%.
// Preços fixos em ouro por unidade. Comprar custa o dobro de vender.
const MARKET_GOODS = [
  { id: 'wheat', name: 'Trigo', icon: 'icon-market-wheat-photo-v01.png', sell: 1, buy: 2 },
  { id: 'flour', name: 'Farinha', icon: 'icon-market-flour-photo-v01.png', sell: 2, buy: 4 },
  { id: 'garlic', name: 'Alho', icon: 'icon-market-garlic-photo-v01.png', sell: 1, buy: 2 },
  { id: 'eggs', name: 'Ovos', icon: 'icon-market-eggs-photo-v01.png', sell: 2, buy: 4 },
  { id: 'milk', name: 'Leite', icon: 'icon-market-milk-photo-v01.png', sell: 2, buy: 4 },
  { id: 'bacon', name: 'Bacon', icon: 'icon-market-bacon-photo-v01.png', sell: 3, buy: 6 },
  { id: 'pork', name: 'Suínos', art: 'farm-bacon', sell: 3, buy: 6 },
  { id: 'chicken', name: 'Carne de frango', art: 'farm-eggs', sell: 2, buy: 4 },
  { id: 'beef', name: 'Carne de vaca', art: 'farm-milk', sell: 3, buy: 6 },
  { id: 'boarMeat', name: 'Carne de javali', art: 'farm-bacon', sell: 3, buy: 6 },
  { id: 'elkMeat', name: 'Carne de alce', art: 'farm-milk', sell: 4, buy: 8 },
  // Da Padaria. Vale um pouco mais que os ingredientes: o forno também cobra.
  { id: 'bread', name: 'Pão', art: BAKED_ART.bread, sell: 3, buy: 6 },
  { id: 'cheese', name: 'Queijo', art: BAKED_ART.cheese, sell: 5, buy: 10 },
  { id: 'quiche', name: 'Quiche Lorraine', art: BAKED_ART.quiche, sell: 9, buy: 18 },
  { id: 'sausage', name: 'Linguiça', art: 'sausage', sell: 5, buy: 10 },
  { id: 'nuggets', name: 'Chicken Nuggets', art: 'chicken-nuggets', sell: 4, buy: 8 },
  { id: 'wings', name: 'Buffalo Wings', art: 'buffalo-wings', sell: 4, buy: 8 },
  { id: 'hamburger', name: 'Hambúrguer', art: 'hamburger', sell: 5, buy: 10 },
  { id: 'steak', name: 'Filé', art: 'steak', sell: 6, buy: 12 },
  { id: 'parmesanSteak', name: 'Filé à Parmegiana', art: 'parmesan-steak', sell: 12, buy: 24 },
  { id: 'roastBoar', name: 'Javali Assado', art: 'steak', sell: 8, buy: 16 },
  { id: 'garlicBoar', name: 'Javali ao Alho', art: 'parmesan-steak', sell: 14, buy: 28 },
  { id: 'roastElk', name: 'Alce Assado', art: 'steak', sell: 9, buy: 18 },
  { id: 'elkStew', name: 'Ensopado de Alce', art: 'quiche', sell: 16, buy: 32 },
  { id: 'fish', name: 'Truta', art: 'fish', sell: 3, buy: 6 },
  { id: 'timber', name: 'Madeira', icon: 'icon-logs-photo-v01.png', sell: 2, buy: 4 },
];
const BASIC_GOODS = ['wheat', 'garlic', 'timber'];
const hasMarket = () => state.buildings.some(building => building.type === 'market' && isDone(building));
const marketGoods = () => hasMarket() ? MARKET_GOODS : MARKET_GOODS.filter(good => BASIC_GOODS.includes(good.id));
const unitPrice = (good, side) => hasMarket() ? good[side] : good[side] * (side === 'sell' ? 0.75 : 1.25);
const formatPrice = n => n.toLocaleString('pt-BR', { maximumFractionDigits: 2 });
const stockOf = id => id === 'timber' ? state.timber : (state.food[id] || 0);
function trade(id, amount) {
  const good = marketGoods().find(item => item.id === id);
  if (!good) return;
  if (amount > 0) {
    let qty = amount;
    if (id !== 'timber') qty = Math.min(qty, Math.floor(storageCap() - storedUnits()));
    if (qty <= 0) { say('O Depósito de Provisões está cheio. Não cabe nem um ovo.'); return; }
    const price = qty * unitPrice(good, 'buy');
    if (state.gold < price) { say(`Falta ouro. ${qty} de ${good.name.toLowerCase()} custam ${formatPrice(price)}.`); return; }
    state.gold -= price;
    if (id === 'timber') state.timber += qty; else state.food[id] = (state.food[id] || 0) + qty;
    say(`Comprou ${qty} de ${good.name.toLowerCase()} por ${formatPrice(price)} de ouro. O mercador sorriu demais.`);
  } else {
    const qty = Math.min(-amount, Math.floor(stockOf(id)));
    if (qty <= 0) { say(`Não tem ${good.name.toLowerCase()} pra vender.`); return; }
    const price = qty * unitPrice(good, 'sell');
    if (id === 'timber') state.timber -= qty; else state.food[id] -= qty;
    state.gold += price;
    say(`Vendeu ${qty} de ${good.name.toLowerCase()} por ${formatPrice(price)} de ouro. O mercador fingiu que era caro.`);
  }
  state.grain = totalFood();
  updateHud(); renderPanel();
}
function marketPanelHTML() {
  const coin = pixelIcon('coin', 14).replace('class="pixel-icon"', 'class="pixel-icon cost-ico"');
  return `<div class="panel-title"><h3>Mercado</h3><span>${coin} ${format(state.gold)} · depósito ${Math.floor(storedUnits())}/${storageCap()}</span></div>
    <div class="market-list">${marketGoods().sort((a, b) => a.name.localeCompare(b.name, 'pt-BR')).map(good => `<div class="market-row">
      <img src="${good.icon ? `public/art/${good.icon}` : `public/art/${good.art}-v01.webp`}" alt="">
      <span class="market-name"><b>${good.name}</b><small>Tem ${format(stockOf(good.id))}</small></span>
      <span class="market-side"><small>VENDER ${formatPrice(unitPrice(good, 'sell'))} ${coin}</small><span><button type="button" data-trade="${good.id}" data-amount="-1">1</button><button type="button" data-trade="${good.id}" data-amount="-10">10</button></span></span>
      <span class="market-side"><small>COMPRAR ${formatPrice(unitPrice(good, 'buy'))} ${coin}</small><span><button type="button" class="buy" data-trade="${good.id}" data-amount="1">1</button><button type="button" class="buy" data-trade="${good.id}" data-amount="10">10</button></span></span>
    </div>`).join('')}</div>
    <p class="build-note">${hasMarket() ? 'Armas e itens ainda não chegaram aos mercadores.' : 'Sem um Mercado, só passa a carroça de trigo, alho e madeira, e o carroceiro cobra 25% a mais. Construa um Mercado para preços justos e para negociar ovos, leite, bacon e o que sai da padaria.'}</p>`;
}

// Cabana do Lenhador: até 3 camponeses escalados pelo painel.
const LUMBER_RATE = [0, 0.05, 0.15, 0.25];
const lumberCrew = camp => state.workers.filter(worker => worker.alive && worker.workplace && worker.workplace.site === camp.site);
// Pedreira e Campo de Mineração: equipes escolhidas pelo jogador, com 2 vagas no nível 1 e 4 no nível 2.
const PIT_CREW_MAX = 4;
// A pedreira tira 20 kg de rubble a cada 200 segundos por trabalhador.
// Com 2 trabalhadores no nível 1: 0,20 kg/s; no nível 2, 0,40 kg/s com 4.
const PIT_GOODS = { quarry: { good: 'stone', name: 'pedra de construção', rate: 20 / 200 }, mine: { good: 'metal', name: 'lotes de minério', rate: 20 / 1200 } };
const MARBLE_RATE_PER_WORKER = 20 / 800;
const METAL_DEPOSIT_RANKS = [
  { id: 'I', name: 'Pobre', chance: { ironOre: 75, silverOre: 8, goldOre: 2 }, qty: { ironOre: [1, 2], silverOre: [1, 1], goldOre: [1, 1] } },
  { id: 'II', name: 'Comum', chance: { ironOre: 85, silverOre: 15, goldOre: 5 }, qty: { ironOre: [1, 3], silverOre: [1, 1], goldOre: [1, 1] } },
  { id: 'III', name: 'Bom', chance: { ironOre: 95, silverOre: 25, goldOre: 10 }, qty: { ironOre: [2, 4], silverOre: [1, 2], goldOre: [1, 1] } },
  { id: 'IV', name: 'Rico', chance: { ironOre: 100, silverOre: 40, goldOre: 20 }, qty: { ironOre: [3, 5], silverOre: [1, 2], goldOre: [1, 2] } },
  { id: 'V', name: 'Excepcional', chance: { ironOre: 100, silverOre: 65, goldOre: 35 }, qty: { ironOre: [4, 7], silverOre: [1, 3], goldOre: [1, 2] } },
];
const metalDepositRank = pit => METAL_DEPOSIT_RANKS.find(rank => rank.id === pit?.depositRank)?.id || ((pit?.mapSet || currentMapSet) === 2 ? 'IV' : 'II');
const metalRankData = pit => METAL_DEPOSIT_RANKS.find(rank => rank.id === metalDepositRank(pit)) || METAL_DEPOSIT_RANKS[1];
const MINERAL_SEAMS = {
  iron:   { rock: 'hematiteRock', label: 'Hematita', id: 'ironOre', assayPpm: 350000, ref: 20000000, depth: 'shallow' },
  copper: { rock: 'malachiteRock', label: 'Malaquita', id: 'copperOre', assayPpm: 35000, ref: 20000000, depth: 'shallow' },
  arsenicalCopper: { rock: 'arsenicalCopperOre', label: 'Veio regional de cobre arsenical', id: 'arsenicalCopperOre', assayPpm: 35000, ref: 300000, depth: 'shallow' },
  tin: { rock: 'cassiterite', label: 'Cassiterita', id: 'tinOre', assayPpm: 150000, ref: 600000, depth: 'shallow' },
  nativeCopper: { rock: 'nativeCopper', label: 'Ocorrência de cobre nativo', id: 'nativeCopper', assayPpm: 1000000, ref: 40000, depth: 'shallow', find: true },
  meteorite: { rock: 'meteoriteIron', label: 'Fragmentos de ferro meteórico', id: 'meteoriteFragments', assayPpm: 1000000, ref: 4000, depth: 'shallow', find: true },
  silver: { rock: 'silverGalena', label: 'Galena argentífera', id: 'silverOre', assayPpm: 500, ref: 40000000, depth: 'deep' },
  gold:   { rock: 'goldQuartz', label: 'Quartzo aurífero', id: 'goldOre', assayPpm: 12, ref: 100000000, depth: 'deep' },
};
const SEAM_PROFILE = {
  I: { iron: [1, 1200000], copper: [.25, 400000], silver: [.05, 240000], gold: [0, 0] },
  II: { iron: [1, 1200000], copper: [.25, 400000], silver: [.05, 240000], gold: [0, 0] },
  III: { iron: [1, 2000000], copper: [.5, 650000], silver: [.15, 480000], gold: [.05, 600000] },
  IV: { iron: [1, 3200000], copper: [.7, 1000000], silver: [.35, 800000], gold: [.15, 1000000] },
  V: { iron: [1, 4400000], copper: [.85, 1400000], silver: [.6, 1200000], gold: [.35, 1600000] },
};
const REF_GRADE_MULTIPLIER = { I: 0.75, II: 0.75, III: 1, IV: 1.25, V: 1.5 };
const hashSeed = value => { let h = 2166136261; for (const ch of String(value)) { h ^= ch.charCodeAt(0); h = Math.imul(h, 16777619); } return h >>> 0; };
const stableRoll = (key, salt = '') => { let x = hashSeed(`${key}:${salt}`); x ^= x << 13; x ^= x >>> 17; x ^= x << 5; return (x >>> 0) / 4294967296; };
const depositKey = (mapSet, id) => `${mapSet}:${id}`;
function makeMineralDeposit(place, mapSet) {
  const key = depositKey(mapSet, place.id), profile = SEAM_PROFILE[place.rank] || SEAM_PROFILE[mapSet === 2 ? 'III' : 'II'];
  const authored = mapSet === 2 && place.type === 'mine';
  const seams = [];
  for (const [mineral, spec] of Object.entries(MINERAL_SEAMS)) {
    const eligible = profile[mineral];
    if (!eligible) continue;
    const [chance, reserveKg] = eligible;
    if (authored || stableRoll(key, `exists:${mineral}`) < chance) {
      const deep = authored ? ['silver', 'gold'].includes(mineral) : mineral === 'iron' ? false : stableRoll(key, `depth:${mineral}`) < .35;
      const grade = authored ? 1 : (REF_GRADE_MULTIPLIER[place.rank] || 1);
      seams.push({ id: `${key}:seam:${mineral}`, mineral, rock: spec.rock, access: deep ? 'deep' : 'shallow', x: place.x, y: place.y, initialGrams: Math.round(reserveKg * 1000), remainingGrams: Math.round(reserveKg * 1000), gradePpm: Math.max(1, Math.round(spec.assayPpm * grade)) });
    }
  }
  if (!seams.some(seam => seam.mineral === 'iron')) {
    const spec = MINERAL_SEAMS.iron;
    seams.unshift({ id: `${key}:seam:iron`, mineral: 'iron', rock: spec.rock, access: 'shallow', x: place.x, y: place.y, initialGrams: 1200000000, remainingGrams: 1200000000, gradePpm: spec.assayPpm });
  }
  const tinSourceId = extractionPlaces('mine', 'wilds', mapSet)[0]?.id;
  if (String(place.id) === String(tinSourceId)) {
    const spec = MINERAL_SEAMS.tin, reserveKg = 600000;
    seams.push({ id: `${key}:authored:tin`, mineral: 'tin', rock: spec.rock, occurrence: 'ore-lot', access: 'shallow', x: place.x, y: place.y, initialGrams: reserveKg * 1000, remainingGrams: reserveKg * 1000, gradePpm: spec.assayPpm, authored: true });
  }
  if (authored && String(place.id) === String(tinSourceId)) {
    const authoredRegional = [
      ['arsenicalCopper', 300000, 35000, -1, 0], ['nativeCopper', 40000, 1000000, 1, 0], ['meteorite', 4, 1000000, 0, 1],
    ];
    authoredRegional.forEach(([mineral, reserveKg, gradePpm, dx, dy]) => {
      const spec = MINERAL_SEAMS[mineral];
      seams.push({ id: `${key}:authored:${mineral}`, mineral, rock: spec.rock, occurrence: spec.find ? 'metallic-find' : 'ore-lot', access: 'shallow', x: place.x + dx, y: place.y + dy, initialGrams: reserveKg * 1000, remainingGrams: reserveKg * 1000, gradePpm, authored: true });
    });
  }
  return { id: key, mapSet, markerId: String(place.id), x: place.x, y: place.y, seams };
}
function ensureMineralDeposit(place, mapSet = currentMapSet) {
  state.mineralDeposits ||= {};
  const key = depositKey(mapSet, place.id);
  if (!state.mineralDeposits[key]) state.mineralDeposits[key] = makeMineralDeposit(place, mapSet);
  return state.mineralDeposits[key];
}
function ensureStoneDeposit(place, mapSet = currentMapSet) {
  state.stoneDeposits ||= {};
  const key = depositKey(mapSet, place.id);
  if (!state.stoneDeposits[key]) {
    const initialUnits = Math.max(0, Number(place.initialUnits ?? 20000));
    state.stoneDeposits[key] = { id: key, mapSet, markerId: String(place.id), initialUnits, remainingUnits: initialUnits, x: place.x, y: place.y };
  }
  const deposit = state.stoneDeposits[key];
  if (!Array.isArray(deposit.faces)) {
    const reserve = Math.max(0, Number(deposit.remainingUnits ?? deposit.initialUnits ?? 20000));
    const authoredFaces = place.faces || place.geology?.faces;
    if (Array.isArray(authoredFaces) && authoredFaces.length) {
      deposit.rockFamily ||= place.rockFamily || place.geology?.rockFamily || authoredFaces[0].rockFamily || 'limestone';
      deposit.faces = authoredFaces.map((face, index) => ({
        id: String(face.id || `${key}:face:${index + 1}`), name: face.name || `Bench ${index + 1}`, rockFamily: face.rockFamily || deposit.rockFamily,
        benchLevel: Number(face.benchLevel || 0), workPosition: face.workPosition || { x: place.x, y: place.y }, weathering: face.weathering || 'sã',
        bedding: face.bedding || null, cleavage: face.cleavage || null, flatness: face.flatness ?? null, limeCompatible: !!face.limeCompatible,
        looseMassKg: Math.max(0, Number(face.looseMassKg || 0)), initialLooseMassKg: Math.max(0, Number(face.looseMassKg || 0)), blocked: false,
        blanks: (Array.isArray(face.blanks) ? face.blanks : []).map((blank, blankIndex) => ({ id: String(blank.id || `${key}:face:${index + 1}:blank-${blankIndex + 1}`), sourceFaceId: String(blank.sourceFaceId || `${key}:face:${index + 1}`), rockFamily: blank.rockFamily || face.rockFamily || place.rockFamily || place.geology?.rockFamily || 'limestone', template: blank.template || 'authored-stone-blank', massKg: Number(blank.massKg || 0), dimensionsCm: blank.dimensionsCm || [25, 20, 16], condition: blank.condition || 'sound', eligibleProducts: blank.eligibleProducts || [], limeCompatible: !!blank.limeCompatible || !!face.limeCompatible, flatness: blank.flatness ?? face.flatness ?? null, cleavage: blank.cleavage || face.cleavage || null })),
      }));
      deposit.facesInitialized = true;
      return deposit;
    }
    deposit.rockFamily ||= place.rockFamily || place.geology?.rockFamily || 'limestone';
    const specs = deposit.rockFamily === 'gritstone'
      ? [{ id: 'millstone-bed', name: 'Sound millstone bed', level: 0, weathering: 'sã', bedding: 'acamamento espesso', loose: 400, blankCount: 10, blankKg: 80, dimensions: [40, 40, 20], products: ['millstone'], lime: false }]
      : deposit.rockFamily === 'slate'
        ? [{ id: 'roofing-slate-bed', name: 'Flat cleavage bed', level: 0, weathering: 'foliada e regular', bedding: 'clivagem fina', cleavage: 'fine', flatness: .95, loose: 400, blankCount: 20, blankKg: 40, dimensions: [80, 50, 4], products: ['roofing-slate'], lime: false }]
        : deposit.rockFamily === 'granite'
          ? [{ id: 'granite-paving-bed', name: 'Sound granite face', level: 0, weathering: 'sã', bedding: 'juntas de corte', loose: 600, blankCount: 20, blankKg: 40, dimensions: [32, 20, 25], products: ['setts', 'road-aggregate'], lime: false }]
          : [
      { id: 'weathered-ledge', name: 'Weathered ledge', level: 0, weathering: 'fraturada e intemperizada', bedding: 'juntas curtas', loose: 1200, blankCount: 12, blankKg: 20, dimensions: [25, 20, 16], lime: false },
      { id: 'sound-bed', name: 'Sound bed', level: 0, weathering: 'sã', bedding: 'acamamento regular', loose: 400, blankCount: 40, blankKg: 20, dimensions: [25, 20, 16], lime: true },
      { id: 'lower-bed', name: 'Lower bed', level: 1, weathering: 'sã', bedding: 'banco espesso', loose: 600, blankCount: 20, blankKg: 120, dimensions: [60, 40, 20], lime: true },
    ];
    const nominal = specs.reduce((sum, face) => sum + face.loose + face.blankCount * face.blankKg, 0);
    const scale = nominal > 0 ? Math.min(1, reserve / nominal) : 0;
    deposit.faces = specs.map(face => {
      const count = Math.min(face.blankCount, Math.floor(face.blankCount * scale));
      const looseMassKg = Math.floor(face.loose * scale);
      const blanks = Array.from({ length: count }, (_, index) => ({ id: `${key}:${face.id}:blank-${index + 1}`, sourceFaceId: `${key}:face:${face.id}`, rockFamily: deposit.rockFamily, template: face.blankKg === 20 ? 'walling-blank-20kg' : face.products?.includes('millstone') ? 'millstone-blank-80kg' : face.products?.includes('roofing-slate') ? 'roofing-slab-40kg' : face.products?.includes('setts') ? 'granite-paving-blank-40kg' : 'dimension-blank-120kg', massKg: face.blankKg, dimensionsCm: face.dimensions, condition: face.weathering.includes('intemperizada') ? 'weathered' : face.weathering === 'sã' ? 'sound' : 'fractured', eligibleProducts: face.products || (face.blankKg === 20 ? ['walling', ...(deposit.rockFamily === 'limestone' && face.lime ? ['lime-feed'] : []), 'road-aggregate'] : ['dressed-stone', 'lintel']), limeCompatible: face.lime, cleavage: face.cleavage, flatness: face.flatness }));
      return { id: `${key}:face:${face.id}`, name: face.name, rockFamily: deposit.rockFamily, benchLevel: face.level, workPosition: { x: place.x, y: place.y }, weathering: face.weathering, bedding: face.bedding, cleavage: face.cleavage || null, flatness: face.flatness ?? null, limeCompatible: face.lime && deposit.rockFamily === 'limestone', looseMassKg, initialLooseMassKg: looseMassKg, blanks, blocked: false };
    });
    const faceMass = deposit.faces.reduce((sum, face) => sum + face.looseMassKg + face.blanks.reduce((mass, blank) => mass + blank.massKg, 0), 0);
    // Carry any pre-existing tracked reserve that exceeds the starter templates as loose limestone.
    deposit.faces[0].looseMassKg += Math.max(0, reserve - faceMass);
    deposit.faces[0].initialLooseMassKg += Math.max(0, reserve - faceMass);
    deposit.facesInitialized = true;
  }
  return deposit;
}
function stoneDepositForPit(pit) {
  if (!pit?.resourcePlaceId) return null;
  const place = { id: pit.resourcePlaceId, x: pit.x, y: pit.y, initialUnits: pit.quarryInitialUnits ?? ((pit.mapId || 'wilds') === 'city' ? 2400 : 20000) };
  return ensureStoneDeposit(place, pit.mapSet || currentMapSet);
}
function selectedQuarryFace(pit) {
  const deposit = stoneDepositForPit(pit);
  return deposit?.faces.find(face => face.id === pit.quarryFaceId) || deposit?.faces[0] || null;
}
function quarryInventory(pit) {
  const defaults = { wallBlanks: [], dimensionBlocks: [], wallingKg: 0, lintels: [], chipsKg: [], finesKg: 0, aggregateKg: 0, kilnFeedKg: 0, quicklimeKg: 0, kilnResidueKg: 0, gaseousLossKg: 0, limePuttyKg: 0, mortarKg: 0, plasterKg: 0, settsKg: 0, roofingSlateKg: 0, millstones: [], spoilKg: 0 };
  pit.quarryInventory = { ...defaults, ...(pit.quarryInventory || {}) };
  return pit.quarryInventory;
}
const QUARRY_ORDERS = [
  { id: 'rubble', label: 'Pedra de Construção', level: 0 },
  { id: 'marble', label: 'Mármore', level: 1 },
];
const QUARRY_ORDER_OUTPUT = {
  rubble: 'Pedra de Construção vai para obras comuns junto com madeira.', marble: 'Mármore: produção lenta, disponível no nível 2 e reservada para a mansão.', 'wall-blank': 'Um blank inteiro de alvenaria; fica separado do rubble.',
  'dimension-blank': 'Um bloco indivisível de 120 kg no pátio; requer dois trabalhadores e derrick.',
  'special-blank': 'Separa a peça candidata desta frente para slate, mó ou setts; tamanho e elegibilidade vêm da geologia.',
  'breakdown-blank': 'Quebra um blank intacto selecionado em fragmentos; remove para sempre sua elegibilidade dimensional.',
  'dress-wall': '16 kg walling stone + 3 kg chips + 1 kg fines por blank de 20 kg.',
  'dress-lintel': 'Um lintel de 90 kg + 24 kg chips + 6 kg fines por bloco dimensional.',
  'split-slate': '24 kg roofing slate + 12 kg chips + 4 kg fines por 40 kg slate.',
  'shape-millstone': 'Uma mó de 50 kg + 24 kg chips + 6 kg fines por blank de 80 kg.',
  'dress-setts': '28 kg granite setts + 9 kg chips + 3 kg fines por blank de pavimentação de 40 kg.',
  'grade-chips': '18 kg graded aggregate + 2 kg fines por 20 kg chips.',
  'lime-feed': '20 kg de chips calcários qualificados para o kiln.', 'burn-lime': '10 kg quicklime + 2 kg kiln residue por 20 kg kiln feed e 4 kg charcoal.',
};
function processQuarryOrder(pit, dt) {
  const deposit = stoneDepositForPit(pit), selected = selectedQuarryFace(pit), stock = quarryInventory(pit);
  const face = pit.quarryWork?.faceId ? deposit?.faces.find(item => item.id === pit.quarryWork.faceId) || selected : selected;
  if (!deposit || !face) { pit.workStatus = 'Nenhuma frente de pedra'; return; }
  const order = QUARRY_ORDERS.find(item => item.id === pit.quarryOrder) || QUARRY_ORDERS[0];
  if ((pit.level || 0) < order.level) { pit.workStatus = 'Bancada inferior inacessível'; return; }
  const crew = workingCrew(pit).filter(worker => assignExtractionTool(worker, pit));
  if (!crew.length) {
    pit.workStatus = workingCrew(pit).length ? 'Sem ferramenta disponível' : lumberCrew(pit).length ? 'Aguardando trabalhador no posto' : 'Sem trabalhador disponível';
    return;
  }
  const workMinutes = quarryCrewMinutes(crew, dt);
  if (order.id === 'marble') {
    if ((pit.level || 0) < 1) { pit.workStatus = 'Mármore requer pedreira nível 2'; return; }
    if (face.looseMassKg <= 0) { pit.workStatus = 'Sem pedra disponível nesta frente'; return; }
    const mass = Math.min(face.looseMassKg, pitRate(pit) * dt, Math.max(0, 80 - (pit.stockMarble || 0)));
    if (mass <= 0) { pit.workStatus = 'Estoque de mármore no posto cheio'; return; }
    face.looseMassKg -= mass; deposit.remainingUnits = Math.max(0, deposit.remainingUnits - mass); pit.stockMarble = (pit.stockMarble || 0) + mass; pit.workStatus = `Extraindo mármore · ${face.name}`; return;
  }
  if (order.id === 'rubble') {
    if (face.looseMassKg <= 0) { pit.workStatus = 'Sem Pedra de Construção nesta frente'; return; }
    const mass = Math.min(face.looseMassKg, pitRate(pit) * dt, Math.max(0, 200 - (pit.stockStone || 0)));
    if (mass <= 0) { pit.workStatus = 'Estoque de Pedra de Construção cheio'; return; }
    face.looseMassKg -= mass; deposit.remainingUnits = Math.max(0, deposit.remainingUnits - mass); pit.stockStone = (pit.stockStone || 0) + mass; pit.workStatus = `Extraindo Pedra de Construção · ${face.name}`; return;
  }
  if (order.id === 'wall-blank' || order.id === 'dimension-blank') {
    const wanted = order.id === 'wall-blank' ? 20 : 120;
    if (wanted === 120 && (crew.length < 2 || !pit.derrick || pit.derrick.capacityKg < 120 || !pit.sharedCart)) { pit.workStatus = 'Bloco dimensional requer dois trabalhadores, derrick e carrinho'; return; }
    if (wanted === 120 && face.benchLevel < 1) { pit.workStatus = 'Bancada inferior inacessível'; return; }
    let job = pit.quarryWork;
    if (!job) {
      const blank = face.blanks.find(piece => piece.massKg === wanted && (piece.condition === 'sound' || (order.id === 'wall-blank' && piece.condition === 'weathered')) && !piece.reservedBy);
      if (!blank) { pit.workStatus = wanted === 120 ? 'Sem blank dimensional compatível' : 'Sem blank inteiro compatível'; return; }
      blank.reservedBy = `quarry:${pit.site}`;
      pit.quarryWork = job = { id: `quarry-work-${pit.site}-${state.ticks}`, order: order.id, faceId: face.id, blankId: blank.id, elapsedWorkerMinutes: 0, requiredWorkerMinutes: wanted === 120 ? 120 : 25, separated: false };
    }
    job.elapsedWorkerMinutes += workMinutes;
    if (job.elapsedWorkerMinutes >= job.requiredWorkerMinutes) {
      const index = face.blanks.findIndex(piece => piece.id === job.blankId);
      const [piece] = index >= 0 ? face.blanks.splice(index, 1) : [];
      if (piece) {
        delete piece.reservedBy; deposit.remainingUnits = Math.max(0, deposit.remainingUnits - piece.massKg); job.separated = true;
        piece.location = order.id === 'wall-blank' ? 'quarry-bench' : 'quarry-loading-pad';
        piece.transportRequirement = piece.massKg > 40 ? 'derrick + cart · carga compartilhada até 160 kg' : 'carga normal';
        if (order.id === 'wall-blank') stock.wallBlanks.push(piece); else stock.dimensionBlocks.push(piece);
        pit.loadingPad = order.id === 'dimension-blank' ? { pieceId: piece.id, cargoId: job.id, state: 'separated-on-pad', location: 'quarry-loading-pad' } : pit.loadingPad;
        pit.workStatus = `Blank separado · ${piece.massKg} kg · ${piece.transportRequirement}`;
      }
      pit.quarryWork = null;
    } else pit.workStatus = `${order.label} · ${Math.floor(job.elapsedWorkerMinutes)}/${job.requiredWorkerMinutes} worker-min`;
    return;
  }
  if (order.id === 'special-blank') {
    let job = pit.quarryWork;
    if (!job) {
      const blank = face.blanks.find(piece => piece.condition === 'sound' && piece.eligibleProducts?.some(product => ['roofing-slate', 'millstone', 'setts'].includes(product)) && !piece.reservedBy);
      if (!blank) { pit.workStatus = 'Nenhuma peça especial intacta compatível nesta frente'; return; }
      if (blank.massKg > 40 && (crew.length < 2 || !pit.derrick || pit.derrick.capacityKg < blank.massKg)) { pit.workStatus = 'Blank especial pesado requer nível 2, dois trabalhadores e derrick'; return; }
      blank.reservedBy = `quarry:${pit.site}`;
      pit.quarryWork = job = { id: `quarry-work-${pit.site}-${state.ticks}`, order: order.id, faceId: face.id, blankId: blank.id, elapsedWorkerMinutes: 0, requiredWorkerMinutes: 25 * blank.massKg / 20, separated: false };
    }
    const blank = face.blanks.find(piece => piece.id === job.blankId);
    if (!blank || !blank.eligibleProducts?.some(product => ['roofing-slate', 'millstone', 'setts'].includes(product))) { pit.workStatus = 'Peça reservada não atende à ordem especial'; return; }
    job.elapsedWorkerMinutes += workMinutes;
    if (job.elapsedWorkerMinutes >= job.requiredWorkerMinutes) {
      const index = face.blanks.findIndex(piece => piece.id === job.blankId);
      const [piece] = index >= 0 ? face.blanks.splice(index, 1) : [];
      if (piece) {
        delete piece.reservedBy; deposit.remainingUnits = Math.max(0, deposit.remainingUnits - piece.massKg);
        piece.location = 'quarry-masonry-bench'; piece.transportRequirement = piece.massKg > 40 ? 'derrick + carrinho compartilhado' : 'carga normal';
        stock.dimensionBlocks.push(piece);
        if (piece.massKg > 40) pit.loadingPad = { pieceId: piece.id, cargoId: job.id, state: 'separated-on-pad', location: 'quarry-loading-pad' };
      }
      pit.quarryWork = null; pit.workStatus = `Blank especial separado · ${piece?.massKg || 0} kg · ${piece?.transportRequirement || ''}`;
    } else pit.workStatus = `Separando blank especial · ${Math.floor(job.elapsedWorkerMinutes)}/${Math.floor(job.requiredWorkerMinutes)} worker-min`;
    return;
  }
  if (order.id === 'breakdown-blank') {
    let job = pit.quarryWork;
    if (!job) {
      const blank = face.blanks.find(piece => piece.condition === 'sound' && !piece.reservedBy);
      if (!blank) { pit.workStatus = 'Sem blank intacto para quebra deliberada'; return; }
      blank.reservedBy = `quarry:${pit.site}`;
      pit.quarryWork = job = { id: `quarry-work-${pit.site}-${state.ticks}`, order: order.id, faceId: face.id, blankId: blank.id, elapsedWorkerMinutes: 0, requiredWorkerMinutes: 15 * blank.massKg / 20, separated: false };
    }
    job.elapsedWorkerMinutes += workMinutes;
    if (job.elapsedWorkerMinutes >= job.requiredWorkerMinutes) {
      const index = face.blanks.findIndex(piece => piece.id === job.blankId);
      const [piece] = index >= 0 ? face.blanks.splice(index, 1) : [];
      if (piece) {
        delete piece.reservedBy; deposit.remainingUnits = Math.max(0, deposit.remainingUnits - piece.massKg);
        piece.eligibleProducts = []; piece.condition = 'broken'; piece.location = 'quarry-chip-stock';
        stock.chipsKg.push({ id: `${piece.id}:broken`, massKg: piece.massKg, rockFamily: piece.rockFamily || face.rockFamily, limeCompatible: !!piece.limeCompatible && face.limeCompatible, sourceFaceId: face.id });
      }
      pit.quarryWork = null; pit.workStatus = `Blank quebrado deliberadamente · ${piece?.massKg || 0} kg de chips; reserva intacta debitada`;
    } else pit.workStatus = `Quebrando blank · ${Math.floor(job.elapsedWorkerMinutes)}/${Math.floor(job.requiredWorkerMinutes)} worker-min`;
    return;
  }
  if (order.id === 'dress-wall') {
    const blank = stock.wallBlanks[0];
    if (!blank || blank.massKg !== 20 || blank.condition !== 'sound') { pit.workStatus = 'Sem blank de alvenaria para dress'; return; }
    pit.quarryWork ||= { id: `quarry-work-${pit.site}-${state.ticks}`, order: order.id, blankId: blank.id, elapsedWorkerMinutes: 0, requiredWorkerMinutes: 18 };
    pit.quarryWork.elapsedWorkerMinutes += workMinutes;
    if (pit.quarryWork.elapsedWorkerMinutes >= pit.quarryWork.requiredWorkerMinutes) {
      stock.wallBlanks.shift(); stock.wallingKg += 16; stock.chipsKg.push({ massKg: 3, rockFamily: blank.rockFamily || face.rockFamily, limeCompatible: !!blank.limeCompatible, sourceFaceId: face.id }); stock.finesKg += 1; pit.quarryWork = null; pit.workStatus = 'Bloco vestido · 16 kg walling, 3 kg chips, 1 kg fines';
    } else pit.workStatus = `Dressing block · ${Math.floor(pit.quarryWork.elapsedWorkerMinutes)}/18 worker-min`;
    return;
  }
  if (order.id === 'dress-lintel') {
    const cityBlocks = state.quarryMaterials?.dimensionBlocks || [];
    const blank = stock.dimensionBlocks[0] || cityBlocks[0];
    if (!blank || blank.rockFamily !== 'limestone' || blank.condition !== 'sound' || !blank.eligibleProducts?.includes('lintel') || blank.massKg < 120 || (blank.dimensionsCm || []).join('x') !== '60x40x20') { pit.workStatus = 'A lintel requires one sound 60 × 40 × 20 cm limestone block'; return; }
    if (crew.length < 2) { pit.workStatus = 'Lintel dressing requires two workers and heavy handling'; return; }
    pit.quarryWork ||= { id: `quarry-work-${pit.site}-${state.ticks}`, order: order.id, blankId: blank.id, elapsedWorkerMinutes: 0, requiredWorkerMinutes: 55 };
    pit.quarryWork.elapsedWorkerMinutes += workMinutes;
    if (pit.quarryWork.elapsedWorkerMinutes >= 55) {
      const localIndex = stock.dimensionBlocks.indexOf(blank);
      if (localIndex >= 0) stock.dimensionBlocks.splice(localIndex, 1); else cityBlocks.splice(cityBlocks.indexOf(blank), 1);
      const output = { id: `${pit.quarryWork.id}:lintel`, massKg: 90, dimensionsCm: [60, 30, 20], condition: 'sound' };
      if (localIndex >= 0) stock.lintels.push(output); else { state.quarryMaterials.lintels ||= []; state.quarryMaterials.lintels.push(output); }
      stock.chipsKg.push({ massKg: 24, rockFamily: 'limestone', limeCompatible: !!blank.limeCompatible, sourceFaceId: blank.sourceFaceId }); stock.finesKg += 6; pit.quarryWork = null; pit.workStatus = 'Lintel vestido · 90 kg lintel, 24 kg chips, 6 kg fines';
    }
    else pit.workStatus = `Dressing lintel · ${Math.floor(pit.quarryWork.elapsedWorkerMinutes)}/55 worker-min`;
    return;
  }
  if (order.id === 'split-slate') {
    if (face.rockFamily !== 'slate' || !face.cleavage || !face.flatness || Number(face.flatness) < .7) { pit.workStatus = 'Slate face lacks suitable cleavage or flatness'; return; }
    const blank = stock.dimensionBlocks.find(piece => piece.massKg >= 40 && piece.cleavage && Number(piece.flatness) >= .7);
    if (!blank) { pit.workStatus = 'No intact roofing-grade slate slab'; return; }
    pit.quarryWork ||= { id: `quarry-work-${pit.site}-${state.ticks}`, order: order.id, blankId: blank.id, elapsedWorkerMinutes: 0, requiredWorkerMinutes: 40 };
    pit.quarryWork.elapsedWorkerMinutes += workMinutes;
    if (pit.quarryWork.elapsedWorkerMinutes >= 40) { stock.dimensionBlocks.splice(stock.dimensionBlocks.indexOf(blank), 1); stock.roofingSlateKg += 24; stock.chipsKg.push({ massKg: 12, rockFamily: 'slate', limeCompatible: false }); stock.finesKg += 4; pit.quarryWork = null; pit.workStatus = '40 kg slate → 24 kg roofing slate + 12 kg chips + 4 kg fines'; }
    else pit.workStatus = `Splitting slate · ${Math.floor(pit.quarryWork.elapsedWorkerMinutes)}/40 worker-min`;
    return;
  }
  if (order.id === 'shape-millstone') {
    const blank = stock.dimensionBlocks.find(piece => piece.massKg >= 80 && piece.eligibleProducts?.includes('millstone'));
    if (face.rockFamily !== 'gritstone' || !blank) { pit.workStatus = 'Requires one 80 kg millstone-grade gritstone blank'; return; }
    pit.quarryWork ||= { id: `quarry-work-${pit.site}-${state.ticks}`, order: order.id, blankId: blank.id, elapsedWorkerMinutes: 0, requiredWorkerMinutes: 90 };
    pit.quarryWork.elapsedWorkerMinutes += workMinutes;
    if (pit.quarryWork.elapsedWorkerMinutes >= 90) { stock.dimensionBlocks.splice(stock.dimensionBlocks.indexOf(blank), 1); stock.millstones.push({ id: `${pit.quarryWork.id}:millstone`, massKg: 50, condition: 'sound' }); stock.chipsKg.push({ massKg: 24, rockFamily: 'gritstone', limeCompatible: false }); stock.finesKg += 6; pit.quarryWork = null; pit.workStatus = 'Millstone shaped · 50 kg stone + 24 kg chips + 6 kg fines'; }
    else pit.workStatus = `Shaping millstone · ${Math.floor(pit.quarryWork.elapsedWorkerMinutes)}/90 worker-min`;
    return;
  }
  if (order.id === 'dress-setts') {
    if (face.rockFamily !== 'granite') { pit.workStatus = 'Setts require granite paving stock'; return; }
    const blank = stock.dimensionBlocks.find(piece => piece.massKg >= 40 && piece.rockFamily === 'granite' && piece.eligibleProducts?.includes('setts'))
      || stock.wallBlanks.find(piece => piece.massKg >= 40 && piece.rockFamily === 'granite' && piece.eligibleProducts?.includes('setts'));
    if (!blank) { pit.workStatus = 'Falta um blank de granito de 40 kg para setts'; return; }
    pit.quarryWork ||= { id: `quarry-work-${pit.site}-${state.ticks}`, order: order.id, elapsedWorkerMinutes: 0, requiredWorkerMinutes: 55 };
    pit.quarryWork.elapsedWorkerMinutes += workMinutes;
    pit.quarryWork.blankId = blank.id;
    if (pit.quarryWork.elapsedWorkerMinutes >= 55) { const list = stock.dimensionBlocks.includes(blank) ? stock.dimensionBlocks : stock.wallBlanks; list.splice(list.indexOf(blank), 1); stock.settsKg += 28; stock.chipsKg.push({ massKg: 9, rockFamily: 'granite', limeCompatible: false }); stock.finesKg += 3; pit.quarryWork = null; pit.workStatus = '40 kg granite → 28 kg setts + 9 kg chips + 3 kg fines'; }
    else pit.workStatus = `Dressing granite setts · ${Math.floor(pit.quarryWork.elapsedWorkerMinutes)}/55 worker-min`;
    return;
  }
  if (order.id === 'grade-chips') {
    const batch = stock.chipsKg.reduce((sum, chip) => sum + (['limestone', 'granite'].includes(chip.rockFamily) ? chip.massKg : 0), 0);
    if (batch < 20) { pit.workStatus = 'Faltam 20 kg de chips compatíveis'; return; }
    pit.quarryWork ||= { id: `quarry-work-${pit.site}-${state.ticks}`, order: order.id, elapsedWorkerMinutes: 0, requiredWorkerMinutes: 15 };
    pit.quarryWork.elapsedWorkerMinutes += workMinutes;
    if (pit.quarryWork.elapsedWorkerMinutes >= 15) { consumeQuarryChips(stock.chipsKg, 20, chip => ['limestone', 'granite'].includes(chip.rockFamily)); stock.aggregateKg += 18; stock.finesKg += 2; pit.quarryWork = null; pit.workStatus = '20 kg chips → 18 kg aggregate + 2 kg fines'; }
    else pit.workStatus = `Grading chips · ${Math.floor(pit.quarryWork.elapsedWorkerMinutes)}/15 worker-min`;
    return;
  }
  if (order.id === 'lime-feed') {
    const batch = stock.chipsKg.reduce((sum, chip) => sum + (chip.rockFamily === 'limestone' && chip.limeCompatible ? chip.massKg : 0), 0);
    if (batch < 20) { pit.workStatus = 'Incompatible kiln feed · 20 kg limestone chips required'; return; }
    pit.quarryWork ||= { id: `quarry-work-${pit.site}-${state.ticks}`, order: order.id, elapsedWorkerMinutes: 0, requiredWorkerMinutes: 10 };
    pit.quarryWork.elapsedWorkerMinutes += workMinutes;
    if (pit.quarryWork.elapsedWorkerMinutes >= 10) { consumeQuarryChips(stock.chipsKg, 20, chip => chip.rockFamily === 'limestone' && chip.limeCompatible); stock.kilnFeedKg += 20; pit.quarryWork = null; pit.workStatus = '20 kg limestone chips sorted to kiln feed'; }
    else pit.workStatus = `Sorting kiln feed · ${Math.floor(pit.quarryWork.elapsedWorkerMinutes)}/10 worker-min`;
    return;
  }
  if ((state.charcoal || 0) < 4) { pit.workStatus = 'Faltam 4 kg de charcoal'; return; }
  if (stock.kilnFeedKg < 20) { pit.workStatus = 'Faltam 20 kg de kiln feed qualificado'; return; }
  pit.quarryWork ||= { id: `quarry-work-${pit.site}-${state.ticks}`, order: order.id, elapsedWorkerMinutes: 0, requiredWorkerMinutes: 70 };
  pit.quarryWork.elapsedWorkerMinutes += workMinutes;
  if (pit.quarryWork.elapsedWorkerMinutes >= 70) { state.charcoal -= 4; stock.kilnFeedKg -= 20; stock.quicklimeKg += 10; stock.kilnResidueKg += 2; stock.gaseousLossKg += 8; pit.quarryWork = null; pit.workStatus = '20 kg kiln feed → 10 kg quicklime + 2 kg kiln residue + 8 kg process loss'; }
  else pit.workStatus = `Burning lime · ${Math.floor(pit.quarryWork.elapsedWorkerMinutes)}/70 worker-min`;
}
function consumeQuarryChips(chips, amount, predicate = () => true) {
  let remaining = amount;
  for (let i = 0; i < chips.length && remaining > 0; i++) {
    const chip = chips[i];
    if (!predicate(chip)) continue;
    const take = Math.min(remaining, chip.massKg); chip.massKg -= take; remaining -= take;
  }
  for (let i = chips.length - 1; i >= 0; i--) if (chips[i].massKg <= 1e-6) chips.splice(i, 1);
}
function quarryCrewMinutes(crew, dt) {
  return crew.reduce((sum, worker) => {
    const tool = state.extractionTools.find(item => item.id === worker.extractionToolId);
    if (!tool) return sum;
    const sharp = tool.edge === 'steel' ? 1.15 : 1;
    return sum + dt * sharp;
  }, 0);
}
function depositForPit(pit) {
  if (!pit?.resourcePlaceId) return null;
  const place = extractionPlaces('mine').find(item => String(item.id) === String(pit.resourcePlaceId)) || { id: pit.resourcePlaceId, x: pit.x, y: pit.y, type: 'mine', rank: pit.depositRank };
  return ensureMineralDeposit(place, pit.mapSet || currentMapSet);
}
const pitSeams = pit => depositForPit(pit)?.seams || [];
const selectedSeam = pit => pitSeams(pit).find(seam => seam.id === pit.selectedSeamId) || pitSeams(pit).find(seam => seam.mineral === 'iron') || pitSeams(pit)[0] || null;
const workerLimitForPit = pit => (pit.level || 0) >= 1 ? 4 : 2;
const workerLoadForPit = pit => (pit.level || 0) >= 1 ? 40000 : 20000;
const oreName = mineral => MINERAL_SEAMS[mineral]?.label || mineral;
function assignExtractionTool(actor, pit) {
  state.extractionTools ||= [];
  let tool = state.extractionTools.find(item => item.id === actor.extractionToolId && item.durability > 0 && item.type === pit.type);
  if (tool) return tool;
  tool = state.extractionTools.find(item => item.type === pit.type && item.durability > 0 && (item.assignedTo == null || item.assignedTo === actor.id));
  if (tool) { tool.assignedTo = actor.id; actor.extractionToolId = tool.id; }
  return tool || null;
}
function addMetalStock(id, amount) {
  const free = Math.max(0, (storageCap() - storedUnits()) * 100);
  const added = Math.min(amount, free);
  state.metals[id] = (state.metals[id] || 0) + added;
  if (added < amount && !state.storageWarned) { state.storageWarned = true; say('O depósito está cheio. A carga ficou aguardando em vez de se perder.'); }
  return added;
}
function consumeOreLots(id, amount) {
  const lots = state.oreLots || (state.oreLots = []);
  let remaining = amount, gradeTotal = 0, consumed = 0;
  for (let i = 0; i < lots.length && remaining > 1e-9; i++) {
    const lot = lots[i];
    if (lot.id !== id || lot.quantityKg <= 0) continue;
    const take = Math.min(remaining, lot.quantityKg);
    gradeTotal += take * lot.gradePpm; consumed += take; remaining -= take; lot.quantityKg -= take;
  }
  for (let i = lots.length - 1; i >= 0; i--) if (lots[i].quantityKg <= 1e-9) lots.splice(i, 1);
  return consumed > 1e-9 ? gradeTotal / consumed : null;
}
function advanceMines(dt) {
  state.buildings.filter(building => building.type === 'quarry' && isDone(building)).forEach(pit => {
    const deposit = stoneDepositForPit(pit);
    if (!pit.underConstruction && deposit && deposit.remainingUnits > 0) processQuarryOrder(pit, dt);
    else if (!deposit || deposit.remainingUnits <= 0) pit.workStatus = 'Depósito de pedra esgotado';
    advanceQuarryCart(pit, dt);
  });
  state.buildings.filter(building => building.type === 'mine' && isDone(building)).forEach(pit => {
    const seam = selectedSeam(pit);
    if (!seam || seam.remainingGrams <= 0) { pit.workStatus = 'Veio esgotado'; return; }
    if (pit.underConstruction) return;
    if (!workingCrew(pit).length) { pit.workStatus = lumberCrew(pit).length ? 'Aguardando trabalhador no posto' : 'Sem trabalhador disponível'; return; }
    if (!pitRate(pit)) { pit.workStatus = 'Sem ferramenta disponível'; return; }
    const cap = Math.max(0, 200000 - (pit.stockGrams || 0));
    if (!cap) { pit.workStatus = 'Estoque do posto cheio'; return; }
    const throughput = Math.max(0, pitRate(pit)) * 1000 * dt;
    const mined = Math.min(Math.floor(throughput), seam.remainingGrams, cap);
    if (mined > 0) { seam.remainingGrams -= mined; pit.stockGrams = (pit.stockGrams || 0) + mined; pit.workStatus = 'Extraindo'; }
  });
}
function quarryMaterialMass(materials = state.quarryMaterials) {
  if (!materials) return 0;
  return (materials.wallingKg || 0) + (materials.dimensionBlocks || []).reduce((sum, piece) => sum + (piece.massKg || 0), 0)
    + (materials.lintels || []).reduce((sum, piece) => sum + (piece.massKg || 0), 0) + (materials.aggregateKg || 0)
    + (materials.settsKg || 0) + (materials.roofingSlateKg || 0) + (materials.quicklimeKg || 0)
    + (materials.roofingSlatesKg || 0) + (materials.limePuttyKg || 0) + (materials.mortarKg || 0) + (materials.plasterKg || 0)
    + (materials.millstones || []).reduce((sum, piece) => sum + (piece.massKg || 0), 0);
}
function advanceQuarryCart(pit, dt) {
  const cart = pit.sharedCart;
  if (!cart || (!pit.loadingPad && !cart.cargo)) return;
  const stock = quarryInventory(pit);
  if (cart.cargo) {
    cart.tripRemaining = Math.max(0, (cart.tripRemaining || 0) - dt);
    pit.workStatus = cart.tripRemaining > 0 ? `Carrinho a caminho da City · ${Math.ceil(cart.tripRemaining)} min` : 'Carrinho chegou à City';
    if (cart.tripRemaining > 0) return;
    const cargo = cart.cargo;
    const occupiedWithoutCargo = storedUnits() - cargo.massKg;
    if (storageCap() - occupiedWithoutCargo < cargo.massKg) { pit.workStatus = 'Depósito da City sem espaço para bloco dimensional'; return; }
    state.quarryMaterials ||= newGameState().quarryMaterials;
    state.quarryMaterials.dimensionBlocks ||= [];
    cargo.location = 'city-quarry-stock';
    state.quarryMaterials.dimensionBlocks.push(cargo);
    cart.cargo = null; cart.cargoId = null; cart.tripRemaining = 0;
    pit.workStatus = `Bloco dimensional entregue à City · ${cargo.massKg} kg`;
    return;
  }
  const piece = stock.dimensionBlocks.find(item => item.id === pit.loadingPad.pieceId);
  if (!piece) { pit.loadingPad = null; pit.workStatus = 'Pátio de carga vazio'; return; }
  if ((cart.capacityKg || 160) < piece.massKg) { pit.workStatus = `Carrinho suporta ${cart.capacityKg || 160} kg; bloco tem ${piece.massKg} kg`; return; }
  if (storageCap() - storedUnits() < piece.massKg) { pit.workStatus = 'Depósito da City sem espaço para reservar o bloco'; return; }
  stock.dimensionBlocks.splice(stock.dimensionBlocks.indexOf(piece), 1);
  piece.location = 'shared-quarry-cart';
  piece.transportRequirement = `carrinho compartilhado · ${cart.capacityKg || 160} kg por viagem`;
  cart.cargo = piece; cart.cargoId = piece.id; cart.tripRemaining = Math.max(1, Number(cart.tripMinutes || 8));
  pit.loadingPad = null;
  pit.workStatus = `Bloco carregado uma única vez · ${piece.massKg} kg · em trânsito para a City`;
}
const isPit = building => !!PIT_GOODS[building?.type];
// Mapa onde a construção fica. Sem registro (saves antigos, casarão), é City.
const buildingMapId = building => building?.mapId || 'city';
const pitRate = pit => pit.type === 'mine'
  ? workingCrew(pit).reduce((sum, actor) => { const tool = (state.extractionTools || []).find(item => item.id === actor.extractionToolId && item.durability > 0); return sum + (tool ? (tool.edge === 'steel' ? 20 / 960 : 20 / 1200) * (1 + gearBonus(actor, 'work') + tradeWorkBonus(actor, 'mine') / 2) : 0); }, 0)
  : pit.type === 'quarry' ? (pit.quarryOrder === 'marble' ? MARBLE_RATE_PER_WORKER : PIT_GOODS.quarry.rate) * workingCrew(pit).filter(actor => assignExtractionTool(actor, pit)).length * (1 + workingCrewWork(pit)) : 0;
// Metal rende 0,025: duas casas cortariam pra 0,03.
const fineRate = value => value.toLocaleString('pt-BR', { minimumFractionDigits: 2, maximumFractionDigits: 3 });
const CREW_TYPES = ['lumber', 'quarry', 'mine'];
function freeWorkers() {
  return state.workers.filter(worker => worker.alive && worker.buildSite == null && !CREW_TYPES.includes(worker.workplace?.type));
}
// Clique no nome: entra ou sai da equipe. Quem entra larga o trabalho que tinha.
function togglePitWorker(site, workerId) {
  const pit = state.buildings.find(building => building.site === site && isPit(building) && isDone(building));
  const worker = state.workers.find(item => String(item.id) === String(workerId) && item.alive);
  if (!pit || !worker) return;
  const name = buildingName(pit);
  if (worker.workplace?.site === pit.site) {
    worker.workplace = null;
    const tool = state.extractionTools?.find(item => item.id === worker.extractionToolId);
    if (tool) tool.assignedTo = null;
    worker.extractionToolId = null;
    say(`${worker.name} largou a picareta ${pit.type === 'quarry' ? 'na Pedreira' : 'no Campo de Mineração'} e voltou pro trabalho de antes.`);
  } else {
    if (lumberCrew(pit).length >= workerLimitForPit(pit)) { say(`${name} já tem ${workerLimitForPit(pit)} trabalhadores neste nível.`); return; }
    worker.workplace = pit;
    worker.jobSite = null;
    interruptTavernVisit(worker);
    say(pit.type === 'quarry' ? `${worker.name} foi quebrar pedra. Disse que as costas aguentam. Não aguentam.` : `${worker.name} foi cavar metal. Volta mais sujo e mais pobre.`);
  }
  worker.route = []; worker.finalPoint = null; worker.wait = 0; worker.workTrip = false;
  renderPanel(); updateHud();
}
function pitBoxHTML(pit) {
  const crew = lumberCrew(pit);
  const people = state.workers.filter(worker => worker.alive).sort((a, b) => Number(b.workplace?.site === pit.site) - Number(a.workplace?.site === pit.site));
  const doing = worker => worker.workplace?.site === pit.site ? 'trabalhando aqui' : worker.buildSite != null ? 'em obra' : worker.workplace ? workPlaceName(worker.workplace).replace(/^(na|no) /, '') : workplaceOf(worker) ? workPlaceName(workplaceOf(worker)).replace(/^(na|no) /, '') : 'à toa';
  const maxCrew = workerLimitForPit(pit);
  if (pit.type === 'quarry') {
    const deposit = stoneDepositForPit(pit);
    const selectedOrder = QUARRY_ORDERS.find(order => order.id === pit.quarryOrder) || QUARRY_ORDERS[0];
    return `<section class="inspector-section"><h4>Produção da Pedreira</h4>
      <label class="pit-hint">Buscar <select data-quarry-order="${pit.site}">${QUARRY_ORDERS.map(order => `<option value="${order.id}"${order.id === (pit.quarryOrder || 'rubble') ? ' selected' : ''}${order.level > (pit.level || 0) ? ' disabled' : ''}>${order.label}${order.level > (pit.level || 0) ? ' · nível 2' : ''}</option>`).join('')}</select></label>
      <p class="pit-hint">${QUARRY_ORDER_OUTPUT[selectedOrder.id] || ''}</p>
      <p class="pit-hint">${pit.workStatus || 'Aguardando trabalho'} · no posto: ${formatMass(pit.stockStone || 0)} kg de Pedra de Construção e ${formatMass(pit.stockMarble || 0)} kg de Mármore · reserva: ${formatMass(deposit?.remainingUnits || 0)} kg</p>
      ${pit.level ? '' : `<button type="button" class="hut-upgrade" data-pit-upgrade="${pit.site}" ${state.stone < 20 || (state.metals.iron || 0) < 1 ? 'disabled' : ''}>Ampliar nível 2 · 20 kg de Pedra de Construção e 1 kg de ferro</button>`}
      <h4>Equipe · ${crew.length} de ${maxCrew}</h4><small class="pit-hint">Clique no nome para colocar ou tirar.</small>
      <div class="pit-crew">${people.map(worker => { const here = worker.workplace?.site === pit.site; const tool = state.extractionTools?.find(item => item.id === worker.extractionToolId); return `<button type="button" class="pit-worker${here ? ' here' : ''}" data-pit="${pit.site}" data-worker="${worker.id}" aria-pressed="${here}" ${!here && crew.length >= maxCrew ? 'disabled' : ''}><b>${here ? '✓ ' : ''}${worker.name}</b><small>${doing(worker)} · ${tool ? `Ferramenta ${formatMass(tool.durability)}/${tool.maxDurability} cargas` : 'sem ferramenta'} · ${tradeLevel(worker, 'extractor')} nv extrator</small></button>`; }).join('') || '<div class="empty-state">Nenhum camponês. Construa um casebre.</div>'}</div></section>`;
  }
  if (pit.type === 'mine') {
    const seams = pitSeams(pit), active = selectedSeam(pit);
    return `<section class="inspector-section"><h4>Veio de trabalho</h4>
      <label class="pit-hint">Selecionar veio <select data-seam-pit="${pit.site}">${seams.map(seam => `<option value="${seam.id}"${seam.id === active?.id ? ' selected' : ''}${seam.access === 'deep' && (pit.level || 0) < 1 ? ' disabled' : ''}>${oreName(seam.mineral)} · ${seam.access === 'deep' ? 'profundo' : 'raso'}${seam.access === 'deep' && (pit.level || 0) < 1 ? ' · nível 2' : ''}</option>`).join('')}</select></label>
      <p class="pit-hint">Teor aproximado ${(active?.gradePpm || 0).toLocaleString('pt-BR')} ppm · ${(active?.remainingGrams || 0).toLocaleString('pt-BR')} g restantes · ${(pit.stockGrams || 0).toLocaleString('pt-BR')} g no posto</p>
      <p class="pit-hint">${pit.workStatus || 'Aguardando trabalho'} · carga máxima ${(workerLoadForPit(pit) / 1000)} kg por trabalhador</p>
      ${pit.level ? '' : `<button type="button" class="hut-upgrade" data-pit-upgrade="${pit.site}" ${state.stone < 20 || (state.metals.iron || 0) < 1 ? 'disabled' : ''}>Ampliar para nível 2 · 20 kg Pedra de Construção, 1 kg ferro</button>`}
      <h4>Equipe · ${crew.length} de ${maxCrew}</h4><small class="pit-hint">Clique no nome para colocar ou tirar.</small>
      <div class="pit-crew">${people.map(worker => { const here = worker.workplace?.site === pit.site; const tool = state.extractionTools?.find(item => item.id === worker.extractionToolId); return `<button type="button" class="pit-worker${here ? ' here' : ''}" data-pit="${pit.site}" data-worker="${worker.id}" aria-pressed="${here}" ${!here && crew.length >= maxCrew ? 'disabled' : ''}><b>${here ? '✓ ' : ''}${worker.name}</b><small>${doing(worker)} · ${tool ? `Ferramenta ${tool.edge === 'steel' ? 'aço' : 'ferro'} ${formatMass(tool.durability)}/${tool.maxDurability} cargas` : 'sem ferramenta'} · Extrator nv ${tradeLevel(worker, 'extractor')}</small></button>`; }).join('') || '<div class="empty-state">Nenhum camponês. Construa um casebre.</div>'}</div></section>`;
  }
  const reserve = stoneDepositForPit(pit)?.remainingUnits || 0;
  return `<section class="inspector-section"><p class="pit-hint">${pit.workStatus || 'Aguardando trabalho'} · ${formatMass(pit.stockStone || 0)} pedra no posto · ${formatMass(reserve)} de reserva restante</p><h4>Quem trabalha aqui · ${crew.length} de ${maxCrew}</h4><small class="pit-hint">Clique no nome para colocar ou tirar.</small>${pit.level ? '' : `<button type="button" class="hut-upgrade" data-pit-upgrade="${pit.site}" ${state.stone < 20 || (state.metals.iron || 0) < 1 ? 'disabled' : ''}>Ampliar nível 2 · 20 pedra, 1 kg ferro</button>`}<div class="pit-crew">${people.map(worker => { const here = worker.workplace?.site === pit.site; const tool = state.extractionTools?.find(item => item.id === worker.extractionToolId); return `<button type="button" class="pit-worker${here ? ' here' : ''}" data-pit="${pit.site}" data-worker="${worker.id}" aria-pressed="${here}" ${!here && crew.length >= maxCrew ? 'disabled' : ''}><b>${here ? '✓ ' : ''}${worker.name}</b><small>${doing(worker)} · ${tool ? `Ferramenta ${formatMass(tool.durability)}/${tool.maxDurability} cargas` : 'sem ferramenta'} · Extrator nv ${tradeLevel(worker, 'extractor')}</small></button>`; }).join('') || '<div class="empty-state">Nenhum camponês. Construa um casebre.</div>'}</div></section>`;
}
function upgradePit(site) {
  const pit = state.buildings.find(building => building.site === site && isPit(building) && isDone(building));
  if (!pit || (pit.level || 0) >= 1 || state.stone < 20 || (state.metals.iron || 0) < 1) return;
  state.stone -= 20; state.metals.iron -= 1;
  startUpgrade(pit, { kind: 'pit' }, `Ampliação de ${buildingName(pit)} para o nível 2`);
}
function setLumberCrew(site, change) {
  const camp = state.buildings.find(building => building.site === site && building.type === 'lumber' && isDone(building));
  if (!camp) return;
  const crew = lumberCrew(camp);
  if (change > 0) {
    if (crew.length >= 3) { say('A cabana já tem três lenhadores. Um quarto só ia atrapalhar.'); return; }
    const worker = freeWorkers()[0];
    if (!worker) { say('Nenhum camponês livre. Construa um casebre ou tire alguém de outra cabana.'); return; }
    worker.workplace = camp; interruptTavernVisit(worker); worker.route = []; worker.finalPoint = null; worker.wait = 0; worker.workTrip = false;
    say(`${worker.name} foi cortar lenha. Pediu um machado melhor. Não ganhou.`);
  } else {
    const worker = crew[crew.length - 1];
    if (!worker) return;
    worker.workplace = null; worker.route = []; worker.finalPoint = null; worker.wait = 0; worker.workTrip = false;
    say(`${worker.name} largou o machado e voltou para a fazenda.`);
  }
  renderPanel(); updateHud();
}
function lumberBoxHTML(camp) {
  const crew = lumberCrew(camp);
  return `<section class="inspector-section"><h4>Equipe</h4>
    <div class="guild-branches"><button type="button" class="hut-upgrade" data-lumber="${camp.site}" data-change="1" ${crew.length >= 3 ? 'disabled' : ''}>+1 camponês</button><button type="button" class="hut-upgrade" data-lumber="${camp.site}" data-change="-1" ${crew.length ? '' : 'disabled'}>−1 camponês</button></div></section>`;
}

function rates() {
  const sum = state.buildings.reduce((sum, building) => {
    const def = BUILDINGS.find(item => item.id === building.type);
    if (def && isDone(building)) {
      // Provisões comestíveis, pelo quanto alimentam: trigo cru não conta; o moinho conta a farinha que mói.
      // A padaria não soma: o produto alimenta o mesmo que os ingredientes que gasta.
      if (building.type === 'farm') sum.grain += (farmFoodId(building) === 'wheat' ? 0 : farmOutput(building) * foodFeeds(farmFoodId(building))) + (farmType(building.produce).secondary ? farmSecondaryRate(building) * foodFeeds(farmType(building.produce).secondary) : 0);
      else if (!['mill', 'bakery'].includes(building.type)) sum.grain += def.grain;
      sum.people += def.people;
    }
    return sum;
  }, { gold: 0, grain: 0, timber: 0, stone: 0, marble: 0, metal: 0, people: 0 });
  state.buildings.filter(building => isPit(building) && isDone(building)).forEach(pit => {
    if (pit.type === 'mine') sum.metal += pitRate(pit);
    else if (pit.quarryOrder === 'marble') sum.marble += pitRate(pit);
    else sum.stone += pitRate(pit);
  });
  state.workers.filter(worker => worker.alive && worker.profession === 'fisher').forEach(worker => { sum.grain += FISHING_RATE * (1 + skillOf(worker, 'fishing') / 100); });
  sum.timber += state.buildings.filter(building => building.type === 'lumber' && isDone(building)).reduce((total, camp) => total + LUMBER_RATE[workingCrew(camp).length] * (1 + workingCrewWork(camp)), 0);
  // Farinha: o moinho só mói o trigo que existe; sem estoque, fica limitado ao que as fazendas colhem.
  sum.grain += (state.food.wheat || 0) > 0.5 ? millCapacity() : Math.min(millCapacity(), foodProduction().wheat);
  sum.grain -= foodDemand();
  return sum;
}

// ==========================================================================
// Impostos: cobrados uma vez por dia, à meia-noite. A 10%, cada camponês paga 2 de ouro.
// A taverna paga pelo uso: cada hora bebendo vale 1 (camponês), 2 (herói) ou 3 (herói de prestígio), a 10%.
// Tudo escala com a porcentagem escolhida.
// ==========================================================================
const taxFactor = () => (state.taxPercent ?? 10) / 10;
// Cada personagem tem a própria bolsa. Trabalhadores recebem salário dos cofres do monarca à meia-noite
// e pagam dela o imposto e a taverna. Heróis enchem a bolsa com loot.
const purseOf = actor => actor?.purse || 0;
const dailyWages = () => state.workers.filter(worker => worker.alive).length * WORKER_WAGE;
function lootHero(hero, amount) { hero.purse = purseOf(hero) + amount; }
const openTavern = () => state.buildings.find(building => building.type === 'tavern' && isDone(building)) || null;
const tavernRate = actor => actor.className === 'Peasant' ? 1 : actor.prestige ? 3 : 2;
function dailyTaxEstimate() {
  return state.workers.filter(worker => worker.alive).reduce((sum, worker) => sum + Math.min(purseOf(worker), 2 * taxFactor()), 0) + (state.tavernTab || 0) * taxFactor();
}
function tavernSpot(actor, tavern) {
  if (!actor.tavernSpot || actor.tavernSpot.site !== tavern.site) {
    actor.tavernSpot = { site: tavern.site, x: tavern.x + (Math.random() * 4 - 2), y: Math.min(198, tavern.y + 1 + Math.random() * 1.5) };
  }
  return actor.tavernSpot;
}
function goDrink(actor, tavern) {
  const spot = tavernSpot(actor, tavern);
  if (Math.hypot(actor.x - spot.x, actor.y - spot.y) < 1.2) { actor.wait = 2; return; }
  if (actor.className === 'Peasant' && (actor.mapId || 'city') === 'city') routeToPoint(actor, { x: spot.x, y: spot.y - 1.2 });
  else { routeActor(actor, nearestNode(spot)); actor.finalPoint = { x: spot.x, y: spot.y }; }
}
function isDrinking(actor) {
  const tavern = openTavern();
  return !!tavern && actor.drinkUntil > state.minutes && actor.tavernSpot?.site === tavern.site
    && Math.hypot(actor.x - actor.tavernSpot.x, actor.y - actor.tavernSpot.y) < 1.5;
}
const alcoholismOf = actor => Math.max(0, Math.min(100, Number(actor?.skills?.alcoholism) || 0));
function interruptTavernVisit(actor) {
  if (!actor) return;
  actor.drinkUntil = 0;
  actor.tavernTripSite = null;
  actor.tavernBreakMinutes = 0;
  actor.nextTavernAt = state.minutes + 720 + Math.floor(Math.random() * 360);
  actor.route = []; actor.goal = null; actor.finalPoint = null; actor.wait = 0;
}
function tavernPause(actor) {
  if (actor.buildSite != null || actor.carrying) return false;
  const tavern = openTavern();
  if (!tavern) {
    actor.drinkUntil = 0;
    actor.tavernTripSite = null;
    return false;
  }
  if (actor.drinkUntil > state.minutes || actor.tavernTripSite === tavern.site) {
    actor.task = actor.drinkUntil > state.minutes ? 'Bebendo na taverna' : 'Indo fazer uma pausa na taverna';
    if ((actor.mapId || 'city') !== 'city') { travelToMap(actor, 'city', actor.mapSet || 1); return true; }
    const spot = tavernSpot(actor, tavern);
    if (Math.hypot(actor.x - spot.x, actor.y - spot.y) >= 1.2) { goDrink(actor, tavern); return true; }
    if (actor.drinkUntil <= state.minutes) {
      const level = alcoholismOf(actor);
      const breakMinutes = actor.tavernBreakMinutes || (8 + Math.round(level * 0.52));
      const interval = Math.max(360, 1080 + Math.floor(Math.random() * 360) - Math.round(level * 3.2));
      actor.drinkUntil = state.minutes + breakMinutes;
      actor.nextTavernAt = state.minutes + interval;
      actor.tavernTripSite = null;
      actor.tavernBreakMinutes = 0;
      actor.task = 'Bebendo na taverna';
      actor.wait = 1;
    }
    return true;
  }
  if (!Number.isFinite(actor.nextTavernAt)) actor.nextTavernAt = state.minutes + 600 + Math.floor(Math.random() * 480);
  if (state.minutes < actor.nextTavernAt) return false;
  if (purseOf(actor) < 0.2) { actor.nextTavernAt = state.minutes + 180; return false; }
  actor.tavernTripSite = tavern.site;
  actor.tavernBreakMinutes = 8 + Math.round(alcoholismOf(actor) * 0.52);
  actor.task = 'Indo fazer uma pausa na taverna';
  if ((actor.mapId || 'city') !== 'city') { travelToMap(actor, 'city', actor.mapSet || 1); return true; }
  goDrink(actor, tavern);
  return true;
}
// Uma vez por segundo de jogo (speed minutos): anota o consumo de quem está bebendo.
function accrueTavern(minutesPassed) {
  [...state.heroes, ...state.workers].filter(actor => actor.alive).forEach(actor => {
    actor.skills ||= {};
    const level = alcoholismOf(actor);
    if (isDrinking(actor)) {
      const paid = Math.min(purseOf(actor), tavernRate(actor) * minutesPassed / 60);
      actor.purse = purseOf(actor) - paid;
      state.tavernTab = (state.tavernTab || 0) + paid;
      actor.skills.alcoholism = Math.min(100, level + minutesPassed * 0.6 * (1 - level / 130));
    } else {
      const working = actor.className !== 'Peasant' || !!(actor.workplace || actor.jobSite || actor.buildSite != null || actor.profession);
      actor.skills.alcoholism = Math.max(0, level - minutesPassed * (working ? 0.0025 : 0.0006));
    }
  });
}
function collectTaxes() {
  const workers = state.workers.filter(worker => worker.alive);
  // Primeiro o imposto, tirado da bolsa de cada um; quem não tem, paga o que tem.
  let fromPeasants = 0;
  workers.forEach(worker => {
    const paid = Math.min(purseOf(worker), 2 * taxFactor());
    worker.purse = purseOf(worker) - paid;
    fromPeasants += paid;
  });
  const fromTavern = (state.tavernTab || 0) * taxFactor();
  state.tavernTab = 0;
  state.armorySales = 0;
  state.gold += fromPeasants + fromTavern;
  // Depois o salário, direto dos cofres do monarca. Sem ouro, alguém fica sem.
  const paidCount = Math.min(workers.length, Math.floor(state.gold / WORKER_WAGE));
  workers.slice(0, paidCount).forEach(worker => { worker.purse = purseOf(worker) + WORKER_WAGE; });
  state.gold -= paidCount * WORKER_WAGE;
  const unpaid = workers.length - paidCount;
  say(`Impostos a ${state.taxPercent}%: ${format(fromPeasants)} de ouro dos camponeses${openTavern() ? ` e ${format(fromTavern)} da taverna` : ''}. Salários: ${format(paidCount * WORKER_WAGE)} de ouro${unpaid ? `; ${unpaid} ficaram sem receber` : ''}.`,
    unpaid ? 'O cofre não deu pra todos. Alguns trabalharam de graça hoje. Lembram disso.' : 'Os camponeses pagaram e receberam. O ouro deu uma volta e voltou menor.');
}

function updateHud() {
  const r = rates();
  setHudAmount('#gold', state.gold);
  state.grain = totalFood();
  setHudAmount('#grain', state.grain);
  $('#storage-cap').textContent = storageCap();
  $('#grain').closest('.stat').classList.toggle('full', storedUnits() >= storageCap() - 1);
  $('#happiness').textContent = Math.round(state.happiness);
  $('#happiness-label').textContent = happinessLabel(state.happiness);
  $('#favor').textContent = Math.round(state.favor ?? 50);
  renderDemandAlerts();
  $('#favor-label').textContent = `${favorLabel(state.favor ?? 50)}${state.demands?.length ? ` · ${state.demands.length} ${state.demands.length === 1 ? 'tributo' : 'tributos'}` : ''}`;
  $('#favor').closest('.stat').dataset.mood = (state.favor ?? 50) < 40 ? 'low' : (state.favor ?? 50) < 60 ? 'mid' : 'high';
  $('#happiness').closest('.stat').dataset.mood = state.happiness < 40 ? 'low' : state.happiness < 60 ? 'mid' : 'high';
  setHudAmount('#timber', state.timber);
  const goldNet = dailyTaxEstimate() - dailyWages();
  $('#gold-rate').textContent = `${goldNet < 0 ? '−' : '+'}${format(Math.abs(goldNet))}/dia`;
  $('#grain-rate').textContent = `${r.grain < 0 ? '−' : '+'}${Math.abs(r.grain).toFixed(1).replace('.', ',')}/s`;
  $('#grain-rate').classList.toggle('negative', r.grain < 0);
  $('#timber-rate').textContent = `+${rateNumber(r.timber)}/s`;
  setHudAmount('#stone', state.stone || 0);
  $('#stone-rate').textContent = `+${fineRate(r.stone)}/s`;
  setHudAmount('#marble', state.marble || 0);
  $('#marble-rate').textContent = `+${fineRate(r.marble)}/s`;
  state.metal = metalStockTotal(); // mantém o campo antigo para saves e integrações
  setHudAmount('#metal', state.metal || 0);
  $('#metal-rate').textContent = `+${fineRate(r.metal)}/s`;
  if ($('#ember')) { setHudAmount('#ember', state.ember || 0); $('#ember-rate').textContent = `${remainsCount()} ${remainsCount() === 1 ? 'resto' : 'restos'}`; }
  // Personagens: quantos existem e quantos o domínio comporta.
  const heroes = state.heroes.filter(hero => hero.alive).length;
  const heroCap = heroCapacity();
  const workers = state.workers.filter(worker => worker.alive).length;
  const workerCap = workerCapacity();
  $('#hero-count').textContent = heroes;
  $('#hero-cap').textContent = heroCap;
  $('#hero-count').closest('.stat').classList.toggle('full', heroes >= heroCap);
  $('#worker-count').textContent = workers;
  $('#worker-cap').textContent = workerCap;
  $('#tax-pct').textContent = `${state.taxPercent}%`;
  $('#tax-preview').textContent = `≈ ${format(dailyTaxEstimate())} de imposto · −${format(dailyWages())} de salários por dia`;
  updateGoal();
  const hh = Math.floor(state.minutes / 60) % 24;
  const mm = state.minutes % 60;
  $('#clock').textContent = `${String(hh).padStart(2, '0')}:${String(mm).padStart(2, '0')}`;
  $('#date').textContent = `${dayPeriod()} · ${dateLabel()}`;
  if ((state.goalStep || 0) >= SCENARIO_GOALS.length && !state.won) {
    state.won = true;
    say('Cenário conquistado: o domínio agora paga as próprias dívidas.', 'Os credores receberam a notícia. Infelizmente, também receberam o endereço.');
    if (!state.retinue && game.slot) { menu.stack = []; showScreen('scenarioEnd', { servants: [], protectors: [] }); }
  }
}

// Objetivos do Cenário 01, um depois do outro. O cenário termina quando o último é cumprido.
const SCENARIO_GOALS = [
  { title: 'Acumule 600 moedas',
    progress: () => ({ have: state.gold, need: 600, text: `${format(state.gold)} / 600` }),
    done: ['Dívidas quitadas. Os credores levaram as 600 moedas e deixaram o recibo.', 'Destrua um covil: na aba Heróis, ofereça uma recompensa para destruir um covil.'] },
  { title: 'Destrua um covil: na aba Heróis, ofereça uma recompensa para destruir um covil.',
    progress: () => { const destroyed = (state.lairs || []).some(lair => lair.destroyed); return { have: destroyed ? 1 : 0, need: 1, text: destroyed ? 'Covil destruído' : 'Nenhum covil destruído' }; },
    done: ['O covil foi destruído.', ''] },
  { title: 'Pão, Cerveja e Gente: 10 camponeses e uma Taverna',
    progress: () => {
      const people = state.workers.filter(worker => worker.alive).length;
      const tavern = state.buildings.some(building => building.type === 'tavern' && isDone(building));
      return { have: Math.min(10, people) + (tavern ? 1 : 0), need: 11, text: `${Math.min(10, people)} / 10 · ${tavern ? 'taverna aberta' : 'sem taverna'}` };
    },
    done: ['A Taverna lotou e ninguém foi enforcado esta semana. Recorde no Vale do Luto.', 'O vale funciona. Mal, mas funciona.'] },
];
function updateGoal() {
  const step = Math.min(state.goalStep || 0, SCENARIO_GOALS.length - 1);
  const goal = SCENARIO_GOALS[step];
  const { have, need, text } = goal.progress();
  $('#objective-title').textContent = goal.title;
  $('#objective-step').textContent = 'atual';
  $('#objective-text').textContent = text;
  $('#objective-bar').style.width = `${Math.max(0, Math.min(100, have / need * 100))}%`;
  renderObjectiveJournal();
  if ((state.goalStep || 0) >= SCENARIO_GOALS.length || have < need) return;
  state.goalStep = (state.goalStep || 0) + 1;
  if (state.goalStep === 1) payDebt();
  say(...goal.done);
  if (state.goalStep < SCENARIO_GOALS.length) updateGoal();
}

function renderObjectiveJournal() {
  const list = $('#objective-list');
  if (!list) return;
  const step = state.goalStep || 0;
  list.innerHTML = SCENARIO_GOALS.map((goal, index) => {
    const completed = index < step;
    const current = index === step;
    const status = completed ? 'Concluído' : current ? 'Atual' : 'A fazer';
    const detail = completed ? goal.done[0] : current ? goal.progress().text : '';
    return `<li class="objective-journal-item${current ? ' is-current' : ''}"><span>${status}</span><b>${goal.title}</b>${detail ? `<small>${detail}</small>` : ''}</li>`;
  }).join('');
}

// Primeiro objetivo cumprido: os credores levam as 600 moedas e os nobres dão Favor.
const DEBT_GOLD = 600;
const DEBT_FAVOR = 16;
function payDebt() {
  state.gold -= DEBT_GOLD;
  changeFavor(DEBT_FAVOR);
  if (game.slot) { menu.stack = []; showScreen('debtPaid'); }
}

const BRANCH_NAMES = { Bishop: 'Templo Bishop', Cleric: 'Treino Cleric', Elementalist: 'Torre dos Elementalistas', Warlock: 'Torre dos Arcanistas' };
function siteLabel(building) {
  if (building.type === 'manor') return 'O Casarão';
  const def = BUILDINGS.find(item => item.id === building.type);
  return building.type === 'farm' ? building.produce === 'eggs' ? 'Galinheiro' : `Fazenda de ${farmType(building.produce).name}` : building.type === 'hut' ? (building.big ? 'Casebre Grande' : 'Casebre Pequeno') : building.branch ? (BRANCH_NAMES[building.branch] || `Guilda ${building.branch}`) : def.name;
}
function siteArt(building) {
  const art = building.type === 'farm' ? farmType(building.produce).art : building.type === 'hut' && building.big ? 'hut-big' : BUILDING_ART[building.type];
  if (building.type === 'quarry' || building.type === 'mine') return `${building.type}-l${(building.level || 0) + 1}`;
  return PRODUCER_TYPES.includes(building.type) ? levelArt(art, building) : art;
}

// Vida do casarão: aparece acima dele só quando está ferido.
function renderManorHp() {
  const layer = $('#site-layer');
  if (!layer) return;
  let tag = $('.manor-hp', layer);
  const manor = MANOR();
  if (!(manor.damage > 0) || currentMapSet !== 1 || currentMap !== 'city') { tag?.remove(); return; }
  if (!tag) { tag = document.createElement('div'); tag.className = 'manor-hp'; layer.append(tag); }
  const hp = buildingHp(manor), maxHp = buildingMaxHp(manor);
  tag.style.setProperty('--x', `${manor.x}%`);
  tag.style.setProperty('--y', `${(manor.y - 9) / WORLD_Y_UNITS * 100}%`);
  tag.innerHTML = `<small>O Casarão · ${manor.noWood ? 'falta madeira' : manor.builderId != null ? 'consertando' : 'vida'} ${hp}/${maxHp}</small><i style="--hp:${Math.round(hp / maxHp * 100)}%"></i>`;
}

// O casarão caiu: o herdeiro morre lá dentro e o jogo acaba. O save fica como estava no último salvamento.
function manorFell() {
  state.gameOver = true;
  renderManorHp();
  say('O Casarão desabou. O herdeiro estava lá dentro.');
  menu.stack = [];
  showScreen('gameOver');
}

// Uma construção por botão no mapa, no ponto onde foi erguida.
function renderSites() {
  const layer = $('#site-layer');
  const visibleBuildings = state.buildings.filter(building => (building.mapId || 'city') === currentMap && (building.mapSet || 1) === currentMapSet);
  const ids = new Set(visibleBuildings.map(building => String(building.site)));
  $$('.site', layer).filter(site => !ids.has(site.dataset.site)).forEach(site => site.remove());
  const moving = state.movingSite != null;
  renderBountyFlags();
  renderManorHp();
  $('#map-scene').classList.toggle('placing', !!state.selectedBuild || moving || !!state.placingBounty);
  if (!state.selectedBuild && !moving) $('#placement-preview').hidden = true;
  visibleBuildings.forEach(building => {
    const idx = building.site;
    let site = $(`.site[data-site="${idx}"]`, layer);
    if (!site) {
      site = document.createElement('button');
      site.type = 'button';
      site.className = 'site built';
      site.dataset.site = String(idx);
      layer.append(site);
    }
    site.style.setProperty('--x', `${building.x}%`);
    site.style.setProperty('--y', `${building.y / WORLD_Y_UNITS * 100}%`);
    site.classList.toggle('selected', state.selectedSite === idx);
    site.classList.toggle('constructing', !!building.underConstruction);
    site.classList.toggle('moving', state.movingSite === idx);
    site.classList.toggle('ruined', !!building.ruined);
    site.style.zIndex = String(Math.round(building.y * 10));
    {
      const label = siteLabel(building);
      const art = building.type === 'farm' ? farmType(building.produce).art : building.type === 'hut' && building.big ? 'hut-big' : BUILDING_ART[building.type];
      const constructionStage = building.underConstruction
        ? ((building.type === 'quarry' || building.type === 'mine' ? (building.progress || 0) < BUILD_TIME * 0.4 : (building.progress || 0) < BUILD_TIME / 2) ? 'early' : 'mid')
        : null;
      site.classList.toggle('just-built', Date.now() - building.builtAt < 1400);
      // Upgrade: a animação continua de onde parou mesmo que o mapa seja redesenhado no meio dela.
      const upgradeAge = Date.now() - (building.upgradedAt || 0);
      const upgrading = !building.underConstruction && upgradeAge < UPGRADE_FX_MS;
      site.classList.toggle('just-upgraded', upgrading);
      site.style.setProperty('--up-delay', `${upgrading ? -upgradeAge : 0}ms`);
      site.dataset.art = art;
      site.dataset.type = building.type;
      const displayedArt = siteArt(building);
      const artAspect = options.buildingArtStyle === 'dark' || constructionStage ? BUILDING_ART_ASPECTS[displayedArt] || BUILDING_ART_ASPECTS[art] || 1 : displayedArt === 'manor' ? 1.5 : 1;
      site.innerHTML = `<span class="building-art" style="--art-aspect:${artAspect};${buildingImageStyle(siteArt(building), constructionStage, building)}" aria-hidden="true">${building.type === 'mill' && !building.underConstruction ? '<span class="mill-sails"></span>' : ''}</span>${upgrading ? `<span class="upgrade-burst" aria-hidden="true">${'<i></i>'.repeat(8)}<b>${building.upgradeText || 'Melhorado'}</b></span>` : ''}${building.underConstruction ? `<small class="build-progress">${constructionLabel(building)}</small><small class="build-spacer" aria-hidden="true">Em obra</small>` : `<small>${building.damage > 0 ? `${label} · ${building.ruined ? 'em ruínas' : 'vida'} ${buildingHp(building)}/${buildingMaxHp(building)}` : label}</small>`}`;
      site.setAttribute('aria-label', `${label}. Toque para ver opções.`);
    }
  });
  renderLairs();
  renderFishingSpots();
}

// Animação de upgrade: a construção pula, brilha, solta brasas e mostra o que ganhou.
const UPGRADE_FX_MS = 1800;
function markUpgrade(building, text) {
  building.upgradedAt = Date.now();
  building.upgradeText = text;
  setTimeout(renderSites, UPGRADE_FX_MS + 60);
}

// Se a construção cabe neste ponto: dentro do mapa, fora das áreas bloqueadas e longe das outras.

function renderLairs() {
  const layer = $('#lair-layer');
  if (!layer) return;
  const visibleIds = new Set();
  (state.lairs || []).filter(lair => lair.active).forEach(lair => {
    visibleIds.add(lair.id);
    let marker = $(`.lair-marker[data-lair="${lair.id}"]`, layer);
    if (!marker) { marker = document.createElement('button'); marker.type = 'button'; marker.className = 'lair-marker'; marker.dataset.lair = lair.id; layer.append(marker); }
    marker.style.left = `${lair.x}%`;
    marker.style.top = `${lair.y / WORLD_Y_UNITS * 100}%`;
    marker.style.zIndex = String(Math.round(lair.y * 10));
    marker.hidden = (lair.mapId || 'city') !== currentMap || (lair.mapSet || 1) !== currentMapSet;
    marker.classList.toggle('level-two', lair.level >= 2);
    marker.classList.toggle('selected', selectedLairId === lair.id);
    marker.setAttribute('aria-label', `${lair.name}. Clique para ver informações.`);
    marker.title = `${lair.name} · ver informações`;
    const image = `public/art/${lair.art}`;
    if (marker.dataset.image !== image) { marker.innerHTML = `<img src="${image}" alt=""><small>${lair.name}</small>`; marker.dataset.image = image; }
    const label = lair.damage > 0 ? `${lair.name} · vida ${lairHp(lair)}/${lairMaxHp(lair)}` : lair.name;
    if ($('small', marker).textContent !== label) $('small', marker).textContent = label;
  });
  (state.remainsPiles || []).forEach(pile => {
    visibleIds.add(pile.id);
    let marker = $(`.lair-marker[data-lair="${pile.id}"]`, layer);
    if (!marker) {
      marker = document.createElement('div');
      marker.className = 'lair-marker remains-pile';
      marker.dataset.lair = pile.id;
      marker.innerHTML = `<img src="${remainsImage(pile.grade)}" alt=""><small>${remainsName(pile.grade)}</small>`;
      layer.append(marker);
    }
    marker.style.left = `${pile.x}%`;
    marker.style.top = `${pile.y / WORLD_Y_UNITS * 100}%`;
    marker.style.zIndex = String(Math.round(pile.y * 10));
    marker.hidden = (pile.mapId || 'city') !== currentMap || (pile.mapSet || 1) !== currentMapSet;
    marker.setAttribute('aria-label', remainsName(pile.grade));
  });
  (state.wildMeatPiles || []).forEach(pile => {
    visibleIds.add(pile.id);
    let marker = $(`.lair-marker[data-lair="${pile.id}"]`, layer);
    if (!marker) { marker = document.createElement('div'); marker.className = 'lair-marker remains-pile meat-pile'; marker.dataset.lair = pile.id; layer.append(marker); }
    marker.style.left = `${pile.x}%`; marker.style.top = `${pile.y / WORLD_Y_UNITS * 100}%`;
    marker.style.zIndex = String(Math.round(pile.y * 10));
    marker.hidden = (pile.mapId || 'wilds') !== currentMap || (pile.mapSet || 2) !== currentMapSet;
    const name = foodName(pile.food);
    marker.setAttribute('aria-label', `${format(pile.qty)} de ${name} no chão`);
    const image = `public/art/${pile.food === 'boarMeat' ? 'wild-boar' : 'elk'}-v01.webp`;
    if (marker.dataset.image !== image) { marker.innerHTML = `<img src="${image}" alt=""><small></small>`; marker.dataset.image = image; }
    $('small', marker).textContent = `${format(pile.qty)} · ${name}`;
  });
  $$('.lair-marker', layer).filter(marker => !visibleIds.has(marker.dataset.lair)).forEach(marker => marker.remove());
  if (selectedLairId) renderInspector();
}

function extractionPlaces(type, mapId = currentMap, mapSet = currentMapSet) {
  if (type === 'quarry') return [];
  const marked = (worldScenario?.places || []).filter(place => place.type === type)
    .map(place => ({ ...place, mapId: place.mapId || 'wilds' }))
    .filter(place => place.mapId === mapId);
  if (marked.length) return marked.map(place => type === 'mine' ? { ...place, rank: METAL_DEPOSIT_RANKS.some(rank => rank.id === place.rank) ? place.rank : 'II' } : place);
  // Esses pontos ja estao pintados no mapa dos Wilds; servem mesmo sem marcador do editor.
  const defaults = {
    quarry: { id: `map-${mapSet}-stone-quarry`, x: 22, y: 18 },
    // Ponto fixo original do Campo de Mineração em Wilds.
    mine: { id: `map-${mapSet}-metal-mine`, x: 72, y: 61, rank: mapSet === 2 ? 'IV' : 'II' },
  };
  if (mapId !== 'wilds' || !defaults[type]) return [];
  const places = [{ ...defaults[type], type, mapId: 'wilds' }];
  if (type === 'mine' && mapSet === 1) places.push({ ...places[0], x: 37, y: 100 });
  return places;
}

// Áreas desenhadas na arte de Wilds (mapa 1) onde nada pode ser construído: a pedreira, a mina de ouro e o riacho.
// x em % da largura, y de 0 a 200, medidos sobre mordavia-wilds-map.webp.
const WILDS_PAINTED = {
  1: {
    areas: [
      { name: 'a pedreira desenhada no mapa', x1: 4, x2: 48, y1: 8, y2: 46 },
      { name: 'a mina de ouro desenhada no mapa', x1: 64, x2: 90, y1: 62, y2: 95 },
    ],
    river: [[4, 70], [13, 68], [24, 65], [34, 61], [42, 59], [53, 59], [62, 56], [75, 54], [83, 52], [89, 51], [98, 49]],
  },
};
function paintedProblem(x, y, mapSet = currentMapSet) {
  const painted = WILDS_PAINTED[mapSet];
  if (!painted) return null;
  const area = painted.areas.find(item => x >= item.x1 && x <= item.x2 && y >= item.y1 && y <= item.y2);
  if (area) return `Não dá para construir em cima d${area.name.slice(0, 1) === 'a' ? '' : 'e '}${area.name}.`;
  const near = painted.river.slice(1).some(([bx, by], i) => {
    const [ax, ay] = painted.river[i];
    const dx = bx - ax, dy = (by - ay) * Y_TO_X_SCALE, px = x - ax, py = (y - ay) * Y_TO_X_SCALE;
    const t = Math.max(0, Math.min(1, (px * dx + py * dy) / (dx * dx + dy * dy)));
    return Math.hypot(px - t * dx, py - t * dy) < 5;
  });
  return near ? 'Não dá para construir em cima do riacho.' : null;
}
function protectedWildsDeposits(mapSet = currentMapSet) {
  const quarry = (worldScenario?.places || []).find(place => place.type === 'quarry' && (place.mapId || 'wilds') === 'wilds')
    || { id: `map-${mapSet}-stone-quarry`, x: 22, y: 18 };
  const gold = extractionPlaces('mine', 'wilds', mapSet)[0] || { id: `map-${mapSet}-metal-mine`, x: 72, y: 61 };
  return [
    { building: 'quarry', place: quarry, name: 'pedra' },
    { building: 'mine', place: gold, name: 'ouro' },
  ];
}

// Faixas de água visíveis nos mapas Wilds. Coordenadas em unidades do mapa (x%, y 0–200).
// O corredor impede qualquer prédio de cobrir riachos, piscinas e cachoeiras.
const WILDS_WATERWAYS = {
  1: [
    [[0, 75], [17, 68], [29, 64], [40, 59], [52, 53], [64, 50], [75, 54], [88, 60], [100, 57]],
    [[0, 91], [9, 87], [18, 84]],
  ],
  2: [
    [[0, 39], [12, 38], [25, 42], [37, 49], [51, 58], [68, 67], [83, 71], [100, 65]],
    [[52, 13], [48, 22], [43, 30], [39, 38]],
  ],
};

function nearWildsWater(x, y, mapSet = currentMapSet) {
  const py = y * Y_TO_X_SCALE;
  return (WILDS_WATERWAYS[mapSet] || []).some(path => path.some((point, index) => {
    if (index === path.length - 1) return false;
    const [ax, ayRaw] = point, [bx, byRaw] = path[index + 1];
    const ay = ayRaw * Y_TO_X_SCALE, by = byRaw * Y_TO_X_SCALE;
    const dx = bx - ax, dy = by - ay;
    const lengthSq = dx * dx + dy * dy;
    const t = lengthSq ? Math.max(0, Math.min(1, ((x - ax) * dx + (py - ay) * dy) / lengthSq)) : 0;
    return Math.hypot(x - (ax + t * dx), py - (ay + t * dy)) <= 2.7;
  }));
}

function renderFishingSpots() {
  const layer = $('#fishing-layer');
  if (!layer) return;
  FISHING_SPOTS.forEach((spot, index) => {
    let marker = $(`.fishing-spot[data-spot="${spot.site}"]`, layer);
    if (!marker) {
      marker = document.createElement('button');
      marker.type = 'button';
      marker.className = 'fishing-spot';
      marker.dataset.spot = spot.site;
      marker.innerHTML = '<span class="fishing-jump" aria-hidden="true"></span><i aria-hidden="true"></i><small>Pesca · atribuir camponês</small>';
      marker.addEventListener('click', () => selectFishingSpot(spot.site));
      layer.append(marker);
    }
    marker.style.left = `${spot.x}%`;
    marker.style.top = `${spot.y / WORLD_Y_UNITS * 100}%`;
    marker.style.setProperty('--jump-delay', `${(index % 2) * -1.35}s`);
    marker.hidden = currentMap !== spot.mapId || currentMapSet !== spot.mapSet;
    marker.classList.toggle('selected', selectedFishingSpotId === spot.site);
    const assigned = state.workers.filter(worker => worker.alive && worker.profession === 'fisher' && worker.jobSite === spot.site).length;
    marker.setAttribute('aria-label', `${spot.name}. ${format(fishingStock(spot))} de peixe disponível. ${assigned} camponeses atribuídos. Clique para escolher um camponês.`);
    marker.title = `${spot.name} · ${assigned} camponeses · clique para atribuir`;
  });
}

// A prévia e a construção usam o ponto escolhido pelo jogador.
function snapPlacement(type, x, y) {
  return { x, y };
}

function selectFishingSpot(site) {
  if (!FISHING_SPOTS.some(spot => spot.site === site)) return;
  selectedFishingSpotId = site;
  state.selectedSite = null;
  selectedLairId = null;
  state.selectedBuild = null;
  renderLairs();
  renderSites();
  renderFishingSpots();
  renderPanel();
}

function fishingSpotInfoHTML(spot) {
  const assigned = state.workers.filter(worker => worker.alive && worker.profession === 'fisher' && worker.jobSite === spot.site);
  const workers = state.workers.filter(worker => worker.alive).map(worker => {
    const alreadyHere = assigned.includes(worker);
    const awayOnOtherMap = worker.mapId === 'wilds' && (worker.mapSet || 1) !== spot.mapSet;
    const unavailable = worker.buildSite != null || !!worker.carrying || awayOnOtherMap;
    const detail = worker.buildSite != null ? 'terminando uma obra' : worker.carrying ? 'terminando uma entrega' : awayOnOtherMap ? 'está em outra área das Wilds' : peasantActivity(worker);
    const action = alreadyHere ? 'Atribuído aqui' : worker.profession === 'fisher' ? 'Transferir para cá' : 'Atribuir a este ponto';
    return `<button type="button" class="pit-worker" data-fishing-assign="${worker.id}" data-fishing-spot="${spot.site}" ${alreadyHere || unavailable ? 'disabled' : ''}><b>${worker.name}</b><small>${detail} · ${action}</small></button>`;
  }).join('');
  return `<section class="inspector" aria-label="${spot.name}">
    <button type="button" class="inspector-back" data-inspector-close aria-label="Fechar ponto de pesca" title="Fechar">×</button>
    <header class="inspector-id"><span class="fishing-inspector-mark" aria-hidden="true">≈</span><div><h3>${spot.name}</h3><small>Ponto fixo de pesca · Wilds</small></div></header>
    <dl class="inspector-stats"><div><dt>Peixe no ponto</dt><dd>${format(fishingStock(spot))} Trutas</dd></div><div><dt>Produção base</dt><dd>${hn(FISHING_RATE)} Truta/s por pescador</dd></div><div><dt>Pescadores aqui</dt><dd>${assigned.length}</dd></div></dl>
    <section class="inspector-section"><h4>Atribuir camponês</h4><p class="pit-hint">Clique em um nome. O camponês irá até este ponto, pescará e levará a carga ao depósito.</p><div class="pit-crew">${workers || '<div class="empty-state">Nenhum camponês disponível.</div>'}</div></section>
    ${assigned.length ? `<section class="inspector-section"><h4>Retirar do ponto</h4><div class="pit-crew">${assigned.map(worker => `<button type="button" class="pit-worker" data-fishing-remove="${worker.id}" ${worker.carrying?.food === 'fish' ? 'disabled title="Espere a entrega das Trutas"' : ''}><b>${worker.name}</b><small>${worker.carrying?.food === 'fish' ? 'Terminando entrega das Trutas' : 'Remover deste ponto'}</small></button>`).join('')}</div></section>` : ''}
  </section>`;
}

function placementProblem(x, y, ignoreSite = null) {
  const moving = state.movingSite != null ? state.buildings.find(building => building.site === state.movingSite) : null;
  const type = moving?.type || state.selectedBuild;
  if (type === 'mine' && currentMap !== 'wilds') return 'Minas s\u00f3 podem ser constru\u00eddas em Wilds.';
  if (type === 'mine' && state.buildings.some(building => building.type === 'mine' && (building.mapId || 'city') === 'wilds' && (building.mapSet || 1) === currentMapSet && building.site !== ignoreSite)) return 'Este ponto das Wilds permite uma única Mina.';
  if (type === 'quarry' && currentMap === 'city' && state.buildings.some(building => building.type === 'quarry' && (building.mapId || 'city') === 'city' && building.site !== ignoreSite)) return 'City permite apenas uma pedreira.';
  if (x < 6 || x > 94 || y < 4 || y > 196) return 'Fora do mapa.';
  if (currentMap === 'wilds' && nearWildsWater(x, y, currentMapSet)) return 'Não é possível construir sobre a água do riacho.';
  if (currentMap === 'wilds' && paintedProblem(x, y)) return paintedProblem(x, y);
  if (currentMap === 'wilds' && !WILDS_PAINTED[currentMapSet]) {
    const blockedDeposit = protectedWildsDeposits(currentMapSet).find(({ building, place }) => building !== type
      && Math.hypot(place.x - x, (place.y - y) * Y_TO_X_SCALE) <= 5.5);
    if (blockedDeposit) return `Depósito de ${blockedDeposit.name} reservado para ${blockedDeposit.building === 'quarry' ? 'a Pedreira' : 'o Campo de Mineração'}.`;
  }
  if (type === 'mine') {
    const deposits = extractionPlaces(type);
    if (!deposits.length) return 'Este mapa n\u00e3o tem um dep\u00f3sito de metal marcado.';
    const deposit = deposits.find(place => Math.hypot(place.x - x, (place.y - y) * Y_TO_X_SCALE) <= 5.5);
    if (!deposit) return 'Construa junto ao dep\u00f3sito de metais marcado no mapa.';
  }
  const crowded = state.buildings.some(building => building.site !== ignoreSite && (building.mapId || 'city') === currentMap && (building.mapSet || 1) === currentMapSet && Math.hypot(building.x - x, (building.y - y) * Y_TO_X_SCALE) < BUILD_SPACING);
  if (crowded) return 'Encostado demais em outra construção.';
  return null;
}

function migrateLegacyExtractionSites() {
  const used = new Set(state.buildings.filter(building => building.resourcePlaceId).map(building => `${building.mapSet || 1}:${building.resourcePlaceId}`));
  state.buildings.filter(building => building.type === 'mine' && (building.mapId || 'city') === 'city').forEach(building => {
    const deposits = extractionPlaces(building.type);
    const deposit = deposits.find(place => !used.has(`${building.mapSet || 1}:${place.id}`)) || deposits[0];
    building.mapId = 'wilds';
    if (deposit) {
      building.x = deposit.x;
      building.y = deposit.y;
      building.resourcePlaceId = deposit.id;
      if (building.type === 'mine') building.depositRank = deposit.rank || 'II';
      used.add(`${building.mapSet || 1}:${deposit.id}`);
    }
  });
}


function worldPoint(event) {
  const rect = $('#map-world').getBoundingClientRect();
  return { x: (event.clientX - rect.left) / rect.width * 100, y: (event.clientY - rect.top) / rect.height * WORLD_Y_UNITS };
}

function showPlacementPreview(event) {
  const preview = $('#placement-preview');
  const item = BUILDINGS.find(building => building.id === state.selectedBuild);
  const moving = state.movingSite != null ? state.buildings.find(building => building.site === state.movingSite) : null;
  if ((!item && !moving) || event.target.closest('.map-tools, .menu-btn')) { preview.hidden = true; return; }
  const { x, y } = snapPlacement(moving?.type || item?.id, ...Object.values(worldPoint(event)));
  const art = moving ? siteArt(moving) : item.id === 'farm' ? farmType(state.selectedFarmType).art : BUILDING_ART[item.id];
  preview.hidden = false;
  preview.style.setProperty('--x', `${x}%`);
  preview.style.setProperty('--y', `${y / WORLD_Y_UNITS * 100}%`);
  preview.style.zIndex = String(Math.round(y * 10) + 1);
  preview.dataset.art = art;
  applyBuildingImage(preview, art, null, moving);
  preview.classList.toggle('invalid', !!placementProblem(x, y, moving ? moving.site : null));
}

function workerIsWorkingIndoors(actor) {
  if (actor.className !== 'Peasant' || actor.action !== 'work' || !(actor.actionTime > 0) || actor.carrying || actor.buildSite != null) return false;
  const workplace = actor.workplace || state.buildings.find(building => building.site === actor.jobSite);
  return !!workplace
    && ['mill', 'bakery', 'butcher', 'emberLab', 'blacksmith'].includes(workplace.type)
    && (actor.mapId || 'city') === (workplace.mapId || 'city')
    && (actor.mapSet || 1) === (workplace.mapSet || 1);
}

const NEKOMATA_FRAME_MS = { base: 130.787, atk: 142.361, move: 55.556, 'move-left': 55.556, cast: 138.889, special: 84.491, hit: 52.083, death: 92.593 };
const NEKOMATA_BOX = { base: [4.7, 2.32857, 0.05714], atk: [4.95, 2.90714, 0.09286], move: [4.71429, 2.17143, 0.07143], 'move-left': [4.71429, 2.17143, 0.07143], cast: [4.91429, 2.7, 0.22857], special: [4.9, 3, 0.3], hit: [4.65714, 2.97143, 0.12857], death: [5.05714, 1.82857, 0.17143] };
let nekomataDeaths = [];
function nekomataFrame(actor) {
  const clip = actor.nekoDeathClock != null ? 'death' : actor.nekoHitClock != null ? 'hit' : actor.attackAnim > 0 ? (actor.nekoAttackClip || 'atk') : actor.nekoMoving ? (actor.side === 'left' ? 'move-left' : 'move') : 'base';
  const clock = clip === 'death' ? actor.nekoDeathClock : clip === 'hit' ? actor.nekoHitClock : ['atk', 'cast', 'special'].includes(clip) ? actor.actionClock || 0 : clip === 'base' ? actor.nekoIdleClock || 0 : actor.walkClock || 0;
  let frame = Math.floor(clock * 1000 / NEKOMATA_FRAME_MS[clip]);
  if (clip === 'base') { frame %= 70; frame = frame < 36 ? frame : 70 - frame; }
  else if (clip === 'move' || clip === 'move-left') frame %= 36;
  else frame = Math.min(35, frame);
  return { clip, frame: frame + 1 };
}
function renderHeroes() {
  const layer = $('#hero-layer');
  const active = new Set();
  [...state.heroes, ...state.workers, ...(state.bandits || []), ...(window.Monarch?.summons() || []), ...nekomataDeaths].filter(actor => (actor.alive || actor.nekoDeathClock != null) && !workerIsWorkingIndoors(actor) && (actor.mapId || 'city') === currentMap && (actor.mapSet || 1) === currentMapSet).forEach(actor => {
    active.add(String(actor.id));
    let token = [...layer.children].find(node => node.dataset.actor === String(actor.id));
    if (!token) {
      token = document.createElement('button');
      token.type = 'button';
      token.dataset.actor = String(actor.id);
      token.setAttribute('aria-label', `${actor.name}, ${actor.className}`);
      const art = actorArt(actor);
      token.innerHTML = art
        ? `<span class="character-sprite" style="background-image:url('public/art/${art}-walk-v01.${art.startsWith('bandit') || art.startsWith('skeleton-v02') || art.startsWith('zombie-v01') || art.startsWith('mordavian-wolf-v01') || art.startsWith('mordavian-goblin-v01') || art.startsWith('mordavian-troll-v01') ? 'png' : 'webp'}')" aria-hidden="true"></span><span class="character-name">${actor.name}</span>`
        : actor.className === 'Monster' ? `<span class="monster-skull" aria-hidden="true">☠</span><span class="character-name">${actor.name}</span>`
        : `<span class="hero-face">${actor.className[0]}</span><span class="character-name">${actor.name}</span>`;
      token.addEventListener('click', () => {
        if (!actor.alive) return;
        if (state.selectedBuild) return;
        // Clique esquerdo num inimigo põe recompensa pela cabeça dele (valor escolhido na aba Heróis).
        if (isHostile(actor)) {
          if ((state.bounties || []).some(bounty => bounty.target === actor.id)) { say(`${actor.name} já tem recompensa. Agora é esperar alguém com coragem.`); return; }
          huntBounty(actor.id);
          return;
        }
        state.selectedActor = actor.id;
        renderHeroes();
        // Clicar no boneco abre a ficha dele: nível, experiência, atributos e origem.
        menu.stack = [];
        showScreen('actor', { id: actor.id });
      });
      token.insertAdjacentHTML('beforeend', '<span class="hp-bar" aria-hidden="true"><i></i></span>');
      layer.append(token);
    }
    if (token.parentElement !== layer) layer.append(token);
    const hurt = actor.hp != null && actor.hp < maxHpOf(actor);
    token.className = `character${actor.className === 'Peasant' ? ' worker' : ''}${actor.className === 'Bandit' ? ' bandit' : ''}${actor.className === 'Monster' ? ' bandit monster' : ''}${actor.className === 'Wildlife' ? ' wildlife' : ''}${actor.kind === 'boar' ? ' wild-boar' : actor.kind === 'elk' ? ' elk' : actor.kind === 'wolf' ? ' wolf' : actor.kind === 'nekomata' ? ' nekomata' : actor.kind === 'goblin' ? ' goblin' : actor.kind === 'troll' ? ' troll' : actor.kind === 'zombie' ? ' zombie' : ''}${actor.summon ? ' summon' : ''}${actor.mfx?.convert ? ' converted' : ''}${hurt ? ' hurt' : ''}${state.selectedActor === actor.id ? ' selected' : ''}`;
    if (hurt) token.style.setProperty('--hp', `${Math.max(0, Math.round(hpOf(actor) / maxHpOf(actor) * 100))}%`);
    token.style.left = `${actor.x}%`;
    token.style.top = `${actor.y / WORLD_Y_UNITS * 100}%`;
    token.style.zIndex = String(Math.round(actor.y * 10));
      token.dataset.facing = actor.action === 'fight' ? 'side' : actor.facing || 'front';
    token.dataset.side = actor.side || 'right';
    if (actorArt(actor)) {
      const art = actorArt(actor);
      const sheet = actorSheet(actor);

      // A caminhada de costas tem uma tira própria: a cesta permanece na mesma mão
      // e os quatro quadros mantêm a passada alternada sem espelhar metade do ciclo.
      const backWalk = art === 'peasant' && sheet === 'walk' && token.dataset.facing === 'back';
      const verticalTrollWalk = actor.kind === 'troll' && sheet === 'walk' && ['up', 'down'].includes(actor.walkDirection);
      const extension = actor.className === 'Wildlife' || (art === 'peasant' && ['build', 'work', 'fight'].includes(sheet)) || (art === 'female-peasant' && ['work', 'fight'].includes(sheet)) || art.startsWith('bandit') || art.startsWith('skeleton-v02') || art.startsWith('zombie-v01') || art.startsWith('mordavian-wolf-v01') || art.startsWith('mordavian-goblin-v01') || art.startsWith('mordavian-troll-v01') ? 'png' : 'webp';
      const nekoFrame = actor.kind === 'nekomata' ? nekomataFrame(actor) : null;
      if (nekoFrame) {
        const [width, height, foot] = NEKOMATA_BOX[nekoFrame.clip];
        token.style.setProperty('--neko-width', `${width / 4.7 * 100}%`);
        token.style.setProperty('--neko-height', `${height / 2.32857 * 100}%`);
        token.style.setProperty('--neko-bottom', `${(0.05714 - foot) / 2.32857 * 100}%`);
      }
      const nativeLeft = /^moonwarden-(human|werewolf)$/.test(art) && token.dataset.side === 'left' && ['walk', 'action'].includes(sheet);
      const guildSprite = guildHeroArt(actor) ? `${art}-${sheet === 'action' || sheet === 'idle' ? sheet : 'walk'}${nativeLeft ? '-left' : ''}-v${sheet === 'idle' ? (art.startsWith('moonwarden-') ? '02' : '03') : '01'}.webp` : null;
      const spriteFile = guildSprite || (nekoFrame ? `nekomata-media36-v01/${nekoFrame.clip}-media36-v01/${nekoFrame.clip}-${nekoFrame.frame}.png` : backWalk ? 'peasant-walk-back-v02.png' : verticalTrollWalk ? 'mordavian-troll-v01-walk-vertical-v01.png' : art === 'female-peasant' && sheet === 'work' ? 'female-peasant-work-v02.png' : art === 'peasant' && sheet === 'fight' ? 'peasant-fight-side-v01.png' : art === 'female-peasant' && sheet === 'fight' ? 'female-peasant-fight-side-v01.png' : art === 'skeleton-v02' && sheet === 'action' ? 'skeleton-v02-action-v02.png' : actor.className === 'Wildlife' && sheet === 'idle' ? `${art}-idle-v02.png` : actor.className === 'Wildlife' && art === 'elk' && sheet === 'walk' ? 'elk-walk-v02.png' : `${art}-${sheet}-v01.${extension}`);
      if (token.dataset.spriteFile !== spriteFile) {
        $('.character-sprite', token).style.backgroundImage = `url('public/art/${spriteFile}')`;
        token.dataset.spriteFile = spriteFile;
        token.dataset.sheet = sheet;
        token.dataset.art = art;
      }
      token.dataset.backWalk = backWalk ? 'true' : 'false';
      token.dataset.verticalWalk = verticalTrollWalk ? 'true' : 'false';
      const flipSprite = guildSprite ? !nativeLeft && token.dataset.side === 'left' : actor.kind === 'nekomata' ? (token.dataset.side === 'left' && nekoFrame.clip !== 'move-left') : !verticalTrollWalk && actor.action !== 'fight' && token.dataset.side === 'left'; token.style.setProperty('--sprite-scale-x', flipSprite ? -1 : 1);
    }
    token.style.setProperty('--frame-x', `${(actor.frame || 0) * 100 / 3}%`);
    // Recompensa pela cabeça: a placa acompanha a caveira.
    const wanted = actor.className === 'Monster' ? (state.bounties || []).find(bounty => bounty.target === actor.id) : null;
    let tag = token.querySelector('.wanted-tag');
    if (wanted) {
      if (!tag) { tag = document.createElement('span'); tag.className = 'wanted-tag'; token.append(tag); }
      if (tag.textContent !== String(wanted.reward)) tag.textContent = String(wanted.reward);
    } else if (tag) tag.remove();
  });
  [...layer.children].filter(node => !active.has(node.dataset.actor)).forEach(node => node.remove());
}

function heroCard(hero) {
  const nextInfo = guildHeroArt(hero) ? 'A forma acompanha o nível da guilda.' : hero.prestige ? `Caminho de prestígio: ${hero.prestige}` : hero.level >= 15 ? 'A guilda disponível mais próxima define a especialização.' : `${15 - hero.level} ${15 - hero.level === 1 ? 'nível' : 'níveis'} até o prestígio`;
  return `<article class="hero-card">
    <img class="hero-portrait" data-actor-sheet="${hero.id}" title="Ver ficha" src="public/art/portraits/${actorPortrait(hero)}-portrait-v01.webp" alt="Retrato de ${hero.name}">
    <div class="hero-details"><b>${hero.name}</b><small>${actorLabel(hero)}</small>${hero.trait ? `<small class="hero-trait"><b>${hero.trait}.</b> ${TRAITS[hero.trait] || ''}</small>` : ''}<small>${nextInfo}</small><small>Alcoolismo · penalidade ${Math.floor(alcoholismOf(hero))}/100</small><small>Bolsa: ${format(purseOf(hero))} de ouro</small></div>
    <div class="hero-xp">NÍV. ${hero.level}<i style="--xp:${hero.level >= HEIR_MAX_LEVEL ? 100 : Math.round((hero.xp || 0) / xpToNext(hero.level) * 100)}%"></i><button type="button" class="card-sheet-btn" data-actor-sheet="${hero.id}">FICHA</button></div>
  </article>`;
}

function workerCard(worker) {
  return `<article class="hero-card worker-card">
    <img class="hero-portrait" data-actor-sheet="${worker.id}" title="Ver ficha" src="public/art/portraits/${actorPortrait(worker)}-portrait-v01.webp" alt="Retrato de ${worker.name}">
    <div class="hero-details"><b>${worker.name}</b><small>${actorLabel(worker)}</small><small>${homeOf(worker) ? 'Mora no casebre.' : 'Mora no casarão, como as dívidas.'} ${workplaceOf(worker) ? `Trabalha ${workPlaceName(workplaceOf(worker))}.` : 'Sem fazenda pra trabalhar.'}</small><small class="worker-task">Agora: ${peasantActivity(worker)}.</small><label class="prof-select"><span>Ofício</span><select data-profession="${worker.id}"><option value="">Nenhum</option>${worker.profession === 'fisher' ? '<option value="fisher" selected disabled>Pescador · atribuído no mapa</option>' : ''}${Object.entries(PROFESSIONS).filter(([id]) => id !== 'fisher').map(([id, prof]) => `<option value="${id}"${worker.profession === id ? ' selected' : ''}>${isFemalePeasant(worker) ? prof.fem : prof.name}</option>`).join('')}</select></label><small>Bolsa: ${format(purseOf(worker))} de ouro</small></div>
    <div class="hero-xp">NÍV. ${worker.level || 1}<i style="--xp:${(worker.level || 1) >= HEIR_MAX_LEVEL ? 100 : Math.round((worker.xp || 0) / xpToNext(worker.level || 1) * 100)}%"></i><button type="button" class="card-sheet-btn" data-actor-sheet="${worker.id}">FICHA</button></div>
  </article>`;
}

// O inspetor dos covis usa o mesmo painel de informações das construções.
function lairInfoHTML(lair) {
  const den = CREATURE_DENS.includes(lair.kind);
  const level = lair.level || 1;
  const bounty = (state.bounties || []).find(item => item.lairId === lair.id);
  const creature = lair.kind === 'wolf-den' ? 'Lobo mordaviano'
    : lair.kind === 'goblin-den' ? 'Goblin'
    : lair.kind === 'troll-den' ? 'Troll'
    : lair.kind === 'nekomata-den' ? 'Nekomata'
    : lair.kind === 'cemetery' ? (level >= 2 ? 'Zumbi' : 'Caveira')
    : 'Criatura';
  const activity = den ? 'Solta uma criatura a cada 12 horas de jogo.'
    : lair.kind === 'cemetery' ? `${creature}s surgem às 22h, uma vez por noite.`
    : 'Local perigoso nos Wilds.';
  const stats = [
    ['Estado', bounty ? `Recompensa ativa · ${format(bounty.reward)} de ouro` : 'Ativo'],
    ['Ameaça', creature],
    ['Atividade', activity],
  ];
  const hp = den ? lairHp(lair) : 0;
  const maxHp = den ? lairMaxHp(lair) : 1;
  return `<section class="inspector" aria-label="${lair.name}">
    <button type="button" class="inspector-back" data-inspector-close aria-label="Fechar informações do covil" title="Fechar">×</button>
    <header class="inspector-id">
      <img class="inspector-thumb" src="public/art/${lair.art}" alt="">
      <div><h3>${lair.name}</h3><small>Nível ${level}${den ? ' · Covil de criaturas' : ' · Cemitério'}</small></div>
    </header>
    ${den ? `<div class="inspector-hp${hp < maxHp ? ' hurt' : ''}" title="Vida do covil"><span>Vida</span><i style="--hp:${Math.round(hp / maxHp * 100)}%"></i><b>${hp} / ${maxHp}</b></div>` : ''}
    <dl class="inspector-stats">${stats.map(([label, value]) => `<div><dt>${label}</dt><dd>${value}</dd></div>`).join('')}</dl>
    ${den && !bounty ? `<section class="inspector-section"><button type="button" class="hut-upgrade hunt-btn" data-bounty-lair="${lair.id}">Destruir · ${format(state.bountyAmount)} ouro</button></section>` : ''}
  </section>`;
}

// A ficha da construção abre num menu próprio em cima do mapa (mover, melhorar, demolir).
// A aba Construir fica sempre com a lista de construções.
function inspectorHTML() {
    const selectedLair = selectedLairId == null ? null : (state.lairs || []).find(lair => lair.id === selectedLairId && lair.active);
    if (selectedLair) return lairInfoHTML(selectedLair);
    if (selectedLairId != null) selectedLairId = null;
    if (selectedFishingSpotId != null) {
      const spot = FISHING_SPOTS.find(item => item.site === selectedFishingSpotId);
      if (spot && currentMap === spot.mapId && currentMapSet === spot.mapSet) return fishingSpotInfoHTML(spot);
      selectedFishingSpotId = null;
    }
    const selectedFarm = state.selectedSite === null ? null : state.buildings.find(building => building.site === state.selectedSite && building.type === 'farm' && isDone(building));
    const selectedBuilding = state.selectedSite === null ? null : state.buildings.find(building => building.site === state.selectedSite);
    const selectedArmory = state.selectedSite === null ? null : state.buildings.find(building => building.site === state.selectedSite && building.type === 'armory' && isDone(building));
    const selectedProducer = state.selectedSite === null ? null : state.buildings.find(building => building.site === state.selectedSite && PRODUCER_TYPES.includes(building.type) && isDone(building));
    const selectedHut = state.selectedSite === null ? null : state.buildings.find(building => building.site === state.selectedSite && building.type === 'hut' && isDone(building));
    const selectedLumber = state.selectedSite === null ? null : state.buildings.find(building => building.site === state.selectedSite && building.type === 'lumber' && isDone(building));
    const selectedPit = state.selectedSite === null ? null : state.buildings.find(building => building.site === state.selectedSite && isPit(building) && isDone(building));
    const selectedDepot = state.selectedSite === null ? null : state.buildings.find(building => building.site === state.selectedSite && building.type === 'depot' && isDone(building));
    const selectedGuild = state.selectedSite === null ? null : state.buildings.find(building => building.site === state.selectedSite && isDone(building) && BUILDINGS.find(item => item.id === building.type)?.heroClass);
    const currentFarmType = selectedFarm ? farmType(selectedFarm.produce).id : state.selectedFarmType;
    const inspectorActions = selectedBuilding ? `${selectedFarm ? `<section class="inspector-section" aria-label="Tipo de fazenda"><h4>Produção</h4>
        <div class="farm-type-grid compact">${[...FARM_TYPES].sort((a, b) => a.name.localeCompare(b.name, 'pt-BR')).map(type => {
          const crewMultiplier = 1 + crewWork(selectedFarm);
          const rate = type.grain[producerLevel(selectedFarm)] * FARM_OUTPUT_MULTIPLIER * crewMultiplier;
          const secondaryRate = type.secondary ? rate * FARM_ANIMAL_RATE : 0;
          return `<button class="farm-type${currentFarmType === type.id ? ' selected' : ''}" data-farm-type="${type.id}" aria-pressed="${currentFarmType === type.id}" title="${rateNumber(rate)} de ${foodName(type.foodId || type.id)}/s${type.secondary ? ` · ${rateNumber(secondaryRate)} de ${foodName(type.secondary)}/s` : ''}"><img src="public/art/${type.icon || `${type.art}-v01.webp`}" alt=""><b>${type.name}</b></button>`;
        }).join('')}</div></section>` : ''}
      ${selectedProducer?.type === 'bakery' ? bakeryRecipeBoxHTML(selectedProducer) : ''}
      ${selectedProducer?.type === 'butcher' ? butcherRecipeBoxHTML(selectedProducer) : ''}
      ${selectedProducer?.type === 'blacksmith' ? blacksmithRecipeBoxHTML(selectedProducer) : ''}
      ${selectedProducer ? producerBoxHTML(selectedProducer) : ''}
      ${selectedHut && !selectedHut.big ? `<section class="inspector-section"><h4>Ampliação</h4><button type="button" class="hut-upgrade" data-hut-upgrade="${selectedHut.site}">Casebre Grande · +3 moradores · ${costHTML(HUT_UPGRADE.gold, HUT_UPGRADE.wood)}</button></section>` : ''}
      ${selectedGuild ? guildBoxHTML(selectedGuild) : ''}
      ${selectedDepot ? depotBoxHTML(selectedDepot) : ''}
      ${selectedLumber ? lumberBoxHTML(selectedLumber) : ''}
      ${selectedPit ? pitBoxHTML(selectedPit) : ''}
      ${selectedArmory ? armoryBoxHTML(selectedArmory) : ''}` : '';
  return selectedBuilding ? buildingInfoHTML(selectedBuilding, inspectorActions) : '';
}

function renderInspector() {
  const box = $('#site-inspector');
  if (!box) return;
  const html = inspectorHTML();
  box.hidden = !html;
  box.innerHTML = html;
  if (html) bindPanel(box);
}

function bindPanel(host) {
  $$('[data-build-category]', host).forEach(section => section.addEventListener('toggle', () => {
    if (section.open) openBuildCategories.add(section.dataset.buildCategory);
    else openBuildCategories.delete(section.dataset.buildCategory);
  }));
  $$('.build-card', host).forEach(button => button.addEventListener('click', () => chooseBuild(button.dataset.build)));
  $$('[data-farm-type]', host).forEach(button => button.addEventListener('click', () => chooseFarmType(button.dataset.farmType)));
  $$('[data-bakery-recipe]', host).forEach(button => button.addEventListener('click', () => chooseBakeryRecipe(button.dataset.bakeryRecipe)));
  $$('[data-butcher-recipe]', host).forEach(button => button.addEventListener('click', () => chooseButcherRecipe(button.dataset.butcherRecipe)));
  $$('[data-blacksmith-recipe]', host).forEach(button => button.addEventListener('click', () => chooseBlacksmithRecipe(button.dataset.blacksmithRecipe)));
  $$('[data-weapon-pattern]', host).forEach(select => select.addEventListener('change', () => { const smith = state.buildings.find(item => item.site === Number(select.dataset.weaponPattern)); if (!smith) return; smith.weaponPattern = select.value; smith.weaponMaterial = Object.keys(weaponPattern(select.value).designs)[0]; renderPanel(); }));
  $$('[data-weapon-material]', host).forEach(select => select.addEventListener('change', () => { const smith = state.buildings.find(item => item.site === Number(select.dataset.weaponMaterial)); if (smith) { smith.weaponMaterial = select.value; renderPanel(); } }));
  $$('[data-place-weapon-order]', host).forEach(button => button.addEventListener('click', () => placeWeaponOrder(Number(button.dataset.placeWeaponOrder))));
  $$('[data-cancel-weapon-order]', host).forEach(button => button.addEventListener('click', () => cancelWeaponOrder(Number(button.dataset.cancelWeaponOrder))));
  $$('[data-weapon-repair]', host).forEach(button => button.addEventListener('click', () => orderWeaponRepair(Number(button.dataset.smith), button.dataset.weaponRepair, button.dataset.part)));
  $$('[data-guild-upgrade]', host).forEach(button => button.addEventListener('click', () => upgradeGuild(Number(button.dataset.guildUpgrade), button.dataset.branch || null)));
  $$('[data-ration]', host).forEach(button => button.addEventListener('click', () => { state.ration = Number(button.dataset.ration); say(`Ração ${RATIONS[state.ration].label.toLowerCase()}.`); renderPanel(); updateHud(); }));
  $$('[data-diet]', host).forEach(button => button.addEventListener('click', () => { state.diet[button.dataset.diet] = !state.diet[button.dataset.diet]; renderPanel(); }));
  $$('[data-trade]', host).forEach(button => button.addEventListener('click', () => trade(button.dataset.trade, Number(button.dataset.amount))));
  $$('[data-pit]', host).forEach(button => button.addEventListener('click', () => togglePitWorker(Number(button.dataset.pit), button.dataset.worker)));
  $$('[data-seam-pit]', host).forEach(select => select.addEventListener('change', () => {
    const pit = state.buildings.find(building => building.site === Number(select.dataset.seamPit) && building.type === 'mine');
    const seam = pitSeams(pit).find(item => item.id === select.value);
    if (!pit || !seam || (seam.access === 'deep' && (pit.level || 0) < 1)) return;
    pit.selectedSeamId = seam.id; pit.workStatus = seam.remainingGrams > 0 ? 'Veio selecionado' : 'Veio esgotado';
    renderPanel(); renderSites();
  }));
  $$('[data-quarry-face]', host).forEach(select => select.addEventListener('change', () => {
    const pit = state.buildings.find(building => building.site === Number(select.dataset.quarryFace) && building.type === 'quarry');
    const face = stoneDepositForPit(pit)?.faces.find(item => item.id === select.value);
    if (!pit || !face || face.benchLevel > (pit.level || 0)) return;
    pit.quarryFaceId = face.id; renderPanel(); renderSites();
  }));
  $$('[data-quarry-order]', host).forEach(select => select.addEventListener('change', () => {
    const pit = state.buildings.find(building => building.site === Number(select.dataset.quarryOrder) && building.type === 'quarry');
    const order = QUARRY_ORDERS.find(item => item.id === select.value);
    if (!pit || !order || order.level > (pit.level || 0)) return;
    if (pit.quarryWork && pit.quarryWork.order !== order.id) {
      if (!pit.quarryWork.separated && pit.quarryWork.blankId) {
        const deposit = stoneDepositForPit(pit), blank = deposit?.faces.flatMap(face => face.blanks).find(piece => piece.id === pit.quarryWork.blankId);
        if (blank?.reservedBy === `quarry:${pit.site}`) delete blank.reservedBy;
      }
      pit.quarryWork = null;
    }
    pit.quarryOrder = order.id; renderPanel(); renderSites();
  }));
  $$('[data-pit-upgrade]', host).forEach(button => button.addEventListener('click', () => upgradePit(Number(button.dataset.pitUpgrade))));
  $$('[data-lumber]', host).forEach(button => button.addEventListener('click', () => setLumberCrew(Number(button.dataset.lumber), Number(button.dataset.change))));
  $$('[data-fishing-assign]', host).forEach(button => button.addEventListener('click', () => assignFisherToSpot(button.dataset.fishingAssign, button.dataset.fishingSpot)));
  $$('[data-fishing-remove]', host).forEach(button => button.addEventListener('click', () => removeFisherFromSpot(button.dataset.fishingRemove)));
  $$('[data-inspector-close]', host).forEach(button => button.addEventListener('click', () => { if (state.movingSite != null) cancelMove(); state.selectedSite = null; selectedLairId = null; selectedFishingSpotId = null; renderLairs(); renderSites(); renderFishingSpots(); renderPanel(); }));
  $$('[data-move-site]', host).forEach(button => button.addEventListener('click', () => (state.movingSite === Number(button.dataset.moveSite) ? cancelMove() : startMove(Number(button.dataset.moveSite)))));
  $$('[data-open-market]', host).forEach(button => button.addEventListener('click', () => { state.tab = 'market'; $$('.tab').forEach(tab => { const active = tab.dataset.tab === state.tab; tab.classList.toggle('active', active); tab.setAttribute('aria-selected', String(active)); }); renderPanel(); }));
  $$('[data-bounty-amount]', host).forEach(button => button.addEventListener('click', () => { state.bountyAmount = Number(button.dataset.bountyAmount); renderPanel(); }));
  $$('[data-bounty-input]', host).forEach(input => input.addEventListener('change', () => { state.bountyAmount = Math.max(5, Math.round(Number(input.value) || 25)); renderPanel(); }));
  $$('[data-bounty-place]', host).forEach(button => button.addEventListener('click', () => (state.placingBounty ? cancelBountyPlacement() : startBountyPlacement())));
  $$('[data-bounty-preset]', host).forEach(button => button.addEventListener('click', () => createBounty(BOUNTY_PRESETS[button.dataset.bountyPreset])));
  $$('[data-bounty-hunt]', host).forEach(button => button.addEventListener('click', () => huntBounty(button.dataset.bountyHunt)));
  $$('[data-bounty-lair]', host).forEach(button => button.addEventListener('click', () => { lairBounty(button.dataset.bountyLair); renderPanel(); }));
  $$('[data-bounty-cancel]', host).forEach(button => button.addEventListener('click', () => cancelBounty(button.dataset.bountyCancel)));
  $$('[data-deliver]', host).forEach(button => button.addEventListener('click', () => deliverDemand(button.dataset.deliver)));
  $$('[data-monarch-buy]', host).forEach(button => button.addEventListener('click', () => {
    const select = $(`[data-monarch-pick="${button.dataset.monarchBuy}"]`, host);
    if (select) buyMonarchItem(button.dataset.monarchBuy, select.value);
  }));
  $$('[data-armory-upgrade]', host).forEach(button => button.addEventListener('click', () => upgradeArmory(Number(button.dataset.armoryUpgrade))));
  $$('[data-producer-upgrade]', host).forEach(button => button.addEventListener('click', () => upgradeProducer(Number(button.dataset.producerUpgrade))));
  $$('[data-depot-upgrade]', host).forEach(button => button.addEventListener('click', () => upgradeDepot(Number(button.dataset.depotUpgrade))));
  $$('[data-hut-upgrade]', host).forEach(button => button.addEventListener('click', () => upgradeHut(Number(button.dataset.hutUpgrade))));
  $$('[data-profession]', host).forEach(select => select.addEventListener('change', () => setProfession(select.dataset.profession, select.value)));
  $$('[data-actor-sheet]', host).forEach(button => button.addEventListener('click', () => { menu.stack = []; showScreen('actor', { id: button.dataset.actorSheet }); }));
  $$('[data-demolish]', host).forEach(button => button.addEventListener('click', () => demolishBuilding(Number(button.dataset.demolish))));
  $$('[data-cancel-construction]', host).forEach(button => button.addEventListener('click', () => cancelConstruction(Number(button.dataset.cancelConstruction))));
}

// A aba Construir mostra as construções agrupadas por tipo, uma seção embaixo da outra.
const BUILD_CATEGORIES = [
  { id: 'houses', name: 'Casas', types: ['hut', 'depot'] },
  { id: 'farms', name: 'Fazendas', types: ['farm'] },
  { id: 'production', name: 'Produção', types: ['mill', 'bakery', 'emberLab', 'butcher', 'lumber', 'quarry', 'mine', 'blacksmith', 'market'] },
  { id: 'guilds', name: 'Guildas', types: ['warrior', 'archer', 'lancer', 'conjurer', 'healer', 'moonwarden', 'vanguard', 'armory', 'tavern', 'hotel'] },
];
const buildCardHTML = item => {
  const cityQuarryExists = item.id === 'quarry' && currentMap === 'city' && state.buildings.some(building => building.type === 'quarry' && (building.mapId || 'city') === 'city');
  const blocked = (item.id === 'mine' && currentMap !== 'wilds') || cityQuarryExists;
  const reason = cityQuarryExists ? 'A única pedreira de City já foi construída.' : blocked ? item.id === 'mine' ? 'Disponível apenas nas Wilds.' : 'Disponível apenas na City.' : item.id === 'quarry' && currentMap === 'city' ? 'Pedreira inicial de baixa reserva · City permite uma; Wilds não tem limite.' : item.detail;
  const art = item.id === 'farm' ? farmType(state.selectedFarmType).art : BUILDING_ART[item.id];
  const preview = item.id === 'quarry' || item.id === 'mine'
    ? `<span class="building-sprite-sheet" style="${buildingImageStyle(art)}" aria-hidden="true"></span>`
    : `<img src="${buildingImage(art)}" alt="">`;
  return `<button class="build-card${state.selectedBuild === item.id ? ' selected' : ''}" data-build="${item.id}" aria-label="Construir ${item.id === 'farm' ? `Fazenda de ${farmType(state.selectedFarmType).name}` : item.name}, custa ${item.cost} moedas, ${item.wood} madeira e ${buildingStoneCost(item)} pedra de construção" title="${reason}" ${blocked ? 'disabled aria-disabled="true"' : ''}>
        <span class="build-preview">${preview}${item.id === 'mill' ? '<span class="mill-sails"></span>' : ''}</span><span class="build-copy"><b>${item.name}${item.id === 'farm' ? ` · ${farmType(state.selectedFarmType).name}` : ''}</b><small>${reason}</small></span><span class="build-cost">${costHTML(item.cost, item.wood, buildingStoneCost(item))}</span></button>`;
};
const buildCategoryOf = id => BUILD_CATEGORIES.find(category => category.types.includes(id))?.id || (BUILDINGS.find(item => item.id === id)?.heroClass ? 'guilds' : 'houses');

let panelTabShown = null;
const panelScroll = {};
const openBuildCategories = new Set();
function renderPanel() {
  const host = $('#panel-view');
  // A lista nunca volta pro começo sozinha: cada aba lembra onde estava.
  if (panelTabShown) panelScroll[panelTabShown] = host.scrollTop;
  const keepScroll = panelScroll[state.tab] || 0;
  if (state.tab === 'build') {
    const selectedFarm = null;
    const farmChoice = state.selectedBuild === 'farm';
    const currentFarmType = state.selectedFarmType;
    const farmSelector = farmChoice ? `<section class="farm-selector" aria-label="Tipo de fazenda"><div class="farm-selector-heading"><b>${selectedFarm ? 'Produção desta fazenda' : 'Escolha a produção da fazenda'}</b><small>${selectedFarm ? 'Toque para trocar a produção' : 'Depois toque no mapa'}</small></div>
        <div class="farm-type-grid">${[...FARM_TYPES].sort((a, b) => a.name.localeCompare(b.name, 'pt-BR')).map(type => {
          const level = selectedFarm ? producerLevel(selectedFarm) : 0;
          const multiplier = FARM_OUTPUT_MULTIPLIER * (selectedFarm ? 1 + crewWork(selectedFarm) : 1);
          const rate = type.grain[level] * multiplier;
          const secondaryRate = type.secondary ? rate * FARM_ANIMAL_RATE : 0;
          return `<button class="farm-type${currentFarmType === type.id ? ' selected' : ''}" data-farm-type="${type.id}" aria-pressed="${currentFarmType === type.id}"><img src="public/art/${type.icon || `${type.art}-v01.webp`}" alt=""><span><b>${type.name}</b><small>+${rateNumber(rate)} ${foodName(type.foodId || type.id)}/s</small>${type.secondary ? `<small>+${rateNumber(secondaryRate)} ${foodName(type.secondary)}/s</small>` : ''}</span></button>`;
        }).join('')}</div></section>` : '';
    host.innerHTML = `<div class="panel-title"><h3>Erga o domínio</h3><span>Escolha uma carta e toque no mapa</span></div>
      ${[...BUILD_CATEGORIES].sort((a, b) => a.name.localeCompare(b.name, 'pt-BR')).map(category => `<details class="build-section" data-build-category="${category.id}" ${openBuildCategories.has(category.id) ? 'open' : ''}><summary class="build-section-title">${category.name}</summary><div class="build-grid">${BUILDINGS.filter(item => buildCategoryOf(item.id) === category.id).sort((a, b) => a.name.localeCompare(b.name, 'pt-BR')).map(buildCardHTML).join('')}</div>${category.id === 'farms' ? farmSelector : ''}</details>`).join('')}
      <p class="build-note">Construções comuns usam ouro, madeira e Pedra de Construção. A pedreira inicial não exige pedra.</p>`;
  } else if (state.tab === 'heroes') {
    host.innerHTML = `<div class="panel-title"><h3>Companhia autônoma</h3><span>${state.heroes.filter(h => h.alive).length}/${heroCapacity()} hóspedes</span></div>
      ${bountyPanelHTML()}
      <div class="hero-list">${state.heroes.filter(h => h.alive).sort((a, b) => a.name.localeCompare(b.name, 'pt-BR')).map(heroCard).join('') || '<div class="empty-state">Nenhum herói. Ninguém foi corajoso, ou burro, o bastante.<br>Construa uma taverna ou guilda.</div>'}</div>
      <p class="build-note">Você não manda nos heróis. Eles fazem o que querem.</p>`;
  } else if (state.tab === 'workers') {
    host.innerHTML = `<div class="panel-title"><h3>Ofícios dos camponeses</h3><span>${state.workers.filter(worker => worker.alive).length} no domínio</span></div>
      <p class="build-note">Para pescar, abra Wilds, clique em um marcador <b>Pesca</b> no riacho e atribua um camponês naquele ponto.</p>
      <div class="hero-list">${state.workers.filter(worker => worker.alive).sort((a, b) => a.name.localeCompare(b.name, 'pt-BR')).map(workerCard).join('') || '<div class="empty-state">Nenhum camponês. Construa um casebre para receber trabalhadores.</div>'}</div>`;
  } else if (state.tab === 'food') {
    host.innerHTML = foodPanelHTML();
  } else if (state.tab === 'economy') {
    host.innerHTML = economyPanelHTML();
  } else if (state.tab === 'market') {
    host.innerHTML = marketPanelHTML();
  }
  host.scrollTop = keepScroll;
  panelTabShown = state.tab;
  bindPanel(host);
  renderInspector();
}

// Inspetor da construção selecionada, no padrão dos jogos de construir cidade: quem é, vida, o que faz,
// as ações dela e, por último, pequeno no rodapé, mudar de lugar.
function buildingInfoHTML(building, actions = '') {
  const def = BUILDINGS.find(item => item.id === building.type);
  const alive = state.workers.filter(worker => worker.alive);
  const workersHere = alive.filter(worker => worker.buildSite == null && workplaceOf(worker)?.site === building.site);
  const names = list => list.length ? list.map(actor => actor.name).join(', ') : 'ninguém';
  const lines = [];
  const repairer = needsBuilder(building) && building.builderId != null ? findActor(building.builderId) : null;
  if (building.damage > 0) lines.push(['Conserto', `${building.noWood ? 'Parado: falta madeira' : repairer ? `${repairer.name} está consertando` : 'Esperando um camponês livre'} · ${Math.ceil(building.damage / REPAIR_HP_PER_WOOD)} madeira`]);
  if (building.type === 'farm') {
    const type = farmType(building.produce);
    const effectiveRate = farmOutput(building) * (1 + crewWork(building));
    const effectiveSecondaryRate = farmSecondaryRate(building) * (1 + crewWork(building));
    lines.push(['Produz', `${rateNumber(effectiveRate)} de ${foodName(farmFoodId(building))}/s${type.secondary ? ` · ${rateNumber(effectiveSecondaryRate)} de ${foodName(type.secondary)}/s` : ''}${building.produce === 'wheat' ? ' (vai pro moinho)' : ''}`]);
    lines.push(['Nível', `${producerLevel(building) + 1} de ${PRODUCER_LEVELS.length}`]);
    lines.push(['Esperando carregador', `${format(building.stock || 0)} de ${FARM_STOCK_CAP}`]);
    if (type.secondary) lines.push(['Carne aguardando', `${format(building.secondaryStock || 0)} de ${FARM_STOCK_CAP}`]);
    if (crewWork(building)) lines.push(['Ferramentas e profissão', `+${Math.round(crewWork(building) * 100)}% de produção`]);
    lines.push(['Trabalhando', `${workersHere.length}: ${names(workersHere)}`]);
  } else if (building.type === 'mill') {
    lines.push(['Mói', `${rateNumber(millRate(building))} de trigo em farinha por segundo`]);
    lines.push(['Nível', `${producerLevel(building) + 1} de ${PRODUCER_LEVELS.length}`]);
    lines.push(['No moinho', `${format(building.input || 0)} de trigo · ${format(building.stock || 0)} de farinha esperando`]);
    lines.push(['No depósito', `${format(state.food.wheat || 0)} de trigo · ${format(state.food.flour || 0)} de farinha`]);
    lines.push(['Trabalhando', `${workersHere.length}: ${names(workersHere)}`]);
  } else if (building.type === 'bakery') {
    const recipe = bakeryActiveRecipe(building);
    const pantry = building.pantry || {};
    lines.push(['Assa', `${rateNumber(bakeRate(building))} de ${recipe.name.toLowerCase()} por segundo`]);
    lines.push(['Receita', `${recipe.needs.map(id => `1 de ${foodName(id)}`).join(' + ')} → 1 de ${recipe.name.toLowerCase()}`]);
    lines.push(['Nível', `${producerLevel(building) + 1} de ${PRODUCER_LEVELS.length}`]);
    lines.push(['No forno', `${recipe.needs.map(id => `${format(pantry[id] || 0)} de ${foodName(id)}`).join(' · ')} · ${format(building.stock || 0)} de ${foodName(building.stockFood || recipe.id)} esperando`]);
    lines.push(['No depósito', recipe.needs.map(id => `${format(state.food[id] || 0)} de ${foodName(id)}`).join(' · ')]);
    if (crewWork(building)) lines.push(['Ferramentas e profissão', `+${Math.round(crewWork(building) * 100)}% de produção`]);
    lines.push(['Trabalhando', `${workersHere.length}: ${names(workersHere)}`]);
  } else if (building.type === 'butcher') {
    const recipe = butcherActiveRecipe(building), pantry = building.pantry || {};
    lines.push(['Processa', `${rateNumber(butcherRate(building))} de ${recipe.name} por segundo`]);
    lines.push(['Receita', `${recipe.needs.map(id => `1 de ${foodName(id)}`).join(' + ')} → 1 ${recipe.name} · alimenta ${recipe.feeds}×`]);
    lines.push(['Nível', `${producerLevel(building) + 1} de ${BUTCHER_MAX_LEVEL}`]);
    lines.push(['Matéria-prima', recipe.needs.map(id => `${format(pantry[id] || 0)} de ${foodName(id)}`).join(' · ')]);
    lines.push(['Produto pronto', `${format(building.stock || 0)} de ${foodName(building.stockFood || recipe.id)}`]);
    if (crewWork(building)) lines.push(['Culinária', `+${Math.round(crewWork(building) * 100)}% de produção`]);
    lines.push(['Trabalhando', `${workersHere.length}: ${names(workersHere)}`]);
  } else if (building.type === 'emberLab') {
    lines.push(['Refina', `${rateNumber(labRate(building))} Dead Remains por segundo`]);
    lines.push(['Nível', `${producerLevel(building) + 1} de ${PRODUCER_LEVELS.length}`]);
    lines.push(['Rende', REMAINS_GRADES.map(grade => `${grade.id} → ${grade.ember} de Ember`).join(' · ')]);
    lines.push(['No laboratório', building.input?.length ? `${building.input.join(', ')} · ${Math.floor((building.progress || 0) * 100)}% do primeiro` : 'nada']);
    lines.push(['No depósito', REMAINS_GRADES.map(grade => `${grade.id} ${state.remains?.[grade.id] || 0}`).join(' · ')]);
    lines.push(['Ember', format(state.ember || 0)]);
    lines.push(['Trabalhando', `${workersHere.length}: ${names(workersHere)}`]);
  } else if (building.type === 'blacksmith') {
    const recipe = blacksmithRecipe(building);
    lines.push(['Produz', `${rateNumber(blacksmithRate(building))} ${recipe.name} por segundo`]);
    lines.push(['Receita', `${Object.entries(recipe.needs).map(([id, qty]) => `${qty} ${METAL_NAMES[id]}`).join(' + ')} → ${Object.entries(recipe.makes).map(([id, qty]) => `${qty} ${METAL_NAMES[id]}`).join(' + ')}`]);
    if (building.weaponStatus) lines.push(['Forja e reparos', building.weaponStatus]);
    lines.push(['Metais guardados', METAL_STOCK.map(id => `${formatMetalAmount(id, state.metals[id] || 0)} ${METAL_NAMES[id]}`).join(' · ')]);
    if (crewWork(building)) lines.push(['Ofício', `+${Math.round(crewWork(building) * 100)}% de produção`]);
    lines.push(['Trabalhando', `${workersHere.length}: ${names(workersHere)}`]);
  } else if (building.type === 'hut') {
    const residents = alive.filter(worker => worker.homeSite === building.site);
    lines.push(['Moradores', `${residents.length} de ${building.big ? 5 : 2}: ${names(residents)}`]);
  } else if (building.type === 'depot') {
    lines.push(['Guarda', `${depotCap(building)} unidades`]);
    lines.push(['Ocupação total', `${Math.floor(storedUnits())} de ${storageCap()}`]);
  } else if (building.type === 'lumber') {
    const crew = workingCrew(building);
    lines.push(['Produz', `${rateNumber(LUMBER_RATE[crew.length] * (1 + workingCrewWork(building)))} de madeira por segundo`]);
    lines.push(['Lenhadores no posto', `${crew.length} de 3: ${names(crew)}`]);
  } else if (isPit(building)) {
    const crew = lumberCrew(building);
    lines.push(['Produz', building.type === 'mine' ? `${fineRate(pitRate(building))} kg/s de rocha bruta` : `${fineRate(pitRate(building))} kg/s de ${building.quarryOrder === 'marble' ? 'Mármore' : 'Pedra de Construção'}`]);
    if (building.type === 'mine') {
      const rank = metalRankData(building);
      lines.push(['Depósito', `Ranking ${rank.id} · ${rank.name}`]);
      lines.push(['Veios disponíveis', pitSeams(building).map(seam => `${oreName(seam.mineral)} · ${seam.access === 'deep' ? 'profundo' : 'raso'} · ${seam.gradePpm} ppm`).join(' · ')]);
    }
    if (crewWork(building)) lines.push(['Ferramentas', `+${Math.round(crewWork(building) * 100)}% de produção`]);
    lines.push(['No estoque', building.type === 'mine' ? METAL_STOCK.map(id => `${formatMetalAmount(id, state.metals[id] || 0)} ${METAL_NAMES[id]}`).join(' · ') : `${formatMass(state.stone || 0)} kg Pedra de Construção · ${formatMass(state.marble || 0)} kg Mármore`]);
  } else if (building.type === 'market') {
    lines.push(['Faz', 'Compra e vende comida e madeira a preço fixo']);
  } else if (building.type === 'tavern') {
    const drinkers = [...state.heroes, ...state.workers].filter(actor => actor.alive && isDrinking(actor));
    const guest = state.heroes.filter(hero => hero.alive && hero.homeSite === building.site);
    lines.push(['Faz', 'Uma cama para um herói, bebida e imposto']);
    lines.push(['Hóspede', `${guest.length} de 1: ${names(guest)}`]);
    lines.push(['Bebendo agora', `${drinkers.length}: ${names(drinkers)}`]);
    lines.push(['Imposto de hoje', `${format((state.tavernTab || 0) * taxFactor())} de ouro, pago à meia-noite`]);
  } else if (building.type === 'hotel') {
    const guests = state.heroes.filter(hero => hero.alive && hero.homeSite === building.site);
    lines.push(['Faz', '+4 camas para heróis']);
    lines.push(['Hóspedes', `${guests.length} de 4: ${names(guests)}`]);
    lines.push(['Heróis no domínio', `${state.heroes.filter(hero => hero.alive).length} de ${heroCapacity()}`]);
  } else if (building.type === 'armory') {
    const geared = [...state.heroes, ...state.workers].filter(actor => actor.alive && Object.keys(gearOf(actor)).length).length;
    lines.push(['Vende', `Equipamento até o tier ${ARMORY_LEVELS[Math.min(building.level || 0, ARMORY_LEVELS.length - 1)].tierCap}`]);
    lines.push(['Equipados', `${geared} personagens`]);
  } else if (def.heroClass) {
    const recruits = state.heroes.filter(hero => hero.alive && hero.guildSite === building.site);
    lines.push(['Recruta', GUILD_FORMS[def.id] ? guildForm(building) : building.branch || def.heroClass]);
    lines.push(['Heróis daqui', `${recruits.length}: ${names(recruits)}`]);
  }
  const moving = state.movingSite === building.site;
  const hp = buildingHp(building), maxHp = buildingMaxHp(building);
  const level = PRODUCER_TYPES.includes(building.type) ? `Nível ${producerLevel(building) + 1} de ${building.type === 'butcher' ? BUTCHER_MAX_LEVEL : building.type === 'blacksmith' ? BLACKSMITH_RATE.length : PRODUCER_LEVELS.length}`
    : building.type === 'armory' ? `Nível ${(building.level || 0) + 1} de ${ARMORY_LEVELS.length}`
    : building.type === 'depot' ? `Nível ${(building.level || 0) + 1} de ${STORAGE_LEVELS.length}`
    : def.heroClass ? `Nível ${guildTier(building) + 1} de ${GUILD_FORMS[def.id]?.length || 3}` : building.type === 'hut' ? (building.big ? 'Grande' : 'Pequeno') : '';
  // Obra nova ainda não funciona: mostra o que ela vai fazer. Reforma mostra os dados de antes, parados até acabar.
  const obra = building.underConstruction ? constructionStatus(building) : null;
  const reform = obra && (building.pendingUpgrade || building.armoryUpgradeTo);
  const workName = building.armoryUpgradeTo ? `Reforma do Arsenal · nível ${building.armoryUpgradeTo}` : building.upgradeLabel || 'Construção';
  const state_ = building.ruined ? 'Em ruínas, parada' : obra ? (reform ? 'Em reforma, parada até acabar' : 'Em obra') : building.damage > 0 ? 'Danificada, funcionando' : 'Pronta, funcionando';
  const obraLines = obra ? [['Obra', workName], ['Pedreiro', `<span data-obra-builder>${obra.builder}</span>`], ['Falta', `<span data-obra-eta>${obra.eta}</span>`]] : [];
  const body = obra && !reform ? [['Quando pronta', def.detail]] : lines.filter(([label]) => label !== 'Nível');
  const stats = [['Estado', state_], ...obraLines, ...body];
  return `<section class="inspector" aria-label="${siteLabel(building)}">
    <button type="button" class="inspector-back" data-inspector-close aria-label="Fechar" title="Fechar">×</button>
    <header class="inspector-id">
      ${building.type === 'quarry' || building.type === 'mine' ? `<span class="inspector-thumb building-sprite-sheet" style="${buildingImageStyle(siteArt(building))}" aria-hidden="true"></span>` : `<img class="inspector-thumb" src="${buildingImage(siteArt(building))}" alt="">`}
      <div><h3>${siteLabel(building)}</h3><small>${[def.name === siteLabel(building) ? '' : def.name, level].filter(Boolean).join(' · ')}</small></div>
    </header>
    ${obra ? `<div class="inspector-hp obra" data-obra-bar title="Progresso da obra" style="--hp:${obra.pct}%"><span>Obra</span><i></i><b>${obra.pct}%</b></div>` : ''}
    ${obra && !reform ? '' : `<div class="inspector-hp${building.ruined ? ' ruined' : building.damage > 0 ? ' hurt' : ''}" title="Vida da construção"><span>Vida</span><i style="--hp:${Math.round(hp / maxHp * 100)}%"></i><b>${hp} / ${maxHp}</b></div>`}
    <dl class="inspector-stats">${stats.map(([label, value]) => `<div><dt>${label}</dt><dd>${value}</dd></div>`).join('')}</dl>
    ${building.ruined ? '' : actions}
    ${building.type === 'market' && isDone(building) ? '<section class="inspector-section"><button type="button" class="hut-upgrade" data-open-market>Abrir o Mercado</button></section>' : ''}
    ${obra ? `<footer class="inspector-foot construction-actions"><button type="button" class="cancel-construction" data-cancel-construction="${building.site}">Cancelar obra</button><small>Ouro e madeira já gastos não são devolvidos.</small></footer>` : `<footer class="inspector-foot">${moving ? '<span>Toque no novo lugar no mapa. Esc cancela.</span>' : ''}<button type="button" class="move-site-btn" data-move-site="${building.site}">${moving ? 'Cancelar mudança' : 'Mudar de lugar'}</button>${moving ? '' : `<button type="button" class="demolish-btn${state.demolishArmed === building.site ? ' armed' : ''}" data-demolish="${building.site}">${state.demolishArmed === building.site ? 'Confirmar demolição' : 'Demolir'}</button>`}</footer>`}
  </section>`;
}

// Cancelar uma obra em andamento remove a construção nova ou volta uma melhoria ao nível anterior.
function cancelConstruction(site) {
  const building = state.buildings.find(item => item.site === site && item.underConstruction);
  if (!building) return;
  const name = buildingName(building);
  const upgrade = !!(building.pendingUpgrade || building.armoryUpgradeTo);
  const worker = building.builderId != null ? findActor(building.builderId) : null;
  if (worker) { worker.buildSite = null; worker.route = []; worker.finalPoint = null; worker.wait = 0; worker.headingToWork = false; }
  building.builderId = null;
  if (upgrade) {
    delete building.pendingUpgrade;
    delete building.upgradeLabel;
    delete building.armoryUpgradeTo;
    building.underConstruction = false;
    building.progress = BUILD_TIME;
    if (state.selectedSite === site) state.selectedSite = site;
  } else {
    state.buildings = state.buildings.filter(item => item !== building);
    if (state.selectedSite === site) state.selectedSite = null;
  }
  state.demolishArmed = null;
  assignBuilders();
  renderSites(); renderPanel(); renderHeroes(); updateHud();
  say(upgrade ? `Obra de melhoria de ${name} cancelada. A construção manteve o nível anterior; os custos gastos não foram devolvidos.` : `Obra de ${name} cancelada e removida. Os custos gastos não foram devolvidos.`);
}

// Demolir: primeiro clique arma, segundo confirma. Não devolve ouro nem madeira.
// Quem morava, trabalhava ou se hospedava ali fica sem o lugar e volta para o casarão.
function demolishBuilding(site) {
  const building = state.buildings.find(item => item.site === site);
  if (!building) return;
  if (state.demolishArmed !== site) {
    state.demolishArmed = site;
    renderPanel();
    say(`${siteLabel(building)}: clique de novo em Confirmar demolição. Não tem volta nem reembolso.`);
    return;
  }
  state.demolishArmed = null;
  state.buildings = state.buildings.filter(item => item !== building);
  [...state.heroes, ...state.workers].forEach(actor => {
    let touched = false;
    if (actor.homeSite === site) { actor.homeSite = null; touched = true; }
    if (actor.guildSite === site) { actor.guildSite = null; touched = true; }
    if (actor.jobSite === site) { actor.jobSite = null; touched = true; }
    if (actor.buildSite === site) { actor.buildSite = null; touched = true; }
    if (actor.recruitedFromSite === site) actor.recruitedFromSite = null;
    if (actor.workplace === building || actor.workplace?.site === site) { actor.workplace = null; touched = true; }
    if (actor.tavernSpot?.site === site) { actor.tavernSpot = null; touched = true; }
    if (touched) { actor.route = []; actor.finalPoint = null; actor.wait = 0; actor.workTrip = false; }
  });
  (state.bandits || []).forEach(monster => { if (monster.targetSite === site) monster.targetSite = null; });
  if (state.selectedSite === site) state.selectedSite = null;
  if (state.movingSite === site) state.movingSite = null;
  assignBuilders();
  renderSites(); renderPanel(); renderHeroes(); updateHud();
  say(`${siteLabel(building)}: demolido. Os vizinhos levaram as tábuas antes da poeira baixar.`);
}

// Mudar de lugar: a construção pronta vai inteira para outro ponto livre, sem custo.
function startMove(site) {
  const building = state.buildings.find(item => item.site === site && isDone(item));
  if (!building) return;
  state.selectedBuild = null;
  state.movingSite = site;
  state.selectedSite = site;
  renderSites(); renderPanel();
  say(`${siteLabel(building)}: toque no novo lugar. Esc cancela.`);
}
function cancelMove() {
  if (state.movingSite == null) return;
  state.movingSite = null;
  renderSites(); renderPanel();
  say('Mudança cancelada. Ninguém queria carregar nada mesmo.');
}
function moveBuilding(x, y) {
  const building = state.buildings.find(item => item.site === state.movingSite);
  if (!building) { state.movingSite = null; return; }
  ({ x, y } = snapPlacement(building.type, x, y));
  const problem = placementProblem(x, y, building.site);
  if (problem) { say(problem); return; }
  building.x = Math.round(x * 10) / 10;
  building.y = Math.round(y * 10) / 10;
  state.movingSite = null;
  [...state.heroes, ...state.workers].forEach(actor => { if (actor.tavernSpot?.site === building.site) actor.tavernSpot = null; actor.route = []; });
  renderSites(); renderPanel();
  say(`${siteLabel(building)} mudou de lugar. Os camponeses vão reclamar do caminho novo.`);
}

function chooseBuild(id) {
  state.placingBounty = false;
  const item = BUILDINGS.find(building => building.id === id);
  if (state.gold < item.cost || state.timber < item.wood || state.stone < buildingStoneCost(item)) {
    say('Falta ouro, madeira ou Pedra de Construção. As dívidas, não.');
    return;
  }
  if (state.selectedBuild === id) { cancelPlacement(); return; }
  state.movingSite = null;
  state.selectedBuild = id;
  state.selectedSite = null;
  selectedLairId = null;
  renderSites(); renderPanel();
  say(id === 'farm' ? 'Escolha alho, trigo, suínos, ovos, leite, batatas, cenouras ou tomates e toque onde quiser no mapa. A produção das hortaliças pode ser trocada no painel. Esc cancela.' : `${item.name}: toque onde quiser no mapa. Esc cancela.`, `${item.name} procura um terreno. E alguém pra pagar.`);
}

function cancelPlacement() {
  if (!state.selectedBuild) return;
  state.selectedBuild = null;
  renderSites(); renderPanel();
  say('Obra cancelada. Os pedreiros nem notaram.');
}

function chooseFarmType(id) {
  const type = farmType(id);
  if (state.selectedBuild === 'farm') {
    state.selectedFarmType = type.id;
    renderPanel();
    say(`Fazenda de ${type.name}. Toque onde quiser no mapa.`);
    return;
  }
  const farm = state.buildings.find(building => building.site === state.selectedSite && building.type === 'farm' && isDone(building));
  if (!farm) return;
  if (farm.produce !== type.id) { farm.stock = 0; farm.secondaryStock = 0; }
  farm.produce = type.id;
  state.selectedFarmType = type.id;
  renderSites(); renderPanel(); updateHud();
  say(`Fazenda agora produz ${type.name.toLowerCase()}.`, `A fazenda mudou sua produção para ${type.name.toLowerCase()}.`);
}

// Padaria: troca a fornada. Os ingredientes já no forno ficam; a fornada antiga sai primeiro.
function chooseBakeryRecipe(id) {
  const bakery = state.buildings.find(building => building.site === state.selectedSite && building.type === 'bakery' && isDone(building));
  if (!bakery) return;
  const recipe = bakeryRecipe(id);
  if (recipe.id === 'quiche' && producerLevel(bakery) < 1) { say('A Quiche Lorraine só fica disponível quando a padaria chega ao nível 2.'); return; }
  bakery.produce = recipe.id;
  renderSites(); renderPanel(); updateHud();
  say(`A padaria agora assa ${recipe.name.toLowerCase()}: ${recipe.needs.map(need => foodName(need)).join(', ')}.`, `A padaria mudou a fornada para ${recipe.name.toLowerCase()}.`);
}
function chooseButcherRecipe(id) {
  const butcher = state.buildings.find(building => building.site === state.selectedSite && building.type === 'butcher' && isDone(building));
  if (!butcher) return;
  butcher.produce = butcherRecipe(id).id;
  renderSites(); renderPanel(); updateHud();
  say(`O açougue agora prepara ${butcherActiveRecipe(butcher).name.toLowerCase()}.`);
}
function chooseBlacksmithRecipe(id) {
  const blacksmith = state.buildings.find(building => building.site === state.selectedSite && building.type === 'blacksmith' && isDone(building));
  const recipe = BLACKSMITH_RECIPES.find(item => item.id === id && item.minLevel <= producerLevel(blacksmith) + 1);
  if (!blacksmith || !recipe) return;
  blacksmith.produce = recipe.id;
  blacksmith.refineProgress = 0;
  renderSites(); renderPanel(); updateHud();
  say(`Blacksmith agora produz ${recipe.name}.`);
}
function reserveWeaponMaterials(needs) {
  if (Object.entries(needs).some(([id, qty]) => (id === 'timber' ? state.timber : state.metals[id] || 0) + 1e-9 < qty)) return false;
  Object.entries(needs).forEach(([id, qty]) => { if (id === 'timber') state.timber -= qty; else state.metals[id] -= qty; });
  return true;
}
function releaseWeaponMaterials(needs = {}) {
  Object.entries(needs).forEach(([id, qty]) => { if (id === 'timber') state.timber += qty; else state.metals[id] = (state.metals[id] || 0) + qty; });
}
function placeWeaponOrder(site) {
  const smith = state.buildings.find(item => item.site === site && item.type === 'blacksmith' && isDone(item));
  const design = weaponDesign(smith?.weaponPattern || 'dagger', smith?.weaponMaterial || 'copper');
  if (!smith || !design || producerLevel(smith) + 1 < weaponMinLevel(design.materialId) || smith.weaponOrder || smith.weaponJob || smith.repairJob) return;
  if (!reserveWeaponMaterials(design.needs)) { say('Falta material para esta arma. A ordem não foi criada.'); return; }
  smith.weaponOrder = { id: `weapon-order-${site}-${state.ticks}`, patternId: design.pattern.id, materialId: design.materialId, reserved: design.needs };
  smith.weaponStatus = 'Ordem aguardando a forja';
  say(`Ordem aberta: ${design.name}. Os materiais estão reservados no Blacksmith.`);
  renderPanel(); updateHud();
}
function cancelWeaponOrder(site) {
  const smith = state.buildings.find(item => item.site === site && item.type === 'blacksmith');
  if (!smith) return;
  const pending = smith.weaponOrder || smith.weaponJob || smith.repairJob;
  if (!pending) return;
  releaseWeaponMaterials(pending.reserved || {});
  smith.weaponOrder = null; smith.weaponJob = null; smith.repairJob = null; smith.weaponStatus = 'Ordem cancelada · materiais devolvidos';
  renderPanel(); updateHud();
  say('Ordem do Blacksmith cancelada. Os insumos reservados voltaram ao estoque.');
}
function orderWeaponRepair(site, weaponId, part) {
  const smith = state.buildings.find(item => item.site === site && item.type === 'blacksmith' && isDone(item));
  const weapon = craftedWeapon(weaponId);
  if (!smith || !weapon || smith.weaponOrder || smith.weaponJob || smith.repairJob) return;
  const material = part === 'edge' ? weapon.edgeMaterial : weapon.bodyMaterial;
  if (part === 'edge' && !weapon.sharp) return;
  const value = part === 'edge' ? weapon.edgeCondition : weapon.structuralCondition;
  const max = part === 'edge' ? weapon.edgeMax : weapon.structuralMax;
  if (value >= max - 1e-6) return;
  const needs = { [material]: .25 };
  if (!reserveWeaponMaterials(needs)) { say(`Falta ${METAL_NAMES[material] || material} para esse reparo.`); return; }
  smith.repairJob = { id: `weapon-repair-${site}-${state.ticks}`, weaponId, part, reserved: needs, elapsed: 0, required: 30 };
  smith.weaponStatus = `Reparo de ${part === 'edge' ? 'gume' : 'estrutura'} em produção`;
  renderPanel(); updateHud();
}

// Obras e melhorias: um camponês trabalha no local durante dois minutos de jogo.
// A habilidade de construção e as ferramentas dele aceleram (veja progressConstruction).
const BUILD_TIME = 120;
const buildingName = building => building.type === 'manor' ? 'o Casarão' : building.type === 'farm' ? building.produce === 'eggs' ? 'Galinheiro' : `Fazenda de ${farmType(building.produce).name}` : building.type === 'bakery' ? 'Padaria' : BUILDINGS.find(item => item.id === building.type)?.name || 'Construção';

function placeBuilding(x, y) {
  const item = BUILDINGS.find(building => building.id === state.selectedBuild);
  if (!item) return;
  ({ x, y } = snapPlacement(item.id, x, y));
  const problem = placementProblem(x, y);
  if (problem) { say(problem); return; }
  if (state.gold < item.cost || state.timber < item.wood || state.stone < buildingStoneCost(item)) {
    say('A obra parou: falta ouro, madeira ou Pedra de Construção. Os pedreiros foram beber.');
    return;
  }
  state.gold -= item.cost;
  state.timber -= item.wood;
  state.stone -= buildingStoneCost(item);
  const site = state.nextBuildingId++;
  const resourcePlace = item.id === 'mine' ? extractionPlaces(item.id).find(place => Math.hypot(place.x - x, (place.y - y) * Y_TO_X_SCALE) <= 5.5) : null;
  const quarryInitialUnits = currentMap === 'city' ? 2400 : 20000;
  const building = { site, mapId: currentMap, mapSet: currentMapSet, resourcePlaceId: item.id === 'quarry' ? `quarry-${currentMapSet}-${site}` : resourcePlace?.id || null, quarryInitialUnits: item.id === 'quarry' ? quarryInitialUnits : undefined, depositRank: item.id === 'mine' ? (resourcePlace?.rank || (currentMapSet === 2 ? 'IV' : 'II')) : null, type: item.id, produce: ['bakery', 'blacksmith'].includes(item.id) ? (item.id === 'bakery' ? 'bread' : 'iron') : item.id === 'farm' ? state.selectedFarmType : null, branch: null, x: Math.round(x * 10) / 10, y: Math.round(y * 10) / 10, builtAt: Date.now(), underConstruction: true, progress: 0, builderId: null };
  if (item.id === 'mine' && resourcePlace) {
    const deposit = ensureMineralDeposit(resourcePlace, currentMapSet);
    const starterIron = deposit.seams.find(seam => seam.mineral === 'iron' && seam.access === 'shallow');
    building.selectedSeamId = starterIron?.id || deposit.seams[0]?.id || null;
    building.stockGrams = 0;
  } else if (item.id === 'quarry') {
    const deposit = stoneDepositForPit(building);
    building.quarryFaceId = deposit.faces[0]?.id || null;
    building.quarryOrder = 'rubble';
    quarryInventory(building);
    building.stockStone = 0;
  }
  state.buildings.push(building);
  state.selectedBuild = null;
  // A obra nova não fica selecionada: a janela dela só abre quando o jogador clica na construção.
  state.selectedSite = null;
  assignBuilders();
  renderSites(); renderPanel(); updateHud();
  const builder = building.builderId != null ? findActor(building.builderId) : null;
  say(builder ? `Obra de ${buildingName(building)} iniciada. ${builder.name} vai trabalhar nela por dois minutos.` : `Obra de ${buildingName(building)} marcada. Falta um camponês livre pra trabalhar nela.`);
}

function busyBuilders() {
  return new Set(repairSites().filter(building => needsBuilder(building) && building.builderId != null).map(building => String(building.builderId)));
}

// Cada obra sem camponês recebe o primeiro camponês livre.
function assignBuilders() {
  repairSites().filter(needsBuilder).sort((a, b) => Number(!!b.underConstruction) - Number(!!a.underConstruction)).forEach(building => {
    const current = building.builderId != null ? findActor(building.builderId) : null;
    if (current && current.alive) return;
    const busy = busyBuilders();
    const rank = candidate => candidate.profession === 'builder' ? 0 : !candidate.profession ? 1 : 2;
    const worker = state.workers.filter(candidate => candidate.alive && !busy.has(String(candidate.id))).sort((a, b) => rank(a) - rank(b))[0];
    building.builderId = worker ? worker.id : null;
    if (worker) { interruptTavernVisit(worker); worker.buildSite = building.site; worker.route = []; worker.finalPoint = null; worker.wait = 0; }
  });
}

// A cada BUILD_ACTION_SECONDS de obra ou conserto, uma ação de profissão.
function buildPractice(worker, dt, repairing) {
  const every = BUILD_ACTION_SECONDS / (1 + tradeBuildBonus(worker, repairing) / 2);
  worker.buildClock = (worker.buildClock || 0) + dt;
  while (worker.buildClock >= every) { worker.buildClock -= every; gainTrade(worker, 'build'); }
}

// O camponês trabalha ao lado da obra, na base. O rótulo de progresso fica no alto da obra, longe dele.
const buildSpot = building => ({ x: building.x + (building.x > 92 ? -5 : 5), y: building.y });

// Uma vez por segundo de jogo: a obra avança só com o camponês no local.
function progressConstruction(dt) {
  assignBuilders();
  repairSites().filter(needsBuilder).forEach(building => {
    const worker = building.builderId != null ? findActor(building.builderId) : null;
    const spot = buildSpot(building);
    if (!building.underConstruction) {
      building.noWood = false;
      if (worker && Math.hypot(worker.x - spot.x, worker.y - spot.y) < 1.5) {
        // Conserto custa madeira: sem madeira no estoque, o camponês fica parado esperando.
        const wanted = Math.min(building.damage, REPAIR_RATE * dt * (1 + tradeBuildBonus(worker, true) / 2 + gearBonus(worker, 'build')));
        const repaired = Math.min(wanted, Math.max(0, state.timber) * REPAIR_HP_PER_WOOD);
        state.timber = Math.max(0, state.timber - repaired / REPAIR_HP_PER_WOOD);
        building.noWood = repaired < wanted;
        building.damage = Math.max(0, building.damage - repaired);
        if (repaired > 0) buildPractice(worker, dt, true);
        if (building.damage <= 0) {
          building.damage = 0;
          building.ruined = false;
          building.builderId = null;
          worker.buildSite = null; worker.route = []; worker.finalPoint = null; worker.wait = 0;
          renderSites(); renderManorHp();
          say(`${siteLabel(building)} consertad${building.type === 'manor' ? 'o' : 'a'} por ${worker.name}. Até o próximo monstro.`);
          return;
        }
      }
      if (building.type === 'manor') { renderManorHp(); return; }
      const label = $(`.site[data-site="${building.site}"] small`);
      if (label) label.textContent = `${siteLabel(building)} · ${building.ruined ? 'em ruínas' : building.noWood ? 'falta madeira' : 'consertando'} ${buildingHp(building)}/${buildingMaxHp(building)}`;
      return;
    }
    if (worker && Math.hypot(worker.x - spot.x, worker.y - spot.y) < 1.5) {
      const previousProgress = building.progress || 0;
      building.progress = Math.min(BUILD_TIME, previousProgress + dt * (1 + tradeBuildBonus(worker, false) / 2 + gearBonus(worker, 'build')));
      buildPractice(worker, dt, false);
      if (building.progress >= BUILD_TIME) { completeBuilding(building); return; }
      if (previousProgress < BUILD_TIME / 2 && building.progress >= BUILD_TIME / 2) renderSites();
    }
    const label = $(`.site[data-site="${building.site}"] .build-progress`);
    if (label) label.textContent = constructionLabel(building);
    refreshConstructionInspector(building);
  });
}

// Situação da obra para o inspetor: quanto falta, quem está nela e quanto tempo de trabalho resta.
function constructionStatus(building) {
  const worker = building.builderId != null ? findActor(building.builderId) : null;
  const progress = building.progress || 0;
  const pct = Math.floor(progress / BUILD_TIME * 100);
  const spot = buildSpot(building);
  const onSite = worker && Math.hypot(worker.x - spot.x, worker.y - spot.y) < 1.5;
  const builder = worker ? `${worker.name}, ${onSite ? 'trabalhando' : 'a caminho'}` : 'Esperando um camponês livre';
  let eta = 'Parada sem camponês';
  if (worker) {
    const left = Math.ceil((BUILD_TIME - progress) / (1 + tradeBuildBonus(worker, false) / 2 + gearBonus(worker, 'build')));
    eta = `${left >= 60 ? `${Math.floor(left / 60)} min ` : ''}${left % 60 || left < 60 ? `${left % 60} s` : ''}`.trim() + ' de trabalho';
  }
  return { pct, builder, eta };
}
function refreshConstructionInspector(building) {
  if (state.selectedSite !== building.site) return;
  const box = $('#site-inspector');
  if (!box || box.hidden) return;
  const status = constructionStatus(building);
  const bar = $('[data-obra-bar]', box);
  if (bar) { bar.style.setProperty('--hp', `${status.pct}%`); $('b', bar).textContent = `${status.pct}%`; }
  const builder = $('[data-obra-builder]', box); if (builder) builder.textContent = status.builder;
  const eta = $('[data-obra-eta]', box); if (eta) eta.textContent = status.eta;
}

function constructionLabel(building) {
  const worker = building.builderId != null ? findActor(building.builderId) : null;
  const work = building.armoryUpgradeTo ? `Reforma do Arsenal · nível ${building.armoryUpgradeTo} · ` : building.upgradeLabel ? `${building.upgradeLabel} · ` : 'Em obra · ';
  return `${work}${Math.floor((building.progress || 0) / BUILD_TIME * 100)}%${worker ? '' : ' · sem camponês'}`;
}

function completeBuilding(building) {
  const item = BUILDINGS.find(entry => entry.id === building.type);
  const worker = building.builderId != null ? findActor(building.builderId) : null;
  if (building.pendingUpgrade) {
    const upgrade = building.pendingUpgrade;
    delete building.pendingUpgrade;
    delete building.upgradeLabel;
    building.underConstruction = false;
    building.progress = BUILD_TIME;
    building.builderId = null;
    if (worker) { worker.buildSite = null; worker.route = []; worker.finalPoint = null; worker.wait = 0; }
    finishUpgrade(building, upgrade);
    markUpgrade(building, upgrade.kind === 'hut' ? 'Casebre Grande' : `Nível ${buildingLevel(building) + 1}`);
    assignBuilders();
    renderSites(); renderPanel(); renderHeroes(); updateHud();
    return;
  }
  if (building.type === 'armory' && building.armoryUpgradeTo) {
    const level = building.armoryUpgradeTo;
    building.level = level - 1;
    delete building.armoryUpgradeTo;
    building.underConstruction = false;
    building.progress = BUILD_TIME;
    building.builtAt = Date.now();
    building.builderId = null;
    if (worker) { worker.buildSite = null; worker.route = []; worker.finalPoint = null; worker.wait = 0; }
    gainHeirXp(25);
    markUpgrade(building, `Nível ${level}`);
    assignBuilders();
    renderSites(); renderPanel(); updateHud();
    setTimeout(() => $(`.site[data-site="${building.site}"]`)?.classList.remove('just-built'), 1400);
    say(`Reforma concluída. Arsenal no nível ${level}, equipamento até tier ${ARMORY_LEVELS[level - 1].tierCap}.`);
    return;
  }
  building.underConstruction = false;
  building.progress = BUILD_TIME;
  building.builtAt = Date.now();
  building.builderId = null;
  if (worker) { worker.buildSite = null; worker.route = []; worker.finalPoint = null; worker.wait = 0; }
  gainHeirXp(25);
  const arrived = item.heroClass ? recruit(item.heroClass, building, null, true) : null;
  if (item.housing) for (let i = 0; i < item.housing; i++) hireWorker(building, i % 2 === 1);
  // Fazenda ou moinho novo: todo mundo é redistribuído para ninguém ficar vazio.
  if (PRODUCER_TYPES.includes(item.id)) state.workers.forEach(worker => { worker.jobSite = null; });
  assignBuilders();
  renderSites(); renderPanel(); updateHud();
  setTimeout(() => $(`.site[data-site="${building.site}"]`)?.classList.remove('just-built'), 1400);
  const name = buildingName(building);
  const credit = worker ? ` ${worker.name} terminou a obra.` : '';
  say(arrived ? `${item.name} abriu as portas.${credit} ${arrived.name} já está reclamando da cama.` : item.housing ? `Casebre Pequeno erguido.${credit} Chegaram dois camponeses. Eles não tinham pra onde ir mesmo.` : `${name} erguida.${credit} Ninguém morreu na obra. Ainda.`);
}

function recruit(className, origin, prestige = null, guildResident = false) {
  if (!origin) return null;
  const lodging = guildResident ? null : freeLodging();
  if (guildResident && state.heroes.some(hero => hero.alive && hero.guildSite === origin.site)) {
    say('Esta guilda já tem seu único morador.');
    return null;
  }
  if (!guildResident && !lodging) {
    say('Sem cama na taverna ou no hotel. O herói foi embora ofendido.');
    return null;
  }
  const hero = { id: Date.now() + Math.random(), name: heroName(), className, prestige, trait: pick(Object.keys(TRAITS)), level: 1, xp: 0, x: origin.x, y: origin.y, alive: true, guildSite: guildResident ? origin.site : null, recruitedFromSite: origin.site ?? null, homeSite: lodging?.site ?? null, purse: HERO_START_PURSE, gear: STARTER_WEAPON[className] ? { weapon: STARTER_WEAPON[className] } : {} };
  if (guildResident && GUILD_FORMS[origin.type]) hero.guildForm = guildForm(origin);
  if (className === 'MoonWarden') hero.gear.offhand = 'plankShield';
  state.heroes.push(hero);
  gainHeirXp(10);
  renderHeroes();
  updateHud();
  return hero;
}

function freeWorkerHome() {
  const workers = state.workers.filter(worker => worker.alive);
  const hut = state.buildings.find(building => building.type === 'hut' && isDone(building)
    && workers.filter(worker => worker.homeSite === building.site).length < (building.big ? 5 : 2));
  if (hut) return hut;
  const manorResidents = workers.filter(worker => worker.home === 'manor' || worker.homeSite == null).length;
  return manorResidents < 2 ? MANOR() : null;
}

function workerArrival() {
  if (!arrivalIsDue('workers') || state.workers.filter(worker => worker.alive).length >= workerCapacity()) return null;
  const home = freeWorkerHome();
  if (!home) return null;
  const worker = hireWorker(home, Math.random() < 0.5);
  if (worker) say(`${worker.name} chegou ao domínio e foi morar ${home.site === 'manor' ? 'no casarão' : `em ${buildingName(home)}`}.`);
  return worker;
}

function heroArrival() {
  if (!arrivalIsDue('heroes') || state.heroes.filter(hero => hero.alive).length >= heroCapacity()) return null;
  if (manorHeroes() < MANOR_HERO_SLOTS) {
    const className = pick(BASE_HERO_CLASSES);
    const hero = { id: Date.now() + Math.random(), name: heroName(), className, prestige: null, trait: pick(Object.keys(TRAITS)), level: 1, xp: 0, x: MANOR_DOOR.x, y: MANOR_DOOR.y, alive: true, home: 'manor', guildSite: null, recruitedFromSite: null, homeSite: null, purse: HERO_START_PURSE, gear: STARTER_WEAPON[className] ? { weapon: STARTER_WEAPON[className] } : {} };
    state.heroes.push(hero);
    gainHeirXp(10);
    renderHeroes();
    updateHud();
    say(`${hero.name}, ${className}, bateu na porta do casarão de madrugada. Tinha cama vaga, e ninguém teve coragem de recusar.`);
    return hero;
  }
  const emptyGuild = state.buildings.find(building => BUILDINGS.some(item => item.id === building.type && item.heroClass)
    && isDone(building) && !state.heroes.some(hero => hero.alive && hero.guildSite === building.site));
  if (emptyGuild) {
    const hero = recruit(BUILDINGS.find(item => item.id === emptyGuild.type).heroClass, emptyGuild, null, true);
    if (hero) say(`${hero.name}, ${hero.className}, chegou para ocupar a vaga livre na guilda.`);
    return hero;
  }
  const lodging = freeLodging();
  if (!lodging) return null;
  const guildClasses = BUILDINGS.filter(item => item.heroClass && !GUILD_FORMS[item.id] && state.buildings.some(built => built.type === item.id && isDone(built))).map(item => item.heroClass);
  const arrivals = lodging.type === 'hotel' && guildClasses.length ? guildClasses : BASE_HERO_CLASSES;
  const className = pick(arrivals);
  const hero = recruit(className, lodging);
  if (hero) say(`${hero.name}, ${className}, chegou ao domínio. Ninguém perguntou de onde.`);
  return hero;
}

const HUT_UPGRADE = { gold: 180, wood: 12 };
// Melhoria vira obra: o ouro e a madeira saem na hora, e o efeito só vale quando o camponês termina.
function startUpgrade(building, upgrade, label) {
  building.pendingUpgrade = upgrade;
  building.upgradeLabel = label;
  building.underConstruction = true;
  building.progress = 0;
  building.builderId = null;
  assignBuilders();
  renderSites(); renderPanel(); updateHud();
  const worker = building.builderId != null ? findActor(building.builderId) : null;
  say(worker ? `${label}: obra iniciada. ${worker.name} vai trabalhar nela por dois minutos.` : `${label}: obra iniciada. Falta um camponês livre para trabalhar nela.`);
}
function finishUpgrade(building, upgrade) {
  gainHeirXp(25);
  if (upgrade.kind === 'hut') {
    building.big = true;
    for (let i = 0; i < 3; i++) hireWorker(building, Math.random() < 0.5);
    say('Casebre Grande pronto. Chegaram mais três. Cabem cinco, se ninguém respirar fundo.');
  } else if (upgrade.kind === 'depot') {
    building.level = (building.level || 0) + 1;
    say(`Depósito de Provisões ampliado para ${depotCap(building)} unidades. Os ratos agradecem o espaço extra.`);
  } else if (upgrade.kind === 'pit') {
    building.level = 1;
    building.derrick ||= { capacityKg: 160, shared: true };
    building.sharedCart ||= { capacityKg: 160, route: 'quarry-city', cargoId: null, cargo: null, tripRemaining: 0 };
    say(`${buildingName(building)} no nível 2: quatro vagas e acesso ampliado ao depósito.`);
  } else if (upgrade.kind === 'producer') {
    building.level = producerLevel(building) + 1;
    say(building.type === 'mill' ? `Moinho no nível ${building.level + 1}: mói ${rateNumber(millRate(building))} de trigo por segundo.` : building.type === 'emberLab' ? `Laboratório de Ember no nível ${building.level + 1}: refina ${rateNumber(labRate(building))} Dead Remains por segundo. Os vapores também melhoraram.` : building.type === 'bakery' ? `Padaria no nível ${building.level + 1}: ${rateNumber(bakeRate(building))} fornadas por segundo. O forno esquenta mais; o padeiro, também.` : building.type === 'butcher' ? `Açougue no nível ${building.level + 1}: processa ${rateNumber(butcherRate(building))} por segundo.` : `${buildingName(building)} no nível ${building.level + 1}: ${rateNumber(farmOutput(building))} por segundo. A terra não pediu, mas aceitou.`);
  } else if (upgrade.kind === 'guild') {
    const def = BUILDINGS.find(item => item.id === building.type);
    if (GUILD_FORMS[building.type]) {
      building.tier = upgrade.tier + 1;
      building.guildForm = upgrade.branch;
      const resident = state.heroes.find(hero => hero.alive && hero.guildSite === building.site);
      if (resident) {
        const life = hpOf(resident) / maxHpOf(resident);
        resident.guildForm = guildForm(building);
        resident.hp = Math.max(1, Math.round(life * maxHpOf(resident)));
      }
      say(`${def.name} chegou ao nível ${building.tier + 1}. Forma disponível: ${guildForm(building)}.`);
    } else if (upgrade.tier === 0) {
      building.tier = 1;
      const hero = recruit(def.heroClass, building, null, false);
      say(hero ? `${def.name} chegou ao nível 2. ${hero.name} ficou alojado fora da guilda.` : `${def.name} chegou ao nível 2, mas falta alojamento para o herói novo.`);
    } else {
      building.tier = 2;
      building.branch = upgrade.branch;
      const hero = recruit(def.heroClass, building, upgrade.branch);
      say(hero ? `${def.name} chegou ao nível 3 e forma ${upgrade.branch}. ${hero.name} ficou alojado fora da guilda.` : `${def.name} chegou ao nível 3, mas falta alojamento para o herói novo.`);
    }
  }
}

function upgradeHut(site) {
  const hut = state.buildings.find(building => building.site === site && building.type === 'hut');
  if (!hut || hut.big || hut.underConstruction) return;
  if (state.gold < HUT_UPGRADE.gold || state.timber < HUT_UPGRADE.wood) { say('Falta ouro ou madeira. Os camponeses vão continuar apertados.'); return; }
  state.gold -= HUT_UPGRADE.gold;
  state.timber -= HUT_UPGRADE.wood;
  startUpgrade(hut, { kind: 'hut' }, 'Ampliação para Casebre Grande');
}

// Camas para camponeses: 2 no casarão, 2 por casebre, 5 no Casebre Grande. Ninguém chega sem cama.
const workerCapacity = () => 2 + state.buildings.filter(building => building.type === 'hut' && isDone(building)).reduce((sum, hut) => sum + (hut.big ? 5 : 2), 0);
function hireWorker(home, female) {
  if (state.workers.filter(worker => worker.alive).length >= workerCapacity()) return null;
  const n = state.workers.length + 1;
  const worker = {
    id: `worker-${Date.now()}-${n}`,
    name: peasantName(female),
    level: 1, xp: 0,
    className: 'Peasant',
    spriteId: female ? 'female-peasant' : 'peasant',
    portraitId: female ? 'female-peasant' : 'male-peasant',
    x: home.x, y: home.y, alive: true, workTrip: false, ...(home.site === 'manor' ? { home: 'manor' } : { homeSite: home.site }), purse: WORKER_START_PURSE,
  };
  state.workers.push(worker);
  renderHeroes();
  updateHud();
  if (state.tab === 'heroes' || state.tab === 'workers') renderPanel();
  return worker;
}

function ensureFarmWorkers() {
  state.buildings.filter(building => ['farm', 'mill'].includes(building.type) && isDone(building)).forEach(building => {
    const worker = state.workers.find(candidate => candidate.alive && (
      candidate.homeSite === building.site || candidate.jobSite === building.site || candidate.workplace?.site === building.site
    ));
    if (worker) {
      if (worker.homeSite === building.site && !worker.workplace) worker.jobSite = building.site;
      return;
    }
  });
}

// Nível 1 traz um morador da guilda; níveis 2 e 3 trazem heróis extras, alojados em taverna/hotel.
const GUILD_UPGRADES = [{ gold: 240, wood: 16 }, { gold: 360, wood: 24 }];
const guildTier = guild => guild.tier ?? (guild.branch ? 2 : 0);
const guildForm = guild => guild.guildForm || GUILD_FORMS[guild.type][guildTier(guild)][0];
function upgradeGuild(site, branch) {
  const guild = state.buildings.find(building => building.site === site);
  const def = guild && BUILDINGS.find(item => item.id === guild.type);
  if (!def?.heroClass || guild.underConstruction) return;
  const tier = guildTier(guild);
  const cost = GUILD_UPGRADES[tier];
  if (!cost) return;
  const forms = GUILD_FORMS[def.id];
  if (forms ? !forms[tier + 1]?.includes(branch) : tier === 1 && !PRESTIGE[def.heroClass]?.includes(branch)) return;
  if (!forms && !freeLodging()) { say('A guilda mantém um único herói. Construa uma taverna ou hotel para alojar os próximos níveis.'); return; }
  if (state.gold < cost.gold || state.timber < cost.wood) { say('Falta ouro ou madeira. Os heróis vão continuar dividindo beliche.'); return; }
  state.gold -= cost.gold;
  state.timber -= cost.wood;
  startUpgrade(guild, { kind: 'guild', tier, branch }, `${def.name} para o nível ${tier + 2}`);
}

function guildBoxHTML(guild) {
  const def = BUILDINGS.find(item => item.id === guild.type);
  const tier = guildTier(guild);
  const members = state.heroes.filter(hero => hero.alive && hero.guildSite === guild.site).length;
  const cost = GUILD_UPGRADES[tier];
  let actions = '';
  const forms = GUILD_FORMS[def.id];
  if (forms) {
    const next = forms[tier + 1];
    actions = `<p class="storage-items">Nível ${tier + 1} · ${guildForm(guild)}</p>`;
    if (next) actions += `<div class="guild-branches">${next.map(form =>
      `<button type="button" class="hut-upgrade" data-guild-upgrade="${guild.site}" data-branch="${form}">Nível ${tier + 2} · ${form} · ${costHTML(cost.gold, cost.wood)}</button>`
    ).join('')}</div>`;
  } else if (tier === 0) actions = `<button type="button" class="hut-upgrade" data-guild-upgrade="${guild.site}" ${freeLodging() ? '' : 'disabled'}>Nível 2 · ${costHTML(cost.gold, cost.wood)} · +1 ${def.heroClass} (precisa de cama)</button>`;
  else if (tier === 1) actions = `<p class="storage-items">Nível 3: escolha a especialização (${costHTML(cost.gold, cost.wood)}). Chega um herói especializado; os anteriores ficam como são.</p>
      <div class="guild-branches">${PRESTIGE[def.heroClass].map(path => `<button type="button" class="hut-upgrade" data-guild-upgrade="${guild.site}" data-branch="${path}" ${freeLodging() ? '' : 'disabled'}>${path} · +1 ${path}</button>`).join('')}</div>`;
  else actions = `<p class="storage-items">Especializada em ${guild.branch}. Guilda no nível máximo.</p>`;
  return `<section class="inspector-section"><h4>Melhoria · ${members}/1 morador na guilda</h4>${actions}</section>`;
}

function assignPrestige(hero) {
  if (!hero || hero.prestige) return;
  const guildType = GUILD_FOR[hero.className];
  const available = state.buildings.filter(b => b.type === guildType && b.branch && isDone(b));
  if (!available.length) return;
  const nearest = available.sort((a, b) => {
    const da = Math.hypot(a.x - hero.x, a.y - hero.y);
    const db = Math.hypot(b.x - hero.x, b.y - hero.y);
    return da - db;
  })[0];
  hero.prestige = nearest.branch;
  say((PROMO_MSG[hero.prestige] || (n => `${n} virou ${hero.prestige}.`))(hero.name));
}

function nearestNode(actor) {
  return PATH_NODES.reduce((best, node, index) =>
    Math.hypot(node.x - actor.x, node.y - actor.y) < Math.hypot(PATH_NODES[best].x - actor.x, PATH_NODES[best].y - actor.y) ? index : best, 0);
}

function shortestPath(start, finish) {
  const queue = [start];
  const previous = new Map([[start, null]]);
  for (let head = 0; head < queue.length && !previous.has(finish); head++) {
    PATH_NODES[queue[head]].links.forEach(next => {
      if (!previous.has(next)) { previous.set(next, queue[head]); queue.push(next); }
    });
  }
  if (!previous.has(finish)) return [];
  const path = [];
  for (let node = finish; node !== null; node = previous.get(node)) path.unshift(node);
  return path;
}

// Menor rota caminhável até um ponto: mede os metros das trilhas e compara todas as entradas possíveis.
function roadPathsFrom(actor) {
  const count = PATH_NODES.length;
  const distances = PATH_NODES.map(node => Math.hypot(node.x - actor.x, node.y - actor.y));
  const previous = Array(count).fill(null), visited = new Set();
  while (visited.size < count) {
    let current = -1;
    for (let i = 0; i < count; i++) if (!visited.has(i) && (current < 0 || distances[i] < distances[current])) current = i;
    if (current < 0 || !Number.isFinite(distances[current])) break;
    visited.add(current);
    PATH_NODES[current].links.forEach(next => {
      const edge = Math.hypot(PATH_NODES[next].x - PATH_NODES[current].x, PATH_NODES[next].y - PATH_NODES[current].y);
      if (distances[current] + edge < distances[next]) { distances[next] = distances[current] + edge; previous[next] = current; }
    });
  }
  return { distances, previous };
}
function pathFromTree(tree, finish) {
  const path = [];
  for (let node = finish; node !== null; node = tree.previous[node]) path.unshift(node);
  return path;
}
function roadDistanceTo(actor, point) {
  const tree = roadPathsFrom(actor);
  return Math.min(...PATH_NODES.map((node, i) => tree.distances[i] + Math.hypot(node.x - point.x, node.y - point.y)));
}
function taskDistance(actor, place) {
  const fromMap = actor.mapId || 'city', fromSet = actor.mapSet || 1;
  const toMap = buildingMapId(place), toSet = place.mapSet || fromSet;
  const point = { x: place.x, y: Math.min(198, place.y + 1.2) };
  if (fromMap === toMap && fromSet === toSet) return roadDistanceTo(actor, point);
  const gate = PATH_NODES[18], exit = WILDS_ROAD_EXIT[toSet === 2 ? 2 : 1];
  if (fromMap === 'city') return roadDistanceTo(actor, gate) + 8 + Math.hypot(exit.x - point.x, exit.y - point.y);
  return Math.hypot(actor.x - exit.x, actor.y - exit.y) + 8 + roadDistanceTo(gate, point);
}
function routeToPoint(actor, point) {
  const tree = roadPathsFrom(actor);
  let finish = 0, best = Infinity;
  PATH_NODES.forEach((node, i) => {
    const cost = tree.distances[i] + Math.hypot(node.x - point.x, node.y - point.y);
    if (cost < best) { best = cost; finish = i; }
  });
  actor.route = pathFromTree(tree, finish);
  actor.goal = finish;
  actor.finalPoint = { x: point.x, y: Math.min(198, point.y + 1.2) };
  return best;
}

function routeActor(actor, finish) {
  const path = shortestPath(nearestNode(actor), finish);
  if (path.length && Math.hypot(PATH_NODES[path[0]].x - actor.x, PATH_NODES[path[0]].y - actor.y) < 0.5) path.shift();
  actor.route = path;
  actor.goal = finish;
}

function homeOf(actor) {
  return actor && actor.homeSite != null ? state.buildings.find(building => building.site === actor.homeSite) || null : null;
}

function workplaceOf(actor) {
  if (actor?.workplace) return actor.workplace;
  if (actor?.profession === 'fisher') {
    const current = FISHING_SPOTS.find(item => item.site === actor.jobSite);
    const crew = spot => state.workers.filter(worker => worker.alive && worker !== actor && worker.profession === 'fisher' && worker.jobSite === spot.site).length;
    const spot = current || [...FISHING_SPOTS].sort((a, b) => crew(a) - crew(b))[0];
    if (actor) actor.jobSite = spot.site;
    return spot;
  }
  if (actor?.profession === 'hunter') { actor.jobSite = null; return { type: 'hunting', name: 'nas Wilds', mapId: 'wilds', mapSet: WILDLIFE_MAP_SET, x: 48, y: 98 }; }
  // Lenhador entra sozinho numa Cabana do Lenhador com vaga.
  if (actor?.profession === 'woodcutter') {
    const camp = state.buildings.find(building => building.type === 'lumber' && isDone(building) && lumberCrew(building).length < 3);
    if (camp) { actor.workplace = camp; actor.jobSite = null; return camp; }
  }
  if (actor?.profession === 'miner') {
    const pit = state.buildings.find(building => isPit(building) && isDone(building) && lumberCrew(building).length < workerLimitForPit(building));
    if (pit) { actor.workplace = pit; actor.jobSite = null; return pit; }
  }
  const all = state.buildings.filter(building => PRODUCER_TYPES.includes(building.type) && isDone(building));
  if (!all.length) return null;
  // Com profissão, só os lugares dela; se não houver nenhum, faz o que estiver disponível.
  const own = all.filter(building => PROFESSIONS[actor?.profession]?.types.includes(building.type));
  const places = own.length ? own : all;
  // Posto manual continua fixo. Generalistas sem posto escolhem o trabalho caminhável mais próximo;
  // uma pequena penalidade por equipe evita concentrar todos no mesmo prédio quando há opções perto.
  const current = actor && places.find(place => place.site === actor.jobSite);
  if (current) return current;
  const crew = site => state.workers.filter(worker => worker.alive && worker !== actor && !worker.workplace && worker.jobSite === site).length;
  const cost = place => taskDistance(actor || { x: 42, y: 39 }, place) + crew(place.site) * 5;
  const best = places.reduce((best, place) => cost(place) < cost(best) ? place : best);
  if (actor) actor.jobSite = best.site;
  return best;
}

function workerDestination(actor) {
  const workplace = workplaceOf(actor);
  if (!workplace) return 5;
  return nearestNode(workplace);
}

// Cada personagem anda na própria faixa da trilha: um desvio fixo, tirado do id, para ninguém andar em cima do outro.
function laneOffset(actor) {
  if (!actor.lane) {
    let hash = 0;
    for (const ch of String(actor.id)) hash = (hash * 31 + ch.charCodeAt(0)) | 0;
    hash >>>= 0;
    const angle = (hash % 360) * Math.PI / 180, radius = 0.7 + (Math.floor(hash / 360) % 100) / 100 * 1.3;
    actor.lane = { x: Math.round(Math.cos(angle) * radius * 100) / 100, y: Math.round(Math.sin(angle) * radius * 100) / 100 };
  }
  return actor.lane;
}

// Camponês sem trabalho passeia pela vila: escolhe um ponto da trilha que nenhum outro ocioso escolheu
// e para um pouco fora dela. Ficam longe do covil e da estrada velha.
const IDLE_NODES = [1, 2, 3, 4, 5, 6, 10, 12];
function idleSpot(actor) {
  const taken = state.workers.filter(other => other !== actor && other.alive && other.idleNode != null).map(other => other.idleNode);
  const free = IDLE_NODES.filter(node => !taken.includes(node) && node !== actor.idleNode);
  const node = pick(free.length ? free : IDLE_NODES.filter(node => node !== actor.idleNode));
  const base = PATH_NODES[node], angle = Math.random() * Math.PI * 2, radius = 2 + Math.random() * 3;
  actor.idleNode = node;
  return { node, point: { x: Math.max(2, Math.min(98, base.x + Math.cos(angle) * radius)), y: Math.min(198, base.y + Math.sin(angle) * radius) } };
}

function nextActorGoal(actor) {
  if (actor.className === 'Peasant') {
    if (tavernPause(actor)) return;
    if (actor.profession === 'fisher') { fishingRoutine(actor, workplaceOf(actor)); return; }
    if (actor.profession === 'hunter') { hunterRoutine(actor); return; }
    if (actor.buildSite != null) {
      const site = buildSiteOf(actor);
      if (site) {
        if (travelToMap(actor, site.mapId || 'city', site.mapSet || actor.mapSet || 1)) {
          actor.headingToWork = false;
          actor.task = `Indo construir ${buildingName(site)}`;
          return;
        }
        const spot = buildSpot(site);
        actor.headingToWork = false;
        actor.task = `${site.underConstruction ? 'Construindo' : 'Consertando'} ${buildingName(site)}`;
        if (Math.hypot(actor.x - spot.x, actor.y - spot.y) < 1.2) { actor.wait = 1.5; actor.actionTime = 1.5; actor.actionClock = 0; return; }
        if (buildingMapId(site) === 'wilds') { actor.route = []; actor.finalPoint = spot; }
        else routeToPoint(actor, spot);
        return;
      }
      actor.buildSite = null;
    }
    const job = workplaceOf(actor);
    if (job && job.type === 'bakery' && isDone(job)) { bakeRoutine(actor, job); return; }
    if (job && job.type === 'butcher' && isDone(job)) { butcherRoutine(actor, job); return; }
    if (job && job.type === 'emberLab' && isDone(job)) { labRoutine(actor, job); return; }
    if (job && isPit(job) && isDone(job)) { pitRoutine(actor, job); return; }
    if (job && ['farm', 'mill'].includes(job.type) && isDone(job)) { haulRoutine(actor, job); return; }
    const home = homeOf(actor) || (buildingMapId(job) === 'wilds' ? MANOR() : null);
    const workplace = actor.workTrip ? null : workplaceOf(actor);
    if (!workplace && !job && !actor.workTrip) {
      const spot = idleSpot(actor);
      actor.task = 'À toa, sem fazenda pra trabalhar';
      routeToPoint(actor, spot.point);
      actor.headingToWork = false;
      actor.workTrip = true;
      return;
    }
    if (workplace || job) actor.idleNode = null;
    actor.task = workplace ? (workplace.type === 'lumber' ? 'Cortando lenha na Cabana do Lenhador' : isPit(workplace) ? `Tirando ${PIT_GOODS[workplace.type].name} ${workPlaceName(workplace)}` : `Indo trabalhar ${workPlaceName(workplace)}`) : home ? 'Voltando pra casa' : job ? 'Voltando pro casarão' : 'À toa, sem fazenda pra trabalhar';
    const target = actor.workTrip ? home : workplace;
    if (target && travelToMap(actor, target.mapId || 'city', target.mapSet || actor.mapSet || 1)) {
      actor.headingToWork = !!workplace && !actor.workTrip;
      actor.task = actor.workTrip ? 'Voltando para casa' : `Indo trabalhar ${workPlaceName(workplace)}`;
      return;
    }
    if (target && buildingMapId(target) === 'wilds') { actor.route = []; actor.finalPoint = { x: target.x, y: Math.min(198, target.y + 1.2) }; }
    else if (target) routeToPoint(actor, target);
    else actor.finalPoint = null;
    actor.headingToWork = !!workplace;
    actor.workTrip = !actor.workTrip;
    return;
  }
  if (actor.remains) { carryRemainsStep(actor); return; }
  const current = actor.bountyId ? (state.bounties || []).find(bounty => bounty.id === actor.bountyId && String(bounty.takenBy) === String(actor.id)) : null;
  if (current) { bountyStep(actor, current); return; }
  actor.bountyId = null;
  if (remainsPileStep(actor)) return;
  if (['Archer', 'Ranger', 'Assassin'].includes(classOf(actor))) {
    const liveWildlife = (state.bandits || []).some(item => item.alive && item.className === 'Wildlife' && (item.mapSet || 1) === WILDLIFE_MAP_SET);
    if (liveWildlife && ((actor.mapId || 'city') !== 'wilds' || (actor.mapSet || 1) !== WILDLIFE_MAP_SET)) { actor.task = 'Caçando nas Wilds'; travelToMap(actor, 'wilds', WILDLIFE_MAP_SET); return; }
  }
  if ((actor.mapId || 'city') !== 'city') { actor.task = 'Voltando dos Wilds'; travelToMap(actor, 'city', actor.mapSet || 1); return; }
  if (tavernPause(actor)) return;
  const offer = considerBounties(actor);
  if (offer) { bountyStep(actor, offer); return; }
  const patrol = PATROLS[actor.className] || PATROLS.other;
  let destination = patrol[(actor.patrolIndex || 0) % patrol.length];
  actor.patrolIndex = (actor.patrolIndex || 0) + 1;
  if (destination === nearestNode(actor)) {
    destination = patrol[actor.patrolIndex % patrol.length];
    actor.patrolIndex++;
  }
  routeActor(actor, destination);
}

let lastActorFrame = 0;
let actorMovementErrorReported = false;
function reportActorMovementError(error) {
  if (actorMovementErrorReported) return;
  actorMovementErrorReported = true;
  console.error('Falha isolada no ciclo de movimento dos personagens:', error);
  const line = $('#event-line');
  if (line) line.textContent = 'Um personagem teve um erro de movimento; os camponeses seguem por uma rota de recuperação.';
}
function animateActors(now) {
  if (!game.running) { lastActorFrame = now; return; }
  const delta = Math.min(0.05, lastActorFrame ? (now - lastActorFrame) / 1000 : 0) * state.speed;
  lastActorFrame = now;
  const reducedMotion = window.matchMedia?.('(prefers-reduced-motion: reduce)')?.matches ?? false;
  [...state.heroes, ...state.workers].filter(actor => actor.alive).forEach(actor => {
    try {
    if (actor.className !== 'Peasant' && window.Monarch?.chargeStep(actor, delta, reducedMotion)) return;
    if (actor.className === 'Peasant' ? (peasantHuntStep(actor, delta, reducedMotion) || peasantDefenseStep(actor, delta, reducedMotion)) : heroCombatStep(actor, delta, reducedMotion)) return;
    const construction = actor.className === 'Peasant' && actor.buildSite != null
      ? buildSiteOf(actor)
      : null;
    if (construction) {
      const spot = buildSpot(construction);
      if (Math.hypot(actor.x - spot.x, actor.y - spot.y) < 1.5) {
        actor.actionClock = (actor.action === 'build' ? actor.actionClock || 0 : 0) + delta;
        actor.action = 'build';
        actor.frame = reducedMotion && actor.className !== 'Peasant' ? 0 : Math.floor(actor.actionClock * 5) % 4;
        return;
      }
    }
    if (actor.wait > 0) {
      actor.wait = Math.max(0, actor.wait - delta);
      if (guildHeroArt(actor) && !(actor.actionTime > 0)) { animateIdle(actor, delta, reducedMotion); return; }
      actor.action = actor.actionTime > 0 ? (actor.className === 'Peasant' ? (actor.pickupAnim ? 'collect' : 'work') : 'action') : 'walk';
      if (actor.actionTime > 0) actor.actionTime = Math.max(0, actor.actionTime - delta);
      if (actor.actionTime <= 0) actor.pickupAnim = false;
      actor.actionClock = (actor.actionClock || 0) + delta;
      actor.frame = ['action', 'collect', 'work'].includes(actor.action) && (!reducedMotion || actor.className === 'Peasant') ? Math.floor(actor.actionClock * 5) % 4 : 0;
      return;
    }
    if (!actor.route) actor.route = [];
    if (!actor.route.length && !actor.finalPoint) nextActorGoal(actor);
    let remaining = (actor.className === 'Peasant' ? 3.1 : 4.2) * (1 + gearBonus(actor, 'speed')) * delta * (window.Monarch?.moveMul(actor) ?? 1);
    let moved = false;
    while (remaining > 0 && (actor.route.length || actor.finalPoint)) {
      const onTrail = actor.route.length > 0;
      const lane = onTrail ? laneOffset(actor) : null;
      const node = onTrail ? { x: PATH_NODES[actor.route[0]].x + lane.x, y: Math.min(198, PATH_NODES[actor.route[0]].y + lane.y) } : actor.finalPoint;
      const dx = node.x - actor.x, dy = node.y - actor.y;
      const distance = Math.hypot(dx, dy);
      if (distance < 0.001) { if (onTrail) actor.route.shift(); else actor.finalPoint = null; continue; }
      actor.facing = dy < -0.2 ? 'back' : dy > 0.2 ? 'front' : (actor.facing || 'front');
      if (Math.abs(dx) > 0.2) actor.side = dx < 0 ? 'left' : 'right';
      const step = Math.min(distance, remaining);
      actor.x += dx / distance * step;
      actor.y += dy / distance * step;
      remaining -= step;
      moved = true;
      if (step >= distance - 0.001) {
        actor.x = node.x; actor.y = node.y;
        if (onTrail) actor.route.shift(); else actor.finalPoint = null;
        if (!actor.route.length && !actor.finalPoint) {
          // Quem trabalha tem ritmo fixo; quem só anda para um tempo diferente a cada vez, para ninguém andar em fila sincronizada.
          actor.wait = actor.className === 'Peasant' ? (actor.headingToWork ? 2.3 / taskSpeed(actor, workplaceOf(actor)) : 1.8 + Math.random() * 3) : 1.2;
          if (actor.className === 'Peasant' && actor.headingToWork && workplaceOf(actor)?.type === 'lumber') gainTrade(actor, 'lumber');
          if (actor.className === 'Peasant' && actor.headingToWork && isPit(workplaceOf(actor))) gainTrade(actor, workplaceOf(actor).type);
          if (actor.className === 'Peasant' && actor.headingToWork) { actor.actionTime = actor.wait; actor.actionClock = 0; gainActorXp(actor, 13); actor.headingToWork = false; }
          actor.goal = null;
        }
      }
    }
    if (!moved && guildHeroArt(actor)) animateIdle(actor, delta, reducedMotion);
    else animateWalk(actor, moved, delta, reducedMotion);
    } catch (error) {
      actor.route = [];
      actor.wait = 0;
      actor.goal = null;
      if (actor.className === 'Peasant') {
        // Keep a failed peasant AI from freezing the visible villager in place.
        // Walk to the next connected road node; normal job logic resumes there.
        const node = PATH_NODES[nearestNode(actor)];
        const next = node?.links?.[0];
        actor.finalPoint = next == null ? null : { x: PATH_NODES[next].x, y: PATH_NODES[next].y };
      } else {
        actor.finalPoint = null;
      }
      reportActorMovementError(error);
    }
  });
  nekomataDeaths.forEach(actor => { actor.nekoDeathClock += delta; });
  nekomataDeaths = nekomataDeaths.filter(actor => actor.nekoDeathClock < NEKOMATA_FRAME_MS.death * 36 / 1000);
  (state.bandits || []).filter(bandit => bandit.alive).forEach(bandit => {
    if (bandit.kind === 'nekomata') {
      bandit.nekoIdleClock = (bandit.nekoIdleClock || 0) + delta;
      if (bandit.nekoHitClock != null) {
        bandit.nekoHitClock += delta;
        if (bandit.nekoHitClock >= NEKOMATA_FRAME_MS.hit * 36 / 1000) bandit.nekoHitClock = null;
      }
    }
    // Atordoado, derrubado, empurrado ou convertido pelo Monarca: quem decide o passo é ele.
    try {
      if (!isWildlife(bandit) && window.Monarch?.enemyStep(bandit, delta, reducedMotion)) return;
      if (isWildlife(bandit)) { wildlifeStep(bandit, delta, reducedMotion); return; }
      (bandit.className === 'Monster' ? monsterStep : banditStep)(bandit, delta, reducedMotion);
    } catch (error) {
      bandit.route = [];
      bandit.finalPoint = null;
      bandit.wait = 0;
      reportActorMovementError(error);
    }
  });
  try { window.Monarch?.step(delta, reducedMotion); } catch (error) { reportActorMovementError(error); }
  try { renderHeroes(); } catch (error) { reportActorMovementError(error); }
}

function tick() {
  if (!game.running) return;
  grantStartingStock(state);
  state.ticks++;
  state.playSeconds = (state.playSeconds || 0) + 1;
  if (state.playSeconds % 300 === 0 && Math.random() < 0.25) spawnRandomNekomata();
  if (options.autosave && state.playSeconds % (options.autosave * 60) === 0) writeSave(game.slot);
  const r = rates();
  feedWorkers(state.speed);
  if (!$('#inventory-layer').hidden && state.ticks % 5 === 0) renderManorInventory();
  progressConstruction(state.speed);
  state.grain = totalFood();
  if ((state.tab === 'food' || state.tab === 'market') && !document.querySelector('#panel-view button:hover')) renderPanel();
  if (state.tab === 'economy' && state.ticks % 10 === 0 && !document.querySelector('#panel-view button:hover, #panel-view details[open]')) renderPanel();
  state.timber += r.timber * state.speed;
  state.stone = (state.stone || 0) + r.stone * state.speed;
  advanceMines(state.speed);
  processMetalworks(state.speed);
  accrueTavern(state.speed);
  if (state.minutes % 60 < state.speed) { shopAtArmory(); processDemands(); processBounties(); }
  state.minutes += state.speed;
  workerArrival();
  heroArrival();
  if (Math.floor(state.minutes / 1440) > Math.floor((state.minutes - state.speed) / 1440)) {
    gainHeirXp(40); collectTaxes();
    if (Math.floor(state.minutes / 1440) % DAYS_PER_WEEK === 0) applyWeeklyMood();
    if (Math.floor(state.minutes / 1440) % DAYS_PER_YEAR === 0) newYear();
  }
  // Heróis ganham experiência só em combate (golpes e curas). Aqui chegam os bandos e todos se recuperam.
  combatTick(state.speed);
  const minuteOfDay = state.minutes % 1440;
  if (minuteOfDay >= 1200 && minuteOfDay - state.speed < 1200) say(NIGHT_MSG[Math.floor(state.minutes / 1440) % NIGHT_MSG.length]);
  updateHud();
}

// ==========================================================================
// Herdeiros: cinco casas de Mordavia, um herdeiro e uma herdeira em cada.
// ==========================================================================
const HOUSES = {
  vaelmont: { name: 'Casa Vaelmont', motto: 'O sangue é velho. A dívida, mais ainda.', colors: ['#6b1717', '#151313'], ink: '#e9cf9a', pattern: 'pale', letter: 'V',
    about: 'Nobreza antiga, castelo hipotecado e um retrato de família que ninguém tem coragem de tirar da parede.' },
  grenholt: { name: 'Casa Grenholt', motto: 'Quem uiva por último uiva sozinho.', colors: ['#6a7076', '#1e2328'], ink: '#f1e6cf', pattern: 'fess', letter: 'G',
    about: 'Caçadores de lobo das montanhas. Dizem que há lobo no sangue deles. Eles não negam.' },
  montalvo: { name: 'Casa Montalvo', motto: 'Tudo tem preço. Inclusive o luto.', colors: ['#21412a', '#b48c3c'], ink: '#f4e3b0', pattern: 'bend', letter: 'M',
    about: 'Mercadores de sal, de grão e de dívidas alheias. Ficaram ricos e faliram tantas vezes que perderam a conta.' },
  salazar: { name: 'Casa Salazar', motto: 'A chama recorda o sangue.', colors: ['#7a2d1d', '#c19a4a'], ink: '#f4e3b0', pattern: 'quarterly', letter: 'S',
    about: 'Uma linhagem ibérica antiga, moldada por séculos de encontros entre povos latinos, ibéricos e norte-africanos do Mediterrâneo.' },
  lucerna: { name: 'Casa Lucerna', motto: 'Enterramos todo mundo. Um dia, a gente também.', colors: ['#3d2549', '#d9cfb6'], ink: '#2a1a31', pattern: 'chevron', letter: 'L',
    about: 'Coveiros, carpideiras e padres de aluguel. Nenhuma família de Mordavia conhece tantos mortos pelo nome.' },
  kastrell: { name: 'Casa Kastrell', motto: 'Prontos pra guerra. A guerra é que se atrasou.', colors: ['#1e2f50', '#8d939d'], ink: '#efe6d2', pattern: 'quarterly', letter: 'K',
    about: 'Família de soldados que treina para uma guerra que nunca chega. Os duelos entre primos ajudam a passar o tempo.' },
};

const HEIRS = [
  { id: 'radu', name: 'Lucien Vaelmont', house: 'vaelmont', gender: 'm', age: 19, tie: 'Sobrinho do falecido.', about: 'Vendeu o cavalo pra pagar o funeral do cavalo. Ainda acha que foi um bom negócio.' },
  { id: 'costel', name: 'Bran Grenholt', house: 'grenholt', gender: 'm', age: 20, tie: 'Primo, por um casamento que ninguém aprovou.', about: 'Caça lobos desde os doze. Os lobos caçam ele de volta. Empate técnico.' },
  { id: 'mircea', name: 'Teodoro Montalvo', house: 'montalvo', gender: 'm', age: 22, tie: 'Credor do falecido. Também é parente.', about: 'Sabe o preço de tudo. O valor, ele cobra à parte.' },
  { id: 'toma', name: 'Inácio Lucerna', house: 'lucerna', gender: 'm', age: 21, tie: 'Enterrou o falecido. E o pai dele.', about: 'Coveiro de terceira geração. Os clientes nunca reclamam.' },
  { id: 'petru', name: 'Darian Kastrell', house: 'kastrell', gender: 'm', age: 18, tie: 'Afilhado do falecido.', about: 'Treinado pra guerra desde criança. A guerra nunca veio buscar ele.' },
  { id: 'ileana', name: 'Odette Vaelmont', house: 'vaelmont', gender: 'f', age: 17, tie: 'Sobrinha do falecido.', about: 'Herdou o orgulho da família. Só o orgulho. O resto foi penhorado.' },
  { id: 'doina', name: 'Wren Grenholt', house: 'grenholt', gender: 'f', age: 16, tie: 'Prima distante. A uma distância segura.', about: 'Uiva na lua cheia. Jura que é canto.' },
  { id: 'viorica', name: 'Lavinia Montalvo', house: 'montalvo', gender: 'f', age: 18, tie: 'Viúva do sobrinho do falecido.', about: 'Já faliu três vezes. Na prática, é uma especialista.' },
  { id: 'zoia', name: 'Amara Lucerna', house: 'lucerna', gender: 'f', age: 17, tie: 'Parente, segundo os mortos.', about: 'Conversa com os mortos. Eles respondem mais que os vivos.' },
  { id: 'anca', name: 'Rowena Kastrell', house: 'kastrell', gender: 'f', age: 16, tie: 'Neta do irmão do falecido.', about: 'Duelou com metade da corte. A outra metade foi embora do reino.' },
  { id: 'isabel-salazar', name: 'Isabel Salazar', house: 'salazar', gender: 'f', age: 21, tie: 'Descendente do mesmo ramo antigo da família Salazar.', about: 'Aprendeu que a elegância também é uma armadura — e que uma família sobrevive melhor quando sabe mudar sem esquecer de onde veio.' },
  { id: 'afonso-salazar', name: 'Afonso Salazar', house: 'salazar', gender: 'm', age: 21, tie: 'Herdeiro de um ramo antigo da Casa Salazar.', about: 'Criado entre os salões da corte, aprendeu cedo que nome, presença e lealdade também decidem disputas.' },
  // Os nobres das casas também podem herdar. Os retratos deles ficam em noble-portraits-v01.
  { id: 'anselm', name: 'Anselm Grenholt', house: 'grenholt', gender: 'm', age: 21, portrait: 'noble-portraits-v01/grenholt', tie: 'Caçador-Mor das Montanhas. Primo do falecido, pelo lado dos lobos.', about: 'Os cães dele comem melhor que os camponeses. Ele acha justo: os cães trabalham mais.' },
  { id: 'ottokar', name: 'Ottokar Lucerna', house: 'lucerna', gender: 'm', age: 22, portrait: 'noble-portraits-v01/lucerna', tie: 'Padre-Conde. Rezou a missa do falecido e leu o testamento na mesma tarde.', about: 'Mede a porta de quem visita. Pra saber se o caixão passa.' },
  { id: 'severina', name: 'Severina Montalvo', house: 'montalvo', gender: 'f', age: 20, portrait: 'noble-portraits-v01/montalvo', tie: 'Credora do falecido. Veio cobrar e saiu com a herança.', about: 'Anota tudo num livro de capa vermelha. Seu nome já está lá, sublinhado duas vezes.' },
  { id: 'ilka', name: 'Ilka Kastrell', house: 'kastrell', gender: 'f', age: 19, portrait: 'noble-portraits-v01/kastrell', tie: 'Baronesa e marechal de uma guerra que nunca veio.', about: 'Abastece um exército há anos pra uma guerra que não chega. Desta vez, jura ela, vem.' },
  { id: 'brunhilde', name: 'Brunhilde Vaelmont', house: 'vaelmont', gender: 'f', age: 18, portrait: 'noble-portraits-v01/vaelmont', tie: 'Tia-avó do falecido. A idade verdadeira dela é segredo de família.', about: 'Ninguém sobrevive ao chá dela por muito tempo. Ela jura que é a receita da família.' },
  { id: 'godefroy-vaelmont', name: 'Godefroy Vaelmont', house: 'vaelmont', gender: 'm', age: 20, tie: 'Herdeiro de um ramo cortesão da Casa Vaelmont.', about: 'Aprendeu que um brasão abre portas; a dívida da família costuma fechá-las.' },
  { id: 'henri-kastrell', name: 'Henri Kastrell', house: 'kastrell', gender: 'm', age: 21, tie: 'Segundo filho de uma linhagem nobre da Casa Kastrell.', about: 'Prefere tratados a torneios, embora vista ambos com o mesmo cuidado.' },
  { id: 'eleonore-grenholt', name: 'Éléonore Grenholt', house: 'grenholt', gender: 'f', age: 20, tie: 'Herdeira de um ramo antigo da Casa Grenholt.', about: 'Combina graça cortesã e determinação; até os rivais lembram de sua presença.' },
  { id: 'adelaide-vaelmont', name: 'Adélaïde Vaelmont', house: 'vaelmont', gender: 'f', age: 20, tie: 'Herdeira de um ramo cortesão da Casa Vaelmont.', about: 'Conhece os códigos da corte e sabe quando um sorriso vale mais do que um brasão.' },
  { id: 'gautier-lucerna', name: 'Gautier Lucerna', house: 'lucerna', gender: 'm', age: 22, tie: 'Herdeiro de um ramo menor da Casa Lucerna.', about: 'Seu nariz já foi assunto em três banquetes; os contratos que lê, esses ninguém esquece.' },
];
const heirById = id => HEIRS.find(heir => heir.id === id) || null;

// Brasão da casa, desenhado em SVG (escudo com divisão heráldica e a inicial da casa).
function crestSVG(houseId, size = 64) {
  const h = HOUSES[houseId];
  if (!h) return '';
  const [a, b] = h.colors;
  const shield = 'M4 4H60V34C60 54 46 66 32 72C18 66 4 54 4 34Z';
  const fields = {
    pale: `<rect x="0" y="0" width="32" height="76" fill="${a}"/><rect x="32" y="0" width="32" height="76" fill="${b}"/>`,
    fess: `<rect x="0" y="0" width="64" height="30" fill="${a}"/><rect x="0" y="30" width="64" height="46" fill="${b}"/>`,
    bend: `<rect x="0" y="0" width="64" height="76" fill="${a}"/><path d="M-6 8L8 -6L74 60L60 74Z" fill="${b}"/>`,
    chevron: `<rect x="0" y="0" width="64" height="76" fill="${a}"/><path d="M4 50L32 24L60 50V62L32 36L4 62Z" fill="${b}"/>`,
    quarterly: `<rect x="0" y="0" width="32" height="34" fill="${a}"/><rect x="32" y="0" width="32" height="34" fill="${b}"/><rect x="0" y="34" width="32" height="42" fill="${b}"/><rect x="32" y="34" width="32" height="42" fill="${a}"/>`,
  };
  const clip = `crest-${houseId}-${Math.random().toString(36).slice(2, 7)}`;
  return `<svg class="crest" width="${size}" height="${Math.round(size * 76 / 64)}" viewBox="0 0 64 76" aria-hidden="true">
    <defs><clipPath id="${clip}"><path d="${shield}"/></clipPath></defs>
    <g clip-path="url(#${clip})">${fields[h.pattern]}</g>
    <path d="${shield}" fill="none" stroke="#c9a464" stroke-width="2.4"/>
    <path d="${shield}" fill="none" stroke="#000" stroke-opacity=".45" stroke-width="1" transform="translate(0 1)"/>
    <text x="32" y="44" text-anchor="middle" font-family="'Pirata One', Georgia, serif" font-size="30" fill="${h.ink}" stroke="#000" stroke-opacity=".35" stroke-width=".8">${h.letter}</text>
  </svg>`;
}

// Nível do herdeiro (1 a 30) e experiência.
const HEIR_MAX_LEVEL = 30;
// Mesma tabela do Ember Ashes (expToLevel): experiência para passar do nível atual ao próximo.
const xpToNext = level => { const c = Math.max(1, Math.min(HEIR_MAX_LEVEL - 1, level)); return c >= 28 ? 600 : c >= 23 ? 500 : c >= 18 ? 400 : c >= 13 ? 300 : c >= 8 ? 225 : c >= 3 ? 150 : 100; };
const LEVEL_MSG = [
  n => `${n} subiu de nível. Ninguém notou.`,
  n => `${n} subiu de nível. A família mandou parabéns e uma conta.`,
  n => `${n} subiu de nível. Os credores também.`,
  n => `${n} subiu de nível. A fazenda continua do mesmo jeito.`,
];
function gainHeirXp(amount) {
  if (!state.heir || state.heirLevel >= HEIR_MAX_LEVEL) return;
  state.heirXp += amount;
  let gainedLevels = 0;
  while (state.heirLevel < HEIR_MAX_LEVEL && state.heirXp >= xpToNext(state.heirLevel)) {
    state.heirXp -= xpToNext(state.heirLevel);
    state.heirLevel++;
    state.heirAttributePoints = (state.heirAttributePoints || 0) + 6;
    gainedLevels++;
  }
  if (state.heirLevel >= HEIR_MAX_LEVEL) state.heirXp = 0;
  const heir = heirById(state.heir);
  if (gainedLevels && heir) say(`${pick(LEVEL_MSG)(heir.name.split(' ')[0])} Nível ${state.heirLevel}: +${gainedLevels * 6} pontos para distribuir na ficha.`);
}

// Equipamento básico do herdeiro, com ícones provisórios em pixel art (12 x 12).
const PIXEL_COLORS = {
  k: '#1a1410', g: '#d6a94a', G: '#f2d27a', d: '#8a6420', w: '#7a4b26', W: '#a8703c', r: '#7c1f1a', R: '#b8382c',
  b: '#2f5f9a', B: '#7fb2e6', s: '#9aa0a6', S: '#d8dde2', c: '#b0642e', C: '#e09056', o: '#d8cfb6', O: '#a89c80', x: '#3c9a5a',
};
const RING = ['............', '.....kk.....', '....kxxk....', '....kxxk....', '...kcCCck...', '..kcckkcck..', '.kck....kck.', '.kck....kck.', '.kck....kck.', '..kcckkcck..', '...kcccck...', '....kkkk....'];
const PIXEL_ART = {
  scepter: ['.........kk.', '........kRRk', '........kRRk', '.......kgkk.', '......kgk...', '.....kwk....', '....kwk.....', '...kwk......', '..kwk.......', '.kgk........', 'kgk.........', '.k..........'],
  orb: ['.....kk.....', '....kggk....', '.....kk.....', '...kkkkkk...', '..kbBbbbbk..', '.kbBbbbbbbk.', '.kggggggggk.', '.kbbbbbbbbk.', '.kbbbbbbbbk.', '..kbbbbbbk..', '...kkkkkk...', '............'],
  mantle: ['....kkkk....', '...kgRRgk...', '..kRRggRRk..', '..kRRRRRRk..', '.kRRrRRrRRk.', '.kRrRRRRrRk.', '.kRrRRRRrRk.', 'kRrRRRRRRrRk', 'kRrRRRRRRrRk', 'krRRRRRRRRrk', 'krrRrRRrRrrk', 'kkkkkkkkkkkk'],
  crown: ['............', '.k...kk...k.', 'kSk.kSSk.kSk', 'kSk.kSSk.kSk', 'kSSkSSSSkSSk', 'kSSSSSSSSSSk', 'ksRssbbssRsk', 'kSSSSSSSSSSk', 'kssssssssssk', '.kkkkkkkkkk.', '............', '............'],
  medallion: ['.k........k.', '.kk......kk.', '..kk....kk..', '...kk..kk...', '....kkkk....', '...kggggk...', '..kgGggggk..', '..kgggrggk..', '..kggrrggk..', '..kgggggdk..', '...kddddk...', '....kkkk....'],
  ringCopper: RING,
  ringBone: RING.map(row => row.replace(/c/g, 'o').replace(/C/g, 'O').replace(/x/g, 'R')),
};
PIXEL_ART.coin = ['....kkkk....', '..kkGGGgkk..', '.kGGgggggdk.', '.kGgkkkkgdk.', 'kGgkggggkgdk', 'kGgkgGggkgdk', 'kggkggggkgdk', 'kggkggggkddk', '.kgdkkkkddk.', '.kddddddddk.', '..kkddddkk..', '....kkkk....'];
PIXEL_ART.logs = ['............', '............', '.kkkkkkkkkk.', 'kwWwwWwwwkWk', 'kwwwwwwWwkdk', '.kkkkkkkkkk.', '.kkkkkkkkkk.', 'kwwWwwwwWkWk', 'kwwwwWwwwkdk', '.kkkkkkkkkk.', '............', '............'];

// Custo com ícones: moeda para ouro, lenha para madeira.
function costHTML(gold, wood, stone = 0) {
  const ico = name => pixelIcon(name, 14).replace('class="pixel-icon"', 'class="pixel-icon cost-ico"');
  const stoneIcon = '<img class="pixel-icon cost-ico" src="public/art/icon-stone-photo-v01.png" width="14" height="14" alt="" aria-hidden="true">';
  return `<span class="cost-pair"><span class="cost-item" title="${gold} de ouro">${ico('coin')}${gold}</span><span class="cost-item" title="${wood} de madeira">${ico('logs')}${wood}</span>${stone ? `<span class="cost-item" title="${stone} de Pedra de Construção">${stoneIcon}${stone}</span>` : ''}</span>`;
}

function pixelIcon(name, size = 48) {
  if (!PIXEL_ART[name]) return '';
  const asset = { ringCopper: 'ring-copper', ringBone: 'ring-bone' }[name] || name;
  return `<img class="pixel-icon" src="public/art/icon-${asset}-photo-v01.png" width="${size}" height="${size}" alt="" aria-hidden="true" decoding="async">`;
}

const EQUIP_SLOTS = [
  { slot: 'scepter', label: 'Cetro' }, { slot: 'orb', label: 'Globo' }, { slot: 'mantle', label: 'Manto' }, { slot: 'crown', label: 'Coroa' },
  { slot: 'medallion', label: 'Medalhão' }, { slot: 'ring1', label: 'Anel I' }, { slot: 'ring2', label: 'Anel II' },
];
const ITEMS = {
  pineScepter: { name: 'Cetro de Pinho', slot: 'scepter', tier: 1, price: 0, bonus: {}, icon: 'scepter', about: 'Pintado de dourado. A tinta descasca quando você aponta pra alguém.' },
  crackedOrb: { name: 'Globo Rachado', slot: 'orb', tier: 1, price: 0, bonus: {}, icon: 'orb', about: 'Representa o mundo. A rachadura também.' },
  wornMantle: { name: 'Manto Puído', slot: 'mantle', tier: 1, price: 0, bonus: {}, icon: 'mantle', about: 'Era vermelho real. Hoje é vermelho tanto faz.' },
  tinCrown: { name: 'Coroa de Lata', slot: 'crown', tier: 1, price: 0, bonus: {}, icon: 'crown', about: 'Brilha bem de longe. De perto, faz barulho.' },
  familyMedallion: { name: 'Medalhão da Família', slot: 'medallion', tier: 1, price: 0, bonus: {}, icon: 'medallion', about: 'Tem o brasão da casa na frente e o nome de um credor atrás.' },
  copperRing: { name: 'Anel de Cobre', slot: 'ring', tier: 1, price: 0, bonus: {}, icon: 'ringCopper', about: 'Deixa o dedo verde. Nobreza tem seu preço.' },
  boneRing: { name: 'Anel de Osso', slot: 'ring', tier: 1, price: 0, bonus: {}, icon: 'ringBone', about: 'Ninguém sabe de quem é o osso. Melhor assim.' },
};
function BASIC_EQUIPMENT() { return { scepter: 'pineScepter', orb: 'crackedOrb', mantle: 'wornMantle', crown: 'tinCrown', medallion: 'familyMedallion', ring1: 'copperRing', ring2: 'boneRing' }; }

// Equipamento do monarca: 10 tiers por peça. O tier 1 é o que veio com a herança; os outros
// se compram no Arsenal com o ouro do cofre, e cada nível do Arsenal libera até o seu tierCap.
// Bônus: atributo principal +2 por tier acima do 1, secundário +1 (Mana vale o triplo).
const MONARCH_TIER_PRICE = [0, 25, 50, 90, 140, 210, 300, 420, 580, 800];
const MONARCH_LINES = {
  scepter: { icon: 'scepter', main: 'MAG', side: 'CHA', items: [
    ['ashScepter', 'Cetro de Freixo', 'Não é ouro, mas aponta igual.'],
    ['oldBronzeScepter', 'Cetro de Bronze Velho', 'Herdado de um prefeito que sumiu.'],
    ['villageJudgeScepter', 'Cetro do Juiz de Aldeia', 'Já condenou três galinhas e um primo.'],
    ['beatenSilverScepter', 'Cetro de Prata Batida', 'A prata é verdadeira. O brasão, discutível.'],
    ['mourningValeScepter', 'Cetro do Vale do Luto', 'Os camponeses abaixam a cabeça. Por hábito.'],
    ['onyxScepter', 'Cetro de Ônix', 'Pesa na mão e na consciência.'],
    ['baronScepter', 'Cetro do Barão', 'Um barão morreu segurando. Não soltou fácil.'],
    ['ashenScepter', 'Cetro das Cinzas', 'Esquenta quando alguém mente. Vive quente.'],
    ['forgottenKingScepter', 'Cetro do Rei Esquecido', 'Mordavia já teve um rei. Ninguém lembra o nome, só o cetro.'],
  ] },
  orb: { icon: 'orb', main: 'INT', side: 'WIS', items: [
    ['blownGlassOrb', 'Globo de Vidro Soprado', 'Mostra o mundo. Embaçado, como ele é.'],
    ['brassOrb', 'Globo de Latão', 'Gira. Ninguém sabe pra quê.'],
    ['cartographerOrb', 'Globo do Cartógrafo', 'Tem o mapa da região. Com erros de propósito.'],
    ['silverOrb', 'Globo de Prata', 'Reflete o rosto do dono. Melhor não olhar muito.'],
    ['rockCrystalOrb', 'Globo de Cristal de Rocha', 'Prevê o tempo. Sempre chuva.'],
    ['astrologerOrb', 'Globo do Astrólogo', 'As estrelas dizem que você vai pagar caro por ele.'],
    ['blackAmberOrb', 'Globo de Âmbar Negro', 'Tem um inseto dentro. Ele pisca.'],
    ['livingAshOrb', 'Globo das Cinzas Vivas', 'A fumaça lá dentro forma nomes de devedores.'],
    ['wholeWorldOrb', 'Globo do Mundo Inteiro', 'Contém Mordavia inteira. Ela não sabe.'],
  ] },
  mantle: { icon: 'mantle', main: 'MP', side: 'CHA', items: [
    ['dyedWoolMantle', 'Manto de Lã Tingida', 'Vermelho de beterraba. Desbota na chuva.'],
    ['marketFeltMantle', 'Manto de Feltro do Mercado', 'Comprado de um mercador que jurou que era nobre.'],
    ['wornVelvetMantle', 'Manto de Veludo Gasto', 'Veludo de verdade. Gasto de verdade.'],
    ['foxCollarMantle', 'Manto com Gola de Raposa', 'A raposa ainda tem opinião.'],
    ['fakeErmineMantle', 'Manto de Arminho Falso', 'Coelho pintado. Ninguém tem coragem de dizer.'],
    ['councilMantle', 'Manto do Conselho', 'Usado em reuniões longas. Esconde o cochilo.'],
    ['trueErmineMantle', 'Manto de Arminho Verdadeiro', 'Agora sim. Os coelhos estão aliviados.'],
    ['goldThreadMantle', 'Manto Bordado a Ouro', 'Cada fio de ouro é um imposto de alguém.'],
    ['coronationMantle', 'Manto da Coroação', 'Feito para a coroação de um rei que não veio. Serve em você.'],
  ] },
  crown: { icon: 'crown', main: 'CHA', side: 'WIS', items: [
    ['copperDiadem', 'Diadema de Cobre', 'Fica verde na testa. Lembra de onde você veio.'],
    ['beatenIronCrown', 'Coroa de Ferro Batido', 'O ferreiro bateu com raiva. Dá pra ver.'],
    ['bronzeCrown', 'Coroa de Bronze', 'Brilha o bastante para ser roubada.'],
    ['fineSilverCrown', 'Coroa de Prata Fina', 'Fina demais para guerra. Ótima para jantar.'],
    ['garnetCrown', 'Coroa com Granadas', 'Pedras vermelhas. Dizem que são granadas.'],
    ['valeCrown', 'Coroa do Vale', 'O povo do Vale do Luto reconhece. E reclama.'],
    ['oldGoldCrown', 'Coroa de Ouro Velho', 'Ouro antigo, derretido de outras coroas menos sortudas.'],
    ['ashAndGoldCrown', 'Coroa de Cinzas e Ouro', 'Leve como cinza. Pesada como herança.'],
    ['mordaviaCrown', 'Coroa de Mordavia', 'A verdadeira. Ou a melhor cópia já feita.'],
  ] },
  medallion: { icon: 'medallion', main: 'WIS', side: 'MP', items: [
    ['pewterMedallion', 'Medalhão de Estanho', 'O brasão está ao contrário. Ninguém percebeu ainda.'],
    ['engravedCopperMedallion', 'Medalhão de Cobre Gravado', 'Gravado à mão. A mão tremia.'],
    ['guildMedallion', 'Medalhão da Guilda', 'Abre portas. Algumas.'],
    ['houseSilverMedallion', 'Medalhão de Prata da Casa', 'O brasão certo, desta vez.'],
    ['portraitMedallion', 'Medalhão com Retrato', 'Tem o retrato de uma tia. Ela ainda observa.'],
    ['oathMedallion', 'Medalhão do Juramento', 'Jurado diante de testemunhas. Elas morreram.'],
    ['baronGoldMedallion', 'Medalhão de Ouro do Barão', 'Prova que você é barão. Ou que roubou um.'],
    ['ashMedallion', 'Medalhão das Cinzas', 'Guarda as cinzas de um antepassado. Ele pesa.'],
    ['firstHeirMedallion', 'Medalhão do Primeiro Herdeiro', 'O primeiro dono fundou a casa. O segundo a perdeu.'],
  ] },
  ring: { icon: 'ringCopper', main: 'MAG', side: 'INT', items: [
    ['brassRing', 'Anel de Latão', 'Brilha como ouro por uma semana.'],
    ['carvedBoneRing', 'Anel de Osso Entalhado', 'Entalhado por um coveiro com tempo livre.', 'ringBone'],
    ['plainSilverRing', 'Anel de Prata Lisa', 'Liso. Sem brasão, sem dívida.'],
    ['signetRing', 'Anel de Sinete', 'Sela cartas. As respostas demoram.'],
    ['jadeRing', 'Anel de Jade', 'Jade de verdade, trazido de longe por alguém que não voltou.'],
    ['wolfBoneRing', 'Anel de Osso de Lobo', 'Lobo grande. Dedo pequeno.', 'ringBone'],
    ['rubyGoldRing', 'Anel de Ouro com Rubi', 'O rubi pisca no escuro. Não pergunte.'],
    ['ashRing', 'Anel das Cinzas', 'Morno o tempo todo. Como uma brasa que não apaga.', 'ringBone'],
    ['forgottenKingRing', 'Anel do Rei Esquecido', 'Faz par com o cetro. Os dois esperam alguém.'],
  ] },
};
Object.entries(MONARCH_LINES).forEach(([slot, line]) => line.items.forEach(([id, name, about, icon], i) => {
  const tier = i + 2, step = tier - 1, mana = key => (key === 'MP' ? 3 : 1);
  ITEMS[id] = { name, slot, tier, price: MONARCH_TIER_PRICE[tier - 1], icon: icon || line.icon, about,
    bonus: { [line.main]: 2 * step * mana(line.main), [line.side]: step * mana(line.side) } };
}));
const monarchEquipment = () => state.equipment || (state.equipment = BASIC_EQUIPMENT());
const monarchBonus = key => Object.values(monarchEquipment()).reduce((sum, id) => sum + (ITEMS[id]?.bonus?.[key] || 0), 0);
const monarchBonusText = bonus => Object.entries(bonus).map(([key, value]) => `+${value} ${key}`).join(' ');

// Loja do monarca no Arsenal: uma linha por slot, com o que este Arsenal libera e é melhor que o atual.
function monarchShopHTML(tierCap) {
  const equipped = monarchEquipment();
  const rows = EQUIP_SLOTS.map(({ slot, label }) => {
    const current = ITEMS[equipped[slot]];
    const kind = slot.startsWith('ring') ? 'ring' : slot;
    const options = Object.entries(ITEMS).filter(([, item]) => item.slot === kind && item.tier === (current?.tier || 0) + 1 && item.tier <= tierCap).sort((a, b) => a[1].tier - b[1].tier);
    // Pré-seleciona sempre o próximo tier para a progressão não saltar níveis.
    const pick = (options[0] || [])[0];
    return `<div class="monarch-shop-row"><span class="monarch-shop-slot">${current ? pixelIcon(current.icon, 24) : ''}<span><small>${label}</small><b>${current ? `${current.name} · T${current.tier}` : 'Vazio'}</b></span></span>
      ${options.length ? `<select data-monarch-pick="${slot}" aria-label="Escolher ${label.toLowerCase()}">${options.map(([id, item]) => `<option value="${id}"${id === pick ? ' selected' : ''}>T${item.tier} · ${item.price} ouro · ${item.name} · ${monarchBonusText(item.bonus)}</option>`).join('')}</select>
      <button type="button" class="monarch-buy" data-monarch-buy="${slot}">Comprar</button>` : `<em>${(current?.tier || 0) >= 10 ? 'Melhor que isso não existe' : 'Nada melhor neste nível'}</em>`}</div>`;
  });
  return `<p class="storage-items">O monarca paga com o cofre. A peça antiga vai pro fundo do baú.</p><div class="monarch-shop">${rows.join('')}</div>`;
}

function buyMonarchItem(slot, id) {
  const item = ITEMS[id];
  const current = ITEMS[monarchEquipment()[slot]];
  const kind = slot.startsWith('ring') ? 'ring' : slot;
  if (!item || item.slot !== kind || item.tier !== (current?.tier || 0) + 1) { say('O equipamento só avança para o próximo tier.'); return; }
  if (item.tier > armoryTier()) { say('O Arsenal ainda não vende isso. Melhore o Arsenal.'); return; }
  if (state.gold < item.price) { say(`Falta ouro. ${item.name} custa ${item.price}.`); return; }
  state.gold -= item.price;
  monarchEquipment()[slot] = id;
  say(`${item.name} agora é do monarca. O cofre emagreceu ${item.price}.`);
  updateHud();
  renderPanel();
}

// ==========================================================================
// Equipamento de heróis e camponeses. Cada um compra no Arsenal com a própria bolsa, pela ordem
// de prioridade que acha mais importante; o ouro da venda vai pro cofre do monarca.
// Cada nível do Arsenal libera o teto de tier definido em ARMORY_LEVELS.
// ==========================================================================
const HERO_SLOTS = [
  { slot: 'weapon', label: 'Arma' }, { slot: 'offhand', label: 'Mão secundária' }, { slot: 'head', label: 'Cabeça' }, { slot: 'body', label: 'Armadura' },
  { slot: 'hands', label: 'Mãos' }, { slot: 'feet', label: 'Pés' }, { slot: 'cloak', label: 'Capa' }, { slot: 'amulet', label: 'Amuleto' },
  { slot: 'ring1', label: 'Anel I' }, { slot: 'ring2', label: 'Anel II' },
];
// Usa as classes de arte que existem no catálogo de equipamento.
const HERO_WEAPON_ART_CLASS = {
  Warrior: 'warrior', Archer: 'hunter', 'Black Mage': 'mage', Conjurer: 'mage', Healer: 'priest',
};
const PEASANT_SLOTS = [
  { slot: 'tool', label: 'Ferramenta' }, { slot: 'clothes', label: 'Roupa' }, { slot: 'hands', label: 'Mãos' }, { slot: 'feet', label: 'Pés' }, { slot: 'head', label: 'Cabeça' },
];
const MELEE = ['MoonWarden', 'Warrior', 'Lancer', 'Healer'];
const FRONT = ['MoonWarden', 'Warrior', 'Lancer'];
const CASTERS = ['Black Mage', 'Conjurer'];
// bonus: atk, def, mag, hp, dex (heróis) · work (produção, fração), carry (carga extra), speed (passo, fração) (camponeses)
const GEAR = {
  rustySword: { name: 'Espada Enferrujada', slot: 'weapon', tier: 1, price: 10, classes: ['Warrior'], bonus: { atk: 2 }, about: 'A ferrugem também corta. Mais devagar.' },
  warAxe: { name: 'Machado de Guerra', slot: 'weapon', tier: 2, price: 40, classes: ['Warrior', 'MoonWarden'], bonus: { atk: 5 }, about: 'Pesado o bastante pra resolver discussões.' },
  valeGreatsword: { name: 'Montante do Vale', slot: 'weapon', tier: 3, price: 110, classes: ['Warrior', 'Vanguard'], bonus: { atk: 9 }, about: 'Forjado pra um herói. Vendido pra quem pagou.' },
  shortBow: { name: 'Arco Curto', slot: 'weapon', tier: 1, price: 10, classes: ['Archer'], bonus: { atk: 2, dex: 1 }, about: 'Curto no alcance e na paciência.' },
  yewBow: { name: 'Arco Longo de Teixo', slot: 'weapon', tier: 2, price: 40, classes: ['Archer'], bonus: { atk: 4, dex: 2 }, about: 'Teixo de cemitério. Por isso acerta.' },
  hornBow: { name: 'Arco de Chifre', slot: 'weapon', tier: 3, price: 110, classes: ['Archer'], bonus: { atk: 7, dex: 3 }, about: 'O dono anterior era o bicho do chifre.' },
  ashSpear: { name: 'Lança de Freixo', slot: 'weapon', tier: 1, price: 10, classes: ['Lancer'], bonus: { atk: 2 }, about: 'Um pau comprido com opinião na ponta.' },
  halberd: { name: 'Alabarda', slot: 'weapon', tier: 2, price: 40, classes: ['Lancer'], bonus: { atk: 5 }, about: 'Machado, lança e gancho. Indecisa, mas eficiente.' },
  knightLance: { name: 'Lança de Cavaleiro', slot: 'weapon', tier: 3, price: 110, classes: ['Lancer'], bonus: { atk: 9 }, about: 'Feita pra cavalo. Ninguém aqui tem cavalo.' },
  crookedStaff: { name: 'Cajado Torto', slot: 'weapon', tier: 1, price: 10, classes: ['Black Mage'], bonus: { mag: 2 }, about: 'Torto de nascença. A magia sai torta também.' },
  scorchedTome: { name: 'Grimório Chamuscado', slot: 'weapon', tier: 1, price: 10, classes: ['Conjurer'], bonus: { mag: 2 }, about: 'Faltam páginas. As que sobraram mordem.' },
  ebonyStaff: { name: 'Cajado de Ébano', slot: 'weapon', tier: 2, price: 45, classes: CASTERS, bonus: { mag: 5 }, about: 'Preto, lustroso e frio ao toque. Como o dono.' },
  skullStaff: { name: 'Cajado de Caveira', slot: 'weapon', tier: 3, price: 120, classes: CASTERS, bonus: { mag: 9 }, about: 'A caveira às vezes dá palpite.' },
  woodenMace: { name: 'Maça de Madeira', slot: 'weapon', tier: 1, price: 10, classes: ['Healer'], bonus: { atk: 1, mag: 1 }, about: 'Cura de um lado, machuca do outro.' },
  blessedMace: { name: 'Maça Benta', slot: 'weapon', tier: 2, price: 40, classes: ['Healer'], bonus: { atk: 2, mag: 3 }, about: 'Abençoada por um padre bêbado. Conta igual.' },
  reliquaryMace: { name: 'Maça Relicário', slot: 'weapon', tier: 3, price: 110, classes: ['Healer'], bonus: { atk: 4, mag: 5 }, about: 'Tem um dedo de santo dentro. Dizem.' },
  plankShield: { name: 'Escudo de Tábuas', slot: 'offhand', tier: 1, price: 8, classes: MELEE, bonus: { def: 2 }, about: 'Era uma porta. Ainda range.' },
  ironShield: { name: 'Escudo de Ferro', slot: 'offhand', tier: 2, price: 35, classes: MELEE, bonus: { def: 4 }, about: 'Amassado nos lugares certos.' },
  towerShield: { name: 'Escudo-Torre', slot: 'offhand', tier: 3, price: 95, classes: FRONT, bonus: { def: 7 }, about: 'Dá pra morar atrás dele.' },
  quiver: { name: 'Aljava de Couro', slot: 'offhand', tier: 1, price: 8, classes: ['Archer'], bonus: { dex: 2 }, about: 'Cabe flecha, pão e arrependimento.' },
  boneTalisman: { name: 'Talismã de Osso', slot: 'offhand', tier: 1, price: 8, classes: CASTERS, bonus: { mag: 1 }, about: 'Chacoalha quando alguém mente.' },
  crystalFocus: { name: 'Foco de Cristal', slot: 'offhand', tier: 2, price: 35, classes: CASTERS, bonus: { mag: 3 }, about: 'Concentra a magia. E o sol, se esquecer na janela.' },
  leatherHood: { name: 'Capuz de Couro', slot: 'head', tier: 1, price: 6, bonus: { def: 1 }, about: 'Protege da chuva. Do resto, nem tanto.' },
  ironHelm: { name: 'Elmo de Ferro', slot: 'head', tier: 2, price: 30, classes: MELEE, bonus: { def: 3 }, about: 'Ouve-se o mar dentro dele. Não tem mar aqui.' },
  greatHelm: { name: 'Elmo Fechado', slot: 'head', tier: 3, price: 80, classes: FRONT, bonus: { def: 5 }, about: 'Não se vê nada. Mas também não se é visto.' },
  pointyHat: { name: 'Chapéu Pontudo', slot: 'head', tier: 2, price: 30, classes: CASTERS, bonus: { mag: 2 }, about: 'Ninguém sabe se ajuda. Ninguém arrisca tirar.' },
  paddedJerkin: { name: 'Gibão Acolchoado', slot: 'body', tier: 1, price: 12, bonus: { def: 2, hp: 3 }, about: 'Recheado de palha e esperança.' },
  studdedLeather: { name: 'Couro Tachonado', slot: 'body', tier: 2, price: 45, classes: ['Archer'], bonus: { def: 3, dex: 2 }, about: 'Leve, firme e com cheiro de curtume.' },
  chainmail: { name: 'Cota de Malha', slot: 'body', tier: 2, price: 50, classes: MELEE, bonus: { def: 5, hp: 5 }, about: 'Mil argolinhas, cada uma com um furo.' },
  arcaneRobe: { name: 'Manto Arcano', slot: 'body', tier: 2, price: 45, classes: [...CASTERS, 'Healer'], bonus: { mag: 3, hp: 3 }, about: 'Bordado com runas. Uma diz "lavar à mão".' },
  plateArmor: { name: 'Armadura de Placas', slot: 'body', tier: 3, price: 130, classes: FRONT, bonus: { def: 9, hp: 8 }, about: 'Leva meia hora pra vestir e a vida toda pra pagar.' },
  leatherGloves: { name: 'Luvas de Couro', slot: 'hands', tier: 1, price: 5, bonus: { dex: 1 }, about: 'Firmes no punho. Furadas no dedão.' },
  ironGauntlets: { name: 'Manoplas de Ferro', slot: 'hands', tier: 2, price: 25, classes: MELEE, bonus: { def: 2, atk: 1 }, about: 'Ótimas pra socar. Péssimas pra coçar.' },
  wornBoots: { name: 'Botas Gastas', slot: 'feet', tier: 1, price: 5, bonus: { dex: 1 }, about: 'Já andaram mais que o dono.' },
  travelBoots: { name: 'Botas de Viagem', slot: 'feet', tier: 2, price: 22, bonus: { dex: 2, hp: 2 }, about: 'Feitas pra ir longe. Ninguém volta pra reclamar.' },
  steelGreaves: { name: 'Grevas de Aço', slot: 'feet', tier: 3, price: 70, classes: FRONT, bonus: { def: 4 }, about: 'Chutam portas. E canelas.' },
  woolCloak: { name: 'Capa de Lã', slot: 'cloak', tier: 1, price: 6, bonus: { hp: 2 }, about: 'Esquenta e pinica na mesma medida.' },
  travelerCloak: { name: 'Capa de Viajante', slot: 'cloak', tier: 2, price: 25, bonus: { hp: 4, def: 1 }, about: 'Tem bolso secreto. Vazio.' },
  shadowCloak: { name: 'Capa das Sombras', slot: 'cloak', tier: 3, price: 75, bonus: { dex: 3, def: 2 }, about: 'Some no escuro. O dono também, às vezes.' },
  garlicAmulet: { name: 'Amuleto de Alho', slot: 'amulet', tier: 1, price: 6, bonus: { def: 1 }, about: 'Afasta strigoi e amizades.' },
  silverAmulet: { name: 'Amuleto de Prata', slot: 'amulet', tier: 2, price: 35, bonus: { mag: 2, def: 1 }, about: 'Prata de verdade. Quase toda.' },
  saintReliquary: { name: 'Relicário de Santo', slot: 'amulet', tier: 3, price: 90, bonus: { hp: 6, mag: 2 }, about: 'Ossinho de santo. De qual santo, ninguém garante.' },
  ironRing: { name: 'Anel de Ferro', slot: 'ring', tier: 1, price: 8, bonus: { atk: 1 }, about: 'Deixa marca no dedo e na cara dos outros.' },
  silverRing: { name: 'Anel de Prata', slot: 'ring', tier: 2, price: 30, bonus: { def: 2 }, about: 'Brilha o suficiente pra ser roubado.' },
  rubyRing: { name: 'Anel de Rubi', slot: 'ring', tier: 3, price: 85, bonus: { atk: 3, mag: 3 }, about: 'Vermelho como a dívida que ele custou.' },
  // Camponeses. Itens com prof são da profissão; sem profissão o camponês só compra o básico (sem prof).
  oldSickle: { name: 'Foice Velha', slot: 'tool', tier: 1, price: 5, peasant: true, prof: ['farmer'], bonus: { work: 0.1 }, about: 'Cega, mas insiste.' },
  ironHoe: { name: 'Enxada de Ferro', slot: 'tool', tier: 2, price: 15, peasant: true, prof: ['farmer'], bonus: { work: 0.2 }, about: 'A terra cede. As costas também.' },
  greatScythe: { name: 'Gadanha', slot: 'tool', tier: 3, price: 30, peasant: true, prof: ['farmer'], bonus: { work: 0.3 }, about: 'Corta trigo e conversa fiada.' },
  flourSieve: { name: 'Peneira de Farinha', slot: 'tool', tier: 1, price: 5, peasant: true, prof: ['miller'], bonus: { work: 0.1 }, about: 'Separa a farinha do resto. O resto é a maior parte.' },
  millPick: { name: 'Picão de Mó', slot: 'tool', tier: 2, price: 15, peasant: true, prof: ['miller'], bonus: { work: 0.2 }, about: 'Afia a pedra do moinho. E a paciência.' },
  millwrightKit: { name: 'Ferramentas de Moleiro', slot: 'tool', tier: 3, price: 30, peasant: true, prof: ['miller'], bonus: { work: 0.3 }, about: 'Conserta engrenagem, pá e eixo. Menos a vida.' },
  hatchet: { name: 'Machadinha', slot: 'tool', tier: 1, price: 5, peasant: true, prof: ['woodcutter'], bonus: { work: 0.1 }, about: 'Pequena, mas não se ofende.' },
  woodAxe: { name: 'Machado de Lenhador', slot: 'tool', tier: 2, price: 15, peasant: true, prof: ['woodcutter'], bonus: { work: 0.2 }, about: 'As árvores do vale já ouviram falar dele.' },
  bowSaw: { name: 'Serra de Arco', slot: 'tool', tier: 3, price: 30, peasant: true, prof: ['woodcutter'], bonus: { work: 0.3 }, about: 'Range como gente. Corta melhor que gente.' },
  mallet: { name: 'Maço de Madeira', slot: 'tool', tier: 1, price: 5, peasant: true, prof: ['builder'], bonus: { build: 0.1 }, about: 'Bate prego, bate dedo.' },
  masonHammer: { name: 'Martelo de Pedreiro', slot: 'tool', tier: 2, price: 15, peasant: true, prof: ['builder'], bonus: { build: 0.2 }, about: 'Ergue parede. Às vezes reta.' },
  smithTools: { name: 'Ferramentas de Ferreiro', slot: 'tool', tier: 3, price: 30, peasant: true, prof: ['builder'], bonus: { build: 0.3 }, about: 'Martelo, tenaz e lima. Resolve até dobradiça.' },
  workClothes: { name: 'Roupa de Trabalho', slot: 'clothes', tier: 1, price: 4, peasant: true, bonus: { carry: 1 }, about: 'Remendada sobre remendo.' },
  farmSmock: { name: 'Bata de Lavrador', slot: 'clothes', tier: 2, price: 10, peasant: true, prof: ['farmer', 'woodcutter'], bonus: { carry: 2 }, about: 'Bolsos fundos e cheiro de terra.' },
  leatherApron: { name: 'Avental de Couro', slot: 'clothes', tier: 2, price: 10, peasant: true, prof: ['miller', 'builder'], bonus: { carry: 2 }, about: 'Segura farinha, cal e reclamação.' },
  smithApron: { name: 'Avental de Ferreiro', slot: 'clothes', tier: 3, price: 20, peasant: true, prof: ['builder'], bonus: { carry: 3 }, about: 'Aguenta brasa, ferro e patrão.' },
  woolGloves: { name: 'Luvas de Lã', slot: 'hands', tier: 1, price: 3, peasant: true, bonus: { work: 0.05 }, about: 'Quentinhas até molhar.' },
  workGloves: { name: 'Luvas de Couro Grosso', slot: 'hands', tier: 2, price: 8, peasant: true, bonus: { work: 0.1 }, about: 'Calo é pra quem não tem.' },
  smithGloves: { name: 'Luvas de Ferreiro', slot: 'hands', tier: 3, price: 16, peasant: true, prof: ['builder', 'woodcutter'], bonus: { work: 0.1, build: 0.15 }, about: 'Pega carvão em brasa. Não recomendado.' },
  clogs: { name: 'Tamancos', slot: 'feet', tier: 1, price: 3, peasant: true, bonus: { speed: 0.05 }, about: 'Dá pra ouvir chegando.' },
  workBoots: { name: 'Botas de Trabalho', slot: 'feet', tier: 2, price: 8, peasant: true, bonus: { speed: 0.1 }, about: 'Lama não entra. Quase.' },
  hobnailBoots: { name: 'Botas Ferradas', slot: 'feet', tier: 3, price: 16, peasant: true, prof: ['woodcutter', 'builder'], bonus: { speed: 0.15 }, about: 'Pregos na sola, firmeza no passo.' },
  strawHat: { name: 'Chapéu de Palha', slot: 'head', tier: 1, price: 2, peasant: true, bonus: { work: 0.05 }, about: 'Protege do sol. Sol aqui é raro.' },
  wideHat: { name: 'Chapéu de Abas Largas', slot: 'head', tier: 2, price: 6, peasant: true, prof: ['farmer'], bonus: { work: 0.1 }, about: 'Sombra pra ele e pra quem estiver do lado.' },
};
const METAL_WEAPON_PROFILE = {
  copper: { structural: .75, wear: 1.6 }, arsenicalCopper: { structural: .9, wear: 1.25 }, bronze: { structural: 1, wear: 1 },
  iron: { structural: 1.1, wear: 1.2 }, castIron: { structural: .85, wear: 1 }, steel: { structural: 1.25, wear: .7 },
  crucibleSteel: { structural: 1.3, wear: .6 }, meteoriteIron: { structural: 1.1, wear: 1.1 },
};
const WEAPON_PATTERNS = [
  { id: 'dagger', name: 'Adaga', base: 'rustySword', classes: MELEE, sharp: true, cost: 1, designs: { copper: ['copper'], arsenicalCopper: ['arsenicalCopper'], bronze: ['bronze'], iron: ['iron'], steel: ['steel'], crucibleSteel: ['crucibleSteel'], meteoriteIron: ['meteoriteIron'] } },
  { id: 'spear', name: 'Lança', base: 'ashSpear', classes: ['Lancer'], sharp: true, shaft: 1, cost: 2, designs: { copper: ['copper'], arsenicalCopper: ['arsenicalCopper'], bronze: ['bronze'], iron: ['iron'], steel: ['steel'] } },
  { id: 'axe', name: 'Machado', base: 'warAxe', classes: ['Warrior', 'MoonWarden'], sharp: true, handle: 1, cost: 3, designs: { bronze: ['bronze'], iron: ['iron'], steelEdge: ['iron', 'steel'], steel: ['steel'] } },
  { id: 'mace', name: 'Maça', base: 'woodenMace', classes: MELEE, sharp: false, handle: 1, cost: 3, designs: { bronze: ['bronze'], iron: ['iron'], castIron: ['castIron'] } },
  { id: 'sword', name: 'Espada', base: 'rustySword', classes: ['Warrior'], sharp: true, grip: 1, cost: 4, designs: { bronze: ['bronze'], iron: ['iron'], steelEdge: ['iron', 'steel'], steel: ['steel'], crucibleSteel: ['crucibleSteel'] } },
];
const WEAPON_MATERIAL_LABELS = { copper: 'Cobre', arsenicalCopper: 'Cobre arsenical', bronze: 'Bronze', iron: 'Ferro trabalhado', castIron: 'Ferro fundido', steel: 'Aço', steelEdge: 'Ferro trabalhado com gume de aço', crucibleSteel: 'Aço de cadinho', meteoriteIron: 'Ferro meteórico' };
const weaponPattern = id => WEAPON_PATTERNS.find(item => item.id === id) || WEAPON_PATTERNS[0];
const weaponMinLevel = material => ['steel', 'steelEdge', 'castIron', 'crucibleSteel', 'meteoriteIron'].includes(material) ? 2 : 1;
function weaponDesign(patternId, materialId) {
  const pattern = weaponPattern(patternId), materials = pattern.designs[materialId];
  if (!materials) return null;
  const needs = {};
  if (materials.length === 1) needs[materials[0]] = pattern.cost;
  else { needs[materials[0]] = pattern.cost - 1; needs[materials[1]] = 1; }
  if (pattern.shaft || pattern.handle || pattern.grip) needs.timber = pattern.shaft || pattern.handle || pattern.grip;
  return { pattern, materialId, materials, needs, body: materials[0], edge: pattern.sharp ? materials.at(-1) : null, name: `${pattern.name} de ${WEAPON_MATERIAL_LABELS[materialId]}` };
}
const craftedWeapon = id => (state.craftedWeapons || []).find(item => item.id === id) || null;
const gearItem = id => GEAR[id] || craftedWeapon(id);
const STARTER_WEAPON = { MoonWarden: 'warAxe', Vanguard: 'valeGreatsword', Warrior: 'rustySword', Archer: 'shortBow', Lancer: 'ashSpear', 'Black Mage': 'crookedStaff', Conjurer: 'scorchedTome', Healer: 'woodenMace' };
function gearIconHTML(item, actorClass, label = '') {
  const artClass = HERO_WEAPON_ART_CLASS[actorClass];
  const tier = Number(item?.tier);
  if (item?.slot === 'weapon' && !item.crafted && artClass && tier >= 1 && tier <= 10) {
    const path = `public/art/equipment/icon-${artClass}-t${String(tier).padStart(2, '0')}-mainHand-photo-v01.png`;
    return `<span class="gear-icon weapon-icon"><img src="${path}" alt="" aria-hidden="true" decoding="async"></span>`;
  }
  return `<span class="gear-icon" aria-hidden="true">${label[0] || '·'}</span>`;
}
// Cada herói compra na ordem que acha mais importante. A mania muda a ordem.
const SLOT_PRIORITY = {
  default: ['weapon', 'body', 'head', 'offhand', 'feet', 'hands', 'cloak', 'amulet', 'ring1', 'ring2'],
  Valente: ['weapon', 'hands', 'ring1', 'body', 'offhand', 'head', 'feet', 'cloak', 'amulet', 'ring2'],
  Teimoso: ['weapon', 'ring1', 'ring2', 'body', 'head', 'offhand', 'hands', 'feet', 'cloak', 'amulet'],
  Covarde: ['body', 'head', 'offhand', 'feet', 'cloak', 'hands', 'amulet', 'weapon', 'ring1', 'ring2'],
  Amaldiçoado: ['amulet', 'body', 'head', 'cloak', 'weapon', 'offhand', 'feet', 'hands', 'ring1', 'ring2'],
  Ganancioso: ['ring1', 'ring2', 'amulet', 'weapon', 'body', 'head', 'offhand', 'cloak', 'feet', 'hands'],
  Bêbado: ['cloak', 'feet', 'weapon', 'body', 'head', 'offhand', 'hands', 'amulet', 'ring1', 'ring2'],
};
// Ofício: você escolhe onde o camponês trabalha. Sem ofício ele faz o que houver e não compra ferramenta de ofício.
// O que ele sabe fazer são as profissões de data/peasant-professions.js (abaixo), que sobem com o trabalho.
const PROFESSIONS = {
  farmer: { name: 'Fazendeiro', fem: 'Fazendeira', skill: 'farming', types: ['farm'] },
  miller: { name: 'Moleiro', fem: 'Moleira', skill: 'milling', types: ['mill'] },
  cook: { name: 'Cozinheiro', fem: 'Cozinheira', skill: 'cooking', types: ['bakery', 'butcher'] },
  fisher: { name: 'Pescador', fem: 'Pescadora', skill: 'fishing', types: [] },
  hunter: { name: 'Caçador', fem: 'Caçadora', skill: 'hunting', types: [] },
  woodcutter: { name: 'Lenhador', fem: 'Lenhadora', skill: 'woodcutting', types: ['lumber'] },
  miner: { name: 'Minerador', fem: 'Mineradora', skill: 'mining', types: ['quarry', 'mine'] },
  builder: { name: 'Pedreiro', fem: 'Pedreira', skill: 'building', types: [] },
};
// Habilidades de 0 a 100. Sobem devagar com o que o camponês faz, e mais devagar quanto mais alto.
const SKILLS = [
  { id: 'farming', name: 'Lavoura' }, { id: 'milling', name: 'Moagem' }, { id: 'woodcutting', name: 'Lenha' }, { id: 'mining', name: 'Mineração' },
  { id: 'building', name: 'Construção' }, { id: 'hauling', name: 'Carga' }, { id: 'cooking', name: 'Culinária' }, { id: 'fishing', name: 'Pesca' }, { id: 'hunting', name: 'Caça' },
  { id: 'alcoholism', name: 'Alcoolismo · penalidade' },
];
const SKILL_OF_TYPE = { farm: 'farming', mill: 'milling', bakery: 'cooking', butcher: 'cooking', lumber: 'woodcutting', quarry: 'mining', mine: 'mining' };
const skillOf = (actor, id) => actor?.skills?.[id] || 0;
// ==========================================================================
// Profissões (data/peasant-professions.js). Todo camponês começa generalista, com as sete no nível 1.
// Cada trabalho feito no jogo rende XP para a profissão ligada a ele pelo "hook" da ação.
// No nível 50 a profissão para e libera a especialização, escolhida uma vez na ficha.
// ==========================================================================
const TRADE_DATA = window.PEASANT_PROFESSIONS || { maxLevel: 50, levelTable: [], masteryRanks: [0], attributes: {}, professions: [] };
const TRADES = TRADE_DATA.professions;
const TRADE_MAX = TRADE_DATA.maxLevel;
const TRADE_BY_KEY = Object.fromEntries(TRADES.map(trade => [trade.key, trade]));
const TRADE_HOOKS = {};
TRADES.forEach(trade => trade.actions.forEach(action => { if (action.hook) TRADE_HOOKS[action.hook] = { trade: trade.key, action }; }));
// Cada segundo de obra ou conserto conta como um pedaço do "consertar equipamento de madeira".
const BUILD_ACTION_SECONDS = 3;
const hookOf = building => building.type === 'farm' ? `farm:${farmType(building.produce).id}` : building.type;
// Rendimento extra que só vale num tipo de trabalho.
const HOOK_YIELD = { lumber: 'woodYieldPercent', quarry: 'oreYieldPercent', mine: 'oreYieldPercent', 'farm:wheat': 'harvestYieldPercent', 'farm:garlic': 'harvestYieldPercent', 'farm:bacon': 'animalYieldPercent', 'farm:milk': 'animalYieldPercent', 'farm:eggs': 'animalYieldPercent' };
const EFFECT_LABELS = {
  workSpeedPercent: 'trabalho', yieldPercent: 'rendimento', toolDurabilityPercent: 'durabilidade da ferramenta', doubleYieldChancePercent: 'chance de render dobrado',
  growthTimePercent: 'tempo de crescimento', harvestYieldPercent: 'colheita', animalYieldPercent: 'rendimento dos animais', animalDeathChancePercent: 'chance de animal morrer',
  catchChancePercent: 'chance de pegar', rareCatchChancePercent: 'chance de pegar coisa rara', repairAmountPercent: 'conserto', buildSpeedPercent: 'obra',
  fuelCostPercent: 'gasto de carvão', masterworkChancePercent: 'chance de obra-prima', foodSpoilagePercent: 'comida estragando', mealHappinessBonus: 'felicidade por refeição',
  woodYieldPercent: 'madeira', oreYieldPercent: 'minério', yieldPerSwingPercent: 'rendimento por golpe', gemChancePercent: 'chance de gema',
  herbYieldPercent: 'ervas', rareHerbChancePercent: 'chance de erva rara', breedingSpeedPercent: 'reprodução', tamingSpeedPercent: 'doma', beastHpPercent: 'vida das feras',
  fishYieldPercent: 'peixe', treasureChancePercent: 'chance de tesouro', hideQualityPercent: 'qualidade do couro', gameYieldPercent: 'carne de caça',
  clothArmorQualityPercent: 'qualidade de roupa e couro', constructionWoodCostPercent: 'madeira gasta em obra', armorQualityPercent: 'qualidade da armadura', batchSizeBonus: 'peças por fornada',
  bladeQualityPercent: 'qualidade da lâmina', mealBuffDurationPercent: 'duração do efeito da comida', mealQualityPercent: 'qualidade da comida', storageCapacityPercent: 'espaço de depósito',
  xpGainPercent: 'XP',
};
// Efeitos que o jogo já usa; os outros ficam guardados até a parte do jogo existir.
const LIVE_EFFECTS = new Set(['workSpeedPercent', 'yieldPercent', 'woodYieldPercent', 'oreYieldPercent', 'harvestYieldPercent', 'animalYieldPercent', 'growthTimePercent', 'buildSpeedPercent', 'repairAmountPercent', 'xpGainPercent']);
// mark: põe * nos efeitos que o jogo ainda não usa.
const effectText = (effects, mark = false) => Object.entries(effects).map(([key, value]) => {
  const sign = value > 0 ? '+' : '−';
  if (TRADE_DATA.attributes[key]) return `${sign}${Math.abs(value)} ${TRADE_DATA.attributes[key].name}`;
  return `${sign}${Math.abs(value)}${key.endsWith('Percent') ? '%' : ''} ${EFFECT_LABELS[key] || key}${mark && !LIVE_EFFECTS.has(key) ? '*' : ''}`;
}).join(', ');
function tradeOf(actor, key) {
  actor.trades = actor.trades || {};
  return actor.trades[key] || (actor.trades[key] = { xp: 0, level: 1, spec: null, mastery: 0 });
}
const tradeLevel = (actor, key) => actor?.trades?.[key]?.level || 1;
const tradeSpec = (actor, key) => TRADE_BY_KEY[key]?.specializations.find(spec => spec.key === actor?.trades?.[key]?.spec) || null;
const masteryRank = progress => TRADE_DATA.masteryRanks.filter(start => (progress.mastery || 0) >= start).length;
// Passivos ganhos e bônus da especialização, somados.
function tradeEffects(actor, key) {
  const trade = TRADE_BY_KEY[key];
  const total = {};
  if (!trade || !actor?.trades?.[key]) return total;
  const add = effects => Object.entries(effects).forEach(([id, value]) => { total[id] = (total[id] || 0) + value; });
  trade.passives.filter(passive => passive.level <= tradeLevel(actor, key)).forEach(passive => add(passive.effects));
  const spec = tradeSpec(actor, key);
  if (spec) add(spec.bonus);
  return total;
}
// Pontos de atributo de todas as profissões, já convertidos em atributos de luta (hp, atk, def, dex, mag).
function tradeCombat(actor) {
  const combat = {};
  if (!actor?.trades) return combat;
  TRADES.forEach(trade => Object.entries(tradeEffects(actor, trade.key)).forEach(([id, points]) => {
    Object.entries(TRADE_DATA.attributes[id]?.combat || {}).forEach(([stat, per]) => { combat[stat] = (combat[stat] || 0) + points * per; });
  }));
  return combat;
}
const tradeAttribute = (actor, id) => TRADES.reduce((sum, trade) => sum + (tradeEffects(actor, trade.key)[id] || 0), 0);
// Bônus total da profissão ligada a esse trabalho (0,1 = +10%). No trabalho ele se divide no meio:
// metade aumenta a produção do lugar (crewWork) e metade encurta cada tarefa (taskSpeed).
// Na obra e no conserto é igual: metade no avanço da obra, metade no ritmo de cada ação (buildPractice).
function tradeWorkBonus(actor, hook) {
  const link = TRADE_HOOKS[hook];
  if (!link) return 0;
  const fx = tradeEffects(actor, link.trade);
  return ((fx.workSpeedPercent || 0) + (fx.yieldPercent || 0) + (fx[HOOK_YIELD[hook]] || 0) - (hook.startsWith('farm:') ? fx.growthTimePercent || 0 : 0)) / 100;
}
const taskSpeed = (actor, place) => 1 + (place ? tradeWorkBonus(actor, hookOf(place)) / 2 : 0);
function tradeBuildBonus(actor, repairing) {
  const fx = tradeEffects(actor, TRADE_HOOKS.build?.trade);
  return ((fx.workSpeedPercent || 0) + (fx.buildSpeedPercent || 0) + (repairing ? fx.repairAmountPercent || 0 : 0)) / 100;
}
// Uma ação feita no mundo: xp = baseXp × max(0,25; 1 − 0,03 × (nível − nível da ação)) × (1 + xpGainPercent / 100).
function gainTrade(actor, hook) {
  if (!actor || actor.className !== 'Peasant') return;
  const link = TRADE_HOOKS[hook];
  if (!link) return;
  const trade = TRADE_BY_KEY[link.trade];
  const progress = tradeOf(actor, trade.key);
  const xp = Math.round(link.action.baseXp * Math.max(0.25, 1 - 0.03 * (progress.level - link.action.level)) * (1 + (tradeEffects(actor, trade.key).xpGainPercent || 0) / 100));
  if (progress.level >= TRADE_MAX) {
    // Sem especialização escolhida, o XP se perde. Com ela, vira maestria.
    if (!progress.spec) return;
    const before = masteryRank(progress);
    progress.mastery = (progress.mastery || 0) + xp;
    const rank = masteryRank(progress);
    if (rank > before) say(`${actor.name} chegou ao rank ${rank} de ${tradeSpec(actor, trade.key).name}. Liberou ${tradeSpec(actor, trade.key).unlocks.filter(unlock => unlock.rank === rank).map(unlock => unlock.name).join(', ')}.`);
    return;
  }
  progress.xp += xp;
  const before = progress.level;
  while (progress.level < TRADE_MAX && progress.xp >= TRADE_DATA.levelTable[progress.level].totalXp) progress.level++;
  for (let level = before + 1; level <= progress.level; level++) {
    const passive = trade.passives.find(item => item.level === level);
    const unlocked = trade.unlocks.filter(unlock => unlock.level === level).map(unlock => unlock.name);
    if (level === TRADE_MAX) say(`${actor.name} chegou ao nível ${TRADE_MAX} de ${trade.name}. Ganhou ${passive?.name || 'nada'} e agora escolhe a especialização, na ficha. Pra sempre.`);
    else if (passive) say(`${actor.name} chegou ao nível ${level} de ${trade.name} e ganhou ${passive.name} (${effectText(passive.effects)}).${unlocked.length ? ` Liberou ${unlocked.join(', ')}.` : ''} Ninguém comemorou.`);
  }
}
const UNLOCK_TYPES = { tool: 'Ferramenta', recipe: 'Receita', action: 'Ação' };
const unlockLine = (unlock, got, tag) => `<li class="${got ? 'got' : ''}"><em>${tag}</em> ${UNLOCK_TYPES[unlock.type] || unlock.type}: <b>${unlock.name}</b>${unlock.about ? ` · ${unlock.about}` : ''}</li>`;
// Ficha do camponês: as sete profissões, com XP, passivos, o que liberam e a especialização.
function tradesHTML(actor) {
  const place = actor.buildSite != null ? null : workplaceOf(actor);
  const current = actor.buildSite != null ? TRADE_HOOKS.build?.trade : place ? TRADE_HOOKS[hookOf(place)]?.trade : null;
  const table = TRADE_DATA.levelTable;
  const rows = TRADES.map(trade => {
    const progress = actor.trades?.[trade.key] || { xp: 0, level: 1, spec: null, mastery: 0 };
    const spec = tradeSpec(actor, trade.key);
    const maxed = progress.level >= TRADE_MAX;
    let fill = 100, status;
    if (spec) {
      const rank = masteryRank(progress);
      const start = TRADE_DATA.masteryRanks[rank - 1], next = TRADE_DATA.masteryRanks[rank];
      if (next != null) fill = ((progress.mastery || 0) - start) / (next - start) * 100;
      status = `${spec.name} · rank ${rank}${next != null ? ` · ${format(progress.mastery || 0)}/${format(next)} de maestria` : ' · maestria completa'}`;
    } else if (maxed) status = 'Pronto para especializar';
    else {
      const from = table[progress.level - 1].totalXp, to = table[progress.level].totalXp;
      fill = (progress.xp - from) / (to - from) * 100;
      status = `${format(progress.xp - from)}/${format(to - from)} XP`;
    }
    const actions = trade.actions.map(action => `${action.name}${action.hook ? '' : '*'}`).join(', ');
    const passives = trade.passives.map(passive => `<li class="${progress.level >= passive.level ? 'got' : ''}"><em>Nv ${passive.level}</em> <b>${passive.name}</b> · ${effectText(passive.effects, true)}</li>`).join('');
    const unlocks = trade.unlocks.map(unlock => unlockLine(unlock, progress.level >= unlock.level, `Nv ${unlock.level}`)).join('');
    const specs = spec
      ? `<p class="trade-note"><b>${spec.option} · ${spec.name}</b>: ${spec.focus}. ${effectText(spec.bonus, true)}.</p><ul>${spec.unlocks.map(unlock => unlockLine(unlock, masteryRank(progress) >= unlock.rank, `Rank ${unlock.rank}`)).join('')}</ul>`
      : maxed
        ? `<p class="trade-note">Escolha uma. É definitiva.</p><div class="trade-specs">${trade.specializations.map(item => `<button type="button" class="trade-spec" data-action="pick-spec" data-id="${actor.id}" data-trade="${trade.key}" data-spec="${item.key}"><b>${item.option} · ${item.name}</b><small>${item.focus}. ${effectText(item.bonus, true)}</small></button>`).join('')}</div>`
        : `<ul>${trade.specializations.map(item => `<li><em>Nv ${TRADE_DATA.specializationLevel}</em> <b>${item.option} · ${item.name}</b> · ${item.focus}</li>`).join('')}</ul>`;
    return `<details class="trade${current === trade.key ? ' main' : ''}${maxed && !spec ? ' ready' : ''}"><summary><span class="trade-name">${trade.name}</span><span class="trade-bar"><i style="width:${Math.max(0, Math.min(100, fill))}%"></i></span><b>${progress.level}</b><small>${status}</small></summary>
      <div class="trade-body"><p class="trade-note">${trade.area}. Sobe com: ${actions}.</p>
        <div class="trade-sub">PASSIVOS</div><ul>${passives}</ul>
        <div class="trade-sub">LIBERA</div><ul>${unlocks}</ul>
        <div class="trade-sub">ESPECIALIZAÇÃO</div>${specs}</div></details>`;
  });
  return `<div class="sheet-section-label">PROFISSÕES</div><div class="trade-list">${rows.join('')}</div>
    <p class="trade-note">* Ainda sem lugar no jogo: a ação não rende XP e o efeito não muda nada. Receitas e ferramentas ficam liberadas, mas ainda não há oficina para fazê-las.</p>`;
}
function chooseSpecialization(id, key, specKey) {
  const actor = findActor(id);
  const trade = TRADE_BY_KEY[key];
  const spec = trade?.specializations.find(item => item.key === specKey);
  if (!actor || !spec) return;
  const progress = tradeOf(actor, key);
  if (progress.level < TRADE_DATA.specializationLevel || progress.spec) return;
  progress.spec = spec.key;
  progress.mastery = 0;
  say(`${actor.name} agora é ${spec.name}. Não tem volta, como quase tudo em Mordavia.`);
}
function gainSkill(actor, id, amount) {
  if (!actor || actor.className !== 'Peasant') return;
  actor.skills = actor.skills || {};
  const now = actor.skills[id] || 0;
  actor.skills[id] = Math.min(100, now + amount * (1 - now / 110));
}
const professionName = actor => { const prof = PROFESSIONS[actor.profession]; return prof ? (isFemalePeasant(actor) ? prof.fem : prof.name) : 'Nenhum, faz o que aparece'; };
function setProfession(id, profession) {
  const worker = findActor(id);
  if (!worker || worker.className !== 'Peasant') return;
  worker.profession = PROFESSIONS[profession] ? profession : null;
  interruptTavernVisit(worker);
  worker.jobSite = null;
  if (worker.workplace?.type === 'lumber' && worker.profession !== 'woodcutter') worker.workplace = null;
  if (isPit(worker.workplace) && worker.profession && worker.profession !== 'miner') worker.workplace = null;
  worker.route = []; worker.finalPoint = null; worker.wait = 0;
  workplaceOf(worker);
  renderPanel();
  say(worker.profession ? `${worker.name} agora é ${professionName(worker).toLowerCase()}. Ninguém perguntou se queria.` : `${worker.name} ficou sem ofício. Vai fazer o que aparecer.`);
}
function assignFisherToSpot(workerId, spotId) {
  const worker = findActor(workerId), spot = FISHING_SPOTS.find(item => item.site === spotId);
  if (!worker || worker.className !== 'Peasant' || !worker.alive || !spot || worker.buildSite != null || worker.carrying) return;
  worker.profession = 'fisher';
  worker.jobSite = spot.site;
  worker.workplace = null;
  interruptTavernVisit(worker);
  worker.workTrip = false;
  worker.route = [];
  worker.goal = null;
  worker.finalPoint = null;
  worker.wait = 0;
  worker.actionTime = 0;
  worker.actionClock = 0;
  say(`${worker.name} foi atribuído ao ${spot.name}. Está indo pescar.`);
  renderHeroes();
  renderFishingSpots();
  renderPanel();
}
function removeFisherFromSpot(workerId) {
  const worker = findActor(workerId);
  if (!worker || worker.className !== 'Peasant' || worker.profession !== 'fisher' || worker.carrying?.food === 'fish') return;
  worker.profession = null;
  worker.jobSite = null;
  worker.workplace = null;
  worker.workTrip = false;
  worker.route = [];
  worker.goal = null;
  worker.finalPoint = null;
  worker.wait = 0;
  say(`${worker.name} foi retirado do ponto de pesca.`);
  renderHeroes();
  renderFishingSpots();
  renderPanel();
}
const PEASANT_PRIORITY = ['tool', 'clothes', 'feet', 'hands', 'head'];
const ARMORY_LEVELS = [
  { gold: 0, wood: 0, tierCap: 3 },
  { gold: 240, wood: 16, tierCap: 5 },
  { gold: 400, wood: 28, tierCap: 8 },
  { gold: 720, wood: 48, tierCap: 10 },
];
const BONUS_LABELS = { atk: 'Ataque', def: 'Defesa', mag: 'Magia', hp: 'Vida', dex: 'Destreza', work: 'trabalho', carry: 'carga', speed: 'passo', build: 'obra' };
const gearOf = actor => actor.gear || (actor.gear = {});
const slotsFor = actor => actor.className === 'Peasant' ? PEASANT_SLOTS
  : actor.guildForm === 'Grande Lobo' ? []
  : actor.className === 'Vanguard' ? HERO_SLOTS.filter(({ slot }) => slot !== 'offhand') : HERO_SLOTS;
const slotKind = slot => slot.startsWith('ring') ? 'ring' : slot;
const fitsSlot = (item, actor, slot) => (actor.guildForm !== 'Grande Lobo' && !(actor.className === 'Vanguard' && slot === 'offhand')) && item.slot === slotKind(slot) && (actor.className === 'Peasant' ? !!item.peasant && (!item.prof || item.prof.includes(actor.profession)) : !item.peasant && (!item.classes || item.classes.includes(actor.className)));
const gearBonus = (actor, key) => actor.guildForm === 'Grande Lobo' ? 0 : Object.values(gearOf(actor)).reduce((sum, id) => {
  const item = gearItem(id);
  if (!item || (item.crafted && ((item.structuralCondition || 0) <= 0 || (key === 'atk' && item.sharp && (item.edgeCondition || 0) <= 0)))) return sum;
  return sum + (item.bonus?.[key] || 0);
}, 0);
const bonusText = bonus => Object.entries(bonus).map(([key, value]) => ['work', 'speed', 'build'].includes(key) ? `+${Math.round(value * 100)}% ${BONUS_LABELS[key]}` : `+${value} ${BONUS_LABELS[key]}`).join(', ');
const carryLoad = actor => CARRY_LOAD + gearBonus(actor, 'carry') + Math.floor(skillOf(actor, 'hauling') / 25);
// Bônus de trabalho de quem está escalado nesta construção: equipamento e metade do bônus da profissão.
function crewWork(building) {
  return state.workers.filter(worker => worker.alive && worker.buildSite == null && (worker.workplace ? worker.workplace.site : worker.jobSite) === building.site)
    .reduce((sum, worker) => sum + gearBonus(worker, 'work') + tradeWorkBonus(worker, hookOf(building)) / 2, 0);
}
const armoryTier = () => state.buildings.filter(building => building.type === 'armory' && isDone(building))
  .reduce((best, armory) => Math.max(best, ARMORY_LEVELS[Math.min(armory.level || 0, ARMORY_LEVELS.length - 1)].tierCap), 0);
function bestPurchase(actor, tier) {
  const order = actor.className === 'Peasant' ? PEASANT_PRIORITY.filter(slot => actor.profession || slot !== 'tool') : (SLOT_PRIORITY[actor.trait] || SLOT_PRIORITY.default);
  for (const slot of order) {
    const current = gearItem(gearOf(actor)[slot]);
    const options = Object.entries(GEAR).filter(([, item]) => fitsSlot(item, actor, slot) && item.tier <= tier && item.price <= purseOf(actor) && item.tier > (current?.tier || 0));
    if (options.length) {
      const [id, item] = options.sort((a, b) => b[1].tier - a[1].tier)[0];
      return { slot, id, item };
    }
  }
  return null;
}
// Uma vez por hora de jogo: quem está livre pode passar no Arsenal. No máximo uma compra por dia por pessoa.
function shopAtArmory() {
  const tier = armoryTier();
  if (!tier) return;
  const day = Math.floor(state.minutes / 1440);
  [...state.heroes, ...state.workers].filter(actor => actor.alive && actor.buildSite == null && !(actor.drinkUntil > state.minutes) && actor.lastShopDay !== day).forEach(actor => {
    if (Math.random() > 0.3) return;
    const buy = bestPurchase(actor, tier);
    if (!buy) return;
    actor.purse = purseOf(actor) - buy.item.price;
    gearOf(actor)[buy.slot] = buy.id;
    actor.lastShopDay = day;
    state.gold += buy.item.price;
    state.armorySales = (state.armorySales || 0) + buy.item.price;
    say(`${actor.name} comprou ${buy.item.name} no Arsenal por ${buy.item.price} de ouro.`, `${actor.name} saiu do Arsenal com ${buy.item.name}. E com a bolsa mais leve.`);
  });
}
function upgradeArmory(site) {
  const armory = state.buildings.find(building => building.site === site && building.type === 'armory' && isDone(building));
  const nextIndex = (armory?.level || 0) + 1;
  const next = armory && ARMORY_LEVELS[nextIndex];
  if (!next) return;
  if (state.gold < next.gold || state.timber < next.wood) { say('Falta ouro ou madeira. O ferreiro vai continuar vendendo sucata.'); return; }
  state.gold -= next.gold;
  state.timber -= next.wood;
  armory.armoryUpgradeTo = nextIndex + 1;
  armory.underConstruction = true;
  armory.progress = 0;
  armory.builderId = null;
  assignBuilders();
  renderSites(); renderPanel(); updateHud();
  const worker = armory.builderId != null ? findActor(armory.builderId) : null;
  say(worker ? `Reforma do Arsenal para o nível ${nextIndex + 1} iniciada. ${worker.name} vai trabalhar nela.` : `Reforma do Arsenal para o nível ${nextIndex + 1} iniciada. Falta um camponês livre para a obra.`);
}
function armoryBoxHTML(armory) {
  const level = armory.level || 0;
  const next = ARMORY_LEVELS[level + 1];
  const tierCap = ARMORY_LEVELS[Math.min(level, ARMORY_LEVELS.length - 1)].tierCap;
  const forSale = Object.values(GEAR).filter(item => item.tier <= tierCap).sort((a, b) => (a.peasant === b.peasant ? 0 : a.peasant ? 1 : -1) || a.tier - b.tier || a.price - b.price);
  return `<section class="inspector-section"><h4>Melhoria</h4>
    ${next ? `<button type="button" class="hut-upgrade" data-armory-upgrade="${armory.site}">Nível ${level + 2} · até tier ${next.tierCap} · ${costHTML(next.gold, next.wood)}</button>` : '<p class="storage-items">Nível máximo. O ferreiro já não dorme.</p>'}</section>
    <section class="inspector-section"><h4>Loja do monarca · até tier ${tierCap}</h4>${monarchShopHTML(tierCap)}</section>
    <details class="inspector-section"><summary>À venda para heróis e camponeses · ${forSale.length} itens</summary>
    <p class="storage-items">Heróis e camponeses compram sozinhos, com o próprio ouro. O ouro vai pro cofre.</p>
    <ul class="shop-list">${forSale.map(item => `<li>${gearIconHTML(item, item.classes?.find(className => HERO_WEAPON_ART_CLASS[className]), item.name)}<span><b>${item.name}</b><small>${item.peasant ? (item.prof ? item.prof.map(id => PROFESSIONS[id].name).join(', ') : 'Qualquer camponês') : item.classes ? item.classes.join(', ') : 'Qualquer herói'} · ${bonusText(item.bonus)}</small></span><em>${item.price}</em></li>`).join('')}</ul></details>`;
}
function gearHTML(actor) {
  if (actor.className === 'Bandit' || actor.className === 'Monster') return '';
  return `<div class="sheet-section-label">EQUIPAMENTO</div><div class="equip-grid">${slotsFor(actor).map(({ slot, label }) => {
    const item = gearItem(gearOf(actor)[slot]);
    const condition = item?.crafted ? `<em>Estrutura ${formatMass(item.structuralCondition)}/${formatMass(item.structuralMax)}${item.sharp ? ` · gume ${formatMass(item.edgeCondition)}/${formatMass(item.edgeMax)}` : ''}</em>` : '';
    return `<div class="equip-slot${item ? '' : ' empty'}">${gearIconHTML(item, actor.className, label)}<span><small>${label}</small><b>${item ? item.name : 'Vazio'}</b><em>${item ? `${bonusText(item.bonus)}. ${item.about}` : ''}</em>${condition}</span></div>`;
  }).join('')}</div>${actor.className !== 'Peasant' ? craftedWeaponOptionsHTML(actor) : ''}`;
}
function craftedWeaponOptionsHTML(actor) {
  const candidates = (state.items || []).map(id => craftedWeapon(id)).filter(item => item && fitsSlot(item, actor, 'weapon'));
  if (!candidates.length) return '';
  return `<section class="inspector-section"><h4>Armas forjadas no depósito</h4><div class="pit-crew">${candidates.map(item => `<button type="button" class="pit-worker" data-action="equip-crafted-weapon" data-actor="${actor.id}" data-item="${item.id}"><b>${item.name}</b><small>${WEAPON_MATERIAL_LABELS[item.materialId]} · estrutura ${formatMass(item.structuralCondition)}/${formatMass(item.structuralMax)}${item.sharp ? ` · gume ${formatMass(item.edgeCondition)}/${formatMass(item.edgeMax)}` : ''} · ${bonusText(item.bonus)}</small></button>`).join('')}</div></section>`;
}
function equipCraftedWeapon(actorId, itemId) {
  const actor = findActor(actorId), item = craftedWeapon(itemId);
  if (!actor || actor.className === 'Peasant' || !item || !fitsSlot(item, actor, 'weapon') || !(state.items || []).includes(itemId)) return;
  const previousId = gearOf(actor).weapon;
  state.items.splice(state.items.indexOf(itemId), 1);
  item.location = actor.id;
  const previous = craftedWeapon(previousId);
  if (previous && !(state.items || []).includes(previousId)) { state.items.push(previousId); previous.location = 'inventory'; }
  gearOf(actor).weapon = itemId;
  say(`${actor.name} equipou ${item.name}.`);
  updateHud(); renderPanel(); renderMenu();
}

// Tabelas de atributos de combate: [Vida, Ataque, Magia, Defesa, Destreza] no nível 1; mov = Movimento.
// Todas as classes crescem no mesmo ritmo (+16% do valor inicial por nível, até o nível 30). Assim o equilíbrio
// entre classes é o mesmo no nível 1 e no 30, e só a diferença de nível muda quem leva vantagem.
// Referência contra um Salteador do mesmo nível (golpes para matá-lo / golpes que ele precisa):
// Warrior 3/12, Heavy Knight 4/18, Paladin 4/13, Lancer 3/8, Archer 3/5, Assassin 2/5, Mage 2/4, Healer 7/7, Camponês −/5.
const STAT_GROWTH = 0.16;
const statTable = (mov, base) => ({
  mov, stats: Array.from({ length: 30 }, (_, i) => base.map(value => Math.round(value * (1 + STAT_GROWTH * i)))),
});
const EMBER_CLASSES = {
  swordsman: statTable(5, [40, 11, 0, 10, 4]),
  moonwardenWerewolf: statTable(5, [49, 14, 0, 12, 5]),
  moonwardenGreatWolf: statTable(6, [58, 22, 0, 14, 8]),
  vanguard: { mov: 5, stats: [
    [34, 9, 5, 6, 3], [38, 11, 6, 8, 4], [42, 13, 7, 10, 5], [46, 15, 8, 12, 6], [50, 17, 9, 14, 7],
    [54, 19, 10, 16, 8], [58, 21, 11, 18, 9], [62, 23, 12, 20, 10], [66, 25, 13, 22, 11], [70, 27, 14, 24, 12],
    [74, 29, 15, 26, 13], [78, 31, 16, 28, 14], [82, 33, 17, 30, 15], [86, 35, 18, 32, 16], [90, 37, 19, 34, 17],
    [94, 39, 20, 36, 18], [98, 41, 21, 38, 19], [102, 43, 22, 40, 20], [106, 45, 23, 42, 21], [110, 47, 24, 44, 22],
    [114, 49, 25, 46, 23], [118, 51, 26, 48, 24], [122, 53, 27, 50, 25], [126, 55, 28, 52, 26], [130, 57, 29, 54, 27],
    [134, 59, 30, 56, 28], [138, 61, 31, 58, 29], [142, 63, 32, 60, 30], [146, 65, 33, 62, 31], [150, 67, 34, 64, 32],
  ] },
  archer: statTable(6, [26, 11, 0, 4, 8]),
  lancer: statTable(5, [34, 11, 0, 7, 5]),
  mage: statTable(5, [22, 3, 14, 3, 6]),
  conjurer: statTable(5, [20, 3, 12, 2, 8]),
  healer: statTable(5, [30, 6, 8, 6, 6]),
  paladin: statTable(5, [42, 9, 6, 11, 4]),
  heavyKnight: statTable(4, [50, 10, 0, 15, 3]),
  ranger: statTable(6, [28, 12, 0, 5, 9]),
  assassin: statTable(6, [26, 15, 0, 4, 12]),
  templar: statTable(5, [36, 11, 4, 9, 4]),
  elementalist: statTable(5, [24, 3, 16, 3, 7]),
  warlock: statTable(5, [24, 3, 17, 3, 6]),
  sorcerer: statTable(5, [22, 3, 14, 3, 9]),
  necromancer: statTable(5, [24, 3, 14, 4, 7]),
  bishop: statTable(5, [30, 5, 11, 6, 7]),
  cleric: statTable(5, [36, 9, 8, 9, 5]),
  peasant1: statTable(4, [24, 4, 0, 4, 2]),
  villagerF1: statTable(4, [22, 3, 0, 3, 5]),
  captain: statTable(4, [60, 11, 0, 7, 4]),
  rogue: statTable(6, [25, 8, 0, 3, 5]),
  brigandCrossbowman: statTable(3, [20, 9, 0, 2, 6]),
};

// Cada classe do jogo usa a tabela da classe equivalente do Ember. Sentinel não existe no Ember e usa a do Lanceiro;
// o herdeiro usa a do Capitão; camponeses usam Camponês e Aldeã.
const CLASS_TABLE = {
  MoonWarden: 'swordsman', 'Lobisomem de machado e escudo': 'moonwardenWerewolf', 'Grande Lobo': 'moonwardenGreatWolf',
  Vanguard: 'vanguard', Swordsmaster: 'vanguard', WarBringer: 'vanguard',
  Warrior: 'swordsman', Archer: 'archer', Lancer: 'lancer', 'Black Mage': 'mage', Conjurer: 'conjurer', Healer: 'healer',
  Paladin: 'paladin', 'Heavy Knight': 'heavyKnight', Ranger: 'ranger', Assassin: 'assassin', Sentinel: 'lancer', Templar: 'templar',
  Elementalist: 'elementalist', Warlock: 'warlock', Sorcerer: 'sorcerer', Necromancer: 'necromancer', Bishop: 'bishop', Cleric: 'cleric',
};
const isFemalePeasant = actor => actor.spriteId === 'female-peasant';
function tableFor(actor) {
  if (actor === 'heir') return EMBER_CLASSES.captain;
  if (actor.className === 'Bandit') return EMBER_CLASSES[actor.kind === 'crossbow' ? 'brigandCrossbowman' : 'rogue'];
  if (actor.className === 'Monster') return SKELETON_TABLE;
  if (actor.className === 'Peasant') return EMBER_CLASSES[isFemalePeasant(actor) ? 'villagerF1' : 'peasant1'];
  return EMBER_CLASSES[CLASS_TABLE[actor.guildForm || actor.prestige] || CLASS_TABLE[actor.className]] || EMBER_CLASSES.swordsman;
}
function attributesAt(table, level) {
  const [hp, atk, mag, def, dex] = table.stats[Math.max(1, Math.min(HEIR_MAX_LEVEL, level)) - 1];
  return { hp, atk, mag, def, dex, mov: table.mov };
}
function attributesHTML(attrs, actor = null) {
  const rows = [['Vida', attrs.hp, 'hp'], ['Ataque', attrs.atk, 'atk'], ['Magia', attrs.mag, 'mag'], ['Defesa', attrs.def, 'def'], ['Destreza', attrs.dex, 'dex'], ['Movimento', attrs.mov, null]];
  return `<div class="sheet-section-label">ATRIBUTOS</div><dl class="attr-grid">${rows.map(([label, value, key]) => { const extra = actor && key ? statBonus(actor, key) : 0; return `<div><dt>${label}</dt><dd>${value + extra}${extra ? `<small class="attr-bonus">+${extra}</small>` : ''}</dd></div>`; }).join('')}</dl>`;
}
// Atributos-base do Monarca. Depois do nível 1, o jogador recebe pontos para distribuir.
const HEIR_STATS = [
  { key: 'CHA', name: 'Carisma', base: 8, pointsValue: 1, effect: '+0,5% de potência das auras e +0,5 MP máximo por ponto.' },
  { key: 'MAG', name: 'Magia', base: 10, pointsValue: 1, effect: 'Aumenta o dano das magias conforme o % de MAG de cada magia.' },
  { key: 'INT', name: 'Inteligência', base: 9, pointsValue: 1, effect: 'Aumenta a regeneração de MP e concede +0,5 MP máximo por ponto.' },
  { key: 'WIS', name: 'Sabedoria', base: 7, pointsValue: 1, effect: 'Aumenta a regeneração de MP e concede +0,5 MP máximo por ponto.' },
  { key: 'RES', name: 'Resistência', base: 10, pointsValue: 1, effect: '+3 de vida ao casarão por ponto.' },
];
// Valor final de um atributo: base + pontos distribuídos + equipamento.
function heirStat(key) {
  const stat = HEIR_STATS.find(item => item.key === key);
  return stat ? stat.base + (state.heirAttributes?.[key] || 0) * stat.pointsValue + monarchBonus(key) : 0;
}
function heirAttributesHTML() {
  return `<div class="sheet-section-label">ATRIBUTOS · ${state.heirAttributePoints || 0} PONTOS PARA DISTRIBUIR</div><dl class="attr-grid heir-attrs">${HEIR_STATS.map(stat => { const spent = state.heirAttributes?.[stat.key] || 0, base = stat.base + spent * stat.pointsValue, extra = monarchBonus(stat.key), total = base + extra; return `<div title="${stat.name}: base ${stat.base} + ${spent} ponto(s)${stat.pointsValue !== 1 ? ` × ${stat.pointsValue}` : ''}${extra ? ` + ${extra} do equipamento` : ''}. ${stat.effect}"><dt>${stat.key}</dt><dd>${total}${extra ? `<i>+${extra}</i>` : ''}</dd><small>${stat.name} · ${spent} distribuídos</small><div class="heir-stat-controls"><button type="button" data-action="heir-stat-down" data-stat="${stat.key}" aria-label="Retirar 1 ponto de ${stat.name}" ${spent ? '' : 'disabled'}>−</button><button type="button" data-action="heir-stat-up" data-stat="${stat.key}" aria-label="Adicionar 1 ponto a ${stat.name}" ${state.heirAttributePoints ? '' : 'disabled'}>+</button></div></div>`; }).join('')}</dl>`;
}

function adjustHeirAttribute(key, amount) {
  const stat = HEIR_STATS.find(item => item.key === key);
  if (!stat || !Number.isInteger(amount) || Math.abs(amount) !== 1) return;
  const spent = state.heirAttributes?.[key] || 0;
  if (amount > 0) {
    if ((state.heirAttributePoints || 0) < 1) return;
    state.heirAttributes[key] = spent + 1;
    state.heirAttributePoints--;
  } else {
    if (spent < 1) return;
    state.heirAttributes[key] = spent - 1;
    state.heirAttributePoints = (state.heirAttributePoints || 0) + 1;
  }
  window.Monarch?.attributesChanged();
  if (game.slot) writeSave(game.slot);
  renderMenu();
}

function levelHTML(level, xp) {
  const maxed = level >= HEIR_MAX_LEVEL;
  return `<div class="sheet-level">
    <div class="sheet-level-head"><b>Nível ${level}</b><span>de ${HEIR_MAX_LEVEL}</span><small>${maxed ? 'Nível máximo. O tédio também.' : `${xp} / ${xpToNext(level)} XP`}</small></div>
    <div class="xp-bar"><i style="width:${maxed ? 100 : Math.round(xp / xpToNext(level) * 100)}%"></i></div></div>`;
}
// Experiência de heróis e camponeses, na mesma regra do Ember (1 a 30).
// O herdeiro fica com uma parte do XP de cada súdito (herói ou camponês do domínio), sem tirar nada dele.
const HEIR_SUBJECT_SHARE = 0.1;
function gainActorXp(actor, amount) {
  actor.level = actor.level || 1;
  actor.xp = actor.xp || 0;
  if (actor.level >= HEIR_MAX_LEVEL) return 0;
  if (state.workers.includes(actor) || state.heroes.includes(actor)) gainHeirXp(amount * HEIR_SUBJECT_SHARE);
  actor.xp += amount;
  let gained = 0;
  while (actor.level < HEIR_MAX_LEVEL && actor.xp >= xpToNext(actor.level)) { actor.xp -= xpToNext(actor.level); actor.level++; gained++; }
  if (actor.level >= HEIR_MAX_LEVEL) actor.xp = 0;
  return gained;
}
const PEASANT_EPITHETS = ['Três Dedos', 'Sem Sorte', 'do Brejo', 'da Vala', 'de Ninguém', 'do Poço', 'Olho Torto', 'Dente Mole'];
function peasantName(female) {
  const firsts = FIRST_NAMES.filter((_, i) => (i % 2 === 1) === female);
  const taken = new Set(state.workers.map(worker => worker.name));
  for (let i = 0; i < 40; i++) { const name = `${pick(firsts)} ${pick(PEASANT_EPITHETS)}`; if (!taken.has(name)) return name; }
  return `${pick(firsts)} ${pick(PEASANT_EPITHETS)} ${state.workers.length + 1}`;
}
const actorLabel = actor => actor.className === 'Wildlife' ? `${WILDLIFE_NAMES[actor.kind] || 'Animal selvagem'}` : actor.className === 'Monster' ? `Caveira · nível ${actor.level || 1}` : actor.className === 'Bandit' ? (actor.kind === 'crossbow' ? 'Ladrão de besta' : 'Ladrão de espada') : actor.className === 'Peasant' ? (isFemalePeasant(actor) ? 'Camponesa' : 'Camponês') : `${actor.className}${actor.guildForm && actor.guildForm !== actor.className ? ` · ${actor.guildForm}` : actor.prestige ? ` · ${actor.prestige}` : ''}`;
const findActor = id => [...state.heroes, ...state.workers, ...(state.bandits || [])].find(actor => String(actor.id) === String(id)) || null;
function actorOrigin(actor) {
  if (actor.className === 'Bandit') return 'Veio pela estrada velha atrás do que não é dele.';
  if (actor.className === 'Monster') return 'Levantou do cemitério dos Wilds. Ninguém sabe o que quer. Ela também não.';
  if (actor.className === 'Peasant' && homeOf(actor)) return 'Veio morar no casebre. Não tinha pra onde ir mesmo.';
  if (actor.className === 'Peasant') return actor.workplace ? `Veio com ${actor.workplace.type === 'mill' ? 'o moinho' : 'a fazenda'}. Não tinha pra onde ir mesmo.` : 'Veio junto com o casarão, como as dívidas.';
  if (actor.home === 'manor') return 'Veio junto com o casarão. Ninguém lembra de ter contratado.';
  return 'Chegou atraído pelas construções do domínio. Ninguém perguntou de onde.';
}


// ==========================================================================
// Comida e felicidade, no estilo Lords of the Realm: estoque por alimento, ração escolhida
// e cardápio servido. Carne e variedade alegram; ração pequena e fome entristecem.
// ==========================================================================
// feeds: quanto cada unidade alimenta. O pão é a base (1×): 1 de pão mata 1 de fome.
// baked: sai da Padaria, não de fazenda nem moinho.
const FOOD_TYPES = [
  { id: 'flour', name: 'Farinha', art: 'mill-body', mood: 2, feeds: 1, note: 'Sai do moinho. Eles que cozinhem em casa.' },
  { id: 'wheat', name: 'Trigo', art: 'farm-wheat', raw: true, feeds: 0, note: 'Cru não serve pra nada. Vai pro moinho virar farinha.' },
  { id: 'garlic', name: 'Alho', art: 'farm-garlic', mood: 1, feeds: 1, note: 'Afasta strigoi. E todo o resto também.' },
  { id: 'pork', name: 'Suínos', art: 'farm-bacon', raw: true, feeds: 0, note: 'Matéria-prima do açougue.' },
  { id: 'chicken', name: 'Carne de frango', art: 'farm-eggs', raw: true, feeds: 0, note: 'Produção secundária do galinheiro; matéria-prima do açougue.' },
  { id: 'beef', name: 'Carne de vaca', art: 'farm-milk', raw: true, feeds: 0, note: 'Produção secundária da fazenda de leite; matéria-prima do açougue.' },
  { id: 'bacon', name: 'Bacon', art: 'farm-bacon', icon: 'icon-market-bacon-photo-v01.png', meat: true, mood: 2, feeds: 1, note: 'Processado no açougue a partir de suínos.' },
  { id: 'eggs', name: 'Ovos', art: 'farm-eggs', mood: 1, feeds: 2, note: 'Variedade. Os camponeses notam.' },
  { id: 'milk', name: 'Leite', art: 'farm-milk', mood: 1, feeds: 1, note: 'Variedade. Às vezes azeda antes.' },
  { id: 'bread', name: 'Pão', art: BAKED_ART.bread, baked: true, mood: 2, feeds: 1, note: 'Da padaria. Duro como a vida, mas é pão.' },
  { id: 'cheese', name: 'Queijo', art: BAKED_ART.cheese, baked: true, mood: 2, feeds: 2, note: 'Da padaria. Farinha e leite que resolveram durar mais.' },
  { id: 'quiche', name: 'Quiche Lorraine', art: BAKED_ART.quiche, baked: true, meat: true, mood: 3, feeds: 4, note: 'Da padaria. Comida de nobre na mesa de servo. Ninguém conta pro Barão.' },
  { id: 'sausage', name: 'Linguiça', art: 'sausage', meat: true, mood: 2, feeds: 1, note: 'Processada no açougue.' },
  { id: 'nuggets', name: 'Chicken Nuggets', art: 'chicken-nuggets', meat: true, mood: 2, feeds: 1, note: 'Processado no açougue.' },
  { id: 'wings', name: 'Buffalo Wings', art: 'buffalo-wings', meat: true, mood: 2, feeds: 1, note: 'Processadas no açougue.' },
  { id: 'hamburger', name: 'Hambúrguer', art: 'hamburger', meat: true, mood: 2, feeds: 1, note: 'Processado no açougue.' },
  { id: 'steak', name: 'Filé', art: 'steak', meat: true, mood: 2, feeds: 1, note: 'Processado no açougue.' },
  { id: 'parmesanSteak', name: 'Filé à Parmegiana', art: 'parmesan-steak', baked: true, meat: true, mood: 3, feeds: 3, note: 'Filé com queijo, preparado no açougue.' },
  { id: 'boarMeat', name: 'Carne de Javali', art: 'boar-meat', icon: 'boar-meat-v01.png', raw: true, feeds: 0, note: 'Carne crua recolhida dos javalis; vai para o açougue.' },
  { id: 'elkMeat', name: 'Carne de Alce', art: 'elk-meat', icon: 'elk-meat-v01.png', raw: true, feeds: 0, note: 'Carne crua recolhida dos alces; vai para o açougue.' },
  { id: 'roastBoar', name: 'Javali Assado', art: 'roast-boar', icon: 'roast-boar-v01.png', meat: true, mood: 2, feeds: 2, note: 'Carne de javali assada no açougue.' },
  { id: 'garlicBoar', name: 'Javali ao Alho', art: 'garlic-boar', icon: 'garlic-boar-v01.png', meat: true, mood: 3, feeds: 4, note: 'Javali assado com alho no açougue.' },
  { id: 'roastElk', name: 'Alce Assado', art: 'roast-elk', icon: 'roast-elk-v01.png', meat: true, mood: 2, feeds: 2, note: 'Carne de alce assada no açougue.' },
  { id: 'elkStew', name: 'Ensopado de Alce', art: 'elk-stew', icon: 'elk-stew-v01.png', meat: true, mood: 3, feeds: 4, note: 'Ensopado de alce com leite e batatas, feito no açougue.' },
  { id: 'potato', name: 'Batatas', art: 'farm-vegetables', icon: 'potato-v01.png', mood: 1, feeds: 2, note: 'Colheita da Fazenda de Hortaliças; também ingrediente do Ensopado de Alce.' },
  { id: 'carrot', name: 'Cenouras', art: 'farm-vegetables', icon: 'carrot-v01.png', mood: 1, feeds: 1, note: 'Colheita da Fazenda de Hortaliças.' },
  { id: 'tomato', name: 'Tomates', art: 'farm-vegetables', icon: 'tomato-v01.png', mood: 1, feeds: 1, note: 'Colheita da Fazenda de Hortaliças.' },
  { id: 'fish', name: 'Truta', art: 'fish', meat: true, mood: 2, feeds: 2, note: 'Alimento direto da pesca; vale 2× e não passa pelo açougue.' },
];
const foodIcon = food => food.icon || `${food.art}-v01.webp`;
const foodFeeds = id => FOOD_TYPES.find(food => food.id === id)?.feeds ?? 1;
const RATIONS = [
  { label: 'Nenhuma', mult: 0, mood: -20 }, { label: 'Meia', mult: 0.5, mood: -8 }, { label: 'Normal', mult: 1, mood: 0 },
  { label: 'Dobrada', mult: 2, mood: 5 }, { label: 'Tripla', mult: 3, mood: 10 },
];
// Imposto: 0% alegra, 10% é neutro, 20% entristece.
const TAX_MAX = 20;
const taxMood = () => Math.round(10 - (state.taxPercent ?? 10));
// Felicidade muda uma vez por semana, à meia-noite do 7º dia: anda até MOOD_WEEKLY_STEP pontos
// na direção da média da semana (50 + efeitos), no estilo de turno do Lords of the Realm.
const MOOD_WEEKLY_STEP = 10;
const moodWeekAverage = () => (state.moodTime > 0 ? state.moodSum / state.moodTime : state.moodTarget ?? state.happiness);
// Calendário: usa o mesmo relógio (state.minutes) e a mesma semana de 7 dias da felicidade.
// Um mês tem 4 semanas e um ano tem 12 meses. O jogo começa no dia 1 do mês 1 do ano 1.
const DAYS_PER_WEEK = 7;
const WEEKS_PER_MONTH = 4;
const MONTHS_PER_YEAR = 12;
const DAYS_PER_MONTH = DAYS_PER_WEEK * WEEKS_PER_MONTH;
const DAYS_PER_YEAR = DAYS_PER_MONTH * MONTHS_PER_YEAR;
function gameDate(minutes = state.minutes) {
  const days = Math.floor(minutes / 1440);
  return { day: days % DAYS_PER_MONTH + 1, month: Math.floor(days / DAYS_PER_MONTH) % MONTHS_PER_YEAR + 1, year: Math.floor(days / DAYS_PER_YEAR) + 1 };
}
const dateLabel = (minutes = state.minutes) => { const d = gameDate(minutes); return `Dia ${d.day} · Mês ${d.month} · Ano ${d.year}`; };
// Período do dia: madrugada (0h–6h), manhã (6h–12h), tarde (12h–18h), noite (18h–24h).
const DAY_PERIODS = ['Madrugada', 'Manhã', 'Tarde', 'Noite'];
const dayPeriod = (minutes = state.minutes) => DAY_PERIODS[Math.floor((minutes % 1440) / 360)];
// O herdeiro faz aniversário a cada virada de ano: a idade da escolha mais os anos já passados.
const heirAge = (heir, minutes = state.minutes) => heir.age + gameDate(minutes).year - 1;
const BIRTHDAY_MSG = [
  (n, a) => `Ano novo. ${n} fez ${a} anos. A festa foi cancelada por falta de convidados.`,
  (n, a) => `Ano novo. ${n} fez ${a} anos. Os credores mandaram um cartão. Com juros.`,
  (n, a) => `Ano novo. ${n} fez ${a} anos. O espelho foi o primeiro a notar.`,
];
function newYear() {
  const heir = heirById(state.heir);
  if (heir) say(pick(BIRTHDAY_MSG)(heir.name.split(' ')[0], heirAge(heir)), `${heir.name.split(' ')[0]} fez ${heirAge(heir)} anos.`);
}
const nextMoodDay = () => (Math.floor(Math.floor(state.minutes / 1440) / 7) + 1) * 7 + 1;
function applyWeeklyMood() {
  const average = moodWeekAverage();
  const change = Math.round(Math.max(-MOOD_WEEKLY_STEP, Math.min(MOOD_WEEKLY_STEP, average - state.happiness)));
  state.happiness = Math.max(0, Math.min(100, state.happiness + change));
  state.moodSum = 0;
  state.moodTime = 0;
  say(change ? `Fim da semana: felicidade ${change > 0 ? '+' : '−'}${Math.abs(change)}, agora ${Math.round(state.happiness)}. ${change > 0 ? 'Ninguém agradeceu.' : 'Ninguém se surpreendeu.'}` : `Fim da semana: a felicidade ficou em ${Math.round(state.happiness)}.`);
  updateHud();
  if (state.tab === 'food') renderPanel();
}
const emptyFood = () => ({ wheat: 0, flour: 0, garlic: 0, bacon: 0, pork: 0, chicken: 0, beef: 0, eggs: 0, milk: 0, bread: 0, cheese: 0, quiche: 0, sausage: 0, nuggets: 0, wings: 0, hamburger: 0, steak: 0, parmesanSteak: 0, boarMeat: 0, elkMeat: 0, roastBoar: 0, garlicBoar: 0, roastElk: 0, elkStew: 0, potato: 0, carrot: 0, tomato: 0, fish: 0 });
// O casarão guarda 500 unidades. Cada Depósito de Provisões construído soma 30, e amplia para 45, 70 e 100.
// Guardam comida, armas e itens; madeira e ouro ficam fora.
const MANOR_STORAGE = 500;
const STORAGE_LEVELS = [
  { cap: 30, gold: 0, wood: 0 }, { cap: 45, gold: 160, wood: 12 }, { cap: 70, gold: 280, wood: 20 }, { cap: 100, gold: 440, wood: 32 },
];
const depots = () => state.buildings.filter(building => building.type === 'depot' && isDone(building));
const depotCap = depot => STORAGE_LEVELS[depot.level || 0].cap;
const storageCap = () => MANOR_STORAGE + depots().reduce((sum, depot) => sum + depotCap(depot), 0);
const reservedWorkshopSpace = () => (state.buildings || []).reduce((sum, building) => sum + Object.entries(building.metalJob?.output || {}).reduce((n, [id, qty]) => n + qty / 100, 0), 0);
const storedUnits = () => totalFood() + (state.items?.length || 0) + remainsCount() + (state.stone || 0) + (state.marble || 0) + metalStorageUnits() + (state.charcoal || 0) / 100 + reservedWorkshopSpace() + quarryMaterialMass() + (state.buildings || []).reduce((sum, pit) => sum + (pit.type === 'quarry' ? (pit.stockStone || 0) + (pit.stockMarble || 0) + (pit.sharedCart?.cargo ? pit.sharedCart.cargo.massKg || 0 : 0) : 0), 0);
function upgradeDepot(site) {
  const depot = state.buildings.find(building => building.site === site && building.type === 'depot');
  const next = depot && STORAGE_LEVELS[(depot.level || 0) + 1];
  if (!next || depot.underConstruction) return;
  if (state.gold < next.gold || state.timber < next.wood) { say('Falta ouro ou madeira. A comida vai continuar empilhada no corredor.'); return; }
  state.gold -= next.gold;
  state.timber -= next.wood;
  startUpgrade(depot, { kind: 'depot' }, `Ampliação do Depósito para ${next.cap} unidades`);
}
function depotBoxHTML(depot) {
  const level = depot.level || 0;
  const next = STORAGE_LEVELS[level + 1];
  return `<section class="inspector-section"><h4>Ampliação</h4>
    ${next ? `<button type="button" class="hut-upgrade" data-depot-upgrade="${depot.site}">${next.cap} unidades · ${costHTML(next.gold, next.wood)}</button>` : '<p class="storage-items">Tamanho máximo. Agora só falta ter o que guardar.</p>'}</section>`;
}
const totalFood = () => FOOD_TYPES.reduce((sum, food) => sum + (state.food?.[food.id] || 0), 0);
const eaters = () => state.workers.filter(worker => worker.alive).length;

function foodProduction() {
  const prod = emptyFood();
  state.buildings.filter(isDone).forEach(building => {
    if (building.type === 'farm') {
      prod[farmFoodId(building)] += farmOutput(building);
      const secondary = farmType(building.produce).secondary;
      if (secondary) prod[secondary] += farmSecondaryRate(building);
    }
  });
  return prod;
}
// Moinhos prontos: quanto trigo conseguem moer por segundo, somados.
const millCapacity = () => state.buildings.filter(building => building.type === 'mill' && isDone(building)).reduce((sum, mill) => sum + millRate(mill), 0);
// Cada moinho mói o trigo que os camponeses trouxeram; a farinha espera no moinho até alguém levar ao depósito.
function grindWheat(dt) {
  state.buildings.filter(building => building.type === 'mill' && isDone(building)).forEach(mill => {
    const qty = Math.min(millRate(mill) * (1 + crewWork(mill)) * dt, mill.input || 0, Math.max(0, industryCap(mill) - (mill.stock || 0)));
    if (qty <= 0) return;
    mill.input -= qty;
    mill.stock = (mill.stock || 0) + qty;
  });
}
// Padarias prontas: quanto de cada produto conseguem assar por segundo, somados.
const bakingCapacity = id => state.buildings.filter(building => building.type === 'bakery' && isDone(building) && bakeryActiveRecipe(building).id === id).reduce((sum, bakery) => sum + bakeRate(bakery), 0);
// A fornada só sai com todos os ingredientes no forno; a fornada antiga precisa sair antes de começar outra.
function bakeryCanBake(bakery) {
  const recipe = bakeryActiveRecipe(bakery);
  if ((bakery.stock || 0) > 1e-6 && bakery.stockFood && bakery.stockFood !== recipe.id) return false;
  return recipe.needs.every(id => (bakery.pantry?.[id] || 0) > 1e-6) && (bakery.stock || 0) < industryCap(bakery);
}
function bakeFood(dt) {
  state.buildings.filter(building => building.type === 'bakery' && isDone(building)).forEach(bakery => {
    if (!bakeryCanBake(bakery)) return;
    const recipe = bakeryActiveRecipe(bakery);
    const pantry = bakery.pantry;
    const qty = Math.min(bakeRate(bakery) * (1 + crewWork(bakery)) * dt, ...recipe.needs.map(id => pantry[id]), industryCap(bakery) - (bakery.stock || 0));
    if (qty <= 0) return;
    recipe.needs.forEach(id => { pantry[id] -= qty; });
    bakery.stockFood = recipe.id;
    bakery.stock = (bakery.stock || 0) + qty;
  });
}
function butcherCanProcess(butcher) {
  const recipe = butcherActiveRecipe(butcher);
  if ((butcher.stock || 0) > 1e-6 && butcher.stockFood && butcher.stockFood !== recipe.id) return false;
  return recipe.needs.every(id => (butcher.pantry?.[id] || 0) >= 1e-6) && (butcher.stock || 0) < industryCap(butcher);
}
function processButcher(dt) {
  state.buildings.filter(building => building.type === 'butcher' && isDone(building)).forEach(butcher => {
    if (!butcherCanProcess(butcher)) return;
    const recipe = butcherActiveRecipe(butcher), pantry = butcher.pantry || {};
    const qty = Math.min(butcherRate(butcher) * (1 + crewWork(butcher)) * dt, ...recipe.needs.map(id => pantry[id]), industryCap(butcher) - (butcher.stock || 0));
    if (qty <= 0) return;
    recipe.needs.forEach(id => { pantry[id] -= qty; });
    butcher.stockFood = recipe.id;
    butcher.stock = (butcher.stock || 0) + qty;
  });
}
function processMetalworks(dt) {
  state.buildings.filter(building => building.type === 'blacksmith' && isDone(building)).forEach(building => {
    if (building.weaponJob) {
      const job = building.weaponJob, design = weaponDesign(job.patternId, job.materialId);
      job.elapsed = Math.min(job.required, job.elapsed + dt * (1 + crewWork(building)));
      building.weaponStatus = `Forjando ${design.name} · ${Math.floor(job.elapsed)}/${job.required} min`;
      if (job.elapsed < job.required || storedUnits() + 1 > storageCap() + 1e-6) return;
      const base = GEAR[design.pattern.base], bodyProfile = METAL_WEAPON_PROFILE[design.body], edgeProfile = METAL_WEAPON_PROFILE[design.edge || design.body];
      const weapon = { id: `forged-${building.site}-${state.ticks}`, name: design.name, crafted: true, slot: 'weapon', tier: 1, price: 0, classes: design.pattern.classes, bonus: { ...base.bonus }, about: `Corpo: ${METAL_NAMES[design.body]}${design.pattern.sharp ? ` · gume/ponta: ${METAL_NAMES[design.edge]}` : ''}. Forjada de ${WEAPON_MATERIAL_LABELS[design.materialId]}.`,
        patternId: design.pattern.id, materialId: design.materialId, bodyMaterial: design.body, edgeMaterial: design.edge || design.body, sharp: design.pattern.sharp,
        structuralMultiplier: bodyProfile.structural, wearMultiplier: edgeProfile.wear, structuralMax: 100 * bodyProfile.structural, structuralCondition: 100 * bodyProfile.structural,
        edgeMax: 100, edgeCondition: design.pattern.sharp ? 100 : 0, location: 'inventory' };
      state.craftedWeapons ||= []; state.craftedWeapons.push(weapon); state.items ||= []; state.items.push(weapon.id);
      building.lastWeaponOutputId = job.id; building.weaponJob = null; building.weaponStatus = `${weapon.name} entregue ao depósito`;
      say(`${weapon.name} saiu da forja e foi para o inventário.`);
      updateHud(); if (state.selectedSite === building.site) renderPanel();
      return;
    }
    if (building.repairJob) {
      const job = building.repairJob;
      job.elapsed = Math.min(job.required, job.elapsed + dt * (1 + crewWork(building)));
      building.weaponStatus = `Reparo em produção · ${Math.floor(job.elapsed)}/${job.required} min`;
      if (job.elapsed < job.required) return;
      const weapon = craftedWeapon(job.weaponId);
      if (weapon) {
        if (job.part === 'edge') weapon.edgeCondition = weapon.edgeMax;
        else weapon.structuralCondition = weapon.structuralMax;
        building.lastRepairOutputId = job.id; building.weaponStatus = `${weapon.name}: ${job.part === 'edge' ? 'gume' : 'estrutura'} reparado`;
        say(`${weapon.name}: ${job.part === 'edge' ? 'gume' : 'estrutura'} restaurado no Blacksmith.`);
      }
      building.repairJob = null;
      if (state.selectedSite === building.site) renderPanel();
      return;
    }
    if (building.weaponOrder && !building.metalJob) {
      const order = building.weaponOrder, design = weaponDesign(order.patternId, order.materialId);
      const required = design.pattern.id === 'sword' ? 120 : design.pattern.id === 'axe' || design.pattern.id === 'mace' ? 90 : 60;
      building.weaponJob = { ...order, elapsed: 0, required };
      building.weaponOrder = null;
      building.weaponStatus = `${design.name} entrou na forja`;
      if (state.selectedSite === building.site) renderPanel();
      return;
    }
    let job = building.metalJob || null;
    let recipe = job ? BLACKSMITH_RECIPES.find(item => item.id === job.recipeId) : blacksmithRecipe(building);
    if (!recipe || recipe.minLevel > producerLevel(building) + 1) return;
    if (!job) {
      const amounts = Object.entries(recipe.needs).map(([id, qty]) => [id, qty, id === 'timber' ? state.timber : state.metals[id] || 0]);
      const scale = Math.min(1, ...amounts.map(([, qty, have]) => have / qty));
      if (!(scale > 0)) { building.metalStatus = 'Aguardando matéria-prima'; return; }
      if ((state.charcoal || 0) + 1e-9 < recipe.fuel * scale) { building.metalStatus = 'Aguardando carvão vegetal'; return; }
      const inputSpace = amounts.reduce((sum, [id, qty]) => sum + qty * scale / (id === 'timber' ? 1 : 100), 0) + recipe.fuel * scale / 100;
      const outputSpace = Object.entries(recipe.makes).reduce((sum, [id, qty]) => sum + qty * scale / (id === 'charcoal' ? 100 : 100), 0);
      if (storedUnits() - inputSpace + outputSpace > storageCap() + 1e-6) { building.metalStatus = 'Depósito cheio'; return; }
      const consumedAssays = {};
      amounts.forEach(([id, qty]) => {
        if (id === 'timber') state.timber -= qty * scale;
        else {
          state.metals[id] -= qty * scale;
          if (ORE_STOCK.has(id)) consumedAssays[id] = consumeOreLots(id, qty * scale);
        }
      });
      state.charcoal = (state.charcoal || 0) - recipe.fuel * scale;
      const output = Object.fromEntries(Object.entries(recipe.makes).map(([id, qty]) => [id, qty * scale]));
      if (recipe.assay) {
        const oreId = Object.keys(recipe.needs)[0];
        const rawKg = recipe.needs[oreId] * scale;
        const assayPpm = consumedAssays[oreId] || state.oreGrades[oreId] || recipe.assay;
        output[Object.keys(recipe.makes)[0]] = rawKg * assayPpm / 1000000 * recipe.recovery;
      }
      job = building.metalJob = { id: `metal-job-${building.site}-${state.ticks}`, recipeId: recipe.id, elapsed: 0, required: recipe.work * scale, output, scale, residue: recipe.residue ? recipe.needs[Object.keys(recipe.needs)[0]] * scale : 0 };
      building.metalStatus = 'Em produção';
    }
    job.elapsed = Math.min(job.required, job.elapsed + dt * (1 + crewWork(building)));
    building.refineProgress = job.required ? job.elapsed / job.required : 0;
    if (job.elapsed >= job.required) {
      Object.entries(job.output).forEach(([id, qty]) => {
        if (id === 'charcoal') state.charcoal = (state.charcoal || 0) + qty;
        else state.metals[id] = (state.metals[id] || 0) + qty;
      });
      if (recipe.residue) state.residues[recipe.residue] = (state.residues[recipe.residue] || 0) + job.residue;
      building.lastMetalOutputId = job.id;
      building.metalJob = null; building.refineProgress = 0;
      building.metalStatus = 'Ordem concluída';
    }
  });
}
function butcherMissing(butcher) {
  const pantry = { ...(butcher.pantry || {}) };
  state.workers.forEach(worker => {
    const load = worker.alive && worker.carrying?.toButcher === butcher.site ? worker.carrying : null;
    if (load) pantry[load.food] = (pantry[load.food] || 0) + load.qty;
  });
  return butcherActiveRecipe(butcher).needs.filter(id => (pantry[id] || 0) < FOOD_LOAD && (state.food[id] || 0) >= 0.5)
    .sort((a, b) => (pantry[a] || 0) - (pantry[b] || 0))[0] || null;
}
function butcherRoutine(actor, butcher) {
  const store = nearestStorage(butcher), recipe = butcherActiveRecipe(butcher);
  if (isAt(actor, butcher)) {
    if (actor.carrying?.toButcher) {
      butcher.pantry = butcher.pantry || {};
      butcher.pantry[actor.carrying.food] = (butcher.pantry[actor.carrying.food] || 0) + actor.carrying.qty;
      actor.carrying = null;
    }
    // Mesma regra da padaria: processa até a carne acabar; sai com a carga cheia, ou com o que tiver quando a carne acabou.
    const fullLoad = foodLoad(actor);
    const canProcess = butcherCanProcess(butcher);
    if ((butcher.stock || 0) >= fullLoad - 1e-6 || ((butcher.stock || 0) > 1e-6 && !canProcess)) {
      const qty = Math.min(foodLoad(actor), butcher.stock);
      butcher.stock -= qty;
      actor.carrying = { food: butcher.stockFood || recipe.id, qty };
      actor.task = `Levando ${foodName(actor.carrying.food)} ao depósito`;
      walkTo(actor, store); return;
    }
    const need = !canProcess && butcherMissing(butcher);
    if (need) { actor.task = `Buscando ${foodName(need)} para o açougue`; walkTo(actor, store); return; }
    actor.task = butcherCanProcess(butcher) ? `Processando ${recipe.name} no açougue` : `No açougue, esperando ${recipe.needs.map(foodName).join(' e ')}`;
    if (butcherCanProcess(butcher)) gainSkill(actor, 'cooking', 0.3);
    actor.wait = 3; actor.actionTime = 3; actor.actionClock = 0; return;
  }
  if (isAt(actor, store)) {
    if (actor.carrying && !actor.carrying.toButcher) deliverLoad(actor);
    const need = !actor.carrying && butcherMissing(butcher);
    if (need) {
      const qty = Math.min(foodLoad(actor), state.food[need]); state.food[need] -= qty;
      actor.carrying = { food: need, qty, toButcher: butcher.site };
    }
  }
  if (actor.carrying && !actor.carrying.toButcher) { walkTo(actor, store); return; }
  actor.task = actor.carrying ? `Levando ${foodName(actor.carrying.food)} ao açougue` : 'Indo trabalhar no açougue';
  actor.headingToWork = true; walkTo(actor, butcher);
}
function fishingRoutine(actor, spot) {
  const mapSet = spot.mapSet || 2;
  if (actor.carrying?.food === 'fish' && !actor.carrying.toButcher) {
    if ((actor.mapId || 'city') === 'wilds') {
      actor.task = 'Voltando com peixe do riacho'; travelToMap(actor, 'city', mapSet); return;
    }
    const store = nearestStorage({ x: 42, y: 39 });
    if (isAt(actor, store)) { deliverLoad(actor); actor.task = 'Entregou o peixe'; }
    else { actor.task = 'Levando o peixe ao depósito'; walkTo(actor, store); return; }
  }
  if ((actor.mapId || 'city') !== 'wilds') { actor.task = 'Indo pescar no riacho das Wilds'; travelToMap(actor, 'wilds', mapSet); return; }
  actor.mapSet = mapSet;
  const point = { x: spot.x, y: spot.y };
  if (!isAt(actor, point)) { actor.task = `Indo a ${spot.name}`; walkTo(actor, point); return; }
  if (fishingStock(spot) >= 1) {
    const qty = Math.min(foodLoad(actor), fishingStock(spot));
    state.fishingStock[spot.site] = fishingStock(spot) - qty; actor.carrying = { food: 'fish', qty };
    gainSkill(actor, 'fishing', 0.3); actor.task = `Pescou ${format(qty)} de peixe`;
    travelToMap(actor, 'city', mapSet); return;
  }
  state.fishingStock[spot.site] = Math.min(FARM_STOCK_CAP, fishingStock(spot) + FISHING_RATE * (1 + skillOf(actor, 'fishing') / 100) * state.speed * 3);
  gainSkill(actor, 'fishing', 0.2); actor.task = 'Pescando no riacho das Wilds';
  actor.wait = 3; actor.actionTime = 3; actor.actionClock = 0;
}
function hunterRoutine(actor) {
  const rawMeat = ['boarMeat', 'elkMeat'];
  if (actor.carrying && rawMeat.includes(actor.carrying.food)) {
    if ((actor.mapId || 'city') !== 'city') { actor.task = 'Voltando com carne das Wilds'; travelToMap(actor, 'city', WILDLIFE_MAP_SET); return; }
    const store = nearestStorage({ x: 42, y: 39 });
    if (isAt(actor, store)) { deliverLoad(actor); actor.task = 'Entregou a caça no depósito'; return; }
    actor.task = 'Levando a caça ao depósito'; walkTo(actor, store); return;
  }
  if ((actor.mapId || 'city') !== 'wilds' || (actor.mapSet || 1) !== WILDLIFE_MAP_SET) { actor.task = 'Indo caçar nas Wilds'; travelToMap(actor, 'wilds', WILDLIFE_MAP_SET); return; }
  actor.mapSet = WILDLIFE_MAP_SET;
  const pile = (state.wildMeatPiles || []).filter(item => item.qty > 0 && (item.mapSet || WILDLIFE_MAP_SET) === WILDLIFE_MAP_SET).sort((a, b) => distanceTo(actor, a) - distanceTo(actor, b))[0];
  if (pile) {
    if (distanceTo(actor, pile) > 1.3) { actor.task = `Buscando ${foodName(pile.food)}`; walkTo(actor, pile); return; }
    const qty = Math.min(foodLoad(actor), pile.qty);
    pile.qty -= qty;
    actor.carrying = { food: pile.food, qty };
    if (pile.qty <= 0) state.wildMeatPiles = state.wildMeatPiles.filter(item => item !== pile);
    renderLairs();
    gainSkill(actor, 'hunting', 0.2); actor.task = `Recolheu ${format(qty)} de ${foodName(actor.carrying.food)}`;
    travelToMap(actor, 'city', WILDLIFE_MAP_SET); return;
  }
  const animal = (state.bandits || []).filter(item => item.alive && item.className === 'Wildlife' && (item.mapSet || WILDLIFE_MAP_SET) === WILDLIFE_MAP_SET).sort((a, b) => distanceTo(actor, a) - distanceTo(actor, b))[0];
  if (animal) { actor.task = `Caçando ${animal.name}`; return; }
  actor.task = 'Procurando caça nas Wilds';
  actor.wait = 2 + Math.random() * 2;
}
// O ingrediente que mais falta no forno e que o depósito tem. Conta o que outro padeiro já está trazendo.
function bakeryMissing(bakery) {
  const pantry = { ...(bakery.pantry || {}) };
  state.workers.forEach(worker => {
    const load = worker.alive && worker.carrying?.toBakery === bakery.site ? worker.carrying : null;
    if (load) pantry[load.food] = (pantry[load.food] || 0) + load.qty;
  });
  return bakeryActiveRecipe(bakery).needs
    .filter(id => (pantry[id] || 0) < FOOD_LOAD && (state.food[id] || 0) >= 0.5)
    .sort((a, b) => (pantry[a] || 0) - (pantry[b] || 0))[0] || null;
}
// Rotina do padeiro: busca um ingrediente por viagem, assa e leva a fornada ao depósito.
function bakeRoutine(actor, bakery) {
  const store = nearestStorage(bakery);
  const recipe = bakeryActiveRecipe(bakery);
  if (isAt(actor, bakery)) {
    if (actor.carrying?.toBakery) {
      bakery.pantry = bakery.pantry || {};
      bakery.pantry[actor.carrying.food] = (bakery.pantry[actor.carrying.food] || 0) + actor.carrying.qty;
      actor.carrying = null;
    }
    // Assa até a farinha acabar. Sai com o pão quando junta a carga cheia, ou quando a farinha acabou (e traz mais na volta).
    const fullLoad = foodLoad(actor);
    const canBake = bakeryCanBake(bakery);
    if ((bakery.stock || 0) >= fullLoad - 1e-6 || ((bakery.stock || 0) > 0 && !canBake)) {
      const qty = Math.min(fullLoad, bakery.stock);
      bakery.stock -= qty;
      actor.carrying = { food: bakery.stockFood || recipe.id, qty };
      actor.task = `Carregando ${format(qty)} de ${foodName(actor.carrying.food)} para o depósito`;
      walkTo(actor, store);
      return;
    }
    const need = !canBake && bakeryMissing(bakery);
    if (need) {
      actor.task = `Indo buscar ${foodName(need)} no depósito`;
      walkTo(actor, store);
      return;
    }
    const baking = bakeryCanBake(bakery);
    actor.task = baking ? `Assando ${foodName(recipe.id)} na padaria` : `Na padaria, esperando ${recipe.needs.map(id => foodName(id)).join(', ')}`;
    if (baking) gainTrade(actor, 'bakery');
    actor.wait = 3 / taskSpeed(actor, bakery); actor.actionTime = actor.wait; actor.actionClock = 0;
    return;
  }
  if (isAt(actor, store)) {
    if (actor.carrying && !actor.carrying.toBakery) deliverLoad(actor);
    const need = !actor.carrying && bakeryMissing(bakery);
    if (need) {
      const qty = Math.min(foodLoad(actor), state.food[need]);
      state.food[need] -= qty;
      actor.carrying = { food: need, qty, toBakery: bakery.site };
    }
  }
  if (actor.carrying && !actor.carrying.toBakery) {
    actor.task = `Carregando ${format(actor.carrying.qty)} de ${foodName(actor.carrying.food)} para o depósito`;
    walkTo(actor, store);
    return;
  }
  actor.task = actor.carrying ? `Levando ${format(actor.carrying.qty)} de ${foodName(actor.carrying.food)} pra padaria` : 'Indo trabalhar na padaria';
  actor.headingToWork = true;
  walkTo(actor, bakery);
}
// Depósitos onde o camponês descarrega: o casarão e os Depósitos de Provisões prontos.
function nearestStorage(from) {
  const points = [MANOR_DOOR, ...depots()];
  return points.reduce((best, point) => Math.hypot(point.x - from.x, point.y - from.y) < Math.hypot(best.x - from.x, best.y - from.y) ? point : best);
}
function pitRoutine(actor, pit) {
  if (pit.type === 'quarry') {
    const store = nearestStorage({ x: MANOR_DOOR.x, y: MANOR_DOOR.y });
    if (actor.carrying?.stoneQty || actor.carrying?.marbleQty) {
      const isMarble = Number.isFinite(actor.carrying.marbleQty);
      const material = isMarble ? 'mármore' : 'pedra de construção';
      if ((actor.mapId || 'city') !== 'city') { actor.task = `Levando ${material} para a City`; travelToMap(actor, 'city', pit.mapSet || 2); return; }
      if (isAt(actor, store)) { deliverLoad(actor); actor.task = `${material} entregue no depósito`; actor.wait = 1; return; }
      actor.task = `Levando ${material} ao depósito`; walkTo(actor, store); return;
    }
    const quarryMap = pit.mapId || 'wilds';
    if ((actor.mapId || 'city') !== quarryMap || (actor.mapSet || 1) !== (pit.mapSet || 2)) { actor.task = 'Indo para a pedreira'; travelToMap(actor, quarryMap, pit.mapSet || 2); return; }
    if (!isAt(actor, pit)) { actor.task = 'Indo à bancada da pedreira'; walkTo(actor, pit); return; }
    const tool = assignExtractionTool(actor, pit);
    if (!tool) { pit.workStatus = 'Sem ferramenta disponível'; actor.task = 'Falta ferramenta de pedreira'; actor.wait = 3; return; }
    // Só sai com a carga cheia; leva o que sobrou apenas quando a bancada parou de render material.
    const isMarble = pit.quarryOrder === 'marble';
    const fullLoad = pit.level ? 40 : 20;
    const face = selectedQuarryFace(pit);
    const stalled = !['rubble', 'marble'].includes(pit.quarryOrder || 'rubble') || !face || face.looseMassKg <= 0 || (stoneDepositForPit(pit)?.remainingUnits || 0) <= 0;
    const qty = Math.min(fullLoad, isMarble ? (pit.stockMarble || 0) : (pit.stockStone || 0));
    if (qty > 0 && !stalled && qty < fullLoad - 1e-6) { pit.workStatus = 'Juntando carga na bancada'; actor.task = `Juntando ${isMarble ? 'mármore' : 'pedra'}: ${formatMass(qty)} de ${fullLoad} kg`; actor.wait = 2; actor.actionTime = 2; actor.actionClock = 0; return; }
    if (qty > 0) {
      if (isMarble) pit.stockMarble -= qty; else pit.stockStone -= qty;
      tool.durability = Math.max(0, tool.durability - qty / 20);
      actor.carrying = isMarble ? { marbleQty: qty, fromPit: pit.site } : { stoneQty: qty, fromPit: pit.site };
      actor.task = `Levando ${formatMass(qty)} ${isMarble ? 'mármore' : 'pedra de construção'}`;
      travelToMap(actor, 'city', pit.mapSet || 2); return;
    }
    pit.workStatus = 'Aguardando material no posto'; actor.task = `Aguardando ${isMarble ? 'mármore' : 'pedra de construção'} na pedreira`; actor.wait = 2; return;
  }
  const store = nearestStorage({ x: MANOR_DOOR.x, y: MANOR_DOOR.y });
  if (actor.carrying?.ore) {
    if ((actor.mapId || 'city') !== 'city') { actor.task = 'Levando minério para a City'; travelToMap(actor, 'city', pit.mapSet || 2); return; }
    if (isAt(actor, store)) { deliverLoad(actor); actor.task = 'Minério entregue no depósito'; actor.wait = 1; return; }
    actor.task = 'Levando minério ao depósito'; walkTo(actor, store); return;
  }
  if ((actor.mapId || 'city') !== 'wilds' || (actor.mapSet || 1) !== (pit.mapSet || 2)) {
    actor.task = 'Indo para o posto de mineração'; travelToMap(actor, 'wilds', pit.mapSet || 2); return;
  }
  const seam = selectedSeam(pit);
  if (!seam || seam.remainingGrams <= 0) { pit.workStatus = 'Veio esgotado'; actor.task = 'Veio esgotado'; actor.wait = 3; return; }
  if (!isAt(actor, pit)) { actor.task = `Indo à frente de ${oreName(seam.mineral)}`; walkTo(actor, pit); return; }
  const tool = assignExtractionTool(actor, pit);
  if (!tool) { pit.workStatus = 'Sem picareta disponível'; actor.task = 'Falta picareta'; actor.wait = 3; return; }
  // Só sai com a carga cheia (20 kg, 40 no nível 2); leva o resto apenas quando o veio acabou ou o depósito só aceita menos.
  const fullGrams = Math.min(workerLoadForPit(pit), storageCap() * 100000);
  const loadGrams = Math.min(fullGrams, pit.stockGrams || 0);
  if (loadGrams > 0 && loadGrams < fullGrams - 1e-6 && seam.remainingGrams > 0) { pit.workStatus = 'Juntando carga no posto'; actor.task = `Juntando ${oreName(seam.mineral)}: ${formatMass(loadGrams / 1000)} de ${formatMass(fullGrams / 1000)} kg`; actor.wait = 2; actor.actionTime = 2; actor.actionClock = 0; gainSkill(actor, 'mining', 0.05); return; }
  if (loadGrams > 0) {
    pit.stockGrams -= loadGrams;
    tool.durability = Math.max(0, tool.durability - loadGrams / 20000);
    actor.carrying = { ore: MINERAL_SEAMS[seam.mineral]?.id || 'ironOre', mineral: seam.mineral, seamId: seam.id, occurrence: seam.occurrence || 'ore-lot', qtyGrams: loadGrams, assayPpm: seam.gradePpm, fromPit: pit.site };
    actor.task = `Levando ${(loadGrams / 1000).toFixed(1)} kg de ${oreName(seam.mineral)}`;
    gainTrade(actor, pit.type); travelToMap(actor, 'city', pit.mapSet || 2); return;
  }
  pit.workStatus = 'Aguardando carga no posto'; actor.task = `Aguardando carga de ${oreName(seam.mineral)}`;
  actor.wait = 2; actor.actionTime = 2; actor.actionClock = 0; gainSkill(actor, 'mining', 0.05);
}
function deliverLoad(actor) {
  const load = actor.carrying;
  actor.carrying = null;
  if (!load) return;
  gainSkill(actor, 'hauling', 0.25);
  if (Number.isFinite(load.stoneQty)) {
    const accepted = Math.min(load.stoneQty, Math.max(0, storageCap() - storedUnits()));
    state.stone = (state.stone || 0) + accepted;
    if (accepted < load.stoneQty - 1e-6) {
      const pit = state.buildings.find(building => building.site === load.fromPit && building.type === 'quarry');
      if (pit) pit.stockStone = (pit.stockStone || 0) + load.stoneQty - accepted;
      if (!state.storageWarned) { state.storageWarned = true; say('O depósito da City está cheio. A pedra voltou ao estoque da pedreira.'); }
    } else state.storageWarned = false;
    return;
  }
  if (Number.isFinite(load.marbleQty)) {
    const accepted = Math.min(load.marbleQty, Math.max(0, storageCap() - storedUnits()));
    state.marble = (state.marble || 0) + accepted;
    if (accepted < load.marbleQty - 1e-6) {
      const pit = state.buildings.find(building => building.site === load.fromPit && building.type === 'quarry');
      if (pit) pit.stockMarble = (pit.stockMarble || 0) + load.marbleQty - accepted;
      if (!state.storageWarned) { state.storageWarned = true; say('O depósito da City está cheio. O mármore voltou ao estoque da pedreira.'); }
    } else state.storageWarned = false;
    return;
  }
  if (load.ore && load.mineral && Number.isFinite(load.qtyGrams)) {
    const amountKg = load.qtyGrams / 1000;
    const freeKg = Math.max(0, (storageCap() - storedUnits()) * 100);
    const acceptedKg = Math.min(amountKg, freeKg);
    const oldKg = state.metals[load.ore] || 0;
    state.oreGrades ||= {};
    if (acceptedKg > 0 && !MINERAL_SEAMS[load.mineral]?.find) state.oreGrades[load.ore] = ((state.oreGrades[load.ore] || load.assayPpm || MINERAL_SEAMS[load.mineral].assayPpm) * oldKg + (load.assayPpm || MINERAL_SEAMS[load.mineral].assayPpm) * acceptedKg) / (oldKg + acceptedKg);
    state.metals[load.ore] = (state.metals[load.ore] || 0) + acceptedKg;
    if (acceptedKg > 0) { state.oreLots ||= []; state.oreLots.push({ id: load.ore, mineral: load.mineral, seamId: load.seamId || null, sourcePit: load.fromPit, occurrence: load.occurrence || 'ore-lot', quantityKg: acceptedKg, gradePpm: load.assayPpm || MINERAL_SEAMS[load.mineral].assayPpm }); }
    if (acceptedKg < amountKg - 1e-6) {
      const pit = state.buildings.find(building => building.site === load.fromPit && building.type === 'mine');
      if (pit) pit.stockGrams = (pit.stockGrams || 0) + Math.round((amountKg - acceptedKg) * 1000);
      if (!state.storageWarned) { state.storageWarned = true; say('O depósito da City está cheio. A rocha restante voltou para o estoque do posto.'); }
    } else state.storageWarned = false;
    return;
  }
  // Dead Remains que ia pro laboratório volta pro depósito.
  if (load.remains) { state.remains[load.remains] = (state.remains[load.remains] || 0) + 1; return; }
  const put = Math.min(load.qty, Math.max(0, storageCap() - storedUnits()));
  state.food[load.food] = (state.food[load.food] || 0) + put;
  if (put < load.qty - 1e-6 && !state.storageWarned) { state.storageWarned = true; say('O Depósito de Provisões está cheio. O resto da carga apodreceu do lado de fora.'); }
  if (put >= load.qty - 1e-6) state.storageWarned = false;
}
const foodName = id => (FOOD_TYPES.find(food => food.id === id)?.name || id).toLowerCase();
const workPlaceName = building => building.type === 'fishing' ? `em ${building.name}` : building.type === 'hunting' ? 'nas Wilds, caçando e recolhendo carne' : building.type === 'mill' ? 'no moinho' : building.type === 'emberLab' ? 'no Laboratório de Ember' : building.type === 'lumber' ? 'na Cabana do Lenhador' : building.type === 'quarry' ? 'na Pedreira' : building.type === 'mine' ? 'no Campo de Mineração' : `no ${buildingName(building)}`;
function walkTo(actor, point) {
  if (actor.className === 'Peasant' && (actor.mapId || 'city') === 'city') routeToPoint(actor, point);
  else { routeActor(actor, nearestNode(point)); actor.finalPoint = { x: point.x, y: Math.min(198, point.y + 1.2) }; }
}
const isAt = (actor, point) => Math.hypot(actor.x - point.x, actor.y - Math.min(198, point.y + 1.2)) < 1.5;
const actorAtPost = (actor, building) =>
  (actor.mapId || 'city') === (building.mapId || 'city') &&
  (actor.mapSet || 1) === (building.mapSet || 1) && isAt(actor, building);
const workingCrew = building => lumberCrew(building).filter(actor => actor.buildSite == null && actorAtPost(actor, building));
const workingCrewWork = building => workingCrew(building).reduce((sum, worker) => sum + gearBonus(worker, 'work') + tradeWorkBonus(worker, hookOf(building)) / 2, 0);
// Rotina do camponês de fazenda ou moinho: trabalha, carrega a produção ao depósito e volta.
// No moinho ele também busca trigo no depósito e leva a farinha de volta.
function haulRoutine(actor, workplace) {
  const store = nearestStorage(workplace);
  const mill = workplace.type === 'mill';
  if (isAt(actor, workplace)) {
    if (mill && actor.carrying?.food === 'wheat') { workplace.input = (workplace.input || 0) + actor.carrying.qty; actor.carrying = null; }
    const secondaryId = !mill && workplace.type === 'farm' ? farmType(workplace.produce).secondary : null;
    const secondaryStock = workplace.secondaryStock || 0;
    const foodId = mill ? 'flour' : secondaryId && secondaryStock >= 1 ? secondaryId : farmFoodId(workplace);
    const availableStock = mill ? workplace.stock || 0 : foodId === secondaryId ? secondaryStock : workplace.stock || 0;
    // Moinho: mói até o trigo acabar e sai com a carga cheia de farinha, ou com o que tiver quando o trigo acabou.
    const millDone = mill && (workplace.input || 0) <= 1e-6;
    if (mill ? availableStock >= foodLoad(actor) - 1e-6 || (availableStock >= 0.5 && millDone) : availableStock >= 1) {
      const qty = Math.min(foodLoad(actor), availableStock);
      if (!mill && foodId === secondaryId) workplace.secondaryStock -= qty; else workplace.stock -= qty;
      actor.carrying = { food: foodId, qty };
      actor.task = `Carregando ${format(qty)} de ${foodName(actor.carrying.food)} para o depósito`;
      actor.pickupAnim = true;
      actor.wait = 0.65;
      actor.actionTime = 0.65;
      actor.actionClock = 0;
      walkTo(actor, store);
      return;
    }
    if (mill && (state.food.wheat || 0) >= 1 && millDone) {
      actor.task = 'Indo buscar trigo no depósito';
      walkTo(actor, store);
      return;
    }
    actor.task = mill ? (workplace.input > 0 ? 'Moendo trigo no moinho' : 'No moinho, esperando trigo') : `Trabalhando ${workPlaceName(workplace)}`;
    if (!mill || workplace.input > 0) gainTrade(actor, hookOf(workplace));
    actor.wait = 3 / taskSpeed(actor, workplace); actor.actionTime = actor.wait; actor.actionClock = 0;
    return;
  }
  if (isAt(actor, store)) {
    if (actor.carrying && (actor.carrying.food !== 'wheat' || !mill)) deliverLoad(actor);
    if (mill && !actor.carrying && (state.food.wheat || 0) >= 0.5) {
      const qty = Math.min(foodLoad(actor), state.food.wheat);
      state.food.wheat -= qty;
      actor.carrying = { food: 'wheat', qty };
    }
  }
  if (actor.carrying && (actor.carrying.food !== 'wheat' || !mill)) {
    actor.task = `Carregando ${format(actor.carrying.qty)} de ${foodName(actor.carrying.food)} para o depósito`;
    walkTo(actor, store);
    return;
  }
  actor.task = actor.carrying ? `Levando ${format(actor.carrying.qty)} de trigo pro moinho` : `Indo trabalhar ${workPlaceName(workplace)}`;
  actor.headingToWork = true;
  walkTo(actor, workplace);
}
// O que o camponês está fazendo agora, para a ficha e o cartão.
function peasantActivity(actor) {
  if (actor.buildSite != null) {
    const site = buildSiteOf(actor);
    if (site) return `${site.underConstruction ? 'Construindo' : 'Consertando'} ${buildingName(site)}`;
  }
  if (actor.drinkUntil > state.minutes && openTavern()) return 'Bebendo na taverna';
  return actor.task || 'À toa, esperando trabalho';
}
function producerBoxHTML(building) {
  const level = producerLevel(building);
  const maxLevel = building.type === 'butcher' ? BUTCHER_MAX_LEVEL : building.type === 'blacksmith' ? BLACKSMITH_RATE.length : PRODUCER_LEVELS.length;
  const next = level + 1 < maxLevel ? PRODUCER_LEVELS[level + 1] : null;
  const mill = building.type === 'mill';
  const bakery = building.type === 'bakery';
  const rateAt = lv => mill ? MILL_RATE[lv] : bakery ? BAKE_RATE[lv] : building.type === 'emberLab' ? LAB_RATE[lv] : building.type === 'butcher' ? BUTCHER_RATE[lv] : building.type === 'blacksmith' ? BLACKSMITH_RATE[lv] : farmType(building.produce).grain[lv];
  const unit = building.type === 'blacksmith' ? 'lotes por segundo' : mill ? 'de trigo moído em farinha por segundo' : `de ${farmType(building.produce).name.toLowerCase()} por segundo`;
  return `<section class="inspector-section"><h4>Melhoria</h4>
    ${next ? `<button type="button" class="hut-upgrade" data-producer-upgrade="${building.site}">Nível ${level + 2} · ${rateNumber(rateAt(level + 1))}/s · ${costHTML(next.gold, next.wood)}</button>` : '<p class="storage-items">Nível máximo. Mais que isso, só com milagre.</p>'}</section>`;
}
function bakeryRecipeBoxHTML(bakery) {
  const current = bakeryActiveRecipe(bakery).id;
  return `<section class="inspector-section" aria-label="Fornada da padaria"><h4>Produção</h4>
    <div class="farm-type-grid compact">${BAKERY_RECIPES.map(recipe => {
      const locked = recipe.id === 'quiche' && producerLevel(bakery) < 1;
      const detail = locked ? 'Desbloqueia no nível 2' : `${recipe.needs.map(id => foodName(id)).join(' + ')} · alimenta ${foodFeeds(recipe.id)}×`;
      return `<button class="farm-type${current === recipe.id ? ' selected' : ''}${locked ? ' locked' : ''}" data-bakery-recipe="${recipe.id}" aria-pressed="${current === recipe.id}"${locked ? ' disabled aria-disabled="true"' : ''} title="${detail}"><img src="public/art/${BAKED_ART[recipe.id]}-v01.webp" alt=""><b>${recipe.name}</b>${locked ? '<small>nível 2</small>' : ''}</button>`;
    }).join('')}</div></section>`;
}
function weaponOrderBoxHTML(building) {
  const pattern = weaponPattern(building.weaponPattern), materials = Object.keys(pattern.designs);
  const materialId = materials.includes(building.weaponMaterial) ? building.weaponMaterial : materials[0];
  const design = weaponDesign(pattern.id, materialId);
  const missing = Object.entries(design.needs).filter(([id, qty]) => (id === 'timber' ? state.timber : state.metals[id] || 0) + 1e-9 < qty);
  const base = GEAR[pattern.base], bodyProfile = METAL_WEAPON_PROFILE[design.body], edgeProfile = METAL_WEAPON_PROFILE[design.edge || design.body];
  const locked = producerLevel(building) + 1 < weaponMinLevel(materialId);
  const occupied = building.weaponOrder || building.weaponJob || building.repairJob;
  const work = building.weaponJob ? `${Math.floor(building.weaponJob.elapsed)}/${building.weaponJob.required} min · forjando` : building.repairJob ? `${Math.floor(building.repairJob.elapsed)}/${building.repairJob.required} min · reparando` : building.weaponOrder ? 'Aguardando a fila de produção' : building.weaponStatus || 'Forja livre';
  const weaponRows = (state.craftedWeapons || []).map(weapon => {
    const parts = [['body', 'estrutura', weapon.bodyMaterial, weapon.structuralCondition, weapon.structuralMax]];
    if (weapon.sharp) parts.push(['edge', 'gume', weapon.edgeMaterial, weapon.edgeCondition, weapon.edgeMax]);
    return `<div class="pit-hint"><b>${weapon.name}</b> · ${weapon.location === 'inventory' ? 'no depósito' : 'equipada'} · ${parts.map(([part, label, metal, value, max]) => `<button type="button" class="hut-upgrade" data-weapon-repair="${weapon.id}" data-part="${part}" data-smith="${building.site}" ${occupied || value >= max - 1e-6 || (state.metals[metal] || 0) < .25 ? 'disabled' : ''}>Reparar ${label} · 0,25 kg ${METAL_NAMES[metal]} (${formatMass(value)}/${formatMass(max)})</button>`).join(' ')}</div>`;
  }).join('') || '<p class="storage-items">Nenhuma arma forjada para reparar.</p>';
  return `<section class="inspector-section" aria-label="Ordens de armas"><h4>Forjar arma</h4>
    <label class="pit-hint">Padrão <select data-weapon-pattern="${building.site}">${WEAPON_PATTERNS.map(item => `<option value="${item.id}"${item.id === pattern.id ? ' selected' : ''}>${item.name}</option>`).join('')}</select></label>
    <label class="pit-hint">Material e construção <select data-weapon-material="${building.site}">${materials.map(id => `<option value="${id}"${id === materialId ? ' selected' : ''}>${WEAPON_MATERIAL_LABELS[id]}</option>`).join('')}</select></label>
    <p class="pit-hint">${design.name} · corpo: ${METAL_NAMES[design.body]}${pattern.sharp ? ` · gume/ponta: ${METAL_NAMES[design.edge]}` : ''} · custo ${Object.entries(design.needs).map(([id, qty]) => `${qty} ${id === 'timber' ? 'madeira' : METAL_NAMES[id]}`).join(' + ')} · Blacksmith nível ${weaponMinLevel(materialId)}${locked ? ' · bloqueado' : ''}<br>Estrutura ${Math.round(bodyProfile.structural * 100)} · desgaste ${pattern.sharp ? `gume/ponta ×${edgeProfile.wear}` : 'estrutural'} · ataque ${bonusText(base.bonus)}${missing.length ? `<br><b>Falta: ${missing.map(([id, qty]) => `${formatMass(Math.max(0, qty - (id === 'timber' ? state.timber : state.metals[id] || 0)))} ${id === 'timber' ? 'madeira' : METAL_NAMES[id]}`).join(' · ')}</b>` : ''}</p>
    <p class="pit-hint">Fila: ${work}</p>
    ${occupied ? `<button type="button" class="hut-upgrade" data-cancel-weapon-order="${building.site}">Cancelar ordem · devolver insumos</button>` : `<button type="button" class="hut-upgrade" data-place-weapon-order="${building.site}" ${missing.length || locked ? 'disabled' : ''}>Encomendar ${design.name}</button>`}
    <h4>Reparos do arsenal</h4>${weaponRows}</section>`;
}
function blacksmithRecipeBoxHTML(building) {
  const current = blacksmithRecipe(building).id;
  return `<section class="inspector-section" aria-label="Metais produzidos no Blacksmith"><h4>Produção</h4>
    <div class="farm-type-grid compact">${BLACKSMITH_RECIPES.map(recipe => {
      const locked = recipe.minLevel > producerLevel(building) + 1;
      const inputs = Object.entries(recipe.needs).map(([id, qty]) => `${qty} ${METAL_NAMES[id]}`).join(' + ');
      const outputs = Object.entries(recipe.makes).map(([id, qty]) => `${qty} ${METAL_NAMES[id]}`).join(' + ');
      return `<button class="farm-type${current === recipe.id ? ' selected' : ''}${locked ? ' locked' : ''}" data-blacksmith-recipe="${recipe.id}" aria-pressed="${current === recipe.id}"${locked ? ' disabled aria-disabled="true"' : ''} title="${inputs} → ${outputs}"><b>${recipe.name}</b><small>${locked ? 'Nível 2' : `${inputs} → ${outputs}`}</small></button>`;
    }).join('')}</div>${weaponOrderBoxHTML(building)}</section>`;
}
function butcherRecipeBoxHTML(butcher) {
  const current = butcherActiveRecipe(butcher).id;
  return `<section class="inspector-section" aria-label="Receita do açougue"><h4>Produção</h4>
    <div class="farm-type-grid compact">${BUTCHER_RECIPES.map(recipe => `<button class="farm-type${current === recipe.id ? ' selected' : ''}" data-butcher-recipe="${recipe.id}" aria-pressed="${current === recipe.id}" title="${recipe.needs.map(id => foodName(id)).join(' + ')} → alimenta ${recipe.feeds}×"><img src="public/art/${recipe.icon || `${recipe.art}-v01.webp`}" alt=""><b>${recipe.name}</b><small>${recipe.feeds}× alimento</small></button>`).join('')}</div></section>`;
}
function upgradeProducer(site) {
  const building = state.buildings.find(item => item.site === site && PRODUCER_TYPES.includes(item.type) && isDone(item));
  const maxLevel = building?.type === 'butcher' ? BUTCHER_MAX_LEVEL : building?.type === 'blacksmith' ? BLACKSMITH_RATE.length : PRODUCER_LEVELS.length;
  const next = building && producerLevel(building) + 1 < maxLevel ? PRODUCER_LEVELS[producerLevel(building) + 1] : null;
  if (!next) return;
  if (state.gold < next.gold || state.timber < next.wood) { say('Falta ouro ou madeira. A colheita vai continuar do mesmo tamanho.'); return; }
  state.gold -= next.gold;
  state.timber -= next.wood;
  startUpgrade(building, { kind: 'producer' }, `${buildingName(building)} para o nível ${producerLevel(building) + 2}`);
}
const foodDemand = () => eaters() * WORKER_UPKEEP * RATIONS[state.ration].mult;

function happinessLabel(value) {
  return value < 20 ? 'Revoltados' : value < 40 ? 'Infelizes' : value < 60 ? 'Conformados' : value < 80 ? 'Contentes' : 'Felizes';
}

// Uma vez por segundo de jogo: produz, serve a mesa e anota o alvo de felicidade da semana.
function feedWorkers(dt) {
  state.buildings.filter(building => building.type === 'farm' && isDone(building)).forEach(farm => {
    farm.stock = Math.min(FARM_STOCK_CAP, (farm.stock || 0) + farmOutput(farm) * (1 + crewWork(farm)) * dt);
    if (farmType(farm.produce).secondary) farm.secondaryStock = Math.min(FARM_STOCK_CAP, (farm.secondaryStock || 0) + farmSecondaryRate(farm) * (1 + crewWork(farm)) * dt);
  });
  grindWheat(dt);
  bakeFood(dt);
  processButcher(dt);
  refineRemains(dt);
  const ration = RATIONS[state.ration];
  const demand = foodDemand() * dt;
  let remaining = demand;
  const eaten = new Set();
  let pool = FOOD_TYPES.filter(food => !food.raw && state.diet[food.id] && state.food[food.id] > 0);
  for (let pass = 0; pass < 6 && remaining > 1e-9 && pool.length; pass++) {
    const share = remaining / pool.length;
    // A fome se mede em pão: 1 de quiche mata 4 de fome, então sai 1/4 do estoque para a mesma parte.
    pool.forEach(food => {
      const take = Math.min(share, state.food[food.id] * food.feeds);
      state.food[food.id] -= take / food.feeds;
      remaining -= take;
      if (take > 0) eaten.add(food.id);
    });
    pool = pool.filter(food => state.food[food.id] > 1e-9);
  }
  const fulfilled = demand > 0 ? (demand - remaining) / demand : 1;
  const parts = eaters() ? {
    ration: ration.mood,
    food: ration.mult > 0 ? [...eaten].reduce((sum, id) => sum + (FOOD_TYPES.find(food => food.id === id)?.mood || 0), 0) : 0,
    tax: taxMood(),
    hunger: demand > 0 ? -Math.round(20 * (1 - fulfilled)) : 0,
    empathy: Math.round(state.workers.filter(worker => worker.alive).reduce((sum, worker) => sum + tradeAttribute(worker, 'emp'), 0) * 0.5),
  } : { ration: 0, food: 0, tax: 0, hunger: 0, empathy: 0 };
  const target = Math.max(0, Math.min(100, 50 + parts.ration + parts.food + parts.tax + parts.hunger + parts.empathy));
  // Não muda agora: acumula o alvo para a média da semana (ver applyWeeklyMood).
  state.moodSum = (state.moodSum || 0) + target * dt;
  state.moodTime = (state.moodTime || 0) + dt;
  state.moodParts = parts;
  state.moodTarget = target;
  if (fulfilled < 0.999 && eaters() && !state.hungerWarned) { state.hungerWarned = true; say('A comida não deu pra todo mundo. Os camponeses estão passando fome. Por enquanto, reclamam baixinho.'); }
  if (fulfilled >= 0.999) state.hungerWarned = false;
}

// ==========================================================================
// Tributos e Favor dos nobres. De tempos em tempos um nobre de Mordavia (às vezes o próprio
// Barão das Cinzas) exige um tanto de algum recurso, com prazo. Entregar no prazo dá Favor;
// atrasar tira Favor a cada hora; passado o fim da tolerância, os homens dele vêm buscar
// à força. Com Favor muito baixo, os nobres atacam o domínio.
// ==========================================================================
const FAVOR_ATTACK = 20;
const DEMAND_GRACE = 720; // minutos de atraso tolerados antes de virem buscar
const NOBLES = [
  { id: 'grenholt', name: 'Lorde Anselm Grenholt', house: 'grenholt', title: 'Caçador-Mor das Montanhas', wants: ['bacon', 'timber', 'garlic'],
    lines: ['Diz que é pros cães. Os cães dele comem melhor que seus camponeses.', 'A lua cheia vem aí e a despensa dele esvaziou de um jeito que ninguém comenta.', 'Mandou junto um dente de lobo embrulhado num bilhete. O bilhete diz "lembrança".'],
    paid: 'Lorde Anselm mandou agradecer com um uivo. Pelo menos foi o que o mensageiro disse.', late: 'Lorde Anselm não gosta de esperar. Os cães dele, menos ainda.' },
  { id: 'montalvo', name: 'Dama Severina Montalvo', house: 'montalvo', title: 'Credora de Meio Reino', wants: ['gold', 'flour', 'eggs'],
    lines: ['Ela chama de "juros de cortesia". Você chama de assalto com sotaque.', 'Ela anotou a dívida num livro. Seu nome está em vermelho, sublinhado duas vezes.', 'O tio de vocês devia a ela. Ela fez questão de lembrar que dívida é herança.'],
    paid: 'Dama Severina riscou uma linha do livro. Sobraram só umas quarenta.', late: 'Dama Severina cobra juros sobre o atraso. E juros sobre os juros.' },
  { id: 'lucerna', name: 'Padre-Conde Ottokar Lucerna', house: 'lucerna', title: 'Coveiro da Coroa', wants: ['milk', 'flour', 'timber'],
    lines: ['É pra um velório. Ele não disse de quem. Ainda.', 'Segundo ele, os mortos de Mordavia também precisam comer.', 'É pros caixões, explicou. Perguntou o seu tamanho, por curiosidade.'],
    paid: 'O Padre-Conde rezou pela sua alma. Disse que ela vai precisar.', late: 'O Padre-Conde mandou medir a sua porta. Pra ver se o caixão passa.' },
  { id: 'kastrell', name: 'Baronesa Ilka Kastrell', house: 'kastrell', title: 'Marechal de uma guerra que não veio', wants: ['timber', 'bacon', 'gold'],
    lines: ['Ela abastece um exército pra uma guerra que nunca chega. Os soldados comem do mesmo jeito.', 'Diz que é pras paliçadas. Contra quem, ninguém sabe. Ela sabe.', 'A guerra vai começar a qualquer momento, garante ela. Faz vinte anos.'],
    paid: 'A Baronesa anotou seu nome na lista de aliados. A lista é curta e nervosa.', late: 'A Baronesa considera atraso um ato de guerra. Finalmente, uma guerra.' },
  { id: 'vaelmont', name: 'Tia-Avó Brunhilde Vaelmont', house: 'vaelmont', title: 'Matriarca de uma casa hipotecada', wants: ['eggs', 'milk', 'garlic', 'gold'],
    lines: ['Ela é da família. Isso não é bom sinal.', 'Diz que é pra um chá. Ninguém sobrevive ao chá dela por muito tempo.', 'Exige respeito aos mais velhos. E pontualidade. Principalmente a pontualidade.'],
    paid: 'A Tia-Avó Brunhilde resmungou. Na língua dela, isso é amor.', late: 'A Tia-Avó Brunhilde mencionou o testamento. O seu nome saiu dele.' },
];
const BARON = { id: 'baron', name: 'O Barão das Cinzas', house: null, title: 'Senhor de tudo o que já queimou', baron: true, wants: ['gold', 'flour', 'bacon', 'timber'],
  lines: ['Ninguém viu o rosto dele. Quem viu não comenta. Quem comenta não é mais visto.', 'O mensageiro chegou com o cabelo chamuscado e pediu pra não ser o próximo.', 'A carta veio selada com cera preta. Ainda estava quente.'],
  paid: 'O Barão das Cinzas aceitou o tributo. Ninguém se queimou hoje. Ainda.', late: 'Cheiro de fumaça no vento. O Barão das Cinzas não manda segundo aviso.' };
const DEMAND_BASE = { gold: 30, timber: 15, flour: 15, garlic: 10, bacon: 6, eggs: 8, milk: 8, wheat: 15 };
// Para a cronologia dos herdeiros, um ano ficcional equivale a 360 dias de jogo.
const GAME_YEAR_MINUTES = DAYS_PER_YEAR * 1440; // o mesmo ano do calendário do relógio
const HEIR_PHASE_YEARS = 25;
const heirYearsElapsed = (minutes = state.minutes) => Math.min(75, Math.floor(Math.max(0, minutes) / (GAME_YEAR_MINUTES * HEIR_PHASE_YEARS)) * HEIR_PHASE_YEARS);
const heirPhase = (minutes = state.minutes) => Math.min(4, 1 + Math.floor(heirYearsElapsed(minutes) / HEIR_PHASE_YEARS));
const heirPortraitSrc = (id, phase = 1) => `public/art/${heirById(id)?.portrait || `heir-portraits-v01/${id}`}-phase-${phase}.webp`;
const noblePortraitSrc = (house, phase = heirPhase()) => `public/art/noble-portraits-v01/${house}-phase-${phase}.webp`;
const resourceName = id => id === 'gold' ? 'ouro' : id === 'timber' ? 'madeira' : foodName(id);
const resourceStock = id => id === 'gold' ? state.gold : id === 'timber' ? state.timber : (state.food[id] || 0);
const nobleOf = demand => demand.noble === 'baron' ? BARON : NOBLES.find(noble => noble.id === demand.noble) || BARON;
function favorLabel(value) {
  return value < FAVOR_ATTACK ? 'Inimigo' : value < 40 ? 'Mal-visto' : value < 60 ? 'Tolerado' : value < 80 ? 'Bem-visto' : 'Favorito';
}
function changeFavor(amount) { state.favor = Math.max(0, Math.min(100, (state.favor ?? 50) + amount)); }
function newDemand() {
  const baron = Math.random() < 0.15;
  const noble = baron ? BARON : pick(NOBLES);
  const res = pick(noble.wants);
  const day = Math.floor(state.minutes / 1440);
  const qty = Math.round(DEMAND_BASE[res] * (1 + day * 0.15) * (baron ? 2 : 1));
  const demand = { id: `demand-${Date.now()}-${Math.floor(Math.random() * 1000)}`, noble: noble.id, res, qty, line: pick(noble.lines),
    created: state.minutes, due: state.minutes + (baron ? 3240 : 4320), late: false };
  state.demands.push(demand);
  say(`${noble.name} exige ${qty} de ${resourceName(res)} em ${baron ? 54 : 72} horas. ${demand.line}`, `${noble.name} mandou um mensageiro. Veja na aba Economia.`);
  if (state.tab === 'economy') renderPanel();
}
function deliverDemand(id) {
  const demand = state.demands.find(item => item.id === id);
  if (!demand) return;
  const noble = nobleOf(demand);
  if (resourceStock(demand.res) < demand.qty) { say(`Falta ${resourceName(demand.res)}. ${noble.name} não aceita metade. Nem fiado.`); return; }
  if (demand.res === 'gold') state.gold -= demand.qty;
  else if (demand.res === 'timber') state.timber -= demand.qty;
  else state.food[demand.res] -= demand.qty;
  const gain = demand.late ? 2 : noble.baron ? 15 : 8;
  changeFavor(gain);
  state.demands = state.demands.filter(item => item !== demand);
  say(`${noble.paid} Favor +${gain}.`);
  updateHud(); renderPanel();
}
// Os homens do nobre vêm buscar o que der e ainda quebram alguma coisa no caminho.
function seizeDemand(demand) {
  const noble = nobleOf(demand);
  const taken = Math.min(demand.qty, Math.max(0, resourceStock(demand.res)));
  if (demand.res === 'gold') state.gold -= taken;
  else if (demand.res === 'timber') state.timber -= taken;
  else state.food[demand.res] -= taken;
  changeFavor(noble.baron ? -20 : -10);
  const targets = state.buildings.filter(building => !building.underConstruction && !building.ruined);
  const hits = noble.baron ? 2 : 1;
  for (let i = 0; i < hits && targets.length; i++) {
    const target = targets.splice(Math.floor(Math.random() * targets.length), 1)[0];
    damageBuilding(target, Math.round(buildingMaxHp(target) * (noble.baron ? 0.6 : 0.3)));
  }
  state.demands = state.demands.filter(item => item !== demand);
  say(noble.baron ? `O Barão das Cinzas mandou buscar o tributo. Levaram ${format(taken)} de ${resourceName(demand.res)} e deixaram fogo. Favor −20.`
    : `Os homens de ${noble.name} vieram buscar à força: levaram ${format(taken)} de ${resourceName(demand.res)} e quebraram o que acharam. Favor −10.`);
}
// Com Favor abaixo de FAVOR_ATTACK, uma vez por dia algum nobre manda atacar o domínio.
function nobleRaid() {
  const day = Math.floor(state.minutes / 1440);
  if ((state.favor ?? 50) >= FAVOR_ATTACK || state.lastRaidDay === day) return;
  const targets = state.buildings.filter(building => !building.underConstruction && !building.ruined);
  if (!targets.length) return;
  state.lastRaidDay = day;
  const noble = pick(NOBLES);
  const hits = 1 + Math.floor(Math.random() * 2);
  for (let i = 0; i < hits && targets.length; i++) {
    const target = targets.splice(Math.floor(Math.random() * targets.length), 1)[0];
    damageBuilding(target, Math.round(buildingMaxHp(target) * (0.25 + Math.random() * 0.25)));
  }
  say(`Os homens de ${noble.name} atacaram o domínio à noite. Com Favor assim, ninguém em Mordavia vai te defender.`);
}
// Uma vez por hora de jogo.
function processDemands() {
  if (!state.demands) state.demands = [];
  if (state.demands.length < 2 && state.minutes >= (state.nextDemandAt ?? 600)) {
    newDemand();
    state.nextDemandAt = state.minutes + 1440 + Math.floor(Math.random() * 1440);
  }
  [...state.demands].forEach(demand => {
    const noble = nobleOf(demand);
    if (!demand.late && state.minutes > demand.due) {
      demand.late = true;
      changeFavor(-5);
      say(`Tributo de ${noble.name} atrasado. ${noble.late} Favor −5, e cai mais a cada hora.`);
    } else if (demand.late) changeFavor(-1);
    if (state.minutes > demand.due + DEMAND_GRACE) seizeDemand(demand);
  });
  nobleRaid();
  updateHud();
}
// Aviso fixo no canto do mapa: cada tributo pendente, com o prazo. Clicar abre a aba Economia.
function renderDemandAlerts() {
  const host = $('#demand-alerts');
  if (!host) return;
  const html = (state.demands || []).map(demand => {
    const noble = nobleOf(demand);
    const hours = Math.max(0, Math.ceil(((demand.late ? demand.due + DEMAND_GRACE : demand.due) - state.minutes) / 60));
    return `<button type="button" class="demand-alert${demand.late ? ' late' : ''}" data-demand-open title="Ver na aba Economia"><b>${noble.name}</b> quer ${demand.qty} de ${resourceName(demand.res)}<small>${demand.late ? `ATRASADO · vêm buscar em ${hours}h` : `${hours}h de prazo`}</small></button>`;
  }).join('');
  if (host.dataset.html !== html) { host.dataset.html = html; host.innerHTML = html; }
}
function demandsHTML() {
  const favor = Math.round(state.favor ?? 50);
  const rows = (state.demands || []).map(demand => {
    const noble = nobleOf(demand);
    const left = demand.due - state.minutes;
    const when = demand.late ? `ATRASADO · vêm buscar em ${Math.max(0, Math.ceil((demand.due + DEMAND_GRACE - state.minutes) / 60))}h` : `${Math.ceil(left / 60)}h de prazo`;
    const enough = resourceStock(demand.res) >= demand.qty;
    return `<div class="demand${demand.late ? ' late' : ''}${noble.baron ? ' baron' : ''}">
      <div class="demand-head">${noble.house ? `<img class="noble-portrait" src="${noblePortraitSrc(noble.house)}" alt="Retrato de ${noble.name}, fase ${heirPhase()} de 4" decoding="async">` : '<span class="baron-seal" aria-hidden="true">B</span>'}<span><b>${noble.name}</b><small>${noble.title}</small></span></div>
      <p>${demand.line}</p>
      <div class="demand-foot"><span><b>${demand.qty} de ${resourceName(demand.res)}</b><small>${when} · você tem ${format(resourceStock(demand.res))}</small></span>
        <button type="button" class="hut-upgrade" data-deliver="${demand.id}" ${enough ? '' : 'disabled'}>Entregar</button></div></div>`;
  }).join('');
  return `<div class="econ-block favor-block"><div class="mood-head"><span>FAVOR DOS NOBRES</span><b>${favor}</b><em>${favorLabel(favor)}</em></div>
    <div class="mood-bar favor-bar"><i style="width:${favor}%"></i></div>
    <p class="storage-items">Entregar no prazo: +8 (Barão +15). Atrasar: −5 e −1 por hora. Se não entregar, vêm buscar à força. Abaixo de ${FAVOR_ATTACK}, os nobres atacam o domínio.</p>
    <div class="food-label">TRIBUTOS</div>${rows || '<p class="storage-items">Nenhum nobre lembrou de você hoje. Aproveite.</p>'}</div>`;
}
// ==========================================================================
// Recompensas: você escolhe quanto paga e onde (um ponto do mapa, a patrulha do segundo mapa
// ou as ruínas do covil). O ouro sai do cofre na hora e fica guardado. Ninguém recebe ordem:
// cada herói decide se vale a pena pelo valor, pela distância e pela própria mania.
// Quem cumpre (vigiar o ponto por uma hora de jogo, ou fazer a patrulha inteira) embolsa tudo.
// ==========================================================================
const BOUNTY_GUARD = 60; // minutos de vigia no ponto
const BOUNTY_DAYS = 2; // prazo; vencida, o ouro volta pro cofre
const SOUTH_PATROL = [12, 13, 14, 15, 16, 17, 18, 17, 16, 15, 14, 13, 12];
const BOUNTY_PRESETS = {
  south: { label: 'Patrulha do segundo mapa', route: SOUTH_PATROL, node: 15 },
  lair: { label: 'Vigiar as ruínas do covil', node: 9 },
};
const bountyTaker = bounty => bounty.takenBy != null ? state.heroes.find(hero => String(hero.id) === String(bounty.takenBy) && hero.alive) || null : null;
function bountyLabelAt(x, y) {
  return y > 100 ? 'Vigiar a estrada velha' : x > 75 ? 'Vigiar a borda da floresta' : 'Vigiar este ponto';
}
function createBounty(spec) {
  const reward = Math.max(5, Math.round(state.bountyAmount || 25));
  if (state.gold < reward) { say(`Falta ouro. Ninguém arrisca o pescoço por promessa.`); return null; }
  state.gold -= reward;
  const node = spec.node ?? nearestNode(spec);
  const point = spec.x != null ? { x: spec.x, y: spec.y } : { x: PATH_NODES[node].x, y: PATH_NODES[node].y };
  const bounty = { id: `bounty-${Date.now()}-${Math.floor(Math.random() * 1000)}`, label: spec.label || bountyLabelAt(point.x, point.y), reward, x: point.x, y: point.y, node,
    route: spec.route || null, target: spec.target || null, lairId: spec.lairId || null, takenBy: null, until: state.minutes + BOUNTY_DAYS * 1440 };
  state.bounties.push(bounty);
  renderSites(); renderPanel(); updateHud();
  say(`Recompensa de ${reward} de ouro: ${bounty.label.toLowerCase()}. Agora é ver quem tem coragem. Ou dívida.`);
  return bounty;
}
// Caça: a recompensa fica presa à caveira e vale enquanto ela andar.
function huntBounty(monsterId) {
  const monster = (state.bandits || []).find(actor => actor.id === monsterId && actor.alive);
  if (!monster || state.bounties.some(bounty => bounty.target === monsterId)) return;
  createBounty({ x: monster.x, y: monster.y, target: monster.id, label: `Caçar: ${monster.name}` });
}
const bountyPos = bounty => {
  if (!bounty.target) return bounty;
  const monster = (state.bandits || []).find(actor => actor.id === bounty.target);
  return monster ? { x: monster.x, y: monster.y } : bounty;
};
// Caveira derrubada: quem derrubou embolsa; se não foi herói, quem aceitou; senão o ouro volta.
function payHuntBounty(monster, killer) {
  const bounty = (state.bounties || []).find(item => item.target === monster.id);
  if (!bounty) return '';
  const taker = bountyTaker(bounty);
  const winner = killer && state.heroes.includes(killer) ? killer : taker;
  if (taker) taker.bountyId = null;
  state.bounties = state.bounties.filter(item => item !== bounty);
  if (winner) { lootHero(winner, bounty.reward); return ` ${winner.name} embolsou a recompensa de ${bounty.reward} de ouro.`; }
  state.gold += bounty.reward;
  return ` A recompensa de ${bounty.reward} de ouro voltou pro cofre.`;
}
function cancelBounty(id) {
  const bounty = state.bounties.find(item => item.id === id);
  if (!bounty) return;
  const taker = bountyTaker(bounty);
  if (taker) { taker.bountyId = null; taker.route = []; taker.finalPoint = null; }
  state.gold += bounty.reward;
  state.bounties = state.bounties.filter(item => item !== bounty);
  renderSites(); renderPanel(); updateHud();
  say(`Recompensa cancelada. ${bounty.reward} de ouro voltou pro cofre${taker ? `, e ${taker.name} reclamou do caminho à toa` : ''}.`);
}
function startBountyPlacement() {
  state.selectedBuild = null;
  state.movingSite = null;
  state.placingBounty = true;
  renderSites();
  say(`Toque no mapa onde os heróis devem vigiar. Recompensa: ${state.bountyAmount} de ouro. Esc cancela.`);
}
function cancelBountyPlacement() {
  if (!state.placingBounty) return;
  state.placingBounty = false;
  renderSites();
  say('Nenhuma recompensa marcada. Os heróis nem souberam.');
}
function placeBountyAt(x, y) {
  if (x < 3 || x > 97 || y < 3 || y > 197) { say('Fora do mapa. Nem os heróis vão tão longe.'); return; }
  state.placingBounty = false;
  createBounty({ x: Math.round(x * 10) / 10, y: Math.round(y * 10) / 10 });
}
// O herói pesa o valor contra a distância e o perigo; a mania muda a conta.
function bountyScore(hero, bounty) {
  const pos = bountyPos(bounty);
  const dist = Math.hypot(hero.x - pos.x, hero.y - pos.y);
  const far = bounty.route || pos.y > 100;
  let score = bounty.reward / (10 + dist * 0.3);
  switch (hero.trait) {
    case 'Ganancioso': if (bounty.reward < 40) return 0; score *= 1.5; break;
    case 'Covarde': if (bounty.reward < 20 || (far && bounty.reward < 60)) return 0; score *= 0.6; break;
    case 'Bêbado': score *= 0.7; break;
    case 'Valente': score *= 1.25; break;
    case 'Teimoso': if (Math.random() < 0.5) return 0; score *= 1.2; break;
    default: break;
  }
  if (far) score *= 0.85;
  return score;
}
function considerBounties(hero) {
  if (hpOf(hero) < maxHpOf(hero) * 0.5) return null;
  const open = (state.bounties || []).filter(bounty => !bountyTaker(bounty));
  if (!open.length) return null;
  const best = open.map(bounty => ({ bounty, score: bountyScore(hero, bounty) })).sort((a, b) => b.score - a.score)[0];
  if (!best || best.score < 1) return null;
  best.bounty.takenBy = hero.id;
  best.bounty.leg = 0;
  best.bounty.arrivedAt = null;
  hero.bountyId = best.bounty.id;
  say(`${hero.name} aceitou a recompensa de ${best.bounty.reward} de ouro: ${best.bounty.label.toLowerCase()}.`, `${hero.name} foi atrás do ouro. Do ouro, não de você.`);
  if (state.tab === 'heroes') renderPanel();
  return best.bounty;
}
function completeBounty(hero, bounty) {
  lootHero(hero, bounty.reward);
  hero.bountyId = null;
  state.bounties = state.bounties.filter(item => item !== bounty);
  renderSites();
  if (state.tab === 'heroes') renderPanel();
  say(`${hero.name} cumpriu a recompensa (${bounty.label.toLowerCase()}) e embolsou ${bounty.reward} de ouro.`);
}
// Um passo da recompensa: anda até o ponto (ou pelo roteiro da patrulha) e vigia.
function bountyStep(hero, bounty) {
  if (bounty.lairId) { lairBountyStep(hero, bounty); return; }
  if (bounty.target) {
    const monster = (state.bandits || []).find(actor => actor.id === bounty.target && actor.alive);
    if (!monster) return;
    hero.task = bounty.label;
    routeActor(hero, nearestNode(monster));
    hero.finalPoint = { x: monster.x, y: monster.y };
    return;
  }
  if (bounty.route) {
    const node = PATH_NODES[bounty.route[bounty.leg || 0]];
    if (Math.hypot(hero.x - node.x, hero.y - node.y) < 1) {
      bounty.leg = (bounty.leg || 0) + 1;
      if (bounty.leg >= bounty.route.length) { completeBounty(hero, bounty); return; }
    }
    hero.task = `Patrulhando: ${bounty.label}`;
    routeActor(hero, bounty.route[bounty.leg]);
    return;
  }
  if (Math.hypot(hero.x - bounty.x, hero.y - bounty.y) < 1.5) {
    if (bounty.arrivedAt == null) bounty.arrivedAt = state.minutes;
    if (state.minutes - bounty.arrivedAt >= BOUNTY_GUARD) { completeBounty(hero, bounty); return; }
    hero.wait = 2;
    return;
  }
  routeActor(hero, bounty.node);
  hero.finalPoint = { x: bounty.x, y: bounty.y };
}
// Uma vez por hora: recompensa vencida devolve o ouro; herói morto larga a dele.
function processBounties() {
  (state.bounties || []).forEach(bounty => {
    if (bounty.takenBy != null && !bountyTaker(bounty)) { bounty.takenBy = null; bounty.leg = 0; bounty.arrivedAt = null; }
  });
  // Covil que já caiu ou sumiu: o ouro volta.
  (state.bounties || []).filter(bounty => bounty.lairId && !(state.lairs || []).some(lair => lair.id === bounty.lairId && lair.active)).forEach(bounty => {
    const taker = bountyTaker(bounty);
    if (taker) taker.bountyId = null;
    state.gold += bounty.reward;
    state.bounties = state.bounties.filter(item => item !== bounty);
  });
  // Caça cuja caveira sumiu sem ninguém derrubar: o ouro volta.
  (state.bounties || []).filter(bounty => bounty.target && !(state.bandits || []).some(actor => actor.id === bounty.target && actor.alive)).forEach(bounty => {
    state.gold += bounty.reward;
    state.bounties = state.bounties.filter(item => item !== bounty);
  });
  const expired = (state.bounties || []).filter(bounty => !bounty.target && state.minutes > bounty.until && !bountyTaker(bounty));
  expired.forEach(bounty => {
    state.gold += bounty.reward;
    say(`Ninguém quis a recompensa (${bounty.label.toLowerCase()}). ${bounty.reward} de ouro voltou pro cofre, envergonhado.`);
  });
  if (expired.length) { state.bounties = state.bounties.filter(bounty => !expired.includes(bounty)); renderSites(); }
}
function bountyPanelHTML() {
  const amounts = [10, 25, 50, 100];
  const monsters = (state.bandits || []).filter(actor => actor.alive && actor.className === 'Monster');
  const hunts = monsters.filter(monster => !(state.bounties || []).some(bounty => bounty.target === monster.id)).map(monster => {
    const eta = skeletonEta(monster);
    return `<div class="bounty-row monster-row"><span><b>${monster.name}</b><small>nível ${monster.level} · ${eta > 0 ? `chega à cidade em ~${Math.max(1, Math.round(eta / 60))}h` : 'atacando a cidade agora'}</small></span><button type="button" class="hut-upgrade hunt-btn" data-bounty-hunt="${monster.id}">Recompensa · ${state.bountyAmount}</button></div>`;
  }).join('');
  const dens = (state.lairs || []).filter(lair => lair.active && CREATURE_DENS.includes(lair.kind) && !(state.bounties || []).some(bounty => bounty.lairId === lair.id)).map(lair =>
    `<div class="bounty-row monster-row"><span><b>${lair.name}</b><small>nível ${lair.level || 1} · vida ${lairHp(lair)}/${lairMaxHp(lair)} · deixa um Dead Remains</small></span><button type="button" class="hut-upgrade hunt-btn" data-bounty-lair="${lair.id}">Destruir · ${state.bountyAmount}</button></div>`).join('');
  const rows = (state.bounties || []).map(bounty => {
    const taker = bountyTaker(bounty);
    const lair = bounty.lairId ? (state.lairs || []).find(item => item.id === bounty.lairId) : null;
    const status = lair ? `${taker ? `${taker.name} ${(taker.mapId || 'city') === (lair.mapId || 'wilds') ? 'atacando' : 'a caminho'}` : 'Ninguém aceitou ainda'} · vida ${lairHp(lair)}/${lairMaxHp(lair)}`
      : bounty.target ? (taker ? `${taker.name} foi caçar` : 'Ninguém aceitou ainda') : taker ? (bounty.route ? `${taker.name} · trecho ${Math.min(bounty.leg || 0, bounty.route.length)} de ${bounty.route.length}` : bounty.arrivedAt != null ? `${taker.name} vigiando · ${Math.max(0, BOUNTY_GUARD - (state.minutes - bounty.arrivedAt))} min` : `${taker.name} a caminho`) : 'Ninguém aceitou ainda';
    return `<div class="bounty-row"><span><b>${bounty.label}</b><small>${status}${bounty.target ? '' : ` · vence em ${Math.max(0, Math.ceil((bounty.until - state.minutes) / 60))}h`}</small></span><em>${bounty.reward}</em><button type="button" class="bounty-cancel" data-bounty-cancel="${bounty.id}" title="Cancelar e devolver o ouro">×</button></div>`;
  }).join('');
  return `<section class="farm-selector hut-box bounty-box"><div class="farm-selector-heading"><b>Recompensas</b><small>você paga, eles decidem</small></div>
    <div class="bounty-amount"><span>Valor</span>${amounts.map(value => `<button type="button" class="bounty-chip${state.bountyAmount === value ? ' active' : ''}" data-bounty-amount="${value}">${value}</button>`).join('')}<input type="number" min="5" step="5" value="${state.bountyAmount}" data-bounty-input aria-label="Valor da recompensa"></div>
    <div class="guild-branches"><button type="button" class="hut-upgrade" data-bounty-place>${state.placingBounty ? 'Toque no mapa…' : 'Marcar no mapa'}</button><button type="button" class="hut-upgrade" data-bounty-preset="south">Patrulhar o segundo mapa</button></div>
    <button type="button" class="hut-upgrade bounty-lair" data-bounty-preset="lair">Vigiar as ruínas do covil</button>
    <p class="storage-items">O ouro sai do cofre agora. Ganancioso só vai por 40 ou mais; Covarde não vai longe por pouco; Teimoso às vezes nem escuta.</p>
    ${hunts ? `<div class="food-label">CAVEIRAS NA ESTRADA</div><div class="bounty-list">${hunts}</div>` : ''}
    ${dens ? `<div class="food-label">COVIS NOS WILDS</div><div class="bounty-list">${dens}</div>` : ''}
    ${rows ? `<div class="bounty-list">${rows}</div>` : ''}</section>`;
}
function renderBountyFlags() {
  // Camada própria, por cima de tudo no mapa: construções, árvores, covis e personagens.
  const layer = $('#flag-layer');
  const ids = new Set((state.bounties || []).filter(bounty => !bounty.target && !bounty.lairId).map(bounty => bounty.id));
  $$('.bounty-flag', layer).filter(flag => !ids.has(flag.dataset.bounty)).forEach(flag => flag.remove());
  (state.bounties || []).filter(bounty => !bounty.target && !bounty.lairId).forEach(bounty => {
    let flag = $(`.bounty-flag[data-bounty="${bounty.id}"]`, layer);
    if (!flag) {
      flag = document.createElement('div');
      flag.className = 'bounty-flag';
      flag.dataset.bounty = bounty.id;
      layer.append(flag);
    }
    flag.style.setProperty('--x', `${bounty.x}%`);
    flag.style.setProperty('--y', `${bounty.y / WORLD_Y_UNITS * 100}%`);
    flag.style.zIndex = String(Math.round(bounty.y * 10) + 2);
    flag.classList.toggle('taken', !!bountyTaker(bounty));
    flag.innerHTML = `<i aria-hidden="true"></i><span>${bounty.reward}</span>`;
    flag.title = `${bounty.label}: ${bounty.reward} de ouro`;
  });
}
// Economia: tudo o que entra e sai, por segundo (comida e madeira) e por dia (ouro).
function economyPanelHTML() {
  const per = value => `${rateNumber(value)}/s`;
  const row = (label, value, note = '') => `<div><dt>${label}${note ? `<small>${note}</small>` : ''}</dt><dd>${value}</dd></div>`;
  const farms = state.buildings.filter(building => building.type === 'farm' && isDone(building));
  const mills = state.buildings.filter(building => building.type === 'mill' && isDone(building));
  const bakeries = state.buildings.filter(building => building.type === 'bakery' && isDone(building));
  const butchers = state.buildings.filter(building => building.type === 'butcher' && isDone(building));
  const fishers = state.workers.filter(worker => worker.alive && worker.profession === 'fisher');
  const camps = state.buildings.filter(building => building.type === 'lumber' && isDone(building));
  const pits = state.buildings.filter(building => isPit(building) && isDone(building));
  const crewOf = building => state.workers.filter(worker => worker.alive && worker.buildSite == null && (worker.workplace ? worker.workplace.site : worker.jobSite) === building.site).length;
  const produced = emptyFood();
  farms.forEach(farm => { produced[farmFoodId(farm)] += farmOutput(farm) * (1 + crewWork(farm)); const secondary = farmType(farm.produce).secondary; if (secondary) produced[secondary] += farmSecondaryRate(farm) * (1 + crewWork(farm)); });
  bakeries.forEach(bakery => { produced[bakeryActiveRecipe(bakery).id] += bakeRate(bakery) * (1 + crewWork(bakery)); });
  state.buildings.filter(building => building.type === 'butcher' && isDone(building)).forEach(building => { produced[butcherActiveRecipe(building).id] += butcherRate(building) * (1 + crewWork(building)); });
  produced.fish += fishers.reduce((sum, worker) => sum + FISHING_RATE * (1 + skillOf(worker, 'fishing') / 100), 0);
  const milling = mills.reduce((sum, mill) => sum + millRate(mill) * (1 + crewWork(mill)), 0);
  const woodRates = camps.map(camp => LUMBER_RATE[workingCrew(camp).length] * (1 + workingCrewWork(camp)));
  const wood = woodRates.reduce((a, b) => a + b, 0);
  const taxes = dailyTaxEstimate();
  const wages = dailyWages();
  const waiting = [...farms, ...mills, ...bakeries, ...butchers].reduce((sum, building) => sum + (building.stock || 0) + (building.secondaryStock || 0), 0);
  const buildingRateText = building => {
    if (building.type === 'mill') return `${per(millRate(building) * (1 + crewWork(building)))} de trigo`;
    if (building.type === 'bakery') return `${per(bakeRate(building) * (1 + crewWork(building)))} de ${foodName(bakeryActiveRecipe(building).id)}`;
    if (building.type === 'butcher') return `${per(butcherRate(building) * (1 + crewWork(building)))} de ${foodName(butcherActiveRecipe(building).id)}`;
    const type = farmType(building.produce);
    const secondary = type.secondary ? ` + ${per(farmSecondaryRate(building) * (1 + crewWork(building)))} de ${foodName(type.secondary)}` : '';
    return `${per(farmOutput(building) * (1 + crewWork(building)))} de ${foodName(farmFoodId(building))}${secondary}`;
  };
  return `<div class="panel-title"><h3>Economia</h3><span>${format(state.gold)} de ouro no cofre</span></div>
    ${demandsHTML()}
    <div class="econ-block"><div class="food-label">OURO POR DIA</div><dl class="econ-lines">
      ${row('Impostos', `+${format(taxes - (state.tavernTab || 0) * taxFactor())}`, `${state.taxPercent}%, da bolsa dos camponeses`)}
      ${row('Taverna', `+${format((state.tavernTab || 0) * taxFactor())}`, 'conta de hoje, paga à meia-noite')}
      ${row('Vendas do Arsenal', `+${format(state.armorySales || 0)}`, 'hoje, já no cofre')}
      ${row('Salários', `−${format(wages)}`, `${WORKER_WAGE} por trabalhador`)}
      ${row('<b>Saldo da meia-noite</b>', `<b>${taxes - wages < 0 ? '−' : '+'}${format(Math.abs(taxes - wages))}</b>`)}
    </dl></div>
    <div class="econ-block"><div class="food-label">MADEIRA POR SEGUNDO</div><dl class="econ-lines">
      ${camps.map((camp, i) => row(`Cabana do Lenhador ${camps.length > 1 ? i + 1 : ''}`, per(woodRates[i]), `${workingCrew(camp).length} de 3 lenhadores no posto`)).join('')}
      ${row('<b>Total</b>', `<b>${per(wood)}</b>`, `${format(state.timber)} no estoque`)}
    </dl></div>
    ${pits.length ? `<div class="econ-block"><div class="food-label">PEDRA E EXTRAÇÃO POR SEGUNDO</div><dl class="econ-lines">
      ${pits.map(pit => row(buildingName(pit), `${fineRate(pitRate(pit))}/s ${pit.type === 'mine' ? 'kg de rocha' : `kg de ${pit.quarryOrder === 'marble' ? 'Mármore' : 'Pedra de Construção'}`}`, `${pit.type === 'mine' ? `${selectedSeam(pit) ? oreName(selectedSeam(pit).mineral) : 'sem veio'} · ` : ''}${lumberCrew(pit).length} de ${workerLimitForPit(pit)} camponeses`)).join('')}
      ${row('<b>Pedra de Construção</b>', `<b>${fineRate(rates().stone)}/s</b>`, `${formatMass(state.stone || 0)} kg no estoque`)}
      ${row('<b>Mármore</b>', `<b>${fineRate(rates().marble)}/s</b>`, `${formatMass(state.marble || 0)} kg no estoque · usado na mansão`)}
      ${METAL_STOCK.map(id => row(`<b>${METAL_NAMES[id]}</b>`, id.endsWith('Ore') ? 'rocha bruta entregue pelo posto' : id === 'steel' ? 'estoque de corte cementado' : 'produção do Blacksmith', `${formatMetalAmount(id, state.metals[id] || 0)} no estoque`)).join('')}
    </dl></div>` : ''}
    ${state.buildings.some(building => building.type === 'blacksmith' && isDone(building)) ? `<div class="econ-block"><div class="food-label">BLACKSMITH</div><dl class="econ-lines">${state.buildings.filter(building => building.type === 'blacksmith' && isDone(building)).map(building => row(siteLabel(building), `${blacksmithRecipe(building).name} · ${rateNumber(blacksmithRate(building) * (1 + crewWork(building)))}/s`, `nível ${producerLevel(building) + 1} de 2 · receita selecionada`)).join('')}</dl></div>` : ''}
    <div class="econ-block"><div class="food-label">COMIDA POR SEGUNDO</div><dl class="econ-lines">
      ${FOOD_TYPES.filter(food => food.id !== 'flour').map(food => row(food.name, produced[food.id] ? `${food.baked ? 'até ' : ''}+${per(produced[food.id])}` : '—', `${format(state.food[food.id] || 0)} no depósito${food.raw ? ' · vai pro moinho' : food.baked ? ' · da padaria' : ''} · alimenta ${food.feeds}×`)).join('')}
      ${row('Farinha', milling ? `até +${per(milling)}` : '—', `${format(state.food.flour || 0)} no depósito · ${mills.length ? `${mills.length} ${mills.length === 1 ? 'moinho' : 'moinhos'}` : 'sem moinho'}`)}
      ${row('Pesca nas Wilds', fishers.length ? `+${per(produced.fish)}` : '—', `${fishers.length} pescador${fishers.length === 1 ? '' : 'es'} · pontos fixos no riacho das Wilds`)}
      ${row('Consumo dos camponeses', `−${per(foodDemand())}`, `${eaters()} bocas · ração ${RATIONS[state.ration].label.toLowerCase()}`)}
      ${row('Esperando carregador', format(waiting), 'nas fazendas, moinhos, padarias e açougues')}
      ${row('Depósito', `${Math.floor(storedUnits())} / ${storageCap()}`)}
    </dl></div>
    <div class="econ-block"><div class="food-label">POR CONSTRUÇÃO</div><dl class="econ-lines">
      ${[...farms, ...mills, ...bakeries, ...butchers].map(building => row(siteLabel(building), buildingRateText(building),
        `nível ${producerLevel(building) + 1} · ${crewOf(building)} ${crewOf(building) === 1 ? 'trabalhador' : 'trabalhadores'}${crewWork(building) ? ` · +${Math.round(crewWork(building) * 100)}%` : ''} · ${format(building.stock || 0)} esperando`)).join('') || '<p class="storage-items">Nenhuma fazenda, moinho ou padaria ainda.</p>'}
    </dl></div>
    <details class="econ-block"><summary class="food-label">CUSTOS PARA CONSTRUIR</summary><dl class="econ-lines">
      ${BUILDINGS.map(building => row(building.name, `${building.cost} ouro · ${building.wood} madeira`)).join('')}
    </dl><p class="storage-items">Custos únicos; melhorias têm seus próprios valores.</p></details>`;
}

function foodPanelHTML() {
  const prod = foodProduction();
  const milling = millCapacity();
  const parts = { ration: 0, food: 0, tax: 0, hunger: 0, empathy: 0, ...(state.moodParts || {}) };
  const signed = n => `${n > 0 ? '+' : n < 0 ? '−' : ''}${Math.abs(n)}`;
  const n = eaters();
  return `<div class="panel-title"><h3>Alimentos e consumo</h3><span>${n} ${n === 1 ? 'boca' : 'bocas'} · ${foodDemand().toFixed(1).replace('.', ',')} por segundo</span></div>
    <div class="food-block"><div class="food-label">RAÇÃO</div>
      <div class="segmented ration">${RATIONS.map((ration, i) => `<button type="button" data-ration="${i}" class="${state.ration === i ? 'active' : ''}"><b>${ration.label}</b><small>${hn(eaters() * WORKER_UPKEEP * ration.mult)}/s</small><em class="${ration.mood > 0 ? 'mood-up' : ration.mood < 0 ? 'mood-down' : ''}">${ration.mood > 0 ? '+' : ration.mood < 0 ? '−' : ''}${Math.abs(ration.mood)} felicidade</em></button>`).join('')}</div>
      <p class="storage-items">Cada camponês come ${hn(WORKER_UPKEEP)} de provisão por segundo na ração normal. Meia come metade; dobrada, o dobro; tripla, o triplo.</p>
      <p class="storage-items">A provisão se mede em pão. ${[1, 2, 4].map(n => `${FOOD_TYPES.filter(food => !food.raw && food.feeds === n).map(food => food.name.toLowerCase()).join(', ')} alimentam ${n}×`).join('; ')}: quanto mais alimenta, menos sai do estoque.</p>
      <p class="ration-effect">Efeito na felicidade do povo: a ração <b>${RATIONS[state.ration].label.toLowerCase()}</b> ${RATIONS[state.ration].mood > 0 ? 'deixa o povo mais feliz' : RATIONS[state.ration].mood < 0 ? 'deixa o povo mais triste' : 'não muda a felicidade'} (${RATIONS[state.ration].mood > 0 ? '+' : RATIONS[state.ration].mood < 0 ? '−' : ''}${Math.abs(RATIONS[state.ration].mood)}). Quanto mais comida, mais feliz o povo, e mais rápido o estoque acaba. Se a comida servida não der para todos, a fome tira até 40.</p></div>
    <div class="food-label">CARDÁPIO</div>
    <div class="food-list">${[...FOOD_TYPES].sort((a, b) => a.name.localeCompare(b.name, 'pt-BR')).map(food => { const on = !!state.diet[food.id]; const stock = state.food[food.id] || 0; return `<div class="food-row${on ? ' on' : ''}${stock <= 0 ? ' empty' : ''}">
        <img src="public/art/${foodIcon(food)}" alt=""><span class="food-name"><b>${food.name}</b><small>${food.raw ? '' : `Alimenta ${food.feeds}×. `}${food.note}</small></span>
        <span class="food-stock"><b>${format(stock)}</b><small>${food.id === 'flour' ? (milling ? `até +${rateNumber(milling)}/s` : 'sem moinho') : food.baked ? (bakingCapacity(food.id) ? `até +${rateNumber(bakingCapacity(food.id))}/s` : 'sem padaria assando') : prod[food.id] ? `+${rateNumber(prod[food.id])}/s${food.raw && milling ? ` · moinho −${rateNumber(milling)}/s` : ''}` : 'sem produção'}</small></span>
        ${food.raw ? '<button type="button" disabled>PRO MOINHO</button>' : `<button type="button" data-diet="${food.id}" class="${on ? 'on' : ''}">${on ? 'SERVINDO' : 'GUARDADO'}</button>`}</div>`; }).join('')}</div>
    <div class="mood-box"><div class="mood-head"><span>FELICIDADE</span><b>${Math.round(state.happiness)}</b><em>${happinessLabel(state.happiness)}</em><small>média da semana ${Math.round(moodWeekAverage())} · muda no dia ${nextMoodDay()}</small></div>
      <div class="mood-bar"><i style="width:${Math.round(state.happiness)}%"></i></div>
      <dl class="mood-parts"><div><dt>Ração</dt><dd>${signed(parts.ration)}</dd></div><div><dt>Comida</dt><dd>${signed(parts.food)}</dd></div><div><dt>Imposto</dt><dd>${signed(parts.tax)}</dd></div><div><dt>Fome</dt><dd>${signed(parts.hunger)}</dd></div><div><dt>Empatia</dt><dd>${signed(parts.empathy || 0)}</dd></div></dl></div>
    <div class="mood-rules"><div class="food-label">O QUE MUDA A FELICIDADE</div>
      <p>A felicidade muda uma vez por semana, à meia-noite do 7º dia: anda até ${MOOD_WEEKLY_STEP} pontos na direção da média da semana, que é 50 somado a estes efeitos:</p>
      <dl>
        <div><dt>Ração</dt><dd>Nenhuma −20 · Meia −8 · Normal 0 · Dobrada +5 · Tripla +10</dd></div>
        <div><dt>Comida</dt><dd>Cada alimento servido soma: ${FOOD_TYPES.filter(food => !food.raw).map(food => `${food.name.toLowerCase()} +${food.mood}`).join(' · ')}</dd></div>
        <div><dt>Imposto</dt><dd>0% +10 · 5% +5 · 10% 0 · 15% −5 · 20% −10</dd></div>
        <div><dt>Fome</dt><dd>Quando a comida servida não dá pra todos: até −20</dd></div>
        <div><dt>Empatia</dt><dd>Cada ponto de Empatia dos camponeses (passivos de Criador) soma +0,5</dd></div>
      </dl>
      <p>Só conta o que está marcado como SERVINDO e tem estoque. Trigo cru não vai pra mesa: o moinho transforma em farinha.</p></div>`;
}



// ==========================================================================
// Ladrões e combate automático. Ladrões chegam pela estrada velha a cada 6 horas de jogo,
// vão roubar uma construção e fogem. Heróis que os veem lutam sozinhos (idle combat).
// Dano proporcional (ver strike); experiência segue o Ember: 13 + 3 × (nível do alvo − nível de quem bate).
// ==========================================================================
// Caveiras: toda noite, às 22h, o cemitério dos Wilds (ponta sul da estrada velha) solta uma.
// Ela sobe a estrada devagar até a cidade; chegando, ataca as construções até alguém derrubar.
// Mais dura que um Salteador: um herói sozinho do mesmo nível luta parelho; dois resolvem.
const SKELETON_TABLE = statTable(3, [44, 12, 0, 6, 2]);
// Onde a estrada dos Wilds encosta na borda de cima do mapa (um ponto por conjunto de mapas).
const WILDS_ROAD_EXIT = { 1: { x: 57.5, y: 1 }, 2: { x: 64, y: 1 } };
const SKELETON_PATH = [18, 17, 16, 15, 14, 13, 12, 5, 4, 2, 1];
const SKELETON_SPEED = 3.6; // monstros andam no passo dos outros personagens (ladrões: 3,6)
const MONSTER_ROUTE_LANES = [0, -1, 1, -2, 2, -3, 3];
function assignMonsterRouteLane(monster) {
  const nearby = (state.bandits || []).filter(other => other !== monster && other.alive && other.className === 'Monster'
    && (other.mapId || 'city') === (monster.mapId || 'city') && (other.mapSet || 1) === (monster.mapSet || 1));
  const counts = new Map(MONSTER_ROUTE_LANES.map(lane => [lane, 0]));
  nearby.forEach(other => { if (counts.has(other.routeLane)) counts.set(other.routeLane, counts.get(other.routeLane) + 1); });
  monster.routeLane = MONSTER_ROUTE_LANES.find(lane => counts.get(lane) === 0)
    ?? MONSTER_ROUTE_LANES.reduce((best, lane) => counts.get(lane) < counts.get(best) ? lane : best, MONSTER_ROUTE_LANES[0]);
  monster.routeMap = `${monster.mapId || 'city'}:${monster.mapSet || 1}`;
  return monster.routeLane;
}
function ensureMonsterRouteLane(monster) {
  const mapKey = `${monster.mapId || 'city'}:${monster.mapSet || 1}`;
  if (!Number.isFinite(monster.routeLane) || monster.routeMap !== mapKey) assignMonsterRouteLane(monster);
  return monster.routeLane;
}
function monsterRoutePoint(path, index, lane) {
  const node = PATH_NODES[path[index]];
  const previous = PATH_NODES[path[Math.max(0, index - 1)]];
  const next = PATH_NODES[path[Math.min(path.length - 1, index + 1)]];
  let dx = next.x - previous.x, dy = next.y - previous.y;
  const length = Math.hypot(dx, dy) || 1;
  dx /= length; dy /= length;
  const offset = lane * 1.15;
  return { x: node.x - dy * offset, y: node.y + dx * offset };
}
function monsterWildsExit(monster) {
  if (!monster.routeExit) {
    const exit = WILDS_ROAD_EXIT[monster.mapSet === 2 ? 2 : 1];
    const dx = exit.x - monster.x, dy = exit.y - monster.y, length = Math.hypot(dx, dy) || 1;
    const offset = ensureMonsterRouteLane(monster) * 0.8;
    monster.routeExit = { x: exit.x - dy / length * offset, y: exit.y + dx / length * offset };
  }
  return monster.routeExit;
}
const SKELETON_NAMES = ['do Coveiro Esquecido', 'Sem Lápide', 'da Cova Rasa', 'do Velório Barato', 'Que Ninguém Chorou', 'do Jazigo Vendido', 'da Vala Comum'];
const isHostile = actor => actor.className === 'Bandit' || actor.className === 'Monster';
function spawnSkeleton(lair) {
  if (!lair?.active || lair.kind !== 'cemetery' || lair.level !== 1) return;
  const day = Math.floor(state.minutes / 1440) + 1;
  const start = PATH_NODES[SKELETON_PATH[0]];
  const monster = {
    id: `skeleton-${Date.now()}`, className: 'Monster', kind: 'skeleton', spriteId: 'skeleton-v02', mapId: lair.mapId || 'city', mapSet: lair.mapSet || 1, lairId: lair.id, name: `Caveira ${pick(SKELETON_NAMES)}`,
    level: Math.min(HEIR_MAX_LEVEL, day), alive: true, x: start.x, y: Math.min(198, start.y + 2), leg: 0, mode: 'walk', bornAt: state.minutes,
  };
  assignMonsterRouteLane(monster);
  const laneStart = monsterRoutePoint(SKELETON_PATH, 0, monster.routeLane);
  monster.x = laneStart.x; monster.y = Math.min(198, laneStart.y + 2);
  state.bandits.push(monster);
  say(`${lair.name} soltou uma caveira. Ela vem subindo a estrada velha, devagar. Dá tempo de pagar alguém pra ir atrás.`, 'Uma caveira vem pela estrada velha.');
  if (state.tab === 'heroes') renderPanel();
}
// Quanto falta (em minutos de jogo, na velocidade 1) para a caveira chegar à cidade.

function spawnWolf(lair) {
  if (!lair?.active || lair.kind !== 'wolf-den') return;
  const n = (state.bandits || []).filter(actor => actor.kind === 'wolf' && actor.lairId === lair.id).length + 1;
  const monster = { id: `wolf-${lair.id}-${state.minutes}`, className: 'Monster', kind: 'wolf', spriteId: 'mordavian-wolf-v01', mapId: lair.mapId || 'wilds', mapSet: lair.mapSet || 1, lairId: lair.id, name: `Lobo mordaviano ${n}`, level: lair.level || 1, alive: true, x: lair.x, y: lair.y, mode: 'approach-road', bornAt: state.minutes };
  assignMonsterRouteLane(monster); monsterWildsExit(monster);
  state.bandits.push(monster);
  say(`Um novo lobo mordaviano saiu de ${lair.name} e desce em direcao a Estrada Velha.`);
}
function spawnRandomNekomata() {
  const locations = [{ x: 16, y: 31 }, { x: 31, y: 55 }, { x: 48, y: 83 }, { x: 72, y: 42 }, { x: 79, y: 116 }, { x: 23, y: 143 }, { x: 54, y: 158 }, { x: 91, y: 88 }];
  const spot = pick(locations);
  const monster = { id: `nekomata-wild-${state.playSeconds}`, className: 'Monster', kind: 'nekomata', spriteId: 'nekomata', name: 'Nekomata errante', level: 1, alive: true, mapId: 'wilds', mapSet: state.mapSet || 1, x: spot.x, y: spot.y, mode: 'approach-road', bornAt: state.minutes, wildNekomata: true };
  assignMonsterRouteLane(monster); monsterWildsExit(monster);
  state.bandits.push(monster);
  say('Uma Nekomata errante apareceu nos Wilds.');
}
function spawnLairCreature(lair) {
  if (!lair?.active) return;
  const creatureByDen = {
    'nekomata-den': { kind: 'nekomata', spriteId: 'nekomata', name: 'Nekomata' },
    'goblin-den': { kind: 'goblin', spriteId: 'mordavian-goblin-v01', name: 'Goblin' },
    'troll-den': { kind: 'troll', spriteId: 'mordavian-troll-v01', name: 'Troll' },
  };
  const creature = creatureByDen[lair.kind];
  if (!creature) return;
  const { kind, spriteId, name } = creature;
  const n = (state.bandits || []).filter(actor => actor.kind === kind && actor.lairId === lair.id).length + 1;
  const monster = { id: `${kind}-${lair.id}-${state.minutes}`, className: 'Monster', kind, spriteId, mapId: lair.mapId || 'wilds', mapSet: lair.mapSet || 1, lairId: lair.id, name: `${name} ${n}`, level: lair.level || 1, alive: true, x: lair.x, y: lair.y, mode: 'approach-road', bornAt: state.minutes };
  assignMonsterRouteLane(monster); monsterWildsExit(monster);
  state.bandits.push(monster);
  say(`${name} saiu de ${lair.name} e avança pela Estrada Velha.`);
}
function spawnZombie() {
  const lair = (state.lairs || []).find(item => item.active && item.kind === 'cemetery' && item.level === 2);
  if (!lair?.active || lair.kind !== 'cemetery' || lair.level !== 2) return;
  const monster = { id: `zombie-${Date.now()}`, className: 'Monster', kind: 'zombie', spriteId: 'zombie-v01', mapId: lair.mapId || 'wilds', mapSet: lair.mapSet || 1, lairId: lair.id, name: 'Zumbi da cova funda', level: Math.min(HEIR_MAX_LEVEL, Math.floor(state.minutes / 1440) + 1), alive: true, x: lair.x, y: lair.y, mode: 'approach-road', leg: 0, bornAt: state.minutes };
  assignMonsterRouteLane(monster); monsterWildsExit(monster);
  state.bandits.push(monster);
  say('Uma cova funda se abriu. Um zumbi saiu do cemiterio level 2.');
}
function skeletonEta(monster) {
  if (monster.mode !== 'walk') return 0;
  let dist = 0, from = monster;
  for (let i = monster.leg || 0; i < SKELETON_PATH.length; i++) { const node = PATH_NODES[SKELETON_PATH[i]]; dist += Math.hypot(node.x - from.x, node.y - from.y); from = node; }
  return dist / SKELETON_SPEED;
}
function monsterStep(monster, delta, reducedMotion) {
  ensureMonsterRouteLane(monster);
  monster.cooldown = (monster.cooldown || 0) - delta;
  monster.calm = Math.max(0, (monster.calm || 0) - delta);
  const heroes = [...state.heroes, ...(window.Monarch?.allyTargets() || [])].filter(hero => hero.alive && (hero.mapId || 'city') === (monster.mapId || 'city') && distanceTo(monster, hero) < 6);
  const foe = window.Monarch?.tauntTarget(monster) || heroes.sort((x, y) => distanceTo(monster, x) - distanceTo(monster, y))[0] || state.workers.find(worker => worker.alive && (worker.mapId || 'city') === (monster.mapId || 'city') && distanceTo(monster, worker) < 2.5) || null;
  if (monster.kind === 'nekomata' && monster.attackAnim > 0) { animateAttack(monster, delta, reducedMotion); return; }
  if (foe) {
    if (distanceTo(monster, foe) > MELEE_RANGE) { animateWalk(monster, moveToward(monster, foe, SKELETON_SPEED * delta) > 0, delta, reducedMotion); return; }
    faceToward(monster, foe);
    if (monster.cooldown <= 0) {
      if (monster.kind === 'nekomata') {
        const clips = ['atk', 'cast', 'special'];
        monster.nekoAttackClip = clips[(monster.nekoAttackIndex || 0) % clips.length];
        monster.nekoAttackIndex = (monster.nekoAttackIndex || 0) + 1;
        monster.attackAnim = NEKOMATA_FRAME_MS[monster.nekoAttackClip] * 36 / 1000;
        monster.cooldown = monster.attackAnim;
      } else { monster.cooldown = ATTACK_COOLDOWN; monster.attackAnim = 0.5; }
      monster.actionClock = 0; strike(monster, foe);
    }
    animateAttack(monster, delta, reducedMotion);
    return;
  }
  if (monster.mode === 'approach-road') {
    // Nos Wilds a estrada sobe para a cidade: a saída é a ponta de cima, não a de baixo.
    const entry = { x: PATH_NODES[18].x, y: PATH_NODES[18].y };
    const inWilds = (monster.mapId || 'city') === 'wilds';
    const exit = inWilds ? monsterWildsExit(monster) : entry;
    animateWalk(monster, moveToward(monster, exit, SKELETON_SPEED * delta) > 0, delta, reducedMotion);
    if (distanceTo(monster, exit) < 0.5) {
      monster.mapId = 'city'; monster.mode = 'walk'; monster.leg = 0; monster.routeExit = null;
      assignMonsterRouteLane(monster);
      const start = monsterRoutePoint(SKELETON_PATH, 0, monster.routeLane);
      monster.x = start.x; monster.y = start.y;
    }
    return;
  }
  if (monster.mode === 'walk') {
    const node = monsterRoutePoint(SKELETON_PATH, monster.leg || 0, monster.routeLane);
    animateWalk(monster, moveToward(monster, node, SKELETON_SPEED * delta) > 0, delta, reducedMotion);
    if (distanceTo(monster, node) < 0.5) {
      monster.leg = (monster.leg || 0) + 1;
      if (monster.leg >= SKELETON_PATH.length) {
        monster.mode = 'siege';
        say(`${monster.name} chegou à cidade e começou a bater nas paredes. Com os ossos.`);
      }
    }
    return;
  }
  // Na cidade: escolhe a construção mais perto e bate até virar ruína.
  let target = monster.targetSite != null ? repairSites().find(building => building.site === monster.targetSite && !building.ruined && !building.underConstruction) : null;
  if (!target) {
    const options = repairSites().filter(building => !building.ruined && !building.underConstruction);
    target = options.sort((a, b) => distanceTo(monster, a) - distanceTo(monster, b))[0] || null;
    monster.targetSite = target ? target.site : null;
  }
  const spot = target ? { x: target.x, y: Math.min(198, target.y + 1.2) } : { x: 42, y: 40 };
  if (distanceTo(monster, spot) > 1.5) { animateWalk(monster, moveToward(monster, spot, SKELETON_SPEED * delta) > 0, delta, reducedMotion); return; }
  if (!target) { animateWalk(monster, false, delta, reducedMotion); return; }
  faceToward(monster, target);
  if (monster.cooldown <= 0) {
    if (monster.kind === 'nekomata') {
      const clips = ['atk', 'cast', 'special'];
      monster.nekoAttackClip = clips[(monster.nekoAttackIndex || 0) % clips.length];
      monster.nekoAttackIndex = (monster.nekoAttackIndex || 0) + 1;
      monster.attackAnim = NEKOMATA_FRAME_MS[monster.nekoAttackClip] * 36 / 1000;
      monster.cooldown = monster.attackAnim;
    } else { monster.cooldown = ATTACK_COOLDOWN * 1.5; monster.attackAnim = 0.5; }
    monster.actionClock = 0;
    damageBuilding(target, Math.round(statsOf(monster).atk * 1.5));
    if (target.ruined) monster.targetSite = null;
  }
  animateAttack(monster, delta, reducedMotion);
}

const RAID_EVERY = 360;                 // minutos de jogo entre bandos
const AGGRO_RANGE = 22;                 // heróis enxergam ladrões até esta distância
const MELEE_RANGE = 2.5;
const SHOT_RANGE = 10;
const ATTACK_COOLDOWN = 1.2;            // segundos de jogo entre golpes
const RANGED_CLASSES = ['Archer', 'Ranger', 'Assassin', 'Black Mage', 'Elementalist', 'Warlock', 'Conjurer', 'Sorcerer', 'Necromancer'];
const MAGIC_CLASSES = ['Black Mage', 'Elementalist', 'Warlock', 'Conjurer', 'Sorcerer', 'Necromancer'];
const HEALER_CLASSES = ['Healer', 'Bishop', 'Cleric'];
const DEATH_MSG = [
  n => `${n} morreu. Deixou dívidas na taverna.`, n => `${n} morreu fazendo o que amava: fugindo devagar demais.`,
  n => `${n} morreu. A família vai receber uma carta. Ou não.`, n => `${n} virou adubo. Os campos agradecem.`, n => `${n} morreu. Era o favorito de ninguém.`,
];
const isBandit = actor => actor.className === 'Bandit';
const isWildlife = actor => actor.className === 'Wildlife';
const classOf = actor => actor.guildForm || actor.prestige || actor.className;
// Atributos de luta: os da classe no nível, mais o equipamento.
// Invocações do Monarca trazem os próprios atributos (fixedStats); auras e debuffs dele entram por último.
// No camponês, os atributos das profissões também entram.
const statBonus = (actor, key) => (actor.gear ? gearBonus(actor, key) : 0) + (tradeCombat(actor)[key] || 0);
const statsOf = actor => {
  const raw = actor.fixedStats ? { ...actor.fixedStats } : attributesAt(tableFor(actor), actor.level || 1);
  const base = !actor.gear && !actor.trades ? raw
    : { ...raw, hp: raw.hp + statBonus(actor, 'hp'), atk: raw.atk + statBonus(actor, 'atk'), mag: raw.mag + statBonus(actor, 'mag'), def: raw.def + statBonus(actor, 'def'), dex: raw.dex + statBonus(actor, 'dex') };
  return window.Monarch ? Monarch.statMods(actor, base) : base;
};
const maxHpOf = actor => statsOf(actor).hp;
const hpOf = actor => (actor.hp == null ? maxHpOf(actor) : actor.hp);
const distanceTo = (a, b) => Math.hypot(a.x - b.x, a.y - b.y);
const raidEntry = () => PATH_NODES[18] || PATH_NODES[9];

function moveToward(actor, target, step) {
  const dx = target.x - actor.x, dy = target.y - actor.y;
  const distance = Math.hypot(dx, dy);
  if (distance < 0.001) return 0;
  if (actor.kind === 'troll') actor.walkDirection = Math.abs(dy) > Math.abs(dx) ? (dy < 0 ? 'up' : 'down') : 'side';
  // Lentidão, raízes, atordoamento e pressa vêm das magias do Monarca.
  const move = Math.min(distance, step * (window.Monarch?.moveMul(actor) ?? 1));
  if (move <= 0) return 0;
  actor.x += dx / distance * move;
  actor.y += dy / distance * move;
  actor.facing = dy < -0.2 ? 'back' : dy > 0.2 ? 'front' : (actor.facing || 'front');
  if (Math.abs(dx) > 0.2) actor.side = dx < 0 ? 'left' : 'right';
  return move;
}

function faceToward(actor, target) {
  const dx = target.x - actor.x, dy = target.y - actor.y;
  actor.facing = dy < -0.5 ? 'back' : 'front';
  if (Math.abs(dx) > 0.2) actor.side = dx < 0 ? 'left' : 'right';
}

function animateWalk(actor, moved, delta, reducedMotion) {
  if (actor.kind === 'nekomata') actor.nekoMoving = moved;
  actor.action = 'walk';
  actor.walkClock = (actor.walkClock || 0) + (moved ? delta : 0);
  actor.frame = moved && (!reducedMotion || actor.className === 'Peasant') ? Math.floor(actor.walkClock * 7) % 4 : 0;
}

// A espera dos animais usa a folha idle própria (quatro poses), nunca um efeito visual sintético.
function animateIdle(actor, delta, reducedMotion) {
  if (guildHeroArt(actor) && actor.action !== 'idle') actor.idleClock = 0;
  actor.action = 'idle';
  actor.idleClock = (actor.idleClock || 0) + delta;
  const idleRate = guildHeroArt(actor) ? 3.2 : 2.4;
  actor.frame = reducedMotion ? 0 : Math.floor(actor.idleClock * idleRate) % 4;
}

function animateAttack(actor, delta, reducedMotion) {
  if (actor.kind === 'nekomata') actor.nekoMoving = false;
  // O ataque do troll só existe de lado: em combate ele fica de lado entre um golpe e outro,
  // senão alterna com a folha de frente/costas a cada golpe.
  if (actor.kind === 'troll') actor.walkDirection = 'side';
  if ((actor.attackAnim || 0) > 0) {
    actor.attackAnim -= delta;
    actor.action = actor.className === 'Peasant' ? 'fight' : 'action';
    actor.actionClock = (actor.actionClock || 0) + delta;
    actor.frame = reducedMotion && actor.className !== 'Peasant' ? 0 : Math.floor(actor.actionClock * 8) % 4;
  } else {
    if (guildHeroArt(actor)) animateIdle(actor, delta, reducedMotion);
    else { actor.action = 'walk'; actor.frame = 0; }
  }
}

function checkPrestige(hero, before) {
  if (!guildHeroArt(hero) && before < 15 && hero.level >= 15) {
    say(`${hero.name} chegou ao nível 15 e acha que merece mais.`, `${hero.name} procura uma guilda de prestígio. As guildas fingem que não estão.`);
    assignPrestige(hero);
    if (state.tab === 'heroes') renderPanel();
  }
}

function lootText(loot) {
  if (!loot) return 'nada';
  if (loot.gold) return `${loot.gold} de ouro`;
  if (loot.food) return `${loot.food.qty} de ${FOOD_TYPES.find(food => food.id === loot.food.id)?.name.toLowerCase() || 'comida'}`;
  return 'nada';
}

function killActor(victim, killer) {
  if (victim.summon) { window.Monarch?.summonDied(victim); return; }
  victim.alive = false;
  victim.hp = 0;
  if (victim.kind === 'nekomata') {
    nekomataDeaths.push({ ...victim, id: `${victim.id}-death`, nekoDeathClock: 0 });
    if (victim.wildNekomata) {
      state.remainsPiles = state.remainsPiles || [];
      state.remainsPiles.push({ id: `remains-${victim.id}`, grade: 'nekomataWild', mapId: victim.mapId || 'city', mapSet: victim.mapSet || 1, x: victim.x, y: victim.y });
      renderLairs();
    }
  }
  if (isWildlife(victim)) {
    const range = HUNT_MEAT_DROP[victim.kind] || [1, 2];
    const qty = range[0] + Math.floor(Math.random() * (range[1] - range[0] + 1));
    state.wildMeatPiles = state.wildMeatPiles || [];
    state.wildMeatPiles.push({ id: `wild-meat-${victim.kind}-${state.minutes}-${Math.floor(Math.random() * 9999)}`, food: WILDLIFE_MEAT[victim.kind], qty, mapId: 'wilds', mapSet: victim.mapSet || WILDLIFE_MAP_SET, x: victim.x, y: victim.y });
    if (killer?.className === 'Peasant') gainSkill(killer, 'hunting', 1.2);
    say(`${killer?.name || 'Um caçador'} abateu ${victim.name}. ${format(qty)} de ${foodName(WILDLIFE_MEAT[victim.kind])} ficou no chão para recolher.`);
    renderLairs();
  }
  // O cemitério do topo lembra onde, quando e por quem cada herói e camponês caiu.
  if (!isBandit(victim) && victim.className !== 'Monster' && !isWildlife(victim)) {
    victim.deathAt = state.minutes;
    victim.deathMap = victim.mapId || 'city';
    victim.deathMapSet = victim.mapSet || 1;
    victim.deathBy = killer ? killer.name : null;
  }
  if (isWildlife(victim)) {
    // Wildlife creates a meat pile above; skip hostile loot handling.
  } else if (isBandit(victim)) {
    let back = '';
    if (victim.loot?.gold) { state.gold += victim.loot.gold; back = ` Recuperou ${victim.loot.gold} de ouro.`; }
    if (victim.loot?.food) {
      const qty = Math.min(victim.loot.food.qty, Math.max(0, Math.floor(storageCap() - storedUnits())));
      state.food[victim.loot.food.id] = (state.food[victim.loot.food.id] || 0) + qty;
      if (qty) back = ` Recuperou ${qty} de ${FOOD_TYPES.find(food => food.id === victim.loot.food.id)?.name.toLowerCase()}.`;
    }
    // O que o ladrão tinha no bolso é de quem derrubou: o loot do herói.
    if (killer && !isHostile(killer) && killer.className !== 'Peasant') {
      const coins = 2 + Math.floor(Math.random() * 4) + 2 * (victim.level || 1);
      lootHero(killer, coins);
      back += ` ${killer.name} ficou com ${coins} de ouro do bolso dele.`;
    }
    say(`${killer ? killer.name : 'Alguém'} derrubou ${victim.name}.${back}`, `${victim.name} não volta mais pela estrada velha.`);
  } else if (victim.className === 'Monster') {
    say(`${killer ? killer.name : 'Alguém'} desmontou ${victim.name}. Os ossos voltaram a ser só ossos.${payHuntBounty(victim, killer)}`, `${victim.name} não sobe mais a estrada.`);
  } else if (victim.className === 'Peasant') {
    if (victim.carrying && ['boarMeat', 'elkMeat'].includes(victim.carrying.food)) {
      state.wildMeatPiles = state.wildMeatPiles || [];
      state.wildMeatPiles.push({ id: `wild-meat-dropped-${state.minutes}-${Math.floor(Math.random() * 9999)}`, food: victim.carrying.food, qty: victim.carrying.qty, mapId: victim.mapId || 'city', mapSet: victim.mapSet || 1, x: victim.x, y: victim.y });
      victim.carrying = null; renderLairs();
    }
    const killerName = killer?.name || 'uma causa desconhecida';
    say(`${victim.name} foi ${isFemalePeasant(victim) ? 'morta' : 'morto'} por ${killerName}. ${pick(['Os outros pediram aumento.', 'A colheita atrasou.', 'Ele tinha nome. Ninguém lembra qual.'])}`);
  } else {
    // Herói que morre carregando restos larga tudo no chão; outro herói vai buscar.
    if (victim.remains) {
      state.remainsPiles.push({ id: `remains-${Date.now()}-${Math.floor(Math.random() * 1000)}`, grade: victim.remains, mapId: victim.mapId || 'city', mapSet: victim.mapSet || 1, x: victim.x, y: victim.y });
      victim.remains = null;
      renderLairs();
    }
    const killerName = killer?.name || 'uma causa desconhecida';
    say(`${pick(DEATH_MSG)(victim.name)} Atacante: ${killerName}.`, `${victim.name} morreu pelas mãos de ${killerName}.`);
  }
  updateHud();
  if (state.tab === 'heroes') renderPanel();
}

// Destreza: quem é mais ágil que o outro desvia mais e acha mais pontos fracos. Iguais: 5%; teto de 30%.
const dexChance = (mine, theirs) => Math.max(0, Math.min(0.3, (40 * mine / Math.max(1, mine + theirs) - 15) / 100));

// Um golpe: dano proporcional (ataque² / (ataque + defesa)), então defesa alta reduz o dano mas nunca zera;
// magia ignora metade da defesa. Destreza dá crítico (×1,5) e esquiva. Heróis ganham experiência a cada golpe, mais 25% quando matam.
function wearCraftedWeapon(actor) {
  const item = actor.guildForm === 'Grande Lobo' ? null : craftedWeapon(gearOf(actor).weapon);
  if (!item || item.structuralCondition <= 0) return;
  item.structuralCondition = Math.max(0, item.structuralCondition - 0.5);
  if (item.sharp && item.edgeCondition > 0) item.edgeCondition = Math.max(0, item.edgeCondition - item.wearMultiplier);
}
function strike(attacker, defender) {
  if (window.Monarch?.blocksAttack(attacker)) return;
  const a = statsOf(attacker), d = statsOf(defender);
  const magic = MAGIC_CLASSES.includes(classOf(attacker));
  const power = magic ? a.mag : a.atk;
  const guard = magic ? d.def / 2 : d.def;
  const raw = power * power / Math.max(1, power + guard);
  // Ordem de Aniquilação: crítico garantido, que também não pode ser esquivado.
  const boost = window.Monarch?.strikeBoost(attacker, defender) || null;
  const crit = boost?.crit || Math.random() < dexChance(a.dex, d.dex);
  const dodged = !boost?.crit && Math.random() < dexChance(d.dex, a.dex);
  let damage = dodged ? 0 : Math.max(1, Math.round(raw * (crit ? 1.5 + (boost?.critBonus || 0) : 1) * (0.8 + Math.random() * 0.4)));
  if (window.Monarch) damage = Monarch.adjustDamage(attacker, defender, damage, magic);
  if (!attacker.alive) return; // refletido pelo Santuário do Trono
  if (damage > 0 && !magic) wearCraftedWeapon(attacker);
  defender.hp = hpOf(defender) - damage;
  if (damage > 0 && defender.kind === 'nekomata' && defender.hp > 0) defender.nekoHitClock = 0;
  defender.calm = 5;
  attacker.calm = 5;
  const killed = defender.hp <= 0;
  if (!isHostile(attacker) && !isWildlife(attacker) && attacker.className !== 'Peasant' && !attacker.summon) {
    let xp = Math.max(1, 13 + 3 * ((defender.level || 1) - (attacker.level || 1)));
    if (killed) xp = Math.floor(xp * 1.25);
    const before = attacker.level || 1;
    gainActorXp(attacker, xp);
    checkPrestige(attacker, before);
  }
  window.Monarch?.afterStrike(attacker, defender, damage, killed);
  if (killed && defender.alive) killActor(defender, attacker);
}

function healHit(healer, patient) {
  const amount = Math.max(1, statsOf(healer).mag);
  patient.hp = Math.min(maxHpOf(patient), hpOf(patient) + amount);
  healer.calm = 5;
  const before = healer.level || 1;
  gainActorXp(healer, 13);
  checkPrestige(healer, before);
}

// Herói: procura ladrão (ou ferido, se for curandeiro) por perto; se achar, larga o resto e luta.
function heroCombatStep(hero, delta, reducedMotion) {
  hero.calm = Math.max(0, (hero.calm || 0) - delta);
  const hunterArcher = ['Archer', 'Ranger', 'Assassin'].includes(classOf(hero));
  const bandits = (state.bandits || []).filter(bandit => bandit.alive && !bandit.mfx?.convert && (bandit.className !== 'Wildlife' || hunterArcher));
  let target = null, heal = false;
  if (HEALER_CLASSES.includes(classOf(hero))) {
    const wounded = state.heroes.filter(other => other.alive && other !== hero && hpOf(other) < maxHpOf(other) * 0.7 && distanceTo(hero, other) < AGGRO_RANGE);
    if (wounded.length) { target = wounded.sort((x, y) => distanceTo(hero, x) - distanceTo(hero, y))[0]; heal = true; }
  }
  if (!target && bandits.length) {
    const sameMap = bandits.filter(bandit => (bandit.mapId || 'city') === (hero.mapId || 'city') && (bandit.mapSet || 1) === (hero.mapSet || 1));
    const near = sameMap.filter(bandit => distanceTo(hero, bandit) < AGGRO_RANGE).sort((x, y) => distanceTo(hero, x) - distanceTo(hero, y));
    target = near[0] || sameMap.find(bandit => bandit.id === hero.respondTo) || null;
    if (!target) hero.respondTo = null;
  }
  if (!target) { if (hero.combatTarget) { hero.combatTarget = null; hero.route = []; hero.finalPoint = null; } return false; }
  if (!hero.combatTarget) { hero.route = []; hero.finalPoint = null; hero.wait = 0; }
  hero.combatTarget = target.id;
  hero.cooldown = (hero.cooldown || 0) - delta * (window.Monarch?.attackRate(hero) ?? 1);
  const range = heal ? SHOT_RANGE : RANGED_CLASSES.includes(classOf(hero)) ? SHOT_RANGE : MELEE_RANGE;
  if (distanceTo(hero, target) > range) {
    const moved = moveToward(hero, target, 4.2 * delta);
    animateWalk(hero, moved > 0, delta, reducedMotion);
    return true;
  }
  faceToward(hero, target);
  if (hero.cooldown <= 0) {
    hero.cooldown = ATTACK_COOLDOWN;
    hero.attackAnim = 0.5;
    hero.actionClock = 0;
    if (heal) healHit(hero, target); else strike(hero, target);
  }
  animateAttack(hero, delta, reducedMotion);
  return true;
}

// Camponês: não caça ladrão, mas depois de apanhar revida quem estiver colado nele. Bate fraco.
function peasantHuntStep(worker, delta, reducedMotion) {
  if (worker.profession !== 'hunter' || (worker.mapId || 'city') !== 'wilds' || (worker.mapSet || 1) !== WILDLIFE_MAP_SET) return false;
  const animal = (state.bandits || []).filter(item => item.alive && item.className === 'Wildlife' && (item.mapSet || WILDLIFE_MAP_SET) === WILDLIFE_MAP_SET)
    .sort((a, b) => distanceTo(worker, a) - distanceTo(worker, b))[0];
  if (!animal) return false;
  if (distanceTo(worker, animal) > MELEE_RANGE) {
    const moved = moveToward(worker, animal, 3.5 * delta);
    animateWalk(worker, moved > 0, delta, reducedMotion);
    return true;
  }
  faceToward(worker, animal);
  worker.cooldown = (worker.cooldown || 0) - delta;
  if (worker.cooldown <= 0) { worker.cooldown = ATTACK_COOLDOWN; worker.attackAnim = 0.5; worker.actionClock = 0; gainSkill(worker, 'hunting', 0.3); strike(worker, animal); }
  animateAttack(worker, delta, reducedMotion);
  return true;
}

function peasantDefenseStep(worker, delta, reducedMotion) {
  worker.calm = Math.max(0, (worker.calm || 0) - delta);
  worker.cooldown = (worker.cooldown || 0) - delta;
  if (!worker.calm) return false;
  const foe = (state.bandits || []).filter(bandit => bandit.alive && !bandit.mfx?.convert && distanceTo(worker, bandit) < MELEE_RANGE + 0.5)
    .sort((x, y) => distanceTo(worker, x) - distanceTo(worker, y))[0];
  if (!foe) return false;
  faceToward(worker, foe);
  if (worker.cooldown <= 0) { worker.cooldown = ATTACK_COOLDOWN; worker.attackAnim = 0.5; worker.actionClock = 0; strike(worker, foe); }
  animateAttack(worker, delta, reducedMotion);
  return true;
}

// Ladrão: revida heróis por perto, ataca camponês colado (espada), senão rouba e foge.
function wildlifeStep(animal, delta, reducedMotion) {
  animal.cooldown = (animal.cooldown || 0) - delta;
  const nearby = [...state.heroes, ...state.workers].filter(actor => actor.alive && (actor.mapId || 'city') === 'wilds' && (actor.mapSet || 1) === (animal.mapSet || WILDLIFE_MAP_SET) && distanceTo(animal, actor) < (actor.className === 'Peasant' && actor.profession !== 'hunter' ? 2.5 : 6));
  const foe = nearby.sort((a, b) => distanceTo(animal, a) - distanceTo(animal, b))[0];
  if (foe) {
    if (distanceTo(animal, foe) > MELEE_RANGE) { animateWalk(animal, moveToward(animal, foe, 3.2 * delta) > 0, delta, reducedMotion); return; }
    faceToward(animal, foe);
    if (animal.cooldown <= 0) { animal.cooldown = ATTACK_COOLDOWN * 1.5; animal.attackAnim = 0.55; animal.actionClock = 0; strike(animal, foe); }
    animateAttack(animal, delta, reducedMotion); return;
  }
  if (animal.wait > 0) {
    animal.wait = Math.max(0, animal.wait - delta);
    animateIdle(animal, delta, reducedMotion); return;
  }
  if (!animal.roamTarget) animal.roamTarget = { x: 10 + Math.random() * 80, y: 18 + Math.random() * 155 };
  if (distanceTo(animal, animal.roamTarget) < 0.65) {
    animal.roamTarget = null; animal.wait = 1.8 + Math.random() * 3.6;
    animateIdle(animal, delta, reducedMotion); return;
  }
  animateWalk(animal, moveToward(animal, animal.roamTarget, 1.25 * delta) > 0, delta, reducedMotion);
}

function banditStep(bandit, delta, reducedMotion) {
  bandit.cooldown = (bandit.cooldown || 0) - delta;
  bandit.calm = Math.max(0, (bandit.calm || 0) - delta);
  const range = bandit.kind === 'crossbow' ? SHOT_RANGE : MELEE_RANGE;
  const heroes = [...state.heroes, ...(window.Monarch?.allyTargets() || [])].filter(hero => hero.alive && distanceTo(bandit, hero) < (bandit.kind === 'crossbow' ? SHOT_RANGE + 2 : 7));
  // Provocação da Guarda Real: o ladrão só enxerga o guarda.
  let foe = window.Monarch?.tauntTarget(bandit) || heroes.sort((x, y) => distanceTo(bandit, x) - distanceTo(bandit, y))[0] || null;
  if (!foe && bandit.kind === 'sword') foe = state.workers.find(worker => worker.alive && distanceTo(bandit, worker) < 3) || null;
  if (foe) {
    if (distanceTo(bandit, foe) > range) {
      const moved = moveToward(bandit, foe, (bandit.kind === 'crossbow' ? 3.2 : 3.6) * delta);
      animateWalk(bandit, moved > 0, delta, reducedMotion);
      return;
    }
    faceToward(bandit, foe);
    if (bandit.cooldown <= 0) { bandit.cooldown = ATTACK_COOLDOWN; bandit.attackAnim = 0.5; bandit.actionClock = 0; strike(bandit, foe); }
    animateAttack(bandit, delta, reducedMotion);
    return;
  }
  const speed = (bandit.kind === 'crossbow' ? 3.2 : 3.6) * delta;
  if (bandit.mode === 'raid') {
    const goal = bandit.goalSpot;
    const moved = moveToward(bandit, goal, speed);
    animateWalk(bandit, moved > 0, delta, reducedMotion);
    if (distanceTo(bandit, goal) < 0.6) stealFrom(bandit);
  } else {
    const exit = raidEntry();
    const moved = moveToward(bandit, exit, speed);
    animateWalk(bandit, moved > 0, delta, reducedMotion);
    if (distanceTo(bandit, exit) < 0.6) {
      bandit.alive = false;
      bandit.escaped = true;
      say(`${bandit.name} escapou pela estrada velha com ${lootText(bandit.loot)}. Ninguém viu nada. Ninguém nunca vê.`);
    }
  }
}

// "do casarão", "da Taverna", "do Mercado": artigo pelo gênero da construção.
function fromPlace(name) {
  if (name.startsWith('o ')) return `do ${name.slice(2)}`;
  const feminine = ['Fazenda', 'Taverna', 'Guilda', 'Torre', 'Casa', 'Cabana'].includes(name.split(' ')[0]);
  return `${feminine ? 'da' : 'do'} ${name}`;
}

function stealFrom(bandit) {
  const where = bandit.goalName || 'o domínio';
  if (state.gold >= 1) {
    const amount = Math.min(Math.floor(state.gold), 10 + 5 * (bandit.level || 1));
    state.gold -= amount;
    bandit.loot = { gold: amount };
  } else {
    const richest = FOOD_TYPES.map(food => ({ id: food.id, qty: state.food[food.id] || 0 })).sort((a, b) => b.qty - a.qty)[0];
    const qty = Math.min(10, Math.floor(richest?.qty || 0));
    if (qty > 0) { state.food[richest.id] -= qty; bandit.loot = { food: { id: richest.id, qty } }; }
  }
  bandit.mode = 'flee';
  updateHud();
  say(`${bandit.name} roubou ${lootText(bandit.loot)} ${fromPlace(where)} e está fugindo pela estrada velha.`);
}

// Bando novo: cresce com os dias.
function spawnBandits() {
  const day = Math.floor(state.minutes / 1440) + 1;
  const count = Math.min(4, 1 + Math.floor((day - 1) / 2));
  const level = Math.min(HEIR_MAX_LEVEL, 1 + Math.floor((day - 1) / 2));
  const entry = raidEntry();
  const targets = [{ x: 42, y: 40, name: 'o casarão' }, ...state.buildings.filter(isDone).map(building => ({ x: building.x, y: Math.min(198, building.y + 1.2), name: buildingName(building) }))];
  for (let i = 0; i < count; i++) {
    const kind = Math.random() < 0.4 ? 'crossbow' : 'sword';
    const goal = pick(targets);
    state.bandits.push({
      id: `bandit-${Date.now()}-${i}`, className: 'Bandit', kind, spriteId: kind === 'crossbow' ? 'bandit-crossbow' : 'bandit-sword',
      name: `${kind === 'crossbow' ? 'Besteiro' : 'Salteador'} ${pick(EPITHETS)}`, level, alive: true,
      x: entry.x + (Math.random() * 4 - 2), y: entry.y - i * 1.5, mode: 'raid', goalSpot: { x: goal.x, y: goal.y }, goalName: goal.name,
    });
  }
  say(count === 1 ? 'Um ladrão apareceu na estrada velha. Ele não veio pedir informação.' : `${count} ladrões apareceram na estrada velha. Eles não vieram pedir informação.`);
}

function spawnWildlife() {
  const live = (state.bandits || []).filter(actor => isWildlife(actor) && actor.alive).length;
  const count = Math.min(WILDLIFE_STAY_CAP - live, Math.floor(Math.random() * (WILDLIFE_SPAWN_MAX + 1)));
  const locations = [
    { x: 16, y: 31 }, { x: 31, y: 55 }, { x: 48, y: 83 }, { x: 72, y: 42 }, { x: 79, y: 116 }, { x: 23, y: 143 }, { x: 54, y: 158 }, { x: 91, y: 88 },
  ];
  for (let i = 0; i < count; i++) {
    const kind = Math.random() < 0.58 ? 'boar' : 'elk';
    const base = pick(locations);
    const stats = WILDLIFE_STATS[kind];
    state.bandits.push({ id: `wildlife-${kind}-${state.minutes}-${Math.floor(Math.random() * 99999)}`, className: 'Wildlife', kind, spriteId: WILDLIFE_ART[kind], mapId: 'wilds', mapSet: WILDLIFE_MAP_SET, name: `${WILDLIFE_NAMES[kind]} ${i + 1}`, level: 1, fixedStats: stats, hp: stats.hp, alive: true, x: base.x + Math.random() * 4 - 2, y: base.y + Math.random() * 5 - 2, action: 'idle', frame: Math.floor(Math.random() * 4), idleClock: Math.random() * 2, wait: 1 + Math.random() * 4, facing: 'front', side: 'right' });
  }
  if (count > 0) say(`${count} ${count === 1 ? 'animal apareceu' : 'animais apareceram'} nas Wilds: javalis e alces podem ser caçados.`);
}

const LAIR_SPAWN_EVERY = 720;
// ==========================================================================
// Covis de criaturas têm vida. Heróis derrubam por recompensa ("Destruir o covil") e o covil
// deixa um Dead Remains, de grade D a A. Só herói recolhe. Ao deixar no depósito, o herói cobra
// do cofre 1 de ouro pelas grades D e C e 2 pelas grades B e A. O Laboratório de Ember refina em Ember.
// ==========================================================================
const CREATURE_DENS = ['wolf-den', 'goblin-den', 'troll-den', 'nekomata-den'];
const LAIR_HP = { 1: 300, 2: 700 }; // por nível do covil
const lairMaxHp = lair => LAIR_HP[lair.level || 1] || LAIR_HP[1];
const lairHp = lair => Math.max(0, lairMaxHp(lair) - (lair.damage || 0));
const REMAINS_GRADES = [
  { id: 'D', gold: 1, ember: 1 }, { id: 'C', gold: 1, ember: 2 }, { id: 'B', gold: 2, ember: 4 }, { id: 'A', gold: 2, ember: 7 },
];
// Chance de cada grade (D, C, B, A) pelo nível do covil.
const REMAINS_ODDS = { 1: [0.6, 0.3, 0.1, 0], 2: [0.1, 0.3, 0.4, 0.2] };
const SPECIAL_REMAINS = { nekomata: { id: 'nekomata', gold: 2 }, nekomataWild: { id: 'nekomataWild', gold: 2 } };
const REMAINS_ART = { nekomata: 'nekomata-remains-photo-candidate-v01.png', nekomataWild: 'nekomata-remains-photo-v04.png', D: 'icon-dead-remains-d-photo-v01.png', C: 'icon-dead-remains-c-photo-v01.png', B: 'icon-dead-remains-b-photo-v01.png', A: 'icon-dead-remains-a-photo-v01.png' };
const remainsImage = grade => `public/art/${REMAINS_ART[grade]}`;
const remainsGrade = id => SPECIAL_REMAINS[id] || REMAINS_GRADES.find(grade => grade.id === id) || REMAINS_GRADES[0];
const remainsName = id => id === 'nekomata' ? 'Nekomata Remains · covil' : id === 'nekomataWild' ? 'Nekomata Remains · individual' : `Dead Remains grade ${id}`;
const emptyRemains = () => ({ D: 0, C: 0, B: 0, A: 0 });
const remainsCount = () => REMAINS_GRADES.reduce((sum, grade) => sum + (state.remains?.[grade.id] || 0), 0) + (state.remains?.nekomata || 0) + (state.remains?.nekomataWild || 0);
function rollRemains(level) {
  const odds = REMAINS_ODDS[level] || REMAINS_ODDS[1];
  let roll = Math.random();
  for (let i = 0; i < odds.length; i++) { roll -= odds[i]; if (roll < 0) return REMAINS_GRADES[i].id; }
  return REMAINS_GRADES[0].id;
}
// A ponta de baixo da estrada velha na cidade (nó 18) encosta na ponta de cima da estrada dos Wilds.
// Devolve true enquanto o herói ainda está a caminho do outro mapa.
function travelToMap(hero, mapId, mapSet = 1) {
  const here = hero.mapId || 'city';
  if (here === mapId) return false;
  const gate = PATH_NODES[18];
  if (here === 'city') {
    if (Math.hypot(hero.x - gate.x, hero.y - gate.y) < 1.5) {
      const exit = WILDS_ROAD_EXIT[mapSet === 2 ? 2 : 1];
      hero.mapId = 'wilds'; hero.mapSet = mapSet; hero.x = exit.x; hero.y = exit.y + 1; hero.route = []; hero.finalPoint = null;
      return false;
    }
    routeActor(hero, 18);
    hero.finalPoint = { x: gate.x, y: gate.y };
    return true;
  }
  const exit = WILDS_ROAD_EXIT[mapSet === 2 ? 2 : 1];
  if (distanceTo(hero, exit) < 1.5) {
    hero.mapId = 'city'; hero.mapSet = mapSet; hero.x = gate.x; hero.y = gate.y; hero.route = []; hero.finalPoint = null;
    return false;
  }
  hero.route = [];
  hero.finalPoint = { x: exit.x, y: exit.y };
  return true;
}
function lairBounty(lairId) {
  const lair = (state.lairs || []).find(item => item.id === lairId && item.active && CREATURE_DENS.includes(item.kind));
  if (!lair || (state.bounties || []).some(bounty => bounty.lairId === lairId)) return;
  const gate = PATH_NODES[18];
  createBounty({ x: gate.x, y: gate.y, node: 18, lairId, label: `Destruir: ${lair.name}` });
}
// O herói desce a estrada, chega ao covil e bate até ele cair. As criaturas por perto defendem.
function lairBountyStep(hero, bounty) {
  const lair = (state.lairs || []).find(item => item.id === bounty.lairId && item.active);
  if (!lair) return;
  hero.task = bounty.label;
  if (travelToMap(hero, lair.mapId || 'wilds', lair.mapSet || 1)) return;
  const spot = { x: lair.x, y: Math.min(198, lair.y + 3) };
  if (distanceTo(hero, spot) > 1.5) { hero.route = []; hero.finalPoint = spot; return; }
  faceToward(hero, lair);
  const stats = statsOf(hero);
  const power = MAGIC_CLASSES.includes(classOf(hero)) ? stats.mag : stats.atk;
  lair.damage = (lair.damage || 0) + Math.max(1, Math.round(power * (0.8 + Math.random() * 0.4)));
  const before = hero.level || 1;
  gainActorXp(hero, 5);
  checkPrestige(hero, before);
  hero.wait = ATTACK_COOLDOWN; hero.actionTime = 0.5; hero.actionClock = 0;
  if (lairHp(lair) <= 0) destroyLair(lair, hero, bounty);
  renderLairs();
  if ((state.tab === 'heroes' || state.tab === 'workers') && state.ticks % 3 === 0) renderPanel();
}
function destroyLair(lair, hero, bounty) {
  lair.active = false;
  lair.destroyed = true;
  lair.damage = 0;
  const grade = lair.kind === 'nekomata-den' ? 'nekomata' : rollRemains(lair.level || 1);
  hero.bountyId = null;
  state.bounties = state.bounties.filter(item => item !== bounty);
  lootHero(hero, bounty.reward);
  hero.remains = grade;
  gainHeirXp(25);
  say(`${hero.name} derrubou ${lair.name} e embolsou ${bounty.reward} de ouro. Sobrou ${remainsName(grade)}; ele vai levar pro depósito.`, `${lair.name} caiu. Os bichos vão ter que morar em outro lugar.`);
  renderLairs(); renderSites();
  if (state.tab === 'heroes') renderPanel();
}
// Herói com restos na mão: volta pra cidade, deixa no depósito mais perto e cobra do cofre.
function carryRemainsStep(hero) {
  const grade = remainsGrade(hero.remains);
  hero.task = `Levando ${remainsName(grade.id)} pro depósito`;
  if (travelToMap(hero, 'city')) return;
  const store = nearestStorage(hero);
  if (!isAt(hero, store)) { walkTo(hero, store); return; }
  if (storedUnits() > storageCap() - 1) {
    if (!state.remainsFullWarned) { state.remainsFullWarned = true; say(`O depósito está cheio. ${hero.name} está esperando na porta com o Dead Remains na mão.`); }
    hero.task = 'Esperando espaço no depósito com um Dead Remains';
    hero.wait = 5;
    return;
  }
  state.remainsFullWarned = false;
  state.remains[grade.id] = (state.remains[grade.id] || 0) + 1;
  const pay = Math.min(grade.gold, Math.max(0, Math.floor(state.gold)));
  state.gold -= pay;
  lootHero(hero, pay);
  hero.remains = null;
  say(`${hero.name} deixou ${remainsName(grade.id)} no depósito e cobrou ${pay} de ouro do cofre${pay < grade.gold ? ', o que tinha' : ''}.`);
  updateHud();
  if (['food', 'heroes'].includes(state.tab)) renderPanel();
}
// Restos no chão (de herói que morreu no caminho): um herói livre e inteiro vai buscar.
function remainsPileStep(hero) {
  const piles = state.remainsPiles || [];
  let pile = piles.find(item => String(item.claimedBy) === String(hero.id));
  if (!pile) {
    if (hpOf(hero) < maxHpOf(hero) * 0.5) return false;
    pile = piles.find(item => item.claimedBy == null || !state.heroes.some(other => other.alive && String(other.id) === String(item.claimedBy)));
    if (!pile) return false;
    pile.claimedBy = hero.id;
  }
  hero.task = `Indo buscar ${remainsName(pile.grade)}`;
  if (travelToMap(hero, pile.mapId || 'city', pile.mapSet || 1)) return true;
  if (distanceTo(hero, pile) < 1.5) {
    hero.remains = pile.grade;
    state.remainsPiles = piles.filter(item => item !== pile);
    renderLairs();
    return true;
  }
  hero.route = [];
  if ((pile.mapId || 'city') === 'city') routeActor(hero, nearestNode(pile));
  hero.finalPoint = { x: pile.x, y: pile.y };
  return true;
}
// Laboratório: o camponês busca um Dead Remains por viagem (o de grade mais alta) e o laboratório refina.
// O Ember vai direto pro estoque, como pedra e metal.
function refineRemains(dt) {
  state.buildings.filter(building => building.type === 'emberLab' && isDone(building)).forEach(lab => {
    if (!lab.input?.length) { lab.progress = 0; return; }
    lab.progress = (lab.progress || 0) + labRate(lab) * (1 + crewWork(lab)) * dt;
    while (lab.progress >= 1 && lab.input.length) {
      lab.progress -= 1;
      const grade = remainsGrade(lab.input.shift());
      state.ember = (state.ember || 0) + grade.ember;
    }
  });
}
const bestRemains = () => [...REMAINS_GRADES].reverse().find(grade => (state.remains?.[grade.id] || 0) > 0)?.id || null;
function labRoutine(actor, lab) {
  const store = nearestStorage(lab);
  const queued = () => (lab.input?.length || 0) + state.workers.filter(worker => worker.alive && worker.carrying?.toLab === lab.site).length;
  if (isAt(actor, lab)) {
    if (actor.carrying?.toLab) { lab.input = [...(lab.input || []), actor.carrying.remains]; actor.carrying = null; }
    if (queued() < 2 && bestRemains()) { actor.task = 'Indo buscar Dead Remains no depósito'; walkTo(actor, store); return; }
    actor.task = lab.input?.length ? `Refinando um Dead Remains grade ${lab.input[0]} em Ember` : 'No laboratório, esperando Dead Remains';
    actor.wait = 3; actor.actionTime = lab.input?.length ? 3 : 0; actor.actionClock = 0;
    return;
  }
  if (isAt(actor, store)) {
    if (actor.carrying && !actor.carrying.toLab) deliverLoad(actor);
    const grade = !actor.carrying && queued() < 2 && bestRemains();
    if (grade) { state.remains[grade] -= 1; actor.carrying = { remains: grade, qty: 1, toLab: lab.site }; }
  }
  if (actor.carrying && !actor.carrying.toLab) {
    actor.task = `Carregando ${format(actor.carrying.qty)} de ${foodName(actor.carrying.food)} para o depósito`;
    walkTo(actor, store);
    return;
  }
  actor.task = actor.carrying ? `Levando um Dead Remains grade ${actor.carrying.remains} pro laboratório` : 'Indo trabalhar no Laboratório de Ember';
  actor.headingToWork = true;
  walkTo(actor, lab);
}
// Chamado às armas: cada inimigo na cidade tem pelo menos um herói indo atrás dele.
// Quem estava bebendo larga o copo; o herói mais perto e livre é o escolhido.
function assignResponders() {
  const heroes = state.heroes.filter(hero => hero.alive && (hero.mapId || 'city') === 'city');
  const enemies = (state.bandits || []).filter(enemy => enemy.alive && enemy.className !== 'Wildlife' && (enemy.mapId || 'city') === 'city');
  heroes.forEach(hero => { if (hero.respondTo && !enemies.some(enemy => enemy.id === hero.respondTo)) hero.respondTo = null; });
  enemies.forEach(enemy => {
    if (heroes.some(hero => hero.respondTo === enemy.id || hero.combatTarget === enemy.id)) return;
    const free = heroes.filter(hero => !hero.respondTo && !hero.combatTarget);
    const pick = (free.filter(hero => !hero.bountyId).length ? free.filter(hero => !hero.bountyId) : free)
      .sort((a, b) => distanceTo(a, enemy) - distanceTo(b, enemy))[0];
    if (!pick) return;
    const wasDrinking = pick.drinkUntil > state.minutes || pick.tavernTripSite != null;
    pick.respondTo = enemy.id;
    interruptTavernVisit(pick);
    if (wasDrinking) say(`${pick.name} largou o copo na taverna e saiu atrás de ${enemy.name}.`);
  });
}
// Uma vez por segundo de jogo: bandos novos, limpeza e recuperação fora de combate.
function combatTick(minutesPassed) {
  assignResponders();
  if (state.nextRaidAt == null) state.nextRaidAt = 720;
  if (state.minutes >= state.nextRaidAt) { spawnBandits(); state.nextRaidAt += RAID_EVERY; }
  const night = Math.floor(state.minutes / 1440);
  if (state.wildlifeSpawnDay == null) state.wildlifeSpawnDay = night;
  if (night > state.wildlifeSpawnDay) { state.wildlifeSpawnDay = night; spawnWildlife(); }
  const firstLevelCemetery = (state.lairs || []).find(lair => lair.active && lair.kind === 'cemetery' && lair.level === 1);
  if (state.minutes % 1440 >= 1320 && state.lastSkeletonDay !== night && firstLevelCemetery) { state.lastSkeletonDay = night; spawnSkeleton(firstLevelCemetery); }
  // Covis ativos soltam um bicho a cada 12 horas de jogo (12 minutos reais na velocidade 1).
  // Saves antigos tinham o próximo marcado para dias depois: puxa para no máximo 12 horas.
  (state.lairs || []).filter(lair => lair.active && CREATURE_DENS.includes(lair.kind)).forEach(lair => {
    if (lair.nextSpawnAt == null || lair.nextSpawnAt > state.minutes + LAIR_SPAWN_EVERY) lair.nextSpawnAt = state.minutes + LAIR_SPAWN_EVERY;
    if (state.minutes < lair.nextSpawnAt) return;
    if (lair.kind === 'wolf-den') spawnWolf(lair); else spawnLairCreature(lair);
    lair.nextSpawnAt = state.minutes + LAIR_SPAWN_EVERY;
  });
  if (state.minutes % 1440 >= 1320 && state.lastZombieDay !== night && (state.lairs || []).some(lair => lair.active && lair.kind === 'cemetery' && lair.level === 2)) { state.lastZombieDay = night; spawnZombie(); }
  state.bandits = (state.bandits || []).filter(bandit => bandit.alive);
  [...state.heroes, ...state.workers].filter(actor => actor.alive && actor.hp != null && (actor.calm || 0) <= 0).forEach(actor => {
    actor.hp = Math.min(maxHpOf(actor), actor.hp + maxHpOf(actor) * 0.03 * minutesPassed);
  });
}

// ==========================================================================
// Saves, menus e telas: entrada, continuar, escolher save, opções, debug e pausa.
// ==========================================================================
function readSave(slot) {
  try { const raw = localStorage.getItem(SAVE_KEY(slot)); return raw ? JSON.parse(raw) : null; } catch { return null; }
}

function allSaves() {
  return Array.from({ length: SAVE_SLOTS }, (_, i) => ({ slot: i + 1, record: readSave(i + 1) }));
}

function latestSave() {
  return allSaves().filter(entry => entry.record).sort((a, b) => b.record.savedAt - a.record.savedAt)[0] || null;
}

function serializeState() {
  const copy = { ...state, workers: state.workers.map(worker => ({ ...worker, workplace: worker.workplace ? worker.workplace.site : null })) };
  const data = JSON.parse(JSON.stringify(copy));
  data.selectedBuild = null;
  data.movingSite = null;
  data.placingBounty = false;
  delete data.testMode;
  delete data.stash;
  return data;
}

function hydrateState(data) {
  const defaults = newGameState();
  const loaded = Object.assign(defaults, data);
  const savedAttributes = data.heirAttributes && typeof data.heirAttributes === 'object' ? data.heirAttributes : {};
  const returnedMpPoints = Math.max(0, Math.floor(Number(savedAttributes.MP) || 0));
  loaded.heirAttributes = Object.fromEntries(HEIR_STATS.map(stat => [stat.key, Math.max(0, Math.floor(Number(savedAttributes[stat.key]) || 0))]));
  const spentAttributes = Object.values(loaded.heirAttributes).reduce((sum, value) => sum + value, 0);
  const earnedAttributes = Math.max(0, (Math.max(1, Number(data.heirLevel) || 1) - 1) * 6);
  loaded.heirAttributePoints = Number.isFinite(data.heirAttributePoints)
    ? Math.max(0, Math.floor(data.heirAttributePoints)) + returnedMpPoints
    : Math.max(0, earnedAttributes - spentAttributes);
  loaded.metals = { ...defaults.metals, ...(data.metals || {}) };
  loaded.metals.steel = (loaded.metals.steel || 0) + (loaded.metals.steelScrap || 0);
  delete loaded.metals.steelScrap;
  if (!data.metals && Number(data.metal) > 0) loaded.metals.ironOre = Number(data.metal);
  loaded.fishingStock = { ...Object.fromEntries(FISHING_SPOTS.map(spot => [spot.site, 0])), ...(data.fishingStock || {}) };
  loaded.oreGrades = data.oreGrades && typeof data.oreGrades === 'object' ? data.oreGrades : {};
  loaded.residues = data.residues && typeof data.residues === 'object' ? data.residues : {};
  loaded.extractionTools = Array.isArray(data.extractionTools) ? data.extractionTools : defaults.extractionTools;
  loaded.mineralDeposits = data.mineralDeposits && typeof data.mineralDeposits === 'object' ? data.mineralDeposits : {};
  loaded.stoneDeposits = data.stoneDeposits && typeof data.stoneDeposits === 'object' ? data.stoneDeposits : {};
  loaded.quarryMaterials = { ...defaults.quarryMaterials, ...(data.quarryMaterials || {}) };
  loaded.mapSet = data.mapSet === 2 ? 2 : 1;
  const defaultLairs = defaults.lairs;
  loaded.lairs = defaultLairs.map(base => ({
    ...base, mapSet: 1, ...(Array.isArray(data.lairs) ? data.lairs.find(lair => lair.id === base.id) : null),
    id: base.id, name: base.name, kind: base.kind, level: base.level, art: base.art, mapId: base.mapId, x: base.x, y: base.y,
  }));
  ['goblin-den-l1', 'troll-den-l1'].forEach(id => { const lair = loaded.lairs.find(item => item.id === id); if (lair && !lair.destroyed) lair.active = true; });
  loaded.remains = { ...emptyRemains(), ...(data.remains || {}) };
  if (!Array.isArray(loaded.remainsPiles)) loaded.remainsPiles = [];
  if (!Array.isArray(loaded.remainsPiles)) loaded.remainsPiles = [];
  if (!Array.isArray(loaded.wildMeatPiles)) loaded.wildMeatPiles = [];
  if (!Number.isFinite(loaded.wildlifeSpawnDay)) loaded.wildlifeSpawnDay = Math.floor((loaded.minutes || 0) / 1440);
  loaded.buildings.forEach(building => { building.builtAt = 0; building.upgradedAt = 0; });
  loaded.buildings.forEach(building => { if (!building.mapId) building.mapId = 'city'; });
  loaded.buildings.filter(building => building.type === 'mine').forEach(building => {
    if (!METAL_DEPOSIT_RANKS.some(rank => rank.id === building.depositRank)) building.depositRank = (building.mapSet || loaded.mapSet) === 2 ? 'IV' : 'II';
  });
  loaded.buildings.filter(building => building.type === 'blacksmith').forEach(building => {
    if (!BLACKSMITH_RECIPES.some(recipe => recipe.id === building.produce && recipe.minLevel <= (building.level || 0) + 1)) building.produce = 'iron';
  });
  if (!data.food) loaded.food = { ...emptyFood(), wheat: data.grain || 0 };
  loaded.food = { ...emptyFood(), ...(loaded.food || {}) };
  loaded.diet = { ...defaults.diet, ...(loaded.diet || {}) };
  delete loaded.testMode;
  delete loaded.stash;
  if (loaded.food.flour == null) loaded.food.flour = 0;
  if (loaded.diet && loaded.diet.flour == null) loaded.diet.flour = true;
  ['bread', 'cheese', 'quiche'].forEach(id => {
    if (loaded.food[id] == null) loaded.food[id] = 0;
    if (loaded.diet && loaded.diet[id] == null) loaded.diet[id] = true;
  });
  if (!Array.isArray(loaded.items)) loaded.items = [];
  if (!Array.isArray(loaded.bandits)) loaded.bandits = [];
  (loaded.lairs || []).filter(lair => lair.active && ['wolf-den', 'goblin-den', 'troll-den'].includes(lair.kind)).forEach(lair => {
    if (!Number.isFinite(lair.nextSpawnAt)) lair.nextSpawnAt = loaded.minutes + 2880;
  });
  if (data.nextRaidAt == null) loaded.nextRaidAt = loaded.minutes + RAID_EVERY;
  loaded.bandits.forEach(bandit => {
    bandit.route = []; bandit.finalPoint = null;
    if (isWildlife(bandit)) { bandit.mapId = 'wilds'; bandit.mapSet = WILDLIFE_MAP_SET; }
  });
  loaded.wildMeatPiles.forEach(pile => {
    if ((pile.mapId || 'wilds') === 'wilds') pile.mapSet = WILDLIFE_MAP_SET;
  });
  if (loaded.taxPercent == null) loaded.taxPercent = 10;
  if (loaded.stone == null) loaded.stone = 0;
  if (!Number.isFinite(loaded.marble)) loaded.marble = 0;
  if (loaded.metal == null) loaded.metal = 0;
  if (loaded.favor == null) loaded.favor = 50;
  if (!Array.isArray(loaded.demands)) loaded.demands = [];
  if (!Array.isArray(loaded.bounties)) loaded.bounties = [];
  if (loaded.bountyAmount == null) loaded.bountyAmount = 25;
  loaded.placingBounty = false;
  if (loaded.nextDemandAt == null) loaded.nextDemandAt = loaded.minutes + 600;
  loaded.taxPercent = Math.min(TAX_MAX, loaded.taxPercent);
  if (loaded.tavernTab == null) loaded.tavernTab = 0;
  // Saves de antes dos objetivos em sequência: quem já tinha vencido fica com todos cumpridos.
  if (data.goalStep == null) loaded.goalStep = loaded.won ? SCENARIO_GOALS.length : 0;
  loaded.nextBuildingId = Math.max(loaded.nextBuildingId || 1, ...loaded.buildings.map(building => building.site + 1));
  if (loaded.tab === 'contracts') loaded.tab = 'build';
  loaded.workers.forEach(worker => {
    worker.workplace = worker.workplace == null ? null : loaded.buildings.find(building => building.site === worker.workplace) || null;
  });
  // Saves antigos marcavam todos os heróis recrutados por uma guilda como moradores dela.
  // Mantém um por guilda e transfere os demais para camas disponíveis, sem apagar personagens.
  loaded.buildings.filter(building => isDone(building) && BUILDINGS.some(item => item.id === building.type && item.heroClass)).forEach(guild => {
    const residents = loaded.heroes.filter(hero => hero.alive && hero.guildSite === guild.site);
    residents.slice(1).forEach(hero => {
      hero.recruitedFromSite = guild.site;
      hero.guildSite = null;
      const lodging = loaded.buildings.find(building => isDone(building) && (
        (building.type === 'hotel' && loaded.heroes.filter(other => other.alive && other.homeSite === building.site).length < 4)
        || (building.type === 'tavern' && !loaded.heroes.some(other => other.alive && other.homeSite === building.site))));
      hero.homeSite = lodging?.site ?? null;
    });
  });
  loaded.heroes.filter(hero => hero.alive && hero.guildSite != null
    && !loaded.buildings.some(building => building.site === hero.guildSite && BUILDINGS.some(item => item.id === building.type && item.heroClass)))
    .forEach(hero => {
      hero.recruitedFromSite = hero.recruitedFromSite ?? hero.guildSite;
      hero.guildSite = null;
      if (hero.homeSite == null) {
        const lodging = loaded.buildings.find(building => isDone(building) && (
          (building.type === 'hotel' && loaded.heroes.filter(other => other.alive && other.homeSite === building.site).length < 4)
          || (building.type === 'tavern' && !loaded.heroes.some(other => other.alive && other.homeSite === building.site))));
        hero.homeSite = lodging?.site ?? null;
      }
    });
  [...loaded.heroes, ...loaded.workers].forEach(actor => { actor.route = []; actor.goal = null; actor.wait = 0; actor.finalPoint = null; });
  loaded.selectedBuild = null;
  loaded.movingSite = null;
  loaded.selectedSite = null;
  loaded.selectedActor = null;
  return loaded;
}

function writeSave(slot) {
  if (!slot || state.gameOver) return false;
  const ok = writeStore(SAVE_KEY(slot), { version: 1, savedAt: Date.now(), state: serializeState() });
  if (ok) game.lastSaved = Date.now();
  return ok;
}

function deleteSave(slot) {
  try { localStorage.removeItem(SAVE_KEY(slot)); } catch {}
}

const pad2 = n => String(n).padStart(2, '0');
function formatPlayTime(seconds) {
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  return h ? `${h} h ${pad2(m)} min` : `${m} min`;
}

function describeSave(record) {
  const s = record.state;
  return {
    day: Math.floor(s.minutes / 1440) + 1,
    clock: `${pad2(Math.floor(s.minutes / 60) % 24)}:${pad2(s.minutes % 60)}`,
    gold: format(s.gold),
    buildings: s.buildings.length,
    heroes: s.heroes.filter(hero => hero.alive).length,
    played: formatPlayTime(s.playSeconds || 0),
    savedAt: new Date(record.savedAt).toLocaleString('pt-BR', { dateStyle: 'short', timeStyle: 'short' }),
  };
}

function saveCard(slot, record) {
  if (!record) return `<div class="save-head"><b>Slot ${slot}</b><span>Vazio</span></div><p class="save-empty">Nenhuma herança registrada. Ainda.</p>`;
  const d = describeSave(record);
  return `<div class="save-head"><b>Slot ${slot}</b><span>${d.savedAt}</span></div>
    <div class="save-title">${heirById(record.state.heir) ? `${heirById(record.state.heir).name} · ` : ''}Vale do Luto · Cenário 01</div>
    <dl class="save-stats"><div><dt>Dia</dt><dd>${d.day} · ${d.clock}</dd></div><div><dt>Ouro</dt><dd>${d.gold}</dd></div><div><dt>Construções</dt><dd>${d.buildings}</dd></div><div><dt>Heróis</dt><dd>${d.heroes}</dd></div><div><dt>Tempo de jogo</dt><dd>${d.played}</dd></div></dl>`;
}

// Aplica o jogo escolhido na tela e começa a rodar.
function startGame(newState, slot) {
  selectedLairId = null;
  state = newState;
  resetArrivalClock();
  state.speed = [1, 2, 3].includes(Number(state.speed)) ? Number(state.speed) : 1;
  ensureFarmWorkers();
  game.slot = slot;
  currentMapSet = state.mapSet === 2 ? 2 : 1;
  worldScenario = loadScenario(state.scenario || 1);
  migrateLegacyExtractionSites();
  state.buildings.filter(building => building.type === 'mine' && building.resourcePlaceId).forEach(building => {
    const deposit = depositForPit(building);
    if (!building.selectedSeamId) building.selectedSeamId = deposit?.seams.find(seam => seam.mineral === 'iron' && seam.access === 'shallow')?.id || deposit?.seams[0]?.id || null;
    if (!Number.isFinite(building.stockGrams)) building.stockGrams = 0;
    if (!Number.isFinite(building.level)) building.level = 0;
  });
  state.buildings.filter(building => building.type === 'quarry' && building.resourcePlaceId).forEach(building => {
    const deposit = stoneDepositForPit(building);
    if (!building.quarryFaceId) building.quarryFaceId = deposit?.faces[0]?.id || null;
    if (!QUARRY_ORDERS.some(order => order.id === building.quarryOrder)) building.quarryOrder = 'rubble';
    quarryInventory(building);
    if (!Number.isFinite(building.stockStone)) building.stockStone = 0;
    if (!Number.isFinite(building.stockMarble)) building.stockMarble = 0;
    if (!Number.isFinite(building.level)) building.level = 0;
    if (building.loadingPad && !building.loadingPad.pieceId && building.loadingPad.id) {
      building.loadingPad = { pieceId: building.loadingPad.id, cargoId: building.loadingPad.cargoId || building.loadingPad.id, state: 'separated-on-pad', location: 'quarry-loading-pad' };
    }
    if (building.level >= 1) {
      building.derrick ||= { capacityKg: 160, shared: true };
      building.sharedCart ||= { capacityKg: 160, route: 'quarry-city', cargoId: null, cargo: null, tripRemaining: 0 };
      building.sharedCart.capacityKg ||= 160;
      building.sharedCart.cargoId ||= building.sharedCart.cargo?.id || null;
    }
  });
  switchMap('city');
  $('#hero-layer').innerHTML = '';
  $('#map-scene').style.setProperty('--mill-spin-duration', `${8 / state.speed}s`);
  $$('.tab').forEach(tab => { const active = tab.dataset.tab === state.tab; tab.classList.toggle('active', active); tab.setAttribute('aria-selected', String(active)); });
  camera.zoom = Math.min(2, Math.max(0, options.startZoom));
  $$('.zoom-btn').forEach(button => { const active = Number(button.dataset.zoom) === camera.zoom; button.classList.toggle('active', active); button.setAttribute('aria-pressed', String(active)); });
  closeMenu();
  sizeWorld();
  homeCamera();
  window.Monarch?.reset();
  renderSites(); renderHeroes(); renderPanel(); updateHud();
  renderPlaces();
  $('#event-line').textContent = state.ticks ? 'De volta ao Vale do Luto.' : 'A herança foi aceita. Os credores já estão a caminho.';
  writeSave(slot);
  game.running = true;
  setMusic('game');
}

function newGame(slot, heir) {
  const fresh = newGameState(heir);
  grantStartingStock(fresh);
  startGame(fresh, slot);
  say('A herança é sua. Os credores já foram avisados.');
}

function loadGame(slot) {
  const record = readSave(slot);
  if (!record) return;
  startGame(hydrateState(record.state), slot);
  say(`Bem-vindo de volta. Dia ${Math.floor(state.minutes / 1440) + 1}. A fazenda continua horrível.`);
}

function setStartingStock(on) {
  debug.startingStock = on ? 'full' : 'standard';
  writeStore(DEBUG_KEY, debug);
  if (game.slot) { grantStartingStock(state); updateHud(); }
}

function applyOptions() {
  document.body.classList.toggle('names-always', options.names === 'always');
  document.documentElement.style.setProperty('--mill-sails-image', `url('${buildingImage('mill-sails')}')`);
  syncMusic();
}

// Música: tema principal nas telas de entrada; durante o jogo, as faixas do Ciclo 01 tocam uma depois da outra.
// O navegador só deixa tocar depois do primeiro clique ou tecla; até lá a música espera.
const MUSIC = {
  title: ['Music/MainTheme/LANDR-Dragonomicon i - usesuno.com-Balanced-Medium.mp3'],
  game: [
    'Music/Cycle01/LANDR-Connard The Barbarian 18 - usesuno.com-Balanced-Medium.mp3',
    'Music/Cycle01/LANDR-Connard The Barbarian 18 - usesuno.com (1)-Balanced-Medium.mp3',
  ],
};
const MUSIC_VOLUME = 0.45;
const MUSIC_FADE_MS = 900;
const music = { audio: new Audio(), mode: null, index: 0, fade: 0 };
music.audio.preload = 'auto';
music.audio.addEventListener('ended', () => {
  const list = MUSIC[music.mode];
  if (!list) return;
  music.index = (music.index + 1) % list.length;
  music.audio.src = encodeURI(list[music.index]);
  syncMusic();
});

function fadeMusic(target, done) {
  cancelAnimationFrame(music.fade);
  const audio = music.audio;
  const from = audio.volume;
  const start = performance.now();
  const step = now => {
    const t = Math.min(1, (now - start) / MUSIC_FADE_MS);
    audio.volume = from + (target - from) * t;
    if (t < 1) music.fade = requestAnimationFrame(step); else done?.();
  };
  music.fade = requestAnimationFrame(step);
}

// Troca a trilha: 'title' nas telas de entrada, 'game' com uma partida aberta.
function setMusic(mode) {
  if (music.mode === mode) return;
  music.mode = mode;
  music.index = 0;
  cancelAnimationFrame(music.fade);
  music.audio.src = encodeURI(MUSIC[mode][0]);
  music.audio.volume = 0;
  syncMusic();
}

// Toca ou para conforme a opção de som e atualiza o botão de música.
function syncMusic() {
  const on = options.sound === 1;
  $$('[data-music-toggle]').forEach(button => {
    button.classList.toggle('off', !on);
    button.setAttribute('aria-pressed', String(on));
    button.title = on ? 'Música: ligada' : 'Música: desligada';
    button.setAttribute('aria-label', on ? 'Desligar música' : 'Ligar música');
  });
  if (!music.mode || !music.audio.src) return;
  if (!on) { fadeMusic(0, () => music.audio.pause()); return; }
  const playing = music.audio.play();
  playing?.then(() => fadeMusic(MUSIC_VOLUME)).catch(() => {});
}

function toggleMusic() {
  options.sound = options.sound === 1 ? 0 : 1;
  writeStore(OPTIONS_KEY, options);
  syncMusic();
  if (menu.stack[menu.stack.length - 1]?.name === 'options') renderMenu();
}

function initMusic() {
  $$('[data-music-toggle]').forEach(button => button.addEventListener('click', event => { event.stopPropagation(); toggleMusic(); }));
  // Primeiro clique ou tecla da página libera o áudio no navegador.
  const unlock = () => { if (options.sound === 1 && music.audio.paused) syncMusic(); };
  ['pointerdown', 'keydown'].forEach(type => document.addEventListener(type, unlock, true));
}

const menu = { stack: [], pending: null };


function segmented(name, value, choices) {
  return `<div class="segmented" role="group">${choices.map(([v, label]) => `<button type="button" data-set="${name}" data-value="${v}" class="${String(v) === String(value) ? 'active' : ''}" aria-pressed="${String(v) === String(value)}">${label}</button>`).join('')}</div>`;
}

const menuMode = () => (game.slot ? 'is-pause' : 'is-title');

const titleLine = 'Dizem que a primeira noite é mais difícil. Dizem muitas coisas.';

function ago(ts) {
  const m = Math.round((Date.now() - ts) / 60000);
  if (m < 1) return 'agora há pouco';
  if (m < 60) return `há ${m} min`;
  const h = Math.round(m / 60);
  return h < 24 ? `há ${h} h` : `há ${Math.round(h / 24)} dias`;
}

function slotSummary(record) {
  const d = describeSave(record);
  return `Dia ${d.day}, ${d.clock} · ${d.gold} de ouro · ${d.heroes} ${d.heroes === 1 ? 'herói' : 'heróis'} · salvo ${ago(record.savedAt)}`;
}

function economySnapshot(source) {
  const workers = (source.workers || []).filter(worker => worker.alive);
  const buildings = (source.buildings || []).filter(isDone);
  const produced = Object.fromEntries(FOOD_TYPES.map(food => [food.id, 0]));
  buildings.forEach(building => {
    if (building.type === 'farm') { produced[farmFoodId(building)] += farmOutput(building); const secondary = farmType(building.produce).secondary; if (secondary) produced[secondary] += farmSecondaryRate(building); }
    if (building.type === 'mill') produced.flour += millRate(building);
    if (building.type === 'bakery') produced[bakeryActiveRecipe(building).id] += bakeRate(building);
    if (building.type === 'butcher') produced[butcherActiveRecipe(building).id] += butcherRate(building);
  });
  const ration = RATIONS[source.ration ?? 2] || RATIONS[2];
  const consumption = workers.length * WORKER_UPKEEP * ration.mult;
  const woodRate = buildings.filter(building => building.type === 'lumber').reduce((sum, camp) => {
    const crew = workers.filter(worker => worker.buildSite == null && (worker.workplace?.site ?? worker.workplace) === camp.site && actorAtPost(worker, camp)).length;
    return sum + LUMBER_RATE[Math.min(3, crew)];
  }, 0);
  const factor = (source.taxPercent ?? 10) / 10;
  const workerTax = workers.reduce((sum, worker) => sum + Math.min(purseOf(worker), 2 * factor), 0);
  const wages = workers.length * WORKER_WAGE;
  const tavernTax = (source.tavernTab || 0) * factor;
  const food = source.food || {};
  const stock = FOOD_TYPES.reduce((sum, item) => sum + (food[item.id] || 0), 0);
  const capacity = MANOR_STORAGE + buildings.filter(building => building.type === 'depot').reduce((sum, depot) => sum + STORAGE_LEVELS[depot.level || 0].cap, 0);
  // Em provisões (pão). A padaria não soma: o que ela assa alimenta o mesmo que os ingredientes que gasta.
  const edible = FOOD_TYPES.filter(item => !item.raw && !item.baked).reduce((sum, item) => sum + produced[item.id] * item.feeds, 0);
  return { source, workers: workers.length, buildings: buildings.length, produced, edible, consumption, woodRate, workerTax, wages, tavernTax, taxEstimate: workerTax + tavernTax, stock, capacity };
}

const economyRate = value => `${value.toFixed(2).replace('.', ',')}`;

// ==========================================================================
// Como jogar: manual com as regras e as fórmulas do jogo. Os números saem das mesmas
// constantes que o jogo usa, então o manual acompanha qualquer ajuste de balanço.
// ==========================================================================
const hn = value => Number(value).toLocaleString('pt-BR', { maximumFractionDigits: 3 });
const howTable = (head, rows) => `<div class="howto-table-wrap"><table class="howto-table"><thead><tr>${head.map(cell => `<th>${cell}</th>`).join('')}</tr></thead><tbody>${rows.map(row => `<tr>${row.map((cell, i) => i ? `<td>${cell}</td>` : `<th>${cell}</th>`).join('')}</tr>`).join('')}</tbody></table></div>`;
const howFormula = text => `<p class="howto-formula">${text}</p>`;
const HOWTO = [
  { title: 'Começo', html: () => `
    <h3>O objetivo</h3>
    <p>O painel de objetivo mostra o que fazer agora e o progresso atual.</p>
    <h3>O que você recebe</h3>
    <p>240 de ouro, 200 de madeira, 200 de pedra, 40 de farinha e 18 de alho. O casarão vem com um casal de camponeses e um Warrior Valente. O casarão guarda ${MANOR_STORAGE} unidades de comida e itens.</p>
    <h3>Você não manda em ninguém</h3>
    <p>Camponeses trabalham sozinhos onde houver trabalho. Heróis decidem tudo: quando lutar, beber, comprar e aceitar recompensas. Você só constrói, ajusta imposto e ração, oferece recompensas e lança magias.</p>
    <h3>Construir</h3>
    <ul>
      <li>Escolha a carta na aba <b>Construir</b>: a prévia fica dourada onde dá e vermelha onde não dá. Toque no mapa para erguer. <b>Esc</b>, botão direito ou tocar de novo na carta cancela.</li>
      <li>Não dá para construir na borda do mapa nem encostado em outra construção (distância mínima de ${hn(BUILD_SPACING)}).</li>
      <li>Ouro e madeira saem na hora. Depois a construção vira <b>obra</b>: um camponês livre vai até lá e trabalha. Sem camponês livre, a obra espera.</li>
      <li>Toque numa construção pronta para ver o que ela faz, melhorar, <b>mudar de lugar</b> (de graça) ou <b>demolir</b> (sem reembolso).</li>
    </ul>` },
  { title: 'Metais', html: () => `
    <h3>Veios e postos</h3>
    <p>Cada marcador de metal mantém seus próprios veios e reservas. Selecione um veio no posto; no nível 1, trabalhe veios rasos. O nível 2 libera quatro trabalhadores, cargas de 40 kg e veios profundos. Mais de um posto pode retirar rocha do mesmo depósito compartilhado.</p>
    ${howTable(['Perfil', 'Hematita', 'Malaquita', 'Galena argentífera', 'Quartzo aurífero'], ['II', 'III', 'IV', 'V'].map((id, i) => { const rank = SEAM_PROFILE[id]; return [`${['Comum', 'Bom', 'Rico', 'Excepcional'][i]}`, `${rank.iron[1]} kg`, `${rank.copper[1]} kg · ${rank.copper[0] * 100}%`, `${rank.silver[1]} kg · ${rank.silver[0] * 100}%`, `${rank.gold[1]} kg · ${rank.gold[0] * 100}%`]; }))}
    <p>O minério chega à City como rocha bruta. Carvão vegetal: 10 kg de madeira → 2 kg de carvão em 60 minutos de trabalho.</p>
    <h3>Blacksmith · receitas de referência</h3>
    ${howTable(['Operação', 'Entrada', 'Saída de metal', 'Carvão', 'Trabalho'], BLACKSMITH_RECIPES.map(recipe => [recipe.name, `${Object.entries(recipe.needs).map(([id, qty]) => `${formatMass(qty)} ${METAL_NAMES[id] || id}`).join(' + ')}`, `${Object.entries(recipe.makes).map(([id, qty]) => `${formatMetalAmount(id, qty)}`).join(' + ')}`, `${formatMass(recipe.fuel)} kg`, `${recipe.work} min`]))}
    <p>Cassiterita já tem um veio garantido no primeiro posto das Wilds. O cobre nativo e os fragmentos de meteorito passam por preparação; o cobre arsenical vem de um veio regional próprio. Cada carga mantém a origem e o teor, e ferramentas só alteram ritmo e desgaste.</p>
    <h3>Armas e manutenção</h3>
    <p>No inspetor do Blacksmith na City, escolha o padrão e depois o metal ou construção. A prévia mostra custo, estoque que falta, bônus do padrão, durabilidade estrutural e desgaste do gume. A ordem reserva metal e madeira, entra na fila após a receita em andamento e entrega a peça ao inventário do casarão. Abra a ficha de um herói para equipar uma arma forjada compatível. O Blacksmith repara separadamente estrutura e gume: cada reparo usa 0,25 kg do material daquela parte; cancelar devolve os insumos reservados.</p>
    <p>Bronze: 4 kg de cobre + 1 kg de estanho → 5 kg. Aço de cadinho: 4 kg de ferro trabalhado + 1 kg de carvão → 3 kg. Machados de corpo de ferro com gume de aço guardam os dois materiais e permitem reparar cada parte com o estoque correspondente.</p>` },
  { title: 'Tempo', html: () => `
    <h3>Relógio</h3>
    <p>Na velocidade <b>1×</b>, cada segundo real é um minuto de jogo. Em <b>2×</b> e <b>3×</b> tudo anda duas ou três vezes mais rápido: produção, obras, fome, viagens. Todo valor "por segundo" deste manual é na velocidade 1×.</p>
    ${howTable(['Unidade', 'Duração'], [['1 hora de jogo', '1 minuto real (a 1×)'], ['1 dia', '24 minutos reais (a 1×)'], ['1 semana', `${DAYS_PER_WEEK} dias`], ['1 mês', `${WEEKS_PER_MONTH} semanas (${DAYS_PER_MONTH} dias)`], ['1 ano', `${MONTHS_PER_YEAR} meses (${DAYS_PER_YEAR} dias)`]])}
    <p>Períodos: madrugada (0h–6h), manhã (6h–12h), tarde (12h–18h) e noite (18h–24h). O jogo começa às 6h20 do dia 1.</p>
    <h3>O que acontece em cada hora certa</h3>
    <ul>
      <li><b>Toda hora:</b> heróis e camponeses livres podem passar no Arsenal; tributos e recompensas são conferidos.</li>
      <li><b>Meia-noite:</b> impostos, depois salários. O herdeiro ganha 40 de experiência.</li>
      <li><b>Meia-noite do 7º dia:</b> a felicidade muda.</li>
      <li><b>Virada do ano:</b> o herdeiro faz aniversário. A cada 25 anos ele muda de fase e o retrato mostra a corrupção de Ember.</li>
    </ul>
    <h3>Controles</h3>
    <ul>
      <li>Arraste o mapa com mouse ou toque; a roda do mouse e as setas do teclado também movem a câmera.</li>
      <li>Zoom 1, 2 e 3: ampla (100%), normal (130%) e próxima (160%). O botão <b>WILDS</b> leva ao segundo mapa e volta.</li>
      <li><b>Esc</b> ou <b>MENU</b> pausa. <b>FICHA</b> abre a ficha do herdeiro. Mensagens ficam na tela até você clicar.</li>
      <li>Salvar sozinho (Opções) grava a cada 1, 3 ou 5 minutos e quando a página fecha.</li>
    </ul>` },
  { title: 'Construções', html: () => `
    <h3>Custo e vida</h3>
    ${howTable(['Construção', 'Ouro', 'Madeira', 'Vida'], BUILDINGS.map(building => [building.name, building.cost, building.wood, BASE_HP[building.id] ?? GUILD_HP]))}
    ${howFormula(`Vida máxima = vida base × (1 + 0,5 × nível da melhoria)`)}
    <h3>Obra</h3>
    <p>Toda construção e toda melhoria precisa de ${BUILD_TIME} de trabalho de um camponês no local (2 minutos reais a 1×, sem bônus). Pedreiros são chamados primeiro, depois quem não tem profissão.</p>
    ${howFormula(`Avanço por segundo = 1 + Construção/100 + bônus de obra do equipamento`)}
    <h3>Melhorias</h3>
    ${howTable(['Melhoria', 'Ouro', 'Madeira', 'Efeito'], [
      ['Fazenda, moinho, padaria ou laboratório, nível 2', PRODUCER_LEVELS[1].gold, PRODUCER_LEVELS[1].wood, 'produz mais'],
      ['Fazenda, moinho, padaria ou laboratório, nível 3', PRODUCER_LEVELS[2].gold, PRODUCER_LEVELS[2].wood, 'produz mais'],
      ['Casebre Grande', HUT_UPGRADE.gold, HUT_UPGRADE.wood, '+3 moradores (5 no total)'],
      ...STORAGE_LEVELS.slice(1).map(level => [`Depósito para ${level.cap}`, level.gold, level.wood, `guarda ${level.cap}`]),
      ['Guilda, nível 2', GUILD_UPGRADES[0].gold, GUILD_UPGRADES[0].wood, 'recruta mais um herói'],
      ['Guilda, nível 3', GUILD_UPGRADES[1].gold, GUILD_UPGRADES[1].wood, 'especialização e mais um herói'],
      ['MoonWarden, nível 2', GUILD_UPGRADES[0].gold, GUILD_UPGRADES[0].wood, 'morador vira lobisomem de machado e escudo'],
      ['MoonWarden, nível 3', GUILD_UPGRADES[1].gold, GUILD_UPGRADES[1].wood, 'morador vira Grande Lobo'],
      ['Vanguards, nível 2', GUILD_UPGRADES[0].gold, GUILD_UPGRADES[0].wood, 'escolha Swordsmaster ou WarBringer para o morador'],
      ...ARMORY_LEVELS.slice(1).map((level, i) => [`Arsenal, nível ${i + 2}`, level.gold, level.wood, `vende até tier ${level.tierCap}`]),
    ])}
    <h3>Dano e conserto</h3>
    <p>Construção com a vida zerada vira <b>ruína</b> e para de funcionar. Os camponeses consertam sozinhos, pedreiros primeiro.</p>
    ${howFormula(`Conserto por segundo = ${REPAIR_RATE} × (1 + Construção/100 + bônus de obra)`)}` },
  { title: 'Comida', html: () => `
    <h3>Fazendas</h3>
    <p>Cada fazenda produz um alimento. Dá para trocar a produção a qualquer hora. A colheita fica na fazenda (até ${FARM_STOCK_CAP}) até um camponês levar ao casarão ou ao Depósito mais perto.</p>
    ${howTable(['Fazenda', 'Nível 1', 'Nível 2', 'Nível 3'], FARM_TYPES.map(type => [type.name, ...type.grain.map(value => `${hn(value * FARM_OUTPUT_MULTIPLIER)}/s`)]))}
    ${howFormula(`Produção = valor da tabela × (1 + bônus da equipe)<br>Bônus da equipe = soma, de quem trabalha ali, de (bônus de trabalho do equipamento + habilidade do ofício/200)`)}
    <h3>Moinho</h3>
    <p>Trigo cru não alimenta ninguém. O moinho mói trigo em farinha, 1 para 1: o camponês busca trigo no depósito e leva a farinha de volta. Moagem por nível: ${MILL_RATE.map(value => `${hn(value)}/s`).join(', ')}, com o mesmo bônus da equipe.</p>
    <h3>Padaria</h3>
    <p>Você escolhe a fornada no painel da padaria. O camponês busca os ingredientes no depósito, um por viagem, e leva o que sai do forno de volta. Cada fornada gasta 1 de cada ingrediente e faz 1 do produto. A Quiche Lorraine só abre no nível 2. Fornadas por nível: ${BAKE_RATE.map(value => `${hn(value)}/s`).join(', ')}. A habilidade <b>Culinária</b> melhora a produção.</p>
    ${howTable(['Fornada', 'Ingredientes', 'Alimenta', 'Disponível'], BAKERY_RECIPES.map(recipe => [recipe.name, recipe.needs.map(id => FOOD_TYPES.find(food => food.id === id).name).join(' + '), `${foodFeeds(recipe.id)}×`, recipe.id === 'quiche' ? 'Padaria nível 2' : 'Padaria nível 1']))}
    <h3>Açougue</h3>
    <p>O Galinheiro continua produzindo ovos e também frango; a fazenda de leite continua produzindo leite e também carne de vaca. Cada carne animal sai a ${hn(FARM_ANIMAL_RATE)}× a taxa normal da fazenda. A fazenda de Suínos entrega matéria-prima ao açougue. O camponês busca insumos e entrega a produção no depósito. Nível 2: ${hn(BUTCHER_RATE[1])} unidades por segundo, com valores padrão editáveis.</p>
    ${howTable(['Produto', 'Ingredientes', 'Alimenta'], BUTCHER_RECIPES.map(recipe => [recipe.name, recipe.needs.map(id => FOOD_TYPES.find(food => food.id === id).name).join(' + '), `${recipe.feeds}×`]))}
    <h3>Pesca</h3>
    <p>Nas Wilds, clique em um ponto de pesca marcado e escolha um camponês. Ele vai ao ponto, acumula Trutas, leva a carga ao depósito e melhora com a habilidade Pesca. Taxa base: ${hn(FISHING_RATE)} por segundo; cada ponto guarda até ${FARM_STOCK_CAP}.</p>
    <h3>Caça nas Wilds</h3>
    <p>Javalis e alces aparecem em quantidade variável a cada dia (às vezes nenhum). Camponeses com a profissão Caçador e arqueiros podem abatê-los; a carne crua cai no local e os camponeses recolhem e levam ao depósito. Os animais alternam caminhada e um ciclo próprio de espera quando param entre destinos.</p>
    <h3>Ração e consumo</h3>
    ${howFormula(`Consumo por segundo = camponeses × ${hn(WORKER_UPKEEP)} × multiplicador da ração<br>Estoque gasto de um alimento = parte dele no consumo ÷ quanto ele alimenta`)}
    ${howTable(['Ração', 'Multiplicador', 'Felicidade'], RATIONS.map(ration => [ration.label, `${hn(ration.mult)}×`, ration.mood > 0 ? `+${ration.mood}` : ration.mood]))}
    <p>A mesa divide a ração em partes iguais entre os alimentos marcados na aba <b>Provisões</b>. Cada alimento servido soma felicidade:</p>
    ${howTable(['Alimento', 'Alimenta', 'Felicidade'], FOOD_TYPES.filter(food => !food.raw).map(food => [food.name, `${food.feeds}×`, `+${food.mood}`]))}
    <h3>Depósito</h3>
    <p>O casarão guarda ${MANOR_STORAGE}. Cada Depósito de Provisões soma ${STORAGE_LEVELS.map(level => level.cap).join(', ')} conforme o nível. Comida e itens contam; ouro e madeira não. Com tudo cheio, o excesso se perde.</p>` },
  { title: 'Felicidade', html: () => `
    <p>A felicidade dos camponeses vai de 0 a 100 e começa em 50. A cada segundo o jogo calcula um <b>alvo</b>:</p>
    ${howFormula(`Alvo = 50 + ração + alimentos servidos + imposto + fome<br>Imposto = 10 − porcentagem do imposto<br>Fome = −20 × (parte da ração que faltou)`)}
    ${howTable(['Imposto', '0%', '5%', '10%', '15%', '20%'], [['Felicidade', '+10', '+5', '0', '−5', '−10']])}
    <p>A felicidade não muda na hora. Na meia-noite do 7º dia ela anda até <b>${MOOD_WEEKLY_STEP} pontos</b> na direção da média dos alvos da semana.</p>
    ${howTable(['Faixa', 'Humor'], [['0 a 19', 'Revoltados'], ['20 a 39', 'Infelizes'], ['40 a 59', 'Conformados'], ['60 a 79', 'Contentes'], ['80 a 100', 'Felizes']])}` },
  { title: 'Ouro', html: () => `
    <p>Construções não geram ouro. O cofre recebe impostos, a conta da taverna, as vendas do Arsenal e do Mercado, e o que se recupera dos ladrões.</p>
    <h3>Impostos e salários (meia-noite)</h3>
    ${howFormula(`Imposto de cada camponês = menor valor entre a bolsa dele e 2 × (imposto/10%)<br>Taverna = conta acumulada × (imposto/10%)`)}
    <p>O imposto vai de 0% a ${TAX_MAX}%, de 5 em 5. Depois do imposto, cada trabalhador recebe <b>${WORKER_WAGE} de ouro</b> de salário, tirado do cofre. Se o cofre não der, alguém fica sem.</p>
    <h3>Taverna</h3>
    <p>Camponeses e heróis fazem pausas curtas e espaçadas para beber e pagam da própria bolsa: <b>1</b> por hora de camponês, <b>2</b> por herói e <b>3</b> por herói de prestígio. Beber aumenta <b>Alcoolismo</b>, uma habilidade penalidade que alonga e aproxima as pausas. Trabalhar sem beber reduz essa habilidade. A pausa pode ser interrompida ao atribuir a pessoa a uma obra ou posto.</p>
    <h3>Bolsas</h3>
    <p>Cada personagem tem a própria bolsa. Camponeses chegam com ${WORKER_START_PURSE} e heróis com ${HERO_START_PURSE}. Heróis enchem a bolsa com loot e recompensas.</p>
    <h3>Mercado</h3>
    ${howTable(['Produto', 'Vender', 'Comprar'], MARKET_GOODS.map(good => [good.name, hn(good.sell), hn(good.buy)]))}
    <p>Sem a construção do Mercado, só passa a carroça de trigo, alho e madeira: vende por 75% e compra por 125% do preço. A comida comprada precisa caber no depósito.</p>` },
  { title: 'Madeira', html: () => `
    <p>O casarão rende 0,14 de madeira por segundo sozinho. A Cabana do Lenhador aceita de 1 a 3 camponeses escalados no painel.</p>
    ${howTable(['Lenhadores', '1', '2', '3'], [['Madeira por segundo', ...LUMBER_RATE.slice(1).map(hn)]])}
    <p>Pedreira e Campo de Mineração aceitam dois camponeses no nível 1 e quatro no nível 2. A equipe corta reservas finitas e leva as cargas à City.</p>
    ${howFormula(`Produção = valor base × (1 + bônus da equipe)`)}
    <p>Quem está escalado numa cabana, pedreira ou mina trabalha lá em vez da fazenda.</p>` },
  { title: 'Camponeses', html: () => `
    <h3>De onde vêm</h3>
    <p>Cada Casebre Pequeno traz um casal. Ampliar para Casebre Grande traz mais três (5 moradores). Sem moradia, ninguém chega.</p>
    <h3>Profissões</h3>
    ${howTable(['Profissão', 'Trabalha em', 'Habilidade'], Object.values(PROFESSIONS).map(prof => [prof.name, prof.types.length ? prof.types.map(type => BUILDINGS.find(item => item.id === type)?.name).join(', ') : prof.skill === 'fishing' ? 'Pontos de pesca no riacho das Wilds' : prof.skill === 'hunting' ? 'Caça e coleta de carne nas Wilds' : 'Obras e consertos', SKILLS.find(skill => skill.id === prof.skill).name]))}
    <p>Sem profissão, o camponês faz o que houver e só compra equipamento básico.</p>
    <h3>Habilidades</h3>
    <p>Lavoura, Moagem, Lenha, Mineração, Construção, Culinária, Pesca, Caça e Carga vão de 0 a 100 e sobem com o uso, mais devagar quanto mais altas.</p>
    ${howFormula(`Ganho = quantidade × (1 − habilidade atual/110)<br>Trabalho na fazenda ou moinho: 0,3 por ciclo · Obra: 0,02 por segundo`)}
    <h3>Progressão profissional</h3>
    <p>Todo camponês também tem as sete profissões de ofício no nível 1: ${TRADES.map(trade => trade.name).join(', ')}. Cada trabalho rende XP para a profissão ligada a ele, até o nível ${TRADE_MAX}. A cada 10 níveis vem um passivo; no ${TRADE_MAX}, a especialização, escolhida uma vez na ficha.</p>
    ${howFormula(`XP da ação = XP base × max(0,25; 1 − 0,03 × (nível − nível da ação)) × (1 + bônus de XP)<br>Bônus no trabalho: metade na produção do lugar, metade no ritmo de cada tarefa`)}
    <p>Carga vai de 0 a 100 e sobe a cada entrega, mais devagar quanto mais alta.</p>
    <h3>Carga</h3>
    ${howFormula(`Carga por viagem = ${CARRY_LOAD} + bônus de carga do equipamento + (Carga/25, arredondado para baixo)<br>Comida (trigo, farinha, pão, carne, peixe) = ${FOOD_LOAD} + (Carga/5, arredondado para baixo) + bônus de carga, até ${FOOD_LOAD_MAX}`)}` },
  { title: 'Heróis', html: () => `
    <h3>Alojamento</h3>
    ${howFormula(`Vagas = 1 (Warrior do casarão) + 1 por guilda + 4 por hotel + 1 por taverna`)}
    <p>Cada guilda abriga só o seu morador. Quem chega de fora e os recrutas extras das guildas precisam de cama na taverna ou no hotel; sem cama, vão embora.</p>
    <p>MoonWarden e Vanguards melhoram o próprio morador e usam a mesma vaga da guilda.</p>
    <h3>Quem chega</h3>
    <ul>
      <li>Guilda pronta: chega um herói da classe.</li>
      <li>Taverna: um visitante a cada 3 horas de jogo. Hotel: a cada 2h30.</li>
      <li>Com hotel e guildas, os visitantes são das classes das guildas; sem isso, de qualquer classe.</li>
    </ul>
    <h3>Prestígio</h3>
    <p>No nível 15 o herói procura a guilda especializada da classe mais perto e vira a especialização dela. Especialize as guildas no nível 3.</p>
    ${howTable(['Classe', 'Especializações'], Object.entries(PRESTIGE).map(([cls, paths]) => [cls, paths.join(' / ')]))}
    <h3>Manias</h3>
    ${howTable(['Mania', 'O que faz'], Object.entries(TRAITS).map(([trait, about]) => [trait, about]))}
    <h3>Experiência</h3>
    <p>Heróis só ganham experiência lutando ou curando. O nível vai até ${HEIR_MAX_LEVEL}.</p>
    ${howFormula(`Por golpe = 13 + 3 × (nível do alvo − nível do herói), no mínimo 1<br>Golpe que mata: × 1,25 · Cura: 13`)}
    ${howTable(['Do nível', 'Experiência para subir'], [['1 e 2', 100], ['3 a 7', 150], ['8 a 12', 225], ['13 a 17', 300], ['18 a 22', 400], ['23 a 27', 500], ['28 e 29', 600]])}` },
  { title: 'Atributos', html: () => {
    const table = (name, key) => { const [hp, atk, mag, def, dex] = EMBER_CLASSES[key].stats[0]; return [name, hp, atk, mag, def, dex, EMBER_CLASSES[key].mov]; };
    const rows = Object.entries(CLASS_TABLE).map(([cls, key]) => table(cls, key));
    rows.push(table('Camponês', 'peasant1'), table('Camponesa', 'villagerF1'), table('Salteador', 'rogue'), table('Besteiro', 'brigandCrossbowman'));
    rows.push(['Caveira', ...SKELETON_TABLE.stats[0], SKELETON_TABLE.mov]);
    return `<p>Valores no nível 1. Todas as classes crescem no mesmo ritmo, então só a diferença de nível muda quem leva vantagem.</p>
    ${howFormula(`Atributo no nível N = valor do nível 1 × (1 + ${hn(STAT_GROWTH)} × (N − 1)), arredondado<br>Atributo final = atributo do nível + bônus do equipamento`)}
    ${howTable(['Classe', 'Vida', 'Ataque', 'Magia', 'Defesa', 'Destreza', 'Mov.'], rows)}`;
  } },
  { title: 'Combate', html: () => `
    <p>Heróis enxergam inimigos até ${AGGRO_RANGE} de distância e largam o que estavam fazendo para lutar, com ou sem recompensa. Cada inimigo na cidade atrai pelo menos o herói livre mais perto, até quem estava bebendo.</p>
    <h3>Dano</h3>
    ${howFormula(`Poder = Ataque (magos usam Magia) · Guarda = Defesa do alvo (contra magia, metade)<br>Dano = Poder² / (Poder + Guarda) × variação de 0,8 a 1,2 × 1,5 se crítico, no mínimo 1`)}
    <p>Defesa alta reduz o dano, mas nunca zera.</p>
    <h3>Crítico e esquiva</h3>
    ${howFormula(`Chance = (40 × minha Destreza / (minha Destreza + Destreza dele) − 15)%, entre 0% e 30%`)}
    <p>Com Destreza igual, 5%. A mesma conta vale para acertar crítico e para desviar.</p>
    <h3>Ritmo e alcance</h3>
    <ul>
      <li>Um golpe a cada ${hn(ATTACK_COOLDOWN)} s. Corpo a corpo alcança ${hn(MELEE_RANGE)}; arqueiros e magos, ${SHOT_RANGE}.</li>
      <li>Curandeiros (Healer, Bishop, Cleric) curam aliados abaixo de 70% da vida: cura = Magia do curandeiro.</li>
      <li>Fora de combate, todos recuperam 3% da vida máxima por minuto de jogo.</li>
      <li>Camponeses caçadores perseguem javalis e alces nas Wilds; os demais só revidam se forem atacados.</li>
    </ul>` },
  { title: 'Inimigos', html: () => `
    <h3>Ladrões</h3>
    <p>O primeiro bando chega ao meio-dia do dia 1 pela estrada velha; depois, um bando a cada ${RAID_EVERY / 60} horas.</p>
    ${howFormula(`Ladrões no bando = 1 + (dia − 1)/2, arredondado para baixo, no máximo 4<br>Nível = 1 + (dia − 1)/2, arredondado para baixo`)}
    <p>40% são besteiros, o resto salteadores. Cada um escolhe um alvo (o casarão ou uma construção), rouba e foge.</p>
    ${howFormula(`Roubo = menor valor entre o ouro do cofre e 10 + 5 × nível<br>Sem ouro: até 10 do alimento mais estocado`)}
    <p>Quem derruba o ladrão devolve o roubo ao cofre e, se for herói, fica com o bolso dele: 2 a 5 + 2 × nível de ouro.</p>
    <h3>Caveiras</h3>
    <p>Toda noite, às 22h, o cemitério solta uma caveira com nível igual ao dia. Ela sobe a estrada devagar (velocidade ${hn(SKELETON_SPEED)}) e aparece na aba Heróis com o tempo que falta. Na cidade, bate na construção mais perto até virar ruína, depois na próxima.</p>
    ${howFormula(`Dano da caveira na construção = 1,5 × Ataque, a cada ${hn(ATTACK_COOLDOWN * 1.5)} s`)}
    <h3>Covis dos Wilds</h3>
    <p>Covis ativos de lobos, goblins e trolls soltam uma criatura a cada ${LAIR_SPAWN_EVERY / 60} horas de jogo. Elas descem pela estrada velha.</p>
    <h3>Destruir covis</h3>
    <p>Na aba <b>Heróis</b>, cada covil tem um botão <b>Destruir</b>: é uma recompensa como as outras. O herói que aceita desce a estrada velha até os Wilds e bate no covil até ele cair. As criaturas por perto defendem.</p>
    ${howTable(['Covil', 'Vida'], Object.entries(LAIR_HP).map(([level, hp]) => [`Nível ${level}`, hp]))}
    ${howFormula(`Dano no covil por golpe = Ataque (Magia, para magos) × 0,8 a 1,2, a cada ${hn(ATTACK_COOLDOWN)} s`)}
    <h3>Dead Remains</h3>
    <p>O covil derrubado deixa um Dead Remains. Só heróis recolhem. O herói leva ao depósito (conta como 1 unidade) e cobra do cofre. Se ele morrer no caminho, os restos ficam no chão até outro herói buscar.</p>
    ${howTable(['Grade', 'Ouro ao herói', 'Ember', 'Covil nível 1', 'Covil nível 2'], REMAINS_GRADES.map((grade, i) => [grade.id, grade.gold, grade.ember, `${Math.round(REMAINS_ODDS[1][i] * 100)}%`, `${Math.round(REMAINS_ODDS[2][i] * 100)}%`]))}
    <h3>Laboratório de Ember</h3>
    <p>Um camponês leva os Dead Remains do depósito ao laboratório, o de grade mais alta primeiro, e o laboratório refina em Ember. Refino por nível: ${LAB_RATE.map(value => `${hn(value)}/s`).join(', ')}, com o mesmo bônus da equipe.</p>` },
  { title: 'Recompensas', html: () => `
    <p>Na aba <b>Heróis</b> você escolhe o valor (mínimo 5) e onde: um ponto do mapa (vigiar ${BOUNTY_GUARD / 60} hora), a patrulha do segundo mapa (ida e volta), as ruínas do covil ou caçar uma caveira na estrada. O ouro sai do cofre na hora.</p>
    <h3>Quem aceita</h3>
    ${howFormula(`Interesse = valor / (10 + 0,3 × distância) × mania × 0,85 se for longe<br>O herói aceita a de maior interesse, se for 1 ou mais e ele tiver pelo menos metade da vida`)}
    ${howTable(['Mania', 'Efeito'], [['Ganancioso', 'recusa abaixo de 40; × 1,5'], ['Covarde', 'recusa abaixo de 20, e abaixo de 60 se for longe; × 0,6'], ['Bêbado', '× 0,7'], ['Valente', '× 1,25'], ['Teimoso', 'metade das vezes nem escuta; × 1,2'], ['Honesto, Amaldiçoado', 'sem mudança']])}
    <p>Quem cumpre embolsa o valor inteiro. Na caça, quem derruba a caveira embolsa. Cancelar, deixar vencer (${BOUNTY_DAYS} dias sem ninguém) ou a caveira sumir devolve o ouro ao cofre.</p>` },
  { title: 'Nobres', html: () => `
    <h3>Tributos</h3>
    <p>O primeiro tributo chega às 10h do dia 1; depois, um a cada 1 ou 2 dias, com no máximo 2 abertos. Em 15% das vezes quem pede é o Barão das Cinzas.</p>
    ${howFormula(`Quantidade = base × (1 + 0,15 × dia) × 2 se for o Barão, arredondado`)}
    ${howTable(['Recurso', 'Base'], Object.entries(DEMAND_BASE).map(([res, base]) => [resourceName(res).replace(/^./, c => c.toUpperCase()), base]))}
    <p>Prazo: 72 horas (Barão, 54). Entregue na aba <b>Economia</b>.</p>
    <h3>Favor</h3>
    ${howTable(['Situação', 'Nobre', 'Barão'], [['Entregar no prazo', '+8', '+15'], ['Entregar atrasado', '+2', '+2'], ['Atrasar', '−5 na hora e −1 por hora', '−5 na hora e −1 por hora'], [`${DEMAND_GRACE / 60} horas depois do prazo`, '−10, leva o recurso e tira 30% da vida de 1 construção', '−20, leva o recurso e tira 60% da vida de 2 construções']])}
    <p>Com Favor abaixo de ${FAVOR_ATTACK}, uma vez por dia algum nobre ataca e tira de 25% a 50% da vida de 1 ou 2 construções.</p>
    ${howTable(['Favor', 'Fama'], [['0 a 19', 'Inimigo'], ['20 a 39', 'Mal-visto'], ['40 a 59', 'Tolerado'], ['60 a 79', 'Bem-visto'], ['80 a 100', 'Favorito']])}` },
  { title: 'Equipamento', html: () => `
    <p>Heróis têm 10 espaços e chegam com a arma básica da classe. Camponeses têm 5 (ferramenta, roupa, mãos, pés e cabeça). Ninguém recebe ordem de compra.</p>
    <h3>Arsenal</h3>
    ${howTable(['Nível', 'Ouro', 'Madeira', 'Vende até'], ARMORY_LEVELS.map((level, i) => { const base = BUILDINGS.find(item => item.id === 'armory'); return [i + 1, i ? level.gold : base.cost, i ? level.wood : base.wood, `tier ${level.tierCap}`]; }))}
    <p>Toda hora, cada herói ou camponês livre tem 30% de chance de passar no Arsenal, no máximo uma compra por dia. Compra com a própria bolsa a melhor peça que puder pagar, no primeiro espaço da sua lista de prioridade que tiver melhora. O ouro da venda vai para o cofre.</p>
    ${howTable(['Mania', 'Compra primeiro'], [['Covarde', 'armadura, cabeça'], ['Ganancioso', 'anéis, amuleto'], ['Valente', 'arma, mãos'], ['Teimoso', 'arma, anéis'], ['Amaldiçoado', 'amuleto, armadura'], ['Bêbado', 'capa, pés'], ['Outros', 'arma, armadura']])}
    <p>Camponeses compram na ordem ferramenta, roupa, pés, mãos e cabeça; sem profissão, pulam a ferramenta. O equipamento deles dá trabalho, carga, passo ou obra.</p>
    <h3>Loja do monarca</h3>
    <p>No Arsenal, o herdeiro compra com o cofre peças para Cetro, Globo, Manto, Coroa, Medalhão e dois anéis. Preço por tier: ${MONARCH_TIER_PRICE.slice(1).map((price, i) => `T${i + 2} ${price}`).join(', ')}.</p>` },
  { title: 'Herdeiro', html: () => {
    const spells = typeof MONARCH_SPELLS !== 'undefined' ? MONARCH_SPELLS : null;
    const field = spells?.fieldSpells || [];
    return `<h3>Nível e atributos</h3>
    <p>O herdeiro vai até o nível ${HEIR_MAX_LEVEL}, com a mesma tabela de experiência dos heróis. Ganha 10 por herói recrutado, 25 por obra ou melhoria pronta, 40 por dia e 4 por inimigo morto com magia. A cada nível ganho, recebe 6 pontos para distribuir na ficha; os atributos não aumentam automaticamente.</p>
    ${howTable(['Atributo', 'Base no nível 1', 'Efeito por ponto distribuído'], HEIR_STATS.map(stat => [`${stat.name} (${stat.key})`, stat.base, stat.effect]))}
    <h3>Magia</h3>
    <p>O herdeiro não aparece no mapa. Abra o botão <b>✦ MAGIAS</b> no canto do mapa, escolha a magia e clique no alvo. Esc ou botão direito cancela.</p>
    ${howFormula(`MP máximo = 30 + 6 × (nível − 1) + (CHA + INT + WIS) ÷ 2 − reserva das auras ligadas (inclui bônus de equipamento)<br>Recuperação = (WIS × 0,10 + INT × 0,04) × 0,5 por segundo (metade em combate)<br>Potência das auras = efeito-base × (1 + CHA × 0,005)<br>Dano das magias = base + crescimento por nível + (% de MAG da magia × MAG)`)}
    <p>Auras ao mesmo tempo: 1 no nível 2, 2 no nível 12 e 3 no nível 20. São 30 magias do Monarca (ordens, auras, editos, decretos e invocações) e 30 magias de campo, uma nova por nível.</p>
    ${howFormula(`Duração de uma magia do Monarca com base de 60 s = 60 s + 1 s por nível acima do desbloqueio.<br>Vale para efeitos com tempo: auras, ordens, escudos, zonas, lentidão, silêncio, enraizar, atordoar, provocar e invocações. As auras desligam automaticamente ao fim da duração. Recargas não mudam.`)}
    ${howFormula(`Valor de dano de uma magia de campo = base + por nível × (nível do herdeiro − nível da magia) + % de MAG do herdeiro<br>Dano causado = valor² / (valor + metade da Defesa do alvo)`)}
    ${field.length ? howTable(['Nível', 'Magia de campo', 'MP', 'Recarga'], field.map(spell => [spell.unlockLevel, spell.name, spell.manaCost, `${spell.cooldownSeconds} s`])) : ''}`;
  } },
];

const SCREENS = {
  title() {
    const latest = latestSave();
    return { cls: 'is-title', html: `<div class="title-screen">
      <div class="title-logo"><h1>Wasteland Farming</h1><p class="title-sub">HERITAGE OF ASHES · A KINGDOM OF MORDAVIA FANTASY SIM</p><p class="title-epigraph">${titleLine}</p><p class="title-version">${GAME_VERSION}</p></div>
      <nav class="menu-list" aria-label="Menu principal">
        ${latest ? '<button type="button" class="primary" data-action="load">Continuar</button>' : ''}
        <button type="button" class="${latest ? '' : 'primary'}" data-action="new">Novo jogo</button>
        ${latest ? '<button type="button" data-action="load">Carregar jogo</button>' : ''}
        <button type="button" data-action="howto">Como jogar</button>
        <button type="button" data-action="nobles">Nobres</button>
        <button type="button" data-action="options">Opções</button>
        <button type="button" data-action="debug">Debug</button>
        <button type="button" data-action="editor">Editor de mapas</button>
      </nav>
      <p class="title-save">${latest ? `Slot ${latest.slot} · ${slotSummary(latest.record)}` : 'Nenhum jogo salvo ainda.'}</p>
    </div>` };
  },
  economy() {
    const latest = latestSave();
    const source = game.slot ? state : latest?.record?.state || newGameState();
    const e = economySnapshot(source);
    const foodRows = FOOD_TYPES.map(food => `<div><dt>${food.name}${food.raw ? ' · vai pro moinho' : food.id === 'flour' ? ' · do moinho' : food.baked ? ' · da padaria' : ''}</dt><dd>+${economyRate(e.produced[food.id])} /s</dd></div>`).join('');
    const demandRow = `<div><dt>Consumo dos camponeses · ${RATIONS[source.ration ?? 2]?.label || 'Normal'}</dt><dd>−${economyRate(e.consumption)} /s</dd></div>`;
    const costs = BUILDINGS.map(building => `<tr><th>${building.name}</th><td>${building.cost} ouro</td><td>${building.wood} madeira</td></tr>`).join('');
    return { cls: menuMode(), html: `<section class="menu-card economy-card wide">
      <header class="menu-card-head"><span class="eyebrow">${game.slot ? `DIA ${Math.floor(source.minutes / 1440) + 1}` : latest ? 'ÚLTIMO SAVE' : 'REGRAS DO INÍCIO'}</span><h2>Economia do domínio</h2><p>${game.slot ? 'Valores atuais do jogo pausado.' : latest ? 'Valores gravados no save mais recente.' : 'Sem save: mostrando o início da partida. Produção e taxas mudam com as construções e moradores.'}</p></header>
      <div class="economy-metrics"><div><small>OURO DISPONÍVEL</small><b>${format(source.gold || 0)}</b></div><div><small>MADEIRA</small><b>${format(source.timber || 0)}</b><em>+${economyRate(e.woodRate)}/s</em></div><div><small>PROVISÕES EM ESTOQUE</small><b>${format(e.stock)} / ${e.capacity}</b></div><div><small>IMPOSTO CONFIGURADO</small><b>${source.taxPercent ?? 10}%</b></div></div>
      <div class="economy-columns"><section><h3>Provisões por segundo</h3><dl class="economy-list">${foodRows}${demandRow}<div class="economy-net"><dt>Saldo líquido atual</dt><dd>${e.edible - e.consumption >= 0 ? '+' : '−'}${economyRate(Math.abs(e.edible - e.consumption))} /s</dd></div></dl></section>
        <section><h3>Ouro na próxima cobrança</h3><dl class="economy-list"><div><dt>Camponeses · ${e.workers} × ${economyRate(2 * ((source.taxPercent ?? 10) / 10))} por dia</dt><dd>+${economyRate(e.workerTax)}</dd></div><div><dt>Conta da taverna acumulada</dt><dd>+${economyRate(e.tavernTax)}</dd></div><div><dt>Salários · ${e.workers} × ${WORKER_WAGE}</dt><dd>−${economyRate(e.wages)}</dd></div><div class="economy-net"><dt>Estimativa da próxima coleta</dt><dd>≈ ${economyRate(e.taxEstimate)} ouro</dd></div></dl><p class="economy-note">Impostos são recolhidos à meia-noite do jogo. A taverna depende de quem bebe e de quanto tempo ficou.</p></section></div>
      <section class="economy-costs"><h3>Gasto para construir</h3><div class="economy-table-wrap"><table><thead><tr><th>Construção</th><th>Ouro</th><th>Madeira</th></tr></thead><tbody>${costs}</tbody></table></div><p class="economy-note">Esses são custos únicos de construção. Melhorias têm custo próprio; comida é o consumo contínuo. Construções não geram ouro.</p></section>
      <div class="menu-actions"><button type="button" class="primary" data-action="back">Voltar</button></div>
    </section>` };
  },
  continue() {
    const latest = latestSave();
    if (!latest) return SCREENS.title();
    return { cls: 'is-title', html: `<section class="menu-card">
      <header class="menu-card-head"><span class="eyebrow">CONTINUAR</span><h2>A fazenda continua horrível</h2><p>Mas continua sua.</p></header>
      <div class="save-slot static"><div class="slot-main">${saveCard(latest.slot, latest.record)}</div></div>
      <div class="menu-actions"><button type="button" class="primary" data-action="load-slot" data-slot="${latest.slot}">Continuar</button><button type="button" data-action="load">Outros saves</button><button type="button" data-action="back">Voltar</button></div>
    </section>` };
  },
  // Um menu de saves só, com a mesma cara de onde for aberto. Vindo de Novo jogo, qualquer slot começa um jogo novo;
  // vindo de Continuar ou Carregar, jogo salvo continua e espaço vazio começa um novo.
  saves({ mode }) {
    const fresh = mode === 'new';
    const slots = allSaves().map(({ slot, record }) => {
      const side = record ? `<span class="slot-actions"><button type="button" class="slot-delete" data-action="new-slot" data-slot="${slot}">Novo jogo</button><button type="button" class="slot-delete" data-action="delete-slot" data-slot="${slot}">Apagar</button></span>` : '';
      return `<div class="save-slot${record ? '' : ' empty'}${game.slot === slot ? ' current' : ''}"><button type="button" class="slot-main" data-action="${record && !fresh ? 'load-slot' : 'new-slot'}" data-slot="${slot}">${saveCard(slot, record)}</button>${side}</div>`;
    }).join('');
    return { cls: menuMode(), html: `<section class="menu-card wide">
      <header class="menu-card-head"><span class="eyebrow">SAVES</span><h2>Jogos salvos</h2><p>${fresh ? 'Clique num espaço para começar um jogo novo nele.' : 'Clique num jogo para continuar. Clique num espaço vazio para começar um novo.'}</p></header>
      <div class="save-list">${slots}</div>
      <div class="menu-actions"><button type="button" data-action="back">Voltar</button></div>
    </section>` };
  },
  intro({ slot, heir }) {
    const occupied = !!readSave(slot);
    return { cls: 'is-title', html: `<section class="menu-card wide intro">
      <header class="menu-card-head"><span class="eyebrow">CENÁRIO 1 · A HERANÇA</span><h2>Wasteland Farming</h2>
        <p class="lead">Seu tio morreu e deixou a fazenda pra você. Ninguém da família quis. Agora você vai descobrir por quê.</p></header>
      <p class="goal"><b>Objetivo:</b> juntar 600 moedas. O antigo dono deixou dívidas, e elas cobram juros.</p>
      <ul class="how">
        <li><b>Casebres</b> trazem camponeses. Eles trabalham nas fazendas e moinhos e comem. Na aba <b>Provisões</b> você escolhe a ração e o que servir. Carne deixa todo mundo mais feliz.</li>
        <li><b>Guildas, taverna e hotel</b> trazem heróis. Você não manda neles. Eles fazem o que querem.</li>
        <li>Escolha a construção e <b>toque onde quiser</b> no mapa. Onde não dá, ela fica vermelha. <b>Esc</b> cancela.</li>
        <li>Toda construção é uma <b>obra</b>: um camponês precisa trabalhar dois minutos no local.</li>
        <li><b>Ladrões</b> chegam pela estrada velha a cada 6 horas. Heróis que os veem lutam sozinhos e ganham experiência com isso.</li>
        <li>O <b>ouro</b> vem dos impostos, cobrados à meia-noite: dos camponeses e de quem bebe na taverna.</li>
        <li><b>Esc</b> ou <b>MENU</b> pausa e salva.</li>
      </ul>
      <div class="menu-actions"><button type="button" class="primary" data-action="accept" data-slot="${slot}">Aceitar a herança</button><button type="button" data-action="back">Voltar</button></div>
    </section>` };
  },
  heir({ slot, selected }) {
    const card = (heir, i) => `<button type="button" class="heir-card${i === selected ? ' selected' : ''}" data-action="pick-heir" data-index="${i}" aria-pressed="${i === selected}">
        <img class="heir-card-portrait" src="${heirPortraitSrc(heir.id)}" alt="Retrato jovem de ${heir.name}"><b>${heir.name.split(' ')[0]}</b><small>${HOUSES[heir.house].name.replace(/^Casa /, '')} · ${heir.age}</small></button>`;
    const men = HEIRS.map((heir, i) => ({ heir, i })).filter(entry => entry.heir.gender === 'm');
    const women = HEIRS.map((heir, i) => ({ heir, i })).filter(entry => entry.heir.gender === 'f');
    const chosen = HEIRS[selected];
    const house = HOUSES[chosen.house];
    return { cls: 'is-title', html: `<section class="menu-card heir-screen">
      <header class="menu-card-head"><span class="eyebrow">NOVO JOGO</span><h2>Quem herda o Vale do Luto?</h2><p>Ninguém da família quis. Escolha quem vai ser obrigado.</p></header>
      <div class="heir-layout">
        <div class="heir-pick">
          <div class="heir-row-label">HERDEIROS</div>
          <div class="heir-grid">${men.map(entry => card(entry.heir, entry.i)).join('')}</div>
          <div class="heir-row-label">HERDEIRAS</div>
          <div class="heir-grid">${women.map(entry => card(entry.heir, entry.i)).join('')}</div>
        </div>
        <div class="heir-detail">
          <img class="heir-detail-portrait" src="${heirPortraitSrc(chosen.id)}" alt="${chosen.name}, fase jovem">
          <span class="eyebrow">${house.name.toUpperCase()}</span><h3>${chosen.name}</h3><p class="heir-motto">“${house.motto}”</p>
          <p>${chosen.about}</p><p class="heir-tie">${chosen.age} anos · ${chosen.tie}</p>
        </div>
      </div>
      <div class="menu-actions"><button type="button" class="primary" data-action="choose-heir" data-slot="${slot}">Escolher ${chosen.name.split(' ')[0]}</button><button type="button" data-action="back">Voltar</button></div>
    </section>` };
  },
  // Nobres e linhagens: a mesma lista que ficava na aba NOBRES do jogo, agora na entrada, com a casa de cada um.
  nobles() {
    const houseNobles = NOBLES.map(noble => ({ name: noble.name, title: noble.title, house: noble.house, src: noblePortraitSrc(noble.house, 1), gender: ['grenholt', 'lucerna'].includes(noble.house) ? 'm' : 'f' }));
    const heirs = HEIRS.filter(heir => !heir.portrait).map(heir => ({ name: heir.name, title: heir.tie, house: heir.house, src: heirPortraitSrc(heir.id, 1), gender: heir.gender }));
    const all = [...houseNobles, ...heirs];
    const men = all.filter(entry => entry.gender === 'm').length;
    const card = entry => `<article class="noble-roster-card"><img src="${entry.src}" alt="${entry.name}"><div class="noble-roster-copy"><b>${entry.name}</b><small>${entry.title}</small><small class="noble-house">${HOUSES[entry.house].name} · “${HOUSES[entry.house].motto}”</small></div></article>`;
    return { cls: 'is-title', html: `<section class="menu-card nobles-screen">
      <header class="menu-card-head"><span class="eyebrow">NOBRES</span><h2>Nobres e linhagens</h2><p>${all.length} personagens · ${men} homens · ${all.length - men} mulheres.</p></header>
      <div class="noble-roster">${all.map(card).join('')}</div>
      <div class="menu-actions"><button type="button" class="primary" data-action="back">Voltar</button></div>
    </section>` };
  },
  howto({ section = 0 }) {
    const current = HOWTO[section] || HOWTO[0];
    return { cls: menuMode(), html: `<section class="menu-card wide howto-screen">
      <header class="menu-card-head"><span class="eyebrow">COMO JOGAR</span><h2>Manual do herdeiro</h2><p>Tudo o que o tio não explicou antes de morrer. Regras e fórmulas do jogo.</p></header>
      <nav class="howto-nav" aria-label="Seções do manual">${HOWTO.map((item, i) => `<button type="button" data-action="howto-section" data-index="${i}" class="${item === current ? 'active' : ''}" aria-pressed="${item === current}">${item.title}</button>`).join('')}</nav>
      <article class="howto-body">${current.html()}</article>
      <div class="menu-actions"><button type="button" class="primary" data-action="back">Voltar</button></div>
    </section>` };
  },
  sheet() {
    const heir = heirById(state.heir);
    if (!heir) return { cls: 'is-pause', html: `<section class="menu-card"><header class="menu-card-head"><span class="eyebrow">FICHA DO HERDEIRO</span><h2>Herdeiro desconhecido</h2><p>Este save é de antes da escolha de herdeiros. Ninguém lembra quem assinou a herança.</p></header><div class="menu-actions"><button type="button" class="primary" data-action="back">Voltar ao jogo</button></div></section>` };
    const house = HOUSES[heir.house];
    const phase = heirPhase();
    const elapsed = heirYearsElapsed();
    return { cls: 'is-pause', html: `<section class="menu-card sheet">
      <div class="sheet-close-row"><button type="button" class="sheet-close" data-action="resume" aria-label="Fechar ficha" title="Fechar ficha">×</button></div>
      <div class="sheet-top"><img class="sheet-portrait" src="${heirPortraitSrc(heir.id, phase)}" alt="${heir.name}, fase ${phase}">
        <div><span class="eyebrow">FICHA DO HERDEIRO · FASE ${phase} DE 4</span><h2>${heir.name}</h2><p class="heir-motto">${house.name} · “${house.motto}”</p><p class="heir-age">${heirAge(heir)} anos · ${elapsed ? `${elapsed} anos de Ember` : 'sem corrupção de Ember'}</p></div></div>
      <div class="sheet-level">
        <div class="sheet-level-head"><b>Nível ${state.heirLevel}</b><span>de ${HEIR_MAX_LEVEL}</span><small>${state.heirLevel >= HEIR_MAX_LEVEL ? 'Nível máximo. O tédio também.' : `${Math.floor(state.heirXp)} / ${xpToNext(state.heirLevel)} XP`}</small></div>
        <div class="xp-bar"><i style="width:${state.heirLevel >= HEIR_MAX_LEVEL ? 100 : Math.round(state.heirXp / xpToNext(state.heirLevel) * 100)}%"></i></div>
      </div>
      ${heirAttributesHTML()}
      ${window.Monarch ? Monarch.sheetHTML() : ''}
      <dl class="sheet-facts">
        <div><dt>Idade</dt><dd>${heirAge(heir)} anos</dd></div>
        <div><dt>Parentesco</dt><dd>${heir.tie}</dd></div>
      </dl>
      <p class="sheet-about">${heir.about}</p>
      <div class="sheet-section-label">EQUIPAMENTO</div>
      <div class="equip-grid">${EQUIP_SLOTS.map(({ slot, label }) => { const item = ITEMS[(state.equipment || BASIC_EQUIPMENT())[slot]]; return `<div class="equip-slot" title="${item ? `${item.about}${Object.keys(item.bonus || {}).length ? ` (${monarchBonusText(item.bonus)})` : ''}` : 'Vazio'}">${item ? pixelIcon(item.icon, 28) : ''}<span><small>${label}${item?.tier ? ` · T${item.tier}` : ''}</small><b>${item ? item.name : 'Vazio'}</b></span></div>`; }).join('')}</div>
    </section>` };
  },
  graveyard() {
    const region = currentMap === 'wilds' ? 'Wilds' : 'City';
    const here = actor => (actor.deathMap || actor.mapId || 'city') === currentMap && (actor.deathMapSet || actor.mapSet || 1) === currentMapSet;
    const dead = [...state.heroes, ...state.workers].filter(actor => !actor.alive && !actor.summon && here(actor)).sort((a, b) => (b.deathAt ?? -1) - (a.deathAt ?? -1));
    const heroes = dead.filter(actor => actor.className !== 'Peasant'), peasants = dead.filter(actor => actor.className === 'Peasant');
    const row = actor => `<li class="grave-row"><img src="public/art/portraits/${actorPortrait(actor)}-portrait-v01.webp" alt="">
        <span><b>${actor.name}</b><small>${actorLabel(actor)} · Nível ${actor.level || 1}</small></span>
        <em>${actor.deathAt != null ? dateLabel(actor.deathAt) : 'Data esquecida'}${actor.deathBy ? `<br>por ${actor.deathBy}` : ''}</em></li>`;
    const group = (label, list, empty) => `<div class="sheet-section-label">${label} · ${list.length}</div>${list.length ? `<ul class="grave-list">${list.map(row).join('')}</ul>` : `<p class="sheet-house grave-empty">${empty}</p>`}`;
    return { cls: 'is-pause', html: `<section class="menu-card sheet graveyard">
      <div class="sheet-close-row"><button type="button" class="sheet-close" data-action="resume" aria-label="Fechar cemitério" title="Fechar cemitério">×</button></div>
      <div class="sheet-top"><img class="grave-crest" src="public/art/icon-heroes-cemetery-photo-v01.png" alt="Lápide memorial com espadas cruzadas">
        <div><span class="eyebrow">CEMITÉRIO · ${region.toUpperCase()}</span><h2>Os que ficaram em ${region}</h2><p class="heir-motto">${dead.length ? `${dead.length} ${dead.length === 1 ? 'nome' : 'nomes'}. Mais do que o padre consegue rezar de uma vez.` : 'Ninguém ainda. A terra está esperando com paciência.'}</p></div></div>
      ${group('HERÓIS', heroes, 'Nenhum herói caiu aqui. Por enquanto.')}
      ${group('CAMPONESES', peasants, 'Nenhum camponês enterrado aqui. Os outros acham suspeito.')}
    </section>` };
  },
  actor({ id }) {
    const actor = findActor(id);
    if (!actor) return { cls: 'is-pause', html: `<section class="menu-card"><header class="menu-card-head"><span class="eyebrow">FICHA</span><h2>Não está mais aqui</h2><p>Foi embora, morreu ou as duas coisas.</p></header><div class="menu-actions"><button type="button" class="primary" data-action="back">Voltar ao jogo</button></div></section>` };
    const level = actor.level || 1;
    return { cls: 'is-pause', html: `<section class="menu-card sheet">
      <div class="sheet-close-row"><button type="button" class="sheet-close" data-action="resume" aria-label="Fechar ficha" title="Fechar ficha">×</button></div>
      <div class="sheet-top"><img class="sheet-portrait" src="public/art/portraits/${actorPortrait(actor)}-portrait-v01.webp" alt="">
        <div><span class="eyebrow">FICHA · ${actor.className === 'Monster' ? 'MONSTRO' : actor.className === 'Bandit' ? 'LADRÃO' : actor.className === 'Peasant' ? 'SERVO' : 'PROTETOR'}</span><h2>${actor.name}</h2><p class="heir-motto">${actorLabel(actor)}</p></div></div>
      ${levelHTML(level, actor.xp || 0)}
      <p class="sheet-house">Vida agora: ${Math.ceil(hpOf(actor))} de ${maxHpOf(actor)}</p>
      ${attributesHTML(attributesAt(tableFor(actor), level), actor)}
      ${actor.trait ? `<p class="sheet-about"><b>${actor.trait}.</b> ${TRAITS[actor.trait] || ''}</p>` : ''}
      ${actor.className === 'Peasant' ? `<dl class="info-lines sheet-work"><div><dt>Ofício</dt><dd>${professionName(actor)}</dd></div><div><dt>Trabalha</dt><dd>${workplaceOf(actor) ? siteLabel(workplaceOf(actor)) : 'Em lugar nenhum'}</dd></div><div><dt>Mora</dt><dd>${homeOf(actor) ? siteLabel(homeOf(actor)) : 'Casarão'}</dd></div><div><dt>Agora</dt><dd>${peasantActivity(actor)}</dd></div>${actor.carrying ? `<div><dt>Carregando</dt><dd>${format(actor.carrying.qty)} de ${foodName(actor.carrying.food)}</dd></div>` : ''}<div><dt>Alcoolismo · penalidade</dt><dd>${Math.floor(alcoholismOf(actor))}/100</dd></div><div><dt>Bolsa</dt><dd>${format(purseOf(actor))} de ouro · salário ${WORKER_WAGE} por dia</dd></div></dl>` : `<dl class="info-lines sheet-work"><div><dt>Alcoolismo · penalidade</dt><dd>${Math.floor(alcoholismOf(actor))}/100</dd></div><div><dt>Bolsa</dt><dd>${format(purseOf(actor))} de ouro, ganho no loot</dd></div></dl>`}
      ${actor.className === 'Peasant' ? `${tradesHTML(actor)}<div class="sheet-section-label">HABILIDADES</div><dl class="skill-grid">${SKILLS.map(skill => { const value = skillOf(actor, skill.id); return `<div><dt>${skill.name}</dt><dd><i style="width:${value}%"></i></dd><small>${Math.floor(value)}</small></div>`; }).join('')}</dl>` : ''}
      ${gearHTML(actor)}
      <p class="sheet-house">${actorOrigin(actor)}</p>
    </section>` };
  },
  gameOver() {
    const heir = heirById(state.heir);
    const first = heir ? heir.name.split(' ')[0] : 'O herdeiro';
    return { cls: 'is-pause', html: `<section class="menu-card">
      <header class="menu-card-head"><span class="eyebrow">FIM DE JOGO</span><h2>O Casarão caiu</h2>
        <p>${first} estava lá dentro. Os monstros não pediram licença, e as paredes não aguentaram. A herança volta para os credores, que ficaram com os escombros.</p></header>
      <div class="menu-actions">${game.slot && readSave(game.slot) ? `<button type="button" class="primary" data-action="load-slot" data-slot="${game.slot}">Carregar o último save</button>` : ''}<button type="button" data-action="dead-quit">Voltar ao menu principal</button></div>
    </section>` };
  },
  scenarioEnd({ servants, protectors }) {
    const heir = heirById(state.heir);
    const workers = state.workers.filter(worker => worker.alive);
    const heroes = state.heroes.filter(hero => hero.alive);
    const needS = Math.min(2, workers.length), needP = Math.min(3, heroes.length);
    const pickCard = (actor, kind, chosen) => `<button type="button" class="retinue-card${chosen.includes(String(actor.id)) ? ' selected' : ''}" data-action="toggle-retinue" data-kind="${kind}" data-id="${actor.id}">
        <img src="public/art/portraits/${actorPortrait(actor)}-portrait-v01.webp" alt=""><span><b>${actor.name}</b><small>${actorLabel(actor)} · Nível ${actor.level || 1}</small></span></button>`;
    const ready = servants.length === needS && protectors.length === needP;
    return { cls: 'is-pause', html: `<section class="menu-card wide retinue">
      <header class="menu-card-head"><span class="eyebrow">FIM DO CENÁRIO 1 · A HERANÇA</span><h2>O Vale do Luto paga as próprias dívidas</h2>
        <p>Por enquanto. Escolha quem segue com ${heir ? heir.name.split(' ')[0] : 'você'} para o castelo. O resto fica. Ninguém vai chorar.</p></header>
      <div class="heir-row-label">SERVOS · ${servants.length} DE ${needS}</div>
      <div class="retinue-grid">${workers.map(worker => pickCard(worker, 's', servants)).join('') || '<p class="save-empty">Nenhum servo sobrou.</p>'}</div>
      <div class="heir-row-label">PROTETORES · ${protectors.length} DE ${needP}</div>
      <div class="retinue-grid">${heroes.map(hero => pickCard(hero, 'p', protectors)).join('') || '<p class="save-empty">Nenhum protetor sobrou.</p>'}</div>
      <div class="menu-actions"><button type="button" class="primary" data-action="confirm-retinue" ${ready ? '' : 'disabled'}>Seguir para o castelo</button></div>
    </section>` };
  },
  retinueDone() {
    const r = state.retinue || { servants: [], protectors: [] };
    const names = list => list.map(actor => `<li><b>${actor.name}</b> · ${actorLabel(actor)} · Nível ${actor.level || 1}</li>`).join('');
    return { cls: 'is-pause', html: `<section class="menu-card">
      <header class="menu-card-head"><span class="eyebrow">COMITIVA FORMADA</span><h2>Quem segue com você</h2><p>O castelo do próximo cenário vai abrigar a comitiva. Ele ainda está sendo erguido. Os pedreiros prometeram. Pedreiros prometem muito.</p></header>
      <div class="heir-row-label">SERVOS</div><ul class="retinue-list">${names(r.servants) || '<li>Ninguém.</li>'}</ul>
      <div class="heir-row-label">PROTETORES</div><ul class="retinue-list">${names(r.protectors) || '<li>Ninguém.</li>'}</ul>
      <div class="menu-actions"><button type="button" class="primary" data-action="resume">Continuar no Vale do Luto</button><button type="button" data-action="save-quit">Voltar ao menu principal</button></div>
    </section>` };
  },
  confirm({ title, text, confirmLabel, danger }) {
    return { cls: menuMode(), html: `<section class="menu-card">
      <header class="menu-card-head"><span class="eyebrow">CONFIRMAR</span><h2>${title}</h2><p>${text}</p></header>
      <div class="menu-actions"><button type="button" class="${danger ? 'danger' : 'primary'}" data-action="confirm-yes">${confirmLabel}</button><button type="button" data-action="back">Cancelar</button></div>
    </section>` };
  },
  options() {
    return { cls: menuMode(), html: `<section class="menu-card">
      <header class="menu-card-head"><span class="eyebrow">OPÇÕES</span><h2>Opções</h2></header>
      <div class="option-list">
        <div class="option-row"><div><b>Som</b><small>Música e efeitos do jogo.</small></div>${segmented('sound', options.sound, [[1, 'Ligado'], [0, 'Desligado']])}</div>
        <div class="option-row"><div><b>Velocidade do jogo</b><small>Controla a passagem do tempo nesta partida.</small></div>${segmented('gameSpeed', state.speed, [[1, '1×'], [2, '2×'], [3, '3×']])}</div>
        <div class="option-row"><div><b>Salvar sozinho</b><small>Salva o jogo de tempos em tempos e quando você fecha a página.</small></div>${segmented('autosave', options.autosave, [[0, 'Desligado'], [1, '1 min'], [3, '3 min'], [5, '5 min']])}</div>
        <div class="option-row"><div><b>Zoom ao começar</b><small>Ampla, normal ou próxima. Dá pra trocar no mapa a qualquer hora.</small></div>${segmented('startZoom', options.startZoom, [[0, 'Ampla'], [1, 'Normal'], [2, 'Próxima']])}</div>
        <div class="option-row"><div><b>Nomes dos personagens</b><small>Eles têm nome. Ninguém lembra qual.</small></div>${segmented('names', options.names, [['selected', 'Ao tocar'], ['always', 'Sempre']])}</div>
        <div class="option-row"><div><b>Aparência das construções</b><small>Só troca a arte. Não altera produção, custos, regras nem saves.</small></div>${segmented('buildingArtStyle', options.buildingArtStyle, [['dark', 'Escura'], ['color', 'Colorida']])}</div>
      </div>
      <div class="menu-actions"><button type="button" class="primary" data-action="back">Voltar</button></div>
    </section>` };
  },
  debug() {
    const used = allSaves().filter(entry => entry.record).length;
    return { cls: menuMode(), html: `<section class="menu-card">
      <header class="menu-card-head"><span class="eyebrow">AJUSTES DE INÍCIO</span><h2>Estoque e saves</h2><p>A quantidade inicial não altera as regras da partida.</p></header>
      <div class="option-list">
        <div class="option-row"><div><b>Estoque inicial</b><small>Alto mantém ouro e madeira em 1 bilhão e a pedra em 40 kg, repostos a cada segundo, em qualquer partida. Custos, consumo e todas as regras continuam iguais; o saldo só não acaba.</small></div>${segmented('startingStock', debug.startingStock === 'full' ? 1 : 0, [[0, 'Padrão'], [1, 'Alto']])}</div>
        <div class="option-row"><div><b>Apagar todos os saves</b><small>Remove os ${SAVE_SLOTS} slots deste navegador.</small></div><button type="button" class="danger small" data-action="wipe" ${used ? '' : 'disabled'}>Apagar</button></div>
      </div>
      <dl class="debug-info"><div><dt>Versão</dt><dd>${GAME_VERSION}</dd></div><div><dt>Slots usados</dt><dd>${used} / ${SAVE_SLOTS}</dd></div><div><dt>Jogo aberto</dt><dd>${game.slot ? `Slot ${game.slot}` : 'Nenhum'}</dd></div></dl>
      <div class="menu-actions"><button type="button" class="primary" data-action="back">Voltar</button></div>
    </section>` };
  },
  log() {
    const entries = (state.messageLog || []).slice().reverse();
    const safe = text => String(text).replace(/[&<>"]/g, ch => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[ch]);
    const when = raw => { const minutes = Math.floor(raw); return `Dia ${Math.floor(minutes / 1440) + 1} · ${pad2(Math.floor(minutes / 60) % 24)}:${pad2(minutes % 60)}`; };
    return { cls: menuMode(), html: `<section class="menu-card wide">
      <header class="menu-card-head"><span class="eyebrow">LOG</span><h2>Mensagens do domínio</h2><p>${entries.length ? 'Da mais nova para a mais antiga. Ninguém leu as outras também.' : 'Nada aconteceu ainda. Aproveite enquanto dura.'}</p></header>
      ${entries.length ? `<ol class="message-log">${entries.map(entry => `<li><small>${when(entry.t)}</small><span>${safe(entry.text)}${entry.n > 1 ? ` <em>×${entry.n}</em>` : ''}</span></li>`).join('')}</ol>` : ''}
      <div class="menu-actions"><button type="button" class="primary" data-action="back">Voltar</button></div>
    </section>` };
  },
  // Continuação do primeiro objetivo: os credores levam o ouro e os nobres mandam cuidar dos mortos.
  debtPaid() {
    const heir = heirById(state.heir);
    const first = heir ? heir.name.split(' ')[0] : 'o herdeiro';
    return { cls: 'is-pause', html: `<section class="menu-card wide mission-story">
      <header class="menu-card-head"><span class="eyebrow">OBJETIVO 1 CUMPRIDO · A DÍVIDA</span><h2>Os credores levaram tudo. Os nobres gostaram.</h2>
        <p>À meia-noite, três homens de luto bateram no casarão e contaram as ${DEBT_GOLD} moedas duas vezes. Levaram todas. Deixaram um recibo e um cheiro de vela.</p>
        <p>De manhã chegou uma carta com o selo do Conselho dos Nobres. Eles ficaram impressionados: ninguém nunca tinha pago aquela dívida. Nem o falecido, que morreu justamente pra não pagar.</p>
        <p>O favor tem preço. Os mortos do Vale do Luto pararam de ficar deitados, e os ladrões da estrada acham que o vale agora tem dinheiro. Os nobres querem os dois problemas resolvidos antes do próximo enterro. Mandam ${first} cuidar disso. Pessoalmente, não: mandar heróis serve.</p></header>
      <div class="mission-rewards"><span class="reward-chip loss">−${DEBT_GOLD} ouro</span><span class="reward-chip gain">+${DEBT_FAVOR} Favor dos nobres</span></div>
      <p class="mission-goal"><b>Objetivo atual:</b> destrua um covil. Na aba Heróis, ofereça uma recompensa para destruí-lo.</p>
      <div class="menu-actions"><button type="button" class="primary" data-action="resume">Voltar ao jogo</button></div>
    </section>` };
  },
  pause() {
    const day = Math.floor(state.minutes / 1440) + 1;
    const clock = `${pad2(Math.floor(state.minutes / 60) % 24)}:${pad2(state.minutes % 60)}`;
    return { cls: 'is-pause', html: `<section class="menu-card narrow">
      <header class="menu-card-head"><span class="eyebrow">DIA ${day} · ${clock}</span><h2>Pausado</h2></header>
      <nav class="menu-list compact" aria-label="Menu de pausa">
        <button type="button" class="primary" data-action="resume">Voltar ao jogo</button>
        <button type="button" data-action="save">Salvar jogo</button>
        <button type="button" data-action="load">Carregar jogo</button>
        <button type="button" data-action="log">Log de mensagens</button>
        <button type="button" data-action="options">Opções</button>
        <button type="button" data-action="save-quit">Voltar ao menu principal</button>
      </nav>
    </section>` };
  },
};

function renderMenu() {
  const current = menu.stack[menu.stack.length - 1];
  if (!current) return;
  const { cls, html } = SCREENS[current.name](current.params || {});
  const layer = $('#menu-layer');
  layer.className = `menu-layer ${cls}${['sheet', 'actor', 'graveyard'].includes(current.name) ? ' sheet-open' : ''}`;
  layer.hidden = false;
  $('#menu-panel').innerHTML = html;
  fitMenu();
  $('#menu-panel button:not([disabled])')?.focus({ preventScroll: true });
}

// A tela de menu encolhe o mínimo necessário para caber na altura da janela, sem rolagem.
// Escolha de herdeiro e Nobres não encolhem (as fotos ficariam minúsculas): essas rolam.
function fitMenu() {
  $('#menu-layer').scrollTop = 0;
  const card = $('#menu-panel').firstElementChild;
  if (!card || card.classList.contains('sheet') || card.classList.contains('heir-screen') || card.classList.contains('nobles-screen') || card.classList.contains('howto-screen')) return;
  card.style.zoom = '';
  const layer = $('#menu-layer');
  const pad = parseFloat(getComputedStyle(layer).paddingTop) + parseFloat(getComputedStyle(layer).paddingBottom);
  const available = window.innerHeight - pad - 6;
  let zoom = 1;
  for (let i = 0; i < 4; i++) {
    const height = card.getBoundingClientRect().height;
    if (height <= available || zoom <= 0.5) break;
    zoom = Math.max(0.5, zoom * available / height - 0.005);
    card.style.zoom = zoom.toFixed(3);
  }
}

function showScreen(name, params = {}) {
  menu.stack.push({ name, params });
  game.running = false;
  renderMenu();
}

function closeMenu() {
  menu.stack = [];
  $('#menu-layer').hidden = true;
  if (game.slot) game.running = true;
}

function goBack() {
  if (menu.stack.length > 1) { menu.stack.pop(); renderMenu(); return; }
  if (game.slot) closeMenu();
}

function openPause() {
  if (!game.slot) return;
  menu.stack = [];
  showScreen('pause');
}

function confirmThen(params, onConfirm) {
  menu.pending = onConfirm;
  showScreen('confirm', params);
}

function toTitle() {
  game.slot = null;
  game.running = false;
  menu.stack = [];
  showScreen('title');
  setMusic('title');
}

function initMenus() {
  initMusic();
  applyOptions();
  $('#menu-panel').addEventListener('click', event => {
    const button = event.target.closest('button');
    if (!button || button.disabled) return;
    const slot = Number(button.dataset.slot);
    if (button.dataset.set) {
      const name = button.dataset.set;
      const raw = button.dataset.value;
      const value = Number.isNaN(Number(raw)) ? raw : Number(raw);
      if (name === 'startingStock') setStartingStock(value === 1);
      else if (name === 'gameSpeed') {
        state.speed = value;
        $('#map-scene').style.setProperty('--mill-spin-duration', `${8 / state.speed}s`);
        if (game.slot) writeSave(game.slot);
        say(`Velocidade do domínio: ${state.speed}×.`, `A rotina agora corre a ${state.speed}×.`);
      }
      else {
        options[name] = value;
        writeStore(OPTIONS_KEY, options);
        applyOptions();
        if (name === 'buildingArtStyle') { renderSites(); renderPanel(); }
      }
      renderMenu();
      return;
    }
    switch (button.dataset.action) {
      case 'heir-stat-up': adjustHeirAttribute(button.dataset.stat, 1); break;
      case 'heir-stat-down': adjustHeirAttribute(button.dataset.stat, -1); break;
      case 'continue': showScreen('continue'); break;
      case 'new': showScreen('saves', { mode: 'new' }); break;
      case 'load': showScreen('saves', { mode: 'load' }); break;
      case 'nobles': showScreen('nobles'); break;
      case 'howto': showScreen('howto', { section: 0 }); break;
      case 'howto-section':
        menu.stack[menu.stack.length - 1].params.section = Number(button.dataset.index);
        renderMenu();
        $('.howto-body')?.scrollTo(0, 0);
        break;
      case 'economy': showScreen('economy'); break;
      case 'log': showScreen('log'); break;
      case 'equip-crafted-weapon': equipCraftedWeapon(button.dataset.actor, button.dataset.item); break;
      case 'options': showScreen('options'); break;
      case 'debug': showScreen('debug'); break;
      case 'editor': openEditor(); break;
      case 'back': goBack(); break;
      case 'resume': closeMenu(); break;
      case 'load-slot':
        if (game.slot && game.slot !== slot) confirmThen({ title: `Carregar o slot ${slot}?`, text: `O jogo aberto vai ser salvo no slot ${game.slot} antes. Por precaução, não por otimismo.`, confirmLabel: 'Carregar' }, () => { writeSave(game.slot); loadGame(slot); });
        else loadGame(slot);
        break;
      case 'new-slot':
        if (readSave(slot)) confirmThen({ title: `Novo jogo no slot ${slot}?`, text: 'O jogo que está lá some pra sempre.', confirmLabel: 'Começar novo', danger: true }, () => { menu.stack.pop(); showScreen('heir', { slot, selected: 0 }); });
        else showScreen('heir', { slot, selected: 0 });
        break;
      case 'pick-heir': case 'pick-noble': {
        // Trocar de retrato redesenha a tela; a rolagem fica onde o jogador estava.
        const keepScroll = $('#menu-layer').scrollTop;
        menu.stack[menu.stack.length - 1].params.selected = Number(button.dataset.index);
        renderMenu();
        $('#menu-layer').scrollTop = keepScroll;
        break;
      }
      case 'choose-heir': showScreen('intro', { slot, heir: HEIRS[menu.stack[menu.stack.length - 1].params.selected].id }); break;
      case 'accept': newGame(slot, menu.stack[menu.stack.length - 1].params.heir); break;
      case 'delete-slot':
        confirmThen({ title: `Apagar o slot ${slot}?`, text: 'Esse jogo some pra sempre. Como a família anterior.', confirmLabel: 'Apagar', danger: true }, () => {
          deleteSave(slot);
          if (game.slot === slot) { toTitle(); return; }
          menu.stack.pop();
          renderMenu();
        });
        break;
      case 'wipe':
        confirmThen({ title: 'Apagar todos os saves?', text: 'Todos os jogos salvos deste navegador serão apagados. Um recomeço limpo, coisa rara em Mordavia.', confirmLabel: 'Apagar tudo', danger: true }, () => {
          for (let i = 1; i <= SAVE_SLOTS; i++) deleteSave(i);
          toTitle();
        });
        break;
      case 'toggle-retinue': {
        const params = menu.stack[menu.stack.length - 1].params;
        const list = button.dataset.kind === 's' ? params.servants : params.protectors;
        const cap = button.dataset.kind === 's' ? 2 : 3;
        const id = button.dataset.id;
        if (list.includes(id)) list.splice(list.indexOf(id), 1); else if (list.length < cap) list.push(id);
        renderMenu();
        break;
      }
      case 'confirm-retinue': {
        const params = menu.stack[menu.stack.length - 1].params;
        const copy = actor => { const { workplace, route, finalPoint, ...rest } = actor; return JSON.parse(JSON.stringify(rest)); };
        const selected = [...params.servants, ...params.protectors].map(findActor).filter(Boolean);
        selected.forEach(actor => { actor.mapSet = 2; });
        state.mapSet = 2;
        currentMapSet = 2;
        switchMap(currentMap);
        state.retinue = { scenario: 1, servants: params.servants.map(findActor).filter(Boolean).map(copy), protectors: params.protectors.map(findActor).filter(Boolean).map(copy) };
        renderSites(); renderHeroes();
        writeSave(game.slot);
        menu.stack = [];
        showScreen('retinueDone');
        break;
      }
      case 'confirm-yes': { const run = menu.pending; menu.pending = null; run?.(); break; }
      case 'pick-spec': {
        const actor = findActor(button.dataset.id);
        const spec = TRADE_BY_KEY[button.dataset.trade]?.specializations.find(item => item.key === button.dataset.spec);
        if (!actor || !spec) break;
        const { id, trade, spec: specKey } = button.dataset;
        confirmThen({ title: `${actor.name} vira ${spec.name}?`, text: 'A especialização é definitiva. A outra opção some pra sempre, como o resto da família.', confirmLabel: 'Escolher' }, () => { chooseSpecialization(id, trade, specKey); goBack(); });
        break;
      }
      case 'save': say(writeSave(game.slot) ? 'Jogo salvo.' : 'Não deu pra salvar. O navegador recusou.'); renderMenu(); break;
      case 'save-quit': writeSave(game.slot); toTitle(); break;
      case 'dead-quit': toTitle(); break;
    }
  });
  $('#menu-btn').addEventListener('click', openPause);
  const positionObjectiveMenu = () => {
    const map = $('#map-scene').getBoundingClientRect();
    const list = $('#objective-list');
    const top = Math.max(0, map.top), bottom = Math.min(window.innerHeight, map.bottom);
    list.style.setProperty('--objective-left', `${map.left + map.width / 2}px`);
    list.style.setProperty('--objective-top', `${(top + bottom) / 2}px`);
    list.style.setProperty('--objective-width', `${Math.max(0, map.width - 24)}px`);
  };
  $('#objective-journal').addEventListener('toggle', positionObjectiveMenu);
  window.addEventListener('resize', positionObjectiveMenu);
  $('#economy-open-btn').addEventListener('click', () => {
    state.tab = 'economy';
    $$('.tab').forEach(tab => { const active = tab.dataset.tab === state.tab; tab.classList.toggle('active', active); tab.setAttribute('aria-selected', String(active)); });
    renderPanel();
  });
  ['#grain', '#happiness'].forEach(sel => $(sel).closest('.stat').addEventListener('click', () => {
    if (!game.slot) return;
    state.tab = 'food';
    $$('.tab').forEach(tab => { const active = tab.dataset.tab === state.tab; tab.classList.toggle('active', active); tab.setAttribute('aria-selected', String(active)); });
    renderPanel();
  }));
  $('#favor').closest('.stat').addEventListener('click', () => {
    if (!game.slot) return;
    state.tab = 'economy';
    $$('.tab').forEach(tab => { const active = tab.dataset.tab === state.tab; tab.classList.toggle('active', active); tab.setAttribute('aria-selected', String(active)); });
    renderPanel();
  });
  $('#demand-alerts').addEventListener('click', event => {
    if (!event.target.closest('[data-demand-open]') || !game.slot) return;
    state.tab = 'economy';
    $$('.tab').forEach(tab => { const active = tab.dataset.tab === state.tab; tab.classList.toggle('active', active); tab.setAttribute('aria-selected', String(active)); });
    renderPanel();
  });
  $('#sheet-btn').addEventListener('click', () => { if (!game.slot) return; menu.stack = []; showScreen('sheet'); });
  $('#graveyard-btn').addEventListener('click', () => { if (!game.slot) return; menu.stack = []; showScreen('graveyard'); });
  $('#log-btn').addEventListener('click', () => { if (!game.slot) return; menu.stack = []; showScreen('log'); });
  // Clique fora de qualquer menu fecha o menu. No jogo, volta pro jogo; sem jogo aberto, volta pra tela de entrada.
  // A tela de entrada e o fim de cenário (que pede uma escolha) não fecham.
  $('#menu-layer').addEventListener('click', event => {
    // Só conta clique no fundo: um botão que acabou de trocar a tela já saiu da página e não é clique fora.
    if (!event.target.isConnected || event.target.closest('#menu-panel > *')) return;
    const top = menu.stack[menu.stack.length - 1]?.name;
    if (!top || top === 'title' || top === 'scenarioEnd' || top === 'gameOver') return;
    menu.pending = null;
    if (game.slot) closeMenu();
    else { menu.stack = []; showScreen('title'); }
  });
  document.addEventListener('keydown', event => {
    if (event.key === 'Escape' && !$('#inventory-layer').hidden) { event.preventDefault(); closeManorInventory(); return; }
    if (event.key !== 'Escape' || editor.active) return;
    if (!$('#menu-layer').hidden) {
      const top = menu.stack[menu.stack.length - 1]?.name;
      if (top === 'title') return;
      event.preventDefault();
      if (top === 'scenarioEnd' || top === 'gameOver') return;
      if (['pause', 'sheet', 'actor', 'graveyard', 'retinueDone'].includes(top)) closeMenu(); else goBack();
    } else if (state.selectedBuild) {
      cancelPlacement();
    } else if (state.movingSite != null) {
      cancelMove();
    } else if (state.placingBounty) {
      cancelBountyPlacement();
    } else if (selectedLairId != null) {
      selectedLairId = null;
      renderLairs(); renderPanel();
    } else if (state.selectedSite != null) {
      state.selectedSite = null; state.demolishArmed = null;
      renderSites(); renderPanel();
    } else {
      event.preventDefault();
      openPause();
    }
  });
  window.addEventListener('resize', () => { if (!$('#menu-layer').hidden) fitMenu(); });
  window.addEventListener('pagehide', () => { if (game.slot && options.autosave) writeSave(game.slot); });
  showScreen('title');
  setMusic('title');
}

// ==========================================================================
// Cenários e editor de mapas. Cada cenário guarda covis e locais de interesse no mesmo sistema
// de coordenadas do jogo: x em % da largura do mapa, y de 0 a 200 (como PATH_NODES).
// Save no modelo do editor do Ember Ashes: cada Salvar grava uma versão nova com número (v001, v002...),
// que vira a ativa no jogo; as versões ficam listadas para Carregar, Ativar ou Apagar (dois cliques).
// Tudo vai para este navegador e, com a pasta do jogo ligada, para data/scenarios/<arquivo>.js, que o
// index.html carrega. Vale o registro mais recente dos dois. O rascunho (o que ainda não foi salvo)
// fica guardado no navegador, como o rascunho aberto do editor do Ember.
// ==========================================================================
const SCENARIOS = [{ id: 1, name: 'Floresta', file: '01-floresta' }];
const PLACE_GROUPS = [{ id: 'lair', name: 'Covis' }, { id: 'poi', name: 'Locais' }];
const PLACE_TYPES = [
  { id: 'wolfDen', group: 'lair', name: 'Covil de lobos', icon: '<circle cx="6.5" cy="9.5" r="1.8"/><circle cx="10.5" cy="6" r="1.8"/><circle cx="15" cy="6.5" r="1.8"/><circle cx="18" cy="10.5" r="1.7"/><path d="M8 17.5c0-3.2 2.3-5.6 4.6-5.6s4.2 2.4 4.2 4.8c0 2-1.6 3-3.3 2.4-1.2-.4-1.9-.4-3 .1-1.6.6-2.5-.2-2.5-1.7z"/>' },
  { id: 'spiderNest', group: 'lair', name: 'Ninho de aranhas', icon: '<path d="M12 3v18M3 12h18M5.6 5.6l12.8 12.8M18.4 5.6L5.6 18.4"/><path d="M12 7.2l3.4 1.4 1.4 3.4-1.4 3.4-3.4 1.4-3.4-1.4L7.2 12l1.4-3.4z"/>' },
  { id: 'goblinDen', group: 'lair', name: 'Toca de goblins', icon: '<path d="M3 20c0-8 4-13 9-13s9 5 9 13"/><path d="M8 20c0-4 1.8-7 4-7s4 3 4 7M2 20h20"/>' },
  { id: 'trollDen', group: 'lair', name: 'Covil de troll', icon: '<path d="M5 19.5l7.5-7.5"/><path d="M11.5 13c-1.2-3 .8-7.2 4-8.2s4.6 1.8 3.4 4.2-4.4 4-7.4 4z"/>' },
  { id: 'graveyard', group: 'lair', name: 'Cemitério', icon: '<path d="M7 20V10a5 5 0 0 1 10 0v10"/><path d="M4 20h16M12 8.5v6.5M9.5 11h5"/>' },
  { id: 'crypt', group: 'lair', name: 'Cripta', icon: '<path d="M5 20V9l7-5 7 5v11"/><path d="M9.5 20v-6a2.5 2.5 0 0 1 5 0v6M3 20h18"/>' },
  { id: 'banditCamp', group: 'lair', name: 'Acampamento de ladrões', icon: '<path d="M3 19L12 5l9 14z"/><path d="M9 19l3-5.5 3 5.5"/>' },
  { id: 'ruins', group: 'poi', name: 'Ruínas', icon: '<path d="M3 20h18M5.5 20V9H9v11M15 20v-6.5h3.5V20M4.5 9h5.5M14 13.5h5.5M9 9l1.5-2.5"/>' },
  { id: 'shrine', group: 'poi', name: 'Santuário', icon: '<path d="M6 20v-9l6-6 6 6v9M4 20h16"/><path d="M12 10v6M10 12h4"/>' },
  { id: 'well', group: 'poi', name: 'Poço', icon: '<path d="M4 8.5l8-4.5 8 4.5M7 8.5V14M17 8.5V14M12 8.5V11"/><path d="M5 14h14v6H5z"/>' },
  { id: 'mine', group: 'poi', name: 'Mina', icon: '<path d="M4.5 9.5c4.2-4.4 10.8-4.4 15 0"/><path d="M12 6.4L7 20"/><path d="M14 20a4 4 0 0 1 6 0"/>' },
  { id: 'watchtower', group: 'poi', name: 'Torre de vigia', icon: '<path d="M8 20l1.5-11h5L16 20M7 9h10M8 9V5h8v4M10 5V3.5M14 5V3.5M5 20h14"/>' },
  { id: 'bridge', group: 'poi', name: 'Ponte', icon: '<path d="M2 11h20M4 11v6M20 11v6M7 17a5 5 0 0 1 10 0"/>' },
  { id: 'quarry', group: 'poi', name: 'Pedreira', icon: '<path d="M3 19l4-6 3 3 4-7 7 10z"/>' },
  { id: 'gallows', group: 'poi', name: 'Forca', icon: '<path d="M6 20V4h9M6 8l4-4M15 4v4M3 20h8"/><circle cx="15" cy="10" r="2"/>' },
  { id: 'crossroads', group: 'poi', name: 'Encruzilhada', icon: '<path d="M12 21V3M12 6h7l2 2-2 2h-7M12 12H5l-2 2 2 2h7"/>' },
];
const placeType = id => PLACE_TYPES.find(type => type.id === id);
const scenarioDef = id => SCENARIOS.find(item => item.id === id) || SCENARIOS[0];
const scenarioKey = id => `mordavia.scenario.${pad2(id)}`;
const scenarioDraftKey = id => `mordavia.scenario.${pad2(id)}.draft`;
const scenarioTitle = id => `Cenário ${pad2(id)} · ${scenarioDef(id).name}`;
const scenarioFilePath = id => `data/scenarios/${scenarioDef(id).file}.js`;
const versionLabel = serial => `v${String(serial).padStart(3, '0')}`;
const editor = { active: false, scenario: 1, record: null, places: [], base: null, tool: null, group: 'lair', selected: null, drag: null, preview: false, armedDelete: null, status: '' };
let worldScenario = null;

// Troca única: os covis de lobos vão para onde estavam os trolls, e os trolls descem para onde
// estavam os lobos. Cada covil de lobos troca de lugar com o covil de troll mais perto dele.
// Vale para todas as versões salvas e para o rascunho do editor, e roda uma vez só por navegador.
(function swapWolfAndTrollDens() {
  const doneKey = 'mordavia.fix.swapWolfTroll.v1';
  try { if (localStorage.getItem(doneKey)) return; } catch { return; }
  const swapIn = places => {
    if (!Array.isArray(places)) return;
    const trolls = places.filter(place => place?.type === 'trollDen');
    places.filter(place => place?.type === 'wolfDen').forEach(wolf => {
      if (!trolls.length) return;
      const near = trolls.reduce((best, troll) => Math.hypot(troll.x - wolf.x, (troll.y - wolf.y) / 2) < Math.hypot(best.x - wolf.x, (best.y - wolf.y) / 2) ? troll : best);
      trolls.splice(trolls.indexOf(near), 1);
      [wolf.x, wolf.y, near.x, near.y] = [near.x, near.y, wolf.x, wolf.y];
    });
  };
  const fix = (key, apply) => {
    try {
      const data = JSON.parse(localStorage.getItem(key));
      if (!data) return;
      apply(data);
      localStorage.setItem(key, JSON.stringify(data));
    } catch {}
  };
  SCENARIOS.forEach(def => {
    fix(scenarioKey(def.id), data => { swapIn(data.places); (data.versions || []).forEach(version => swapIn(version?.places)); });
    fix(scenarioDraftKey(def.id), data => swapIn(data.places));
  });
  try { localStorage.setItem(doneKey, '1'); } catch {}
})();

const cleanPlaces = list => (Array.isArray(list) ? list : []).filter(place => placeType(place?.type) && Number.isFinite(+place.x) && Number.isFinite(+place.y))
  .map(place => ({ id: String(place.id), type: place.type, x: +place.x, y: +place.y,
    ...(place.mapId === 'city' || place.mapId === 'wilds' ? { mapId: place.mapId } : {}),
    ...(place.type === 'quarry' && Number.isFinite(+place.initialUnits) ? { initialUnits: Math.max(0, +place.initialUnits) } : {}),
    ...(place.type === 'mine' ? { rank: METAL_DEPOSIT_RANKS.some(rank => rank.id === place.rank) ? place.rank : 'II' } : {}) }));
// O registro de um cenário: {id, name, updatedAt, active, versions: [{serial, savedAt, places}]}.
// Aceita o formato antigo ({savedAt, places}) como a versão 1.
function normalizeRecord(data) {
  if (!data || typeof data !== 'object') return null;
  if (Array.isArray(data.versions)) {
    const versions = data.versions.filter(item => Number.isInteger(item?.serial) && item.serial > 0)
      .map(item => ({ serial: item.serial, savedAt: item.savedAt || 0, places: cleanPlaces(item.places) })).sort((a, b) => a.serial - b.serial);
    return { updatedAt: data.updatedAt || 0, active: versions.some(item => item.serial === data.active) ? data.active : versions.at(-1)?.serial ?? null, versions };
  }
  if (Array.isArray(data.places) && (data.places.length || data.savedAt))
    return { updatedAt: data.savedAt || 0, active: 1, versions: [{ serial: 1, savedAt: data.savedAt || 0, places: cleanPlaces(data.places) }] };
  return null;
}
function loadScenarioRecord(id) {
  const def = scenarioDef(id);
  let local = null;
  try { local = normalizeRecord(JSON.parse(localStorage.getItem(scenarioKey(def.id)))); } catch {}
  const fromFile = normalizeRecord(window.MORDAVIA_SCENARIOS?.[def.id]);
  const best = [local, fromFile].filter(Boolean).sort((a, b) => b.updatedAt - a.updatedAt)[0];
  return { id: def.id, name: def.name, updatedAt: best?.updatedAt || 0, active: best?.active ?? null, versions: best?.versions || [] };
}
const activeVersion = record => record.versions.find(item => item.serial === record.active) || record.versions.at(-1) || null;
// O que o jogo mostra: a versão ativa do cenário.
function loadScenario(id) {
  const record = loadScenarioRecord(id);
  return { id: record.id, name: record.name, serial: activeVersion(record)?.serial ?? null, places: activeVersion(record)?.places || [] };
}
function scenarioFileText(record) {
  const json = JSON.stringify({ id: record.id, name: record.name, updatedAt: record.updatedAt, active: record.active, versions: record.versions }, null, 2);
  return `// ${scenarioTitle(record.id)}. Gravado pelo editor de mapas do jogo: cada Salvar é uma versão; "active" é a que o jogo usa.\n// x em % da largura do mapa, y de 0 a 200.\n(window.MORDAVIA_SCENARIOS = window.MORDAVIA_SCENARIOS || {})[${record.id}] = ${json};\n`;
}

// Covis e locais no mapa. No jogo (e no preview do editor) só aparecem; editando, viram botões que dá pra arrastar.
function placeMarkerHTML(place, editable) {
  const type = placeType(place.type);
  const tag = editable ? 'button' : 'div';
  const name = place.type === 'mine' ? `Mina · ${place.rank || 'II'}` : type.name;
  return `<${tag} ${editable ? 'type="button" ' : ''}class="poi ${type.group}${editable && editor.selected === place.id ? ' selected' : ''}" data-place="${place.id}" style="--x:${place.x}%;--y:${place.y / WORLD_Y_UNITS * 100}%;z-index:${Math.round(place.y * 10)}" title="${name}"><span class="poi-icon" aria-hidden="true"><svg viewBox="0 0 24 24">${type.icon}</svg></span><small>${name}</small></${tag}>`;
}
function renderPlaces() {
  const editing = editor.active && !editor.preview;
  let places = editor.active ? editor.places : worldScenario?.places || [];
  places = places.filter(place => place.type !== 'quarry');
  const layer = $('#poi-layer');
  layer.classList.toggle('editing', editing);
  layer.innerHTML = places.map(place => placeMarkerHTML(place, editing)).join('');
  // A sombra do objeto escolhido segue o mouse antes do clique: o preview de onde ele vai ficar.
  if (editing && editor.tool) layer.insertAdjacentHTML('beforeend', placeMarkerHTML({ id: 'ghost', type: editor.tool, x: -50, y: -50 }, false).replace('class="poi ', 'class="poi ghost '));
}

// A pasta do jogo fica lembrada no navegador (IndexedDB) depois da primeira vez.
const FOLDER_DB = 'mordavia-editor';
function folderStore(mode, value) {
  return new Promise(resolve => {
    try {
      const open = indexedDB.open(FOLDER_DB, 1);
      open.onupgradeneeded = () => open.result.createObjectStore('handles');
      open.onerror = () => resolve(null);
      open.onsuccess = () => {
        try {
          const tx = open.result.transaction('handles', mode);
          const request = mode === 'readwrite' ? tx.objectStore('handles').put(value, 'gameFolder') : tx.objectStore('handles').get('gameFolder');
          request.onsuccess = () => resolve(mode === 'readwrite' ? true : request.result || null);
          request.onerror = () => resolve(null);
        } catch { resolve(null); }
      };
    } catch { resolve(null); }
  });
}
async function gameFolder(ask) {
  let folder = await folderStore('readonly');
  if (folder) {
    let permission = await folder.queryPermission({ mode: 'readwrite' });
    if (permission !== 'granted' && ask) permission = await folder.requestPermission({ mode: 'readwrite' });
    if (permission === 'granted') return folder;
  }
  if (!ask) return null;
  folder = await window.showDirectoryPicker({ id: 'wasteland-farming', mode: 'readwrite' });
  try { await folder.getFileHandle('index.html'); await folder.getFileHandle('app.js'); } catch { throw new Error('wrong-folder'); }
  await folderStore('readwrite', folder);
  return folder;
}
// Como o Ember: grava, lê de volta e confere. Se não bater, avisa que não salvou o arquivo.
async function writeScenarioFile(record, ask) {
  if (!window.showDirectoryPicker) return 'unsupported';
  const folder = await gameFolder(ask);
  if (!folder) return 'unlinked';
  const text = scenarioFileText(record);
  const dir = await (await folder.getDirectoryHandle('data', { create: true })).getDirectoryHandle('scenarios', { create: true });
  const handle = await dir.getFileHandle(`${scenarioDef(record.id).file}.js`, { create: true });
  const writable = await handle.createWritable();
  await writable.write(text);
  await writable.close();
  if (await (await handle.getFile()).text() !== text) throw new Error('mismatch');
  return 'ok';
}
function downloadScenarioFile(record) {
  const link = document.createElement('a');
  link.href = URL.createObjectURL(new Blob([scenarioFileText(record)], { type: 'text/javascript' }));
  link.download = `${scenarioDef(record.id).file}.js`;
  document.body.append(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(link.href), 1000);
}

function setEditorStatus(text) {
  editor.status = text;
  if (editor.active && $('#editor-status')) $('#editor-status').textContent = text;
}
// Grava o registro (versões e qual está ativa) no navegador e no arquivo. ask = true só em cliques
// (Salvar, Ativar, Apagar): pode abrir a escolha da pasta do jogo na primeira vez.
let editorWrite = Promise.resolve();
function persistRecord(done, ask = true) {
  const record = editor.record;
  record.updatedAt = Date.now();
  const stored = { id: record.id, name: record.name, updatedAt: record.updatedAt, active: record.active, versions: record.versions };
  try { localStorage.setItem(scenarioKey(record.id), JSON.stringify(stored)); } catch {}
  (window.MORDAVIA_SCENARIOS = window.MORDAVIA_SCENARIOS || {})[record.id] = JSON.parse(JSON.stringify(stored));
  const run = () => writeScenarioFile(stored, ask);
  // O seletor de pasta só abre direto do clique; por isso a gravação pedida por clique não espera a fila.
  editorWrite = (ask ? run() : editorWrite.then(run)).then(result => {
    if (result === 'ok') return setEditorStatus(`${done} Gravado neste navegador e em ${scenarioFilePath(record.id)}.`);
    if (result === 'unsupported') {
      if (ask) downloadScenarioFile(stored);
      return setEditorStatus(`${done} Este navegador não grava na pasta do jogo: baixei o ${scenarioDef(record.id).file}.js; ponha em data/scenarios.`);
    }
    setEditorStatus(`${done} Gravado só neste navegador: escolha a pasta do jogo para gravar o arquivo.`);
  }).catch(error => {
    setEditorStatus(error?.message === 'wrong-folder' ? `${done} Gravado só neste navegador: essa não é a pasta do jogo (a do JOGAR.bat).`
      : error?.name === 'AbortError' ? `${done} Gravado só neste navegador: a pasta do jogo não foi escolhida.`
      : `${done} NÃO GRAVOU o arquivo ${scenarioFilePath(record.id)}; ficou só neste navegador.`);
  });
  return editorWrite;
}

// O rascunho: as mudanças ainda não salvas. Fica no navegador a cada mudança, para não perder nada.
const placesKey = places => JSON.stringify(places.map(({ type, x, y }) => [type, x, y]));
const draftDirty = () => placesKey(editor.places) !== placesKey(activeVersionOf(editor.base)?.places || []);
const activeVersionOf = serial => editor.record.versions.find(item => item.serial === serial) || null;
function keepDraft() {
  try {
    if (draftDirty()) localStorage.setItem(scenarioDraftKey(editor.scenario), JSON.stringify({ base: editor.base, places: editor.places }));
    else localStorage.removeItem(scenarioDraftKey(editor.scenario));
  } catch {}
}
function editorChanged() {
  keepDraft();
  renderEditor();
}

function saveVersion() {
  const record = editor.record;
  const serial = Math.max(0, ...record.versions.map(item => item.serial)) + 1;
  record.versions.push({ serial, savedAt: Date.now(), places: editor.places.map(place => ({ ...place })) });
  record.active = serial;
  editor.base = serial;
  editor.armedDelete = null;
  keepDraft();
  renderEditor();
  persistRecord(`Salvo: ${versionLabel(serial)}, ativa no jogo.`);
}
function loadVersion(serial) {
  const version = activeVersionOf(serial);
  if (!version) return;
  editor.places = version.places.map(place => ({ ...place }));
  editor.base = serial;
  editor.selected = null;
  editor.armedDelete = null;
  keepDraft();
  renderEditor();
  setEditorStatus(`Aberta a ${versionLabel(serial)}.`);
}
function activateVersion(serial) {
  editor.record.active = serial;
  editor.armedDelete = null;
  renderEditor();
  persistRecord(`${versionLabel(serial)} agora é a ativa no jogo.`);
}
function deleteVersion(serial) {
  if (editor.armedDelete !== serial) {
    editor.armedDelete = serial;
    renderEditor();
    return setEditorStatus(`Clique de novo em Confirmar para apagar a ${versionLabel(serial)}. Isso não tem volta.`);
  }
  const record = editor.record;
  record.versions = record.versions.filter(item => item.serial !== serial);
  if (record.active === serial) record.active = record.versions.at(-1)?.serial ?? null;
  if (editor.base === serial) editor.base = null;
  editor.armedDelete = null;
  keepDraft();
  renderEditor();
  persistRecord(`${versionLabel(serial)} apagada.${record.active ? ` Ativa no jogo: ${versionLabel(record.active)}.` : ''}`);
}

const savedAtText = time => time ? new Date(time).toLocaleString('pt-BR', { day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit' }) : '';
function versionsHTML() {
  const versions = [...editor.record.versions].reverse();
  if (!versions.length) return `<p class="editor-hint">Nenhuma versão ainda. Salvar grava a ${versionLabel(1)} em ${scenarioFilePath(editor.scenario)}.</p>`;
  return `<ol class="editor-versions">${versions.map(item => {
    const active = item.serial === editor.record.active;
    return `<li class="${active ? 'active' : ''}${item.serial === editor.base ? ' open' : ''}"><b>${versionLabel(item.serial)}</b><span>${savedAtText(item.savedAt)} · ${item.places.length} ${item.places.length === 1 ? 'local' : 'locais'}${active ? ' · ativa no jogo' : ''}${item.serial === editor.base ? ' · aberta' : ''}</span>
      <div><button type="button" data-version-load="${item.serial}">Carregar</button><button type="button" data-version-activate="${item.serial}"${active ? ' disabled' : ''}>Ativar</button><button type="button" class="danger" data-version-delete="${item.serial}">${editor.armedDelete === item.serial ? 'Confirmar' : 'Apagar'}</button></div></li>`;
  }).join('')}</ol>`;
}
function editorPanelHTML() {
  const tools = PLACE_TYPES.filter(type => type.group === editor.group).map(type => `<button type="button" class="editor-tool is-${type.group}${editor.tool === type.id ? ' active' : ''}" data-tool="${type.id}" aria-pressed="${editor.tool === type.id}"><svg viewBox="0 0 24 24" aria-hidden="true">${type.icon}</svg><span>${type.name}</span></button>`).join('');
  const groups = PLACE_GROUPS.map(group => `<button type="button" class="editor-group${editor.group === group.id ? ' active' : ''}" data-group="${group.id}">${group.name} <i>${PLACE_TYPES.filter(type => type.group === group.id).length}</i></button>`).join('');
  const place = editor.places.find(item => item.id === editor.selected);
  const count = editor.places.length;
  const dirty = draftDirty();
  return `<header class="editor-head"><span class="eyebrow">EDITOR DE MAPAS</span>
      <select id="editor-scenario" aria-label="Cenário">${SCENARIOS.map(item => `<option value="${item.id}"${item.id === editor.scenario ? ' selected' : ''}>${scenarioTitle(item.id)}</option>`).join('')}</select></header>
    <div class="editor-groups" role="tablist">${groups}</div>
    <div class="editor-tools">${tools}</div>
    <div class="editor-selected">${place ? `<span><b>${placeType(place.type).name}</b> · x ${place.x.toFixed(1)} · y ${place.y.toFixed(1)}</span><button type="button" class="editor-remove" data-remove="${place.id}">Remover</button>` : `<span>${count ? `${count} ${count === 1 ? 'local' : 'locais'} no mapa` : 'Nada no mapa ainda'}${editor.base ? ` · da ${versionLabel(editor.base)}` : ''}${dirty ? ' · <em>mudanças não salvas</em>' : ''}</span>`}</div>
    ${place?.type === 'mine' ? `<label class="editor-rank"><span>Ranking do depósito</span><select data-deposit-rank="${place.id}">${METAL_DEPOSIT_RANKS.map(rank => `<option value="${rank.id}"${(place.rank || 'II') === rank.id ? ' selected' : ''}>${rank.id} · ${rank.name}</option>`).join('')}</select></label>` : ''}
    <p class="editor-hint">Escolha um objeto e clique no mapa. Arraste para mover. Botão direito ou Delete remove. Esc solta o objeto.</p>
    <div class="editor-actions"><button type="button" class="editor-save" id="editor-save">Salvar</button><button type="button" class="editor-preview${editor.preview ? ' active' : ''}" id="editor-preview" aria-pressed="${editor.preview}">Preview</button><button type="button" class="editor-exit" id="editor-exit">Sair</button></div>
    <p class="editor-status" id="editor-status">${editor.status}</p>
    <div class="food-label">VERSÕES</div>${versionsHTML()}`;
}
function renderEditor() {
  $('#editor-card').innerHTML = editorPanelHTML();
  document.body.classList.toggle('map-preview', editor.preview);
  $('#map-scene').classList.toggle('placing', !!editor.tool && !editor.preview);
  renderPlaces();
}
function openEditor() {
  menu.stack = [];
  $('#menu-layer').hidden = true;
  game.running = false;
  editor.active = true;
  editor.record = loadScenarioRecord(editor.scenario);
  editor.base = activeVersion(editor.record)?.serial ?? null;
  editor.places = (activeVersion(editor.record)?.places || []).map(place => ({ ...place }));
  let draft = null;
  try { draft = JSON.parse(localStorage.getItem(scenarioDraftKey(editor.scenario))); } catch {}
  if (draft && Array.isArray(draft.places)) { editor.places = cleanPlaces(draft.places); editor.base = activeVersionOf(draft.base) ? draft.base : null; }
  editor.tool = null;
  editor.selected = null;
  editor.preview = false;
  editor.armedDelete = null;
  editor.status = draftDirty() ? 'Rascunho reaberto: as mudanças ainda não foram salvas.' : editor.base ? `Aberta a ${versionLabel(editor.base)}, ativa no jogo.` : 'Nada salvo ainda.';
  document.body.classList.add('map-editing');
  $('#editor-card').hidden = false;
  sizeWorld(); renderCamera();
  renderEditor();
}
function closeEditor() {
  editor.active = false;
  editor.drag = null;
  editor.preview = false;
  document.body.classList.remove('map-editing', 'map-preview');
  $('#editor-card').hidden = true;
  $('#map-scene').classList.remove('placing');
  worldScenario = loadScenario(state.scenario || 1);
  renderPlaces();
  sizeWorld(); renderCamera();
  toTitle();
}
function selectPlace(id) {
  editor.selected = id;
  renderEditor();
}
const editorCoord = (value, max) => Math.round(Math.max(2, Math.min(max - 2, value)) * 10) / 10;
function addPlace(x, y) {
  const place = { id: `p${Date.now().toString(36)}${Math.floor(Math.random() * 1296).toString(36)}`, type: editor.tool, x: editorCoord(x, 100), y: editorCoord(y, WORLD_Y_UNITS), ...(editor.tool === 'mine' ? { rank: 'II' } : {}) };
  editor.places.push(place);
  editor.selected = place.id;
  editorChanged();
}
function removePlace(id) {
  editor.places = editor.places.filter(place => place.id !== id);
  if (editor.selected === id) editor.selected = null;
  editorChanged();
}

function initEditor() {
  const scene = $('#map-scene');
  const layer = $('#poi-layer');
  $('#editor-card').addEventListener('click', event => {
    const button = event.target.closest('button');
    if (!button || button.disabled) return;
    const data = button.dataset;
    if (data.tool) { editor.tool = editor.tool === data.tool ? null : data.tool; editor.preview = false; renderEditor(); }
    else if (data.group) { editor.group = data.group; renderEditor(); }
    else if (data.remove) removePlace(data.remove);
    else if (data.versionLoad) loadVersion(Number(data.versionLoad));
    else if (data.versionActivate) activateVersion(Number(data.versionActivate));
    else if (data.versionDelete) deleteVersion(Number(data.versionDelete));
    else if (button.id === 'editor-save') saveVersion();
    else if (button.id === 'editor-preview') { editor.preview = !editor.preview; editor.tool = null; editor.selected = null; renderEditor(); }
    else if (button.id === 'editor-exit') closeEditor();
  });
  $('#editor-card').addEventListener('change', event => {
    if (event.target.dataset.depositRank) {
      const place = editor.places.find(item => item.id === event.target.dataset.depositRank);
      if (place) { place.rank = event.target.value; keepDraft(); renderEditor(); }
      return;
    }
    if (event.target.id !== 'editor-scenario') return;
    editor.scenario = Number(event.target.value);
    openEditor();
  });
  scene.addEventListener('click', event => {
    if (!editor.active || editor.preview || camera.dragMoved || event.target.closest('.poi:not(.ghost), .map-tools')) return;
    if (editor.tool) { const { x, y } = worldPoint(event); addPlace(x, y); }
    else if (editor.selected) selectPlace(null);
  });
  scene.addEventListener('pointermove', event => {
    const ghost = editor.active && !editor.preview && editor.tool ? $('.poi.ghost', layer) : null;
    if (!ghost) return;
    const { x, y } = worldPoint(event);
    ghost.style.setProperty('--x', `${editorCoord(x, 100)}%`);
    ghost.style.setProperty('--y', `${editorCoord(y, WORLD_Y_UNITS) / WORLD_Y_UNITS * 100}%`);
    ghost.style.zIndex = '2000';
  });
  scene.addEventListener('pointerleave', () => { const ghost = $('.poi.ghost', layer); if (ghost) ghost.style.setProperty('--x', '-50%'); });
  scene.addEventListener('contextmenu', event => {
    if (!editor.active) return;
    event.preventDefault();
    if (editor.preview) return;
    const marker = event.target.closest('.poi:not(.ghost)');
    if (marker) removePlace(marker.dataset.place);
    else if (editor.tool) { editor.tool = null; renderEditor(); }
  });
  layer.addEventListener('pointerdown', event => {
    const marker = event.target.closest('.poi:not(.ghost)');
    if (!editor.active || editor.preview || !marker || event.button !== 0) return;
    editor.drag = { id: marker.dataset.place, pointerId: event.pointerId, x: event.clientX, y: event.clientY, moved: false, marker };
    try { marker.setPointerCapture(event.pointerId); } catch {}
    editor.selected = marker.dataset.place;
    $('#editor-card').innerHTML = editorPanelHTML();
    $$('.poi', layer).forEach(item => item.classList.toggle('selected', item === marker));
  });
  layer.addEventListener('pointermove', event => {
    const drag = editor.drag;
    if (!drag || drag.pointerId !== event.pointerId) return;
    if (!drag.moved && Math.hypot(event.clientX - drag.x, event.clientY - drag.y) < 4) return;
    drag.moved = true;
    const place = editor.places.find(item => item.id === drag.id);
    if (!place) return;
    const { x, y } = worldPoint(event);
    place.x = editorCoord(x, 100);
    place.y = editorCoord(y, WORLD_Y_UNITS);
    drag.marker.style.setProperty('--x', `${place.x}%`);
    drag.marker.style.setProperty('--y', `${place.y / WORLD_Y_UNITS * 100}%`);
    drag.marker.style.zIndex = String(Math.round(place.y * 10));
  });
  // Ao soltar, só o painel é redesenhado: o marcador fica no lugar e o clique que vem depois cai nele.
  const endDrag = event => {
    const drag = editor.drag;
    if (!drag || drag.pointerId !== event.pointerId) return;
    editor.drag = null;
    if (drag.moved) { keepDraft(); $('#editor-card').innerHTML = editorPanelHTML(); }
  };
  layer.addEventListener('pointerup', endDrag);
  layer.addEventListener('pointercancel', endDrag);
  document.addEventListener('keydown', event => {
    if (!editor.active || event.target.closest?.('select, input')) return;
    if (event.key === 'Escape') {
      event.preventDefault();
      if (editor.preview) { editor.preview = false; renderEditor(); }
      else if (editor.tool) { editor.tool = null; renderEditor(); }
      else if (editor.selected) selectPlace(null);
    } else if ((event.key === 'Delete' || event.key === 'Backspace') && editor.selected && !editor.preview) { event.preventDefault(); removePlace(editor.selected); }
  });
  worldScenario = loadScenario(state.scenario || 1);
  renderPlaces();
}

function init() {
  initCamera();
  const scene = $('#map-scene');
  $('#inventory-layer').addEventListener('click', event => { if (event.target.closest('[data-inventory-close]')) closeManorInventory(); });
  $('#manor-inventory-trigger').addEventListener('click', event => {
    event.preventDefault();
    event.stopPropagation();
    openManorInventory();
  });
  // O menu da construção fica em cima do mapa: cliques, arrasto e rolagem nele não mexem no mapa.
  ['pointerdown', 'click', 'wheel', 'contextmenu'].forEach(type => $('#site-inspector').addEventListener(type, event => event.stopPropagation()));
  scene.addEventListener('pointermove', event => { if (state.selectedBuild || state.movingSite != null) showPlacementPreview(event); });
  scene.addEventListener('pointerleave', () => { $('#placement-preview').hidden = true; });
  scene.addEventListener('contextmenu', event => {
    if (state.selectedBuild) { event.preventDefault(); cancelPlacement(); }
    else if (state.movingSite != null) { event.preventDefault(); cancelMove(); }
    else if (state.placingBounty) { event.preventDefault(); cancelBountyPlacement(); }
  });
  scene.addEventListener('click', event => {
    // Clique no mapa fora de construções e personagens fecha o menu da construção.
    // O caminho do evento vale mesmo depois que o clique redesenhou a construção e tirou o alvo do botão.
    if ((state.selectedSite != null || selectedLairId != null) && state.movingSite == null && !state.selectedBuild && !state.placingBounty && !camera.dragMoved && !event.composedPath().some(node => node instanceof HTMLButtonElement)) {
      state.selectedSite = null; selectedLairId = null; state.demolishArmed = null;
      renderSites(); renderPanel();
      return;
    }
    if ((!state.selectedBuild && state.movingSite == null && !state.placingBounty) || camera.dragMoved || event.target.closest('button')) return;
    const { x, y } = worldPoint(event);
    if (state.placingBounty) placeBountyAt(x, y);
    else if (state.movingSite != null) moveBuilding(x, y); else placeBuilding(x, y);
  });
  $('#site-layer').addEventListener('click', event => {
    const site = event.target.closest('.site');
    if (!site) return;
    selectedLairId = null;
    const idx = Number(site.dataset.site);
    const building = state.buildings.find(item => item.site === idx);
    if (state.movingSite != null) { if (idx === state.movingSite) cancelMove(); else say('Aqui já tem construção. Escolha outro lugar.'); }
    else if (state.selectedBuild) say('Aqui já tem construção. Escolha outro lugar.');
    else if (building && building.underConstruction) {
      state.selectedBuild = null;
      state.selectedSite = idx;
      state.demolishArmed = null;
      renderSites();
      renderPanel();
      $('#site-inspector').scrollTop = 0;
      const worker = building.builderId != null ? findActor(building.builderId) : null;
      say(`${buildingName(building)}: em obra, ${Math.floor((building.progress || 0) / BUILD_TIME * 100)}%. ${worker ? `${worker.name} está trabalhando.` : 'Esperando um camponês livre.'}`);
    } else if (building) {
      state.selectedBuild = null;
      state.selectedSite = idx;
      const def = BUILDINGS.find(item => item.id === building.type);
      state.demolishArmed = null;
      renderSites();
      if (def.id === 'farm') state.selectedFarmType = farmType(building.produce).id;
      renderPanel();
      $('#site-inspector').scrollTop = 0;
      if (def.id === 'market') {
        say('Mercado: venda o que sobra, compre o que falta.');
      } else if (def.id === 'hut' || def.id === 'depot' || def.id === 'lumber') {
        say(def.id === 'lumber' ? `Cabana do Lenhador: ${lumberCrew(building).length} de 3 camponeses. Escale no painel.` : def.id === 'depot' ? `Depósito de Provisões: ${depotCap(building)} unidades. Dá pra ampliar no painel.` : building.big ? 'Casebre Grande: cinco moradores, nenhuma privacidade.' : 'Casebre Pequeno: dá pra ampliar no painel.');
      } else if (def.id === 'emberLab') {
        say(`Laboratório de Ember: ${remainsCount()} Dead Remains no depósito, ${format(state.ember || 0)} de Ember. Os heróis trazem os restos dos covis.`);
      } else if (def.id === 'bakery') {
        say(`Padaria, nível ${producerLevel(building) + 1}: assa ${bakeryActiveRecipe(building).name.toLowerCase()}. Troque a fornada ou melhore no painel.`);
      } else if (def.heroClass || def.id === 'farm' || def.id === 'mill') {
        if (def.id === 'mill') say(`Moinho, nível ${producerLevel(building) + 1}: mói ${rateNumber(millRate(building))} de trigo em farinha por segundo. Dá pra melhorar no painel.`);
        else say(def.id === 'farm' ? `Fazenda de ${farmType(building.produce).name}, nível ${producerLevel(building) + 1}: troque a produção ou melhore no painel.` : `${def.name}: atualize ou especialize no painel.`, def.id === 'farm' ? `A fazenda está produzindo ${farmType(building.produce).name.toLowerCase()}.` : `${def.name} está aberta. A localização afeta quem a procura.`);
      } else say(def.id === 'tavern' ? 'Taverna: o imposto vem de quem passa o tempo livre bebendo aqui.' : def.grain ? `${def.name}: produz provisões.` : `${def.name}.`);
    }
  });
  $('#lair-layer').addEventListener('click', event => {
    if (state.selectedBuild || state.movingSite != null || state.placingBounty) return;
    const marker = event.target.closest('.lair-marker:not(.remains-pile)');
    if (!marker) return;
    const lair = (state.lairs || []).find(item => item.id === marker.dataset.lair && item.active);
    if (!lair) return;
    state.selectedBuild = null;
    state.selectedSite = null;
    state.demolishArmed = null;
    selectedLairId = lair.id;
    renderSites();
    renderPanel();
    $('#site-inspector').scrollTop = 0;
  });
  $$('.tab').forEach(tab => tab.addEventListener('click', () => {
    state.tab = tab.dataset.tab;
    // Construir sempre abre a lista completa, mesmo com uma construção selecionada.
    if (state.tab === 'build' && (state.selectedSite != null || selectedLairId != null)) { if (state.movingSite != null) cancelMove(); state.selectedSite = null; selectedLairId = null; renderLairs(); renderSites(); }
    $$('.tab').forEach(other => { const active = other === tab; other.classList.toggle('active', active); other.setAttribute('aria-selected', String(active)); });
    renderPanel();
  }));
  const setTax = step => {
    state.taxPercent = Math.max(0, Math.min(TAX_MAX, state.taxPercent + step));
    updateHud();
    say(`Imposto em ${state.taxPercent}%. Cobrado à meia-noite.`, state.taxPercent >= 15 ? `Imposto em ${state.taxPercent}%. Os camponeses aprenderam palavras novas.` : `Imposto em ${state.taxPercent}%.`);
  };
  $('#tax-down').addEventListener('click', () => setTax(-5));
  $('#tax-up').addEventListener('click', () => setTax(5));
  renderSites(); renderHeroes(); renderPanel(); updateHud();
  initEditor();
  setInterval(tick, 1000);
  setInterval(() => animateActors(performance.now()), 50);
  initMenus();
}

init();

// Desenvolvimento em localhost: quando o código do jogo muda, salva a partida, recarrega e volta nela sozinho.
(function liveReload() {
  if (!/^(localhost|127\.0\.0\.1)$/.test(location.hostname)) return;
  const RESUME_KEY = 'mordavia.resume';
  try {
    const resume = Number(sessionStorage.getItem(RESUME_KEY));
    sessionStorage.removeItem(RESUME_KEY);
    if (resume && readSave(resume)) loadGame(resume);
  } catch {}
  let seen = null;
  setInterval(async () => {
    try {
      const stamp = await (await fetch('/__version', { cache: 'no-store' })).text();
      if (seen === null) { seen = stamp; return; }
      if (stamp === seen) return;
      if (game.slot) { writeSave(game.slot); sessionStorage.setItem(RESUME_KEY, String(game.slot)); }
      location.reload();
    } catch {}
  }, 1500);
})();
