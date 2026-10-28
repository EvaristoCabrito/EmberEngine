#pragma once

// Companion conversations, ported from Ember (src/game/companionDialogues.ts): every directed pair of heroes has up to
// five conversations (tiers 0 / 25 / 50 / 80 / 100 affinity; Malrec's pairs stop at 80's tier: three), each unlocked by
// the pair's affinity score or by having been told. A reply changes the leader's affinity with the companion once (the
// scene is then remembered in the save's companionConversations, and replaying never awards or removes points).
// The authored trees come from tools/scripts/data/export_companions.ts (content/data/companions.json, Ember's
// English text as written); the Portuguese is EmberEngine's own translation (content/data/companions-pt.json: Ember
// has none), picked by the window's language.

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "world/Campaign.h"

namespace ember {

class Companions {
 public:
  struct Topic {
    int tier = 0, threshold = 0;
    std::string label;
    bool unlocked = false, resolved = false;
  };

  bool load(const std::filesystem::path& companions, const std::filesystem::path& portuguese);
  bool loaded() const { return loaded_; }

  // companionTopics: empty for the same hero twice.
  std::vector<Topic> topics(const std::string& leader, const std::string& companion, const Json& save, bool english) const;
  // buildCompanionDialogue: the conversation as Ember's DialogTree JSON (null when the tier is locked or unknown):
  // the continuity words of a hard / warm earlier conversation put before the disclosure, a replay's replies worth 0.
  Json build(const std::string& leader, const std::string& companion, int tier, const Json& save, bool english) const;
  // resolveCompanionReply: applies a chosen reply to the save (affinity score and the remembered scene); false when the
  // reply does not count (already told, another leader, locked, not a companion scene).
  bool resolveReply(Json& save, const Json& reply, const std::string& leader) const;

  // "<treeId>:<lineId>" -> {src, framed} (null when the line has no portrait), for DialogScreen.
  const Json& portraits() const { return portraits_; }
  std::string tierName(int tier, bool english) const;
  int maxTier(const std::string& leader, const std::string& companion) const;

  static std::string sceneId(const std::string& leader, const std::string& companion, int tier) {
    return "companion:" + leader + ":" + companion + ":" + std::to_string(tier);
  }
  static double affinityScore(const Json& save, const std::string& a, const std::string& b);

 private:
  std::string text(const std::string& english, bool pt) const;

  bool loaded_ = false;
  Json data_, portraits_;
  std::map<std::string, std::string> portuguese_;
};

}  // namespace ember
