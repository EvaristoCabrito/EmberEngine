// VanguardCheck: Kael's classes and techniques (docs/SKILLS_VANGUARD.md) through the game's own rules, no window. A real battle
// (aldeia015) is loaded, Kael is made a Vanguarda the way the battle screen does it (heroBaseClass + setPromotion), then a Mestre da
// Espada or an Arauto da Guerra, and every technique is cast (castSpell) and played (playSteps / resolveStrike) on foes set beside him;
// the guards, the Riposta, Quebrar o Ritmo, the movement reactions, the cooldowns, Corte Baixo's AP, the ranks, the AP costs, the off
// hand lock and the snapshot round trip are checked. Rolls that may miss are tried over many seeds until one lands.
// Run from the project root: build\release\bin\VanguardCheck.exe

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "world/ActionPoints.h"
#include "world/Battle.h"
#include "world/BattleExport.h"
#include "world/Combat.h"
#include "world/Loadout.h"
#include "world/Spells.h"

using namespace ember;

namespace {

int gFailed = 0, gPassed = 0;
void check(bool ok, const std::string& what) {
  ok ? ++gPassed : ++gFailed;
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

const char* kBattle = "content/data/battles/aldeia015.battle.json";

// odd-r neighbours and the hex past `to` on the line from `from` (the same cube math as the rules)
BattleHex beyond(BattleHex from, BattleHex to) {
  auto q = [](BattleHex h) { return h.x - (h.y - (h.y & 1)) / 2; };
  const int fq = q(from), tq = q(to), nq = 2 * tq - fq, nr = 2 * to.y - from.y;
  return {nq + (nr - (nr & 1)) / 2, nr};
}
bool free(const Battle& b, BattleHex h) {
  if (!b.inBounds(h.x, h.y)) return false;
  const BattleCell& c = b.cell(h.x, h.y);
  return c.passable && !c.blocked && c.terrain != "water" && c.terrain != "void" && c.terrain != "barricade" && b.unitAt(h.x, h.y) < 0;
}

struct Scene {
  Battle b;
  size_t k = 0;             // Kael
  std::vector<size_t> foes;  // free foes to place
  BattleHex c{};            // Kael's hex
};

// The battle with only Kael (at `level`, class `cls`) and three foes on the board (the foes kept off it until placed).
std::optional<Scene> scene(int level, const std::string& cls) {
  std::optional<Battle> loaded = Battle::load(kBattle);
  if (!loaded) return std::nullopt;
  Scene s{*loaded};
  Battle& b = s.b;
  b.endRoaming();
  int kael = -1;
  for (size_t i = 0; i < b.units().size(); ++i) {
    BattleUnit& u = b.units()[i];
    if (u.name == "Kael" && u.side == "player") kael = static_cast<int>(i);
    else if (u.side == "enemy" && s.foes.size() < 3 && !u.footprintOffsets && u.size <= 1) s.foes.push_back(i);
  }
  if (kael < 0 || s.foes.size() < 3) return std::nullopt;
  s.k = static_cast<size_t>(kael);
  for (size_t i = 0; i < b.units().size(); ++i) {
    BattleUnit& u = b.units()[i];
    if (i == s.k) continue;
    u.alive = false;  // off the board until a test places it
    u.x = -10, u.y = -10;
  }
  BattleUnit& k = b.units()[s.k];
  // the battle screen's own order (MapStage): the base class from heroBaseClasses, the level, then the promotion
  if (const std::string base = heroBaseClass(k.name, k.classId); !base.empty()) setPromotion(k, base);
  raiseToLevel(k, level, 0);
  if (cls != k.classId) setPromotion(k, cls);
  grantClassSpells(k);
  for (int& t : k.spells) t = std::max(t, 5);  // enough uses for the many casts below (one spent is checked on its own)
  // an open patch of ground: Kael's hex with every hex within 3 free
  for (int y = 3; y < b.rows() - 3; ++y)
    for (int x = 3; x < b.cols() - 3; ++x) {
      bool open = true;
      for (int yy = 0; yy < b.rows() && open; ++yy)
        for (int xx = 0; xx < b.cols() && open; ++xx)
          if (Battle::distance({x, y}, {xx, yy}) <= 3 && !(xx == k.x && yy == k.y) && !free(b, {xx, yy})) open = false;
      if (open) {
        k.x = x, k.y = y, s.c = {x, y};
        k.ap = apMax(b, k);
        return s;
      }
    }
  return std::nullopt;
}
void place(Scene& s, size_t foe, BattleHex at) {
  BattleUnit& u = s.b.units()[foe];
  u.alive = true, u.x = at.x, u.y = at.y, u.hp = u.maxHp = 400;  // it stands through every blow
  u.ap = 34;
  u.cond = {};
}
std::vector<BattleHex> ring(const Battle& b, BattleHex c) {
  std::vector<BattleHex> out;
  for (const BattleHex& n : Battle::neighbors(c.x, c.y))
    if (b.inBounds(n.x, n.y)) out.push_back(n);
  return out;
}

struct Played {
  Battle b;
  std::vector<BattleStep> steps;
  std::vector<BattleEffect> fx;
  std::string error;
  bool cast = false;
};
Played play(const Scene& s, const std::string& kind, const std::vector<BattleHex>& cells, uint32_t seed) {
  Played p{s.b};
  std::string why;
  std::optional<std::vector<BattleStep>> steps = castSpell(p.b, s.k, kind, cells, &why, true);
  if (!steps) {
    p.error = why;
    return p;
  }
  p.cast = true, p.steps = *steps;
  if (!apCommitSteps(p.b, s.k, p.steps)) p.error = "AP not paid";
  Mulberry32 rng(seed);
  playSteps(p.b, p.steps, rng, p.fx);
  return p;
}
// Tries seeds until `ok` holds on the result.
bool anySeed(const Scene& s, const std::string& kind, const std::vector<BattleHex>& cells, const std::function<bool(const Played&)>& ok, std::string* why = nullptr) {
  for (uint32_t seed = 1; seed <= 300; ++seed) {
    const Played p = play(s, kind, cells, seed);
    if (!p.cast) {
      if (why) *why = p.error;
      return false;
    }
    if (ok(p)) return true;
  }
  if (why) *why = "no seed of 300";
  return false;
}
bool hurt(const Played& p, size_t u, const Scene& s) { return p.b.units()[u].hp < s.b.units()[u].hp || !p.b.units()[u].alive; }
const BattleUnit& K(const Played& p, const Scene& s) { return p.b.units()[s.k]; }
bool has(const std::vector<std::string>& v, const std::string& k) { return std::find(v.begin(), v.end(), k) != v.end(); }

}  // namespace

int main() {
  std::printf("vanguard check: %s\n", kBattle);
  const std::vector<std::string> vanguard = {"measuredCut", "longstepSlash", "crossguard", "pommelCheck", "lowCut", "partingCut", "overheadHew", "steadyFooting", "challenge", "breakGuard"};
  const std::vector<std::string> master = {"feint", "riposte", "passingStep", "needlePoint", "secondIntention", "readTheBlade", "severTheRhythm", "perfectCut", "deadMeasure", "masterstroke"};
  const std::vector<std::string> war = {"drivingCut", "wideCleave", "sidecast", "plantTheLine", "shoulderThrough", "crushAgainstStone", "splitTheRank", "reapTheFlanks", "rollingAdvance", "shatterTheLine"};
  const std::map<std::string, int> ap = {{"measuredCut", 6}, {"longstepSlash", 7}, {"crossguard", 4}, {"pommelCheck", 5}, {"lowCut", 6}, {"partingCut", 7}, {"overheadHew", 8},
                                         {"steadyFooting", 4}, {"challenge", 5}, {"breakGuard", 8}, {"feint", 4}, {"passingStep", 7}, {"needlePoint", 7}, {"secondIntention", 9},
                                         {"readTheBlade", 4}, {"severTheRhythm", 8}, {"perfectCut", 9}, {"deadMeasure", 5}, {"masterstroke", 11}, {"drivingCut", 8}, {"wideCleave", 8},
                                         {"sidecast", 7}, {"plantTheLine", 5}, {"shoulderThrough", 8}, {"crushAgainstStone", 9}, {"splitTheRank", 9}, {"reapTheFlanks", 9},
                                         {"rollingAdvance", 10}, {"shatterTheLine", 11}};

  std::printf("the classes\n");
  {
    std::optional<Scene> s = scene(14, "vanguard");
    check(s.has_value(), "the battle loads with Kael and three foes, on open ground");
    if (!s) return 1;
    const BattleUnit& k = s->b.units()[s->k];
    check(k.classId == "vanguard", "Kael's battle file says " + std::string("swordsman") + ": he plays as a Vanguarda (" + k.classId + ", " + k.className + ")");
    bool all = true;
    for (const std::string& t : vanguard) all = all && has(k.spellList, t);
    check(all, "at level 14 he knows the Vanguarda's ten techniques");
    check(has(k.spellList, "doubleStrike") && !has(k.spellList, "provoke") && !has(k.spellList, "bullRush") && !has(k.spellList, "cleave") &&
              !has(k.spellList, "shieldBash") && !has(k.spellList, "steelWhirlwind") && !has(k.spellList, "guardBreak"),
          "of the Guerreiro's skills only Golpe Duplo is left");
    const auto table = classTierTable("vanguard"), archer = classTierTable("archer");
    bool same = !table.empty() && table == archer;
    for (size_t i = 0; i < k.growth.size() && same; ++i)
      for (size_t t = 0; t < 10; ++t) same = same && k.growth[i][5 + t] == table[std::min(i, table.size() - 1)][t];
    check(same, "his uses per tier are the Conjuração Média table (the Arqueira's), level by level");
    std::optional<Scene> low = scene(12, "vanguard");
    check(low && !has(low->b.units()[low->k].spellList, "challenge") && has(low->b.units()[low->k].spellList, "steadyFooting"), "at 12: Pé Firme (11) yes, Desafio (13) not yet");
    std::optional<Scene> m = scene(30, "swordmaster"), w = scene(30, "warbringer");
    bool m20 = m.has_value(), w20 = w.has_value();
    for (const std::string& t : vanguard) m20 = m20 && has(m->b.units()[m->k].spellList, t), w20 = w20 && has(w->b.units()[w->k].spellList, t);
    for (const std::string& t : master) m20 = m20 && has(m->b.units()[m->k].spellList, t), w20 = w20 && !has(w->b.units()[w->k].spellList, t);
    for (const std::string& t : war) w20 = w20 && has(w->b.units()[w->k].spellList, t), m20 = m20 && !has(m->b.units()[m->k].spellList, t);
    check(m20, "a Mestre da Espada of 30 keeps the Vanguarda's ten and has his own ten, none of the Arauto's");
    check(w20, "an Arauto da Guerra of 30 keeps the Vanguarda's ten and has his own ten, none of the Mestre's");
    std::optional<Scene> m16 = scene(16, "swordmaster");
    check(m16 && has(m16->b.units()[m16->k].spellList, "passingStep") && !has(m16->b.units()[m16->k].spellList, "needlePoint"), "Mestre da Espada at 16: Passo de Passagem yes, Ponta de Agulha (17) not yet");
    // the off hand: a shield in the save never reaches his hand
    Loadouts arms;
    arms.load("content/data/weapons.json");
    check(arms.arms("vanguard") && arms.arms("vanguard")->offHandLocked && arms.arms("warbringer") && arms.arms("warbringer")->offHandLocked, "weapons.json: the three classes lock the off hand");
    BattleUnit e = k;
    const Json save = {{"equipped", {{"Kael", "espada-larga"}}}, {"equipment", {{"Kael", {{"offHand", "shield-round"}}}}}};
    equipFromSave(e, arms, save);
    check(!e.combat.offHandShield && !e.combat.offHandWeapon && e.combat.hasWeapon, "a shield in the save's off hand: he holds the sword alone");
    const Json two = {{"equipped", {{"Kael", "vanguard-espadao-t05-v01"}}}};
    equipFromSave(e, arms, two);
    check(e.combat.faces == 12 && e.weaponType == "sword", "a two-handed Vanguard greatsword is his to wield (1D12)");
    const Json mace = {{"equipped", {{"Kael", "martelo-de-guerra"}}}};
    equipFromSave(e, arms, mace);
    check(e.weaponType == "sword", "a war hammer is not: he falls back on his class's sword");
  }

  std::printf("ranks (one a level, t = (rank - 1) / 14, half up)\n");
  {
    for (const auto& [level, dmg, acc] : std::vector<std::tuple<int, double, double>>{{1, 1.00, 5}, {8, 1.08, 10}, {15, 1.15, 15}, {30, 1.15, 15}}) {
      std::optional<Scene> s = scene(level, "vanguard");
      place(*s, s->foes[0], ring(s->b, s->c)[0]);
      const Played p = play(*s, "measuredCut", {ring(s->b, s->c)[0]}, 1);
      check(p.cast && !p.steps.empty() && std::abs(p.steps[0].dmgMul - dmg) < 1e-9 && p.steps[0].accBonus == acc,
            std::format("Corte Medido at level {}: {:.0f}% and +{:.0f} to hit", level, dmg * 100, acc));
    }
  }

  std::printf("AP and uses\n");
  {
    for (const auto& [cls, list] : std::vector<std::pair<std::string, std::vector<std::string>>>{{"vanguard", vanguard}, {"swordmaster", master}, {"warbringer", war}})
      for (const std::string& t : list) {
        if (t == "riposte") continue;
        std::optional<Scene> s = scene(30, cls);
        const int byKind = apCost(s->b, s->b.units()[s->k], ApAction::cast(t));
        check(byKind == ap.at(t), std::format("{}: {} AP (the design's {})", spellName(t), byKind, ap.at(t)));
      }
  }

  // --- the Vanguarda ---
  std::printf("Vanguarda\n");
  {
    std::optional<Scene> s0 = scene(30, "vanguard");
    Scene& s = *s0;
    const BattleHex n0 = ring(s.b, s.c)[0];
    Scene adj = s;
    place(adj, adj.foes[0], n0);
    // uses: one of its tier; AP: the design's; the steps' AP the same
    {
      const BattleUnit& k = adj.b.units()[adj.k];
      const Played p = play(adj, "measuredCut", {n0}, 1);
      check(p.cast && K(p, adj).spells[0] == k.spells[0] - 1, "Corte Medido spends one T1 use");
      check(p.cast && K(p, adj).ap == k.ap - 6, "... and 6 AP");
      check(anySeed(adj, "measuredCut", {n0}, [&](const Played& q) { return hurt(q, adj.foes[0], adj); }), "Corte Medido lands on the foe beside him");
    }
    {  // Passo Longo: a free hex, then a foe beside it two hexes from Kael
      const BattleHex to = ring(s.b, s.c)[0];
      BattleHex foeAt{-1, -1};
      for (const BattleHex& h : ring(s.b, to))
        if (Battle::distance(h, s.c) == 2) foeAt = h;
      Scene t = s;
      place(t, t.foes[0], foeAt);
      std::string why;
      const bool ok = anySeed(t, "longstepSlash", {to, foeAt}, [&](const Played& q) { return K(q, t).x == to.x && K(q, t).y == to.y && hurt(q, t.foes[0], t); }, &why);
      check(ok, "Passo Longo: he steps to the hex, then cuts the foe beside it" + (ok ? std::string() : " (" + why + ")"));
      const Played p = play(t, "longstepSlash", {to, foeAt}, 1);
      check(p.cast && apStepsCost(t.b, t.k, p.steps) == 7, "... the step is inside its 7 AP");
    }
    {
      const Played p = play(s, "crossguard", {s.c}, 1);
      check(p.cast && K(p, s).cond.martial.guardPct == 55, "Guarda Cruzada at rank 15: the guard 55%");
    }
    {
      Scene quiet = adj;
      quiet.b.units()[quiet.foes[0]].ap = 0;  // no AP, no counter: the mark waits for its next attack
      const bool ok = anySeed(quiet, "pommelCheck", {n0}, [&](const Played& q) { return q.b.units()[quiet.foes[0]].cond.martial.accDownPct == 25; });
      check(ok, "Golpe de Pomo landed (the foe cannot counter): its next weapon attack -25");
      const bool counter = anySeed(adj, "pommelCheck", {n0}, [&](const Played& q) {
        return hurt(q, adj.foes[0], adj) && q.b.units()[adj.foes[0]].ap < 34 && q.b.units()[adj.foes[0]].cond.martial.accDownPct == 0;
      });
      check(counter, "... a foe that counters at once spends it on that counter (its next weapon attack)");
      Played p = play(adj, "pommelCheck", {n0}, 1);
      std::string why;
      const bool again = castSpell(p.b, adj.k, "pommelCheck", {n0}, &why, true).has_value();
      check(!again && why.find("recarga") != std::string::npos, "... cooldown 1: refused again the same turn (" + why + ")");
      p.b.startNewRound();
      check(!castSpell(p.b, adj.k, "pommelCheck", {n0}, &why, true), "... and on his next turn");
      p.b.startNewRound();
      check(castSpell(p.b, adj.k, "pommelCheck", {n0}, &why, true).has_value(), "... free again the turn after");
    }
    {
      Played p = play(adj, "lowCut", {n0}, 1);
      bool landed = false;
      for (uint32_t seed = 1; seed <= 300 && !landed; ++seed)
        if (Played q = play(adj, "lowCut", {n0}, seed); q.b.units()[adj.foes[0]].cond.martial.lowCutHexes == 2) p = std::move(q), landed = true;
      check(landed, "Corte Baixo landed at rank 15: its first two hexes +1 AP");
      if (landed) {
        Battle b = p.b;
        BattleUnit& f = b.units()[adj.foes[0]];
        const std::vector<ReachCell> taxed = b.reachable(adj.foes[0], false);
        f.cond.martial.lowCutHexes = 0;
        const std::vector<ReachCell> plain = b.reachable(adj.foes[0], false);
        int d1 = -1, d2 = -1;
        for (const ReachCell& a : taxed)
          for (const ReachCell& c : plain)
            if (a.x == c.x && a.y == c.y && Battle::distance({a.x, a.y}, {f.x, f.y}) == 1) d1 = a.ap - c.ap;
            else if (a.x == c.x && a.y == c.y && Battle::distance({a.x, a.y}, {f.x, f.y}) == 3) d2 = a.ap - c.ap;
        check(d1 == 1 && d2 == 2, std::format("... its walk: a hex away +{} AP, three away +{} AP", d1, d2));
      }
    }
    {  // Corte de Saída: the cut, then a step away (on a miss too)
      BattleHex away{-1, -1};
      for (const BattleHex& h : ring(s.b, s.c))
        if (Battle::distance(h, n0) > 1) away = h;
      bool always = true;
      for (uint32_t seed = 1; seed <= 20; ++seed) {
        const Played q = play(adj, "partingCut", {n0, away}, seed);
        always = always && q.cast && K(q, adj).x == away.x && K(q, adj).y == away.y;
      }
      check(always, "Corte de Saída: he ends on the hex out of the foe's reach, hit or miss");
      std::string why;
      Battle copy = adj.b;
      check(!castSpell(copy, adj.k, "partingCut", {n0, ring(s.b, s.c)[1]}, &why, true), "... a hex beside the foe is refused (" + why + ")");
    }
    {
      const Played p = play(adj, "overheadHew", {n0}, 1);
      check(p.cast && p.steps[0].armorIgnorePct == 25 && std::abs(p.steps[0].dmgMul - 1.45) < 1e-9, "Talho por Cima at rank 15: 145%, ignores 25% of the armor");
    }
    {
      const Played p = play(s, "steadyFooting", {s.c}, 1);
      check(p.cast && K(p, s).cond.martial.footingPct == 20 && conditionsText(K(p, s)).find("Firme") != std::string::npos, "Pé Firme: -20% physical, on his sheet");
    }
    {  // Desafio: a foe two hexes away; its blows on Kael's allies take the points off, on Kael none
      BattleHex two{-1, -1};
      for (const BattleHex& h : ring(s.b, n0))
        if (Battle::distance(h, s.c) == 2) two = h;
      Scene t = s;
      place(t, t.foes[0], two);
      const Played p = play(t, "challenge", {two}, 1);
      size_t ally = t.k;
      for (size_t i = 0; i < p.b.units().size(); ++i)
        if (i != t.k && p.b.units()[i].side == "player") ally = i;
      check(p.cast && martialAccuracy(p.b, t.foes[0], ally) == -25 && martialAccuracy(p.b, t.foes[0], t.k) == 0, "Desafio: -25 on an ally of Kael's, nothing on Kael");
    }
    {
      const bool ok = anySeed(adj, "breakGuard", {n0}, [&](const Played& q) { return q.b.units()[adj.foes[0]].cond.martial.openPct == 30; });
      check(ok, "Romper a Guarda landed: the next physical hit on it ignores 30%");
    }
    // the turn starts: his own guard ends at his turn, a foe's mark after its next full turn
    {
      Played p = play(adj, "breakGuard", {n0}, 1);
      Battle& b = p.b;
      for (uint32_t seed = 2; b.units()[adj.foes[0]].cond.martial.openPct == 0 && seed < 300; ++seed) p = play(adj, "breakGuard", {n0}, seed);
      BattleUnit& f = b.units()[adj.foes[0]];
      b.units()[adj.k].cond.martial.guardPct = 40;
      Mulberry32 rng(1);
      std::vector<BattleEffect> fx;
      startOfTurnEffects(b, adj.foes[0], rng, fx);
      const bool on = f.cond.martial.openPct > 0;
      startOfTurnEffects(b, adj.foes[0], rng, fx);
      const bool off = f.cond.martial.openPct == 0;
      startOfTurnEffects(b, adj.k, rng, fx);
      check(on && off, "a foe's mark holds through its next full turn and lifts as the turn after begins");
      check(b.units()[adj.k].cond.martial.guardPct == 0, "his guard ends as his next turn begins");
    }
  }

  // --- the Mestre da Espada ---
  std::printf("Mestre da Espada\n");
  {
    std::optional<Scene> s0 = scene(30, "swordmaster");
    Scene& s = *s0;
    const BattleHex n0 = ring(s.b, s.c)[0];
    Scene adj = s;
    place(adj, adj.foes[0], n0);
    {
      const Played p = play(adj, "feint", {n0}, 1);
      const BattleUnit& f = p.b.units()[adj.foes[0]];
      check(p.cast && f.cond.martial.exposedPct == 25 && f.cond.martial.exposedBy == static_cast<int>(adj.k), "Finta: the foe Exposto 25% to Kael");
      Scene e = adj;
      e.b = p.b;
      e.b.units()[adj.k].ap = 34;
      const bool spent = anySeed(e, "needlePoint", {n0}, [&](const Played& q) { return hurt(q, e.foes[0], e) && q.b.units()[e.foes[0]].cond.martial.exposedPct == 0; });
      check(spent, "... his next landed technique hit spends the mark");
      const bool kept = anySeed(e, "needlePoint", {n0}, [&](const Played& q) { return !hurt(q, e.foes[0], e) && q.b.units()[e.foes[0]].cond.martial.exposedPct == 25; });
      check(kept, "... a miss keeps it");
    }
    {  // the guard, the parry and the Riposta
      Played g = play(adj, "crossguard", {adj.c}, 1);
      bool parried = false, riposte = false, exposed = false, used = false;
      for (uint32_t seed = 1; seed <= 300 && !(parried && riposte); ++seed) {
        Battle b = g.b;
        b.units()[adj.k].ap = 34;
        BattleStep hit;
        hit.type = BattleStep::Type::Combat, hit.att = adj.foes[0], hit.def = adj.k;
        Mulberry32 rng(seed);
        std::vector<BattleEffect> fx;
        const int t5 = b.units()[adj.k].spells[4];
        resolveStrike(b, hit, false, rng, fx);
        if (b.units()[adj.k].cond.martial.parriedBy != static_cast<int>(adj.foes[0])) continue;
        parried = std::any_of(fx.begin(), fx.end(), [](const BattleEffect& e) { return e.text == "Aparado" || e.text == "Parried"; });
        if (!strikesBack(b, hit)) continue;
        const int before = b.units()[adj.foes[0]].hp;
        resolveStrike(b, hit, true, rng, fx);
        used = b.units()[adj.k].spells[4] == t5 - 1;
        riposte = b.units()[adj.foes[0]].hp < before;
        exposed = b.units()[adj.foes[0]].cond.martial.exposedPct == 10;
        if (!riposte) parried = false;
      }
      check(parried, "Guarda Cruzada: the foe's blow that lands is parried");
      check(riposte && used && exposed, "... his counter is the Riposta: it lands, spends a T5 use and leaves the foe Exposto 10%");
    }
    {
      const BattleHex to = ring(s.b, s.c)[1];
      BattleHex side{-1, -1};
      for (const BattleHex& h : ring(s.b, s.c))
        if (Battle::distance(h, n0) == 1) side = h;
      const bool ok = anySeed(adj, "passingStep", {n0, side}, [&](const Played& q) { return K(q, adj).x == side.x && K(q, adj).y == side.y && hurt(q, adj.foes[0], adj); });
      check(ok, "Passo de Passagem: he circles to the hex beside both and cuts");
      (void)to;
    }
    {
      const Played p = play(adj, "secondIntention", {n0}, 1);
      check(p.cast && p.steps.size() == 2 && p.steps[0].noCounter && !p.steps[1].noCounter && p.steps[1].parriedAccBonus == 20, "Segunda Intenção: two cuts, the second answers a parry");
    }
    {
      const Played p = play(adj, "readTheBlade", {n0}, 1);
      const auto& m = K(p, adj).cond.martial;
      // learned at 20: rank 11 at 30, 15 -> 35 at t = 10/14: 29
      check(p.cast && m.guardPct == 29 && m.guardFrom == static_cast<int>(adj.foes[0]) && m.guardExposePct == 15, "Ler a Lâmina at rank 11: a 29% guard against that foe only, Exposto 15% on the parry");
    }
    {
      const bool ok = anySeed(adj, "severTheRhythm", {n0}, [&](const Played& q) {
        BattleStep again;
        again.type = BattleStep::Type::Combat, again.att = adj.k, again.def = adj.foes[0];
        return hurt(q, adj.foes[0], adj) && !strikesBack(q.b, again) && q.b.units()[adj.foes[0]].cond.martial.severTurns == -1;
      });
      check(ok, "Quebrar o Ritmo landed: the foe does not strike back at it");
    }
    {  // Corte Perfeito against Exposto: the points more
      Scene e = adj;
      BattleUnit& f = e.b.units()[e.foes[0]];
      f.cond.martial.exposedPct = 25, f.cond.martial.exposedBy = static_cast<int>(e.k), f.cond.martial.exposedTurns = 2;
      int bare = 0, marked = 0;
      for (uint32_t seed = 1; seed <= 300 && (!bare || !marked); ++seed) {
        const Played a = play(adj, "perfectCut", {n0}, seed), b = play(e, "perfectCut", {n0}, seed);
        if (hurt(a, adj.foes[0], adj) && hurt(b, e.foes[0], e))
          bare = adj.b.units()[adj.foes[0]].hp - a.b.units()[adj.foes[0]].hp, marked = e.b.units()[e.foes[0]].hp - b.b.units()[e.foes[0]].hp;
      }
      check(marked > bare && bare > 0, std::format("Corte Perfeito: {} on a plain foe, {} on an Exposto one (same roll)", bare, marked));
    }
    {  // Medida Mortal: the marked foe steps away, he answers before the step
      Played p = play(adj, "deadMeasure", {n0}, 1);
      check(p.cast && K(p, adj).cond.martial.measureTarget == static_cast<int>(adj.foes[0]), "Medida Mortal: the foe beside him marked");
      BattleHex away{-1, -1};
      for (const BattleHex& h : ring(p.b, n0))
        if (Battle::distance(h, adj.c) == 2 && free(p.b, h)) away = h;
      bool cancelled = false, answered = false;
      for (uint32_t seed = 1; seed <= 300 && !cancelled; ++seed) {
        Battle b = p.b;
        b.units()[adj.k].ap = 20;
        Mulberry32 rng(seed);
        std::vector<BattleEffect> fx;
        const bool stop = reactBeforeStep(b, adj.foes[0], n0, away, rng, fx);
        answered = answered || b.units()[adj.k].ap == 14;
        if (stop) cancelled = b.units()[adj.k].cond.martial.measureTarget == -1 && b.units()[adj.foes[0]].hp < 400;
      }
      check(answered, "... the step out of his reach: he spends 6 AP on the blow before it");
      check(cancelled, "... landed: the step is cancelled, the mark spent");
      Battle b = p.b;
      b.units()[adj.k].ap = 3;
      Mulberry32 rng(1);
      std::vector<BattleEffect> fx;
      check(!reactBeforeStep(b, adj.foes[0], n0, away, rng, fx) && b.units()[adj.k].cond.martial.measureTarget == -1, "... without 6 AP he lets it go, the mark spent all the same");
    }
    {
      const Played p = play(adj, "masterstroke", {n0}, 1);
      // learned at 27: rank 4 at 30 (t = 3/14): 145 -> 180 gives 153, 25 -> 40 gives 28
      check(p.cast && std::abs(p.steps[0].dmgMul - 1.53) < 1e-9 && p.steps[0].armorIgnorePct == 28 && p.steps[0].exposedAccBonus == 20,
            "Golpe de Mestre at rank 4: 153%, ignores 28%, +20 against Exposto");
      std::string why;
      Battle b = p.b;
      check(!castSpell(b, adj.k, "masterstroke", {n0}, &why, true), "... cooldown 3 (" + why + ")");
    }
    {
      Battle b = adj.b;
      std::string why;
      check(!castSpell(b, adj.k, "riposte", {n0}, &why, true), "Riposta is passive: never cast (" + why + ")");
    }
  }

  // --- the Arauto da Guerra ---
  std::printf("Arauto da Guerra\n");
  {
    std::optional<Scene> s0 = scene(30, "warbringer");
    Scene& s = *s0;
    const BattleHex n0 = ring(s.b, s.c)[0], past = beyond(s.c, n0);
    Scene adj = s;
    place(adj, adj.foes[0], n0);
    {
      const bool ok = anySeed(adj, "drivingCut", {n0}, [&](const Played& q) {
        const BattleUnit& f = q.b.units()[adj.foes[0]];
        return f.x == past.x && f.y == past.y && K(q, adj).x == n0.x && K(q, adj).y == n0.y;
      });
      check(ok, "Corte Impulsionado: the foe pushed straight back, Kael into its hex");
      const bool stays = anySeed(adj, "drivingCut", {n0}, [&](const Played& q) { return !hurt(q, adj.foes[0], adj) && K(q, adj).x == adj.c.x && K(q, adj).y == adj.c.y; });
      check(stays, "... a miss: no push, he stays");
    }
    {
      Scene t = adj;
      const std::vector<BattleHex> arc = {n0};
      BattleHex flank{-1, -1};
      for (const BattleHex& h : ring(s.b, s.c))
        if (Battle::distance(h, n0) == 1) flank = h;
      place(t, t.foes[1], flank);
      const Played p = play(t, "wideCleave", {n0}, 1);
      check(p.cast && p.steps.size() == 2 && p.steps[0].noCounter && p.steps[1].noCounter, "Talho Largo: the centre and a flank, a blow each, no counter");
      const bool both = anySeed(t, "wideCleave", {n0}, [&](const Played& q) { return hurt(q, t.foes[0], t) && hurt(q, t.foes[1], t); });
      check(both, "... both can be hit");
      const Played sh = play(t, "shatterTheLine", {n0}, 1);
      check(sh.cast && sh.steps.size() == 2 && sh.steps[0].straightPush == 1 && sh.steps[1].accDownPct > 0, "Estilhaçar a Linha: the centre pushed, the flank's next attack weakened");
      const Played re = play(t, "reapTheFlanks", {n0}, 1);
      check(re.cast && re.steps.size() == 1 && re.steps[0].def == t.foes[1], "Ceifar os Flancos: only the outer hexes (the flank, not the centre)");
    }
    {
      BattleHex dest{-1, -1};
      for (const BattleHex& h : ring(s.b, s.c))
        if (Battle::distance(h, n0) == 1) dest = h;
      const bool ok = anySeed(adj, "sidecast", {n0, dest}, [&](const Played& q) { return q.b.units()[adj.foes[0]].x == dest.x && q.b.units()[adj.foes[0]].y == dest.y; });
      check(ok, "Desvio Lateral: the foe moved to the hex chosen beside both");
    }
    {  // Firmar a Linha: a foe walks up beside him
      Played p = play(s, "plantTheLine", {s.c}, 1);
      check(p.cast && K(p, s).cond.martial.plantPct == 108, "Firmar a Linha at rank 14 (learned at 17): the stance, its blow 108%");
      const BattleHex from = beyond(s.c, n0);
      p.b.units()[s.foes[0]].alive = true, p.b.units()[s.foes[0]].x = from.x, p.b.units()[s.foes[0]].y = from.y, p.b.units()[s.foes[0]].hp = 400;
      bool stopped = false, answered = false;
      for (uint32_t seed = 1; seed <= 300 && !stopped; ++seed) {
        Battle b = p.b;
        b.units()[s.k].ap = 20;
        b.units()[s.foes[0]].x = n0.x, b.units()[s.foes[0]].y = n0.y;  // the step taken
        Mulberry32 rng(seed);
        std::vector<BattleEffect> fx;
        stopped = reactAfterStep(b, s.foes[0], from, n0, rng, fx);
        answered = answered || b.units()[s.k].ap == 14;
      }
      check(answered && stopped, "... the foe that comes beside him takes the blow (6 AP); landed, its walk ends");
    }
    {  // Ombrada: the foe two hexes away, the hex between free
      Scene t = s;
      place(t, t.foes[0], past);
      const BattleHex behind = beyond(n0, past);
      const bool ok = anySeed(t, "shoulderThrough", {past}, [&](const Played& q) {
        const BattleUnit& f = q.b.units()[t.foes[0]];
        return K(q, t).x == n0.x && K(q, t).y == n0.y && f.x == behind.x && f.y == behind.y;
      });
      check(ok, "Ombrada: he takes the middle hex, the foe is pushed on");
    }
    {  // Esmagar na Pedra: against the board's edge
      std::optional<Scene> e0 = scene(30, "warbringer");
      Scene e = *e0;
      Battle& b = e.b;
      bool set = false;
      for (int y = 1; y < b.rows() - 1 && !set; ++y) {
        const BattleHex foe{0, y};
        for (const BattleHex& kHex : ring(b, foe))
          if (!set && kHex.x >= 0 && free(b, kHex) && free(b, foe) && !b.inBounds(beyond(kHex, foe).x, beyond(kHex, foe).y))
            b.units()[e.k].x = kHex.x, b.units()[e.k].y = kHex.y, e.c = kHex, place(e, e.foes[0], foe), set = true;
      }
      check(set, "a foe on the board's edge with Kael beside it");
      if (set) {
        const BattleHex foe{b.units()[e.foes[0]].x, b.units()[e.foes[0]].y};
        const bool ok = anySeed(e, "crushAgainstStone", {foe}, [&](const Played& q) {
          return std::any_of(q.fx.begin(), q.fx.end(), [](const BattleEffect& x) { return x.text.find("pedra") != std::string::npos || x.text.find("stone") != std::string::npos; });
        });
        check(ok, "Esmagar na Pedra: the push meets the edge, the impact lands");
      }
    }
    {  // Rachar a Fileira
      Scene t = adj;
      place(t, t.foes[1], past);
      const bool second = anySeed(t, "splitTheRank", {n0}, [&](const Played& q) { return hurt(q, t.foes[0], t) && hurt(q, t.foes[1], t); });
      const bool chain = anySeed(t, "splitTheRank", {n0}, [&](const Played& q) { return !hurt(q, t.foes[0], t) && !hurt(q, t.foes[1], t); });
      bool never = true;
      for (uint32_t seed = 1; seed <= 200; ++seed) {
        const Played q = play(t, "splitTheRank", {n0}, seed);
        if (!hurt(q, t.foes[0], t) && hurt(q, t.foes[1], t)) never = false;
      }
      check(second && chain && never, "Rachar a Fileira: the second hex only when the first lands");
    }
    {  // Avanço Rolante
      const BattleHex third = beyond(n0, past);
      const bool ok = anySeed(adj, "rollingAdvance", {n0}, [&](const Played& q) {
        const BattleUnit& f = q.b.units()[adj.foes[0]];
        return K(q, adj).x == n0.x && K(q, adj).y == n0.y && f.x == third.x && f.y == third.y;
      });
      check(ok, "Avanço Rolante: blow and push, he follows, blow and push again; he ends on the first vacated hex");
    }
  }

  std::printf("save / load\n");
  {
    std::optional<Scene> s0 = scene(30, "swordmaster");
    Scene& s = *s0;
    const BattleHex n0 = ring(s.b, s.c)[0];
    place(s, s.foes[0], n0);
    Played p = play(s, "masterstroke", {n0}, 1);
    BattleUnit& f = p.b.units()[s.foes[0]];
    f.cond.martial.exposedPct = 20, f.cond.martial.exposedBy = static_cast<int>(s.k), f.cond.martial.exposedTurns = 2, f.cond.martial.lowCutHexes = 2, f.cond.martial.lowCutTurns = 1;
    p.b.units()[s.k].cond.martial.guardPct = 35, p.b.units()[s.k].cond.martial.guardFrom = static_cast<int>(s.foes[0]);
    const nlohmann::json s1 = p.b.snapshot();
    std::optional<Battle> c = Battle::load(kBattle);
    const bool applied = c && c->applySnapshot(s1);
    check(applied && c->snapshot() == s1, "snapshot -> apply -> snapshot gives the same state back");
    if (applied) {
      const BattleUnit& k = c->units()[s.k];
      const BattleUnit& g = c->units()[s.foes[0]];
      check(k.classId == "swordmaster" && k.cond.martial.guardPct == 35 && g.cond.martial.exposedPct == 20 && g.cond.martial.lowCutHexes == 2 && !k.cooldowns.empty(),
            "his class, his guard, the foe's marks and his cooldown come back");
      std::string why;
      check(!castSpell(*c, s.k, "masterstroke", {n0}, &why, true), "... the cooldown still blocks Golpe de Mestre after loading (" + why + ")");
    }
  }

  std::printf("vanguard check: %d passed, %d failed\n", gPassed, gFailed);
  return gFailed ? 1 : 0;
}
