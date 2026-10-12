#include "ro/vfs/skill_words.h"

#include <cstdint>
#include <iterator>
#include <string>
#include <utility>

namespace ro::vfs {

std::span<const std::string> SkillWords::Of(std::uint16_t skill_id) const {
  const auto found = by_skill_.find(skill_id);
  if (found == by_skill_.end()) {
    return {};
  }
  return std::span(lines_).subspan(found->second.at, found->second.count);
}

SkillWords SkillWords::FromRecords(std::span<SkillInfoRecord> records) {
  SkillWords words;
  // 🔴 LE TAMPON EST RÉSERVÉ D'ABORD, et c'est ce qui rend les vues valides :
  // une seule réallocation en cours de remplissage ferait pendre toutes celles
  // déjà rendues. Compter coûte un parcours de plus et supprime la classe
  // entière de ce défaut.
  std::size_t total = 0;
  for (const SkillInfoRecord& record : records) {
    total += record.description.size();
  }
  words.lines_.reserve(total);
  words.by_skill_.reserve(records.size());

  for (SkillInfoRecord& record : records) {
    if (record.description.empty()) {
      continue;
    }
    const std::size_t at = words.lines_.size();
    words.lines_.insert(words.lines_.end(), std::make_move_iterator(record.description.begin()),
                        std::make_move_iterator(record.description.end()));
    record.description.clear();
    words.by_skill_.emplace(record.skill_id, Block{.at = at, .count = words.lines_.size() - at});
  }
  return words;
}

}  // namespace ro::vfs
