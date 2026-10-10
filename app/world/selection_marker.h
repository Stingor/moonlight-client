#pragma once

#include <array>
#include <cstdint>
#include <span>

#include <glm/glm.hpp>

#include "ro/render/actor_sprite.h"
#include "ro/render/sprite_renderer.h"

#include "world/overlay_view.h"

namespace moonclient {

// LA FLÈCHE AU-DESSUS DE LA CIBLE : le petit triangle blanc du curseur du jeu,
// posé dans la scène au pied de l'entité désignée. Ce sont les calques de son
// action qui le font flotter au-dessus de la tête, pas le code.
class SelectionMarker {
 public:
  // Ce qui se dessine à cet instant, APRÈS les acteurs. Vide sans sprite de
  // curseur.
  //
  // ⚠ SON ANIMATION COMPTE DEPUIS L'ENTRÉE SUR LA CARTE, et non depuis qu'elle
  // paraît : elle tourne même cachée, et reparaît là où elle en est.
  //
  // `tint` est la lumière de la carte là où elle se pose : la flèche la reçoit
  // comme les corps.
  [[nodiscard]] std::span<const ro::render::DrawnActor> Drawn(
      const CursorSprite& art, const glm::vec3& ground, std::uint32_t since_map_entry_ms,
      const std::array<std::uint8_t, 4>& tint);

 private:
  ro::render::ComposedActor frame_;
  ro::render::DrawnActor drawn_;
};

}  // namespace moonclient
