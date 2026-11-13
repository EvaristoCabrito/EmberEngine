from pathlib import Path
import sys
root = Path(sys.argv[1])
p = root / 'app.js'
original = p.read_bytes()
s = original.decode('utf-8').replace('\r\n', '\n')
def replace(old, new):
    global s
    assert s.count(old) == 1, (old, s.count(old))
    s = s.replace(old, new)

replace('const BUILDINGS = [', """const GUILD_FORMS = {
  moonwarden: [['Guerreiro de machado e escudo'], ['Lobisomem de machado e escudo'], ['Grande Lobo']],
  vanguard: [['Vanguard'], ['Swordsmaster', 'WarBringer']],
};

const BUILDINGS = [""")
replace("  { id: 'armory', name:", """  { id: 'moonwarden', name: 'Guilda MoonWarden', glyph: 'M', cost: 185, wood: 11, detail: 'Seu morador evolui de guerreiro a lobisomem e, por fim, a Grande Lobo.', heroClass: 'MoonWarden', gold: 0.3, grain: 0, people: 1 },
  { id: 'vanguard', name: 'Guilda dos Vanguards', glyph: 'V', cost: 185, wood: 11, detail: 'Seu Vanguard escolhe entre Swordsmaster e WarBringer no nível 2.', heroClass: 'Vanguard', gold: 0.3, grain: 0, people: 1 },
  { id: 'armory', name:""")
replace("  blackMage: 'tower', conjurer: 'conjurer', healer: 'healer', armory: 'armory',", "  blackMage: 'tower', conjurer: 'conjurer', healer: 'healer', armory: 'armory', moonwarden: 'guild', vanguard: 'guild',")
replace("'conjurer', 'healer', 'armory', 'tavern', 'hotel'] },", "'conjurer', 'healer', 'moonwarden', 'vanguard', 'armory', 'tavern', 'hotel'] },")
replace('const actorArt = actor => PRESTIGE_ART', """const GUILD_HERO_ART = {
  MoonWarden: 'moonwarden-human',
  'Lobisomem de machado e escudo': 'moonwarden-werewolf',
  'Grande Lobo': 'moonwarden-great-wolf',
  Vanguard: 'vanguard',
  Swordsmaster: 'swordsmaster',
  WarBringer: 'warbringer',
};
const guildHeroArt = actor => GUILD_HERO_ART[actor.guildForm || actor.className];
const actorArt = actor => guildHeroArt(actor) || PRESTIGE_ART""")
replace('const actorPortrait = actor => PRESTIGE_ART', 'const actorPortrait = actor => guildHeroArt(actor) || PRESTIGE_ART')
replace("if (actor.action === 'action' && !actor.prestige)", "if (actor.action === 'action' && (!actor.prestige || guildHeroArt(actor)))")
replace('const flipSprite = actor.kind', 'const flipSprite = actor.kind')
replace("    const nextInfo", "    const nextInfo") if False else None
replace('  const prestigeTargets = PRESTIGE[hero.className];\n  const nextInfo = hero.prestige', "  const nextInfo = guildHeroArt(hero) ? 'A forma acompanha o nível da guilda.' : hero.prestige")
replace("  const guildType = GUILD_FOR[hero.className];\n  return `<article", "  return `<article")
replace('${hero.className}${hero.prestige ? ` · ${hero.prestige}` : \'\'}</small>', '${actorLabel(hero)}</small>')
replace("    lines.push(['Recruta', building.branch || def.heroClass]);", "    lines.push(['Recruta', GUILD_FORMS[def.id] ? guildForm(building) : building.branch || def.heroClass]);")
replace('Nível ${guildTier(building) + 1} de 3', 'Nível ${guildTier(building) + 1} de ${GUILD_FORMS[def.id]?.length || 3}')
replace('  state.heroes.push(hero);\n  gainHeirXp(10);', """  if (guildResident && GUILD_FORMS[origin.type]) hero.guildForm = guildForm(origin);
  if (className === 'MoonWarden') hero.gear.offhand = 'plankShield';
  state.heroes.push(hero);
  gainHeirXp(10);""")
