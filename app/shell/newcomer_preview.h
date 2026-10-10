#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "ro/render/actor_thumbnails.h"
#include "ro/render/actor_vignette.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/ui/character_create_window.h"

namespace moonclient {

// LE PERSONNAGE QU'ON EST EN TRAIN DE CRÉER : son pantin, et les coupes qu'on
// lui essaie.
//
// ⚠ DEUX COMPOSEURS ET NON UN, et la différence n'est pas la taille.
// `ActorVignette` montre UN pantin qui change — il refait sa texture quand
// l'image change, ce qu'il faut pour un aperçu qu'on tourne à la molette.
// `ActorThumbnails` montre des CHOIX : chaque case est figée pour toujours, et
// il y en a des centaines, qu'il compose par petits paquets pour ne pas ouvrir
// trois cents sprites dans la même image.
class NewcomerPreview {
 public:
  explicit NewcomerPreview(ro::render::ActorWardrobe& wardrobe) : wardrobe_(&wardrobe) {}

  // Compose ce que la fenêtre de création montre : le pantin, et la tranche de
  // grille qu'elle a demandée à l'image précédente.
  //
  // ⚠ NI L'ATTENTE NI LE REFUS : ils viennent de la machine à états, et
  // l'appelant les pose. Cette classe ne connaît que des sprites.
  [[nodiscard]] ro::ui::CharacterCreateView Compose(const ro::ui::CharacterCreateState& state,
                                                    const ro::ui::CharacterCreateAction& asked,
                                                    std::uint32_t elapsed_ms);

  // Jette les vignettes. ⚠ À LA FERMETURE DE LA FENÊTRE : une grille refermée
  // ne vaut aucune texture, et celles-ci se comptent par centaines.
  void Close();

 private:
  // L'APPARENCE D'UN NOUVEAU-NÉ, telle que l'état la compose.
  [[nodiscard]] static ro::render::ActorLook LookOf(const ro::ui::CharacterCreateState& state);

  ro::render::ActorWardrobe* wardrobe_;
  std::optional<ro::render::ActorVignette> doll_;
  std::optional<ro::render::ActorThumbnails> thumbnails_;
  // Quelle grille est composée en ce moment ; sert à rendre les textures de la
  // précédente au changement, et pas à chaque image.
  ro::ui::CreatePicker shown_ = ro::ui::CreatePicker::kNone;
  std::vector<ro::ui::SkinImage> thumbs_;
};

}  // namespace moonclient
