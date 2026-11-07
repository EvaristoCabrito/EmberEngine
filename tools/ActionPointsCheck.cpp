// ActionPointsCheck: the Action Point rules (world/ActionPoints.h) on real battles, headless. Every check the design asked for
// (docs/ACTION_POINTS.md "Testes"): the base 12, DEX by tens, the armor's bonuses, the weapons' costs, the potion, flat ground at
// 1 AP a hex and configured terrain, the Staff and Spell Counters and their gates, the shared reserve, AP kept across turns, never
// negative, modifiers, armor swaps, unpayable routes, refused actions, the AI under the same rules, and the snapshot.
// Run from the project root (the data files are read relative to it). Exit code 0 = every check passed.

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "world/ActionPoints.h"
#include "world/Battle.h"
#include "world/Combat.h"
#include "world/Spells.h"

using namespace ember;

namespace {

int gFailed = 0, gPassed = 0;

void check(bool ok, const std::string& what) {
  if (ok) ++gPassed;
  else ++gFailed;
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

BattleUnit plain(const std::string& id, const std::string& side, int dex, const std::string& armor = {}) {
  BattleUnit u;
  u.id = id, u.name = id, u.side = side, u.alive = true;
  u.hp = u.maxHp = 30, u.atk = 10, u.def = 5, u.dex = dex, u.mov = 4, u.level = 5;
  u.atkBlows = true;
  u.armorCategory = armor;
  return u;
}

const char* kBattle = "content/data/battles/bosque005.battle.json";

// A plains hex of the board nobody stands on, nearest to (x, y).
BattleHex freePlains(const Battle& b, int x, int y, const std::vector<BattleHex>& taken = {}) {
  BattleHex best{x, y};
  int bestD = 1 << 20;
  for (int r = 0; r < b.rows(); ++r)
    for (int c = 0; c < b.cols(); ++c) {
      if (b.cell(c, r).terrain != "plains" || b.cell(c, r).blocked || b.unitAt(c, r) >= 0) continue;
      if (std::find(taken.begin(), taken.end(), BattleHex{c, r}) != taken.end()) continue;
      const int d = Battle::distance({c, r}, {x, y});
      if (d < bestD) bestD = d, best = {c, r};
    }
  return best;
}

}  // namespace

int main() {
  const ApRules& r = apRules();
  std::printf("action points check: rules %s (base %d, DEX/%d)\n", r.loaded ? "from content/data/action-points.json" : "DEFAULTS (file missing)", r.baseAp, r.dexPerAp);
  Battle bare;

  // --- 1..7: the maximum ---
  std::printf("maximum\n");
  check(apMax(bare, plain("a", "player", 0, "heavyPlate")) == 12, "base AP is 12 before bonuses (DEX 0, heavy plate +0)");
  check(apMax(bare, plain("a", "player", 9, "heavyPlate")) == 12, "DEX 9 grants no AP");
  check(apMax(bare, plain("a", "player", 10, "heavyPlate")) == 13, "DEX 10 grants +1 AP");
  check(apMax(bare, plain("a", "player", 19, "heavyPlate")) == 13, "DEX 19 grants +1 AP");
  check(apMax(bare, plain("a", "player", 20, "heavyPlate")) == 14, "DEX 20 grants +2 AP");
  check(apMax(bare, plain("a", "player", 100, "heavyPlate")) == 22, "DEX 100 grants +10 AP");
  check(apMax(bare, plain("a", "player", 0, "unarmored")) == 17, "unarmored grants +5 AP");
  {
    BattleUnit big = plain("a", "player", 100, "unarmored");
    big.apMods.push_back({ApModifierSource::Talent, ApModifier::Scope::Max, "t", "", 20, 0, -1});
    check(apMax(bare, big) == 34, "the maximum never goes over the scale's ceiling of 34");
  }
  check(apMax(bare, plain("a", "player", 0, "")) == 17, "no chest piece counts as unarmored (+5)");
  check(apMax(bare, plain("a", "player", 0, "heavyPlate")) == 12, "heavy plate grants +0 AP and keeps the base 12");
  check(apMax(bare, plain("a", "player", 0, "chainmail")) == 13 && apMax(bare, plain("a", "player", 0, "cloth")) == 16, "the provisional categories come from the rules (chainmail +1, cloth +4)");
  check(apArmorCategoryOfItem("chainmail-hauberk") == "chainmail" && apArmorCategoryOfItem("manto-do-sol-radiante") == "cloth" && apArmorCategoryOfItem("") == "unarmored",
        "chest pieces map to categories by their equipment id");

  // --- 8..11: the costs ---
  std::printf("costs\n");
  auto armed = [&](const char* type) {
    BattleUnit u = plain("w", "player", 0);
    u.weaponType = type;
    return u;
  };
  check(apCost(bare, armed("sword"), ApAction::attack()) == 6, "sword attack costs 6");
  check(apCost(bare, armed("axe"), ApAction::attack()) == 6, "axe attack costs 6");
  check(apCost(bare, armed("mace"), ApAction::attack()) == 6 && apCost(bare, armed("hammer"), ApAction::attack()) == 6, "blunt (mace, hammer) attack costs 6");
  check(apCost(bare, armed("dagger"), ApAction::attack()) == 5, "dagger attack costs 5");
  check(apCost(bare, armed("spear"), ApAction::attack()) == 7, "polearm (spear) attack costs 7");
  check(apCost(bare, armed("bow"), ApAction::attack()) == 7 && apCost(bare, armed("crossbow"), ApAction::attack()) == 7, "bow / crossbow attack costs 7");
  check(apCost(bare, armed("sword"), ApAction::potion()) == 5, "a potion costs 5");
  check(apCost(bare, armed("sword"), ApAction::cast("magicMissile")) == 8, "a spell costs 8");
  check(apCost(bare, armed("sword"), ApAction::cast("doubleStrike")) == 8, "a special skill (Corte Duplo, a weapon skill) costs 8");
  check(apCost(bare, armed("staff"), ApAction::counter("staff")) == 6, "a Staff Counter costs 6");
  check(apCost(bare, armed("staff"), ApAction::counter("spell")) == 7, "a Spell Counter costs 7");
  check(apCost(bare, armed("sword"), ApAction::examine()) == r.examine.cost && r.examine.cost > 0, "Examinar costs its configured AP (" + std::to_string(r.examine.cost) + ")");
  {
    BattleUnit e = plain("e", "enemy", 0);
    e.classId = "miliciaV2";
    check(apWeaponCategory(e) == "sword" && apCost(bare, e, ApAction::attack()) == 6, "an enemy's weapon comes from the rules' class table (milícia: sword, 6)");
    e.classId = "basePikeman";
    check(apCost(bare, e, ApAction::attack()) == 7, "a pikeman strikes as a polearm (7)");
  }

  // --- 12..13: movement on the real board ---
  std::printf("movement (%s)\n", kBattle);
  std::optional<Battle> battle = Battle::load(kBattle);
  if (!battle) {
    std::printf("  [FAIL] cannot load %s\n", kBattle);
    return 1;
  }
  {
    // every unit's reach: at the rules' defaults the AP of a walk is its movement cost (1 on flat ground), hex by hex; and the reach
    // Ember exported (the check's reference) is still ours, the AP never cutting it short at full reserve
    bool apIsCost = true, refOk = true;
    size_t cells = 0;
    for (size_t i = 0; i < battle->units().size(); ++i) {
      const BattleUnit& u = battle->units()[i];
      if (!u.alive) continue;
      const std::vector<ReachCell> reach = battle->reachable(i);
      for (const ReachCell& c : reach) {
        ++cells;
        if (c.ap != c.cost) apIsCost = false;
      }
      if (!u.reference.empty() && u.reference.size() != reach.size()) refOk = false;
    }
    check(apIsCost && cells > 0, "flat terrain costs 1 AP per hex (the AP of every reachable hex equals its Ember movement cost: " + std::to_string(cells) + " hexes)");
    check(refOk, "with a full reserve every unit reaches exactly what Ember's reference reach lists");
    int woods = -1, plainsAp = -1;
    for (int y = 0; y < battle->rows() && (woods < 0 || plainsAp < 0); ++y)
      for (int x = 0; x < battle->cols(); ++x) {
        const BattleCell& c = battle->cell(x, y);
        if (c.terrain == "woods" && woods < 0) woods = apStepCost(*battle, battle->units()[0], x, y, c.moveCost);
        if (c.terrain == "plains" && plainsAp < 0) plainsAp = apStepCost(*battle, battle->units()[0], x, y, c.moveCost);
      }
    check(plainsAp == 1, "a plains hex costs 1 AP");
    check(woods == 2, "difficult terrain (woods) uses its configured cost (the tile's own: 2)");
    // a configured modifier on a terrain: +1 AP on woods for this unit only
    BattleUnit mod = battle->units()[0];
    mod.apMods.push_back({ApModifierSource::StatusEffect, ApModifier::Scope::Movement, "test", "woods", 1, 0, -1});
    int woodsMod = -1;
    for (int y = 0; y < battle->rows() && woodsMod < 0; ++y)
      for (int x = 0; x < battle->cols() && woodsMod < 0; ++x)
        if (battle->cell(x, y).terrain == "woods") woodsMod = apStepCost(*battle, mod, x, y, battle->cell(x, y).moveCost);
    check(woodsMod == 3, "a movement modifier on a terrain changes its AP (woods 2 -> 3)");
  }

  // --- a hero and a foe placed beside each other on plains ---
  size_t hero = 0, foe = 0;
  for (size_t i = 0; i < battle->units().size(); ++i) {
    const BattleUnit& u = battle->units()[i];
    if (u.side == "player" && u.alive && u.weaponType == "sword") hero = i;
  }
  for (size_t i = 0; i < battle->units().size(); ++i)
    if (battle->units()[i].side == "enemy" && battle->units()[i].alive && battle->units()[i].size == 1 && !battle->units()[i].footprintOffsets) {
      foe = i;
      break;
    }
  {
    BattleUnit &h = battle->units()[hero], &f = battle->units()[foe];
    const BattleHex hh = freePlains(*battle, battle->cols() / 2, battle->rows() / 2);
    h.x = hh.x, h.y = hh.y;
    BattleHex fh{};
    for (const BattleHex& n : Battle::neighbors(hh.x, hh.y))
      if (battle->inBounds(n.x, n.y) && battle->cell(n.x, n.y).terrain == "plains" && battle->unitAt(n.x, n.y) < 0) {
        fh = n;
        break;
      }
    f.x = fh.x, f.y = fh.y;
    f.weaponType.clear(), f.classId = "miliciaV2";
    h.ap = apMax(*battle, h), f.ap = apMax(*battle, f);
    std::printf("stage: %s (%s, AP %d) at %d,%d beside %s (AP %d) at %d,%d\n", h.name.c_str(), h.weaponType.c_str(), h.ap, h.x, h.y, f.name.c_str(), f.ap, f.x, f.y);
  }

  // --- 14..17: counters ---
  std::printf("counters\n");
  {
    Battle b = *battle;
    BattleUnit &h = b.units()[hero], &f = b.units()[foe];
    BattleStep blow;
    blow.type = BattleStep::Type::Combat, blow.att = hero, blow.def = foe;
    f.ap = apMax(b, f);
    const int fCounter = apCounterCost(b, f, h, {h.x, h.y});
    check(fCounter == 6, "the milícia's sword counter costs its weapon's 6 (got " + std::to_string(fCounter) + ")");
    check(strikesBack(b, blow), "a defender with AP strikes back");
    f.ap = fCounter - 1;
    check(!strikesBack(b, blow), "a defender one AP short never counters");
    check(!forecast(b, hero, {h.x, h.y}, foe).canCounter, "the forecast shows no counter for a defender short of AP");
    f.ap = fCounter;
    check(strikesBack(b, blow) && forecast(b, hero, {h.x, h.y}, foe).canCounter, "a defender with exactly the counter's AP counters");
    // the Staff Counter: a staff wielder beside its attacker
    BattleUnit staffer = h;
    staffer.weaponType = "staff", staffer.combat.staff = true, staffer.combat.arcane = false, staffer.combat.arrow = false;
    check(apCounterType(b, staffer, f, {f.x, f.y}) == "staff" && apCounterCost(b, staffer, f, {f.x, f.y}) == 6, "a staff wielder beside its attacker makes a Staff Counter (6)");
    BattleHex far = freePlains(b, f.x + 2, f.y, {{h.x, h.y}, {f.x, f.y}});
    BattleUnit caster = staffer;
    caster.combat.arcane = true;
    check(apCounterType(b, caster, f, far) == "spell" && apCounterCost(b, caster, f, far) == 7, "an arcane caster struck from afar makes a Spell Counter (7)");
    staffer.ap = 5;
    BattleStep onStaffer;
    onStaffer.type = BattleStep::Type::Combat, onStaffer.att = foe, onStaffer.def = hero;
    b.units()[hero] = staffer;
    check(!strikesBack(b, onStaffer), "5 AP: no Staff Counter (needs 6)");
    b.units()[hero].ap = 6;
    const bool staffCounters = strikesBack(b, onStaffer);
    check(staffCounters, "6 AP: the Staff Counter is allowed");
    // the counter's commit takes the AP from the one reserve, once
    Mulberry32 rng(11);
    std::vector<BattleEffect> fx;
    b.units()[hero].ap = 9;
    resolveStrike(b, onStaffer, true, rng, fx);
    check(b.units()[hero].ap == 3, "the counter deducts its 6 AP from the shared reserve (9 -> " + std::to_string(b.units()[hero].ap) + ")");
    b.units()[hero].ap = 5;
    fx.clear();
    resolveStrike(b, onStaffer, true, rng, fx);
    check(b.units()[hero].ap == 5 && fx.empty(), "an unaffordable counter never executes and spends nothing");
  }

  // --- 5, 18, 19: the shared reserve across a turn, never negative ---
  std::printf("reserve\n");
  {
    Battle b = *battle;
    BattleUnit& h = b.units()[hero];
    Mulberry32 rng(3);
    b.rollInitiative(rng);
    h.ap = 17;
    BattleStep blow;
    blow.type = BattleStep::Type::Combat, blow.att = hero, blow.def = foe;
    check(apStepsCost(b, hero, {blow}) == 6, "a sword attack's steps cost 6");
    check(apCommitSteps(b, hero, {blow}) && h.ap == 11, "17 - 6 = 11 after the sword attack");
    // three flat hexes
    const std::vector<ReachCell> reach = b.reachable(hero);
    const ReachCell* three = nullptr;
    for (const ReachCell& c : reach)
      if (c.ap == 3 && c.cost == 3) three = &c;
    check(three != nullptr, "a hex three flat steps away is in reach");
    if (three) check(b.move(hero, three->x, three->y) && h.ap == 8, "11 - 3 = 8 after three flat hexes");
    const int before = h.ap;
    check(!apTrySpend(h, before + 1) && h.ap == before, "AP cannot go negative: a cost above the reserve is refused whole");
    check(apTrySpend(h, before) && h.ap == 0, "the whole reserve can be spent, to exactly 0");
    // what is left survives the end of the turn and the other turns; the own turn refreshes once
    h.ap = 4;
    h.moved = true;
    b.startNewRound();
    check(h.ap == 4, "unspent AP survives the end of the turn and the new round until the unit's own turn");
    b.beginTurn(hero);
    const int mx = apMax(b, h);
    check(h.ap == mx, "the own turn's start refreshes AP to the maximum (replace)");
    h.ap -= 5;
    b.beginTurn(hero);
    check(h.ap == mx - 5, "a turn begun twice in the round (a fight resumed) refreshes once");
  }

  // --- 20: modifiers ---
  std::printf("modifiers\n");
  {
    Battle b = *battle;
    BattleUnit u = plain("m", "player", 0, "heavyPlate");
    u.weaponType = "sword";
    u.apMods.push_back({ApModifierSource::Talent, ApModifier::Scope::Max, "talent", "", 3, 0, -1});
    check(apMax(b, u) == 15, "a flat +3 maximum modifier: 12 -> 15");
    u.apMods.push_back({ApModifierSource::Debuff, ApModifier::Scope::Max, "curse", "", 0, -20, 2});
    check(apMax(b, u) == 12, "a -20% maximum modifier after the flat ones: 15 -> 12 (floor)");
    u.apMods.push_back({ApModifierSource::Equipment, ApModifier::Scope::Action, "ring", "sword", -2, 0, -1});
    check(apCost(b, u, ApAction::attack()) == 4, "an action discount on swords: 6 -> 4");
    check(apCost(b, u, ApAction::potion()) == 5, "... and the potion keeps its 5");
    u.apMods.push_back({ApModifierSource::StatusEffect, ApModifier::Scope::Action, "slow", "", 0, 50, -1});
    check(apCost(b, u, ApAction::attack()) == 6 && apCost(b, u, ApAction::potion()) == 7, "a +50% surcharge on every action: sword 4 -> 6, potion 5 -> 7");
    // the curse expires after its two own turns
    b.units().push_back(u);
    const size_t mi = b.units().size() - 1;
    b.beginTurn(mi);
    check(b.units()[mi].apMods.size() == 4, "an expiring modifier counts down at the own turn (1 left)");
    b.startNewRound();
    b.beginTurn(mi);
    check(b.units()[mi].apMods.size() == 3 && apMax(b, b.units()[mi]) == 15, "... and is gone after its turns: the maximum is 15 again");
  }

  // --- 21: armor swaps create no AP ---
  std::printf("equipment\n");
  {
    BattleUnit u = plain("s", "player", 0, "heavyPlate");
    u.ap = 3;
    int old = apMax(bare, u);
    u.armorCategory = "unarmored";
    apOnMaxChanged(u, old, apMax(bare, u));
    check(u.ap == 8 && apMax(bare, u) == 17, "plate -> unarmored in the fight: 3/12 becomes 8/17");
    old = apMax(bare, u);
    u.armorCategory = "heavyPlate";
    apOnMaxChanged(u, old, apMax(bare, u));
    check(u.ap == 3, "... and back to plate: 3/12 again (nothing gained)");
    u.ap = 12;
    u.armorCategory = "unarmored";
    apOnMaxChanged(u, 12, 17);
    u.armorCategory = "heavyPlate";
    apOnMaxChanged(u, 17, 12);
    check(u.ap == 12, "a full reserve swapped back and forth stays full, never over the maximum");
    u.ap = 1;
    apOnMaxChanged(u, 12, 7);
    check(u.ap == 0, "a maximum lowered below the reserve clamps at 0, never negative");
  }

  // --- 22, 23: unpayable routes and refused actions spend nothing ---
  std::printf("refusals\n");
  {
    Battle b = *battle;
    BattleUnit& h = b.units()[hero];
    h.ap = 2;
    const std::vector<ReachCell> reach = b.reachable(hero);
    check(std::all_of(reach.begin(), reach.end(), [](const ReachCell& c) { return c.ap <= 2; }), "with 2 AP the reach holds only hexes two AP away or less");
    BattleHex farHex = freePlains(b, h.x + 3, h.y + 3, {{h.x, h.y}});
    if (Battle::distance({h.x, h.y}, farHex) < 3) farHex = freePlains(b, h.x - 3, h.y - 3, {{h.x, h.y}});
    const bool moved = b.move(hero, farHex.x, farHex.y);
    check(!moved && h.ap == 2, "a hex beyond the AP is not walked and nothing is spent");
    std::vector<BattleHex> route{{h.x, h.y}};
    BattleHex cur{h.x, h.y};
    for (int i = 0; i < 3; ++i) {
      bool stepped = false;
      for (const BattleHex& n : Battle::neighbors(cur.x, cur.y))
        if (b.inBounds(n.x, n.y) && b.cell(n.x, n.y).passable && b.unitAt(n.x, n.y) < 0 && std::find(route.begin(), route.end(), n) == route.end()) {
          route.push_back(n), cur = n, stepped = true;
          break;
        }
      if (!stepped) break;
    }
    check(route.size() == 4 && !b.legalVoluntaryPath(hero, route), "a three-hex route is illegal with 2 AP");
    h.ap = 10;
    check(route.size() == 4 && b.legalVoluntaryPath(hero, route), "... and legal with 10");
    // a potion without AP: refused, the bag and the AP untouched
    h.ap = 4, h.hp = h.maxHp / 2, h.bag[0] = 2;
    std::string why;
    const std::optional<BattleStep> potion = usePotion(b, hero, "weak", {h.x, h.y}, &why);
    check(!potion && h.bag[0] == 2 && h.ap == 4, "a potion with 4 AP is refused (" + why + "), nothing spent");
    h.ap = 5;
    check(usePotion(b, hero, "weak", {h.x, h.y}, &why).has_value(), "... and allowed with 5");
    // an attack's commit without AP
    BattleStep blow;
    blow.type = BattleStep::Type::Combat, blow.att = hero, blow.def = foe;
    h.ap = 5;
    check(!apCommitSteps(b, hero, {blow}) && h.ap == 5, "a sword attack with 5 AP is refused at its commit, nothing spent");
    // a spell without AP (castSpell's own gate, before any use is spent)
    const int uses = h.spells[0];
    h.ap = 0;
    const std::optional<std::vector<BattleStep>> cast = castSpell(b, hero, h.spellList.empty() ? std::string("doubleStrike") : h.spellList.front(), {{b.units()[foe].x, b.units()[foe].y}}, &why);
    check(!cast && h.spells[0] == uses && h.ap == 0, "a cast with 0 AP is refused before its use is spent (" + why + ")");
  }

  // --- 24: the AI under the same rules ---
  std::printf("ai\n");
  {
    Battle b = *battle;
    BattleUnit& f = b.units()[foe];
    for (BattleUnit& u : b.units())
      if (u.side == "enemy" && &u != &f) u.alive = false;  // only our milícia acts
    f.ap = 0;
    AiPlan starved = b.planEnemy(foe);
    check(starved.target < 0, "an enemy with 0 AP plans no strike on the hero beside it");
    f.ap = apCost(b, f, ApAction::attack());
    AiPlan exact = b.planEnemy(foe);
    check(exact.target == static_cast<int>(hero) && exact.path.empty(), "an enemy with exactly its attack's AP strikes from where it stands, walking nowhere");
    f.ap = 1;
    AiPlan one = b.planEnemy(foe);
    check(one.target < 0 && one.path.size() <= 2, "an enemy with 1 AP strikes nobody and walks at most one flat hex");
    check(!planEnemyCast(b, foe).has_value() || f.ap >= apRules().spells.defaultCost, "an enemy short of AP casts nothing");
  }

  // --- examine ---
  std::printf("examine\n");
  {
    Battle b = *battle;
    const ApExamine e = apExamine(b, hero, foe);
    check(!e.categoryId.empty() && e.attackCost == 6, "Examinar names a category (" + e.categoryId + ", ratio " + std::to_string(e.ratio) + ") and the foe's attack cost");
    BattleUnit& h = b.units()[hero];
    h.ap = apRules().examine.cost;
    check(apTrySpend(h, apCost(b, h, ApAction::examine())) && h.ap == 0, "Examinar spends its configured AP");
    check(!apTrySpend(h, apCost(b, h, ApAction::examine())), "... and is refused without them");
  }

  // --- 25: the snapshot keeps the reserve, the refresh mark, the modifiers and the examined foes ---
  std::printf("snapshot\n");
  {
    Battle a = *battle;
    Mulberry32 rng(7);
    a.rollInitiative(rng);
    BattleUnit& h = a.units()[hero];
    a.beginTurn(hero);
    h.ap = 4, h.armorCategory = "chainmail", h.wornItems = {"chainmail-hauberk"};
    h.apMods.push_back({ApModifierSource::Buff, ApModifier::Scope::Action, "rally", "attack", -1, 0, 2});
    a.markExamined(a.units()[foe].id);
    const nlohmann::json s1 = a.snapshot();
    std::optional<Battle> c = Battle::load(kBattle);
    Mulberry32 other(99);
    c->rollInitiative(other);
    const bool applied = c->applySnapshot(s1);
    const nlohmann::json s2 = c->snapshot();
    const BattleUnit& hc = c->units()[hero];
    check(applied && s1 == s2, "snapshot -> apply -> snapshot gives the same state back");
    check(hc.ap == 4 && hc.apRefreshRound == h.apRefreshRound && hc.armorCategory == "chainmail" && hc.apMods.size() == 1 && c->examined(a.units()[foe].id),
          "the reserve (4), the refresh round, the armor, the modifier and the examined foe come back");
    c->beginTurn(hero);
    check(hc.ap == 4, "resuming mid-turn does not refresh the reserve again");
    // a snapshot without AP (saved before): the maximum once, the round's refresh taken as done
    nlohmann::json old = s1;
    for (nlohmann::json& su : old["units"]) su.erase("ap"), su.erase("apRefreshRound"), su.erase("apMods"), su.erase("armorCategory"), su.erase("wornItems");
    std::optional<Battle> d = Battle::load(kBattle);
    d->rollInitiative(other);
    check(d->applySnapshot(old) && d->units()[hero].ap == apMax(*d, d->units()[hero]) && d->units()[hero].apRefreshRound == d->round(),
          "an older snapshot without AP resumes with the full maximum, once");
  }

  std::printf("action points check: %d passed, %d failed\n", gPassed, gFailed);
  return gFailed ? 1 : 0;
}
