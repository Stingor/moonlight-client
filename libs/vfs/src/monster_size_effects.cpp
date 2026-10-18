// La taille et les effets d'office de certains monstres. Voir l'en-tête.

#include "ro/vfs/monster_size_effects.h"

#include <algorithm>
#include <cmath>
#include <expected>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "yaml_required.h"

namespace ro::vfs {
namespace {

constexpr int kSupportedVersion = 1;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData,
                     std::string(kMonsterSizeEffectPath) + " : " + std::move(detail)};
}

}  // namespace

core::Result<MonsterSizeEffects> MonsterSizeEffects::Load(Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kMonsterSizeEffectPath));
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  return Parse(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
}

core::Result<MonsterSizeEffects> MonsterSizeEffects::Parse(std::string_view yaml_text) {
  MonsterSizeEffects loaded;
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const auto root = VersionedRoot(yaml_text, kSupportedVersion);
    if (!root) {
      return std::unexpected(Refused(root.error()));
    }
    const YAML::Node& whole = *root;
    const YAML::Node monsters = whole["monsters"];
    if (!monsters || !monsters.IsMap()) {
      return std::unexpected(Refused("« monsters » n'est pas une table"));
    }
    for (const auto& pair : monsters) {
      const YAML::Node& entry = pair.second;
      if (!entry.IsMap()) {
        return std::unexpected(Refused("entrée mal formée : " + pair.first.as<std::string>()));
      }
      const YAML::Node size = entry["size"];
      const YAML::Node effects = entry["effects"];
      if (!size || !size.IsScalar() || !effects || !effects.IsSequence()) {
        return std::unexpected(Refused("entrée mal formée : " + pair.first.as<std::string>()));
      }
      Entry read{
          .monster_id = pair.first.as<std::uint32_t>(), .size = size.as<float>(), .effect_ids = {}};
      if (!std::isfinite(read.size)) {
        return std::unexpected(Refused("taille non finie : " + pair.first.as<std::string>()));
      }
      for (const YAML::Node& effect : effects) {
        read.effect_ids.push_back(effect.as<std::uint32_t>());
      }
      loaded.entries_.push_back(std::move(read));
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  std::ranges::sort(loaded.entries_, {}, &Entry::monster_id);
  return loaded;
}

const MonsterSizeEffects::Entry* MonsterSizeEffects::For(std::uint32_t monster_id) const {
  const auto found = std::ranges::lower_bound(entries_, monster_id, {}, &Entry::monster_id);
  if (found == entries_.end() || found->monster_id != monster_id) {
    return nullptr;
  }
  return &*found;
}

float MonsterSizeEffects::ScaleOf(std::uint32_t monster_id) const {
  const Entry* entry = For(monster_id);
  return entry == nullptr || entry->size <= 0.0F ? 1.0F : entry->size;
}

}  // namespace ro::vfs
