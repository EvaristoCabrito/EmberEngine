// Garran Holt's techniques: the Guardião da Lua (Moonwarden; docs/SKILLS_MOONWARDEN.md). Included by SpellsVanguard.inc right after
// its step builders: the same technique system as Kael's (kTechniques, the ranks, the cooldowns, one use of the tier, the one strike).
//
// The user, 2026-11-13 (the design "Moonwarden — Permanent Skills, T1–T10", replacing the first moon-phase draft): one class from level
// 1 to 30, one permanent skill a tier (T1-T10 of the casters' table: 1, 3, 6, 8, 11, 13, 16, 18, 21, 23). His loop: guard an ally, take
// part of the hit, punish the foe responsible. Every skill works in every form (none in the game yet). Damage is a share of an
// ordinary blow of his weapon. No test-mode branch.

// The moon on the world's clock (Battle::moonPhase): 0 new, 1 waxing, 2 full (the blood moon too; an unknown moon), 3 waning. No skill
// of his reads it any more (the T1-T10 design has no moon); kept for the screens that show it (moonForm).
int moonFormOf(const std::string& phase) {
  if (phase == "new") return 0;
  if (phase == "waxingCrescent" || phase == "firstQuarter" || phase == "waxingGibbous") return 1;
  if (phase == "waningGibbous" || phase == "lastQuarter" || phase == "waningCrescent") return 3;
  return 2;
}

bool wardKind(const std::string& k) {
  return k == "steadyStrike" || k == "digIn" || k == "flankStep" || k == "closeRanks" || k == "breakStride" || k == "watchTheGap" ||
         k == "followThrough" || k == "pullClear" || k == "takeTheWeight" || k == "reckoning";
}

// --- The board ---------------------------------------------------------------------------------------------------------------------------
// One of his own side (not himself, alive) on the hex, within `reach` of `from`.
int wardAlly(const Battle& b, size_t caster, BattleHex from, BattleHex cell, int reach) {
  const int a = occAt(b, cell);
  if (a < 0 || static_cast<size_t>(a) == caster) return -1;
  const BattleUnit &u = b.units()[caster], &t = b.units()[static_cast<size_t>(a)];
  if (!t.alive || t.side != u.side) return -1;
  return besideUnit(b, from, static_cast<size_t>(a), reach) ? a : -1;
}
void wardNote(std::vector<int>& list, int foe) {
  if (std::find(list.begin(), list.end(), foe) == list.end()) list.push_back(foe);
}
bool wardRemembers(const BattleUnit::Conditions::Martial& m, int foe) {
  return std::find(m.stopped.begin(), m.stopped.end(), foe) != m.stopped.end() ||
         std::find(m.stoppedPrev.begin(), m.stoppedPrev.end(), foe) != m.stoppedPrev.end();
}

// --- The aim ------------------------------------------------------------------------------------------------------------------------------
// Two picks: Passo de Flanco (the hex, then the foe beside it), Cerrar Fileiras (the ally, then the hex he ends on: his own = no step),
// Golpe Seguido (the first foe, then the second), Puxar para Longe (the ally, then the hex it goes to).
int wardTargetCount(const Battle&, size_t, const std::string& k) {
  return k == "flankStep" || k == "closeRanks" || k == "followThrough" || k == "pullClear" ? 2 : 1;
}
int wardReach(const Battle&, const std::string& k) {
  if (k == "digIn") return 0;
  if (k == "flankStep" || k == "closeRanks") return 2;
  return 1;
}
bool wardAimValid(const Battle& b, size_t caster, const std::string& k, BattleHex cell, std::string* error) {
  const BattleUnit& u = b.units()[caster];
  const BattleHex from{u.x, u.y};
  auto fail = [&](const char* why) {
    if (error) *error = why;
    return false;
  };
  const bool stepHex = hexDist(from, cell) == 1 && techniqueStandable(b, cell);
  if (k == "digIn") return cell == from ? true : fail("Esta técnica é em si mesmo.");
  if (k == "flankStep")
    return stepHex || techniqueFoe(b, from, cell, 2) >= 0 ? true : fail("Escolha um hex livre ao lado, depois um inimigo ao lado dele.");
  if (k == "closeRanks")
    return wardAlly(b, caster, from, cell, 2) >= 0 || cell == from || stepHex ? true : fail("Escolha um aliado a até 2 hexes, depois onde você termina.");
  if (k == "pullClear")
    return wardAlly(b, caster, from, cell, 1) >= 0 || stepHex ? true : fail("Escolha um aliado ao lado, depois um hex livre ao seu lado.");
  if (k == "takeTheWeight") return wardAlly(b, caster, from, cell, 1) >= 0 ? true : fail("Escolha um aliado ao seu lado.");
  if (k == "watchTheGap") return hexDist(from, cell) == 1 && b.inBounds(cell.x, cell.y) ? true : fail("Escolha um hex ao seu lado.");
  return techniqueFoe(b, from, cell, 1) >= 0 ? true : fail("Escolha um inimigo ao lado.");
}

