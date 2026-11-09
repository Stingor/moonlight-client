#include "palette_doll.h"

#include <algorithm>
#include <array>

// ⚠ POUR LE NOMBRE DE DIRECTIONS D'UN .ACT : la molette tourne le pantin dans
// la numérotation du fichier, et en redéclarer le compte ici en ferait une
// seconde vérité.
#include "ro/render/actor_layers.h"
#include "ro/ui/character_window.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_widgets.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// Le fond clair du pantin : un gris de papier, assez pâle pour juger un
// sombre, assez terne pour ne pas éblouir à côté de l'habillage.
constexpr ImU32 kLightBackground = IM_COL32(214, 210, 200, 255);
// La largeur de la liste des poses, en largeurs de police.
constexpr float kPoseFieldInFonts = 6.0F;

// Le pas d'un quart de tour, en directions du .ACT.
[[nodiscard]] std::uint8_t Turned(std::uint8_t direction, bool forward) {
  const auto turns = static_cast<int>(render::kDirectionCount);
  return static_cast<std::uint8_t>((static_cast<int>(direction) + (forward ? 1 : turns - 1)) %
                                   turns);
}

}  // namespace

void DrawPaletteDoll(PaletteDollState& doll, float side, const SkinLook& look, bool pipette) {
  const ImVec2 at = ImGui::GetCursorScreenPos();
  doll.clicked = ImGui::InvisibleButton("##pantin", ImVec2(side, side));
  const bool hovered = ImGui::IsItemHovered();

  ImDrawList* into = ImGui::GetWindowDrawList();
  into->AddRectFilled(at, ImVec2(at.x + side, at.y + side),
                      doll.light_background ? kLightBackground : look.doll,
                      kEquipSlotRounding * Scale());

  const float padding = kCharacterAvatarPadding * Scale();
  doll.box_width = std::max(0.0F, side - 2.0F * padding);
  doll.box_height = doll.box_width;

  // ⚠ LE POINT EST RELEVÉ DANS LA BOÎTE, pas dans le carré : la silhouette est
  // dessinée centrée sur la boîte, et c'est d'elle que `app` tirera les unités
  // de l'acteur. Un point pris depuis le coin du carré serait décalé de la
  // marge — la pipette désignerait simplement la pièce d'à côté.
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  doll.hovered = hovered;
  doll.hover_x = mouse.x - (at.x + (side - doll.box_width) * 0.5F);
  doll.hover_y = mouse.y - (at.y + (side - doll.box_height) * 0.5F);

  if (doll.image.usable()) {
    // ⚠ ÉTIRÉE SUR LA BOÎTE : la texture est composée en pixels de sprite, et
    // c'est la carte qui l'agrandit.
    const ImVec2 image_at(at.x + (side - doll.image.width) * 0.5F,
                          at.y + (side - doll.image.height) * 0.5F);
    into->AddImage(doll.image.id, image_at,
                   ImVec2(image_at.x + doll.image.width, image_at.y + doll.image.height));
  }

  if (!hovered) {
    return;
  }
  SkinTooltip(pipette ? core::Tr("Clic : régler la pièce visée. Molette : tourner.")
                      : core::Tr("Molette : tourner"));
  // 🔴 LA MOLETTE PASSE PAR LE VERROU : sans lui, parcourir la fenêtre ferait
  // pivoter le pantin au passage — et le faire pivoter ferait défiler la
  // fenêtre.
  const float wheel = WheelOver(doll.wheel, ImGui::GetItemID(), hovered);
  if (wheel == 0.0F) {
    return;
  }
  doll.direction = Turned(doll.direction, wheel > 0.0F);
}

void DrawPaletteDollControls(PaletteDollState& doll, const SkinArt& art) {
  // ── AVANT / APRÈS ─────────────────────────────────────────────────────────
  //
  // 🔴 TENU, PAS BASCULÉ : on compare en relâchant, et un bouton qu'on aurait
  // oublié enfoncé ferait croire que les réglages ne prennent plus.
  static_cast<void>(SkinSmallButton(core::Tr("Avant"), art));
  doll.before = ImGui::IsItemActive();
  if (ImGui::IsItemHovered()) {
    SkinTooltip(core::Tr("Tiens le bouton : le pantin montre ce que tu portes."));
  }
  ImGui::SameLine();
  if (SkinSmallButton("<", art)) {
    doll.direction = Turned(doll.direction, false);
  }
  ImGui::SameLine();
  if (SkinSmallButton(">", art)) {
    doll.direction = Turned(doll.direction, true);
  }

  // ── LA POSE ───────────────────────────────────────────────────────────────
  // Les libellés se passent en français : la liste les traduit à la lecture.
  static constexpr std::array<const char*, kPaletteDollPoses.size()> kPoseNames = {
      core::TrText("Repos"), core::TrText("Marche"), core::TrText("Assis"),
      core::TrText("Attaque")};
  doll.pose = std::clamp(doll.pose, 0, static_cast<int>(kPaletteDollPoses.size()) - 1);
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * kPoseFieldInFonts);
  static_cast<void>(SkinCombo("##pose", &doll.pose, kPoseNames, art));
  ImGui::SameLine();
  static_cast<void>(SkinCheckBox(core::Tr("Animé"), &doll.animated, art));
  ImGui::SameLine();
  static_cast<void>(SkinCheckBox(core::Tr("Fond clair"), &doll.light_background, art));
}

}  // namespace ro::ui
