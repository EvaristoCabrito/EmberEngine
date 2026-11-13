// Kael's techniques: Vanguarda, then Mestre da Espada or Arauto da Guerra at 15 (docs/SKILLS_VANGUARD.md). Included by Spells.cpp
// inside its unnamed namespace, after SpellsCore.inc: it uses the same board helpers, steps and strikes as every other skill.
//
// The user, 2026-11-13 (the design "Kael's martial skills", then the answers): every technique costs its AP (content/data/action-points.json
// spells.byKind, the design's), one use of its tier and starts its cooldown; it grows one rank a level from the level it is learned (rank 1)
// to 14 levels later (rank 15), every number of it a little at each rank (t = (rank - 1) / 14, rounded half up); Kael keeps Golpe Duplo of
// the Guerreiro's skills and nothing else. Damage is a share of an ordinary blow of his sword (BattleStep::dmgMul); every hit rolls on its
// own through the one strike (resolveStrikeImpl): accuracy, defence, counters, the guards and the marks below. No test-mode branch.

enum class TechAim {
  Foe,      // an adjacent foe
  Self,     // on himself (the area is his own hex, Confirmar)
  StepFoe,  // an empty adjacent hex, then a foe beside it (Passo Longo)
  FoeStep,  // an adjacent foe, then a hex (Corte de Saída, Passo de Passagem, Desvio Lateral)
  Foe2,     // a foe within 2 hexes (Desafio)
  Far2,     // a foe 2 hexes away on a straight line, the hex between empty (Ombrada)
  Arc,      // an adjacent hex: the front arc it centres (it and its two neighbours beside Kael)
  Line,     // an adjacent foe: the straight line through it
  Passive,  // works by itself (Riposta)
};
struct TechniqueDef {
  const char* kind;
  const char* en;  // the English name (the Portuguese one is kEmberSpells')
  int cooldown;    // turns of his blocked after a use (0 = none)
  TechAim aim;
};
constexpr TechniqueDef kTechniques[] = {
    // Vanguarda
    {"measuredCut", "Measured Cut", 0, TechAim::Foe},
    {"longstepSlash", "Longstep Slash", 0, TechAim::StepFoe},
    {"crossguard", "Crossguard", 0, TechAim::Self},
    {"pommelCheck", "Pommel Check", 1, TechAim::Foe},
    {"lowCut", "Low Cut", 0, TechAim::Foe},
    {"partingCut", "Parting Cut", 0, TechAim::FoeStep},
    {"overheadHew", "Overhead Hew", 0, TechAim::Foe},
    {"steadyFooting", "Steady Footing", 1, TechAim::Self},
    {"challenge", "Challenge", 1, TechAim::Foe2},
    {"breakGuard", "Break Guard", 1, TechAim::Foe},
    // Mestre da Espada
    {"feint", "Feint", 0, TechAim::Foe},
    {"riposte", "Riposte", 0, TechAim::Passive},
    {"passingStep", "Passing Step", 0, TechAim::FoeStep},
    {"needlePoint", "Needle Point", 0, TechAim::Foe},
    {"secondIntention", "Second Intention", 0, TechAim::Foe},
    {"readTheBlade", "Read the Blade", 1, TechAim::Foe},
    {"severTheRhythm", "Sever the Rhythm", 2, TechAim::Foe},
    {"perfectCut", "Perfect Cut", 1, TechAim::Foe},
    {"deadMeasure", "Dead Measure", 2, TechAim::Foe},
    {"masterstroke", "Masterstroke", 3, TechAim::Foe},
    // Arauto da Guerra
    {"drivingCut", "Driving Cut", 0, TechAim::Foe},
    {"wideCleave", "Wide Cleave", 0, TechAim::Arc},
    {"sidecast", "Sidecast", 0, TechAim::FoeStep},
    {"plantTheLine", "Plant the Line", 1, TechAim::Self},
    {"shoulderThrough", "Shoulder Through", 1, TechAim::Far2},
    {"crushAgainstStone", "Crush Against Stone", 1, TechAim::Foe},
    {"splitTheRank", "Split the Rank", 1, TechAim::Line},
    {"reapTheFlanks", "Reap the Flanks", 1, TechAim::Arc},
    {"rollingAdvance", "Rolling Advance", 2, TechAim::Line},
    {"shatterTheLine", "Shatter the Line", 3, TechAim::Arc},
    // Guardião da Lua (Garran's, docs/SKILLS_MOONWARDEN.md: SpellsMoonwarden.inc aims and casts them, the moon read as they commit)
    {"guardedCut", "Guarded Cut", 0, TechAim::Foe},
    {"brace", "Brace", 1, TechAim::Self},
    {"wardenBash", "Shield Bash", 1, TechAim::Foe},
    {"interpose", "Interpose", 1, TechAim::Foe},
    {"gapThrust", "Gap Thrust", 1, TechAim::Foe},
    {"affront", "Challenge", 2, TechAim::Foe2},
    {"crushingBlow", "Crushing Blow", 2, TechAim::Foe},
    {"holdTheLine", "Hold the Line", 3, TechAim::Foe},
    {"reckoning", "Reckoning", 3, TechAim::Foe},
    {"unbreakable", "Unbreakable", 4, TechAim::Self},
};
const TechniqueDef* techniqueDef(const std::string& kind) {
  for (const TechniqueDef& t : kTechniques)
    if (kind == t.kind) return &t;
  return nullptr;
}
bool techniqueKind(const std::string& kind) { return techniqueDef(kind) != nullptr; }

