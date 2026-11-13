// Garran Holt's techniques: the Guardião da Lua (Moonwarden; docs/SKILLS_MOONWARDEN.md). Included by SpellsVanguard.inc right after
// its step builders: the same technique system as Kael's (kTechniques, the ranks, the cooldowns, one use of the tier, the one strike).
//
// The user, 2026-11-13 (the design "Werewolf — Human Form", then "Garran Holt — Moonwarden", "Guardião da Lua em Português, add him"):
// sword and shield, ten techniques over T1-T5, one blow and one guard a tier. His loop: take the hit meant for an ally, then punish the
// foe that committed to it. Every technique works under any moon; tonight's moon (the world's clock, Battle::moonPhase) changes three
// of them, read once as the technique commits and carried in its steps (an armed Interpor keeps the moon that was paid for). The blood
// moon is the full moon's (the user did not answer otherwise). Damage is a share of an ordinary blow of his sword. No test-mode branch.

enum WardMoon { kMoonNew = 0, kMoonWaxing = 1, kMoonFull = 2, kMoonWaning = 3 };
int moonFormOf(const std::string& phase) {
  if (phase == "new") return kMoonNew;
  if (phase == "waxingCrescent" || phase == "firstQuarter" || phase == "waxingGibbous") return kMoonWaxing;
  if (phase == "waningGibbous" || phase == "lastQuarter" || phase == "waningCrescent") return kMoonWaning;
  return kMoonFull;  // full, blood, or a map that told nothing (its sky is the full moon's)
}
int wardMoon(const Battle& b) { return moonFormOf(b.moonPhase()); }
std::string wardMoonName(int moon) {
  switch (moon) {
    case kMoonNew: return pick("Lua nova", "New Moon");
    case kMoonWaxing: return pick("Lua crescente", "Waxing Moon");
    case kMoonWaning: return pick("Lua minguante", "Waning Moon");
    default: return pick("Lua cheia", "Full Moon");
  }
}

bool wardKind(const std::string& k) {
  return k == "guardedCut" || k == "brace" || k == "wardenBash" || k == "interpose" || k == "gapThrust" || k == "affront" || k == "crushingBlow" ||
         k == "holdTheLine" || k == "reckoning" || k == "unbreakable";
}

// --- The board ---------------------------------------------------------------------------------------------------------------------------
// One of his own side (not himself) on the hex, standing beside him: Interpor's, Segurar a Linha's, Inquebrável's ally.
int wardAlly(const Battle& b, size_t caster, BattleHex cell) {
  const int a = occAt(b, cell);
  if (a < 0 || static_cast<size_t>(a) == caster) return -1;
  const BattleUnit &u = b.units()[caster], &t = b.units()[static_cast<size_t>(a)];
  if (!t.alive || t.side != u.side) return -1;
  return besideUnit(b, {u.x, u.y}, static_cast<size_t>(a), 1) ? a : -1;
}
int wardAlliesBeside(const Battle& b, size_t caster) {
  int n = 0;
  for (size_t i = 0; i < b.units().size(); ++i)
    if (i != caster && wardAlly(b, caster, {b.units()[i].x, b.units()[i].y}) == static_cast<int>(i)) ++n;
  return n;
}
bool wardLowHp(const BattleUnit& u) { return u.maxHp > 0 && u.hp * 100 <= 35 * u.maxHp; }  // Inquebrável: 35 % of his HP or under
bool wardRemembers(const std::vector<int>& now, const std::vector<int>& before, int foe) {
  return std::find(now.begin(), now.end(), foe) != now.end() || std::find(before.begin(), before.end(), foe) != before.end();
}
void wardNote(std::vector<int>& list, int foe) {
  if (std::find(list.begin(), list.end(), foe) == list.end()) list.push_back(foe);
}

