#pragma once

// CE QUE LES FICHIERS DE `companion_ai.yml` PARTAGENT : la lecture des mots et
// des nombres, et l'écriture puis la relecture d'une section de profil —
// réglages, règles, amis. Interne à `moonclient_companionai`.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_rules.h"

#include "game/companion_ai_file.h"

namespace moonclient::companion_yaml {

// LE RANG D'UN MOT dans son vocabulaire, vide hors du vocabulaire.
template <std::size_t N>
[[nodiscard]] std::optional<std::size_t> IndexOf(const std::array<std::string_view, N>& words,
                                                 const YAML::Node& node) {
  if (!node || !node.IsScalar()) {
    return std::nullopt;
  }
  for (std::size_t index = 0; index < N; ++index) {
    if (words[index] == node.Scalar()) {
      return index;
    }
  }
  return std::nullopt;
}

template <typename Enum, std::size_t N>
[[nodiscard]] const char* WordOf(const std::array<std::string_view, N>& words, Enum value) {
  return words[static_cast<std::size_t>(value)].data();
}

template <typename T>
[[nodiscard]] std::optional<T> NumberOf(const YAML::Node& node) {
  if (!node || !node.IsScalar()) {
    return std::nullopt;
  }
  try {
    return node.as<T>();
  } catch (const YAML::Exception&) {
    return std::nullopt;
  }
}

[[nodiscard]] inline bool IsField(const YAML::Node& key, const char* field) {
  return key.IsScalar() && key.Scalar() == field;
}

// ── LES CHAMPS ET LES MOTS DU FICHIER, EN ANGLAIS ───────────────────────────
//
// ⚠ GRAVÉS : les changer fait oublier à tous les joueurs leurs réglages.

inline constexpr const char* kVersionField = "version";
inline constexpr const char* kTypesField = "types";
inline constexpr const char* kClassField = "class";
inline constexpr const char* kProfilesField = "profiles";
inline constexpr const char* kNameField = "name";
inline constexpr const char* kTacticsField = "tactics";
inline constexpr const char* kMonsterField = "monster";
inline constexpr const char* kGenericField = "generic";
inline constexpr const char* kRankField = "rank";
inline constexpr const char* kTriggerField = "trigger";
inline constexpr const char* kAvoidField = "avoid";
inline constexpr const char* kAvoidThresholdField = "avoid_threshold";
inline constexpr const char* kAvoidStepField = "avoid_step";
inline constexpr const char* kSkillUseField = "skill_use";
inline constexpr const char* kSkillUseCountField = "skill_use_count";
inline constexpr const char* kOpeningField = "opening_skill";
inline constexpr const char* kHarassField = "harass";
inline constexpr const char* kSpReserveField = "sp_reserve";
inline constexpr const char* kWeightField = "weight";
inline constexpr const char* kShareField = "share";
inline constexpr const char* kChaseField = "chase";
// LES BORNES DES RÉGLAGES DU NIVEAU 1 : ce qu'un fichier peut porter, et ce que
// la fenêtre laisse régler.
//
// ⚠ LA BORNE DES DISTANCES EST UN PLAFOND ABSOLU, contre les valeurs aberrantes :
// une distance au-delà y est ramenée, et non refusée. Le fichier garde la valeur
// du joueur ; c'est l'IA qui la borne, en jeu, au rayon de vue du serveur.
inline constexpr int kSettingCellsMost = 50;
inline constexpr int kSettingPercentMost = 100;
inline constexpr std::uint32_t kSettingDelayMostMs = 3600000;
inline constexpr std::uint16_t kSettingSkillLevelMost = 10;

// LES SECTIONS D'UN PROFIL.
inline constexpr const char* kSettingsField = "settings";
inline constexpr const char* kRulesField = "rules";
inline constexpr const char* kFriendsField = "friends";
inline constexpr const char* kPlayersField = "players";

// UNE LIGNE DE CONDUITE : un personnage, une classe de joueur, ou un mot
// générique ; ses autres champs portent les noms de ceux des tactiques.
inline constexpr const char* kPlayerField = "player";
inline constexpr const char* kJobField = "job";
inline constexpr std::array<std::string_view, 2> kConductGenericWords = {"any_other",
                                                                         "owner_guild"};

// LES FICHES DE PERSONNAGE.
inline constexpr const char* kCharactersField = "characters";
inline constexpr const char* kCidField = "cid";
inline constexpr const char* kCompanionsField = "companions";
inline constexpr const char* kPreviousFormsField = "previous_forms";
inline constexpr const char* kDeclinedField = "declined";
inline constexpr const char* kFromField = "from";
inline constexpr const char* kToField = "to";
// LES CONTEXTES, dans l'ordre de `CompanionAiContext`, et les genres, dans
// celui de `AiCompanion`.
inline constexpr std::array<std::string_view, 3> kContextWords = {"pve", "pvp", "woe"};
inline constexpr std::array<std::string_view, 2> kCompanionWords = {"homunculus", "mercenary"};

// LES MOTS DE CHAQUE VOCABULAIRE DES TACTIQUES, dans l'ordre de l'énumération.
inline constexpr std::array<std::string_view, 4> kGenericWords = {"any_other", "chests", "still",
                                                                  "minions"};
inline constexpr std::array<std::string_view, 7> kRankWords = {"absolute", "high", "medium", "low",
                                                               "last",     "hold", "ignore"};
inline constexpr std::array<std::string_view, 3> kTriggerWords = {"always", "if_attacks",
                                                                  "if_attacks_companion"};
inline constexpr std::array<std::string_view, 3> kAvoidWords = {"never", "if_attacks", "always"};
inline constexpr std::array<std::string_view, 4> kSkillUseWords = {"never", "times",
                                                                   "once_at_level", "always"};
inline constexpr std::array<std::string_view, 3> kShareWords = {"never", "if_still", "always"};
inline constexpr std::array<std::string_view, 3> kChaseWords = {"as_base", "always", "never"};

// ── LIRE UN CHAMP FACULTATIF ────────────────────────────────────────────────
//
// Absent : rien ne change. Présent mais faux : la lecture échoue, et `fault`
// dit quel champ et pourquoi.
struct FieldFault {
  // Un mot hors du vocabulaire ; sinon, un nombre illisible.
  bool unknown_word = false;
  std::string field;
};

template <typename Enum, std::size_t N>
[[nodiscard]] bool ReadWord(const YAML::Node& map, const char* field,
                            const std::array<std::string_view, N>& words, Enum& into,
                            FieldFault& fault) {
  const YAML::Node node = map[field];
  if (!node) {
    return true;
  }
  const std::optional<std::size_t> index = IndexOf(words, node);
  if (!index.has_value()) {
    fault = FieldFault{.unknown_word = true, .field = field};
    return false;
  }
  into = static_cast<Enum>(*index);
  return true;
}

template <typename T>
[[nodiscard]] bool ReadNumber(const YAML::Node& map, const char* field, T& into,
                              FieldFault& fault) {
  const YAML::Node node = map[field];
  if (!node) {
    return true;
  }
  const std::optional<T> value = NumberOf<T>(node);
  if (!value.has_value()) {
    fault = FieldFault{.unknown_word = false, .field = field};
    return false;
  }
  into = *value;
  return true;
}

// Ce que le journal en dit.
[[nodiscard]] std::string Describe(const FieldFault& fault);

// UNE DISTANCE LUE, RAMENÉE À SON PLAFOND ABSOLU `most_cells` quand elle le
// dépasse : voir `kSettingCellsMost`. Une distance négative reste telle quelle, et c'est la
// validation qui la refuse.
inline void CapReadCells(int& cells, int most_cells) noexcept {
  cells = std::min(cells, most_cells);
}

// ── LES SECTIONS D'UN PROFIL ────────────────────────────────────────────────

// LES RÉGLAGES DU NIVEAU 1, relus par-dessus `base` : un champ absent garde la
// valeur de `base`, un champ hors du cadre aussi, et se dit dans `rejected`.
[[nodiscard]] ro::sim::CompanionAiSettings ReadSettings(const YAML::Node& node,
                                                        ro::sim::CompanionAiSettings base,
                                                        std::vector<std::string>& rejected);
// Les réglages, réémis par-dessus `existing` : ses champs inconnus restent, et
// un champ hors du cadre aussi tant que la valeur à écrire est celle que sa
// lecture a donnée — `base`, l'IA basique de la classe.
void EmitSettings(YAML::Emitter& out, const YAML::Node& existing,
                  const ro::sim::CompanionAiSettings& settings,
                  const ro::sim::CompanionAiSettings& base);

// UNE RÈGLE LUE, ou la raison de son rejet.
struct RuleReadFailure {
  RuleRejection why = RuleRejection::kNotARule;
  std::string field;
};
[[nodiscard]] std::optional<ro::sim::CompanionRule> ReadRule(const YAML::Node& entry,
                                                             RuleReadFailure& failure);
// LES CHAMPS INCONNUS D'UNE RÈGLE LUE, en texte YAML ; vide sans eux.
[[nodiscard]] std::string UnknownRuleFields(const YAML::Node& entry);
// LES RÈGLES, réémises par-dessus `existing` : une entrée écartée à la lecture
// — illisible, ou au-delà de la dernière permise — garde sa place, les règles
// valides occupent les autres dans l'ordre, et le surplus se range à la fin.
// 🔴 LES CHAMPS INCONNUS SUIVENT LA RÈGLE : `extras`, un texte par règle, les
// porte. Sans eux (une autre taille que `rules`), chaque règle reprend ceux de
// l'entrée dont elle prend la place.
void EmitRules(YAML::Emitter& out, const YAML::Node& existing,
               const std::vector<ro::sim::CompanionRule>& rules,
               const std::vector<std::string>& extras);

// UN NOM D'AMI que la lecture garde.
[[nodiscard]] bool ReadableFriend(const YAML::Node& entry);
// LES AMIS : un nom par entrée. Même règle de place qu'aux règles.
void EmitFriends(YAML::Emitter& out, const YAML::Node& existing,
                 const std::vector<std::string>& friends);

// UNE RÈGLE SEULE, et les champs inconnus de l'entrée qu'elle remplace.
void EmitRule(YAML::Emitter& out, const ro::sim::CompanionRule& rule, const YAML::Node& replaced);

// LES ENTRÉES D'UNE LISTE QUE LA LECTURE A ÉCARTÉES — illisibles, ou au-delà
// du plafond — gardent leur place ; les valeurs neuves occupent les autres dans
// l'ordre, le surplus va à la fin. `emit` reçoit l'entrée remplacée, nulle pour
// une valeur ajoutée.
template <typename Value, typename Readable, typename Emit>
void EmitKeepingRejected(YAML::Emitter& out, const YAML::Node& existing,
                         const std::vector<Value>& values, std::size_t most, Readable readable,
                         Emit emit) {
  out << YAML::BeginSeq;
  std::size_t next = 0;
  std::size_t kept = 0;
  if (existing && existing.IsSequence()) {
    for (const YAML::Node& entry : existing) {
      // Le même verdict que la lecture : lisible ET sous le plafond.
      if (!readable(entry) || kept >= most) {
        out << entry;
        continue;
      }
      ++kept;
      if (next < values.size()) {
        emit(values[next], entry);
        ++next;
      }
    }
  }
  for (; next < values.size(); ++next) {
    emit(values[next], YAML::Node());
  }
  out << YAML::EndSeq;
}

// ── LES TACTIQUES ───────────────────────────────────────────────────────────

// LA CLÉ D'UNE LIGNE : la classe de son monstre, ou son mot générique.
[[nodiscard]] std::optional<ro::sim::TacticKey> ReadTacticKey(const YAML::Node& entry,
                                                              std::string& why);
// UNE LIGNE LUE, ou la raison de son rejet. Un champ facultatif absent prend sa
// valeur d'usine ; présent mais faux, il fait rejeter la ligne.
[[nodiscard]] std::optional<ro::sim::CompanionTactic> ReadTactic(const YAML::Node& entry,
                                                                 std::string& why);
void EmitTactic(YAML::Emitter& out, const ro::sim::CompanionTactic& tactic);

// ── LES CONDUITES ENVERS LES JOUEURS ────────────────────────────────────────

// Les lignes lisibles d'une section `players`, posées dans `into` ; ce qui est
// écarté se dit dans `rejected`, précédé de `prefix`.
void ReadConducts(const YAML::Node& lines, const std::string& prefix,
                  ro::sim::CompanionPlayerConducts& into, std::vector<std::string>& rejected);
// LES LIGNES, réémises par-dessus `existing` : même règle de place qu'aux
// règles.
void EmitConducts(YAML::Emitter& out, const YAML::Node& existing,
                  const std::vector<ro::sim::CompanionPlayerConduct>& conducts);

// ── LES FICHES DE PERSONNAGE ────────────────────────────────────────────────

// UNE FICHE LUE, vide quand elle ne désigne aucun personnage. Un profil actif
// au nom illisible est écarté et dit dans `rejected`.
[[nodiscard]] std::optional<CompanionCharacter> ReadCharacter(const YAML::Node& entry,
                                                              std::vector<std::string>& rejected);
// LES FICHES, réémises : celle d'un personnage modifié remplacée (ses champs
// inconnus gardés), les autres telles quelles, sauf les renommages et retraits
// de profils, qui s'appliquent à toutes. Les fiches neuves vont à la fin.
void EmitCharacters(YAML::Emitter& out, const YAML::Node& existing,
                    const CompanionProfileEdits& edits);

}  // namespace moonclient::companion_yaml