// --- The cast -----------------------------------------------------------------------------------------------------------------------------
std::optional<std::vector<BattleStep>> castWarden(Battle& b, size_t caster, const std::string& kind, const std::vector<BattleHex>& cells, std::string* error) {
  BattleUnit& u = b.units()[caster];
  auto fail = [&](const std::string& why) -> std::optional<std::vector<BattleStep>> {
    if (error) *error = why;
    return std::nullopt;
  };
  if (cells.empty()) return fail("Escolha um alvo.");
  const BattleHex from{u.x, u.y}, cell = cells[0];
  const TechNumbers n = techniqueNumbers(kind, u);
  std::vector<BattleStep> steps;
  auto commit = [&] {
    spendTier(u, kind);
    techniqueStartCooldown(b, u, kind);
  };
  const char* noFoe = "Escolha um inimigo ao lado.";
  if (kind == "steadyStrike" || kind == "breakStride" || kind == "reckoning") {
    const int foe = techniqueFoe(b, from, cell, 1);
    if (foe < 0) return fail(noFoe);
    int pct = n.dmg;
    if (kind == "reckoning" && wardRemembers(u.cond.martial, foe)) pct = std::min(205, pct + n.pct);  // he protected an ally from it
    BattleStep c = techniqueBlow(caster, static_cast<size_t>(foe), kind, pct);
    if (kind == "steadyStrike") c.accBonus = n.pct;
    if (kind == "breakStride") c.lowCutHexes = n.hexes;
    commit();
    steps.push_back(c);
    return steps;
  }
  if (kind == "flankStep") {  // the empty hex beside him, then a foe beside it
    if (cells.size() < 2) return fail("Escolha o hex e depois o inimigo.");
    if (hexDist(from, cell) != 1 || !techniqueStandable(b, cell)) return fail("Escolha primeiro um hex livre ao seu lado.");
    const int foe = techniqueFoe(b, cell, cells[1], 1);
    if (foe < 0) return fail("Escolha um inimigo ao lado do hex escolhido.");
    commit();
    steps.push_back(techniqueMove(caster, kind, from, cell));
    BattleStep c = techniqueBlow(caster, static_cast<size_t>(foe), kind, n.dmg);
    c.chained = true;
    steps.push_back(c);
    return steps;
  }
  if (kind == "followThrough") {  // two different foes beside him: each its own blow and roll, no counter (as every multi-target skill)
    if (cells.size() < 2) return fail("Escolha os dois inimigos.");
    const int a = techniqueFoe(b, from, cells[0], 1), c2 = techniqueFoe(b, from, cells[1], 1);
    if (a < 0 || c2 < 0) return fail("Escolha dois inimigos ao seu lado.");
    if (a == c2) return fail("Escolha dois inimigos diferentes.");
    commit();
    BattleStep first = techniqueBlow(caster, static_cast<size_t>(a), kind, n.dmg), second = techniqueBlow(caster, static_cast<size_t>(c2), kind, n.dmg2);
    first.noCounter = second.noCounter = true;
    steps.push_back(first);
    steps.push_back(second);
    return steps;
  }
  if (kind == "digIn") {
    if (!(cell == from)) return fail("Esta técnica é em si mesmo.");
    commit();
    BattleStep s = techniqueEffectStep(caster, kind, from, n.pct);
    s.braceHits = 1, s.bracePush = true;  // the first physical hit and the first push, each its own charge
    steps.push_back(s);
    return steps;
  }
  if (kind == "closeRanks") {  // the ally within 2, then where he ends (his own hex: no step), beside the ally
    if (cells.size() < 2) return fail("Escolha o aliado e depois onde você termina.");
    const int ally = wardAlly(b, caster, from, cell, 2);
    if (ally < 0) return fail("Escolha um aliado a até 2 hexes.");
    const BattleHex to = cells[1];
    const bool stays = to == from;
    if (!stays && (hexDist(from, to) != 1 || !techniqueStandable(b, to))) return fail("Escolha um hex livre ao seu lado (ou o seu).");
    if (!besideUnit(b, to, static_cast<size_t>(ally), 1)) return fail("Você precisa terminar ao lado do aliado.");
    commit();
    if (!stays) steps.push_back(techniqueMove(caster, kind, from, to));
    BattleStep s = techniqueEffectStep(caster, kind, cell, n.pct);
    s.ids = {static_cast<size_t>(ally)}, s.chained = !stays;
    steps.push_back(s);
    return steps;
  }
  if (kind == "pullClear") {  // the ally beside him, then an empty hex beside him that the ally reaches in one step
    if (cells.size() < 2) return fail("Escolha o aliado e depois o hex.");
    const int ally = wardAlly(b, caster, from, cell, 1);
    if (ally < 0) return fail("Escolha um aliado ao seu lado.");
    const BattleUnit& a = b.units()[static_cast<size_t>(ally)];
    const BattleHex to = cells[1];
    if (hexDist(from, to) != 1 || !techniqueStandable(b, to) || hexDist({a.x, a.y}, to) != 1 || !techniqueDisplaceable(a))
      return fail("Escolha um hex livre ao seu lado e ao lado do aliado.");
    commit();
    steps.push_back(techniqueMove(static_cast<size_t>(ally), kind, {a.x, a.y}, to));  // the ally's step, paid by his AP (the action's)
    BattleStep s = techniqueEffectStep(caster, kind, to, n.pct);
    s.ids = {static_cast<size_t>(ally)}, s.chained = true;
    steps.push_back(s);
    return steps;
  }
  if (kind == "takeTheWeight") {
    const int ally = wardAlly(b, caster, from, cell, 1);
    if (ally < 0) return fail("Escolha um aliado ao seu lado.");
    commit();
    BattleStep s = techniqueEffectStep(caster, kind, cell, n.pct);
    s.ids = {static_cast<size_t>(ally)};
    steps.push_back(s);
    return steps;
  }
  if (kind == "watchTheGap") {
    if (hexDist(from, cell) != 1 || !b.inBounds(cell.x, cell.y)) return fail("Escolha um hex ao seu lado.");
    commit();
    steps.push_back(techniqueEffectStep(caster, kind, cell, n.dmg));
    return steps;
  }
  return fail("Esta técnica não pode ser usada assim.");
}

