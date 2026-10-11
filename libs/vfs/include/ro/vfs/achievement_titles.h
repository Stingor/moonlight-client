#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/vfs/achievement_database.h"

namespace ro::vfs {

// LE TITRE QUE RÉCOMPENSE CHAQUE SUCCÈS.
//
// 🔴 LE SERVEUR NE L'ENVOIE JAMAIS. La liste des succès porte des numéros de
// succès et des drapeaux ; le titre qu'un succès donne se lit dans sa base,
// exportée dans `AchievementDatabase`, et c'est le client qui fait le
// croisement. C'est la même base avec laquelle le serveur accepte ou refuse un
// titre.
class AchievementTitles {
 public:
  // Les titres de la base, dans l'ordre de ses succès.
  [[nodiscard]] static AchievementTitles Of(const AchievementDatabase& database);

  // Le titre qu'un succès récompense, ou rien. ⚠ RIEN EST LE CAS ORDINAIRE : la
  // plupart des succès donnent un objet ou rien du tout.
  [[nodiscard]] std::optional<std::int32_t> TitleOf(std::uint32_t achievement_id) const;

  struct Entry {
    std::uint32_t achievement_id = 0;
    std::int32_t title_id = 0;
  };

  [[nodiscard]] std::span<const Entry> entries() const noexcept { return entries_; }

 private:
  std::vector<Entry> entries_;
};

}  // namespace ro::vfs
