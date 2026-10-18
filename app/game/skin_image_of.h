#pragma once

// UNE TEXTURE TÉLÉVERSÉE, VUE PAR L'INTERFACE : l'identifiant et la taille d'une
// `SkinImage`. `ui` ne connaît pas la carte graphique ; c'est ici qu'`app`
// traduit une `IconTexture` pour elle.

#include <cstdint>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/render/game_images.h"
#include "ro/render/icon_texture.h"
#include "ro/render/item_icons.h"
#include "ro/render/skin_pieces.h"
#include "ro/ui/skin_art.h"

namespace moonclient {

// L'IMAGE D'UNE ICÔNE À LA TAILLE DE SON BITMAP, même absente : l'identifiant
// est nul quand la texture manque, et la case garde sa place.
[[nodiscard]] inline ro::ui::SkinImage SizedSkinImageOf(const ro::render::IconTexture& icon) {
  return ro::ui::SkinImage{
      .id = icon.usable() ? static_cast<ImTextureID>(icon.texture.name()) : ImTextureID{0},
      .width = static_cast<float>(icon.width_px),
      .height = static_cast<float>(icon.height_px)};
}

// L'IMAGE D'UNE TEXTURE, VIDE QUAND ELLE EST INUTILISABLE : `usable()` est faux,
// et l'appelant se rabat sur ce qu'il montre sans elle.
[[nodiscard]] inline ro::ui::SkinImage SkinImageOf(const ro::render::IconTexture& icon) {
  if (!icon.usable()) {
    return ro::ui::SkinImage{};
  }
  return ro::ui::SkinImage{.id = static_cast<ImTextureID>(icon.texture.name()),
                           .width = static_cast<float>(icon.width_px),
                           .height = static_cast<float>(icon.height_px)};
}

// L'ICÔNE DE CET OBJET, à la taille de son bitmap ; vide sans icônes.
[[nodiscard]] inline ro::ui::SkinImage ItemIconImage(ro::render::ItemIcons* icons,
                                                     std::uint32_t item_id) {
  if (icons == nullptr) {
    return {};
  }
  return SizedSkinImageOf(icons->Get(item_id));
}

// UNE IMAGE DE L'HABILLAGE, désignée par son chemin sous le dossier
// d'interface ; vide sans images, ou quand l'installation ne l'a pas.
//
// ⚠ `GetOurs` ET NON `Get` : le chemin est écrit dans NOS sources, en UTF-8,
// et le dossier d'interface est en coréen.
[[nodiscard]] inline ro::ui::SkinImage SkinImageAt(ro::render::GameImages* images,
                                                   std::string_view leaf) {
  if (images == nullptr) {
    return {};
  }
  std::string path(ro::render::kSkinRoot);
  path += leaf;
  return SkinImageOf(images->GetOurs(path));
}

// CE QU'UNE PIÈCE PEINTE SUR DU BLANC MONTRE SUR UN PAPIER SOMBRE : l'art
// d'interface retourné, ou l'objet peint détouré.
using DarkPaperVariant =
    const ro::render::IconTexture& (ro::render::GameImages::*)(std::string_view);

// UNE PIÈCE DE L'HABILLAGE PEINTE SUR DU BLANC, avec sa variante pour un papier
// sombre quand l'installation permet de la tirer.
[[nodiscard]] inline ro::ui::SkinImage PaperArtAt(ro::render::GameImages* images,
                                                  std::string_view leaf, DarkPaperVariant variant) {
  ro::ui::SkinImage image = SkinImageAt(images, leaf);
  if (!image.usable()) {
    return image;
  }
  std::string path(ro::render::kSkinRoot);
  path += leaf;
  if (const ro::render::IconTexture& turned = (images->*variant)(path); turned.usable()) {
    image.dark_paper_id = static_cast<ImTextureID>(turned.texture.name());
  }
  return image;
}

}  // namespace moonclient
