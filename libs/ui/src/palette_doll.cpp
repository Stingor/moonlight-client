#include "palette_doll.h"

#include <algorithm>
#include <array>

// ⚠ POUR LE NOMBRE DE DIRECTIONS D'UN .ACT : la molette tourne le pantin dans
// la numérotation du fichier, et en redéclarer le compte ici en ferait une
// seconde vérité.
#include "ro/render/actor_layers.h"
#include "ro/ui/character_window.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "ui_sampler.h"
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

// LA LOUPE : le côté de la bulle, en points, le grossissement, et la croix qui
// marque le pixel visé.
constexpr float kLoupeSidePoints = 120.0F;
constexpr float kLoupeZoom = 4.0F;
constexpr float kLoupeCrossPoints = 6.0F;
constexpr ImU32 kLoupeCross = IM_COL32(255, 0, 255, 255);

// LE PANTIN GROSSI AUTOUR DU CURSEUR, dans une bulle qui le suit.
//
// ⚠ LA RÉGION SE ROGNE AU BORD DE L'IMAGE au lieu de déborder : au-delà, la
// texture se répéterait ou s'étirerait, et l'on viserait un pixel qui n'existe
// pas. La croix reste sur le pixel visé.
void DrawMagnifier(const PaletteDollState& doll, ImVec2 image_at, ImVec2 mouse, ImU32 background) {
  if (!BeginSkinTooltip()) {
    return;
  }
  const float side = kLoupeSidePoints * Scale();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::Dummy(ImVec2(side, side));
  ImDrawList* into = ImGui::GetWindowDrawList();
  into->AddRectFilled(origin, ImVec2(origin.x + side, origin.y + side), background);

  // Le carré de l'image que la bulle montre, en pixels d'écran du pantin.
  const float seen = side / kLoupeZoom;
  const float left = mouse.x - image_at.x - seen * 0.5F;
  const float top = mouse.y - image_at.y - seen * 0.5F;
  const float clipped_left = std::max(left, 0.0F);
  const float clipped_top = std::max(top, 0.0F);
  const float clipped_right = std::min(left + seen, doll.image.width);
  const float clipped_bottom = std::min(top + seen, doll.image.height);
  if (clipped_right > clipped_left && clipped_bottom > clipped_top) {
    const ImVec2 uv0(clipped_left / doll.image.width, clipped_top / doll.image.height);
    const ImVec2 uv1(clipped_right / doll.image.width, clipped_bottom / doll.image.height);
    const ImVec2 from(origin.x + (clipped_left - left) * kLoupeZoom,
                      origin.y + (clipped_top - top) * kLoupeZoom);
    const ImVec2 to(origin.x + (clipped_right - left) * kLoupeZoom,
                    origin.y + (clipped_bottom - top) * kLoupeZoom);
    // 🔴 NET, PIXEL POUR PIXEL : la loupe sert à viser un pixel, et le lissage
    // ambiant en faisait une tache floue. Voir `ui_sampler.h`.
    ImposeImageSampler(into, /*smooth=*/false);
    into->AddImage(doll.image.id, from, to, uv0, uv1);
    RestoreAmbientSampler(into);
  }
  const ImVec2 middle(origin.x + side * 0.5F, origin.y + side * 0.5F);
  const float cross = kLoupeCrossPoints * Scale();
  into->AddLine(ImVec2(middle.x - cross, middle.y), ImVec2(middle.x + cross, middle.y),
                kLoupeCross);
  into->AddLine(ImVec2(middle.x, middle.y - cross), ImVec2(middle.x, middle.y + cross),
                kLoupeCross);
  ImGui::TextUnformatted(core::Tr("Clic : régler la pièce visée."));
  EndSkinTooltip();
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
    // NET PAR DÉFAUT, comme le sprite qu'on règle ; lissé si le joueur le
    // demande. ⚠ IMPOSÉ DANS LES DEUX CAS : voir `ui_sampler.h`.
    ImposeImageSampler(into, doll.smooth);
    into->AddImage(doll.image.id, image_at,
                   ImVec2(image_at.x + doll.image.width, image_at.y + doll.image.height));
    RestoreAmbientSampler(into);
  }

  if (!hovered) {
    return;
  }
  if (pipette && doll.magnifier && doll.image.usable()) {
    const ImVec2 image_at(at.x + (side - doll.image.width) * 0.5F,
                          at.y + (side - doll.image.height) * 0.5F);
    DrawMagnifier(doll, image_at, mouse, doll.light_background ? kLightBackground : look.doll);
  } else {
    SkinTooltip(pipette ? core::Tr("Clic : régler la pièce visée. Molette : tourner.")
                        : core::Tr("Molette : tourner"));
  }
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
  ImGui::SameLine();
  static_cast<void>(SkinCheckBox(core::Tr("Lisser"), &doll.smooth, art));
  if (ImGui::IsItemHovered()) {
    SkinTooltip(core::Tr("Adoucit le pantin agrandi. Décoché, chaque pixel du sprite reste net."));
  }
  ImGui::SameLine();
  static_cast<void>(SkinCheckBox(core::Tr("Loupe"), &doll.magnifier, art));
  if (ImGui::IsItemHovered()) {
    SkinTooltip(core::Tr("Le pantin grossi autour du curseur pendant que tu vises."));
  }
}

}  // namespace ro::ui
