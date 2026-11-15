// LES POLICES DE LA PARTIE QUI SE CHARGENT À LA DEMANDE : celle des noms, à la
// taille réglée, et celles des enseignes de boutique.

#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <utility>

#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"
#include "ro/render/entity_names.h"
#include "ro/render/font_atlas.h"
#include "ro/render/overlay_renderer.h"
#include "ro/ui/system_fonts.h"

#include "game/game_pieces.h"
#include "world/overlay_view.h"
#include "world/signboard_style.h"

namespace moonclient {

const ro::render::FontAtlas* EnsureNameFont(GamePieces& pieces, float pixels) {
  const float wanted = std::round(
      std::clamp(pixels, ro::render::kSmallestNamePlateFont, ro::render::kLargestNamePlateFont));
  if (!pieces.name_font->has_value() || pieces.name_font_pixels != wanted) {
    auto loaded = ro::render::FontAtlas::Load(DefaultFontPath(), static_cast<int>(wanted));
    if (loaded) {
      // 🔴 OUBLIER L'ANCIENNE TEXTURE AVANT DE DÉTRUIRE L'IMAGE, et c'est ce
      // qui manquait : la couche d'écran reconnaît un atlas par son ADRESSE, et
      // une police regravée dans le même `optional` retombe exactement à la
      // même. Sans cet oubli, elle rendait l'ANCIENNE texture aux nouvelles
      // coordonnées de glyphe — les noms sortaient en charabia.
      if (pieces.name_font->has_value()) {
        pieces.overlay->ForgetAtlas((*pieces.name_font)->image());
      }
      *pieces.name_font = std::move(*loaded);
      pieces.name_font_pixels = wanted;
    } else {
      // ⚠ ON NE RÉESSAIE PAS À CHAQUE IMAGE : la taille est retenue même en
      // échec, sans quoi une police introuvable ferait relire le fichier
      // soixante fois par seconde. Le repli est la police de l'interface.
      pieces.name_font_pixels = wanted;
      ro::core::LogWarning("police des noms : {} — les noms garderont la police de l'interface",
                           ro::core::Describe(loaded.error()));
    }
  }
  if (pieces.name_font->has_value()) {
    return &**pieces.name_font;
  }
  return pieces.font->has_value() ? &**pieces.font : nullptr;
}

namespace {

// LE FICHIER D'UNE FAMILLE NOMMÉE — son gras quand on le veut et qu'elle en a
// un —, ou vide quand la machine ne la porte pas.
[[nodiscard]] std::string SignboardFontFile(const SignboardStyle& style) {
  for (const ro::ui::FontFamilySpec& family : ro::ui::SystemFontFamilies()) {
    if (family.label == style.font_name) {
      return std::string(style.font_bold && !family.bold.empty() ? family.bold : family.regular);
    }
  }
  return {};
}

}  // namespace

const ro::render::FontAtlas* EnsureSignboardFont(GamePieces& pieces, const SignboardStyle& style) {
  const ro::render::FontAtlas* game_font = pieces.font->has_value() ? &**pieces.font : nullptr;
  if (UsesGameFont(style)) {
    return game_font;
  }
  const std::string file = style.font_name.empty() ? ro::core::Utf8FromPath(DefaultFontPath())
                                                   : SignboardFontFile(style);
  if (file.empty()) {
    return game_font;
  }
  const int pixels =
      std::clamp(style.font_pixels, kSmallestSignboardFontPixels, kLargestSignboardFontPixels);
  const std::string key = std::format("{}|{}", file, pixels);
  if (pieces.signboard_font_key != key) {
    // ⚠ LE CHOIX EST RETENU MÊME EN ÉCHEC, pour la raison de `EnsureNameFont` :
    // sans quoi une police illisible se relirait à chaque image.
    pieces.signboard_font_key = key;
    if (pieces.signboard_font->has_value()) {
      pieces.overlay->ForgetAtlas((*pieces.signboard_font)->image());
      pieces.signboard_font->reset();
    }
    auto loaded = ro::render::FontAtlas::Load(ro::core::PathFromUtf8(file), pixels);
    if (loaded) {
      *pieces.signboard_font = std::move(*loaded);
    } else {
      ro::core::LogWarning("police des enseignes : {} — elles garderont la police du jeu",
                           ro::core::Describe(loaded.error()));
    }
  }
  return pieces.signboard_font->has_value() ? &**pieces.signboard_font : game_font;
}

void ForgetShopSignboards(GamePieces& pieces) {
  pieces.overlay->ForgetAtlas(pieces.signboards->vending);
  pieces.overlay->ForgetAtlas(pieces.signboards->buying);
  pieces.overlay->ForgetAtlas(pieces.signboards->spotlit_vending);
  pieces.overlay->ForgetAtlas(pieces.signboards->spotlit_buying);
}

}  // namespace moonclient