// --- Ranks -----------------------------------------------------------------------------------------------------------------------------
constexpr int kTechniqueRanks = 15;
// 0 .. 14: the levels since it was learned (rank - 1).
int techniqueStep(const BattleUnit& u, const std::string& kind) {
  const EmberSpell* e = emberSpell(kind);
  return std::clamp(u.level - (e ? e->learn : 1), 0, kTechniqueRanks - 1);
}
// first -> max at this step, rounded half up (integer arithmetic: a half is never lost to floating error)
int rankValue(int first, int max, int step) {
  const int num = (max - first) * step, den = kTechniqueRanks - 1;
  return first + (num >= 0 ? (2 * num + den) / (2 * den) : -((-2 * num + den - 1) / (2 * den)));
}
// The technique's numbers at a step. dmg: % of an ordinary blow (the first or only hit); dmg2: the second hit / the flanks; pct, pct2:
// its other numbers (points to hit, % of armor ignored, % of a guard ...), as each kind below says; hexes: Corte Baixo's taxed hexes.
struct TechNumbers {
  int dmg = 0, dmg2 = 0, pct = 0, pct2 = 0, hexes = 0;
};
TechNumbers techniqueNumbersAt(const std::string& k, int s) {
  TechNumbers n;
  auto v = [&](int a, int z) { return rankValue(a, z, s); };
  // Vanguarda
  if (k == "measuredCut") n.dmg = v(100, 115), n.pct = v(5, 15);             // pct: points to hit
  else if (k == "longstepSlash") n.dmg = v(90, 115);
  else if (k == "crossguard") n.pct = v(30, 55);                             // pct: the parried hit's damage down
  else if (k == "pommelCheck") n.dmg = v(45, 65), n.pct = v(10, 25);         // pct: its next weapon attack's points off
  else if (k == "lowCut") n.dmg = v(80, 105), n.hexes = 2 * s >= kTechniqueRanks - 1 ? 2 : 1;  // the second hex once t >= 0.5
  else if (k == "partingCut") n.dmg = v(90, 115);
  else if (k == "overheadHew") n.dmg = v(120, 145), n.pct = v(10, 25);       // pct: armor ignored
  else if (k == "steadyFooting") n.pct = v(10, 20);                          // pct: physical damage down
  else if (k == "challenge") n.pct = v(10, 25);                              // pct: points off on Kael's allies
  else if (k == "breakGuard") n.dmg = v(105, 130), n.pct = v(15, 30);        // pct: the next landed physical hit's armor ignored
  // Mestre da Espada
  else if (k == "feint") n.pct = v(10, 25);                                  // pct: Exposto
  else if (k == "riposte") n.dmg = v(100, 130), n.pct = 10;                  // pct: Exposto (fixed)
  else if (k == "passingStep") n.dmg = v(90, 115);
  else if (k == "needlePoint") n.dmg = v(90, 110), n.pct = v(30, 50);        // pct: armor ignored
  else if (k == "secondIntention") n.dmg = v(50, 65), n.pct = 20, n.pct2 = 20;  // each hit; the second, after a parry: +20 to hit, 20 % armor
  else if (k == "readTheBlade") n.pct = v(15, 35), n.pct2 = 15;              // pct: the parried hit's damage down; pct2: Exposto (fixed)
  else if (k == "severTheRhythm") n.dmg = v(95, 125);
  else if (k == "perfectCut") n.dmg = v(115, 145), n.pct = v(35, 45);        // pct: points of the blow more against Exposto
  else if (k == "deadMeasure") n.dmg = v(105, 135);                          // the reaction's blow
  else if (k == "masterstroke") n.dmg = v(145, 180), n.pct = v(25, 40), n.pct2 = 20;  // pct: armor ignored; pct2: to hit against Exposto
  // Arauto da Guerra
  else if (k == "drivingCut") n.dmg = v(90, 115);
  else if (k == "wideCleave") n.dmg = v(70, 95);
  else if (k == "sidecast") n.dmg = v(75, 100);
  else if (k == "plantTheLine") n.dmg = v(80, 110);                          // the reaction's blow
  else if (k == "shoulderThrough") n.dmg = v(85, 110);
  else if (k == "crushAgainstStone") n.dmg = v(110, 135), n.pct = v(30, 50); // pct: the wall's impact
  else if (k == "splitTheRank") n.dmg = v(95, 120), n.dmg2 = v(65, 90);
  else if (k == "reapTheFlanks") n.dmg = v(80, 105);
  else if (k == "rollingAdvance") n.dmg = v(90, 115), n.dmg2 = v(70, 95);
  else if (k == "shatterTheLine") n.dmg = v(130, 160), n.dmg2 = v(85, 110), n.pct = v(10, 20);  // pct: the flanks' next attack points off
  // Guardião da Lua (docs/SKILLS_MOONWARDEN.md)
  else if (k == "guardedCut") n.dmg = v(100, 120), n.pct = v(10, 15);        // pct: his next physical weapon hit taken, % off
  else if (k == "brace") n.pct = v(25, 40);                                  // pct: each braced hit, % off
  else if (k == "wardenBash") n.dmg = v(60, 85);
  else if (k == "interpose") n.pct = v(20, 30);                              // pct: the redirected hit's % off (waning moon)
  else if (k == "gapThrust") n.dmg = v(105, 130), n.pct = v(20, 35);         // pct: armor ignored
  else if (k == "affront") n.pct = v(25, 40);                                // pct: its hit on his party, % off
  else if (k == "crushingBlow") n.dmg = v(120, 150), n.pct = v(20, 30), n.pct2 = v(30, 40);  // pct: its next attack's damage off; pct2: the full moon's
  else if (k == "holdTheLine") n.pct = v(20, 35);                            // pct: each ally's next melee hit, % off
  else if (k == "reckoning") n.dmg = v(130, 160), n.pct = v(35, 50);         // pct: points more on a foe he stopped (210 % at most)
  else if (k == "unbreakable") n.pct = v(20, 30), n.pct2 = v(10, 15);        // pct: his barrier, % of max HP; pct2: the ally's (waning moon)
  return n;
}
TechNumbers techniqueNumbers(const std::string& kind, const BattleUnit& u) { return techniqueNumbersAt(kind, techniqueStep(u, kind)); }

// --- Cooldowns: used in round R, cooldown N -> usable again from round R + N + 1 (he acts once a round) ---------------------------
// The turns of his still blocked: the next ones after the turn it was used in, this one included on a later turn.
int techniqueCooldownLeft(const Battle& b, const BattleUnit& u, const std::string& kind) {
  for (const BattleUnit::Cooldown& c : u.cooldowns)
    if (c.kind == kind && b.round() < c.ready) return c.used == b.round() ? c.ready - b.round() - 1 : c.ready - b.round();
  return 0;
}
void techniqueStartCooldown(const Battle& b, BattleUnit& u, const std::string& kind) {
  const TechniqueDef* d = techniqueDef(kind);
  if (!d || d->cooldown <= 0) return;
  std::erase_if(u.cooldowns, [&](const BattleUnit::Cooldown& c) { return c.kind == kind; });
  u.cooldowns.push_back({kind, b.round(), b.round() + d->cooldown + 1});
}
bool techniqueCooling(const Battle& b, const BattleUnit& u, const std::string& kind) {
  for (const BattleUnit::Cooldown& c : u.cooldowns)
    if (c.kind == kind && b.round() < c.ready) return true;
  return false;
}

