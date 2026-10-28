#include "world/CutsceneScript.h"

#include <cmath>
#include <format>
#include <fstream>

namespace ember {

std::optional<CutsceneScript> CutsceneScript::load(const std::string& file, std::string* why) {
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    if (why) *why = "no such file: " + file;
    return std::nullopt;
  }
  const Json j = Json::parse(in, nullptr, false);
  if (j.is_discarded()) {
    if (why) *why = "not valid JSON: " + file;
    return std::nullopt;
  }
  return parse(j, why);
}

std::optional<CutsceneScript> CutsceneScript::parse(const Json& j, std::string* why) {
  auto refuse = [&](const std::string& w) {
    if (why) *why = w;
    return std::optional<CutsceneScript>();
  };
  if (!j.is_object()) return refuse("the script is not an object");
  if (j.value("format", "") != kFormat) return refuse(std::format("format \"{}\" is not \"{}\"", j.value("format", ""), kFormat));
  CutsceneScript s;
  s.id = j.value("id", "");
  if (s.id.empty()) return refuse("no \"id\"");
  const auto actors = j.find("actors");
  if (actors == j.end() || !actors->is_object()) return refuse("no \"actors\" object");
  for (const auto& [name, a] : actors->items()) {
    if (name.empty()) return refuse("an actor with an empty name");
    const std::string unit = a.is_object() ? a.value("unit", "") : a.is_string() ? a.get<std::string>() : "";
    if (unit.empty()) return refuse("actor \"" + name + "\" names no unit");
    s.actors[name] = unit;
  }
  const auto steps = j.find("steps");
  if (steps == j.end() || !steps->is_array()) return refuse("no \"steps\" array");
  if (steps->empty()) return refuse("\"steps\" is empty");
  size_t n = 0;
  for (const Json& st : *steps) {
    const std::string at = std::format("step {}", n++);
    if (!st.is_object()) return refuse(at + ": not an object");
    const std::string kind = st.value("do", "");
    CutsceneStep step;
    auto actorOf = [&](const char* key, std::string& out) -> std::optional<std::string> {
      out = st.value(key, "");
      if (out.empty()) return std::format("{}: no \"{}\"", at, key);
      if (!s.actors.contains(out)) return std::format("{}: unknown actor \"{}\"", at, out);
      return std::nullopt;
    };
    if (kind == "move") {
      step.kind = CutsceneStep::Kind::Move;
      if (const auto e = actorOf("actor", step.actor)) return refuse(*e);
      const auto to = st.find("to");
      if (to == st.end() || !to->is_array() || to->size() != 2 || !(*to)[0].is_number_integer() || !(*to)[1].is_number_integer())
        return refuse(at + ": \"to\" must be [x, y] (integers)");
      step.x = (*to)[0].get<int>(), step.y = (*to)[1].get<int>();
    } else if (kind == "face") {
      step.kind = CutsceneStep::Kind::Face;
      if (const auto e = actorOf("actor", step.actor)) return refuse(*e);
      const std::string dir = st.value("direction", "");
      const bool hasToward = st.contains("toward");
      if (dir.empty() == !hasToward) return refuse(at + ": one of \"direction\" (e ne nw w sw se) or \"toward\" (an actor)");
      if (hasToward) {
        if (const auto e = actorOf("toward", step.toward)) return refuse(*e);
        if (step.toward == step.actor) return refuse(at + ": an actor cannot face itself");
      } else {
        for (size_t d = 0; d < kDirections.size(); ++d)
          if (dir == kDirections[d]) step.direction = static_cast<int>(d);
        if (step.direction < 0) return refuse(at + ": unknown direction \"" + dir + "\" (e ne nw w sw se)");
      }
    } else if (kind == "say") {
      step.kind = CutsceneStep::Kind::Say;
      if (const auto e = actorOf("actor", step.actor)) return refuse(*e);
      step.text = st.value("text", "");
      if (step.text.empty()) return refuse(at + ": no \"text\"");
      step.translations = st.value("translations", Json());
      if (!step.translations.is_null() && !step.translations.is_object()) return refuse(at + ": \"translations\" must be an object");
      step.portrait = st.value("portrait", "");
    } else if (kind == "wait") {
      step.kind = CutsceneStep::Kind::Wait;
      const auto sec = st.find("seconds");
      if (sec == st.end() || !sec->is_number()) return refuse(at + ": \"seconds\" must be a number");
      step.seconds = sec->get<float>();
      if (!std::isfinite(step.seconds) || step.seconds < 0.0f || step.seconds > 600.0f) return refuse(at + ": \"seconds\" out of range (0..600)");
    } else {
      return refuse(at + ": unknown action \"" + kind + "\" (move, face, say, wait)");
    }
    s.steps.push_back(std::move(step));
  }
  return s;
}

}  // namespace ember
