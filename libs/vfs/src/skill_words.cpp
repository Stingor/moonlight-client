#include "ro/vfs/skill_words.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace ro::vfs {
namespace {

[[nodiscard]] const formats::LuaTable* TableOf(const formats::LuaValue* value) {
  if (value == nullptr || value->kind != formats::LuaValue::Kind::kTable ||
      value->table == nullptr) {
    return nullptr;
  }
  return value->table.get();
}

}  // namespace

std::span<const std::string> SkillWords::Of(std::uint16_t skill_id) const {
  const auto found = by_skill_.find(skill_id);
  if (found == by_skill_.end()) {
    return {};
  }
  return std::span(lines_).subspan(found->second.at, found->second.count);
}

core::Result<SkillWords> SkillWords::FromGlobals(const formats::LuaTable& globals) {
  const formats::LuaTable* descriptions = TableOf(globals.Find(kSkillDescriptGlobal));
  if (descriptions == nullptr) {
    return std::unexpected(core::Error{core::ErrorCode::kCorruptData,
                                       std::string(kSkillDescriptGlobal) + " est absente"});
  }

  SkillWords words;
  // 🔴 LE TAMPON EST RÉSERVÉ EN DEUX PASSES, et c'est ce qui rend les vues
  // valides : une seule réallocation en cours de remplissage ferait pendre
  // toutes celles déjà rendues. Compter d'abord coûte un parcours de plus et
  // supprime la classe entière de ce défaut.
  std::size_t total = 0;
  for (const auto& [skill_id, entry] : descriptions->by_index) {
    const formats::LuaTable* block = TableOf(&entry);
    if (block != nullptr) {
      total += block->by_index.size();
    }
  }
  words.lines_.reserve(total);
  words.by_skill_.reserve(descriptions->by_index.size());

  for (const auto& [key, entry] : descriptions->by_index) {
    const formats::LuaTable* block = TableOf(&entry);
    // ⚠ UN NUMÉRO NUL OU NÉGATIF N'EST PAS UNE COMPÉTENCE : le fichier écrit
    // `[SKID.X]`, et une clé que `SKID` ne connaît pas s'évalue en rien. Un
    // numéro au-delà de seize bits non plus.
    const std::optional<std::uint32_t> skill_id =
        formats::LuaWholeNumber(key, std::numeric_limits<std::uint16_t>::max());
    if (block == nullptr || !skill_id.has_value() || *skill_id == 0) {
      continue;
    }
    const std::size_t at = words.lines_.size();
    for (const auto& [rank, line] : block->by_index) {
      if (line.kind != formats::LuaValue::Kind::kString) {
        continue;
      }
      words.lines_.push_back(line.text);
    }
    const std::size_t count = words.lines_.size() - at;
    // Une description vide ne dit rien de plus qu'une absente, et une entrée qui
    // ne porte aucune ligne ferait croire à un texte perdu.
    if (count == 0) {
      continue;
    }
    words.by_skill_.emplace(static_cast<std::uint16_t>(*skill_id), Block{.at = at, .count = count});
  }
  return words;
}

}  // namespace ro::vfs
