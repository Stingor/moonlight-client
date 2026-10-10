// LES CONDUITES ENVERS LES JOUEURS D'UN PROFIL DE `companion_ai.yaml`. Voir
// `game/companion_ai_yaml.h`.

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "ro/sim/companion_players.h"
#include "ro/sim/companion_rules.h"

#include "game/companion_ai_yaml.h"

namespace moonclient::companion_yaml {
namespace {

using ro::sim::CompanionPlayerConduct;
using ro::sim::PlayerConductKey;
using ro::sim::PlayerConductKeyKind;

// LES CLÉS GÉNÉRIQUES, dans l'ordre de leurs mots.
constexpr std::array<PlayerConductKeyKind, kConductGenericWords.size()> kConductGenericKinds = {
    PlayerConductKeyKind::kAnyPlayer, PlayerConductKeyKind::kOwnerGuild};

[[nodiscard]] std::optional<PlayerConductKey> ReadKey(const YAML::Node& entry, std::string& why) {
  if (const YAML::Node player = entry[kPlayerField]; player) {
    if (!player.IsScalar() || !ro::sim::ValidFriendName(player.Scalar())) {
      why = "nom de personnage illisible";
      return std::nullopt;
    }
    return PlayerConductKey{.kind = PlayerConductKeyKind::kPlayer, .name = player.Scalar()};
  }
  if (const YAML::Node job = entry[kJobField]; job) {
    const std::optional<std::uint16_t> read = NumberOf<std::uint16_t>(job);
    if (!read.has_value()) {
      why = "classe de joueur illisible";
      return std::nullopt;
    }
    return PlayerConductKey{.kind = PlayerConductKeyKind::kJob, .job = *read};
  }
  if (const std::optional<std::size_t> generic =
          IndexOf(kConductGenericWords, entry[kGenericField]);
      generic.has_value()) {
    return PlayerConductKey{.kind = kConductGenericKinds[*generic]};
  }
  why = "ni joueur, ni classe, ni ligne générique connue";
  return std::nullopt;
}

[[nodiscard]] std::optional<CompanionPlayerConduct> ReadConduct(const YAML::Node& entry,
                                                                std::string& why) {
  if (!entry.IsMap()) {
    why = "conduite illisible";
    return std::nullopt;
  }
  CompanionPlayerConduct conduct;
  const std::optional<PlayerConductKey> key = ReadKey(entry, why);
  if (!key.has_value()) {
    return std::nullopt;
  }
  conduct.key = *key;
  FieldFault fault;
  const bool read = ReadWord(entry, kRankField, kRankWords, conduct.rank, fault) &&
                    ReadWord(entry, kTriggerField, kTriggerWords, conduct.trigger, fault) &&
                    ReadWord(entry, kSkillUseField, kSkillUseWords, conduct.skill_use, fault) &&
                    ReadNumber(entry, kSkillUseCountField, conduct.skill_use_count, fault) &&
                    ReadNumber(entry, kOpeningField, conduct.opening_skill_id, fault) &&
                    ReadNumber(entry, kSpReserveField, conduct.sp_reserve_points, fault) &&
                    ReadWord(entry, kChaseField, kChaseWords, conduct.chase, fault);
  if (!read) {
    why = Describe(fault);
    return std::nullopt;
  }
  if (!ro::sim::ValidConduct(conduct)) {
    why = "valeur hors des bornes";
    return std::nullopt;
  }
  return conduct;
}

void EmitConduct(YAML::Emitter& out, const CompanionPlayerConduct& conduct) {
  out << YAML::BeginMap;
  switch (conduct.key.kind) {
    case PlayerConductKeyKind::kPlayer:
      out << YAML::Key << kPlayerField << YAML::Value << conduct.key.name;
      break;
    case PlayerConductKeyKind::kJob:
      out << YAML::Key << kJobField << YAML::Value << conduct.key.job;
      break;
    case PlayerConductKeyKind::kAnyPlayer:
    case PlayerConductKeyKind::kOwnerGuild: {
      const auto found = std::ranges::find(kConductGenericKinds, conduct.key.kind);
      out << YAML::Key << kGenericField << YAML::Value
          << kConductGenericWords[static_cast<std::size_t>(found - kConductGenericKinds.begin())]
                 .data();
      break;
    }
  }
  out << YAML::Key << kRankField << YAML::Value << WordOf(kRankWords, conduct.rank);
  out << YAML::Key << kTriggerField << YAML::Value << WordOf(kTriggerWords, conduct.trigger);
  out << YAML::Key << kSkillUseField << YAML::Value << WordOf(kSkillUseWords, conduct.skill_use);
  out << YAML::Key << kSkillUseCountField << YAML::Value << conduct.skill_use_count;
  out << YAML::Key << kOpeningField << YAML::Value << conduct.opening_skill_id;
  out << YAML::Key << kSpReserveField << YAML::Value << conduct.sp_reserve_points;
  out << YAML::Key << kChaseField << YAML::Value << WordOf(kChaseWords, conduct.chase);
  out << YAML::EndMap;
}

}  // namespace

void ReadConducts(const YAML::Node& lines, const std::string& prefix,
                  ro::sim::CompanionPlayerConducts& into, std::vector<std::string>& rejected) {
  if (!lines || !lines.IsSequence()) {
    return;
  }
  for (const YAML::Node& entry : lines) {
    std::string why;
    const std::optional<CompanionPlayerConduct> conduct = ReadConduct(entry, why);
    if (!conduct.has_value()) {
      rejected.push_back(prefix + why);
    } else if (into.Find(conduct->key) != nullptr) {
      rejected.push_back(prefix + "conduite en double");
    } else if (!into.Set(*conduct)) {
      rejected.push_back(prefix + "table des conduites pleine");
    }
  }
}

void EmitConducts(YAML::Emitter& out, const YAML::Node& existing,
                  const std::vector<CompanionPlayerConduct>& conducts) {
  // LE MÊME VERDICT QUE LA LECTURE : une ligne en double, écartée à la lecture,
  // reste à sa place.
  std::vector<PlayerConductKey> seen;
  EmitKeepingRejected(
      out, existing, conducts, ro::sim::kCompanionConductMost,
      [&seen](const YAML::Node& entry) {
        std::string why;
        const std::optional<CompanionPlayerConduct> conduct = ReadConduct(entry, why);
        if (!conduct.has_value() || std::ranges::find(seen, conduct->key) != seen.end()) {
          return false;
        }
        seen.push_back(conduct->key);
        return true;
      },
      [&out](const CompanionPlayerConduct& conduct, const YAML::Node&) {
        EmitConduct(out, conduct);
      });
}

}  // namespace moonclient::companion_yaml
