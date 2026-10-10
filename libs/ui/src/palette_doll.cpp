#include "palette_doll.h"

#include <algorithm>

// ⚠ POUR LE NOMBRE DE DIRECTIONS D'UN .ACT : la molette tourne le pantin dans
// la numérotation du fichier, et en redéclarer le compte ici en ferait une
// seconde vérité.
#include "ro/render/actor_layers.h"
#include "ro/ui/character_window.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_widgets.h"

namespace ro::ui {
namespace {

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

}  // namespace

void DrawPaletteDoll(PaletteDollState& doll, float side, const SkinLook& look) {
  const ImVec2 at = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("##pantin", ImVec2(side, side));
  const bool hovered = ImGui::IsItemHovered();

  ImDrawList* into = ImGui::GetWindowDrawList();
  into->AddRectFilled(at, ImVec2(at.x + side, at.y + side), look.doll,
                      kEquipSlotRounding * Scale());

  const float padding = kCharacterAvatarPadding * Scale();
  doll.box_width = std::max(0.0F, side - 2.0F * padding);
  doll.box_height = doll.box_width;

  // ⚠ LE POINT EST RELEVÉ DANS LA BOÎTE, pas dans le carré : la silhouette est
  // dessinée centrée sur la boîte, et c'est d'elle que `app` tirera les unités
  // de l'acteur. Un point pris depuis le coin du carré serait décalé de la
  // marge, ce qui ne se verrait pas — la pipette désignerait simplement la
  // pièce d'à côté.
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  doll.hovered = hovered;
  doll.hover_x = mouse.x - (at.x + (side - doll.box_width) * 0.5F);
  doll.hover_y = mouse.y - (at.y + (side - doll.box_height) * 0.5F);

  if (doll.image.usable()) {
    // ⚠ ÉTIRÉE SUR LA BOÎTE : la texture est composée en pixels de sprite, et
    // c'est la carte qui l'agrandit — au plus proche, la silhouette se
    // découperait en marches d'escalier.
    const ImVec2 image_at(at.x + (side - doll.image.width) * 0.5F,
                          at.y + (side - doll.image.height) * 0.5F);
    into->AddImage(doll.image.id, image_at,
                   ImVec2(image_at.x + doll.image.width, image_at.y + doll.image.height));
  }

  if (!hovered) {
    return;
  }
  SkinTooltip(core::Tr("Molette : tourner"));
  // 🔴 LA MOLETTE PASSE PAR LE VERROU : sans lui, parcourir la fenêtre ferait
  // pivoter le pantin au passage — et le faire pivoter ferait défiler la
  // fenêtre.
  const float wheel = WheelOver(doll.wheel, ImGui::GetItemID(), hovered);
  if (wheel == 0.0F) {
    return;
  }
  const auto turns = static_cast<int>(render::kDirectionCount);
  doll.direction = static_cast<std::uint8_t>(
      (static_cast<int>(doll.direction) + (wheel > 0.0F ? 1 : turns - 1)) % turns);
}

}  // namespace ro::ui
