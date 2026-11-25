#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "render/Renderer.h"

namespace ember {

// Spells identifiers
inline constexpr const char* kSpellArcaneBeam = "arcaneBeam";      // Mage — Feixe de Luz
inline constexpr const char* kSpellAnchoredLight = "anchoredLight";  // Conjurer/Sorcerer — Luz Ancorada
inline constexpr const char* kSpellLuminousHalo = "luminousHalo";    // Priest/Healer — Halo Luminoso

// Spell families
inline constexpr const char* kFamilyMage = "mage";
inline constexpr const char* kFamilyConjurer = "conjurer";
inline constexpr const char* kFamilyHealer = "healer";

// Colors: predominantly white, with warm-neutral / pale magical temperature tints (no neon oversaturation)
// Mage: Pale silver-white arcane beam
inline const glm::vec3 kColorArcaneBeam{0.88f, 0.94f, 1.0f};
// Conjurer/Sorcerer: Warm-neutral pearl white with subtle amethyst tint
inline const glm::vec3 kColorAnchoredLight{0.96f, 0.91f, 1.0f};
// Priest/Healer: Warm pale golden-white lantern light
inline const glm::vec3 kColorLuminousHalo{1.0f, 0.95f, 0.84f};

// Hex distance to world scale: in Hex.h, kHexRadius = 1.0, so hex step = sqrt(3) ~= 1.7320508 world units
inline constexpr float kHexToWorldUnits = 1.7320508f;

// Binding type
enum class LightBindType : uint32_t {
  FixedPositionFacing = 0,  // Mage cone: fixed position & orientation at cast
  FixedPosition = 1,        // Conjurer sphere: fixed position at cast
  CasterEntity = 2          // Priest halo: moves with caster entity across floors
};

// Power metrics for a given power tier (1..10)
struct LightSpellMetrics {
  int power = 1;
  int durationMinutes = 60;
  float coneLengthHex = 6.0f;
  float coneLengthWorld = 6.0f * kHexToWorldUnits;
  float coneAngleDegrees = 60.0f;
  float conjurerRadiusHex = 4.0f;
  float conjurerRadiusWorld = 4.0f * kHexToWorldUnits;
  float conjurerRangeHex = 6.0f;
  float conjurerRangeWorld = 6.0f * kHexToWorldUnits;
  float healerRadiusHex = 3.0f;
  float healerRadiusWorld = 3.0f * kHexToWorldUnits;
};

// Calculate effective power / level (1..30)
inline int effectiveLightPower(int level) {
  return std::clamp(level, 1, 30);
}

// Compute metrics from level / power (1..30)
// Progresses every level from level 1 up through the next 15 levels and on to level 30
inline LightSpellMetrics lightMetricsForPower(int power) {
  power = std::clamp(power, 1, 30);
  LightSpellMetrics m;
  m.power = power;
  const float t = static_cast<float>(power - 1) / 29.0f;

  // Feixe de Luz (Mage cone):
  // Level 1: 8 hexes, 65°
  // Level 15: 19 hexes, ~87°
  // Level 30: 30 hexes, 110°
  m.coneLengthHex = std::round(8.0f + 22.0f * t);
  m.coneLengthWorld = m.coneLengthHex * kHexToWorldUnits;
  m.coneAngleDegrees = 65.0f + 45.0f * t;

  // Luz Ancorada (Conjurer sphere & reach):
  // Level 1: radius 5.0 hexes, range 8.0 hexes
  // Level 15: radius 11.3 hexes, range 16.7 hexes
  // Level 30: radius 18.0 hexes, range 26.0 hexes
  m.conjurerRadiusHex = 5.0f + 13.0f * t;
  m.conjurerRadiusWorld = m.conjurerRadiusHex * kHexToWorldUnits;
  m.conjurerRangeHex = 8.0f + 18.0f * t;
  m.conjurerRangeWorld = m.conjurerRangeHex * kHexToWorldUnits;

  // Halo Luminoso (Healer radiant aura):
  // Level 1: radius 4.0 hexes
  // Level 15: radius 9.8 hexes
  // Level 30: radius 16.0 hexes
  m.healerRadiusHex = 4.0f + 12.0f * t;
  m.healerRadiusWorld = m.healerRadiusHex * kHexToWorldUnits;

  // Duration: scales smoothly from 60 min (level 1) to 378 min (~6.3h at level 15) to 720 min (12h at level 30)
  m.durationMinutes = static_cast<int>(std::round(60.0f + 660.0f * t));

  return m;
}

// Explicit mapping from screen / floor ID to logical map ID
inline std::string logicalMapOfScreen(const std::string& screenId) {
  if (screenId.starts_with("watchtower-") || screenId == "watchtower") return "watchtower";
  if (screenId.starts_with("farol")) return "farol";
  if (screenId.starts_with("misty-") || screenId.starts_with("cave") || screenId.starts_with("gruta")) return "misty-cave";
  if (screenId.starts_with("inn-") || screenId == "inn" || screenId.starts_with("estalagem")) return "inn";
  if (screenId.starts_with("monastery") || screenId.starts_with("mosteiro")) return "monastery";
  if (screenId.starts_with("cemetery") || screenId.starts_with("cemiterio-esquecidos")) return "cemetery";
  if (screenId.starts_with("crypt-of-whispers") || screenId.starts_with("cripta")) return "crypt-of-whispers";
  if (screenId.starts_with("frozen-swamp")) return "frozen-swamp";
  if (screenId.starts_with("deep-ashen")) return "deep-ashen";
  if (screenId.starts_with("farmlands")) return "farmlands";
  return screenId;
}

// One persistent lighting spell instance
struct LightSpellInstance {
  std::string instanceId;
  std::string spellKind;     // "arcaneBeam", "anchoredLight", "luminousHalo"
  std::string casterId;      // stable caster identifier (hero name/ID)
  std::string logicalMapId;  // ID of logical map
  std::string floorId;       // screen/floor where it was anchored
  LightBindType bindType = LightBindType::FixedPosition;

