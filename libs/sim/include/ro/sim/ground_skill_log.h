#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "ro/sim/combat_notice.h"

namespace ro::sim {

// UNE COMPÉTENCE POSÉE SUR UNE CASE PAR UN LANCEUR QU'ON NE VOIT PAS, et son
// rang dans l'ordre d'arrivée.
struct UnseenGroundSkill {
  std::uint64_t serial = 0;
  std::uint32_t caster_id = 0;
  GroundSkillUse use;
};

// LES COMPÉTENCES POSÉES SUR UNE CASE PAR UN LANCEUR HORS DE VUE, DANS L'ORDRE.
//
// 🔴 ELLES SE MONTRENT QUAND MÊME. Le serveur annonce la compétence à qui voit
// la CASE, et son lanceur peut être au-delà de la vue : ce qui naît sur la case
// — les sons, les effets, les lanceurs sans corps, les effets nommés au sol —
// naît sans lui. Seul ce qui naîtrait SUR lui manque. Faute d'acteur où la
// ranger, la compétence attend ici qu'un lecteur la joue.
//
// ⚠ LE JOURNAL EST BORNÉ, comme celui des coups : un lecteur qui ne passe plus
// ne doit pas le faire grossir.
class UnseenGroundSkillLog {
 public:
  void Record(std::uint32_t caster_id, const GroundSkillUse& use);
  // Les numéros continuent : un curseur déjà avancé ne revoit rien.
  void Clear() noexcept { skills_.clear(); }

  // Celles dont le numéro dépasse `serial`, dans l'ordre d'arrivée.
  [[nodiscard]] std::span<const UnseenGroundSkill> Since(std::uint64_t serial) const;
  [[nodiscard]] std::uint64_t last_serial() const noexcept { return next_serial_ - 1; }

 private:
  std::vector<UnseenGroundSkill> skills_;
  std::uint64_t next_serial_ = 1;
};

// CE QU'UN LECTEUR A DÉJÀ VU du journal.
class UnseenGroundSkillCursor {
 public:
  // Celles arrivées depuis le dernier appel ; le curseur passe au-delà. Un
  // journal neuf, moins avancé que lui, le fait repartir de zéro.
  [[nodiscard]] std::span<const UnseenGroundSkill> Take(const UnseenGroundSkillLog& log);

 private:
  std::uint64_t seen_serial_ = 0;
};

}  // namespace ro::sim
