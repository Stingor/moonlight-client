#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "ro/render/actor_sprite.h"

namespace moonclient {

// LE SON QU'INSCRIT L'IMAGE DU HÉROS : le .act de son corps nomme un bruit sur
// certaines images — la corde de l'arc qui se détend au tir de l'Archer. Il
// sonne quand le héros ARRIVE sur cette image,
// une fois, et non à chaque peinture où il s'y tient.
class HeroFrameSound {
 public:
  // Le bruit à jouer pour cette image du héros, en UTF-8 ; rien quand l'image
  // n'a pas changé, ou qu'elle ne nomme aucun bruit. Sans héros, l'image vue
  // s'oublie : le héros qui reparaît sonne de nouveau.
  [[nodiscard]] std::optional<std::string> Hear(const ro::render::ComposedActor* hero);

 private:
  // L'action et l'image vues à la dernière peinture.
  std::optional<std::uint64_t> seen_;
};

}  // namespace moonclient