// --- The aim ------------------------------------------------------------------------------------------------------------------------------
// The picks a cast takes: Corte Guardado under the waxing moon the foe then the sidestep's hex, Estocada na Brecha under the new moon and
// Acerto de Contas under the waxing moon the step's hex (his own: no step) then the foe; Segurar a Linha one pick an ally beside him, two
// at most.
int wardTargetCount(const Battle& b, size_t unit, const std::string& k) {
  const int moon = wardMoon(b);
  if (k == "guardedCut") return moon == kMoonWaxing ? 2 : 1;
  if (k == "gapThrust") return moon == kMoonNew ? 2 : 1;
  if (k == "reckoning") return moon == kMoonWaxing ? 2 : 1;
  if (k == "holdTheLine") return std::clamp(wardAlliesBeside(b, unit), 1, 2);
  return 1;
}
int wardReach(const Battle& b, const std::string& k) {
  const int moon = wardMoon(b);
  if (k == "brace") return 0;
  if (k == "unbreakable") return moon == kMoonWaning ? 1 : 0;
  if (k == "affront") return 3;
  if ((k == "gapThrust" && moon == kMoonNew) || (k == "reckoning" && moon == kMoonWaxing)) return 2;
  return 1;
}
bool wardAimValid(const Battle& b, size_t caster, const std::string& k, BattleHex cell, std::string* error) {
  const BattleUnit& u = b.units()[caster];
  const BattleHex from{u.x, u.y};
  const int moon = wardMoon(b);
  auto fail = [&](const char* why) {
    if (error) *error = why;
    return false;
  };
  const bool foe1 = techniqueFoe(b, from, cell, 1) >= 0;
  if (k == "brace") return cell == from ? true : fail("Esta técnica é em si mesmo.");
  if (k == "unbreakable") {
    if (!wardLowHp(u)) return fail("Só com 35% da vida ou menos.");
    if (cell == from) return true;
    if (moon == kMoonWaning && wardAlly(b, caster, cell) >= 0) return true;
    return fail(moon == kMoonWaning ? "Escolha você mesmo ou um aliado ao lado." : "Esta técnica é em si mesmo.");
  }
  if (k == "interpose" || k == "holdTheLine") return wardAlly(b, caster, cell) >= 0 ? true : fail("Escolha um aliado ao seu lado.");
  if (k == "affront") return techniqueFoe(b, from, cell, 3) >= 0 ? true : fail("Escolha um inimigo a até 3 hexes.");
  if (k == "guardedCut" && moon == kMoonWaxing) {  // the foe, then a hex beside him (his own: no sidestep)
    if (foe1 || cell == from || (hexDist(from, cell) == 1 && techniqueStandable(b, cell))) return true;
    return fail("Escolha o inimigo ao lado, depois o hex do passo (o seu, para ficar).");
  }
  if ((k == "gapThrust" && moon == kMoonNew) || (k == "reckoning" && moon == kMoonWaxing)) {  // the step's hex (his own: none), then the foe
    if (cell == from || (hexDist(from, cell) == 1 && techniqueStandable(b, cell)) || techniqueFoe(b, from, cell, 2) >= 0) return true;
    return fail("Escolha o hex do passo (o seu, para ficar), depois um inimigo ao lado dele.");
  }
  return foe1 ? true : fail("Escolha um inimigo ao lado.");
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
  const int moon = wardMoon(b);  // the moon this technique is paid under: every number it changes goes into the steps now
  std::vector<BattleStep> steps;
  auto commit = [&] {
    spendTier(u, kind);
    techniqueStartCooldown(b, u, kind);
  };
  const char* noFoe = "Escolha um inimigo ao lado.";
  // The blow on a foe beside `at`, dressed by the kind (its share of the blow may depend on the foe: Corte Guardado, Acerto de Contas).
  auto blow = [&](size_t foe) {
    const BattleUnit::Conditions::Martial& m = u.cond.martial;
    int pct = n.dmg;
    if (kind == "guardedCut" && moon == kMoonFull && wardRemembers(m.hitBy, m.hitByPrev, static_cast<int>(foe))) pct += 20;
    if (kind == "reckoning" && wardRemembers(m.stopped, m.stoppedPrev, static_cast<int>(foe))) pct = std::min(210, pct + n.pct);
    BattleStep c = techniqueBlow(caster, foe, kind, pct);
    if (kind == "guardedCut") c.wardCutPct = n.pct;
    else if (kind == "wardenBash") c.straightPush = 1, c.sever = moon == kMoonFull;  // a blocked push still hurts (no abort)
    else if (kind == "gapThrust") c.armorIgnorePct = n.pct;
    else if (kind == "crushingBlow") c.crushPct = moon == kMoonFull ? n.pct2 : n.pct;
    return c;
  };
  // Estocada na Brecha (new moon), Acerto de Contas (waxing moon): the hex (his own: no step), then the foe beside it
  if ((kind == "gapThrust" && moon == kMoonNew) || (kind == "reckoning" && moon == kMoonWaxing)) {
    if (cells.size() < 2) return fail("Escolha o hex do passo (o seu, para ficar) e depois o inimigo.");
    const BattleHex to = cells[0];
    const bool stays = to == from;
    if (!stays && (hexDist(from, to) != 1 || !techniqueStandable(b, to))) return fail("Escolha primeiro um hex livre ao seu lado (ou o seu).");
    const int foe = techniqueFoe(b, to, cells[1], 1);
    if (foe < 0) return fail("Escolha um inimigo ao lado do hex escolhido.");
    commit();
    if (!stays) steps.push_back(techniqueMove(caster, kind, from, to));
    BattleStep c = blow(static_cast<size_t>(foe));
    c.chained = !stays;
    steps.push_back(c);
    return steps;
  }
  if (kind == "guardedCut" || kind == "wardenBash" || kind == "gapThrust" || kind == "crushingBlow" || kind == "reckoning") {
    const int foe = techniqueFoe(b, from, cell, 1);
    if (foe < 0) return fail(noFoe);
    std::optional<BattleHex> side;  // Corte Guardado under the waxing moon: the sidestep after a landed hit, still beside the foe
    if (kind == "guardedCut" && moon == kMoonWaxing) {
      if (cells.size() < 2) return fail("Escolha o inimigo e depois o hex do passo (o seu, para ficar).");
      if (!(cells[1] == from)) {
        if (hexDist(from, cells[1]) != 1 || !techniqueStandable(b, cells[1]) || !besideUnit(b, cells[1], static_cast<size_t>(foe), 1))
          return fail("Escolha um hex livre ao seu lado e ao lado do inimigo (ou o seu).");
        side = cells[1];
      }
    }
    commit();
    BattleStep c = blow(static_cast<size_t>(foe));
    if (side) c.abortOnMiss = true;  // the sidestep comes only after a landed hit
    steps.push_back(c);
    if (side) {
      BattleStep m = techniqueMove(caster, kind, from, *side);
      m.chained = true;
      steps.push_back(m);
    }
    return steps;
  }
  if (kind == "brace") {
    if (!(cell == from)) return fail("Esta técnica é em si mesmo.");
    commit();
    BattleStep s = techniqueEffectStep(caster, kind, from, n.pct);
    s.braceHits = moon == kMoonNew ? 3 : 2, s.bracePush = moon == kMoonWaning;
    steps.push_back(s);
    return steps;
  }
  if (kind == "interpose") {
    const int ally = wardAlly(b, caster, cell);
    if (ally < 0) return fail("Escolha um aliado ao seu lado.");
    commit();
    BattleStep s = techniqueEffectStep(caster, kind, cell, moon == kMoonWaning ? n.pct : 0);
    s.ids = {static_cast<size_t>(ally)};
    steps.push_back(s);
    return steps;
  }
  if (kind == "affront") {
    const int foe = techniqueFoe(b, from, cell, 3);
    if (foe < 0) return fail("Escolha um inimigo a até 3 hexes.");
    commit();
    BattleStep s = techniqueEffectStep(caster, kind, cell, n.pct);
    s.ids = {static_cast<size_t>(foe)}, s.wardChallengeSlow = moon == kMoonWaxing;
    steps.push_back(s);
    return steps;
  }
  if (kind == "holdTheLine") {
    std::vector<size_t> allies;
    for (const BattleHex& c : cells) {
      const int a = wardAlly(b, caster, c);
      if (a < 0) return fail("Escolha aliados ao seu lado.");
      if (std::find(allies.begin(), allies.end(), static_cast<size_t>(a)) != allies.end()) return fail("Escolha aliados diferentes.");
      allies.push_back(static_cast<size_t>(a));
    }
    commit();
    BattleStep s = techniqueEffectStep(caster, kind, from, n.pct);
    s.ids = allies, s.tiles = cells, s.holdRanged = moon == kMoonNew;
    steps.push_back(s);
    return steps;
  }
  if (kind == "unbreakable") {
    if (!wardLowHp(u)) return fail("Só com 35% da vida ou menos.");
    int ally = -1;
    if (!(cell == from)) {
      ally = moon == kMoonWaning ? wardAlly(b, caster, cell) : -1;
      if (ally < 0) return fail(moon == kMoonWaning ? "Escolha você mesmo ou um aliado ao lado." : "Esta técnica é em si mesmo.");
    }
    commit();
    BattleStep s = techniqueEffectStep(caster, kind, from, n.pct);
    s.barrierPct = n.pct;
    if (ally >= 0) s.ids = {static_cast<size_t>(ally)}, s.allyBarrierPct = n.pct2, s.tiles = {from, cell};
    steps.push_back(s);
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
  if (step.kind == "brace") {
    m.bracePct = step.bonus, m.braceHits = step.braceHits, m.bracePush = step.bracePush;
    out.push_back(statusEffect(step.att, std::format("{} -{}% ×{}", pick("Escudo erguido", "Shield raised"), step.bonus, step.braceHits)));
  } else if (step.kind == "interpose" && !step.ids.empty()) {
    BattleUnit::Conditions::Martial& a = b.units()[step.ids.front()].cond.martial;
    a.interposeBy = me, a.interposeCutPct = step.bonus;
    out.push_back(statusEffect(step.ids.front(), std::format("{} {}", pick("Coberto por", "Covered by"), g.name)));
  } else if (step.kind == "affront" && !step.ids.empty()) {
    BattleUnit::Conditions::Martial& f = b.units()[step.ids.front()].cond.martial;
    f.wardChallengePct = std::max(f.wardChallengeBy == me ? f.wardChallengePct : 0, step.bonus), f.wardChallengeBy = me;
    f.wardChallengeSlow = step.wardChallengeSlow;
    out.push_back(statusEffect(step.ids.front(), pick("Afrontado", "Challenged")));
  } else if (step.kind == "holdTheLine") {
    for (size_t id : step.ids) {
      BattleUnit::Conditions::Martial& a = b.units()[id].cond.martial;
      a.holdPct = step.bonus, a.holdBy = me, a.holdRanged = step.holdRanged;
      out.push_back(statusEffect(id, std::format("{} -{}%", pick("Segurar a Linha", "Hold the Line"), step.bonus)));
    }
  } else if (step.kind == "unbreakable") {
    auto lay = [&](size_t id, int pct) {
      BattleUnit& t = b.units()[id];
      const int hp = std::max(1, (t.maxHp * pct + 50) / 100);
      t.cond.martial.barrier = std::max(t.cond.martial.barrierBy == me ? t.cond.martial.barrier : 0, hp), t.cond.martial.barrierBy = me;
      out.push_back(statusEffect(id, std::format("{} {}", pick("Barreira", "Barrier"), t.cond.martial.barrier)));
    };
    lay(step.att, step.barrierPct);
    m.unbreakable = true;
    out.push_back(statusEffect(step.att, pick("Inquebrável", "Unbreakable")));
    for (size_t id : step.ids)
      if (step.allyBarrierPct > 0) lay(id, step.allyBarrierPct);
  }
  return true;
}

// --- The start of a unit's own turn -----------------------------------------------------------------------------------------------------
void wardTurnStart(Battle& b, size_t unit) {
  BattleUnit::Conditions::Martial& m = b.units()[unit].cond.martial;
  // his own: until his next turn begins
  m.wardCutPct = 0, m.bracePct = 0, m.braceHits = 0, m.bracePush = false, m.unbreakable = false;
  // what he did and suffered since the start of his previous turn: the turn before is forgotten, this one begins
  m.hitByPrev = std::move(m.hitBy), m.hitBy.clear();
  m.stoppedPrev = std::move(m.stopped), m.stopped.clear();
  // what he laid on others lasts until his next turn
  const int me = static_cast<int>(unit);
  for (BattleUnit& v : b.units()) {
    BattleUnit::Conditions::Martial& o = v.cond.martial;
    if (o.interposeBy == me) o.interposeBy = -1, o.interposeCutPct = 0;
    if (o.holdBy == me) o.holdBy = -1, o.holdPct = 0, o.holdRanged = false;
    if (o.barrierBy == me) o.barrierBy = -1, o.barrier = 0;
    if (o.wardChallengeBy == me) o.wardChallengeBy = -1, o.wardChallengePct = 0, o.wardChallengeSlow = false;
  }
  // on a foe: Golpe Esmagador through the end of its next full turn
  if (m.crushTurns > 0 && --m.crushTurns == 0) m.crushPct = 0, m.crushRound = -1, m.crushKind.clear();
}

// --- The strike (resolveStrikeImpl) -----------------------------------------------------------------------------------------------------
// Interpor: a single-target melee attack (a weapon's, not an arrow, a bolt or a staff's magic) about to start on a covered ally goes to
// its Guardião, before the roll (it can still miss), when he still stands beside the ally and within the attacker's reach.
bool wardRedirect(Battle& b, BattleStep& step, std::vector<BattleEffect>& out) {
  const size_t count = b.units().size();
  if (step.type != BattleStep::Type::Combat || step.att >= count || step.def >= count || step.interposedBy >= 0 || step.tiles.size() > 1) return false;
  const BattleUnit& a = b.units()[step.att];
  BattleUnit& ally = b.units()[step.def];
  BattleUnit::Conditions::Martial& m = ally.cond.martial;
  if (m.interposeBy < 0 || static_cast<size_t>(m.interposeBy) >= count || !a.alive || !ally.alive || a.side == ally.side) return false;
  if (a.combat.arrow || a.combat.arcane || a.combat.staff || !besideUnit(b, {a.x, a.y}, step.def, 1)) return false;
  const size_t g = static_cast<size_t>(m.interposeBy);
  const BattleUnit& w = b.units()[g];
  if (g == step.att || !w.alive || !besideUnit(b, {w.x, w.y}, step.def, 1) || !besideUnit(b, {a.x, a.y}, g, 1) || !targetable(b, static_cast<int>(g))) return false;
  step.def = g, step.interposedBy = static_cast<int>(g), step.interposeCutPct = m.interposeCutPct;
  m.interposeBy = -1, m.interposeCutPct = 0;  // spent by this attack
  wardNote(b.units()[g].cond.martial.stopped, static_cast<int>(step.att));
  out.push_back(statusEffect(g, pick("Interpor", "Interpose")));
  return true;
}
// Golpe Esmagador on the attacker, before its roll: the share it takes off (0: none). The first attack after the mark spends it; the
// same action's other hits of that skill (one kind, one round) are its too; anything later finds it gone.
int wardCrushShare(Battle& b, const BattleStep& step, bool counter, size_t ai) {
  BattleUnit::Conditions::Martial& m = b.units()[ai].cond.martial;
  if (counter || m.crushPct <= 0) return 0;
  if (m.crushRound < 0) {
    m.crushRound = b.round(), m.crushKind = step.kind;
    return m.crushPct;
  }
  if (m.crushRound == b.round() && !m.crushKind.empty() && step.kind == m.crushKind) return m.crushPct;
  m.crushPct = 0, m.crushTurns = 0, m.crushRound = -1, m.crushKind.clear();
  return 0;
}
// A landed physical weapon hit of `ai` on `ti`, after its mitigation and Kael's guards: Garran's reductions (Interpor's own on the hit it
// redirected, Corte Guardado, Erguer o Escudo, Afronta, Segurar a Linha), the strongest one alone, its charge alone spent.
int wardReduce(Battle& b, const BattleStep& step, bool counter, size_t ai, size_t ti, bool arrow, int dmg, std::vector<BattleEffect>& after) {
  BattleUnit &a = b.units()[ai], &t = b.units()[ti];
  if (a.side == t.side) return dmg;
  BattleUnit::Conditions::Martial &am = a.cond.martial, &tm = t.cond.martial;
  enum Source { None, Interposed, Cut, Brace, Challenge, Hold };
  int best = 0;
  Source source = None;
  auto offer = [&](int pct, Source s) {
    if (pct > best) best = pct, source = s;
  };
  const size_t count = b.units().size();
  if (!counter && step.interposedBy == static_cast<int>(ti) && step.interposeCutPct > 0) offer(step.interposeCutPct, Interposed);
  if (tm.wardCutPct > 0) offer(tm.wardCutPct, Cut);
  if (tm.braceHits > 0 && tm.bracePct > 0) offer(tm.bracePct, Brace);
  const int challenger = am.wardChallengeBy;
  if (am.wardChallengePct > 0 && challenger >= 0 && static_cast<size_t>(challenger) < count && (counter || step.tiles.size() <= 1)) {
    if (static_cast<size_t>(challenger) == ti) am.wardChallengePct = 0, am.wardChallengeBy = -1, am.wardChallengeSlow = false;  // spent on him, full damage
    else if (b.units()[static_cast<size_t>(challenger)].side == t.side) offer(am.wardChallengePct, Challenge);
  }
  if (tm.holdPct > 0 && tm.holdBy >= 0 && static_cast<size_t>(tm.holdBy) < count && b.units()[static_cast<size_t>(tm.holdBy)].alive &&
      besideUnit(b, {t.x, t.y}, static_cast<size_t>(tm.holdBy), 1) && (!arrow || tm.holdRanged))
    offer(tm.holdPct, Hold);
  if (source == None) return dmg;
  dmg = std::max(1, static_cast<int>(std::floor(dmg * (1 - best / 100.0))));
  std::string what;
  switch (source) {
    case Interposed: what = pick("Interpor", "Interpose"); break;
    case Cut:
      tm.wardCutPct = 0;
      what = pick("Corte Guardado", "Guarded Cut");
      break;
    case Brace:
      if (--tm.braceHits <= 0) tm.braceHits = 0, tm.bracePct = 0;
      what = pick("Escudo erguido", "Shield raised");
      break;
    case Challenge:
      wardNote(b.units()[static_cast<size_t>(challenger)].cond.martial.stopped, static_cast<int>(ai));
      if (am.wardChallengeSlow) {  // the waxing moon: its next voluntary step +1 AP (Corte Baixo's tax) through the end of its next full turn
        am.lowCutHexes = std::max(am.lowCutHexes, 1), am.lowCutTurns = 2;
        after.push_back(statusEffect(ai, pick("Passo pesado", "Heavy step")));
      }
      am.wardChallengePct = 0, am.wardChallengeBy = -1, am.wardChallengeSlow = false;
      what = pick("Afronta", "Challenge");
      break;
    case Hold:
      wardNote(b.units()[static_cast<size_t>(tm.holdBy)].cond.martial.stopped, static_cast<int>(ai));
      tm.holdPct = 0, tm.holdBy = -1, tm.holdRanged = false;
      what = pick("Segurar a Linha", "Hold the Line");
      break;
    case None: break;
  }
  after.push_back(statusEffect(ti, std::format("{} -{}%", what, best)));
  return dmg;
}
// A barrier takes the damage first, after every mitigation (Inquebrável): what is left of the blow.
int wardAbsorb(BattleUnit& t, size_t ti, int dmg, std::vector<BattleEffect>* after) {
  BattleUnit::Conditions::Martial& m = t.cond.martial;
  if (m.barrier <= 0 || dmg <= 0) return dmg;
  const int took = std::min(m.barrier, dmg);
  m.barrier -= took;
  if (m.barrier <= 0) m.barrier = 0, m.barrierBy = -1;
  if (after) after->push_back(statusEffect(ti, std::format("{} -{}", pick("Barreira", "Barrier"), took)));
  return dmg - took;
}
// Inquebrável: whoever stands under it is not stunned nor rooted anew by what resolves between wardImmunityBefore and wardImmunityAfter.
struct WardImmunity {
  size_t unit;
  bool stunned;
  size_t roots;
};
std::vector<WardImmunity> wardImmunityBefore(const Battle& b) {
  std::vector<WardImmunity> v;
  for (size_t i = 0; i < b.units().size(); ++i) {
    const BattleUnit& u = b.units()[i];
    if (u.cond.martial.unbreakable) v.push_back({i, u.stunned, u.cond.burialRootSources.size()});
  }
  return v;
}
void wardImmunityAfter(Battle& b, const std::vector<WardImmunity>& before, std::vector<BattleEffect>& out) {
  for (const WardImmunity& w : before) {
    BattleUnit& u = b.units()[w.unit];
    bool resisted = false;
    if (!w.stunned && u.stunned) u.stunned = false, u.stunTurns = 0, resisted = true;
    if (u.cond.burialRootSources.size() > w.roots) u.cond.burialRootSources.resize(w.roots), resisted = true;
    if (resisted && u.alive) out.push_back(statusEffect(w.unit, pick("Inquebrável: resiste", "Unbreakable: resists")));
  }
}

// --- Words --------------------------------------------------------------------------------------------------------------------------------
void wardConditions(const BattleUnit& u, const std::function<void(const std::string&)>& add) {
  const BattleUnit::Conditions::Martial& m = u.cond.martial;
  if (m.wardCutPct > 0) add(std::format("{} -{}%", pick("Corte Guardado", "Guarded Cut"), m.wardCutPct));
  if (m.braceHits > 0) add(std::format("{} -{}% ×{}", pick("Escudo erguido", "Shield raised"), m.bracePct, m.braceHits));
  if (m.bracePush) add(pick("Firme contra empurrões", "Braced against pushes"));
  if (m.unbreakable) add(pick("Inquebrável", "Unbreakable"));
  if (m.barrier > 0) add(std::format("{} {}", pick("Barreira", "Barrier"), m.barrier));
  if (m.interposeBy >= 0) add(m.interposeCutPct > 0 ? std::format("{} -{}%", pick("Coberto (Interpor)", "Covered (Interpose)"), m.interposeCutPct)
                                                     : pick("Coberto (Interpor)", "Covered (Interpose)"));
  if (m.holdPct > 0) add(std::format("{} -{}%", pick("Segurar a Linha", "Hold the Line"), m.holdPct));
  if (m.wardChallengePct > 0) add(std::format("{} -{}%", pick("Afrontado", "Challenged"), m.wardChallengePct));
  if (m.crushPct > 0) add(std::format("{} -{}%", pick("Esmagado", "Crushed"), m.crushPct));
}
// The technique in words at his rank, and what tonight's moon does to it (moon < 0: every moon that changes it).
std::string wardTooltipText(const std::string& k, const TechNumbers& n, int moon) {
  const auto pct = [](int v) { return std::format("{}%", v); };
  std::string t;
  std::vector<std::pair<int, std::string>> moons;  // the moons that change it, and how
  if (k == "guardedCut") {
    t = std::format("inimigo ao lado · {} · acertou: o próximo golpe físico de arma em você -{} · até o seu próximo turno", pct(n.dmg), pct(n.pct));
    moons = {{kMoonWaxing, "acertou: um passo para um hex livre, ainda ao lado do mesmo inimigo"},
             {kMoonFull, "+20 pontos contra um inimigo que acertou você com arma desde o início do seu turno anterior"}};
  } else if (k == "brace") {
    t = std::format("os próximos 2 golpes físicos de arma em você -{} cada · até o seu próximo turno", pct(n.pct));
    moons = {{kMoonNew, "cobre 3 golpes"}, {kMoonWaning, "o primeiro empurrão contra você anda 1 hex a menos"}};
  } else if (k == "wardenBash") {
    t = std::format("inimigo ao lado · {} · empurra 1 hex (bloqueado, o dano vem mesmo assim)", pct(n.dmg));
    moons = {{kMoonFull, "o próximo contra-ataque dele não vem, o deste golpe incluído, até o fim do próximo turno dele"}};
  } else if (k == "interpose") {
    t = "um aliado ao lado · o primeiro ataque corpo a corpo de alvo único contra ele vem para você, se você ainda estiver ao lado dele e ao "
        "alcance do atacante (rola contra as suas defesas, pode errar) · até o seu próximo turno";
    moons = {{kMoonWaning, std::format("esse golpe -{}", pct(n.pct))}};
  } else if (k == "gapThrust") {
    t = std::format("inimigo ao lado · {} · ignora {} da armadura", pct(n.dmg), pct(n.pct));
    moons = {{kMoonNew, "alcança um inimigo a 2 hexes: primeiro um passo para um hex livre ao lado dele"}};
  } else if (k == "affront") {
    t = std::format("inimigo a até 3 hexes · o primeiro golpe de arma de alvo único dele em outro do seu grupo -{} de dano; em você, dano cheio · "
                    "até o seu próximo turno", pct(n.pct));
    moons = {{kMoonWaxing, "quando reduzir um golpe, o próximo passo dele custa +1 PA até o fim do próximo turno dele"}};
  } else if (k == "crushingBlow") {
    t = std::format("inimigo ao lado · {} · acertou: o próximo ataque de arma dele -{} de dano em todos os alvos, até o fim do próximo turno dele",
                    pct(n.dmg), pct(n.pct));
    moons = {{kMoonFull, std::format("-{} em vez de -{}", pct(n.pct2), pct(n.pct))}};
  } else if (k == "holdTheLine") {
    t = std::format("até 2 aliados ao lado · o próximo golpe corpo a corpo de arma em cada um -{}, se ainda estiver ao seu lado · até o seu próximo turno",
                    pct(n.pct));
    moons = {{kMoonNew, "golpes de arma à distância também"}};
  } else if (k == "reckoning") {
    t = std::format("inimigo ao lado · {} · +{} pontos se, desde o início do seu turno anterior, você tomou o ataque dele (Interpor) ou o reduziu "
                    "num aliado (Afronta, Segurar a Linha) · máximo 210%", pct(n.dmg), n.pct);
    moons = {{kMoonWaxing, "um passo antes do golpe, para alcançar quem você deteve"}};
  } else if (k == "unbreakable") {
    t = std::format("só com 35% da vida ou menos · barreira de {} da sua vida máxima (absorve depois da mitigação) e imune a novo "
                    "atordoamento e enraizamento · até o seu próximo turno", pct(n.pct));
    moons = {{kMoonWaning, std::format("um aliado ao lado escolhido ganha barreira de {} da vida máxima dele", pct(n.pct2))}};
  }
  if (moon >= 0) {
    std::string now = std::format("{} (agora): ", wardMoonName(moon));
    bool changes = false;
    for (const auto& [m, what] : moons)
      if (m == moon) now += what, changes = true;
    if (!changes) now += "sem mudança";
    t += "\n" + now;
  } else {
    for (const auto& [m, what] : moons) t += std::format("\n{}: {}", wardMoonName(m), what);
  }
  return t;
}