// --- The board (hexBeyond: the Lanceiro's, the hex past `through` on the straight line from `from`) --------------------------------------
// Ground a body may stand on now: on the board, passable, no decoration in the way, no one there.
bool techniqueStandable(const Battle& b, BattleHex h) {
  if (!b.inBounds(h.x, h.y)) return false;
  const BattleCell& c = b.cell(h.x, h.y);
  if (!c.passable || c.blocked || c.terrain == "void" || c.terrain == "water" || c.terrain == "barricade") return false;
  return b.unitAt(h.x, h.y) < 0;
}
// A solid wall or an impassable feature of the map (the edge of the board too): Esmagar na Pedra's stone. Water and bodies are not.
bool techniqueWall(const Battle& b, BattleHex h) {
  if (!b.inBounds(h.x, h.y)) return true;
  const BattleCell& c = b.cell(h.x, h.y);
  return c.terrain != "water" && (!c.passable || c.blocked || c.terrain == "void" || c.terrain == "barricade");
}
// Moved by force or stopped mid-walk: a one-hex body (the board's displacement rule, pushAway's own).
bool techniqueDisplaceable(const BattleUnit& u) { return u.size <= 1 && !u.footprintOffsets; }
// The front arc centred on the adjacent hex `centre`: it first, then its two neighbours that are also Kael's.
std::vector<BattleHex> frontArc(const Battle& b, BattleHex from, BattleHex centre) {
  std::vector<BattleHex> arc{centre};
  for (const BattleHex& n : Battle::neighbors(from.x, from.y))
    if (b.inBounds(n.x, n.y) && !(n == centre) && hexDist(n, centre) == 1) arc.push_back(n);
  return arc;
}
// A visible foe on the hex within `reach` hexes of `from` (any hex of a big body), or -1.
int techniqueFoe(const Battle& b, BattleHex from, BattleHex cell, int reach) {
  const int foe = occAt(b, cell);
  return foe >= 0 && targetable(b, foe) && besideUnit(b, from, static_cast<size_t>(foe), reach) ? foe : -1;
}
// The empty hex between Kael and a foe two hexes away on a straight line (Ombrada), if there is one.
std::optional<BattleHex> shoulderMiddle(const Battle& b, BattleHex from, BattleHex cell) {
  if (hexDist(from, cell) != 2) return std::nullopt;
  for (const BattleHex& n : Battle::neighbors(from.x, from.y))
    if (b.inBounds(n.x, n.y) && hexBeyond(from, n) == cell) return techniqueStandable(b, n) ? std::optional<BattleHex>(n) : std::nullopt;
  return std::nullopt;
}

// --- Steps ------------------------------------------------------------------------------------------------------------------------------
BattleStep techniqueBlow(size_t caster, size_t foe, const std::string& kind, int pct) {
  BattleStep s;
  s.type = BattleStep::Type::Combat, s.att = caster, s.def = foe, s.kind = kind;
  s.dice = 0, s.spellMul = 1, s.dmgMul = pct / 100.0;
  return s;
}
BattleStep techniqueMove(size_t caster, const std::string& kind, BattleHex from, BattleHex to) {
  BattleStep s;
  s.type = BattleStep::Type::Move, s.att = caster, s.kind = kind, s.path = {from, to}, s.martialMove = true;
  return s;
}
// A guard, a stance, a mark: Type::Effect, its number in `bonus` (and `exposePct`), its foe in `ids`.
BattleStep techniqueEffectStep(size_t caster, const std::string& kind, BattleHex at, int pct) {
  BattleStep s;
  s.type = BattleStep::Type::Effect, s.att = caster, s.kind = kind, s.tiles = {at}, s.dice = 0, s.spellMul = 1, s.bonus = pct;
  return s;
}

#include "SpellsMoonwarden.inc"  // Garran's techniques (docs/SKILLS_MOONWARDEN.md): their aims, casts, covers and the moon

