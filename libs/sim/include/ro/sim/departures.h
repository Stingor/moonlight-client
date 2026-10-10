#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "ro/sim/actor.h"
#include "ro/sim/movement.h"

namespace ro::sim {

// UN EFFET QUI SE JOUE LÀ OÙ UN ACTEUR ÉTAIT, au moment où il s'en va.
//
// Le client retire l'acteur sur-le-champ et laisse à sa place un lanceur qui
// ne lui appartient pas : le porteur n'existe plus quand l'effet naît. C'est
// donc la PLACE qui voyage, pas l'identifiant.
struct Departure {
  std::uint32_t effect_id = 0;
  FractionalCell cell;
  std::uint32_t since_ms = 0;
};

// UN ACTEUR SORTI DE VUE, qui s'efface encore.
//
// Le client ne le détruit pas tout de suite : il lui retire son identifiant —
// plus rien ne le désigne ni ne le vise — et le laisse s'effacer. Le monde, lui,
// l'a déjà oublié ; c'est cette COPIE qui se dessine le temps du fondu.
struct FadingActor {
  Actor actor;
  std::uint32_t since_ms = 0;
};

// Ce que les départs laissent, pas encore pris en charge, du plus ancien au plus
// récent. Qui les prend les EMPORTE : un effet ne part qu'une fois.
//
// ⚠ BORNÉES : seize suffisent largement entre deux tours, et un monde que
// personne ne lit ne doit pas grandir sans fin.
class Departures {
 public:
  static constexpr std::size_t kKept = 16;

  void Note(std::uint32_t effect_id, FractionalCell cell, std::uint32_t since_ms) {
    Keep(pending_, Departure{effect_id, cell, since_ms});
  }
  void Fade(const Actor& actor, std::uint32_t since_ms) {
    Keep(fading_, FadingActor{actor, since_ms});
  }

  [[nodiscard]] std::span<const Departure> pending() const noexcept { return pending_; }
  [[nodiscard]] std::span<const FadingActor> fading() const noexcept { return fading_; }
  [[nodiscard]] std::vector<Departure> Take() { return std::exchange(pending_, {}); }
  [[nodiscard]] std::vector<FadingActor> TakeFading() { return std::exchange(fading_, {}); }

 private:
  template <typename Item>
  static void Keep(std::vector<Item>& items, Item item) {
    if (items.size() >= kKept) {
      items.erase(items.begin());
    }
    items.push_back(std::move(item));
  }

  std::vector<Departure> pending_;
  std::vector<FadingActor> fading_;
};

}  // namespace ro::sim
