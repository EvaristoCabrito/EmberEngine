// Engine2's status sheet "Magias e habilidades" rows (src/game/GameApp.tsx StatusPanel), for EmberEngine: per rules class and
// level 1..30, the rows the sheet lists, built with Engine2's own formula functions. A row whose text depends on the unit's
// MAG carries the parameters of damageFormula / spellFormula (`mag`); the window computes the total.
//
//   node C:/Engine2/node_modules/vite-node/vite-node.mjs --root <scratch dir> \
//        tools/scripts/engine2/export_status_rows.ts -- <out.status-rows.json>

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';

const E2 = process.env.ENGINE2 ?? 'C:/Engine2';
const PUBLIC = `${E2}/public`;
globalThis.fetch = (async (url: string) => {
  const text = readFileSync(`${PUBLIC}${url}`, 'utf8');
  return { ok: true, status: 200, json: async () => JSON.parse(text), text: async () => text, headers: new Map([['content-type', 'application/json']]) };
}) as unknown as typeof fetch;

const [outPath] = process.argv.slice(2).filter((a) => a !== '--');
if (!outPath) throw new Error('usage: export_status_rows.ts -- <out.status-rows.json>');

const D = await import(`${E2}/src/game/data.ts`);
const F = await import(`${E2}/src/game/frost.ts`);

// One row. `tier`: the spell's tier (spellTier); `uses`: the row only shows while the tier has uses; `minLevel`: only from
// that level; `lockLevel`: shown but with " · nível N+" and zero uses below it; `mag`: damageFormula (verbose) or
// spellFormula (plain) parameters, appended after `text` with a space.
type Row = { kind: string; text: string; tier: number | null; uses?: boolean; minLevel?: number; lockLevel?: number; mag?: { mul: number; dice: number; faces: number; bonus: number; verbose: boolean } };

const tierOf = (kind: string): number | null => D.spellTier(kind) ?? null;
const plain = (kind: string, text: string, extra: Partial<Row> = {}): Row => ({ kind, text, tier: tierOf(kind), ...extra });
const damage = (kind: string, text: string, p: { mul: number; dice: number; faces: number; bonus: number }, extra: Partial<Row> = {}): Row =>
  ({ kind, text, tier: tierOf(kind), mag: { ...p, verbose: true }, ...extra });
const spell = (kind: string, text: string, p: { mul: number; dice: number; faces: number; bonus: number }, extra: Partial<Row> = {}): Row =>
  ({ kind, text, tier: tierOf(kind), mag: { ...p, verbose: false }, ...extra });