std::optional<std::vector<BattleStep>> castTechnique(Battle& b, size_t caster, const std::string& kind, const std::vector<BattleHex>& cells, std::string* error) {
  BattleUnit& u = b.units()[caster];
  auto fail = [&](const std::string& why) -> std::optional<std::vector<BattleStep>> {
    if (error) *error = why;
    return std::nullopt;
  };
  const TechniqueDef* d = techniqueDef(kind);
  if (!d) return fail("Esta técnica não existe.");
  if (wardKind(kind)) return castWarden(b, caster, kind, cells, error);  // Garran's
  if (d->aim == TechAim::Passive) return fail(pick("Funciona sozinha.", "It works by itself."));
  if (cells.empty()) return fail("Escolha um alvo.");
  const BattleHex from{u.x, u.y}, cell = cells[0];
  const TechNumbers n = techniqueNumbers(kind, u);
  std::vector<BattleStep> steps;
  auto commit = [&] {
    spendTier(u, kind);
    techniqueStartCooldown(b, u, kind);
  };
  const char* noFoe = "Escolha um inimigo ao lado.";
  switch (d->aim) {
    case TechAim::Self: {
      if (!(cell == from)) return fail("Esta técnica é em si mesmo.");
      const int pct = kind == "plantTheLine" ? n.dmg : n.pct;
      commit();
      steps.push_back(techniqueEffectStep(caster, kind, from, pct));
      return steps;
    }
    case TechAim::Foe2: {  // Desafio
      const int foe = techniqueFoe(b, from, cell, 2);
      if (foe < 0) return fail("Escolha um inimigo a até 2 hexes.");
      commit();
      BattleStep s = techniqueEffectStep(caster, kind, cell, n.pct);
      s.ids = {static_cast<size_t>(foe)};
      steps.push_back(s);
      return steps;
    }
    case TechAim::StepFoe: {  // Passo Longo: the hex, then the foe beside it
      if (cells.size() < 2) return fail("Escolha o hex e depois o inimigo.");
      const BattleHex to = cells[0];
      if (hexDist(from, to) != 1 || !techniqueStandable(b, to)) return fail("Escolha primeiro um hex livre ao lado.");
      const int foe = techniqueFoe(b, to, cells[1], 1);
      if (foe < 0) return fail("Escolha um inimigo ao lado do hex escolhido.");
      commit();
      steps.push_back(techniqueMove(caster, kind, from, to));
      BattleStep c = techniqueBlow(caster, static_cast<size_t>(foe), kind, n.dmg);
      c.chained = true;
      steps.push_back(c);
      return steps;
    }
    case TechAim::FoeStep: {  // the foe, then the hex
      if (cells.size() < 2) return fail("Escolha o inimigo e depois o hex.");
      const int foe = techniqueFoe(b, from, cells[0], 1);
      if (foe < 0) return fail(noFoe);
      const BattleUnit& f = b.units()[static_cast<size_t>(foe)];
      const BattleHex to = cells[1];
      if (!techniqueStandable(b, to)) return fail("Escolha um hex livre.");
      if (kind == "partingCut") {  // a hex beside Kael, out of the foe's reach
        if (hexDist(from, to) != 1 || besideUnit(b, to, static_cast<size_t>(foe), 1)) return fail("Escolha um hex ao seu lado e longe do inimigo.");
        commit();
        steps.push_back(techniqueBlow(caster, static_cast<size_t>(foe), kind, n.dmg));
        steps.push_back(techniqueMove(caster, kind, from, to));  // the step comes on a miss too
        return steps;
      }
      if (kind == "passingStep") {  // a hex beside both: circle into it, then the blow
        if (hexDist(from, to) != 1 || !besideUnit(b, to, static_cast<size_t>(foe), 1)) return fail("Escolha um hex ao seu lado e ao lado do inimigo.");
        commit();
        steps.push_back(techniqueMove(caster, kind, from, to));
        BattleStep c = techniqueBlow(caster, static_cast<size_t>(foe), kind, n.dmg);
        c.chained = true;
        steps.push_back(c);
        return steps;
      }
      // Desvio Lateral: the foe goes to a hex beside both Kael and its own starting hex
      if (hexDist(from, to) != 1 || hexDist({f.x, f.y}, to) != 1) return fail("Escolha um hex ao seu lado e ao lado do inimigo.");
      commit();
      BattleStep c = techniqueBlow(caster, static_cast<size_t>(foe), kind, n.dmg);
      c.moveFoeTo = to;
      steps.push_back(c);
      return steps;
    }
    case TechAim::Far2: {  // Ombrada
      const int foe = occAt(b, cell);
      if (foe < 0 || !targetable(b, foe)) return fail("Escolha um inimigo a 2 hexes em linha reta.");
      const std::optional<BattleHex> middle = shoulderMiddle(b, from, cell);
      if (!middle) return fail("O inimigo precisa estar a 2 hexes em linha reta, com o hex do meio livre.");
      commit();
      steps.push_back(techniqueMove(caster, kind, from, *middle));
      BattleStep c = techniqueBlow(caster, static_cast<size_t>(foe), kind, n.dmg);
      c.chained = true, c.straightPush = 1;
      steps.push_back(c);
      return steps;
    }
    case TechAim::Arc: {
      if (hexDist(from, cell) != 1) return fail("Escolha um hex ao seu lado.");
      const std::vector<BattleHex> arc = frontArc(b, from, cell);
      std::vector<std::pair<size_t, bool>> hit;  // foe, at the centre
      for (size_t k = 0; k < arc.size(); ++k) {
        if (kind == "reapTheFlanks" && k == 0) continue;  // only the two outer hexes
        const int foe = occAt(b, arc[k]);
        if (foe < 0 || !targetable(b, foe)) continue;
        if (std::none_of(hit.begin(), hit.end(), [&](const auto& h) { return h.first == static_cast<size_t>(foe); })) hit.push_back({static_cast<size_t>(foe), k == 0});
      }
      if (hit.empty()) return fail("Não há inimigo nesse arco.");
      commit();
      for (const auto& [foe, centre] : hit) {
        BattleStep c = techniqueBlow(caster, foe, kind, kind == "shatterTheLine" && !centre ? n.dmg2 : n.dmg);
        c.noCounter = true, c.tiles = arc;
        if (kind == "reapTheFlanks") c.straightPush = 1;
        if (kind == "shatterTheLine") {
          if (centre) c.straightPush = 1;
          else c.accDownPct = n.pct;
        }
        steps.push_back(c);
      }
      return steps;
    }
    case TechAim::Line: {
      const int foe = techniqueFoe(b, from, cell, 1);
      if (foe < 0 || hexDist(from, cell) != 1) return fail(noFoe);
      commit();
      if (kind == "splitTheRank") {  // the adjacent foe; if it lands, the one on the second hex (Kael stays)
        BattleStep c = techniqueBlow(caster, static_cast<size_t>(foe), kind, n.dmg);
        c.noCounter = true, c.abortOnMiss = true;
        steps.push_back(c);
        const int second = occAt(b, hexBeyond(from, cell));
        if (second >= 0 && second != foe && targetable(b, second)) {
          BattleStep c2 = techniqueBlow(caster, static_cast<size_t>(second), kind, n.dmg2);
          c2.noCounter = true, c2.chained = true;
          steps.push_back(c2);
        }
        return steps;
      }
      // Avanço Rolante: blow and push, follow into the vacated hex, blow and push again; Kael ends on the first vacated hex
      BattleStep c = techniqueBlow(caster, static_cast<size_t>(foe), kind, n.dmg);
      c.straightPush = 1, c.abortOnNoPush = true;
      steps.push_back(c);
      BattleStep m = techniqueMove(caster, kind, from, cell);
      m.chained = true;
      steps.push_back(m);
      BattleStep c2 = techniqueBlow(caster, static_cast<size_t>(foe), kind, n.dmg2);
      c2.chained = true, c2.straightPush = 1;
      steps.push_back(c2);
      return steps;
    }
    case TechAim::Foe: {
      const int foe = techniqueFoe(b, from, cell, 1);
      if (foe < 0) return fail(noFoe);
      const size_t f = static_cast<size_t>(foe);
      // the marks and guards on one foe: Type::Effect, no blow
      if (kind == "feint" || kind == "readTheBlade" || kind == "deadMeasure") {
        commit();
        BattleStep s = techniqueEffectStep(caster, kind, cell, kind == "deadMeasure" ? n.dmg : n.pct);
        s.ids = {f};
        if (kind == "feint") s.exposePct = n.pct;
        if (kind == "readTheBlade") s.exposePct = n.pct2;
        steps.push_back(s);
        return steps;
      }
      BattleStep c = techniqueBlow(caster, f, kind, n.dmg);
      if (kind == "measuredCut") c.accBonus = n.pct;
      else if (kind == "pommelCheck") c.accDownPct = n.pct;
      else if (kind == "lowCut") c.lowCutHexes = n.hexes;
      else if (kind == "overheadHew" || kind == "needlePoint") c.armorIgnorePct = n.pct;
      else if (kind == "breakGuard") c.openPct = n.pct;
      else if (kind == "severTheRhythm") c.sever = true;
      else if (kind == "perfectCut") c.exposedDamagePct = n.pct;
      else if (kind == "masterstroke") c.armorIgnorePct = n.pct, c.exposedAccBonus = n.pct2;
      else if (kind == "crushAgainstStone") c.straightPush = 1, c.wallImpactPct = n.pct;
      else if (kind == "drivingCut") c.straightPush = 1, c.abortOnNoPush = true;
      commit();
      if (kind == "secondIntention") {  // two cuts: the first draws the guard (no counter), the second may answer a parry
        BattleStep first = c;
        first.noCounter = true;
        steps.push_back(first);
        c.parriedAccBonus = n.pct, c.parriedIgnorePct = n.pct2;
        steps.push_back(c);
        return steps;
      }
      steps.push_back(c);
      if (kind == "drivingCut") {  // the push landed: Kael follows into the hex it left
        BattleStep m = techniqueMove(caster, kind, from, cell);
        m.chained = true;
        steps.push_back(m);
      }
      return steps;
    }
    case TechAim::Passive: break;
  }
  return fail("Esta técnica não pode ser usada assim.");
}

