#pragma once

#include <optional>

#include "ro/render/actor_wardrobe.h"
#include "ro/render/head_button.h"
#include "ro/render/icon_texture.h"

namespace ro::render {

// LE BOUTON DE STATUT À LA TÊTE DU PERSONNAGE, téléversé.
//
// Les images se composent par `StatusHeadArt` — voir `head_button.h` —, et ne
// se téléversent que quand elles changent.
//
// ⚠ ENTRE DEUX IMAGES, jamais au milieu d'un dessin : l'interface tient les
// identifiants des textures rendues, et un changement de tête les remplace.
class StatusHeadIcon {
 public:
  explicit StatusHeadIcon(ActorWardrobe& wardrobe) : art_(wardrobe) {}

  // Refait les textures quand la tête change ; sans clé, elles se vident.
  void Refresh(const std::optional<StatusHeadKey>& key);

  // Les deux états, à double densité ; inutilisables quand il n'y a pas de tête
  // à montrer — l'icône d'avant reprend alors sa place.
  [[nodiscard]] const IconTexture& normal() const noexcept { return normal_; }
  [[nodiscard]] const IconTexture& pressed() const noexcept { return pressed_; }

 private:
  StatusHeadArt art_;
  IconTexture normal_;
  IconTexture pressed_;
};

}  // namespace ro::render