  glm::vec3 position{0.0f};  // world position
  glm::vec3 facing{0.0f};    // horizontal direction vector for cone

  int power = 1;
  float range = 0.0f;        // world radius or cone length
  float coneAngleDeg = 60.0f;
  glm::vec3 color{1.0f};
  float intensity = 1.0f;
  float decay = 2.0f;

  int64_t castMinute = 0;
  int64_t expireMinute = 0;

  bool isExpired(int64_t currentMinute) const {
    return currentMinute >= expireMinute;
  }
  int64_t minutesRemaining(int64_t currentMinute) const {
    return std::max<int64_t>(0, expireMinute - currentMinute);
  }
};

// Global / scene registry managing persistent light spells
class LightingSpellRegistry {
 public:
  static LightingSpellRegistry& get();

  const std::string& activeLogicalMapId() const { return currentLogicalMapId_; }

  // Set logical map. If it differs from the previous one, immediately ends all spells from the abandoned map.
  void setLogicalMap(const std::string& newLogicalMapId, int64_t currentMinute);

  // Cast a lighting spell. If caster already has an active spell of this kind, replaces it with full duration.
  bool cast(const std::string& kind,
            const std::string& casterId,
            const std::string& logicalMapId,
            const std::string& floorId,
            int casterLevel,
            glm::vec3 originPos,
            glm::vec3 facingDir,
            int64_t currentMinute,
            std::string* error = nullptr);

  // Update time and prune expired spells
  void updateTime(int64_t currentMinute);

  int64_t currentMinute() const { return lastMinute_; }
  void advanceMinutes(int minutes) { updateTime(lastMinute_ + minutes); }

  // Build local lights for the current frame and floor
  std::vector<LocalLight> activeLightsForFloor(
      const std::string& floorId,
      const std::string& logicalMapId,
      int64_t currentMinute,
      const std::function<std::optional<glm::vec3>(const std::string& casterId)>& getCasterPos) const;

  // Active spells list
  const std::vector<LightSpellInstance>& instances() const { return instances_; }

  // Find active spell for caster
  const LightSpellInstance* findCasterSpell(const std::string& casterId, const std::string& kind, int64_t currentMinute) const;
  const LightSpellInstance* findAnyCasterSpell(const std::string& casterId, int64_t currentMinute) const;
  const LightSpellInstance* findCasterSpell(const std::string& casterId) const {
    return findAnyCasterSpell(casterId, lastMinute_);
  }

  // Save / Load serialization
  nlohmann::json toJson() const;
  void fromJson(const nlohmann::json& j, int64_t currentMinute);

  void clear();

 private:
  std::vector<LightSpellInstance> instances_;
  std::string currentLogicalMapId_;
  int64_t lastMinute_ = 0;
  uint64_t nextIdSeq_ = 1;
};

// Checks if kind is a lighting spell
inline bool isLightingSpell(const std::string& kind) {
  return kind == kSpellArcaneBeam || kind == kSpellAnchoredLight || kind == kSpellLuminousHalo;
}

inline std::string lightingSpellDisplayName(const std::string& kind) {
  if (kind == kSpellArcaneBeam) return "Feixe de Luz";
  if (kind == kSpellAnchoredLight) return "Luz Ancorada";
  if (kind == kSpellLuminousHalo) return "Halo Luminoso";
  return kind;
}

// Cost helpers: fixed AP and Mana cost (AP never scales down with level)
inline int lightingSpellApCost(const std::string& /*kind*/, int /*level*/ = 1) {
  return 4;  // Fixed 4 AP
}

inline int lightingSpellManaCost(const std::string& kind, int /*level*/ = 1) {
  if (kind == kSpellAnchoredLight) return 7;
  return 6;  // arcaneBeam: 6, luminousHalo: 6
}

}  // namespace ember