// --- The aim (spellZone / spellAimValid / spellArea / spellTargetCount) -------------------------------------------------------------
int techniqueTargetCount(const Battle& b, size_t unit, const std::string& kind) {
  if (wardKind(kind)) return wardTargetCount(b, unit, kind);  // Garran's: the moon's steps, Segurar a Linha's allies
  const TechniqueDef* d = techniqueDef(kind);
  return d && (d->aim == TechAim::StepFoe || d->aim == TechAim::FoeStep) ? 2 : 1;
}
int techniqueReach(const Battle& b, const std::string& kind) {
  if (wardKind(kind)) return wardReach(b, kind);
  const TechniqueDef* d = techniqueDef(kind);
  if (!d) return 1;
  return d->aim == TechAim::Self ? 0 : d->aim == TechAim::Foe2 || d->aim == TechAim::Far2 ? 2 : d->aim == TechAim::StepFoe ? 2 : 1;
}
bool techniqueAimValid(const Battle& b, size_t caster, const std::string& kind, BattleHex cell, std::string* error) {
  const BattleUnit& u = b.units()[caster];
  const BattleHex from{u.x, u.y};
  auto fail = [&](const char* why) {
    if (error) *error = why;
    return false;
  };
  const TechniqueDef* d = techniqueDef(kind);
  if (!d || d->aim == TechAim::Passive) return fail("Funciona sozinha.");
  if (wardKind(kind)) return wardAimValid(b, caster, kind, cell, error);  // Garran's
  switch (d->aim) {
    case TechAim::Self: return cell == from ? true : fail("Esta técnica é em si mesmo.");
    case TechAim::Foe:
    case TechAim::Line: return techniqueFoe(b, from, cell, 1) >= 0 ? true : fail("Escolha um inimigo ao lado.");
    case TechAim::Foe2: return techniqueFoe(b, from, cell, 2) >= 0 ? true : fail("Escolha um inimigo a até 2 hexes.");
    case TechAim::Far2:
      return occAt(b, cell) >= 0 && targetable(b, occAt(b, cell)) && shoulderMiddle(b, from, cell) ? true
                                                                                                   : fail("Um inimigo a 2 hexes em linha reta, com o hex do meio livre.");
    case TechAim::Arc: return hexDist(from, cell) == 1 ? true : fail("Escolha um hex ao seu lado.");
    case TechAim::StepFoe:  // the empty hex beside him, or the foe beside it (the cast checks the pair)
      if (hexDist(from, cell) == 1 && techniqueStandable(b, cell)) return true;
      return techniqueFoe(b, from, cell, 2) >= 0 ? true : fail("Escolha um hex livre ao lado, depois um inimigo ao lado dele.");
    case TechAim::FoeStep:
      if (techniqueFoe(b, from, cell, 1) >= 0) return true;
      return hexDist(from, cell) <= 2 && techniqueStandable(b, cell) ? true : fail("Escolha o inimigo ao lado, depois um hex livre.");
    case TechAim::Passive: break;
  }
  return fail("Esta técnica não pode ser usada assim.");
}
std::vector<BattleHex> techniqueArea(const Battle& b, size_t caster, const std::string& kind, BattleHex cell) {
  const BattleUnit& u = b.units()[caster];
  const BattleHex from{u.x, u.y};
  const TechniqueDef* d = techniqueDef(kind);
  if (!d) return {cell};
  if (d->aim == TechAim::Arc && hexDist(from, cell) == 1) {
    std::vector<BattleHex> arc = frontArc(b, from, cell);
    if (kind == "reapTheFlanks" && !arc.empty()) arc.erase(arc.begin());
    return arc;
  }
  if (d->aim == TechAim::Line || kind == "drivingCut" || kind == "crushAgainstStone") {
    const BattleHex past = hexBeyond(from, cell);
    return b.inBounds(past.x, past.y) ? std::vector<BattleHex>{cell, past} : std::vector<BattleHex>{cell};
  }
  if (d->aim == TechAim::Far2)
    if (const std::optional<BattleHex> m = shoulderMiddle(b, from, cell)) return {*m, cell};
  return {cell};
}

