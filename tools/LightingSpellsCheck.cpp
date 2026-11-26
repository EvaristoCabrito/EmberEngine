#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "world/ActionPoints.h"
#include "world/Battle.h"
#include "world/LightingSpells.h"
#include "world/Spells.h"

using namespace ember;

static void assertAlmostEqual(float a, float b, float eps = 1e-3f, const std::string& msg = "") {
  if (std::abs(a - b) > eps) {
    std::cerr << "FAIL: " << msg << " (expected " << b << ", got " << a << ")\n";
    std::exit(1);
  }
}

static void testPowerProgressionTable() {
  std::cout << "[1/11] Testing power progression table (levels 1..30)...\n";

  // Level 1 baseline
  const LightSpellMetrics m1 = lightMetricsForPower(1);
  assert(m1.power == 1);
  assert(m1.durationMinutes == 60);
  assertAlmostEqual(m1.coneLengthHex, 8.0f, 1e-4f, "cone length hex L1");
  assertAlmostEqual(m1.coneAngleDegrees, 65.0f, 1e-4f, "cone angle deg L1");
  assertAlmostEqual(m1.conjurerRadiusHex, 5.0f, 1e-4f, "conjurer radius hex L1");
  assertAlmostEqual(m1.conjurerRangeHex, 8.0f, 1e-4f, "conjurer range hex L1");
  assertAlmostEqual(m1.healerRadiusHex, 4.0f, 1e-4f, "healer radius hex L1");

  // Level 15 (end of base class / next 15 levels)
  const LightSpellMetrics m15 = lightMetricsForPower(15);
  assert(m15.power == 15);
  assert(m15.durationMinutes >= 360);
  assert(m15.coneLengthHex >= 18.0f);
  assert(m15.coneAngleDegrees >= 85.0f);
  assert(m15.conjurerRadiusHex >= 11.0f);
  assert(m15.conjurerRangeHex >= 16.0f);
  assert(m15.healerRadiusHex >= 9.5f);

  // Level 30 (epic scale)
  const LightSpellMetrics m30 = lightMetricsForPower(30);
  assert(m30.power == 30);
  assert(m30.durationMinutes == 720);
  assertAlmostEqual(m30.coneLengthHex, 30.0f, 1e-4f, "cone length hex L30");
  assertAlmostEqual(m30.coneAngleDegrees, 110.0f, 1e-4f, "cone angle deg L30");
  assertAlmostEqual(m30.conjurerRadiusHex, 18.0f, 1e-4f, "conjurer radius hex L30");
  assertAlmostEqual(m30.conjurerRangeHex, 26.0f, 1e-4f, "conjurer range hex L30");
  assertAlmostEqual(m30.healerRadiusHex, 16.0f, 1e-4f, "healer radius hex L30");

  // Every single level from 1 to 30 strictly scales up:
  LightSpellMetrics prev = m1;
  for (int lvl = 2; lvl <= 30; ++lvl) {
    const LightSpellMetrics cur = lightMetricsForPower(lvl);
    assert(cur.power == lvl);
    assert(cur.coneLengthHex >= prev.coneLengthHex);
    assert(cur.coneAngleDegrees > prev.coneAngleDegrees);
    assert(cur.conjurerRadiusHex > prev.conjurerRadiusHex);
    assert(cur.conjurerRangeHex > prev.conjurerRangeHex);
    assert(cur.healerRadiusHex > prev.healerRadiusHex);
    assert(cur.durationMinutes > prev.durationMinutes);
    prev = cur;
  }
}