replace('item => item.heroClass && state.buildings.some(built', 'item => item.heroClass && !GUILD_FORMS[item.id] && state.buildings.some(built')
replace("    if (upgrade.tier === 0) {\n      building.tier = 1;", """    if (GUILD_FORMS[building.type]) {
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
      building.tier = 1;""")
replace('function upgradeGuild(site, branch) {', """const guildForm = guild => guild.guildForm || GUILD_FORMS[guild.type][guildTier(guild)][0];
function upgradeGuild(site, branch) {""")
replace("  if (tier === 1 && !PRESTIGE[def.heroClass]?.includes(branch)) return;\n  if (!freeLodging())", """  const forms = GUILD_FORMS[def.id];
  if (forms ? !forms[tier + 1]?.includes(branch) : tier === 1 && !PRESTIGE[def.heroClass]?.includes(branch)) return;
  if (!forms && !freeLodging())""")
replace("  const branchNames = { Bishop: 'Templo Bishop', Cleric: 'Treino Cleric', Elementalist: 'Torre dos Elementalistas', Warlock: 'Torre dos Arcanistas' };\n  const name = guild.branch ? (branchNames[guild.branch] || `Guilda ${guild.branch}`) : def.name;\n", '')
replace("  if (tier === 0) actions = ", """  const forms = GUILD_FORMS[def.id];
  if (forms) {
    const next = forms[tier + 1];
    actions = `<p class="storage-items">Nível ${tier + 1} · ${guildForm(guild)}</p>`;
    if (next) actions += `<div class="guild-branches">${next.map(form =>
      `<button type="button" class="hut-upgrade" data-guild-upgrade="${guild.site}" data-branch="${form}">Nível ${tier + 2} · ${form} · ${costHTML(cost.gold, cost.wood)}</button>`
    ).join('')}</div>`;
  } else if (tier === 0) actions = """)
replace("    if (actor.wait > 0) {\n      actor.wait = Math.max(0, actor.wait - delta);\n      actor.action =", """    if (actor.wait > 0) {
      actor.wait = Math.max(0, actor.wait - delta);
      if (guildHeroArt(actor) && !(actor.actionTime > 0)) { animateIdle(actor, delta, reducedMotion); return; }
      actor.action =""")
replace("    actor.action = 'walk';\n    actor.walkClock = (actor.walkClock || 0) + (moved ? delta : 0);\n    actor.frame = moved && (!reducedMotion || actor.className === 'Peasant') ? Math.floor(actor.walkClock * 7) % 4 : 0;", """    if (!moved && guildHeroArt(actor)) animateIdle(actor, delta, reducedMotion);
    else animateWalk(actor, moved, delta, reducedMotion);""")
replace("const STARTER_WEAPON = { Warrior:", "const STARTER_WEAPON = { MoonWarden: 'warAxe', Vanguard: 'valeGreatsword', Warrior:")
replace("const MELEE = ['Warrior',", "const MELEE = ['MoonWarden', 'Warrior',")
replace("const FRONT = ['Warrior',", "const FRONT = ['MoonWarden', 'Warrior',")
replace("price: 40, classes: ['Warrior'], bonus: { atk: 5 }", "price: 40, classes: ['Warrior', 'MoonWarden'], bonus: { atk: 5 }")
replace("price: 110, classes: ['Warrior'], bonus: { atk: 9 }", "price: 110, classes: ['Warrior', 'Vanguard'], bonus: { atk: 9 }")
replace("base: 'warAxe', classes: ['Warrior']", "base: 'warAxe', classes: ['Warrior', 'MoonWarden']")
replace("const slotsFor = actor => actor.className === 'Peasant' ? PEASANT_SLOTS : HERO_SLOTS;", """const slotsFor = actor => actor.className === 'Peasant' ? PEASANT_SLOTS
  : actor.guildForm === 'Grande Lobo' ? []
  : actor.className === 'Vanguard' ? HERO_SLOTS.filter(({ slot }) => slot !== 'offhand') : HERO_SLOTS;""")
