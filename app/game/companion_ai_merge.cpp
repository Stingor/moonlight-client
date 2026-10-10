// LA FUSION DE `companion_ai.yml` : les modifications en attente portées sur
// le fichier relu. Voir `game/companion_ai_file.h`.
//
// 🔴 LE TEXTE RELU N'EST JAMAIS MODIFIÉ : on le parcourt en réémettant chaque
// nœud tel quel, sauf les profils renommés, retirés ou modifiés, et les fiches
// de personnage concernées. Chaque étape rend faux sur une forme qu'elle ne sait
// pas compléter (un champ qui devrait être une liste et n'en est pas une).

#include <algorithm>
#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "game/companion_ai_file.h"
#include "game/companion_ai_profiles.h"
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

// ── LE PLAN D'UN TYPE : quel profil du fichier devient quoi ────────────────

// UN PROFIL DU FICHIER, suivi à travers les renommages et les retraits.
struct PlannedProfile {
  YAML::Node node;
  // Son nom dans le fichier écrit.
  std::optional<std::string> name;
  // Le nom sous lequel cette session le désigne ; vide pour un profil qu'elle ne
  // connaît pas — écrit par un autre client sous un nom qu'elle a donné à l'un
  // des siens, ou doublon que la lecture ignorait.
  std::optional<std::string> session_name;
  bool renamed = false;
  bool removed = false;
};

struct TypePlan {
  std::vector<PlannedProfile> profiles;
  // LES OPÉRATIONS TELLES QU'ELLES TOMBENT SUR LE FICHIER, dans l'ordre : ce
  // qu'il faut jouer sur les fiches qu'il porte.
  std::vector<ProfileOp> file_ops;
};

// Un nom pris : par un profil du fichier qui reste, ou par un nom que cette
// session emploie — modifié, créé, renommé, ou choisi par une fiche.
[[nodiscard]] bool NameTaken(const TypePlan& plan, std::uint16_t job, const TypeEdits& edits,
                             const CompanionProfileEdits& all_edits, const std::string& name) {
  const bool on_file = std::ranges::any_of(plan.profiles, [&name](const PlannedProfile& one) {
    return !one.removed && one.name == name;
  });
  const bool by_ops = std::ranges::any_of(
      edits.ops, [&name](const ProfileOp& op) { return op.name == name || op.to == name; });
  const bool by_character =
      std::ranges::any_of(all_edits.characters, [job, &name](const CompanionCharacter& character) {
        const auto chosen = character.active.find(job);
        return chosen != character.active.end() &&
               std::ranges::find(chosen->second, std::optional(name)) != chosen->second.end();
      });
  return on_file || by_ops || by_character || edits.profiles.contains(name) ||
         std::ranges::find(edits.created, name) != edits.created.end();
}

// Le premier nom libre tiré de `wanted`, suffixé. Chaque nom pris n'écarte
// qu'un candidat : un de plus que leur nombre en trouve toujours un.
[[nodiscard]] std::string FreeNameOnFile(const TypePlan& plan, std::uint16_t job,
                                         const TypeEdits& edits,
                                         const CompanionProfileEdits& all_edits,
                                         const std::string& wanted) {
  const std::size_t most_taken = plan.profiles.size() + (2 * edits.ops.size()) +
                                 edits.profiles.size() + edits.created.size() +
                                 (all_edits.characters.size() * ro::sim::kCompanionAiContextCount);
  for (std::size_t rank = 1; rank <= most_taken + 1; ++rank) {
    std::string name = SuffixedProfileName(wanted, rank);
    if (!NameTaken(plan, job, edits, all_edits, name)) {
      return name;
    }
  }
  return wanted;
}

