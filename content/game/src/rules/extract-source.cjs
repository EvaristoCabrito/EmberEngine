// Development provenance tool: extracts Ember's authored pure cast/target logic.
const fs = require('fs');
const ts = require('typescript');
const source = fs.readFileSync('C:/emberashes03D-main/src/game/engine.ts','utf8');
const sf = ts.createSourceFile('engine.ts',source,ts.ScriptTarget.Latest,true);
const cls = sf.statements.find(s=>ts.isClassDeclaration(s)&&s.name.text==='BattleEngine');
const methods = new Map(cls.members.filter(ts.isMethodDeclaration).map(m=>[m.name.getText(sf),m]));
const selected = new Set([...methods.keys()].filter(n=>n.startsWith('cast')&&!['castVisual'].includes(n)));
for(const n of methods.keys())if(n.startsWith('tryAi'))selected.add(n);
['plantSwipeTiles','offHandStrike','isArrowAttack','isArcaneCaster','gainAdjacentAffinity'].forEach(n=>{if(methods.has(n))selected.add(n)});
['nudgeOffHazard','nudgeOffWaypoint','uniqueNeutralNpcSpawns','rollOpeningInitiative','sortByInitiative','highestEnemyLevel'].forEach(n=>{if(methods.has(n))selected.add(n)});
['confirmSweep','startAuraOfProtection','startIntimidatingPresence','startBless','startCreateFoodAndWater','spellAimValid','queueFrost','frostTiles','turnUndeadTiles','tierRemaining','spendTier','longMax','isHeal','isWebCell','webCellSleepChance','zoneDamageMul','isRestrained','movementBudget','targetable','validHealTarget','validCureDiseaseTarget','magicMissileTargetError','wrathRay','piercingRay','piercingThrustRay','bullRushCharge','bullRushPush','bullRushPushDistance','bullRushPushFits','chargeTerrainBlocked','hasFamiliarOut','familiarSpellRemaining','spendFamiliarOrTier','sweepTiles','spellAimError','knockBack','provokeArea','queueMinorVenom','cleaveTiles','shoulderSmashTiles','burningHandsTiles','tendrilTiles'].forEach(n=>{if(methods.has(n))selected.add(n)});
['spellDamage','maybeInflictDisease','gainExp','gainCounterExp','grantExp','addExp','levelUpUnit','trainWeapon','trainElementUse','trainHealing','trainResistance','healingPower','trainingGainForLevel','logSkillGain','adjacentAllies','affinityUnit','adjustAffinity','adjustHeroAffinity','reapplyGear','applyBleedingActionDamage','startOfTurnEffects','applyTileHazard','runAiFor','tryPlantMagicMissile','tryPlantMinorVenom','tryPlantCausticVenom','tryPlantPoisonBreath','tryPlantTendrilSwipe','enmityTarget','gainSupportAffinity','curePlayerDisease','effectiveUnitForReach','smashBarricades','wakeIfSeesParty','playerDistanceFields'].forEach(n=>{if(methods.has(n))selected.add(n)});
const stripped = new Set(['emitParticle','emitBladeFx','emitHolyFx','emitPortalFx','emitMissileFx','emitFireballBurstFx','queueElementalFx','pushLog','adjustHover','recomputeSelection','spawnLevelUp','emitLevelUpFx']);
const transformation = context => root => {
  function visit(node){
    if(ts.isExpressionStatement(node)) {
      const exp=node.expression;
      if(ts.isCallExpression(exp)) {
        const call=exp.expression.getText(sf);
        if(call.startsWith('sfxPlay.')||[...stripped].some(n=>call==='this.'+n))return ts.factory.createBlock([],false);
      }
    }
    return ts.visitEachChild(node,visit,context);
  }
  return ts.visitNode(root,visit);
};
const printer=ts.createPrinter({newLine:ts.NewLineKind.LineFeed});
const transformed=[...selected].map(n=>printer.printNode(ts.EmitHint.Unspecified,ts.transform(methods.get(n),[transformation]).transformed[0],sf).replace(/\bprivate /g,'protected '));
let body=transformed.join('\n\n').replace(/import\("\.\/types"\)/g,'import("../ember/types.ts")').replace(/BattleEngine\./g,'SpellRules.').replace(/CombatAnim/g,'CombatImpact').replace(/(computeReachable\([^\n]+this\.decorOverlay)\)/g,'$1, this.elevations)');
const stepSpell=methods.get('stepSpell');
let impactBlock;function findImpact(node){if(ts.isIfStatement(node)&&node.expression.getText(sf).startsWith('!a.hit &&'))impactBlock=node.thenStatement;ts.forEachChild(node,findImpact)}findImpact(stepSpell);
const statements=impactBlock.statements.filter(st=>!st.getText(sf).includes('this.turnUndeadFx')&&!st.getText(sf).includes('syncBurningHandsVfx')&&!st.getText(sf).includes('SPELL_ELEMENT_FX')&&!st.getText(sf).includes('a.cleaveVfxQueued')&&!st.getText(sf).includes('a.varreduraVfxQueued')&&!st.getText(sf).includes('this.reducedMotion'));
const impact=ts.factory.updateBlock(impactBlock,statements);
body+='\nprotected resolveSpellSource(a: SpellImpact, att: Unit): void '+printer.printNode(ts.EmitHint.Unspecified,ts.transform(impact,[transformation]).transformed[0],sf)+'\n';
let combatBlock;function findCombat(node){if(ts.isIfStatement(node)&&node.expression.getText(sf).startsWith('a.t >= impactAt &&'))combatBlock=node.thenStatement;ts.forEachChild(node,findCombat)}findCombat(methods.get('stepCombat'));
const combatStatements=combatBlock.statements.filter(st=>!st.getText(sf).includes('this.reducedMotion')&&!st.getText(sf).includes('this.hitstop'));
const combatImpact=ts.factory.updateBlock(combatBlock,combatStatements);
body+='\nprotected resolveCombatSource(a: CombatImpact, actor: Unit, target: Unit): void {const arrowShot=this.isArrowAttack(actor)&&!this.offHandStrike(a);const arcaneBolt=!arrowShot&&this.isArcaneCaster(actor);'+printer.printNode(ts.EmitHint.Unspecified,ts.transform(combatImpact,[transformation]).transformed[0],sf)+'}\n';
body+='\nprotected static readonly KILL_EXP_BONUS_MUL=1.25;\nprotected static readonly FAMILIAR_XP_SHARE=0.1;\n';
body=body.replace(/\brollDamage\(/g,'this.rollDamageAt(').replace(/\brollDamageCustom\(/g,'this.rollDamageCustomAt(');
const top=[];
for(const st of sf.statements)if((ts.isFunctionDeclaration(st)&&['spawnUnit','unitFromSnap','remainingTier','heroSpriteFor','initiativeBonus','takesTurns','attackableByPlayer'].includes(st.name?.text))||(ts.isVariableStatement(st)&&['MAGE_RANGE_BONUS_CLASSES','HERO_SPRITE_BY_NAME','BULL_RUSH_RANGE'].some(n=>st.getText(sf).includes('const '+n))))top.push(st.getText(sf).replace('function spawnUnit','export function spawnUnit').replace('function unitFromSnap','export function unitFromSnap').replace('function initiativeBonus','export function initiativeBonus').replace('function takesTurns','export function takesTurns'));
const roster=sf.statements.find(s=>ts.isInterfaceDeclaration(s)&&s.name.text==='Roster').getText(sf).replace('interface Roster','export interface Roster');
const seq=sf.statements.find(s=>ts.isTypeAliasDeclaration(s)&&s.name.text==='Seq').getText(sf).replace('type Seq','export type Seq');
const code=top.join('\n')+'\n'+roster+'\n'+seq+'\nexport abstract class SpellRules extends RuleContext {\n'+body+'\n}\n';
const ids=new Set();const parsed=ts.createSourceFile('extracted.ts',code,ts.ScriptTarget.Latest,true);function walk(n){if(ts.isIdentifier(n))ids.add(n.text);ts.forEachChild(n,walk)}walk(parsed);
let imports='';
for(const st of sf.statements)if(ts.isImportDeclaration(st)&&st.importClause?.namedBindings&&ts.isNamedImports(st.importClause.namedBindings)){
 const path=st.moduleSpecifier.text;
 if(!['./data','./types','./frost','./dexterity','./weaponSkills','./resistances','./skills','./pathfinding','./enmity','./affinity','./hunger','./hexprops','./poison','./combat','./fog'].includes(path))continue;
 const els=st.importClause.namedBindings.elements.filter(e=>ids.has(e.name.text));if(!els.length)continue;
 const dest=['./data','./types','./resistances','./hunger'].includes(path)?'../ember/'+path.slice(2)+'.ts':path+'.ts';
 imports+='import '+(st.importClause.isTypeOnly?'type ':'')+'{ '+els.map(e=>e.getText(sf)).join(', ')+' } from '+JSON.stringify(dest)+';\n';
}
imports+='import { RuleContext } from "./context.ts";\n';
imports+='import type { SpellImpact } from "./context.ts";\n';
imports+='import type { CombatImpact } from "./context.ts";\n';
imports+='const NOTORIOUS_LEVEL_BONUS: Record<string, number> = '+fs.readFileSync('C:/emberashes03D-main/src/game/quests.ts','utf8').match(/export const NOTORIOUS_LEVEL_BONUS[^=]*=\s*(\{[\s\S]*?\n\});/)[1]+';\n';
fs.writeFileSync('src/rules/spell-source.ts','// Preserved Ember engine.ts spawn/cast/target methods; presentation calls removed.\n'+imports+code);
const app=fs.readFileSync('C:/emberashes03D-main/src/game/GameApp.tsx','utf8');
const appSf=ts.createSourceFile('GameApp.tsx',app,ts.ScriptTarget.Latest,true);
const classSpells=appSf.statements.find(s=>ts.isFunctionDeclaration(s)&&s.name?.text==='classSpells').getText(appSf).replace('function classSpells','export function classSpells');
const prestige=appSf.statements.find(s=>ts.isVariableStatement(s)&&s.getText(appSf).includes('const PRESTIGE_SPELLS')).getText(appSf);
fs.writeFileSync('src/rules/available-spells.ts','// Preserved pure spell availability from Ember GameApp.tsx.\nimport type { ClassId, SpellKind } from "../ember/types.ts";\nimport { rulesClass, tierUses, spellTier, BULL_RUSH_UNLOCK_LEVEL, BLESS, POISON_BREATH, BLOODY_SHOT, PROVOKE, PHANTASMAL_FORCE_UNLOCK_LEVEL, SUMMON_FAMILIAR2_UNLOCK_LEVEL, ICE_STORM } from "../ember/data.ts";\nimport { FROST } from "./frost.ts";\n'+prestige+'\n'+classSpells);
console.log('Preserved methods:',[...selected].join(', '));