static void testSpellPropertiesAndProgressionGrant() {
  std::cout << "[2/11] Testing spell properties, Tier 2, mana and AP costs...\n";
  assert(isLightingSpell("arcaneBeam"));
  assert(isLightingSpell("anchoredLight"));
  assert(isLightingSpell("luminousHalo"));
  assert(!isLightingSpell("fireball"));

  assert(spellTierOf("arcaneBeam") == 2);
  assert(spellTierOf("anchoredLight") == 2);
  assert(spellTierOf("luminousHalo") == 2);

  // Fixed costs (AP and Mana are fixed, never scale down)
  assert(lightingSpellApCost("arcaneBeam", 1) == 4);
  assert(lightingSpellApCost("anchoredLight", 1) == 4);
  assert(lightingSpellApCost("luminousHalo", 1) == 4);
  assert(lightingSpellManaCost("arcaneBeam", 1) == 6);
  assert(lightingSpellManaCost("anchoredLight", 1) == 7);
  assert(lightingSpellManaCost("luminousHalo", 1) == 6);

  // Across levels: AP is always 4 PA, mana is always 7 / 6
  assert(lightingSpellApCost("arcaneBeam", 15) == 4);
  assert(lightingSpellApCost("anchoredLight", 15) == 4);
  assert(lightingSpellApCost("luminousHalo", 15) == 4);
  assert(lightingSpellManaCost("arcaneBeam", 15) == 6);
  assert(lightingSpellManaCost("anchoredLight", 15) == 7);
  assert(lightingSpellManaCost("luminousHalo", 15) == 6);

  assert(lightingSpellApCost("arcaneBeam", 30) == 4);
  assert(lightingSpellApCost("anchoredLight", 30) == 4);
  assert(lightingSpellApCost("luminousHalo", 30) == 4);
  assert(lightingSpellManaCost("arcaneBeam", 30) == 6);
  assert(lightingSpellManaCost("anchoredLight", 30) == 7);
  assert(lightingSpellManaCost("luminousHalo", 30) == 6);

  // Test class grants
  BattleUnit mage;
  mage.classId = "mage";
  mage.side = "player";
  mage.level = 1;
  mage.mag = 10;
  grantClassSpells(mage);
  assert(contains(mage.spellList, std::string("arcaneBeam")));
  assert(mage.mana == mage.maxMana && mage.maxMana >= 30);

  BattleUnit conjurer;
  conjurer.classId = "sorcerer";  // Arcanista
  conjurer.side = "player";
  conjurer.level = 1;
  conjurer.mag = 10;
  grantClassSpells(conjurer);
  assert(contains(conjurer.spellList, std::string("anchoredLight")));

  BattleUnit priest;
  priest.classId = "priest";  // Priest maps to healer family
  priest.side = "player";
  priest.level = 1;
  priest.mag = 10;
  grantClassSpells(priest);
  assert(contains(priest.spellList, std::string("luminousHalo")));
}

static void testMageArcaneBeamStationary() {
  std::cout << "[3/11] Testing Mage - Feixe de Luz stationary cone upon moving/turning...\n";
  LightingSpellRegistry& reg = LightingSpellRegistry::get();
  reg.clear();
  reg.setLogicalMap("watchtower", 100);

  const glm::vec3 castPos{5.0f, 1.0f, 5.0f};
  const glm::vec3 castFacing{1.0f, 0.0f, 0.0f};  // East
  std::string err;
  bool ok = reg.cast("arcaneBeam", "mage1", "watchtower", "watchtower-f1", 1, castPos, castFacing, 100, &err);
  assert(ok);

  auto getCasterPos = [](const std::string&) -> std::optional<glm::vec3> {
    // Mage moved to a completely different location!
    return glm::vec3{20.0f, 1.0f, 30.0f};
  };

  std::vector<LocalLight> lights = reg.activeLightsForFloor("watchtower-f1", "watchtower", 100, getCasterPos);
  assert(lights.size() == 1);
  const LocalLight& l = lights[0];
  // Origin and facing must remain locked at cast point!
  assertAlmostEqual(l.position.x, castPos.x, 1e-3f, "Feixe de Luz position X");
  assertAlmostEqual(l.position.z, castPos.z, 1e-3f, "Feixe de Luz position Z");
  assertAlmostEqual(l.facing.x, castFacing.x, 1e-3f, "Feixe de Luz facing X");
  assertAlmostEqual(l.facing.z, castFacing.z, 1e-3f, "Feixe de Luz facing Z");
  assert(l.behind < 0.0f);  // Directional cone
}