// --- The strike (resolveStrikeImpl) -----------------------------------------------------------------------------------------------------
// Kael parried this attacker's blow and knows the Riposta with a use left: his counter is the Riposta.
bool riposteReady(const Battle& b, size_t kael, size_t attacker) {
  const BattleUnit& k = b.units()[kael];
  return k.cond.martial.parriedBy == static_cast<int>(attacker) && contains(k.spellList, std::string("riposte")) && tierRemaining(k, "riposte") > 0 &&
         besideUnit(b, {k.x, k.y}, attacker, 1);
}
struct TechniqueStrike {
  bool exposed = false, open = false;  // this blow uses (and, landed, spends) the Exposto / Romper a Guarda mark
  int exposedDamagePct = 0;            // Corte Perfeito's points more against Exposto
};
// Before the roll: the blow's points to hit and the share of armor it ignores (the strongest bypass of the step's own, the Exposto
// mark for Kael's techniques and the Riposta, an open guard for any physical blow).
TechniqueStrike techniqueBeforeRoll(const Battle& b, const BattleStep& step, bool counter, bool riposte, bool firstParried, size_t ai, size_t ti,
                                    bool magic, StrikeMods& mods) {
  TechniqueStrike t;
  const BattleUnit::Conditions::Martial& m = b.units()[ti].cond.martial;
  int ignore = 0;
  if (!counter) {
    mods.accuracy += step.accBonus;
    ignore = step.armorIgnorePct;
    if (firstParried) mods.accuracy += step.parriedAccBonus, ignore = std::max(ignore, step.parriedIgnorePct);
  }
  const bool technique = (!counter && techniqueKind(step.kind)) || riposte;
  if (technique && m.exposedPct > 0 && m.exposedBy == static_cast<int>(ai)) {
    t.exposed = true, ignore = std::max(ignore, m.exposedPct);
    if (!counter) mods.accuracy += step.exposedAccBonus, t.exposedDamagePct = step.exposedDamagePct;
  }
  if (!magic && m.openPct > 0) t.open = true, ignore = std::max(ignore, m.openPct);
  mods.armorIgnore = ignore / 100.0;
  return t;
}
// The weapon roll was made: Golpe de Pomo's / Estilhaçar a Linha's points off are spent, and Desafio's on one of the challenger's allies.
void techniqueSpendAccuracy(Battle& b, size_t ai, size_t ti) {
  BattleUnit::Conditions::Martial& m = b.units()[ai].cond.martial;
  if (m.accDownPct > 0) m.accDownPct = 0, m.accDownTurns = 0;
  if (m.challengePct > 0 && m.challengeBy >= 0 && static_cast<size_t>(m.challengeBy) < b.units().size() && ti != static_cast<size_t>(m.challengeBy) &&
      b.units()[ti].side == b.units()[static_cast<size_t>(m.challengeBy)].side)
    m.challengePct = 0, m.challengeBy = -1, m.challengeTurns = 0;
}
// A landed physical blow on a guarded or steady unit: the guard (the next parryable melee hit, its attacker beside it and of the other
// side, from guardFrom when Ler a Lâmina named one) takes its share off and counts as a parry; Pé Firme takes its own.
int techniqueGuard(Battle& b, size_t ai, size_t ti, bool arrow, int dmg, bool& parried, int& exposeAttacker) {
  BattleUnit &a = b.units()[ai], &t = b.units()[ti];
  BattleUnit::Conditions::Martial& m = t.cond.martial;
  if (m.guardPct > 0 && !arrow && a.side != t.side && besideUnit(b, {t.x, t.y}, ai, 1) && (m.guardFrom < 0 || m.guardFrom == static_cast<int>(ai))) {
    dmg = std::max(1, static_cast<int>(std::floor(dmg * (1 - m.guardPct / 100.0))));
    parried = true, exposeAttacker = m.guardExposePct;
    m.parriedBy = static_cast<int>(ai);
    m.guardPct = 0, m.guardFrom = -1, m.guardExposePct = 0;  // one guard slot, spent
  }
  if (m.footingPct > 0) dmg = std::max(1, static_cast<int>(std::floor(dmg * (1 - m.footingPct / 100.0))));
  return dmg;
}
// What a landed blow leaves on a foe still standing: the marks it spent and the ones it lays, the foe moved or pushed (a wall's impact),
// in that order (damage first, then the effects and the displacement). `after` gets the words, shown after the blow's number.
void techniqueOnHit(Battle& b, const BattleStep& step, bool counter, bool riposte, const TechniqueStrike& ts, size_t ai, size_t ti, int ordinary,
                    BattleEffect& e, std::vector<BattleEffect>& after, Mulberry32& rng) {
  BattleUnit &a = b.units()[ai], &t = b.units()[ti];
  BattleUnit::Conditions::Martial& m = t.cond.martial;
  if (ts.exposed) m.exposedPct = 0, m.exposedBy = -1, m.exposedTurns = 0;
  if (ts.open) m.openPct = 0, m.openTurns = 0;
  auto expose = [&](int pct) {
    m.exposedPct = std::max(m.exposedBy == static_cast<int>(ai) ? m.exposedPct : 0, pct), m.exposedBy = static_cast<int>(ai), m.exposedTurns = 2;
    after.push_back(statusEffect(ti, std::format("{} {}%", pick("Exposto", "Exposed"), m.exposedPct)));
  };
  if (riposte) expose(techniqueNumbers("riposte", a).pct);  // the old mark resolved first (above), then the Riposta's
  if (counter) return;
  if (step.wardCutPct > 0) a.cond.martial.wardCutPct = std::max(a.cond.martial.wardCutPct, step.wardCutPct);  // Corte Guardado: his own guard
  if (step.crushPct > 0) {  // Golpe Esmagador: its next weapon attack, through the end of its next full turn
    m.crushPct = std::max(m.crushPct, step.crushPct), m.crushTurns = 2, m.crushRound = -1, m.crushKind.clear();
    after.push_back(statusEffect(ti, std::format("{} -{}%", pick("Esmagado", "Crushed"), m.crushPct)));
  }
  if (step.accDownPct > 0) {
    m.accDownPct = std::max(m.accDownPct, step.accDownPct), m.accDownTurns = 2;
    after.push_back(statusEffect(ti, std::format("-{} {}", m.accDownPct, pick("acerto", "to hit"))));
  }
  if (step.openPct > 0) {
    m.openPct = std::max(m.openPct, step.openPct), m.openTurns = 2;
    after.push_back(statusEffect(ti, std::format("{} {}%", pick("Guarda aberta", "Guard open"), m.openPct)));
  }
  if (step.lowCutHexes > 0) {
    m.lowCutHexes = std::max(m.lowCutHexes, step.lowCutHexes), m.lowCutTurns = 2;
    after.push_back(statusEffect(ti, pick("Corte Baixo", "Low Cut")));
  }
  if (step.sever) {
    m.severTurns = 2;  // resolveStrikeImpl spends it on this exchange when the foe could counter it
    after.push_back(statusEffect(ti, pick("Ritmo quebrado", "Rhythm severed")));
  }
  if (step.moveFoeTo && techniqueDisplaceable(t) && techniqueStandable(b, *step.moveFoeTo)) {  // Desvio Lateral
    t.x = step.moveFoeTo->x, t.y = step.moveFoeTo->y;
    e.pushed = true;
    stepOnHex(b, ti, rng, after);
  }
  if (step.straightPush > 0) {
    bool pushed = false, wall = false;
    int hexes = step.straightPush;
    if (techniqueDisplaceable(t) && t.cond.martial.footingPct > 0) --hexes, t.cond.martial.footingPct = 0;  // Pé Firme: one hex shorter
    else if (techniqueDisplaceable(t) && t.cond.martial.bracePush) --hexes, t.cond.martial.bracePush = false;  // Erguer o Escudo (waning moon)
    BattleHex back{a.x, a.y};
    for (int k = 0; k < hexes && techniqueDisplaceable(t); ++k) {
      const BattleHex at{t.x, t.y}, to = hexBeyond(back, at);
      if (!techniqueStandable(b, to)) {
        wall = techniqueWall(b, to);
        break;
      }
      back = at, t.x = to.x, t.y = to.y, pushed = true;
    }
    if (pushed) {
      e.pushed = true;
      stepOnHex(b, ti, rng, after);
    } else {
      if (step.wallImpactPct > 0 && wall && techniqueDisplaceable(t) && hexes > 0) {  // Esmagar na Pedra: against the stone
        const int impact = std::max(1, static_cast<int>(std::floor(ordinary * step.wallImpactPct / 100.0)));
        t.hp = std::max(0, t.hp - impact);
        b.noteDamage(ai, ti, impact, false);
        BattleEffect hit{ti, impact, false, false, false, false, false};
        hit.text = std::format("{} {}", impact, pick("contra a pedra", "against the stone"));
        if (t.hp <= 0) markDead(t, rng), hit.died = true;
        after.push_back(hit);
      }
      if (step.abortOnNoPush) e.abortRush = true;
    }
  }
}

// --- Guards, stances and marks (Type::Effect) ---------------------------------------------------------------------------------------
void techniqueEffect(Battle& b, const BattleStep& step, std::vector<BattleEffect>& out) {
  BattleUnit& k = b.units()[step.att];
  if (!k.alive) return;
  if (wardEffect(b, step, out)) {  // Garran's
    b.noteAware(step.att, kSupportCe, kSupportVe);
    return;
  }
  BattleUnit::Conditions::Martial& m = k.cond.martial;
  const int foe = step.ids.empty() ? -1 : static_cast<int>(step.ids.front());
  if (step.kind == "crossguard") {
    m.guardPct = step.bonus, m.guardFrom = -1, m.guardExposePct = 0;  // a new guard replaces the one before
    out.push_back(statusEffect(step.att, std::format("{} {}%", pick("Guarda Cruzada", "Crossguard"), step.bonus)));
  } else if (step.kind == "readTheBlade" && foe >= 0) {
    m.guardPct = step.bonus, m.guardFrom = foe, m.guardExposePct = step.exposePct;
    out.push_back(statusEffect(step.att, std::format("{} {}%", pick("Ler a Lâmina", "Read the Blade"), step.bonus)));
  } else if (step.kind == "steadyFooting") {
    m.footingPct = step.bonus;
    out.push_back(statusEffect(step.att, std::format("{} {}%", pick("Pé Firme", "Steady Footing"), step.bonus)));
  } else if (step.kind == "plantTheLine") {
    m.plantPct = step.bonus;
    out.push_back(statusEffect(step.att, pick("Firmar a Linha", "Plant the Line")));
  } else if (step.kind == "deadMeasure" && foe >= 0) {
    m.measureTarget = foe, m.measurePct = step.bonus;
    out.push_back(statusEffect(static_cast<size_t>(foe), pick("Medida Mortal", "Dead Measure")));
  } else if (step.kind == "feint" && foe >= 0) {
    BattleUnit::Conditions::Martial& f = b.units()[static_cast<size_t>(foe)].cond.martial;
    f.exposedPct = std::max(f.exposedBy == static_cast<int>(step.att) ? f.exposedPct : 0, step.exposePct), f.exposedBy = static_cast<int>(step.att), f.exposedTurns = 2;
    out.push_back(statusEffect(static_cast<size_t>(foe), std::format("{} {}%", pick("Exposto", "Exposed"), f.exposedPct)));
  } else if (step.kind == "challenge" && foe >= 0) {
    BattleUnit::Conditions::Martial& f = b.units()[static_cast<size_t>(foe)].cond.martial;
    f.challengePct = std::max(f.challengePct, step.bonus), f.challengeBy = static_cast<int>(step.att), f.challengeTurns = 2;
    out.push_back(statusEffect(static_cast<size_t>(foe), pick("Desafiado", "Challenged")));
  }
  b.noteAware(step.att, kSupportCe, kSupportVe);
}