// --- Guards, covers and marks (Type::Effect): true when the step was his -------------------------------------------------------------
bool wardEffect(Battle& b, const BattleStep& step, std::vector<BattleEffect>& out) {
  if (!wardKind(step.kind)) return false;
  BattleUnit& g = b.units()[step.att];
  BattleUnit::Conditions::Martial& m = g.cond.martial;
  const int me = static_cast<int>(step.att);
  const size_t ally = step.ids.empty() ? step.att : step.ids.front();
  BattleUnit::Conditions::Martial& a = b.units()[ally].cond.martial;
  if (step.kind == "digIn") {
    m.bracePct = step.bonus, m.braceHits = step.braceHits, m.bracePush = step.bracePush;
    out.push_back(statusEffect(step.att, std::format("{} -{}%", pick("Firmar o Pé", "Dig In"), step.bonus)));
  } else if (step.kind == "closeRanks" && ally != step.att) {
    a.holdPct = step.bonus, a.holdBy = me;
    out.push_back(statusEffect(ally, std::format("{} -{}%", pick("Fileiras cerradas", "Ranks closed"), step.bonus)));
  } else if (step.kind == "pullClear" && ally != step.att) {
    a.accUpPct = std::max(a.accUpPct, step.bonus), a.accUpTurns = 2;
    out.push_back(statusEffect(ally, std::format("+{} {}", a.accUpPct, pick("acerto", "to hit"))));
  } else if (step.kind == "takeTheWeight" && ally != step.att) {
    a.weightBy = me, a.weightPct = step.bonus;
    out.push_back(statusEffect(ally, std::format("{} {}%", pick("Peso dividido", "Weight shared"), step.bonus)));
  } else if (step.kind == "watchTheGap" && !step.tiles.empty()) {
    m.watchX = step.tiles.front().x, m.watchY = step.tiles.front().y, m.watchPct = step.bonus;
    out.push_back(statusEffect(step.att, pick("Vigiar a Brecha", "Watch the Gap")));
  }
  return true;
}