function rowsFor(base: string, level: number, name: string): Row[] {
  const out: Row[] = [];
  if (base === 'swordsman') {
    out.push(plain('doubleStrike', `${D.DOUBLE_STRIKE.name} ${D.doubleStrikeFormula(level)}`));
    out.push(plain('cleave', `${D.CLEAVE.name} ${D.CLEAVE.hexes} hex, ${D.cleaveFormula(level)} · x${D.CLEAVE.largeMul} vs 3+ hex`));
    out.push(plain('bullRush', D.BULL_RUSH.name));
    out.push(plain('shieldBash', D.SHIELD_BASH.name));
    out.push(plain('executionerStrike', D.EXECUTIONER_STRIKE.name));
    if (level >= D.PROVOKE.unlockLevel) out.push(plain('provoke', `${D.PROVOKE.name} ${D.provokeFormula(level)}`));
  }
  if (base === 'mage') {
    if (level >= F.FROST.unlockLevel) out.push(plain('frost', `${F.FROST.name} · ${F.frostPower(level).dice}D${F.FROST.faces} · linha de ${F.frostPower(level).length} hexes`));
    if (level >= D.POISON_BREATH.unlockLevel) {
      const p = D.poisonBreathPower(level);
      out.push(spell('poisonBreath', `${D.POISON_BREATH.name} ·`, { mul: p.mul, dice: p.dice, faces: p.faces, bonus: 0 }, {}));
      out[out.length - 1].text += '';
      (out[out.length - 1] as Row & { after?: string }).after = ` · raio ${p.radius} · Veneno Menor 1D4/turno`;
    }
    const m = D.MAGIC_MISSILE;
    out.push(damage('magicMissile', D.MAGIC_MISSILE.name, { mul: m.mul, dice: m.dice, faces: m.faces, bonus: m.bonus }));
    const l = D.LIGHTNING;
    out.push(damage('lightning', 'Raio', { mul: l.mul, dice: l.dice, faces: l.faces, bonus: l.bonus }, { uses: true }));
    const f = D.FIREBALL;
    out.push(damage('fireball', 'Fogo', { mul: f.mul, dice: f.dice, faces: f.faces, bonus: f.bonus }, { uses: true }));
    const c = D.CAUSTIC_VENOM;
    out.push(damage('causticVenom', c.name, { mul: c.centerMul, dice: c.centerDice, faces: c.centerFaces, bonus: c.centerBonus ?? 0 }, { uses: true }));
  }
  if (base === 'conjurer') {
    out.push(plain('summonFamiliar', D.SUMMON_FAMILIAR.name));
    out.push(spell('phantasmalForce', D.PHANTASMAL_FORCE.name, { mul: 1, ...D.phantasmalForceDice(level), bonus: 0 }, { lockLevel: D.PHANTASMAL_FORCE_UNLOCK_LEVEL }));
    out.push(plain('webOfDreams', D.WEB_OF_DREAMS.name));
    out.push(plain('summonFamiliar2', D.SUMMON_FAMILIAR2.name, { lockLevel: D.SUMMON_FAMILIAR2_UNLOCK_LEVEL }));
    out.push(plain('summonFamiliar4', D.SUMMON_FAMILIAR4.name));
    out.push(plain('summonFamiliar3', D.SUMMON_FAMILIAR3.name));
    out.push(plain('summonZombieDog', D.SUMMON_ZOMBIE_DOG.name));
  }
  if (base === 'archer') {
    out.push(plain('longShot', `${D.LONG_SHOT.name} ${D.longShotFormula(level)}`));
    out.push(plain('piercing', `${D.PIERCING.name} ${D.piercingMul(level)}× dano de arma`));
  }
  if (base === 'healer') {
    out.push(plain('bless', D.BLESS.name));
    out.push(plain('burningHands', D.BURNING_HANDS.name));
    out.push(plain('turnUndead', D.TURN_UNDEAD.name));
    out.push(plain('createFoodAndWater', D.CREATE_FOOD_AND_WATER.name));
    if (name === 'Salazar') out.push(plain('divineBolt', D.DIVINE_BOLT.name));
    const a = D.CURES.cureMinor, b = D.CURES.cureWounds;
    out.push(damage('cureMinor', a.name, { mul: a.mul, dice: a.dice, faces: a.faces, bonus: a.bonus }));
    out.push(damage('cureWounds', b.name, { mul: b.mul, dice: b.dice, faces: b.faces, bonus: b.bonus }));
    out.push(plain('cureDisease', `${D.CURE_DISEASE.name} · remove doença e veneno`, { uses: true }));
  }
  if (base === 'lancer' || base === 'aldric') {
    out.push(plain('piercingThrust', `${D.PIERCING_THRUST.name} dano de arma, −${Math.round(D.PIERCING_THRUST.armorIgnore * 100)}% armadura`));
    out.push(plain('sweep', `${D.SWEEP.name} dano de arma`, { uses: true }));
    out.push(plain('trip', `${D.TRIP.name} arma +${D.diceFormula(1, D.TRIP.bonusFaces, D.TRIP.bonusBonus)}`, { uses: true }));
  }
  if (base === 'paladin') {
    out.push(plain('cureLight', D.CURES.cureLight.name));
    out.push(plain('auraOfProtection', D.AURA_OF_PROTECTION.name));
    out.push(plain('divineWrath', D.DIVINE_WRATH.name));
  }
  if (base === 'heavyKnight') {
    out.push(plain('shoulderSmash', D.SHOULDER_SMASH.name));
    out.push(plain('intimidatingPresence', D.INTIMIDATING_PRESENCE.name));
    out.push(plain('stampede', D.STAMPEDE.name));
  }
  return out;
}

// Salazar's divineBolt is the only name-dependent row: the healer rows are exported twice (the generic healer, Salazar).
const result: Record<string, Record<string, Row[]>> = {};
for (const base of ['swordsman', 'mage', 'conjurer', 'archer', 'healer', 'lancer', 'paladin', 'heavyKnight']) {
  result[base] = {};
  for (let level = 1; level <= 30; level++) result[base][String(level)] = rowsFor(base, level, '');
}
result['healer:Salazar'] = {};
for (let level = 1; level <= 30; level++) result['healer:Salazar'][String(level)] = rowsFor('healer', level, 'Salazar');

mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, JSON.stringify({ statPointsPerLevel: D.STAT_POINTS_PER_LEVEL, rows: result }));
console.log(`status rows: ${Object.keys(result).length} classes x 30 levels -> ${outPath}`);
