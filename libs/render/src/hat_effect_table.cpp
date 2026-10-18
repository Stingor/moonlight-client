#include "ro/render/hat_effect_table.h"

#include <algorithm>
#include <expected>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/sorted_table.h"
#include "ro/vfs/encoding.h"

namespace ro::render {
namespace {

constexpr int kSupportedVersion = 1;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData,
                     std::string(kHatEffectTablePath) + " : " + std::move(detail)};
}

// Un drapeau ABSENT vaut faux : la table n'écrit que ceux qui sont vrais.
[[nodiscard]] bool Flag(const YAML::Node& record, const char* field) {
  const YAML::Node value = record[field];
  return value && value.as<bool>();
}

[[nodiscard]] float Number(const YAML::Node& record, const char* field) {
  const YAML::Node value = record[field];
  return value ? value.as<float>() : 0.0F;
}

// Une fiche. Une exception de yaml-cpp, ou un refus.
[[nodiscard]] core::Result<HatEffect> ParseRecord(const YAML::Node& key, const YAML::Node& record) {
  if (!record.IsMap()) {
    return std::unexpected(Refused("fiche mal formée : " + key.as<std::string>()));
  }
  HatEffect effect;
  effect.hat_effect_id = key.as<std::uint16_t>();
  if (const YAML::Node name = record["name"]; name) {
    effect.name = name.as<std::string>();
  }
  if (const YAML::Node file = record["file"]; file) {
    // Le YAML est en UTF-8, le VFS cherche ses chemins en CP949.
    auto path = vfs::Utf8ToCp949(file.as<std::string>());
    if (!path) {
      return std::unexpected(Refused("fichier illisible : " + key.as<std::string>()));
    }
    effect.file = std::move(*path);
  }
  if (const YAML::Node effect_id = record["effect_id"]; effect_id) {
    effect.effect_id = effect_id.as<std::uint32_t>();
  }
  effect.offset = Number(record, "offset");
  effect.offset_x = Number(record, "offset_x");
  effect.adjust_position_when_shrunk = Flag(record, "adjust_position_when_shrunk");
  effect.adjust_size_when_shrunk = Flag(record, "adjust_size_when_shrunk");
  effect.render_before_character = Flag(record, "render_before_character");
  effect.ignore_riding = Flag(record, "ignore_riding");
  effect.attached_head = Flag(record, "attached_head");
  effect.effect_pair = Flag(record, "effect_pair");
  return effect;
}

}  // namespace

core::Result<HatEffectTable> HatEffectTable::Load(vfs::Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kHatEffectTablePath));
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  return Parse(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
}

core::Result<HatEffectTable> HatEffectTable::Parse(std::string_view yaml_text) {
  HatEffectTable loaded;
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const YAML::Node whole = YAML::Load(std::string(yaml_text));
    if (!whole.IsMap()) {
      return std::unexpected(Refused("le texte n'est pas une table YAML"));
    }
    const YAML::Node version = whole["version"];
    if (!version || version.as<int>() != kSupportedVersion) {
      return std::unexpected(Refused("version non gérée"));
    }
    const YAML::Node records = whole["hat_effects"];
    if (!records || !records.IsMap()) {
      return std::unexpected(Refused("« hat_effects » n'est pas une table"));
    }
    loaded.entries_.reserve(records.size());
    for (const auto& pair : records) {
      core::Result<HatEffect> effect = ParseRecord(pair.first, pair.second);
      if (!effect) {
        return std::unexpected(effect.error());
      }
      loaded.entries_.push_back(std::move(*effect));
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  std::ranges::sort(loaded.entries_, {}, &HatEffect::hat_effect_id);
  return loaded;
}

const HatEffect* HatEffectTable::FindByConcreteId(std::uint32_t effect_id) const {
  if (effect_id < kHatEffectIdBase) {
    return nullptr;
  }
  return Find(effect_id - kHatEffectIdBase);
}

const HatEffect* HatEffectTable::Find(std::uint32_t hat_effect_id) const {
  return core::RowWithKey(entries_, hat_effect_id, &HatEffect::hat_effect_id);
}

}  // namespace ro::render