replace("const fitsSlot = (item, actor, slot) => item.slot", "const fitsSlot = (item, actor, slot) => (actor.guildForm !== 'Grande Lobo' && !(actor.className === 'Vanguard' && slot === 'offhand')) && item.slot")
replace("const gearBonus = (actor, key) => Object.values(gearOf(actor)).reduce", "const gearBonus = (actor, key) => actor.guildForm === 'Grande Lobo' ? 0 : Object.values(gearOf(actor)).reduce")
replace("  const item = craftedWeapon(gearOf(actor).weapon);", "  const item = actor.guildForm === 'Grande Lobo' ? null : craftedWeapon(gearOf(actor).weapon);")
replace("  swordsman: statTable(5, [40, 11, 0, 10, 4]),", """  swordsman: statTable(5, [40, 11, 0, 10, 4]),
  moonwardenWerewolf: statTable(5, [49, 14, 0, 12, 5]),
  moonwardenGreatWolf: statTable(6, [58, 22, 0, 14, 8]),
  vanguard: { mov: 5, stats: [
    [34, 9, 5, 6, 3], [38, 11, 6, 8, 4], [42, 13, 7, 10, 5], [46, 15, 8, 12, 6], [50, 17, 9, 14, 7],
    [54, 19, 10, 16, 8], [58, 21, 11, 18, 9], [62, 23, 12, 20, 10], [66, 25, 13, 22, 11], [70, 27, 14, 24, 12],
    [74, 29, 15, 26, 13], [78, 31, 16, 28, 14], [82, 33, 17, 30, 15], [86, 35, 18, 32, 16], [90, 37, 19, 34, 17],
    [94, 39, 20, 36, 18], [98, 41, 21, 38, 19], [102, 43, 22, 40, 20], [106, 45, 23, 42, 21], [110, 47, 24, 44, 22],
    [114, 49, 25, 46, 23], [118, 51, 26, 48, 24], [122, 53, 27, 50, 25], [126, 55, 28, 52, 26], [130, 57, 29, 54, 27],
    [134, 59, 30, 56, 28], [138, 61, 31, 58, 29], [142, 63, 32, 60, 30], [146, 65, 33, 62, 31], [150, 67, 34, 64, 32],
  ] },""")
replace("const CLASS_TABLE = {", """const CLASS_TABLE = {
  MoonWarden: 'swordsman', 'Lobisomem de machado e escudo': 'moonwardenWerewolf', 'Grande Lobo': 'moonwardenGreatWolf',
  Vanguard: 'vanguard', Swordsmaster: 'vanguard', WarBringer: 'vanguard',""")
replace('CLASS_TABLE[actor.prestige] || CLASS_TABLE[actor.className]', 'CLASS_TABLE[actor.guildForm || actor.prestige] || CLASS_TABLE[actor.className]')
replace("${actor.className}${actor.prestige ? ` · ${actor.prestige}` : ''}`;", "${actor.className}${actor.guildForm && actor.guildForm !== actor.className ? ` · ${actor.guildForm}` : actor.prestige ? ` · ${actor.prestige}` : ''}`;")
replace("  if (before < 15 && hero.level >= 15)", "  if (!guildHeroArt(hero) && before < 15 && hero.level >= 15)")
replace("    actor.action = 'walk';\n    actor.frame = 0;\n  }\n}\n\nfunction checkPrestige", """    if (guildHeroArt(actor)) animateIdle(actor, delta, reducedMotion);
    else { actor.action = 'walk'; actor.frame = 0; }
  }
}

function checkPrestige""")
replace("    if (actor.action === 'idle')", "    if (actor.action === 'idle')") if False else None
replace("      ['Guilda, nível 3', GUILD_UPGRADES[1].gold, GUILD_UPGRADES[1].wood, 'especialização e mais um herói'],", """      ['Guilda, nível 3', GUILD_UPGRADES[1].gold, GUILD_UPGRADES[1].wood, 'especialização e mais um herói'],
      ['MoonWarden, nível 2', GUILD_UPGRADES[0].gold, GUILD_UPGRADES[0].wood, 'morador vira lobisomem de machado e escudo'],
      ['MoonWarden, nível 3', GUILD_UPGRADES[1].gold, GUILD_UPGRADES[1].wood, 'morador vira Grande Lobo'],
      ['Vanguards, nível 2', GUILD_UPGRADES[0].gold, GUILD_UPGRADES[0].wood, 'escolha Swordsmaster ou WarBringer para o morador'],""")