// RENOMMAGES ET RETRAITS JOUÉS DANS L'ORDRE sur les profils du fichier, chacun
// sur le profil que la session désigne par ce nom.
[[nodiscard]] TypePlan PlanProfiles(const YAML::Node& profiles, std::uint16_t job,
                                    const TypeEdits& edits, const CompanionProfileEdits& all_edits,
                                    std::vector<CompanionRenameShift>* shifts) {
  TypePlan plan;
  if (profiles && profiles.IsSequence()) {
    for (const YAML::Node& profile : profiles) {
      PlannedProfile entry{.node = profile};
      if (profile.IsMap() && profile[kNameField] && profile[kNameField].IsScalar()) {
        entry.name = profile[kNameField].Scalar();
        // D'un doublon déjà écrit, la lecture n'a gardé que le premier.
        const bool seen = std::ranges::any_of(
            plan.profiles, [&entry](const PlannedProfile& one) { return one.name == entry.name; });
        if (!seen) {
          entry.session_name = entry.name;
        }
      }
      plan.profiles.push_back(std::move(entry));
    }
  }
  for (const ProfileOp& op : edits.ops) {
    const auto found = std::ranges::find_if(plan.profiles, [&op](const PlannedProfile& entry) {
      return !entry.removed && entry.session_name == op.name;
    });
    if (found == plan.profiles.end()) {
      // Sans profil au fichier, l'opération vaut encore pour ses fiches.
      plan.file_ops.push_back(op);
      continue;
    }
    if (op.kind == ProfileOp::Kind::kDelete) {
      found->removed = true;
      plan.file_ops.push_back(ProfileOp{.kind = ProfileOp::Kind::kDelete, .name = *found->name});
      continue;
    }
    // Le profil qu'un autre client a écrit sous ce nom, la session ne le
    // connaît pas : le nom désigne désormais le sien.
    for (PlannedProfile& other : plan.profiles) {
      if (&other != &*found && other.session_name == op.to) {
        other.session_name.reset();
      }
    }
    found->session_name = op.to;
    const bool occupied = std::ranges::any_of(plan.profiles, [&](const PlannedProfile& other) {
      return &other != &*found && !other.removed && other.name == op.to;
    });
    std::string written = occupied ? FreeNameOnFile(plan, job, edits, all_edits, op.to) : op.to;
    if (occupied && shifts != nullptr) {
      shifts->push_back(CompanionRenameShift{.job = job, .wanted = op.to, .written = written});
    }
    if (written != *found->name) {
      plan.file_ops.push_back(
          ProfileOp{.kind = ProfileOp::Kind::kRename, .name = *found->name, .to = written});
      found->name = std::move(written);
      found->renamed = true;
    }
  }
  return plan;
}

// LE PROFIL DU FICHIER QUI REÇOIT LES MODIFICATIONS de `key` : celui que la
// session désigne ainsi, sinon celui qui porte ce nom sans qu'elle le connaisse
// (un profil créé l'emporte sur celui qu'un autre client a écrit) ; aucun pour
// un profil neuf.
[[nodiscard]] std::optional<std::size_t> TargetOf(const TypePlan& plan, const std::string& key) {
  for (const bool by_session : {true, false}) {
    for (std::size_t index = 0; index < plan.profiles.size(); ++index) {
      const PlannedProfile& entry = plan.profiles[index];
      if (!entry.removed && (by_session ? entry.session_name == key
                                        : !entry.session_name.has_value() && entry.name == key)) {
        return index;
      }
    }
  }
  return std::nullopt;
}

// LES PROFILS D'UN TYPE : chaque profil du fichier à sa place, modifié sous son
// nom d'arrivée ou recopié ; les profils neufs à la fin.
[[nodiscard]] bool EmitProfiles(YAML::Emitter& out, const TypePlan& plan, std::uint16_t job,
                                const TypeEdits& edits) {
  std::vector<std::optional<std::string>> edited_by(plan.profiles.size());
  std::vector<std::string> fresh;
  // Les créés dans l'ordre de leur création, puis les autres (un profil qu'un
  // autre client a retiré entre-temps).
  std::vector<std::string> keys;
  for (const std::string& name : edits.created) {
    if (edits.profiles.contains(name)) {
      keys.push_back(name);
    }
  }
  for (const auto& [name, profile_edits] : edits.profiles) {
    if (std::ranges::find(keys, name) == keys.end()) {
      keys.push_back(name);
    }
  }
  for (const std::string& key : keys) {
    const std::optional<std::size_t> target = TargetOf(plan, key);
    if (!target.has_value()) {
      fresh.push_back(key);
    } else if (!edited_by[*target].has_value()) {
      edited_by[*target] = key;
    }
  }
  out << YAML::BeginSeq;
  for (std::size_t index = 0; index < plan.profiles.size(); ++index) {
    const PlannedProfile& entry = plan.profiles[index];
    if (entry.removed) {
      continue;
    }
    if (edited_by[index].has_value()) {
      if (!EmitProfileEntry(out, entry.node, edits.profiles.at(*edited_by[index]), job,
                            *entry.name)) {
        return false;
      }
    } else if (entry.renamed) {
      EmitRenamed(out, entry.node, *entry.name);
    } else {
      out << entry.node;
    }
  }
  for (const std::string& name : fresh) {
    if (!EmitProfileEntry(out, YAML::Node(YAML::NodeType::Map), edits.profiles.at(name), job,
                          name)) {
      return false;
    }
  }
  out << YAML::EndSeq;
  return true;
}

