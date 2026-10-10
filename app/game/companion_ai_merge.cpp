// LA FUSION DE `companion_ai.yaml` : les modifications en attente portées sur
// le fichier relu. Voir `game/companion_ai_file.h`.
//
// 🔴 LE TEXTE RELU N'EST JAMAIS MODIFIÉ : on le parcourt en réémettant chaque
// nœud tel quel, sauf les profils renommés, retirés ou modifiés, et les fiches
// de personnage concernées. Chaque étape rend faux sur une forme qu'elle ne sait
// pas compléter (un champ qui devrait être une liste et n'en est pas une).

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "game/companion_ai_file.h"
#include "game/companion_ai_yaml.h"

namespace moonclient {
namespace {

using namespace companion_yaml;
using ro::sim::CompanionTactic;
using ro::sim::TacticKey;
using ProfileEdits = CompanionProfileEdits::ProfileEdits;
using ProfileOp = CompanionProfileEdits::ProfileOp;
using TacticEdits = CompanionProfileEdits::TacticEdits;
using TypeEdits = CompanionProfileEdits::TypeEdits;

constexpr const char* kFileHeader =
    "# L'IA des compagnons de moonclient : des profils nommes par type de\n"
    "# compagnon, et une fiche par personnage.\n"
    "#\n"
    "# class est celle du compagnon (une forme d'homoncule, un mercenaire).\n"
    "# Chaque profil porte ses reglages, ses tactiques par monstre, ses\n"
    "# conduites envers les joueurs, ses regles et ses amis ; une section\n"
    "# absente reprend l'IA basique, qui n'est jamais ecrite. Une fiche dit, pour\n"
    "# chaque classe, le profil actif en PvE, en PvP et en WoE (\"Basic AI\" :\n"
    "# l'IA basique). Une ligne ou une regle hors du vocabulaire est ecartee a la\n"
    "# lecture, mais gardee telle quelle dans le fichier.\n";

[[nodiscard]] const std::optional<CompanionTactic>* EditOf(const TacticEdits& edits,
                                                           TacticKey key) {
  for (const auto& [edited_key, line] : edits) {
    if (edited_key == key) {
      return &line;
    }
  }
  return nullptr;
}

// Absent, ou présent sans être une liste — nul compris : on ne sait pas le compléter.
[[nodiscard]] bool UsableSequence(const YAML::Node& node) {
  return !node || node.IsNull() || node.IsSequence();
}

// LES LIGNES D'UN PROFIL : chaque ligne modifiée à sa place, les autres telles
// quelles — y compris celles qui ne se lisent pas —, et les nouvelles à la fin.
// D'un profil créé (`whole`), seules les siennes.
void EmitLines(YAML::Emitter& out, const YAML::Node& lines, const TacticEdits& edits, bool whole) {
  out << YAML::BeginSeq;
  std::vector<TacticKey> placed;
  if (!whole && lines && lines.IsSequence()) {
    for (const YAML::Node& entry : lines) {
      std::string why;
      const std::optional<TacticKey> key = entry.IsMap() ? ReadTacticKey(entry, why) : std::nullopt;
      const std::optional<CompanionTactic>* edit = key.has_value() ? EditOf(edits, *key) : nullptr;
      if (edit == nullptr) {
        out << entry;
        continue;
      }
      // Retirée, ou déjà posée par un doublon plus haut : elle disparaît.
      if (edit->has_value() && std::ranges::find(placed, *key) == placed.end()) {
        EmitTactic(out, **edit);
        placed.push_back(*key);
      }
    }
  }
  for (const auto& [key, line] : edits) {
    if (line.has_value() && std::ranges::find(placed, key) == placed.end()) {
      EmitTactic(out, *line);
    }
  }
  out << YAML::EndSeq;
}

// LES SECTIONS QUI SE RÉÉCRIVENT EN ENTIER quand elles ont été modifiées ;
// sinon elles se recopient telles quelles.
struct Section {
  const char* field;
  bool (*edited)(const ProfileEdits&);
  void (*emit)(YAML::Emitter&, const YAML::Node&, const ProfileEdits&, std::uint16_t job);
};
constexpr std::array<Section, 4> kEditedSections = {{
    {kSettingsField, [](const ProfileEdits& edits) { return edits.settings.has_value(); },
     [](YAML::Emitter& out, const YAML::Node& existing, const ProfileEdits& edits,
        std::uint16_t job) {
       EmitSettings(out, existing, *edits.settings,
                    ro::sim::BasicCompanionAiSettings(ro::sim::AiCompanionOfJob(job), job));
     }},
    {kPlayersField, [](const ProfileEdits& edits) { return edits.conducts.has_value(); },
     [](YAML::Emitter& out, const YAML::Node& existing, const ProfileEdits& edits, std::uint16_t) {
       EmitConducts(out, existing, *edits.conducts);
     }},
    {kRulesField, [](const ProfileEdits& edits) { return edits.rules.has_value(); },
     [](YAML::Emitter& out, const YAML::Node& existing, const ProfileEdits& edits, std::uint16_t) {
       EmitRules(out, existing, *edits.rules, edits.rule_extras);
     }},
    {kFriendsField, [](const ProfileEdits& edits) { return edits.friends.has_value(); },
     [](YAML::Emitter& out, const YAML::Node& existing, const ProfileEdits& edits, std::uint16_t) {
       EmitFriends(out, existing, *edits.friends);
     }},
}};

// UN PROFIL MODIFIÉ, sous le nom `name`. D'un profil créé, une section qu'il
// n'a pas (des réglages d'usine) disparaît, et les entrées écartées d'avant
// aussi : il remplace tout.
[[nodiscard]] bool EmitProfileEntry(YAML::Emitter& out, const YAML::Node& profile,
                                    const ProfileEdits& edits, std::uint16_t job,
                                    const std::string& name) {
  const YAML::Node settings = profile[kSettingsField];
  if (!UsableSequence(profile[kTacticsField]) ||
      (edits.settings.has_value() && settings && !settings.IsNull() && !settings.IsMap()) ||
      (edits.conducts.has_value() && !UsableSequence(profile[kPlayersField])) ||
      (edits.rules.has_value() && !UsableSequence(profile[kRulesField])) ||
      (edits.friends.has_value() && !UsableSequence(profile[kFriendsField]))) {
    return false;
  }
  out << YAML::BeginMap;
  out << YAML::Key << kNameField << YAML::Value << name;
  for (const auto& field : profile) {
    if (IsField(field.first, kNameField)) {
      continue;
    }
    if (IsField(field.first, kTacticsField)) {
      out << YAML::Key << field.first << YAML::Value;
      EmitLines(out, field.second, edits.tactics, edits.whole);
      continue;
    }
    const auto section = std::ranges::find_if(
        kEditedSections, [&field](const Section& one) { return IsField(field.first, one.field); });
    if (section != kEditedSections.end() && section->edited(edits)) {
      out << YAML::Key << field.first << YAML::Value;
      section->emit(out, edits.whole ? YAML::Node() : field.second, edits, job);
    } else if (!(edits.whole && section != kEditedSections.end())) {
      out << YAML::Key << field.first << YAML::Value << field.second;
    }
  }
  if (!profile[kTacticsField]) {
    out << YAML::Key << kTacticsField << YAML::Value;
    EmitLines(out, YAML::Node(), edits.tactics, edits.whole);
  }
  // UNE SECTION MODIFIÉE QUE LE PROFIL N'AVAIT PAS s'ajoute à la fin.
  for (const Section& section : kEditedSections) {
    if (!profile[section.field] && section.edited(edits)) {
      out << YAML::Key << section.field << YAML::Value;
      section.emit(out, YAML::Node(), edits, job);
    }
  }
  out << YAML::EndMap;
  return true;
}

// UN PROFIL SEULEMENT RENOMMÉ : tout tel quel, sauf son nom.
void EmitRenamed(YAML::Emitter& out, const YAML::Node& profile, const std::string& name) {
  out << YAML::BeginMap;
  out << YAML::Key << kNameField << YAML::Value << name;
  for (const auto& field : profile) {
    if (!IsField(field.first, kNameField)) {
      out << YAML::Key << field.first << YAML::Value << field.second;
    }
  }
  out << YAML::EndMap;
}

// LES PROFILS D'UN TYPE : renommages et retraits joués dans l'ordre, puis
// chaque profil modifié à sa place, sous son nom d'arrivée ; les profils neufs
// à la fin.
[[nodiscard]] bool EmitProfiles(YAML::Emitter& out, const YAML::Node& profiles, std::uint16_t job,
                                const TypeEdits& edits) {
  struct Entry {
    YAML::Node node;
    std::optional<std::string> name;
    bool renamed = false;
    bool removed = false;
  };
  std::vector<Entry> entries;
  if (profiles && profiles.IsSequence()) {
    for (const YAML::Node& profile : profiles) {
      Entry entry{.node = profile};
      if (profile.IsMap() && profile[kNameField] && profile[kNameField].IsScalar()) {
        entry.name = profile[kNameField].Scalar();
      }
      entries.push_back(std::move(entry));
    }
  }
  for (const ProfileOp& op : edits.ops) {
    const auto found = std::ranges::find_if(
        entries, [&op](const Entry& entry) { return !entry.removed && entry.name == op.name; });
    if (found == entries.end()) {
      continue;
    }
    if (op.kind == ProfileOp::Kind::kDelete) {
      found->removed = true;
    } else {
      found->name = op.to;
      found->renamed = true;
    }
  }
  out << YAML::BeginSeq;
  std::vector<std::string> placed;
  for (const Entry& entry : entries) {
    if (entry.removed) {
      continue;
    }
    const auto edited =
        entry.name.has_value() ? edits.profiles.find(*entry.name) : edits.profiles.end();
    if (edited != edits.profiles.end() && std::ranges::find(placed, *entry.name) == placed.end()) {
      if (!EmitProfileEntry(out, entry.node, edited->second, job, *entry.name)) {
        return false;
      }
      placed.push_back(*entry.name);
    } else if (entry.renamed) {
      EmitRenamed(out, entry.node, *entry.name);
    } else {
      out << entry.node;
    }
  }
  // LES PROFILS NEUFS : les créés dans l'ordre de leur création, puis les
  // autres (un profil qu'un autre client a retiré entre-temps).
  std::vector<std::string> fresh;
  for (const std::string& name : edits.created) {
    if (edits.profiles.contains(name)) {
      fresh.push_back(name);
    }
  }
  for (const auto& [name, profile_edits] : edits.profiles) {
    if (std::ranges::find(fresh, name) == fresh.end()) {
      fresh.push_back(name);
    }
  }
  for (const std::string& name : fresh) {
    if (std::ranges::find(placed, name) == placed.end() &&
        !EmitProfileEntry(out, YAML::Node(YAML::NodeType::Map), edits.profiles.at(name), job,
                          name)) {
      return false;
    }
  }
  out << YAML::EndSeq;
  return true;
}

[[nodiscard]] bool EmitType(YAML::Emitter& out, const YAML::Node& type, std::uint16_t job,
                            const TypeEdits& edits) {
  if (!UsableSequence(type[kProfilesField])) {
    return false;
  }
  out << YAML::BeginMap;
  bool class_written = false;
  for (const auto& field : type) {
    class_written = class_written || IsField(field.first, kClassField);
    out << YAML::Key << field.first << YAML::Value;
    if (IsField(field.first, kProfilesField)) {
      if (!EmitProfiles(out, field.second, job, edits)) {
        return false;
      }
    } else {
      out << field.second;
    }
  }
  if (!class_written) {
    out << YAML::Key << kClassField << YAML::Value << job;
  }
  if (!type[kProfilesField]) {
    out << YAML::Key << kProfilesField << YAML::Value;
    if (!EmitProfiles(out, YAML::Node(), job, edits)) {
      return false;
    }
  }
  out << YAML::EndMap;
  return true;
}

[[nodiscard]] bool EmitTypes(YAML::Emitter& out, const YAML::Node& types,
                             const CompanionProfileEdits& edits) {
  out << YAML::BeginSeq;
  std::vector<std::uint16_t> done;
  if (types && types.IsSequence()) {
    for (const YAML::Node& type : types) {
      const std::optional<std::uint16_t> job =
          type.IsMap() ? NumberOf<std::uint16_t>(type[kClassField]) : std::nullopt;
      const auto edited = job.has_value() ? edits.by_job.find(*job) : edits.by_job.end();
      if (edited == edits.by_job.end() || std::ranges::find(done, *job) != done.end()) {
        out << type;
        continue;
      }
      if (!EmitType(out, type, *job, edited->second)) {
        return false;
      }
      done.push_back(*job);
    }
  }
  for (const auto& [job, type_edits] : edits.by_job) {
    // Un type neuf dont rien ne reste à écrire — un profil créé puis retiré —
    // ne s'ajoute pas.
    if (std::ranges::find(done, job) == done.end() && !type_edits.profiles.empty() &&
        !EmitType(out, YAML::Node(YAML::NodeType::Map), job, type_edits)) {
      return false;
    }
  }
  out << YAML::EndSeq;
  return true;
}

}  // namespace

std::optional<std::string> MergeCompanionAi(std::string_view current_text,
                                            const CompanionProfileEdits& edits) {
  YAML::Node whole;
  try {
    whole = YAML::Load(std::string(current_text));
  } catch (const YAML::Exception&) {
    return std::nullopt;
  }
  if (!whole || whole.IsNull()) {
    whole = YAML::Node(YAML::NodeType::Map);
    whole[kVersionField] = kCompanionAiFileVersion;
  }
  if (!whole.IsMap() || NumberOf<int>(whole[kVersionField]) != kCompanionAiFileVersion ||
      !UsableSequence(whole[kTypesField]) || !UsableSequence(whole[kCharactersField])) {
    return std::nullopt;
  }
  YAML::Emitter out;
  out << YAML::BeginMap;
  for (const auto& field : whole) {
    out << YAML::Key << field.first << YAML::Value;
    if (IsField(field.first, kTypesField)) {
      if (!EmitTypes(out, field.second, edits)) {
        return std::nullopt;
      }
    } else if (IsField(field.first, kCharactersField)) {
      EmitCharacters(out, field.second, edits);
    } else {
      out << field.second;
    }
  }
  if (!whole[kTypesField]) {
    out << YAML::Key << kTypesField << YAML::Value;
    if (!EmitTypes(out, YAML::Node(), edits)) {
      return std::nullopt;
    }
  }
  if (!whole[kCharactersField] && !edits.characters.empty()) {
    out << YAML::Key << kCharactersField << YAML::Value;
    EmitCharacters(out, YAML::Node(), edits);
  }
  out << YAML::EndMap;
  if (!out.good()) {
    return std::nullopt;
  }
  return std::string(kFileHeader) + out.c_str() + "\n";
}

}  // namespace moonclient