// --- The start of a unit's own turn -----------------------------------------------------------------------------------------------------
void wardTurnStart(Battle& b, size_t unit) {
  BattleUnit::Conditions::Martial& m = b.units()[unit].cond.martial;
  // his own: until his next turn begins
  m.bracePct = 0, m.braceHits = 0, m.bracePush = false, m.watchPct = 0, m.watchX = m.watchY = -1;
  // the foes he stopped since the start of his previous turn: the turn before is forgotten, this one begins
  m.stoppedPrev = std::move(m.stopped), m.stopped.clear();
  // what he laid on others lasts until his next turn
  const int me = static_cast<int>(unit);
  for (BattleUnit& v : b.units()) {
    BattleUnit::Conditions::Martial& o = v.cond.martial;
    if (o.holdBy == me) o.holdBy = -1, o.holdPct = 0;
    if (o.weightBy == me) o.weightBy = -1, o.weightPct = 0;
  }
  // on an ally: Puxar para Longe's aim through the end of its next turn
  if (m.accUpTurns > 0 && --m.accUpTurns == 0) m.accUpPct = 0;
}

// --- The strike (resolveStrikeImpl) -----------------------------------------------------------------------------------------------------
// Puxar para Longe: the ally's next weapon attack takes its points to hit, spent by that roll.
void wardAccuracy(Battle& b, bool counter, size_t ai, StrikeMods& mods) {
  BattleUnit::Conditions::Martial& m = b.units()[ai].cond.martial;
  if (counter || m.accUpPct <= 0) return;
  mods.accuracy += m.accUpPct;
  m.accUpPct = 0, m.accUpTurns = 0;
}
// A landed physical weapon hit of `ai` on `ti`, after its mitigation and Kael's guards: Firmar o Pé's first hit on him, Cerrar Fileiras'
// first direct hit on the ally beside him - the strongest alone, its charge alone spent (Cerrar Fileiras remembers the foe for Acerto
// de Contas).
int wardReduce(Battle& b, const BattleStep&, bool, size_t ai, size_t ti, bool, int dmg, std::vector<BattleEffect>& after) {
  BattleUnit &a = b.units()[ai], &t = b.units()[ti];
  if (a.side == t.side) return dmg;
  BattleUnit::Conditions::Martial& tm = t.cond.martial;
  enum Source { None, Brace, Hold };
  int best = 0;
  Source source = None;
  if (tm.braceHits > 0 && tm.bracePct > best) best = tm.bracePct, source = Brace;
  const size_t count = b.units().size();
  if (tm.holdPct > best && tm.holdBy >= 0 && static_cast<size_t>(tm.holdBy) < count && b.units()[static_cast<size_t>(tm.holdBy)].alive &&
      besideUnit(b, {t.x, t.y}, static_cast<size_t>(tm.holdBy), 1))
    best = tm.holdPct, source = Hold;
  if (source == None) return dmg;
  dmg = std::max(1, static_cast<int>(std::floor(dmg * (1 - best / 100.0))));
  if (source == Brace) {
    tm.braceHits = 0, tm.bracePct = 0;
    after.push_back(statusEffect(ti, std::format("{} -{}%", pick("Firmar o Pé", "Dig In"), best)));
  } else {
    wardNote(b.units()[static_cast<size_t>(tm.holdBy)].cond.martial.stopped, static_cast<int>(ai));
    tm.holdPct = 0, tm.holdBy = -1;
    after.push_back(statusEffect(ti, std::format("{} -{}%", pick("Cerrar Fileiras", "Close Ranks"), best)));
  }
  return dmg;
}
// Assumir o Peso: the covered ally's first landed single-target weapon hit while he stands beside it - this share of its damage after
// every mitigation is his instead, taken straight off his HP. What is left for the ally.
int wardWeight(Battle& b, const BattleStep& step, bool counter, size_t ai, size_t ti, int dmg, std::vector<BattleEffect>& after, Mulberry32& rng) {
  BattleUnit &a = b.units()[ai], &t = b.units()[ti];
  BattleUnit::Conditions::Martial& tm = t.cond.martial;
  if (dmg <= 0 || tm.weightPct <= 0 || tm.weightBy < 0 || static_cast<size_t>(tm.weightBy) >= b.units().size() || a.side == t.side) return dmg;
  if (!counter && step.tiles.size() > 1) return dmg;  // an area blow is not a single-target one
  const size_t gi = static_cast<size_t>(tm.weightBy);
  BattleUnit& g = b.units()[gi];
  if (!g.alive || !besideUnit(b, {g.x, g.y}, ti, 1)) return dmg;
  const int moved = static_cast<int>(std::floor(dmg * tm.weightPct / 100.0));
  tm.weightBy = -1, tm.weightPct = 0;  // the first such hit spends it
  wardNote(g.cond.martial.stopped, static_cast<int>(ai));
  if (moved <= 0) return dmg;
  g.hp = std::max(0, g.hp - moved);
  b.noteDamage(ai, gi, moved, false);
  BattleEffect taken{gi, moved, false, false, false, false, false};
  taken.text = std::format("{} {}", moved, pick("(Assumir o Peso)", "(Take the Weight)"));
  if (g.hp <= 0) markDead(g, rng), taken.died = true;
  after.push_back(taken);
  return dmg - moved;
}