replace('os heróis dos níveis 2 e 3 da guilda precisam', 'os recrutas extras das guildas precisam')
replace('sem cama, vão embora.</p>', 'sem cama, vão embora.</p>') if False else None
replace("    <h3>Quem chega</h3>", "    <p>MoonWarden e Vanguards melhoram o próprio morador e usam a mesma vaga da guilda.</p>\n    <h3>Quem chega</h3>")
replace("moonwarden: 'guild', vanguard: 'guild'", "moonwarden: 'guild-moonwarden', vanguard: 'guild-vanguard'")
replace("      const spriteFile = nekoFrame", """      const nativeLeft = /^moonwarden-(human|werewolf)$/.test(art) && token.dataset.side === 'left' && ['walk', 'action'].includes(sheet);
      const guildSprite = guildHeroArt(actor) ? `${art}-${sheet === 'action' || sheet === 'idle' ? sheet : 'walk'}${nativeLeft ? '-left' : ''}-v${sheet === 'idle' ? (art.startsWith('moonwarden-') ? '02' : '03') : (art.startsWith('moonwarden-') ? '01' : '02')}.webp` : null;
      const spriteFile = guildSprite || (nekoFrame""")
replace("${art}-${sheet}-v01.${extension}`;", "${art}-${sheet}-v01.${extension}`);")
replace("const flipSprite = actor.kind === 'nekomata'", "const flipSprite = guildSprite ? !nativeLeft && token.dataset.side === 'left' : actor.kind === 'nekomata'")
replace("      actor.action = 'walk'; actor.frame = 0;", "      actor.action = 'walk'; actor.frame = 0;") if False else None
replace("item && item.classes.includes(actor.className));", "item && fitsSlot(item, actor, 'weapon'));")
replace("!item || !item.classes.includes(actor.className) || !(state.items", "!item || !fitsSlot(item, actor, 'weapon') || !(state.items")
replace("const classOf = actor => actor.prestige || actor.className;", "const classOf = actor => actor.guildForm || actor.prestige || actor.className;")
replace("  actor.frame = reducedMotion ? 0 : Math.floor(actor.idleClock * 2.4) % 4;", "  const idleRate = guildHeroArt(actor) ? 3.2 : 2.4;\n  actor.frame = reducedMotion ? 0 : Math.floor(actor.idleClock * idleRate) % 4;")
replace("  actor.action = 'idle';\n  actor.idleClock = (actor.idleClock || 0) + delta;", "  if (guildHeroArt(actor) && actor.action !== 'idle') actor.idleClock = 0;\n  actor.action = 'idle';\n  actor.idleClock = (actor.idleClock || 0) + delta;")
s = s.replace('\n', '\r\n') if b'\r\n' in original else s
p.write_bytes(s.encode('utf-8'))
html = root / 'index.html'
original = html.read_bytes()
import re
html.write_bytes(re.sub(rb'app\.js\?v=(\d+)', lambda m: b'app.js?v=' + str(int(m[1]) + 1).encode(), original, count=1))