static void testConjurerAnchoredLightStationary() {
  std::cout << "[4/11] Testing Conjurer - Luz Ancorada stationary sphere...\n";
  LightingSpellRegistry& reg = LightingSpellRegistry::get();
  reg.clear();
  reg.setLogicalMap("watchtower", 100);

  const glm::vec3 placedPoint{12.0f, 0.6f, 14.0f};
  std::string err;
  bool ok = reg.cast("anchoredLight", "conjurer1", "watchtower", "watchtower-f1", 2, placedPoint, {0, 0, 1}, 100, &err);
  assert(ok);

  auto getCasterPos = [](const std::string&) -> std::optional<glm::vec3> {
    // Conjurer walked away
    return glm::vec3{0.0f, 1.0f, 0.0f};
  };

  std::vector<LocalLight> lights = reg.activeLightsForFloor("watchtower-f1", "watchtower", 100, getCasterPos);
  assert(lights.size() == 1);
  const LocalLight& l = lights[0];
  assertAlmostEqual(l.position.x, placedPoint.x, 1e-3f, "Luz Ancorada position X");
  assertAlmostEqual(l.position.z, placedPoint.z, 1e-3f, "Luz Ancorada position Z");
  assert(l.behind > 0.0f);  // Omni sphere
}

static void testPriestLuminousHaloFollowing() {
  std::cout << "[5/11] Testing Priest/Healer - Halo Luminoso following caster...\n";
  LightingSpellRegistry& reg = LightingSpellRegistry::get();
  reg.clear();
  reg.setLogicalMap("watchtower", 100);

  std::string err;
  bool ok = reg.cast("luminousHalo", "priest1", "watchtower", "watchtower-f1", 1, {2.0f, 1.0f, 2.0f}, {0, 0, 1}, 100, &err);
  assert(ok);

  glm::vec3 priestPos{10.0f, 1.0f, 15.0f};
  auto getCasterPos = [&](const std::string& id) -> std::optional<glm::vec3> {
    if (id == "priest1") return priestPos;
    return std::nullopt;
  };

  // Step 1: at (10, 1, 15)
  std::vector<LocalLight> lights = reg.activeLightsForFloor("watchtower-f1", "watchtower", 100, getCasterPos);
  assert(lights.size() == 1);
  assertAlmostEqual(lights[0].position.x, priestPos.x, 1e-3f, "Halo X");
  assertAlmostEqual(lights[0].position.z, priestPos.z, 1e-3f, "Halo Z");

  // Step 2: priest moves to (18, 1, 22)
  priestPos = glm::vec3{18.0f, 1.0f, 22.0f};
  lights = reg.activeLightsForFloor("watchtower-f1", "watchtower", 100, getCasterPos);
  assert(lights.size() == 1);
  assertAlmostEqual(lights[0].position.x, 18.0f, 1e-3f, "Halo updated X");
  assertAlmostEqual(lights[0].position.z, 22.0f, 1e-3f, "Halo updated Z");
}

static void testTowerFloorTransitionsSameLogicalMap() {
  std::cout << "[6/11] Testing tower floor 1 & 2 transitions in same logical map...\n";
  LightingSpellRegistry& reg = LightingSpellRegistry::get();
  reg.clear();
  reg.setLogicalMap("watchtower", 200);

  // Cast all 3 spells on Floor 1
  reg.cast("arcaneBeam", "mage1", "watchtower", "watchtower-f1", 1, {5.0f, 1.2f, 5.0f}, {1, 0, 0}, 200);
  reg.cast("anchoredLight", "conjurer1", "watchtower", "watchtower-f1", 1, {8.0f, 0.6f, 8.0f}, {0, 0, 1}, 200);
  reg.cast("luminousHalo", "priest1", "watchtower", "watchtower-f1", 1, {3.0f, 1.0f, 3.0f}, {0, 0, 1}, 200);

  glm::vec3 priestPos{3.0f, 1.0f, 3.0f};
  auto getCasterPos = [&](const std::string& id) -> std::optional<glm::vec3> {
    if (id == "priest1") return priestPos;
    return std::nullopt;
  };

  // 1. On Floor 1: all 3 lights are present
  std::vector<LocalLight> f1Lights = reg.activeLightsForFloor("watchtower-f1", "watchtower", 200, getCasterPos);
  assert(f1Lights.size() == 3);

  // 2. On Floor 2: cone and sphere must NOT be present!
  // But Halo follows the priest who is now on floor 2
  priestPos = glm::vec3{3.0f, 1.0f, 3.0f};
  std::vector<LocalLight> f2Lights = reg.activeLightsForFloor("watchtower-f2", "watchtower", 210, getCasterPos);
  assert(f2Lights.size() == 1);  // Only Halo!

  // 3. Return to Floor 1 at minute 220:
  // Cone and sphere are still at origin, duration decremented by 20 min
  f1Lights = reg.activeLightsForFloor("watchtower-f1", "watchtower", 220, getCasterPos);
  assert(f1Lights.size() == 3);

  const LightSpellInstance* coneInst = reg.findCasterSpell("mage1");
  assert(coneInst != nullptr);
  assert(coneInst->minutesRemaining(220) == 40);  // 60 - 20
}

