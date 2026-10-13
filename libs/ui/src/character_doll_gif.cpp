#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/render/doll_animation.h"
#include "ro/ui/character_window.h"
#include "ro/ui/doll_gif.h"
#include "ro/ui/skin_widgets.h"

#include "character_panes.h"

namespace ro::ui {
namespace {

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

// Le jour entre la ligne des poses et celle de l'échelle.
constexpr float kToolsLineGap = 4.0F;
// L'écart entre les boutons d'échelle, serrés : ils forment un seul choix.
constexpr float kScaleButtonGap = 2.0F;
// L'écart entre le choix d'échelle et le bouton de copie.
constexpr float kCopyGap = 8.0F;
// La place d'un libellé d'échelle, « 3× » et son terminateur.
constexpr std::size_t kScaleLabelBytes = 8;

// Ce que le système refuse dans un nom de fichier, et l'espace.
constexpr std::string_view kRefusedInName = "\\/:*?\"<>| ";
// Le premier octet imprimable : en deçà, des codes de contrôle.
constexpr unsigned char kFirstPrintable = 0x20;
// Le pseudo d'un personnage qui n'en a pas encore.
constexpr std::string_view kNamelessPlayer = "perso";

[[nodiscard]] std::string SafePiece(std::string_view piece) {
  std::string safe;
  safe.reserve(piece.size());
  for (const char letter : piece) {
    const auto raw = static_cast<unsigned char>(letter);
    safe.push_back(raw < kFirstPrintable || kRefusedInName.find(letter) != std::string_view::npos
                       ? '_'
                       : letter);
  }
  return safe;
}

[[nodiscard]] std::array<char, kScaleLabelBytes> ScaleLabel(int scale) {
  std::array<char, kScaleLabelBytes> label{};
  std::snprintf(label.data(), label.size(), "%d×", scale);
  return label;
}

}  // namespace

std::string DollGifFileName(std::string_view player_name, std::string_view pose_label,
                            int direction) {
  const std::string player =
      player_name.empty() ? std::string(kNamelessPlayer) : SafePiece(player_name);
  return "avatar_" + player + "_" + SafePiece(pose_label) + "_d" + std::to_string(direction) +
         ".gif";
}

float DollGifButtonWidth(const SkinArt& art) {
  return SkinButtonWidth(core::Tr("Générer le GIF"), art);
}

void DrawDollGifButton(const DollGifView& gif, const SkinArt& art, CharacterWindowAction& action) {
  ImGui::BeginDisabled(gif.busy);
  if (SkinButton(core::Tr("Générer le GIF"), art)) {
    action.doll_gif = DollGifVerb::kSave;
  }
  ImGui::EndDisabled();
  SkinTooltip(
      core::Tr("Enregistre le personnage animé, sur fond transparent, dans la pose et "
               "la direction montrées"));
}

void DrawDollGifTools(CharacterAvatarState& avatar, const DollGifView& gif, float pane_width,
                      const SkinArt& art, CharacterWindowAction& action) {
  const float scale = Scale();
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + kToolsLineGap * scale);
  const float start_x = ImGui::GetCursorPosX();

  // LA LIGNE EST MESURÉE AVANT D'ÊTRE POSÉE : elle se centre comme celle des
  // poses, et ses libellés changent de largeur avec la langue.
  const float scale_width =
      SkinSmallButtonWidth(ScaleLabel(render::kLargestDollGifScale).data(), art);
  const int choices = render::kLargestDollGifScale - render::kSmallestDollGifScale + 1;
  const float copy_width = SkinSmallButtonWidth(core::Tr("Copier"), art);
  const float line = static_cast<float>(choices) * scale_width +
                     static_cast<float>(choices - 1) * kScaleButtonGap * scale + kCopyGap * scale +
                     copy_width;
  ImGui::SetCursorPosX(start_x + std::max(0.0F, (pane_width - line) * 0.5F));

  for (int each = render::kSmallestDollGifScale; each <= render::kLargestDollGifScale; ++each) {
    if (each != render::kSmallestDollGifScale) {
      ImGui::SameLine(0.0F, kScaleButtonGap * scale);
    }
    ImGui::PushID(each);
    if (SkinSmallToggleButton(ScaleLabel(each).data(), avatar.gif_scale == each, art,
                              ImVec2(scale_width, 0.0F))) {
      avatar.gif_scale = static_cast<std::uint8_t>(each);
    }
    ImGui::PopID();
    SkinTooltip(core::Tr("Taille du GIF : chaque pixel du sprite devient un carré de ce côté"));
  }

  ImGui::SameLine(0.0F, kCopyGap * scale);
  ImGui::BeginDisabled(gif.busy);
  if (SkinSmallButton(core::Tr("Copier"), art)) {
    action.doll_gif = DollGifVerb::kCopy;
  }
  ImGui::EndDisabled();
  SkinTooltip(
      core::Tr("Copie le GIF dans le presse-papier, pour le coller dans Discord ou "
               "ailleurs"));

  // ⚠ REPLIÉ DANS LA LARGEUR DU VOLET : un chemin de fichier dépasse aisément.
  if (!gif.message.empty()) {
    ImGui::SetCursorPosX(start_x);
    ImGui::PushTextWrapPos(start_x + pane_width);
    ImGui::TextUnformatted(gif.message.data(), gif.message.data() + gif.message.size());
    ImGui::PopTextWrapPos();
  }
}

}  // namespace ro::ui
