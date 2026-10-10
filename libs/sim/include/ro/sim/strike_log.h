#pragma once

#include <algorithm>
#include <cstddef>
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

// UNE COMPÉTENCE SANS DÉGÂTS ENREGISTRÉE, sa cible, et son rang dans l'ordre
// d'arrivée.
struct RecordedSupport {
  std::uint64_t serial = 0;
  std::uint32_t target_id = 0;
  SupportHit support;
};

// LES COMPÉTENCES SANS DÉGÂTS ARRIVÉES, DANS L'ORDRE, pour qui doit les voir
// TOUTES.
//
// 🔴 `Actor::support` NE GARDE QUE LA DERNIÈRE. Or le serveur en envoie souvent
// deux d'affilée sur la même cible : Potion Pitcher annonce la compétence, puis
// un Heal sans lanceur qui porte le montant ; une potion qui rend aussi des SP
// ajoute un troisième paquet. Un lecteur qui reconnaissait une compétence
// NOUVELLE à son instant ne voyait que la dernière, et la potion lancée ne
// partait jamais. Chacune a ici son numéro, et chaque lecteur tient un
// `SupportCursor`.
//
// ⚠ LE JOURNAL EST BORNÉ, parce qu'un lecteur qui ne passe plus ne doit pas le
// faire grossir. Il garde bien plus d'entrées qu'une image n'en apporte.
class SupportLog {
 public:
  void Record(std::uint32_t target_id, const SupportHit& support) {
    // ⚠ LA PURGE SE FAIT PAR MOITIÉ, pas entrée par entrée : effacer en tête
    // d'un vecteur à chaque ajout recopierait tout le reste à chaque fois.
    if (supports_.size() >= kKeptSupports * 2) {
      supports_.erase(supports_.begin(), supports_.begin() + static_cast<std::ptrdiff_t>(
                                                                 supports_.size() - kKeptSupports));
    }
    supports_.push_back(
        RecordedSupport{.serial = next_serial_, .target_id = target_id, .support = support});
    ++next_serial_;
  }
  // Les numéros continuent : un curseur déjà avancé ne revoit rien.
  void Clear() noexcept { supports_.clear(); }

  // Les entrées dont le numéro dépasse `serial`, dans l'ordre d'arrivée.
  [[nodiscard]] std::span<const RecordedSupport> Since(std::uint64_t serial) const {
    const auto first = std::ranges::upper_bound(
        supports_, serial, {}, [](const RecordedSupport& kept) { return kept.serial; });
    return {first, supports_.end()};
  }
  [[nodiscard]] std::uint64_t last_serial() const noexcept { return next_serial_ - 1; }

 private:
  // Ce que le journal garde au moins : des centaines d'entrées, quand une
  // image en apporte quelques-unes.
  static constexpr std::size_t kKeptSupports = 256;

  std::vector<RecordedSupport> supports_;
  std::uint64_t next_serial_ = 1;
};

// CE QU'UN LECTEUR A DÉJÀ VU du journal.
class SupportCursor {
 public:
  // Les entrées arrivées depuis le dernier appel ; le curseur passe au-delà.
  //
  // ⚠ UN JOURNAL NEUF — un nouveau monde, une nouvelle session — recommence à
  // un : un curseur plus avancé que lui repart de zéro, sans quoi il ne
  // verrait plus rien avant d'avoir rattrapé l'ancien compte.
  [[nodiscard]] std::span<const RecordedSupport> Take(const SupportLog& log) {
    if (seen_serial_ > log.last_serial()) {
      seen_serial_ = 0;
    }
    const std::span<const RecordedSupport> fresh = log.Since(seen_serial_);
    seen_serial_ = log.last_serial();
    return fresh;
  }

 private:
  std::uint64_t seen_serial_ = 0;
};

}  // namespace ro::sim