static void testLogicalMapExitTerminatesImmediately() {
  std::cout << "[7/11] Testing exit to outside logical map terminates spells immediately...\n";
  LightingSpellRegistry& reg = LightingSpellRegistry::get();
  reg.clear();
  reg.setLogicalMap("watchtower", 300);

  reg.cast("arcaneBeam", "mage1", "watchtower", "watchtower-f1", 1, {5.0f, 1.0f, 5.0f}, {1, 0, 0}, 300);
  reg.cast("anchoredLight", "conjurer1", "watchtower", "watchtower-f1", 1, {8.0f, 0.6f, 8.0f}, {0, 0, 1}, 300);
  reg.cast("luminousHalo", "priest1", "watchtower", "watchtower-f1", 1, {3.0f, 1.0f, 3.0f}, {0, 0, 1}, 300);
  assert(reg.instances().size() == 3);

  // Exit watchtower to exterior: logical map changes to "wisp-forest"
  reg.setLogicalMap("wisp-forest", 305);
  assert(reg.instances().empty());

  // Return to watchtower: old spells remain terminated!
  reg.setLogicalMap("watchtower", 310);
  assert(reg.instances().empty());
}

static void testExpirationWhileFloorUnloaded() {
  std::cout << "[8/11] Testing expiration while floor is unloaded...\n";
  LightingSpellRegistry& reg = LightingSpellRegistry::get();
  reg.clear();
  reg.setLogicalMap("watchtower", 400);

  // 60 min duration -> expires at 460
  reg.cast("arcaneBeam", "mage1", "watchtower", "watchtower-f1", 1, {5.0f, 1.0f, 5.0f}, {1, 0, 0}, 400);

  // Go to floor 2 and advance 70 minutes (minute 470)
  reg.updateTime(470);

  // Query Floor 1 lights: spell has expired
  std::vector<LocalLight> lights = reg.activeLightsForFloor("watchtower-f1", "watchtower", 470, [](const std::string&) { return std::nullopt; });
  assert(lights.empty());
  assert(reg.findCasterSpell("mage1") == nullptr);
}

static void testSaveLoadPreservation() {
  std::cout << "[9/11] Testing save/load preservation and expired filtering...\n";
  LightingSpellRegistry& reg = LightingSpellRegistry::get();
  reg.clear();
  reg.setLogicalMap("watchtower", 500);

  reg.cast("arcaneBeam", "mage1", "watchtower", "watchtower-f1", 2, {5.0f, 1.5f, 5.0f}, {1, 0, 0}, 500, nullptr, 2, 0.25f);  // 80 min -> 580
  reg.cast("anchoredLight", "conjurer1", "watchtower", "watchtower-f1", 1, {8.0f, 0.85f, 8.0f}, {0, 0, 1}, 500, nullptr, 1, 0.0f); // 60 min -> 560

  // Advance to minute 520 (20 min passed)
  reg.updateTime(520);

  nlohmann::json snap = reg.toJson();
  assert(snap.contains("instances"));
  assert(snap["instances"].size() == 2);

  // Clear and restore
  reg.clear();
  assert(reg.instances().empty());
  reg.fromJson(snap, 520);
  assert(reg.instances().size() == 2);

  const LightSpellInstance* beam = reg.findCasterSpell("mage1");
  assert(beam != nullptr);
  assert(beam->minutesRemaining(520) == 60);  // 80 - 20
  assert(beam->power == 2);
  assert(beam->look == 2);
  assertAlmostEqual(beam->groundY, 0.25f, 1e-4f, "saved groundY");
  const LightSpellInstance* anchored = reg.findCasterSpell("conjurer1");
  assert(anchored != nullptr);
  assert(anchored->look == 1);

  // Now restore at minute 570: anchoredLight (expired at 560) must be discarded!
  reg.clear();
  reg.fromJson(snap, 570);
  assert(reg.instances().size() == 1);
  assert(reg.findCasterSpell("mage1") != nullptr);
  assert(reg.findCasterSpell("conjurer1") == nullptr);
}