// --- The start of a unit's own turn ---------------------------------------------------------------------------------------------------
void techniqueTurnStart(Battle& b, size_t unit) {
  BattleUnit::Conditions::Martial& m = b.units()[unit].cond.martial;
  // his own: until his next turn begins
  m.guardPct = 0, m.guardFrom = -1, m.guardExposePct = 0, m.footingPct = 0, m.plantPct = 0, m.measureTarget = -1, m.measurePct = 0, m.parriedBy = -1;
  // on a foe: through the end of its next full turn
  auto tick = [](int& turns, auto clear) {
    if (turns > 0 && --turns == 0) clear();
  };
  tick(m.exposedTurns, [&] { m.exposedPct = 0, m.exposedBy = -1; });
  tick(m.openTurns, [&] { m.openPct = 0; });
  tick(m.accDownTurns, [&] { m.accDownPct = 0; });
  tick(m.challengeTurns, [&] { m.challengePct = 0, m.challengeBy = -1; });
  tick(m.lowCutTurns, [&] { m.lowCutHexes = 0; });
  if (m.severTurns < 0) m.severTurns = 0;
  else if (m.severTurns > 0) --m.severTurns;
  wardTurnStart(b, unit);  // Garran's guards, covers and memory; Golpe Esmagador on a foe
}

// --- Movement reactions (Medida Mortal before a step, Firmar a Linha after it): the ordinary movement reaction window, at most one
// answer of each Kael per step, his AP and the target checked as each commits; the walker stopped only when it can be moved ---------
bool techniqueBodyBeside(const Battle& b, const BattleUnit& w, BattleHex at, BattleHex k) {
  for (const BattleHex& p : Battle::footprint(w, at.x, at.y))
    if (hexDist(p, k) <= 1) return true;
  return false;
}
// One reaction blow of `kael` at the walker: true when it landed.
bool techniqueReactionBlow(Battle& b, size_t kael, size_t walker, const std::string& kind, int pct, Mulberry32& rng, std::vector<BattleEffect>& out) {
  BattleUnit& k = b.units()[kael];
  const int cost = apCost(b, k, ApAction::attack());  // "a further 6 AP": his sword's blow
  if (!k.alive || !b.units()[walker].alive || !apTrySpend(k, cost)) return false;
  faceToward(k, {b.units()[walker].x, b.units()[walker].y});
  BattleStep s = techniqueBlow(kael, walker, kind, pct);
  s.noCounter = true;
  const size_t before = out.size();
  resolveStrike(b, s, false, rng, out);
  for (size_t i = before; i < out.size(); ++i)
    if (out[i].unit == walker && !out[i].miss && !out[i].status && !out[i].heal) return true;
  return false;
}
bool techniqueBeforeStep(Battle& b, size_t walker, BattleHex from, BattleHex to, Mulberry32& rng, std::vector<BattleEffect>& out) {
  bool cancel = false;
  for (size_t i = 0; i < b.units().size(); ++i) {
    BattleUnit& k = b.units()[i];
    if (i == walker || !k.alive || k.cond.martial.measureTarget != static_cast<int>(walker)) continue;
    const BattleUnit& w = b.units()[walker];
    if (!techniqueBodyBeside(b, w, from, {k.x, k.y}) || techniqueBodyBeside(b, w, to, {k.x, k.y})) continue;  // a step that leaves his reach
    const int pct = k.cond.martial.measurePct;
    k.cond.martial.measureTarget = -1, k.cond.martial.measurePct = 0;  // spent by this step, answered or not
    if (techniqueReactionBlow(b, i, walker, "deadMeasure", pct, rng, out) && b.units()[walker].alive && techniqueDisplaceable(b.units()[walker])) cancel = true;
  }
  return cancel;
}
bool techniqueAfterStep(Battle& b, size_t walker, BattleHex from, BattleHex to, Mulberry32& rng, std::vector<BattleEffect>& out) {
  bool stop = false;
  for (size_t i = 0; i < b.units().size(); ++i) {
    BattleUnit& k = b.units()[i];
    const BattleUnit& w = b.units()[walker];
    if (i == walker || !k.alive || k.cond.martial.plantPct <= 0 || k.side == w.side) continue;
    if (techniqueBodyBeside(b, w, from, {k.x, k.y}) || !techniqueBodyBeside(b, w, to, {k.x, k.y})) continue;  // it just came beside him
    const int pct = k.cond.martial.plantPct;
    k.cond.martial.plantPct = 0;  // the stance's one event
    if (techniqueReactionBlow(b, i, walker, "plantTheLine", pct, rng, out) && b.units()[walker].alive && techniqueDisplaceable(b.units()[walker])) stop = true;
  }
  return stop;
}

