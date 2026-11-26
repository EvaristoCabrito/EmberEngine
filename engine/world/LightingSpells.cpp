#include "world/LightingSpells.h"

#include <algorithm>
#include <format>

namespace ember {

namespace {

float inferGroundY(const std::string& kind, const glm::vec3& originPos) {
  if (kind == kSpellArcaneBeam) return originPos.y - 1.25f;
  if (kind == kSpellAnchoredLight) return originPos.y - 0.85f;
  return originPos.y;
}

}  // namespace

LightingSpellRegistry& LightingSpellRegistry::get() {
  static LightingSpellRegistry sInstance;
  return sInstance;
}

void LightingSpellRegistry::setLogicalMap(const std::string& newLogicalMapId, int64_t currentMinute) {
  lastMinute_ = currentMinute;
  if (newLogicalMapId != currentLogicalMapId_) {
    // Left the logical map: immediately clear all active spells from abandoned map
    instances_.clear();
    currentLogicalMapId_ = newLogicalMapId;
  } else {
    updateTime(currentMinute);
  }
}

void LightingSpellRegistry::clear() {
  instances_.clear();
  currentLogicalMapId_.clear();
}

void LightingSpellRegistry::updateTime(int64_t currentMinute) {
  lastMinute_ = currentMinute;
  std::erase_if(instances_, [currentMinute](const LightSpellInstance& inst) {
    return inst.isExpired(currentMinute);
  });
}

bool LightingSpellRegistry::cast(const std::string& kind,
                                const std::string& casterId,
                                const std::string& logicalMapId,
                                const std::string& floorId,
                                int casterLevel,
                                glm::vec3 originPos,
                                glm::vec3 facingDir,
                                int64_t currentMinute,
                                std::string* error,
                                int look,
                                std::optional<float> groundY) {
  if (!isLightingSpell(kind)) {
    if (error) *error = "Não é uma magia de iluminação.";
    return false;
  }

  const int power = effectiveLightPower(casterLevel);
  const LightSpellMetrics metrics = lightMetricsForPower(power);
  const int effectiveLook = (look <= 0) ? 1 : std::clamp(look, 1, 2);
  const float t = static_cast<float>(power - 1) / 29.0f;

  // Recasting the same version replaces the previous instance of that caster with fresh full duration
  std::erase_if(instances_, [&](const LightSpellInstance& inst) {
    return inst.casterId == casterId && inst.spellKind == kind;
  });

  LightSpellInstance inst;
  inst.instanceId = std::format("light_{}_{}_{}", casterId, kind, nextIdSeq_++);
  inst.spellKind = kind;
  inst.casterId = casterId;
  inst.logicalMapId = logicalMapId.empty() ? currentLogicalMapId_ : logicalMapId;
  inst.floorId = floorId;
  inst.power = power;
  inst.look = effectiveLook;
  inst.groundY = groundY.value_or(inferGroundY(kind, originPos));
  inst.castMinute = currentMinute;
  inst.expireMinute = currentMinute + metrics.durationMinutes;

  if (kind == kSpellArcaneBeam) {
    inst.bindType = LightBindType::FixedPositionFacing;
    inst.position = originPos;
    if (glm::length(facingDir) > 1e-4f) {
      inst.facing = glm::normalize(glm::vec3(facingDir.x, 0.0f, facingDir.z));
    } else {
      inst.facing = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    inst.range = metrics.coneLengthWorld;
    inst.coneAngleDeg = metrics.coneAngleDegrees;
    inst.color = kColorArcaneBeam;
    if (effectiveLook == 2) {
      inst.intensity = 4.2f + 1.8f * t;
      inst.decay = 0.60f;
    } else {
      inst.intensity = 3.6f + 1.4f * t;
      inst.decay = 0.62f;
    }
  } else if (kind == kSpellAnchoredLight) {
    inst.bindType = LightBindType::FixedPosition;
    inst.position = originPos;
    inst.facing = glm::vec3(0.0f);
    inst.range = metrics.conjurerRadiusWorld;
    inst.color = kColorAnchoredLight;
    if (effectiveLook == 2) {
      inst.intensity = 3.8f + 1.6f * t;
      inst.decay = 0.62f;
    } else {
      inst.intensity = 3.2f + 1.3f * t;
      inst.decay = 0.65f;
    }
  } else if (kind == kSpellLuminousHalo) {
    inst.bindType = LightBindType::CasterEntity;
    inst.position = originPos;
    inst.facing = glm::vec3(0.0f);
    inst.range = metrics.healerRadiusWorld;
    inst.color = kColorLuminousHalo;
    if (effectiveLook == 2) {
      inst.intensity = 3.5f + 1.5f * t;
      inst.decay = 0.65f;
    } else {
      inst.intensity = 3.0f + 1.2f * t;
      inst.decay = 0.68f;
    }
  }

  instances_.push_back(inst);
  return true;
}

std::vector<LocalLight> LightingSpellRegistry::activeLightsForFloor(
    const std::string& floorId,
    const std::string& logicalMapId,
    int64_t currentMinute,
    const std::function<std::optional<glm::vec3>(const std::string& casterId)>& getCasterPos) const {
  std::vector<LocalLight> out;

  const std::string targetLogicalMap = logicalMapId.empty() ? currentLogicalMapId_ : logicalMapId;

  for (const LightSpellInstance& inst : instances_) {
    if (inst.isExpired(currentMinute)) continue;
    if (!targetLogicalMap.empty() && inst.logicalMapId != targetLogicalMap) continue;

    if (inst.bindType == LightBindType::FixedPositionFacing) {
      // Mage cone: fixed to origin floor
      if (inst.floorId != floorId) continue;

      LocalLight l;
      l.position = inst.position;
      l.range = inst.range;
      l.color = inst.color * inst.intensity;
      l.decay = inst.decay;
      l.facing = inst.facing;
      // Analytical spotlight cone in scene.hlsli (-100 - halfAngleDeg)
      l.behind = -(100.0f + inst.coneAngleDeg * 0.5f);
      out.push_back(l);
    } else if (inst.bindType == LightBindType::FixedPosition) {
      // Conjurer sphere: fixed to origin floor
      if (inst.floorId != floorId) continue;

      LocalLight l;
      l.position = inst.position;
      l.range = inst.range;
      l.color = inst.color * inst.intensity;
      l.decay = inst.decay;
      l.facing = glm::vec3(0.0f);
      l.behind = 1.0f;
      out.push_back(l);
    } else if (inst.bindType == LightBindType::CasterEntity) {
      // Priest halo: follows caster entity across floors in same logical map
      std::optional<glm::vec3> casterPos = getCasterPos ? getCasterPos(inst.casterId) : std::nullopt;
      if (!casterPos) {
        // Fallback to stored position if caster position not resolvable this frame
        casterPos = inst.position;
      }
      LocalLight l;
      l.position = *casterPos + glm::vec3(0.0f, 1.25f, 0.0f);
      l.range = inst.range;
      l.color = inst.color * inst.intensity;
      l.decay = inst.decay;
      l.facing = glm::vec3(0.0f);
      l.behind = 1.0f;
      out.push_back(l);
    }
  }

  // Intensity control for overlapping lights to prevent blown-out lighting
  if (out.size() > 1) {
    std::vector<float> scale(out.size(), 1.0f);
    for (size_t i = 0; i < out.size(); ++i) {
      int overlapCount = 0;
      for (size_t j = 0; j < out.size(); ++j) {
        if (i == j) continue;
        const float dist = glm::distance(out[i].position, out[j].position);
        if (dist < (out[i].range + out[j].range) * 0.5f) {
          ++overlapCount;
        }
      }
      if (overlapCount > 0) {
        scale[i] = 1.0f / (1.0f + 0.35f * static_cast<float>(overlapCount));
      }
    }
    for (size_t i = 0; i < out.size(); ++i) {
      out[i].color *= scale[i];
    }
  }

  return out;
}

const LightSpellInstance* LightingSpellRegistry::findCasterSpell(const std::string& casterId,
                                                               const std::string& kind,
                                                               int64_t currentMinute) const {
  for (const LightSpellInstance& inst : instances_) {
    if (inst.casterId == casterId && inst.spellKind == kind && !inst.isExpired(currentMinute)) {
      return &inst;
    }
  }
  return nullptr;
}

const LightSpellInstance* LightingSpellRegistry::findAnyCasterSpell(const std::string& casterId,
                                                                  int64_t currentMinute) const {
  for (const LightSpellInstance& inst : instances_) {
    if (inst.casterId == casterId && !inst.isExpired(currentMinute)) {
      return &inst;
    }
  }
  return nullptr;
}

nlohmann::json LightingSpellRegistry::toJson() const {
  nlohmann::json j = nlohmann::json::object();
  j["logicalMapId"] = currentLogicalMapId_;
  nlohmann::json arr = nlohmann::json::array();
  for (const LightSpellInstance& inst : instances_) {
    nlohmann::json item = {
        {"instanceId", inst.instanceId},
        {"spellKind", inst.spellKind},
        {"casterId", inst.casterId},
        {"logicalMapId", inst.logicalMapId},
        {"floorId", inst.floorId},
        {"bindType", static_cast<uint32_t>(inst.bindType)},
        {"posX", inst.position.x},
        {"posY", inst.position.y},
        {"posZ", inst.position.z},
        {"facingX", inst.facing.x},
        {"facingY", inst.facing.y},
        {"facingZ", inst.facing.z},
        {"power", inst.power},
        {"look", inst.look},
        {"groundY", inst.groundY},
        {"range", inst.range},
        {"coneAngleDeg", inst.coneAngleDeg},
        {"colorR", inst.color.r},
        {"colorG", inst.color.g},
        {"colorB", inst.color.b},
        {"intensity", inst.intensity},
        {"decay", inst.decay},
        {"castMinute", inst.castMinute},
        {"expireMinute", inst.expireMinute},
    };
    arr.push_back(std::move(item));
  }
  j["instances"] = std::move(arr);
  return j;
}

void LightingSpellRegistry::fromJson(const nlohmann::json& j, int64_t currentMinute) {
  instances_.clear();
  if (!j.is_object()) return;
  currentLogicalMapId_ = j.value("logicalMapId", std::string());
  if (j.contains("instances") && j["instances"].is_array()) {
    for (const auto& item : j["instances"]) {
      if (!item.is_object()) continue;
      LightSpellInstance inst;
      inst.instanceId = item.value("instanceId", std::string());
      inst.spellKind = item.value("spellKind", std::string());
      inst.casterId = item.value("casterId", std::string());
      inst.logicalMapId = item.value("logicalMapId", std::string());
      inst.floorId = item.value("floorId", std::string());
      inst.bindType = static_cast<LightBindType>(item.value("bindType", 0u));
      inst.position = {item.value("posX", 0.0f), item.value("posY", 0.0f), item.value("posZ", 0.0f)};
      inst.facing = {item.value("facingX", 0.0f), item.value("facingY", 0.0f), item.value("facingZ", 0.0f)};
      inst.power = item.value("power", 1);
      inst.look = std::clamp(item.value("look", 1), 1, 2);
      inst.groundY = item.value("groundY", inferGroundY(inst.spellKind, inst.position));
      inst.range = item.value("range", 0.0f);
      inst.coneAngleDeg = item.value("coneAngleDeg", 60.0f);
      inst.color = {item.value("colorR", 1.0f), item.value("colorG", 1.0f), item.value("colorB", 1.0f)};
      inst.intensity = item.value("intensity", 1.0f);
      inst.decay = item.value("decay", 2.0f);
      inst.castMinute = item.value("castMinute", int64_t(0));
      inst.expireMinute = item.value("expireMinute", int64_t(0));

      if (!inst.isExpired(currentMinute)) {
        instances_.push_back(std::move(inst));
      }
    }
  }
}

}  // namespace ember
