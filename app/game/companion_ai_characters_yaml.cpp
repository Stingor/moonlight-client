// LES FICHES DE PERSONNAGE DE `companion_ai.yml` : le profil actif par classe
// et par contexte, la dernière forme vue de chaque genre, les propositions
// refusées et celles qui attendent. Voir `game/companion_ai_yaml.h`.

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "game/companion_ai_file.h"
#include "game/companion_ai_yaml.h"

namespace moonclient::companion_yaml {
namespace {

using JobPairs = std::vector<std::pair<std::uint16_t, std::uint16_t>>;

static_assert(kContextWords.size() == ro::sim::kCompanionAiContextCount);
static_assert(kCompanionWords.size() == kAiCompanionCount);

constexpr const char* kProposedField = "proposed";
constexpr const char* kToldField = "told";

// Les champs d'une fiche que ce client connaît : les autres restent.
constexpr std::array<std::string_view, 7> kCharacterFields = {
    kCidField,      kNameField,     kCompanionsField, kPreviousFormsField,
    kDeclinedField, kProposedField, kToldField};

// UN NOM DE PROFIL ACTIF : un profil nommé, ou l'IA basique.
[[nodiscard]] bool ReadableActive(const YAML::Node& node) {
  return node && node.IsScalar() &&
         (ValidProfileName(node.Scalar()) || node.Scalar() == kBasicProfileName);
}

[[nodiscard]] bool Known(const YAML::Node& key, std::span<const std::string_view> fields) {
  return key.IsScalar() && std::ranges::find(fields, key.Scalar()) != fields.end();
}

// LA CLASSE D'UNE ENTRÉE DE `companions`, vide quand elle ne se lit pas.
[[nodiscard]] std::optional<std::uint16_t> ClassOf(const YAML::Node& entry) {
  const std::optional<std::uint16_t> job =
      entry.IsMap() ? NumberOf<std::uint16_t>(entry[kClassField]) : std::nullopt;
  return job.has_value() && *job != 0 ? job : std::nullopt;
}

// UNE PAIRE (ancienne classe, nouvelle classe), vide quand elle ne se lit pas.
[[nodiscard]] std::optional<std::pair<std::uint16_t, std::uint16_t>> PairOf(
    const YAML::Node& entry) {
  const std::optional<std::uint16_t> from_job =
      entry.IsMap() ? NumberOf<std::uint16_t>(entry[kFromField]) : std::nullopt;
  const std::optional<std::uint16_t> to_job =
      entry.IsMap() ? NumberOf<std::uint16_t>(entry[kToField]) : std::nullopt;
  if (!from_job.has_value() || !to_job.has_value()) {
    return std::nullopt;
  }
  return std::pair(*from_job, *to_job);
}

void ReadCompanions(const YAML::Node& companions, CompanionCharacter& into,
                    std::vector<std::string>& rejected) {
  if (!companions || !companions.IsSequence()) {
    return;
  }
  for (const YAML::Node& entry : companions) {
    const std::optional<std::uint16_t> job = ClassOf(entry);
    if (!job.has_value()) {
      rejected.push_back("fiche " + into.key.name + " : classe illisible");
      continue;
    }
    ContextProfiles& active = into.active[*job];
    for (std::size_t context = 0; context < kContextWords.size(); ++context) {
      const YAML::Node chosen = entry[kContextWords[context].data()];
      if (!chosen) {
        continue;
      }
      if (ReadableActive(chosen)) {
        active[context] = chosen.Scalar();
      } else {
        rejected.push_back("fiche " + into.key.name + " : profil actif illisible");
      }
    }
  }
}

void ReadPairs(const YAML::Node& pairs, const std::string& name, JobPairs& into,
               std::vector<std::string>& rejected) {
  if (!pairs || !pairs.IsSequence()) {
    return;
  }
  for (const YAML::Node& entry : pairs) {
    if (const auto pair = PairOf(entry); pair.has_value()) {
      into.push_back(*pair);
    } else {
      rejected.push_back("fiche " + name + " : paire de classes illisible");
    }
  }
}

void EmitCompanion(YAML::Emitter& out, std::uint16_t job, const ContextProfiles& active,
                   const YAML::Node& existing) {
  out << YAML::BeginMap;
  out << YAML::Key << kClassField << YAML::Value << job;
  for (std::size_t context = 0; context < kContextWords.size(); ++context) {
    const char* field = kContextWords[context].data();
    if (active[context].has_value()) {
      out << YAML::Key << field << YAML::Value << *active[context];
    } else if (const YAML::Node kept = existing ? existing[field] : YAML::Node();
               kept && !ReadableActive(kept)) {
      // Écarté à la lecture : gardé tel quel.
      out << YAML::Key << field << YAML::Value << kept;
    }
  }
  if (existing && existing.IsMap()) {
    for (const auto& field : existing) {
      if (!IsField(field.first, kClassField) && !Known(field.first, kContextWords)) {
        out << YAML::Key << field.first << YAML::Value << field.second;
      }
    }
  }
  out << YAML::EndMap;
}

// L'ENTRÉE DE CETTE CLASSE dans une fiche relue, nulle sans elle.
[[nodiscard]] YAML::Node CompanionEntryOf(const YAML::Node& existing, std::uint16_t job) {
  const YAML::Node companions = existing ? existing[kCompanionsField] : YAML::Node();
  if (companions && companions.IsSequence()) {
    for (const YAML::Node& entry : companions) {
      if (ClassOf(entry) == job) {
        return entry;
      }
    }
  }
  return YAML::Node();
}

// LES ENTRÉES D'UNE LISTE DE LA FICHE RELUE que la lecture a écartées : elles
// suivent, telles quelles, celles que la fiche réécrit.
template <typename Readable>
void EmitUnreadable(YAML::Emitter& out, const YAML::Node& existing, const char* field,
                    Readable readable) {
  const YAML::Node list = existing ? existing[field] : YAML::Node();
  if (list && list.IsSequence()) {
    for (const YAML::Node& entry : list) {
      if (!readable(entry)) {
        out << entry;
      }
    }
  }
}

void EmitPairs(YAML::Emitter& out, const YAML::Node& existing, const char* field,
               const JobPairs& pairs) {
  out << YAML::Key << field << YAML::Value << YAML::BeginSeq;
  for (const auto& [from_job, to_job] : pairs) {
    out << YAML::Flow << YAML::BeginMap << YAML::Key << kFromField << YAML::Value << from_job
        << YAML::Key << kToField << YAML::Value << to_job << YAML::EndMap;
  }
  EmitUnreadable(out, existing, field,
                 [](const YAML::Node& entry) { return PairOf(entry).has_value(); });
  out << YAML::EndSeq;
}

void EmitCharacter(YAML::Emitter& out, const YAML::Node& existing,
                   const CompanionCharacter& character) {
  out << YAML::BeginMap;
  if (character.key.cid != 0) {
    out << YAML::Key << kCidField << YAML::Value << character.key.cid;
  }
  if (!character.key.name.empty()) {
    out << YAML::Key << kNameField << YAML::Value << character.key.name;
  }
  out << YAML::Key << kCompanionsField << YAML::Value << YAML::BeginSeq;
  for (const auto& [job, active] : character.active) {
    EmitCompanion(out, job, active, CompanionEntryOf(existing, job));
  }
  EmitUnreadable(out, existing, kCompanionsField,
                 [](const YAML::Node& entry) { return ClassOf(entry).has_value(); });
  out << YAML::EndSeq;
  out << YAML::Key << kPreviousFormsField << YAML::Value << YAML::BeginMap;
  for (std::size_t genre = 0; genre < kCompanionWords.size(); ++genre) {
    if (character.previous_form[genre] != 0) {
      out << YAML::Key << kCompanionWords[genre].data() << YAML::Value
          << character.previous_form[genre];
    }
  }
  // Un genre que ce client ne connaît pas reste.
  if (const YAML::Node forms = existing ? existing[kPreviousFormsField] : YAML::Node();
      forms && forms.IsMap()) {
    for (const auto& form : forms) {
      if (!Known(form.first, kCompanionWords)) {
        out << YAML::Key << form.first << YAML::Value << form.second;
      }
    }
  }
  out << YAML::EndMap;
  EmitPairs(out, existing, kDeclinedField, character.declined);
  EmitPairs(out, existing, kProposedField, character.proposed);
  EmitPairs(out, existing, kToldField, character.told);
  if (existing && existing.IsMap()) {
    for (const auto& field : existing) {
      if (!Known(field.first, kCharacterFields)) {
        out << YAML::Key << field.first << YAML::Value << field.second;
      }
    }
  }
  out << YAML::EndMap;
}

// LES RENOMMAGES ET RETRAITS DE PROFILS, appliqués à une fiche. Rend vrai
// quand elle a changé.
bool ApplyOps(CompanionCharacter& character, const CompanionProfileEdits& edits) {
  bool changed = false;
  for (const auto& [job, type_edits] : edits.by_job) {
    for (const CompanionProfileEdits::ProfileOp& op : type_edits.ops) {
      const bool renamed = op.kind == CompanionProfileEdits::ProfileOp::Kind::kRename;
      changed = RenameInCharacter(character, job, op.name,
                                  renamed ? std::optional(op.to) : std::nullopt) ||
                changed;
    }
  }
  return changed;
}

}  // namespace

std::optional<CompanionCharacter> ReadCharacter(const YAML::Node& entry,
                                                std::vector<std::string>& rejected) {
  if (!entry.IsMap()) {
    return std::nullopt;
  }
  CompanionCharacter character;
  character.key.cid = NumberOf<std::uint32_t>(entry[kCidField]).value_or(0);
  if (const YAML::Node name = entry[kNameField]; name && name.IsScalar()) {
    character.key.name = name.Scalar();
  }
  if (character.key.cid == 0 && character.key.name.empty()) {
    return std::nullopt;
  }
  ReadCompanions(entry[kCompanionsField], character, rejected);
  if (const YAML::Node forms = entry[kPreviousFormsField]; forms && forms.IsMap()) {
    for (std::size_t genre = 0; genre < kCompanionWords.size(); ++genre) {
      character.previous_form[genre] =
          NumberOf<std::uint16_t>(forms[kCompanionWords[genre].data()]).value_or(0);
    }
  }
  ReadPairs(entry[kDeclinedField], character.key.name, character.declined, rejected);
  ReadPairs(entry[kProposedField], character.key.name, character.proposed, rejected);
  ReadPairs(entry[kToldField], character.key.name, character.told, rejected);
  return character;
}

void EmitCharacters(YAML::Emitter& out, const YAML::Node& existing,
                    const CompanionProfileEdits& edits) {
  out << YAML::BeginSeq;
  std::vector<bool> placed(edits.characters.size(), false);
  if (existing && existing.IsSequence()) {
    for (const YAML::Node& entry : existing) {
      std::vector<std::string> ignored;
      std::optional<CompanionCharacter> read = ReadCharacter(entry, ignored);
      if (!read.has_value()) {
        out << entry;
        continue;
      }
      const auto edited =
          std::ranges::find_if(edits.characters, [&read](const CompanionCharacter& character) {
            return SameCharacter(read->key, character.key);
          });
      const auto index = static_cast<std::size_t>(edited - edits.characters.begin());
      if (edited != edits.characters.end() && !placed[index]) {
        EmitCharacter(out, entry, *edited);
        placed[index] = true;
      } else if (ApplyOps(*read, edits)) {
        EmitCharacter(out, entry, *read);
      } else {
        out << entry;
      }
    }
  }
  for (std::size_t index = 0; index < edits.characters.size(); ++index) {
    if (!placed[index]) {
      EmitCharacter(out, YAML::Node(), edits.characters[index]);
    }
  }
  out << YAML::EndSeq;
}

}  // namespace moonclient::companion_yaml
