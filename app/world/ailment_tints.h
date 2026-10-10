#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>

#include "ro/render/ailment_sprites.h"
#include "ro/sim/world_fwd.h"

// LA TEINTE QUE LES ALTÉRATIONS ÉCRIVENT SUR CHAQUE ACTEUR, et qui reste.
//
// 🔴 LE CLIENT NE RECALCULE PAS CETTE TEINTE À CHAQUE IMAGE : il l'écrit aux
// changements d'état, à la pose d'un handicap, à la prise de Blade Stop, et ce
// qu'il a écrit tient jusqu'à l'écriture suivante. Un gelé guéri de son poison
// garde ainsi la teinte du poison jusqu'au dégel. Voir
// `ro::render::AilmentTintLatch`.
//
// ⚠ ELLE VIT CÔTÉ `app`, et non dans le monde : le poison ordinaire s'efface
// devant l'effet 812, et seul `app` sait si un effet vit encore à son âge.
namespace moonclient {

class AilmentTints {
 public:
  // Relit le monde : ce qui a changé depuis la dernière fois écrit sa teinte,
  // et qui est parti est oublié. Un acteur neuf passe par le changement
  // d'état de son apparition.
  void Observe(const ro::sim::World& world, std::uint32_t now_ms);

  // La teinte qu'un acteur porte, ou rien quand il est blanc — ses effets
  // décident alors. Rien non plus pour un acteur jamais observé.
  [[nodiscard]] std::optional<std::array<std::uint8_t, 4>> TintOf(std::uint32_t actor_id) const;

  // Tout oublier : au changement de carte.
  void Forget() { seen_.clear(); }

 private:
  // Ce qu'on a vu d'un acteur au dernier passage, et ce qu'il porte.
  struct Seen {
    ro::render::AilmentTintLatch latch;
    std::uint16_t body_state = 0;
    std::uint32_t state_change_count = 0;
    bool blade_stop = false;
    std::array<bool, ro::render::kTintingHandicaps> handicaps{};
  };
  std::unordered_map<std::uint32_t, Seen> seen_;
};

}  // namespace moonclient
