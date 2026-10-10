#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "ro/sim/combat_notice.h"

namespace ro::sim {

// UN COUP ENREGISTRÉ, et son rang dans l'ordre d'arrivée.
struct RecordedStrike {
  std::uint64_t serial = 0;
  Strike strike;
};

// LES COUPS ARRIVÉS, DANS L'ORDRE, pour qui doit les voir TOUS.
//
// 🔴 `Actor::hit` ET `Actor::attack` NE GARDENT QUE LE DERNIER : c'est ce que la
// pose lit, et elle n'a besoin de rien d'autre. Mais deux coups sur la même
// cible dans la même trame portent le même instant, et un lecteur qui
// reconnaissait un coup NOUVEAU à cet instant perdait le premier — son nombre,
// son bruit, son effet. Chaque coup a ici son numéro, et chaque lecteur tient
// un `StrikeCursor`.
//
// ⚠ LE JOURNAL EST BORNÉ, parce qu'un lecteur qui ne passe plus ne doit pas le
// faire grossir. Il garde bien plus de coups qu'une image n'en apporte.
class StrikeLog {
 public:
  void Record(const Strike& strike);
  // Les numéros continuent : un curseur déjà avancé ne revoit rien.
  void Clear() noexcept { strikes_.clear(); }

  // Les coups dont le numéro dépasse `serial`, dans l'ordre d'arrivée.
  [[nodiscard]] std::span<const RecordedStrike> Since(std::uint64_t serial) const;
  [[nodiscard]] std::uint64_t last_serial() const noexcept { return next_serial_ - 1; }

 private:
  std::vector<RecordedStrike> strikes_;
  std::uint64_t next_serial_ = 1;
};

// CE QU'UN LECTEUR A DÉJÀ VU du journal.
class StrikeCursor {
 public:
  // Les coups arrivés depuis le dernier appel ; le curseur passe au-delà.
  //
  // ⚠ UN JOURNAL NEUF — un nouveau monde, une nouvelle session — recommence à
  // un : un curseur plus avancé que lui repart de zéro, sans quoi il ne
  // verrait plus rien avant d'avoir rattrapé l'ancien compte.
  [[nodiscard]] std::span<const RecordedStrike> Take(const StrikeLog& log);

 private:
  std::uint64_t seen_serial_ = 0;
};

}  // namespace ro::sim