static void testCombatTurnClockAndResting() {
  std::cout << "[10/11] Testing combat turn clock (1 round = 1 min) and resting...\n";
  LightingSpellRegistry& reg = LightingSpellRegistry::get();
  reg.clear();
  reg.setLogicalMap("watchtower", 600);
  reg.cast("luminousHalo", "priest1", "watchtower", "watchtower-f1", 1, {0, 1, 0}, {0, 0, 1}, 600);

  // Simulate battle round advancement: 1 combat turn = 1 game minute
  Battle b;
  b.advanceConditionMinutes(1);
  assert(reg.findCasterSpell("priest1")->minutesRemaining(reg.currentMinute()) == 59);

  // Resting / accelerated time: 30 minutes pass
  b.advanceConditionMinutes(30);
  assert(reg.findCasterSpell("priest1")->minutesRemaining(reg.currentMinute()) == 29);
}

static void testRecastingAndSimultaneousLamps() {
  std::cout << "[11/11] Testing recasting replacement and simultaneous different casters...\n";
  LightingSpellRegistry& reg = LightingSpellRegistry::get();
  reg.clear();
  reg.setLogicalMap("watchtower", 700);

  // Mage 1 casts
  reg.cast("arcaneBeam", "mage1", "watchtower", "watchtower-f1", 1, {0, 1, 0}, {1, 0, 0}, 700);
  assert(reg.instances().size() == 1);
  assert(reg.findCasterSpell("mage1")->minutesRemaining(700) == 60);

  // 10 minutes pass
  reg.updateTime(710);
  assert(reg.findCasterSpell("mage1")->minutesRemaining(710) == 50);

  // Mage 1 recasts: replaces previous instance with full integral duration!
  reg.cast("arcaneBeam", "mage1", "watchtower", "watchtower-f1", 1, {1, 1, 0}, {0, 0, 1}, 710);
  assert(reg.instances().size() == 1);  // No duplicate!
  assert(reg.findCasterSpell("mage1")->minutesRemaining(710) == 60);

  // Different caster (Mage 2) casts: maintains simultaneous light!
  reg.cast("arcaneBeam", "mage2", "watchtower", "watchtower-f1", 1, {5, 1, 5}, {0, 0, 1}, 710);
  assert(reg.instances().size() == 2);
  assert(reg.findCasterSpell("mage1") != nullptr);
  assert(reg.findCasterSpell("mage2") != nullptr);

  // Check overlap intensity dampening (two overlapping lights near each other)
  std::vector<LocalLight> lights = reg.activeLightsForFloor("watchtower-f1", "watchtower", 710, [](const std::string&) { return std::nullopt; });
  assert(lights.size() == 2);
  // Intensity is softly dampened to avoid blown out colors
  const float undampedLuma = glm::length(kColorArcaneBeam * reg.findCasterSpell("mage1")->intensity);
  for (const LocalLight& l : lights) {
    assert(glm::length(l.color) < undampedLuma);
  }

  std::cout << "All 11 tests passed successfully!\n";
}

int main() {
  testPowerProgressionTable();
  testSpellPropertiesAndProgressionGrant();
  testMageArcaneBeamStationary();
  testConjurerAnchoredLightStationary();
  testPriestLuminousHaloFollowing();
  testTowerFloorTransitionsSameLogicalMap();
  testLogicalMapExitTerminatesImmediately();
  testExpirationWhileFloorUnloaded();
  testSaveLoadPreservation();
  testCombatTurnClockAndResting();
  testRecastingAndSimultaneousLamps();
  std::cout << "\n========================================\n";
  std::cout << "LIGHTING SPELLS VERIFICATION: 100% PASSED\n";
  std::cout << "========================================\n";
  return 0;
}
