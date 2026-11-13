const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const input = fs.readFileSync(process.argv[2], 'utf8').split('\ninit();')[0];
const sandbox = { window: {}, Audio: class { addEventListener() {} }, assert, console };
const cases = `
  state = { heroes: [], buildings: [], gold: 2000, timber: 100 };
  say = renderHeroes = updateHud = gainHeirXp = () => {};
  let pending;
  startUpgrade = (guild, upgrade) => { guild.underConstruction = true; pending = upgrade; };

  for (const [id, expected] of Object.entries(GUILD_FORMS)) {
    const def = BUILDINGS.find(item => item.id === id);
    const guild = { type: id, site: id, x: 5, y: 5 };
    state.buildings.push(guild);
    const resident = recruit(def.heroClass, guild, null, true);
    assert.equal(resident.guildForm, expected[0][0]);
    assert.ok(guildBoxHTML(guild).includes(expected[1][0]));
    for (let tier = 1; tier < expected.length; tier++) {
      const before = state.gold;
      pending = null;
      upgradeGuild(guild.site, 'inválido');
      assert.equal(pending, null);
      assert.equal(state.gold, before);
      upgradeGuild(guild.site, expected[tier].at(-1));
      assert.ok(pending);
      assert.equal(state.gold, before - GUILD_UPGRADES[tier - 1].gold);
      assert.equal(resident.guildForm, expected[tier - 1].at(-1));
      guild.underConstruction = false;
      finishUpgrade(guild, pending);
      assert.equal(resident.guildForm, expected[tier].at(-1));
      assert.equal(guildTier(guild), tier);
      assert.equal(state.heroes.filter(hero => hero.guildSite === guild.site).length, 1);
      assert.ok(guildHeroArt(resident));
      assert.ok(statsOf(resident).hp > 0);
    }
    assert.ok(!guildBoxHTML(guild).includes('data-guild-upgrade'));
    const gold = state.gold;
    upgradeGuild(guild.site, expected.at(-1)[0]);
    assert.equal(state.gold, gold);
    resident.alive = false;
    assert.equal(recruit(def.heroClass, guild, null, true).guildForm, expected.at(-1).at(-1));
  }
  const swordGuild = { type: 'vanguard', site: 'sword', x: 3, y: 2 };
  state.buildings.push(swordGuild);
  const sword = recruit('Vanguard', swordGuild, null, true);
  upgradeGuild(swordGuild.site, 'Swordsmaster');
  finishUpgrade(swordGuild, pending);
  assert.equal(sword.guildForm, 'Swordsmaster');
  assert.ok(!slotsFor(sword).some(slot => slot.slot === 'offhand'));
  console.log('Guildas, custos, promoção dos moradores, os dois caminhos e reposição validados.');
`;
vm.runInNewContext(input + '\n' + cases, sandbox);





