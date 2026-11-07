#include "world/CutsceneScript.h"

#include <cmath>
#include <format>
#include <fstream>
#include <numbers>

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
    if (a.is_object() && a.value("offstage", false)) s.offstage.insert(name);
    if (a.is_object() && a.value("fallen", false)) s.fallen.insert(name);
  }
  s.cameraOnActors = j.value("camera", "") == "actors";
  s.cameraBack = j.value("cameraBack", true);
  const auto steps = j.find("steps");
  if (steps == j.end() || !steps->is_array()) return refuse("no \"steps\" array");
  if (steps->empty()) return refuse("\"steps\" is empty");
  constexpr float kDeg = std::numbers::pi_v<float> / 180.0f;
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
    // A number in range, when the key is there (`fallback` when it is not); the error otherwise.
    auto number = [&](const char* key, float& out, float lo, float hi, float fallback) -> std::optional<std::string> {
      const auto it = st.find(key);
      if (it == st.end()) return out = fallback, std::nullopt;
      if (!it->is_number()) return std::format("{}: \"{}\" must be a number", at, key);
      out = it->get<float>();
      if (!std::isfinite(out) || out < lo || out > hi) return std::format("{}: \"{}\" out of range ({}..{})", at, key, lo, hi);
      return std::nullopt;
    };
    auto hexOf = [&](const char* key) -> std::optional<std::string> {
      const auto to = st.find(key);
      if (to == st.end() || !to->is_array() || to->size() != 2 || !(*to)[0].is_number_integer() || !(*to)[1].is_number_integer())
        return std::format("{}: \"{}\" must be [x, y] (integers)", at, key);
      step.x = (*to)[0].get<int>(), step.y = (*to)[1].get<int>();
      return std::nullopt;
    };
    if (kind == "move" || kind == "enter") {
      step.kind = kind == "move" ? CutsceneStep::Kind::Move : CutsceneStep::Kind::Enter;
      if (const auto e = actorOf("actor", step.actor)) return refuse(*e);
      if (kind == "move" && st.contains("near")) {
        if (const auto e = actorOf("near", step.near)) return refuse(*e);
        if (step.near == step.actor) return refuse(at + ": an actor cannot walk near itself");
        if (st.contains("to")) return refuse(at + ": one of \"to\" or \"near\"");
      } else if (const auto e = hexOf("to")) {
        return refuse(*e);
      }
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
      step.speaker = st.value("speaker", "");
    } else if (kind == "wait") {
      step.kind = CutsceneStep::Kind::Wait;
      const auto sec = st.find("seconds");
      if (sec == st.end() || !sec->is_number()) return refuse(at + ": \"seconds\" must be a number");
      step.seconds = sec->get<float>();
      if (!std::isfinite(step.seconds) || step.seconds < 0.0f || step.seconds > 600.0f) return refuse(at + ": \"seconds\" out of range (0..600)");
    } else if (kind == "exit") {
      step.kind = CutsceneStep::Kind::Exit;
      if (const auto e = actorOf("actor", step.actor)) return refuse(*e);
    } else if (kind == "shot") {
      step.kind = CutsceneStep::Kind::Shot;
      bool any = false;
      if (const auto on = st.find("on"); on != st.end()) {
        if (on->is_string()) {
          if (const auto e = actorOf("on", step.on)) return refuse(*e);
        } else if (const auto e = hexOf("on")) {
          return refuse(at + ": \"on\" must be an actor or [x, y]");
        } else {
          step.hasHex = true;
        }
        any = true;
      }
      if (const auto b = st.find("between"); b != st.end()) {
        if (!step.on.empty() || step.hasHex) return refuse(at + ": one of \"on\" or \"between\"");
        if (!b->is_array() || b->size() != 2 || !(*b)[0].is_string() || !(*b)[1].is_string()) return refuse(at + ": \"between\" must be [actor, actor]");
        step.on = (*b)[0].get<std::string>(), step.on2 = (*b)[1].get<std::string>();
        if (!s.actors.contains(step.on) || !s.actors.contains(step.on2)) return refuse(at + ": \"between\" names an unknown actor");
        any = true;
      }
      int aims = 0;
      if (const auto t = st.find("turn"); t != st.end()) {
        if (t->is_number()) {
          step.turn = t->get<float>() * kDeg;
          if (!std::isfinite(step.turn)) return refuse(at + ": \"turn\" must be finite");
        } else if (t->is_string()) {
          bool found = false;
          for (size_t h = 0; h < kHeadings.size(); ++h)
            if (t->get<std::string>() == kHeadings[h]) step.turn = static_cast<float>(h) * 45.0f * kDeg, found = true;
          if (!found) return refuse(at + ": unknown heading \"" + t->get<std::string>() + "\" (n ne e se s sw w nw, or degrees)");
        } else {
          return refuse(at + ": \"turn\" must be a heading or degrees");
        }
        step.hasTurn = true, ++aims, any = true;
      }
      if (st.contains("look")) {
        if (const auto e = actorOf("look", step.look)) return refuse(*e);
        ++aims, any = true;
      }
      if (st.contains("behind")) {
        if (const auto e = actorOf("behind", step.behind)) return refuse(*e);
        ++aims, any = true;
      }
      if (aims > 1) return refuse(at + ": one of \"turn\", \"look\" or \"behind\"");
      if (st.contains("tilt")) {
        float deg = 0.0f;
        if (const auto e = number("tilt", deg, -80.0f, 89.5f, 0.0f)) return refuse(*e);
        step.hasTilt = true, step.tilt = deg * kDeg, any = true;
      }
      if (const auto d = st.find("distance"); d != st.end()) {
        if (d->is_number()) {
          if (const auto e = number("distance", step.distance, 0.5f, 400.0f, 0.0f)) return refuse(*e);
          step.distanceKind = 1;
        } else if (d->is_string()) {
          const std::string name = d->get<std::string>();
          step.distanceKind = name == "close" ? 2 : name == "medium" ? 3 : name == "wide" ? 4 : 0;
          if (step.distanceKind == 0) return refuse(at + ": \"distance\" must be a number, \"close\", \"medium\" or \"wide\"");
        } else {
          return refuse(at + ": \"distance\" must be a number, \"close\", \"medium\" or \"wide\"");
        }
        any = true;
      }
      if (st.contains("lift")) {
        if (const auto e = number("lift", step.lift, 0.0f, 20.0f, 0.0f)) return refuse(*e);
        step.hasLift = true, any = true;
      }
      if (const auto e = number("over", step.seconds, 0.0f, 120.0f, 0.0f)) return refuse(*e);
      step.cut = st.value("cut", false);
      step.async = st.value("async", false);
      if (!any) return refuse(at + ": a \"shot\" that changes nothing (on, between, turn, look, behind, tilt, distance, lift)");
    } else if (kind == "shake") {
      step.kind = CutsceneStep::Kind::Shake;
      if (const auto e = number("seconds", step.seconds, 0.05f, 30.0f, 0.5f)) return refuse(*e);
      if (const auto e = number("strength", step.strength, 0.01f, 5.0f, 0.15f)) return refuse(*e);
    } else if (kind == "fade") {
      step.kind = CutsceneStep::Kind::Fade;
      const std::string to = st.value("to", "");
      if (to != "black" && to != "clear") return refuse(at + ": \"to\" must be \"black\" or \"clear\"");
      step.flag = to == "black";
      if (const auto e = number("seconds", step.seconds, 0.0f, 30.0f, 0.6f)) return refuse(*e);
    } else if (kind == "bars") {
      step.kind = CutsceneStep::Kind::Bars;
      const auto on = st.find("on");
      if (on != st.end() && !on->is_boolean()) return refuse(at + ": \"on\" must be true or false");
      step.flag = on == st.end() || on->get<bool>();
    } else if (kind == "tint") {
      step.kind = CutsceneStep::Kind::Tint;
      float percent = 0.0f;
      if (const auto e = number("percent", percent, 0.0f, 100.0f, 0.0f)) return refuse(*e);
      step.strength = percent / 100.0f;
      if (const auto e = number("over", step.seconds, 0.0f, 30.0f, 0.5f)) return refuse(*e);
    } else if (kind == "flash") {
      step.kind = CutsceneStep::Kind::Flash;
      if (const auto e = number("seconds", step.seconds, 0.05f, 5.0f, 0.2f)) return refuse(*e);
    } else if (kind == "sfx") {
      step.kind = CutsceneStep::Kind::Sfx;
      step.text = st.value("file", "");
      if (step.text.empty()) return refuse(at + ": no \"file\"");
      if (const auto e = number("volume", step.strength, 0.0f, 2.0f, 1.0f)) return refuse(*e);
    } else if (kind == "music") {
      step.kind = CutsceneStep::Kind::Music;
      step.flag = st.value("stop", false);
      step.text = st.value("file", "");
      if (!step.flag && step.text.empty()) return refuse(at + ": \"file\" or \"stop\": true");
      if (const auto e = number("volume", step.strength, 0.0f, 2.0f, 0.4f)) return refuse(*e);
    } else if (kind == "caption") {
      step.kind = CutsceneStep::Kind::Caption;
      step.text = st.value("text", "");
      if (step.text.empty()) return refuse(at + ": no \"text\"");
      step.translations = st.value("translations", Json());
      if (!step.translations.is_null() && !step.translations.is_object()) return refuse(at + ": \"translations\" must be an object");
      const std::string style = st.value("style", "plain");
      step.style = style == "plain" ? 0 : style == "red" ? 1 : style == "blue" ? 2 : -1;
      if (step.style < 0) return refuse(at + ": \"style\" must be \"plain\", \"red\" or \"blue\"");
      if (const auto e = number("seconds", step.seconds, 0.0f, 60.0f, 2.5f)) return refuse(*e);
    } else if (kind == "emote") {
      step.kind = CutsceneStep::Kind::Emote;
      if (const auto e = actorOf("actor", step.actor)) return refuse(*e);
      step.text = st.value("text", "!");
      if (step.text.empty()) return refuse(at + ": an empty \"text\"");
    } else {
      return refuse(at + ": unknown action \"" + kind + "\" (move, face, say, wait, enter, exit, shot, shake, fade, bars, tint, flash, sfx, music, caption, emote)");
    }
    s.steps.push_back(std::move(step));
  }
  return s;
}

}  // namespace ember
