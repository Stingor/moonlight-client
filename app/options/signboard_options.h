#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <imgui.h>

#include "ro/ui/options_window.h"

#include "game/settings_file.h"
#include "options/broadcast_balloon_options.h"
#include "options/shop_options.h"
#include "world/signboard_style.h"

namespace moonclient {

// ── LES ENSEIGNES DES SALONS ET DES ÉCHOPPES, dans un onglet de « Entités » ──
//
// Lesquelles se montrent, leur opacité, leurs couleurs et la police de leur
// titre. Voir `SignboardStyle`.
//
// ⚠ LA BASCULE DES ÉCHOPPES N'EST PAS ICI : elle appartient à `ShopOptions`,
// avec sa commande `/showshop`. L'onglet la compose, `ShopOptions` la change.
inline constexpr int kSignboardRowBase = 9300;

enum class SignboardRow : int {
  kChatRoomsShown = 0,
  kOpacity,
  kChatRoomTint,
  kShopFill,
  kSpotlitFill,
  kShopEdge,
  kTitleColor,
  kFontFamily,
  kFontBold,
  kFontPixels,
  kRestore,
  kCount,
};

static_assert(kBalloonRowBase + static_cast<int>(BalloonRow::kCount) <= kSignboardRowBase,
              "la plage des enseignes chevauche celle des bulles d'annonce");

// Une couleur du rendu, opaque, telle que le nuancier la tient.
[[nodiscard]] constexpr ImU32 SwatchOf(const std::array<std::uint8_t, 3>& color) {
  constexpr std::uint8_t kOpaque = 255;
  return IM_COL32(color[0], color[1], color[2], kOpaque);
}
[[nodiscard]] constexpr ImU32 SwatchOf(const std::array<std::uint8_t, 4>& color) {
  return IM_COL32(color[0], color[1], color[2], color[3]);
}

class SignboardOptions {
 public:
  // CE QUE LE DESSIN REÇOIT. `shops_shown` vient de `ShopOptions`.
  [[nodiscard]] SignboardStyle Style(bool shops_shown) const;

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Les lignes de l'onglet, la bascule des échoppes en tête.
  void ComposeInto(std::vector<ro::ui::OptionRowView>& rows, const ShopOptions& shops);

  // Rend vrai quand la ligne était la sienne.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

 private:
  bool chat_rooms_shown_ = true;
  int opacity_percent_ = 100;
  ImU32 chat_room_tint_ = IM_COL32_WHITE;
  ImU32 shop_fill_ = SwatchOf(ro::render::kShopSignFill);
  ImU32 spotlit_fill_ = SwatchOf(ro::render::kSpotlitShopSignFill);
  ImU32 shop_edge_ = SwatchOf(ro::render::kShopSignEdge);
  ImU32 title_color_ = SwatchOf(ro::render::kSignboardTitleColor);
  std::string font_name_;
  bool font_bold_ = false;
  int font_pixels_ = kNameFontPixels;

  // ⚠ ICI ET NON SUR LA PILE : une rangée garde un `span` de ses choix.
  std::vector<const char*> font_names_;
};

}  // namespace moonclient