// --- Movement reaction (Vigiar a Brecha): an enemy's voluntary step into the hex he watches -------------------------------------------
bool techniqueReactionBlow(Battle& b, size_t kael, size_t walker, const std::string& kind, int pct, Mulberry32& rng, std::vector<BattleEffect>& out);
bool wardAfterStep(Battle& b, size_t walker, BattleHex, BattleHex to, Mulberry32& rng, std::vector<BattleEffect>& out) {
  bool stop = false;
  for (size_t i = 0; i < b.units().size(); ++i) {
    BattleUnit& g = b.units()[i];
    if (i == walker || !g.alive || g.cond.martial.watchPct <= 0 || g.side == b.units()[walker].side) continue;
    if (!(to.x == g.cond.martial.watchX && to.y == g.cond.martial.watchY)) continue;
    const int pct = g.cond.martial.watchPct;
    g.cond.martial.watchPct = 0, g.cond.martial.watchX = g.cond.martial.watchY = -1;  // the first entry spends it, answered or not
    if (techniqueReactionBlow(b, i, walker, "watchTheGap", pct, rng, out) && b.units()[walker].alive && techniqueDisplaceable(b.units()[walker])) stop = true;
  }
  return stop;
}

// --- Words --------------------------------------------------------------------------------------------------------------------------------
void wardConditions(const BattleUnit& u, const std::function<void(const std::string&)>& add) {
  const BattleUnit::Conditions::Martial& m = u.cond.martial;
  if (m.braceHits > 0) add(std::format("{} -{}%", pick("Firmar o Pé", "Dig In"), m.bracePct));
  if (m.bracePush) add(pick("Firme contra empurrões", "Braced against pushes"));
  if (m.watchPct > 0) add(pick("Vigiando a brecha", "Watching the gap"));
  if (m.holdPct > 0) add(std::format("{} -{}%", pick("Fileiras cerradas", "Ranks closed"), m.holdPct));
  if (m.weightPct > 0) add(std::format("{} {}%", pick("Peso dividido", "Weight shared"), m.weightPct));
  if (m.accUpPct > 0) add(std::format("+{} {}", m.accUpPct, pick("no próximo ataque", "on the next attack")));
}
std::string wardTooltipText(const std::string& k, const TechNumbers& n, int) {
  const auto pct = [](int v) { return std::format("{}%", v); };
  if (k == "steadyStrike") return std::format("inimigo ao lado · {} · +{} de acerto", pct(n.dmg), n.pct);
  if (k == "digIn")
    return std::format("até o seu próximo turno: o primeiro golpe físico em você -{}; o primeiro empurrão contra você anda 1 hex a menos (cada um a sua carga)", pct(n.pct));
  if (k == "flankStep") return std::format("um passo para um hex livre ao lado, depois golpeia um inimigo ao lado dele · {}", pct(n.dmg));
  if (k == "closeRanks")
    return std::format("aliado a até 2 hexes · até um passo para terminar ao lado dele · o primeiro golpe de arma direto nele -{}, enquanto estiver ao seu lado · até o seu próximo turno", pct(n.pct));
  if (k == "breakStride") return std::format("inimigo ao lado · {} · {} do próximo turno dele +1 PA", pct(n.dmg), n.hexes > 1 ? "os 2 primeiros passos" : "o primeiro passo");
  if (k == "watchTheGap")
    return std::format("vigia um hex ao seu lado até o seu próximo turno · o primeiro inimigo que entrar nele andando leva {} (6 PA); acertou: o movimento dele acaba", pct(n.dmg));
  if (k == "followThrough") return std::format("dois inimigos diferentes ao seu lado · o primeiro {}, o segundo {} · cada golpe rola sozinho · sem contra-ataque", pct(n.dmg), pct(n.dmg2));
  if (k == "pullClear") return std::format("um aliado ao lado vai 1 hex para um hex livre também ao seu lado · o próximo ataque de arma dele +{} de acerto", n.pct);
  if (k == "takeTheWeight")
    return std::format("um aliado ao lado até o seu próximo turno · o primeiro golpe de arma de alvo único que ele levar, ainda ao seu lado: {} do dano vem para você", pct(n.pct));
  if (k == "reckoning")
    return std::format("inimigo ao lado · {} · +{} pontos se Cerrar Fileiras ou Assumir o Peso protegeu um aliado dele desde o início do seu turno anterior · máximo 205%",
                       pct(n.dmg), n.pct);
  return {};
}
