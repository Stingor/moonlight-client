#pragma once

#include <cstddef>
#include <vector>

#include "ro/formats/act.h"
#include "ro/formats/image.h"
#include "ro/formats/spr.h"

namespace ro::render {

// LES IMAGES D'UNE ACTION, composées chacune sur une toile COMMUNE : une animation
// d'interface qu'on peint image par image, comme le sablier d'attente.
//
// 🔴 UNE TOILE POUR TOUTES : l'emprise est celle de l'action entière, et non
// celle de chaque image. Recadrer chacune séparément ferait sautiller le dessin
// d'une image à l'autre, alors qu'il ne bouge que de ce que le fichier dit.
//
// ⚠ LES ROTATIONS DES CALQUES S'APPLIQUENT : ce n'est pas un acteur, et un
// dessin qui tourne doit tourner. Sans GPU — voir `SpriteCanvas`.
struct ActionFrames {
  // Toutes à la même taille. Vide quand l'action manque ou ne dessine rien.
  std::vector<formats::Image> frames;
  // La durée d'une image, déclarée par l'action.
  float frame_interval_ms = 0.0F;
};

[[nodiscard]] ActionFrames ComposeActionFrames(const formats::SprSprite& sprite,
                                               const formats::ActAnimation& animation,
                                               std::size_t action_index);

}  // namespace ro::render
