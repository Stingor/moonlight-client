// LES RÉGLAGES DU NIVEAU 1 DANS `companion_ai.yml` : leur lecture par-dessus
// l'IA basique, et leur réécriture qui garde ce qu'elle n'a pas compris. Voir
// `game/companion_ai_yaml.h`.

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_skills.h"
#include "ro/sim/companion_tactics.h"

#include "game/companion_ai_yaml.h"

namespace moonclient::companion_yaml {
namespace {

using ro::sim::CompanionAiSettings;

// ⚠ GRAVÉS, comme tous les champs du fichier.

// ⚠ UNE DISTANCE AU-DELÀ DE SA BORNE Y EST RAMENÉE (`capped`) ; un pourcentage
// au-delà de la sienne est refusé.
struct IntSetting {
  const char* key;
  int CompanionAiSettings::* member;
  int most;
  bool capped = false;
};
constexpr std::array<IntSetting, 9> kIntSettings = {{
    {"follow_cells", &CompanionAiSettings::follow_distance_cells, kSettingCellsMost, true},
    {"leash_still_cells", &CompanionAiSettings::leash_still_cells, kSettingCellsMost, true},
    {"leash_walking_cells", &CompanionAiSettings::leash_walking_cells, kSettingCellsMost, true},
    {"engage_still_cells", &CompanionAiSettings::engage_still_cells, kSettingCellsMost, true},
    {"engage_walking_cells", &CompanionAiSettings::engage_walking_cells, kSettingCellsMost, true},
    {"own_attack_min_hp_percent", &CompanionAiSettings::own_attack_min_hp_percent,
     kSettingPercentMost},
    {"own_attack_min_sp_percent", &CompanionAiSettings::own_attack_min_sp_percent,
     kSettingPercentMost},
    {"return_below_hp_percent", &CompanionAiSettings::return_below_hp_percent, kSettingPercentMost},
    {"flee_below_hp_percent", &CompanionAiSettings::flee_below_hp_percent, kSettingPercentMost},
}};

struct BoolSetting {
  const char* key;
  bool CompanionAiSettings::* member;
};
constexpr std::array<BoolSetting, 13> kBoolSettings = {{
    {"attack_on_own", &CompanionAiSettings::attack_on_own},
    {"defend_owner", &CompanionAiSettings::defend_owner},
    {"assist_owner", &CompanionAiSettings::assist_owner},
    {"defend_partner", &CompanionAiSettings::defend_partner},
    {"defend_self", &CompanionAiSettings::defend_self},
    {"spare_others_monsters", &CompanionAiSettings::spare_others_monsters},
    {"chase", &CompanionAiSettings::chase},
    {"retarget_while_chasing", &CompanionAiSettings::retarget_while_chasing},
    {"standby_defends", &CompanionAiSettings::standby_defends},
    {"standby_after_fight", &CompanionAiSettings::standby_after_fight},
    {"standby_kept_across_maps", &CompanionAiSettings::standby_kept_across_maps},
    {"move_order_holds_cell", &CompanionAiSettings::move_order_holds_cell},
    {"move_order_hold_fights", &CompanionAiSettings::move_order_hold_fights},
}};

struct MsSetting {
  const char* key;
  std::uint32_t CompanionAiSettings::* member;
};
constexpr std::array<MsSetting, 2> kMsSettings = {{
    {"drop_untouched_after_ms", &CompanionAiSettings::drop_untouched_after_ms},
    {"arrival_attack_hold_ms", &CompanionAiSettings::arrival_attack_hold_ms},
}};

constexpr const char* kSeatedField = "owner_seated";
constexpr std::array<std::string_view, 2> kSeatedWords = {"no_change", "stay_beside_passive"};

constexpr const char* kSkillsField = "skills";
constexpr const char* kSkillModeField = "offensive";
constexpr const char* kSkillReserveField = "sp_reserve";
constexpr const char* kSkillDelayField = "delay_ms";
constexpr const char* kPermissionsField = "permissions";
constexpr const char* kPermissionSkillField = "skill";
constexpr const char* kPermissionAllowedField = "allowed";
constexpr const char* kPermissionLevelField = "level";
constexpr std::array<std::string_view, 4> kSkillModeWords = {"never", "in_combat",
                                                             "chase_and_combat", "skills_only"};

// ── CE QUI EST RECEVABLE, champ par champ ───────────────────────────────────

[[nodiscard]] std::optional<int> IntOf(const YAML::Node& node, const IntSetting& field) {
  std::optional<int> number = NumberOf<int>(node);
  if (number.has_value() && field.capped) {
    CapReadCells(*number, field.most);
  }
  return number.has_value() && *number >= 0 && *number <= field.most ? number : std::nullopt;
}

[[nodiscard]] std::optional<std::uint32_t> MsOf(const YAML::Node& node) {
  const std::optional<std::uint32_t> ms = NumberOf<std::uint32_t>(node);
  return ms.has_value() && *ms <= kSettingDelayMostMs ? ms : std::nullopt;
}

[[nodiscard]] std::optional<std::int64_t> ReserveOf(const YAML::Node& node) {
  const std::optional<std::int64_t> points = NumberOf<std::int64_t>(node);
  return points.has_value() && *points >= 0 && *points <= ro::sim::kTacticSpReserveMost
             ? points
             : std::nullopt;
}

[[nodiscard]] std::optional<ro::sim::SkillPermission> PermissionOf(const YAML::Node& entry) {
  const std::optional<std::uint16_t> skill_id =
      entry.IsMap() ? NumberOf<std::uint16_t>(entry[kPermissionSkillField]) : std::nullopt;
  if (!skill_id.has_value() || *skill_id == 0) {
    return std::nullopt;
  }
  ro::sim::SkillPermission permission{.skill_id = *skill_id};
  FieldFault fault;
  if (!ReadNumber(entry, kPermissionAllowedField, permission.allowed, fault) ||
      !ReadNumber(entry, kPermissionLevelField, permission.level, fault) ||
      permission.level > kSettingSkillLevelMost) {
    return std::nullopt;
  }
  return permission;
}

// Toutes les permissions se lisent.
[[nodiscard]] bool AllPermissionsRead(const YAML::Node& permissions) {
  return std::ranges::all_of(
      permissions, [](const YAML::Node& entry) { return PermissionOf(entry).has_value(); });
}

// UN CHAMP HORS DU CADRE SE RECOPIE TEL QUEL tant que la valeur à écrire est
// celle que sa lecture a donnée : le joueur ne l'a pas changé en jeu.
template <typename Value>
void EmitField(YAML::Emitter& out, const YAML::Node& existing, const char* key, bool readable,
               const Value& value, const Value& as_read) {
  out << YAML::Key << key << YAML::Value;
  const YAML::Node old = existing[key];
  if (old && !readable && value == as_read) {
    out << old;
  } else {
    out << value;
  }
}

// Les champs de `existing` que la liste des clés connues ne nomme pas.
template <std::size_t N>
void EmitUnknown(YAML::Emitter& out, const YAML::Node& existing,
                 const std::array<std::string_view, N>& known) {
  if (!existing || !existing.IsMap()) {
    return;
  }
  for (const auto& field : existing) {
    if (!field.first.IsScalar() || std::ranges::find(known, field.first.Scalar()) == known.end()) {
      out << YAML::Key << field.first << YAML::Value << field.second;
    }
  }
}

}  // namespace

CompanionAiSettings ReadSettings(const YAML::Node& node, CompanionAiSettings base,
                                 std::vector<std::string>& rejected) {
  if (!node || !node.IsMap()) {
    return base;
  }
  const auto refuse = [&rejected](const char* key) {
    rejected.push_back(std::string("réglage ") + key +
                       " hors du cadre : l'IA basique le remplace, le fichier le garde");
  };
  for (const IntSetting& field : kIntSettings) {
    if (const YAML::Node value = node[field.key]; value) {
      const std::optional<int> number = IntOf(value, field);
      number.has_value() ? static_cast<void>(base.*field.member = *number) : refuse(field.key);
    }
  }
  for (const BoolSetting& field : kBoolSettings) {
    if (const YAML::Node value = node[field.key]; value) {
      const std::optional<bool> flag = NumberOf<bool>(value);
      flag.has_value() ? static_cast<void>(base.*field.member = *flag) : refuse(field.key);
    }
  }
  for (const MsSetting& field : kMsSettings) {
    if (const YAML::Node value = node[field.key]; value) {
      const std::optional<std::uint32_t> ms = MsOf(value);
      ms.has_value() ? static_cast<void>(base.*field.member = *ms) : refuse(field.key);
    }
  }
  if (const YAML::Node seated = node[kSeatedField]; seated) {
    const std::optional<std::size_t> index = IndexOf(kSeatedWords, seated);
    index.has_value()
        ? static_cast<void>(base.owner_seated = static_cast<ro::sim::OwnerSeated>(*index))
        : refuse(kSeatedField);
  }
  const YAML::Node skills = node[kSkillsField];
  if (!skills || !skills.IsMap()) {
    return base;
  }
  if (const YAML::Node mode = skills[kSkillModeField]; mode) {
    const std::optional<std::size_t> index = IndexOf(kSkillModeWords, mode);
    index.has_value()
        ? static_cast<void>(base.skills.offensive = static_cast<ro::sim::OffensiveSkills>(*index))
        : refuse(kSkillModeField);
  }
  if (const YAML::Node reserve = skills[kSkillReserveField]; reserve) {
    const std::optional<std::int64_t> points = ReserveOf(reserve);
    points.has_value() ? static_cast<void>(base.skills.sp_reserve_points = *points)
                       : refuse(kSkillReserveField);
  }
  if (const YAML::Node delay = skills[kSkillDelayField]; delay) {
    const std::optional<std::uint32_t> ms = MsOf(delay);
    ms.has_value() ? static_cast<void>(base.skills.delay_ms = *ms) : refuse(kSkillDelayField);
  }
  if (const YAML::Node permissions = skills[kPermissionsField];
      permissions && permissions.IsSequence()) {
    base.skills.permissions.clear();
    for (const YAML::Node& entry : permissions) {
      if (const std::optional<ro::sim::SkillPermission> permission = PermissionOf(entry);
          permission.has_value()) {
        base.skills.permissions.push_back(*permission);
      } else {
        refuse(kPermissionsField);
      }
    }
  }
  return base;
}

void EmitSettings(YAML::Emitter& out, const YAML::Node& existing,
                  const CompanionAiSettings& settings, const CompanionAiSettings& base) {
  std::vector<std::string> ignored;
  const CompanionAiSettings as_read = ReadSettings(existing, base, ignored);
  const YAML::Node old = existing && existing.IsMap() ? existing : YAML::Node();
  out << YAML::BeginMap;
  for (const IntSetting& field : kIntSettings) {
    EmitField(out, old, field.key, IntOf(old[field.key], field).has_value(), settings.*field.member,
              as_read.*field.member);
  }
  for (const BoolSetting& field : kBoolSettings) {
    EmitField(out, old, field.key, NumberOf<bool>(old[field.key]).has_value(),
              settings.*field.member, as_read.*field.member);
  }
  for (const MsSetting& field : kMsSettings) {
    EmitField(out, old, field.key, MsOf(old[field.key]).has_value(), settings.*field.member,
              as_read.*field.member);
  }
  EmitField(out, old, kSeatedField, IndexOf(kSeatedWords, old[kSeatedField]).has_value(),
            std::string(WordOf(kSeatedWords, settings.owner_seated)),
            std::string(WordOf(kSeatedWords, as_read.owner_seated)));

  const YAML::Node old_skills =
      old[kSkillsField] && old[kSkillsField].IsMap() ? old[kSkillsField] : YAML::Node();
  out << YAML::Key << kSkillsField << YAML::Value << YAML::BeginMap;
  EmitField(out, old_skills, kSkillModeField,
            IndexOf(kSkillModeWords, old_skills[kSkillModeField]).has_value(),
            std::string(WordOf(kSkillModeWords, settings.skills.offensive)),
            std::string(WordOf(kSkillModeWords, as_read.skills.offensive)));
  EmitField(out, old_skills, kSkillReserveField,
            ReserveOf(old_skills[kSkillReserveField]).has_value(),
            settings.skills.sp_reserve_points, as_read.skills.sp_reserve_points);
  EmitField(out, old_skills, kSkillDelayField, MsOf(old_skills[kSkillDelayField]).has_value(),
            settings.skills.delay_ms, as_read.skills.delay_ms);
  out << YAML::Key << kPermissionsField << YAML::Value;
  const YAML::Node old_permissions = old_skills[kPermissionsField];
  if (old_permissions && old_permissions.IsSequence() && !AllPermissionsRead(old_permissions) &&
      settings.skills.permissions == as_read.skills.permissions) {
    out << old_permissions;
  } else {
    out << YAML::BeginSeq;
    for (const ro::sim::SkillPermission& permission : settings.skills.permissions) {
      out << YAML::BeginMap;
      out << YAML::Key << kPermissionSkillField << YAML::Value << permission.skill_id;
      out << YAML::Key << kPermissionAllowedField << YAML::Value << permission.allowed;
      out << YAML::Key << kPermissionLevelField << YAML::Value << permission.level;
      out << YAML::EndMap;
    }
    out << YAML::EndSeq;
  }
  constexpr std::array<std::string_view, 4> kKnownSkillKeys = {kSkillModeField, kSkillReserveField,
                                                               kSkillDelayField, kPermissionsField};
  EmitUnknown(out, old_skills, kKnownSkillKeys);
  out << YAML::EndMap;

  // LES CHAMPS QUE CE CLIENT NE CONNAÎT PAS restent tels quels.
  std::array<std::string_view, kIntSettings.size() + kBoolSettings.size() + kMsSettings.size() + 2>
      known{};
  std::size_t next = 0;
  for (const IntSetting& field : kIntSettings) {
    known[next++] = field.key;
  }
  for (const BoolSetting& field : kBoolSettings) {
    known[next++] = field.key;
  }
  for (const MsSetting& field : kMsSettings) {
    known[next++] = field.key;
  }
  known[next++] = kSeatedField;
  known[next] = kSkillsField;
  EmitUnknown(out, old, known);
  out << YAML::EndMap;
}

}  // namespace moonclient::companion_yaml
