with open('engine/world/Spells.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

sub = """}  // namespace

std::string elementOf(const std::string& kind) {
  const SpellData* s = spellData(kind);
  return s ? s->element : "";
}

std::string elementLabelPt(const std::string& element) {
  static const std::pair<const char*, const char*> kLabels[] = {{"fire", "Fogo"},   {"lightning", "Raio"},   {"ice", "Gelo"},       {"arcane", "Arcana"},
                                                                {"darkness", "Trevas"}, {"holy", "Sagrado"}, {"poison", "Veneno"}, {"ember", "Brasa"}};
  static const std::pair<const char*, const char*> kEnglish[] = {{"fire", "Fire"},   {"lightning", "Lightning"},   {"ice", "Ice"},       {"arcane", "Arcane"},
                                                                 {"darkness", "Darkness"}, {"holy", "Holy"}, {"poison", "Poison"}, {"ember", "Ember"}};
  for (const auto& [id, label] : gLogEnglish ? kEnglish : kLabels)
    if (element == id) return label;
  return element;
}"""

text = text.replace(sub, "}  // namespace")

old_anchor = """}  // namespace

std::vector<LevelUpEvent> takeLevelUpEvents() {"""

new_anchor = """}  // namespace

std::string elementOf(const std::string& kind) {
  const SpellData* s = spellData(kind);
  return s ? s->element : "";
}

std::string elementLabelPt(const std::string& element) {
  static const std::pair<const char*, const char*> kLabels[] = {{"fire", "Fogo"},   {"lightning", "Raio"},   {"ice", "Gelo"},       {"arcane", "Arcana"},
                                                                {"darkness", "Trevas"}, {"holy", "Sagrado"}, {"poison", "Veneno"}, {"ember", "Brasa"}};
  static const std::pair<const char*, const char*> kEnglish[] = {{"fire", "Fire"},   {"lightning", "Lightning"},   {"ice", "Ice"},       {"arcane", "Arcane"},
                                                                 {"darkness", "Darkness"}, {"holy", "Holy"}, {"poison", "Poison"}, {"ember", "Ember"}};
  for (const auto& [id, label] : gLogEnglish ? kEnglish : kLabels)
    if (element == id) return label;
  return element;
}

std::vector<LevelUpEvent> takeLevelUpEvents() {"""

assert old_anchor in text, "old_anchor not found"
text = text.replace(old_anchor, new_anchor, 1)

with open('engine/world/Spells.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

print("Fixed Spells.cpp successfully!")
