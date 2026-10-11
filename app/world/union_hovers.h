#pragma once

#include <cstdint>
#include <map>

#include "ro/render/actor_sprite.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/union_hover.h"
#include "ro/sim/actor.h"

namespace moonclient {

// LA LÉVITATION DE CHAQUE ACTEUR QUI FLOTTE, d'une image à l'autre : voir
// `ro::render::UnionHover`. Seule l'option du vol de l'union la fait naître,
// quel que soit le métier.
//
// ⚠ L'ÉTAT SURVIT À L'OPTION : le client garde l'angle et le palier d'un acteur
// qui cesse de flotter, et les reprend tels quels s'il revole. Un acteur que la
// scène ne compose plus, lui, les perd.
class UnionHovers {
 public:
  // La hauteur dont `actor` flotte à cette image, en pixels de sprite vers le
  // haut ; zéro sans l'option du vol. `motion` est ce qu'il joue, `red_veiled`
  // dit que le voile rouge d'un effet est levé sur lui, et `composed` l'image
  // dont les parties font avancer l'angle.
  [[nodiscard]] float PixelsOf(const ro::sim::Actor& actor, ro::render::Motion motion,
                               bool red_veiled, const ro::render::ComposedActor& composed,
                               std::uint32_t now_ms);

  // Oublie les acteurs qu'aucune image n'a composés depuis le dernier appel.
  void ForgetUnseen();

 private:
  struct Kept {
    ro::render::UnionHoverClock clock;
    bool seen = false;
  };
  std::map<std::uint32_t, Kept> kept_;
};

// LE NOMBRE DE PARTIES QU'UNE IMAGE PEINT : celles qui ont au moins un calque.
// Une image sans partie déclarée compte pour une dès qu'elle a un calque.
[[nodiscard]] std::uint32_t PartsDrawnOf(const ro::render::ComposedActor& composed);

}  // namespace moonclient