// --- Words ----------------------------------------------------------------------------------------------------------------------------
void techniqueConditions(const BattleUnit& u, const std::function<void(const std::string&)>& add) {
  const BattleUnit::Conditions::Martial& m = u.cond.martial;
  if (m.guardPct > 0) add(std::format("{} {}%", m.guardFrom >= 0 ? pick("Ler a Lâmina", "Read the Blade") : pick("Guarda Cruzada", "Crossguard"), m.guardPct));
  if (m.footingPct > 0) add(std::format("{} -{}%", pick("Pé Firme", "Steady Footing"), m.footingPct));
  if (m.plantPct > 0) add(pick("Firmar a Linha", "Plant the Line"));
  if (m.measureTarget >= 0) add(pick("Medida Mortal armada", "Dead Measure set"));
  if (m.exposedPct > 0) add(std::format("{} {}%", pick("Exposto", "Exposed"), m.exposedPct));
  if (m.openPct > 0) add(std::format("{} {}%", pick("Guarda aberta", "Guard open"), m.openPct));
  if (m.accDownPct > 0) add(std::format("-{} {}", m.accDownPct, pick("no próximo ataque", "on its next attack")));
  if (m.challengePct > 0) add(std::format("{} -{}", pick("Desafiado", "Challenged"), m.challengePct));
  if (m.lowCutHexes > 0) add(std::format("{} +{} PA", pick("Corte Baixo", "Low Cut"), m.lowCutHexes));
  if (m.severTurns != 0) add(pick("Ritmo quebrado", "Rhythm severed"));
  wardConditions(u, add);  // Garran's
}
// moon: tonight's (WardMoon) for Garran's techniques, -1 to list every moon that changes one
std::string techniqueTooltip(const BattleUnit& c, const std::string& kind, int moon) {
  const EmberSpell* e = emberSpell(kind);
  const TechniqueDef* d = techniqueDef(kind);
  if (!e || !d) return {};
  const TechNumbers n = techniqueNumbers(kind, c);
  const int rank = techniqueStep(c, kind) + 1;
  const auto& byKind = apRules().spells.byKind;
  const auto ap = byKind.find(kind);
  std::string s = std::format("T{} · {} {}/{}", e->data.tier, "posto", rank, kTechniqueRanks);
  if (ap != byKind.end()) s += std::format(" · {} PA", ap->second);
  if (d->cooldown > 0) s += std::format(" · recarga {}", d->cooldown);
  const auto pct = [](int v) { return std::format("{}%", v); };
  std::string t;
  if (wardKind(kind)) t = wardTooltipText(kind, n, moon);  // Garran's, with the moon
  else if (kind == "measuredCut") t = std::format("inimigo ao lado · {} do golpe · +{} de acerto", pct(n.dmg), n.pct);
  else if (kind == "longstepSlash") t = std::format("um passo para um hex livre ao lado, depois golpeia um inimigo ao lado dele · {}", pct(n.dmg));
  else if (kind == "crossguard") t = std::format("o próximo golpe corpo a corpo que acertar você é aparado: -{} de dano · até o seu próximo turno", pct(n.pct));
  else if (kind == "pommelCheck") t = std::format("inimigo ao lado · {} · o próximo ataque dele -{} de acerto", pct(n.dmg), n.pct);
  else if (kind == "lowCut") t = std::format("inimigo ao lado · {} · {} do próximo movimento dele +1 PA", pct(n.dmg), n.hexes > 1 ? "os 2 primeiros hexes" : "o primeiro hex");
  else if (kind == "partingCut") t = std::format("inimigo ao lado · {} · depois um passo para um hex livre longe dele (mesmo errando)", pct(n.dmg));
  else if (kind == "overheadHew") t = std::format("inimigo ao lado · {} · ignora {} da armadura", pct(n.dmg), pct(n.pct));
  else if (kind == "steadyFooting") t = std::format("-{} de dano físico recebido; o primeiro empurrão contra você anda 1 hex a menos · até o seu próximo turno", pct(n.pct));
  else if (kind == "challenge") t = std::format("inimigo a até 2 hexes · o primeiro ataque dele num aliado seu (não em você): -{} de acerto", n.pct);
  else if (kind == "breakGuard") t = std::format("inimigo ao lado · {} · o próximo golpe físico que o acertar ignora {} da armadura", pct(n.dmg), pct(n.pct));
  else if (kind == "feint") t = std::format("inimigo ao lado fica Exposto: o seu próximo golpe de técnica nele ignora {} da armadura", pct(n.pct));
  else if (kind == "riposte") t = std::format("passiva · depois de aparar um golpe de um inimigo ao lado, o seu contra-ataque é a Riposta: {} · deixa Exposto {} · gasta um uso",
                                              pct(n.dmg), pct(n.pct));
  else if (kind == "passingStep") t = std::format("um passo para um hex ao seu lado e ao lado do inimigo, depois o golpe · {}", pct(n.dmg));
  else if (kind == "needlePoint") t = std::format("inimigo ao lado · {} · ignora {} da armadura", pct(n.dmg), pct(n.pct));
  else if (kind == "secondIntention") t = std::format("dois golpes de {} cada · se o primeiro for aparado, o segundo +{} de acerto e ignora {} da armadura", pct(n.dmg), n.pct, pct(n.pct2));
  else if (kind == "readTheBlade") t = std::format("inimigo ao lado · o próximo golpe dele que acertar você é aparado: -{} de dano, e ele fica Exposto {} · até o seu próximo turno",
                                                   pct(n.pct), pct(n.pct2));
  else if (kind == "severTheRhythm") t = std::format("inimigo ao lado · {} · o próximo contra-ataque dele não vem", pct(n.dmg));
  else if (kind == "perfectCut") t = std::format("inimigo ao lado · {} · Exposto: +{} pontos do golpe e o desvio da marca", pct(n.dmg), n.pct);
  else if (kind == "deadMeasure") t = std::format("marca um inimigo ao lado · se ele der um passo para longe de você, você pode golpear antes ({}, 6 PA); acertou: o passo não acontece", pct(n.dmg));
  else if (kind == "masterstroke") t = std::format("inimigo ao lado · {} · ignora {} da armadura · Exposto: +{} de acerto", pct(n.dmg), pct(n.pct), n.pct2);
  else if (kind == "drivingCut") t = std::format("inimigo ao lado · {} · empurra 1 hex; se ele sair, você avança para o hex dele", pct(n.dmg));
  else if (kind == "wideCleave") t = std::format("arco de 3 hexes na sua frente · {} em cada inimigo · sem contra-ataque", pct(n.dmg));
  else if (kind == "sidecast") t = std::format("inimigo ao lado · {} · ele vai para um hex escolhido ao lado de você e do hex dele", pct(n.dmg));
  else if (kind == "plantTheLine") t = std::format("postura · o primeiro inimigo que chegar ao seu lado andando leva {} (6 PA); acertou: o movimento dele acaba · até o seu próximo turno", pct(n.dmg));
  else if (kind == "shoulderThrough") t = std::format("inimigo a 2 hexes em linha reta · você entra no hex do meio · {} · empurra 1 hex", pct(n.dmg));
  else if (kind == "crushAgainstStone") t = std::format("inimigo ao lado · {} · empurra 1 hex; contra parede ou obstáculo, +{} do golpe", pct(n.dmg), pct(n.pct));
  else if (kind == "splitTheRank") t = std::format("reta de 2 hexes · o inimigo ao lado {}; se acertar, o do segundo hex {} · sem contra-ataque", pct(n.dmg), pct(n.dmg2));
  else if (kind == "reapTheFlanks") t = std::format("os dois hexes de fora do arco · {} em cada · acertou: empurra 1 hex · sem contra-ataque", pct(n.dmg));
  else if (kind == "rollingAdvance") t = std::format("inimigo ao lado · {} e empurra; se ele sair, você avança e golpeia de novo {} e empurra", pct(n.dmg), pct(n.dmg2));
  else if (kind == "shatterTheLine") t = std::format("arco de 3 hexes · o centro {} e empurra 1 hex · os lados {} e -{} de acerto no próximo ataque · sem contra-ataque",
                                                     pct(n.dmg), pct(n.dmg2), n.pct);
  return t.empty() ? s : s + "\n" + t;
}