[[nodiscard]] bool EmitType(YAML::Emitter& out, const YAML::Node& type, std::uint16_t job,
                            const TypeEdits& edits, const TypePlan& plan) {
  if (!UsableSequence(type[kProfilesField])) {
    return false;
  }
  out << YAML::BeginMap;
  bool class_written = false;
  for (const auto& field : type) {
    class_written = class_written || IsField(field.first, kClassField);
    out << YAML::Key << field.first << YAML::Value;
    if (IsField(field.first, kProfilesField)) {
      if (!EmitProfiles(out, plan, job, edits)) {
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
    if (!EmitProfiles(out, plan, job, edits)) {
      return false;
    }
  }
  out << YAML::EndMap;
  return true;
}

using TypePlans = std::map<std::uint16_t, TypePlan>;

// LE TYPE DE CETTE CLASSE QUE LA FUSION RÉÉCRIT : le premier du fichier.
[[nodiscard]] YAML::Node EditedTypeOf(const YAML::Node& types, std::uint16_t job) {
  if (types && types.IsSequence()) {
    for (const YAML::Node& type : types) {
      if (type.IsMap() && NumberOf<std::uint16_t>(type[kClassField]) == job) {
        return type;
      }
    }
  }
  return YAML::Node(YAML::NodeType::Map);
}

[[nodiscard]] TypePlans PlanTypes(const YAML::Node& types, const CompanionProfileEdits& edits,
                                  std::vector<CompanionRenameShift>* shifts) {
  TypePlans plans;
  for (const auto& [job, type_edits] : edits.by_job) {
    plans.emplace(job, PlanProfiles(EditedTypeOf(types, job)[kProfilesField], job, type_edits,
                                    edits, shifts));
  }
  return plans;
}

// CE QUE LES FICHES RÉÉCRIVENT : les opérations telles qu'elles tombent sur le
// fichier, et les fiches de la session, dont le choix suit un profil
// enregistré sous un autre nom que le sien.
[[nodiscard]] CompanionProfileEdits CharacterEditsOf(const CompanionProfileEdits& edits,
                                                     const TypePlans& plans) {
  CompanionProfileEdits for_characters;
  for_characters.characters = edits.characters;
  for (const auto& [job, plan] : plans) {
    for_characters.by_job[job].ops = plan.file_ops;
    for (const PlannedProfile& entry : plan.profiles) {
      if (entry.removed || !entry.session_name.has_value() || entry.session_name == entry.name) {
        continue;
      }
      for (CompanionCharacter& character : for_characters.characters) {
        RenameInCharacter(character, job, *entry.session_name, *entry.name);
      }
    }
  }
  return for_characters;
}

[[nodiscard]] bool EmitTypes(YAML::Emitter& out, const YAML::Node& types,
                             const CompanionProfileEdits& edits, const TypePlans& plans) {
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
      if (!EmitType(out, type, *job, edited->second, plans.at(*job))) {
        return false;
      }
      done.push_back(*job);
    }
  }
  for (const auto& [job, type_edits] : edits.by_job) {
    // Un type neuf dont rien ne reste à écrire — un profil créé puis retiré —
    // ne s'ajoute pas.
    if (std::ranges::find(done, job) == done.end() && !type_edits.profiles.empty() &&
        !EmitType(out, YAML::Node(YAML::NodeType::Map), job, type_edits, plans.at(job))) {
      return false;
    }
  }
  out << YAML::EndSeq;
  return true;
}

}  // namespace

std::optional<std::string> MergeCompanionAi(std::string_view current_text,
                                            const CompanionProfileEdits& edits,
                                            std::vector<CompanionRenameShift>* shifts) {
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
  // LES TYPES ET LES FICHES SUIVENT LE MÊME PLAN, quel que soit l'ordre de
  // leurs sections dans le fichier.
  std::vector<CompanionRenameShift> planned_shifts;
  const TypePlans plans = PlanTypes(whole[kTypesField], edits, &planned_shifts);
  const CompanionProfileEdits for_characters = CharacterEditsOf(edits, plans);
  YAML::Emitter out;
  out << YAML::BeginMap;
  for (const auto& field : whole) {
    out << YAML::Key << field.first << YAML::Value;
    if (IsField(field.first, kTypesField)) {
      if (!EmitTypes(out, field.second, edits, plans)) {
        return std::nullopt;
      }
    } else if (IsField(field.first, kCharactersField)) {
      EmitCharacters(out, field.second, for_characters);
    } else {
      out << field.second;
    }
  }
  if (!whole[kTypesField]) {
    out << YAML::Key << kTypesField << YAML::Value;
    if (!EmitTypes(out, YAML::Node(), edits, plans)) {
      return std::nullopt;
    }
  }
  if (!whole[kCharactersField] && !for_characters.characters.empty()) {
    out << YAML::Key << kCharactersField << YAML::Value;
    EmitCharacters(out, YAML::Node(), for_characters);
  }
  out << YAML::EndMap;
  if (!out.good()) {
    return std::nullopt;
  }
  if (shifts != nullptr) {
    *shifts = std::move(planned_shifts);
  }
  return std::string(kFileHeader) + out.c_str() + "\n";
}

}  // namespace moonclient
