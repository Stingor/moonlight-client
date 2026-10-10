#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>

#include "ro/sim/world_fwd.h"

namespace moonclient {

// 🔴 CE QU'UNE POSE FIGÉE TIENT, noté à l'instant où elle se fige.
//
// Un état que le pas du monde n'avance pas ne recalcule plus rien : ni l'image,
// ni l'action, ni la DIRECTION À L'ÉCRAN — celle que le regard et la caméra
// donnaient quand la motion a changé. L'acteur qui se tourne ensuite, la
// caméra qui pivote, n'y changent plus rien. Une créature y garde en plus
// l'action et l'image qu'elle montrait à cet instant-là.
//
// Rien de cela ne se relit après coup : ni le regard d'avant un demi-tour, ni
// l'angle passé de la caméra, ni l'image qu'une boucle montrait. On le note
// donc à la première image où la pose paraît, d'après ce que l'image
// précédente a montré.
class FrozenPoses {
 public:
  // Ce qu'un acteur montre à l'écran : le rang, la direction, l'image.
  struct Shown {
    std::size_t animation = 0;
    std::size_t direction = 0;
    std::size_t frame = 0;
  };

  // CE QUE LA POSE FIGÉE DEPUIS `since_ms` MONTRE : la direction de l'instant
  // où elle s'est figée, et — quand `keeps_shown_image` — le rang et l'image
  // que l'acteur montrait juste avant. `now` est ce que l'acteur montrerait à
  // cette image sans la pose ; il sert à la noter la première fois, et de
  // repli quand l'acteur n'avait encore rien montré.
  [[nodiscard]] Shown Hold(std::uint32_t actor_id, std::uint32_t since_ms, bool keeps_shown_image,
                           const Shown& now);

  // Ce que l'acteur a montré à cette image, que la pose suivante gardera.
  void NoteShown(std::uint32_t actor_id, const Shown& shown);

  // Oublie qui a quitté le monde.
  void Retain(const ro::sim::World& world);

 private:
  struct Kept {
    std::optional<Shown> last_shown;
    std::optional<std::uint32_t> frozen_since_ms;
    Shown frozen;
  };
  std::unordered_map<std::uint32_t, Kept> kept_;
};

}  // namespace moonclient
