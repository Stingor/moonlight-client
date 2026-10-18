// L'ÉDITEUR D'EMBLÈME : la boîte, et son onglet « Choisir un fichier ».
//
// Les fichiers sont les `.bmp` du dossier `emblem\` de l'installation — le
// dossier où la fenêtre du client va lire les siens, pour que les deux voient
// les mêmes. C'est `app` qui lit le disque ; la boîte montre ce qu'il a trouvé.

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "guild_panes.h"
#include "ui_scale.h"
#include "ui_strings.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

constexpr int kVisibleFiles = 6;
constexpr float kRowLeastPixels = 32.0F;
constexpr float kRowSlackPixels = 8.0F;
// Ce que le « Selectable » d'une ligne rend à la ligne, et la marge de la
// vignette.
constexpr float kRowInsetPixels = 4.0F;
constexpr float kThumbInsetPixels = 10.0F;
constexpr float kTextAfterThumbPixels = 12.0F;
constexpr ImU32 kThumbBorder = IM_COL32(90, 90, 110, 160);
constexpr float kSendPixels = 110.0F;
constexpr float kRescanPixels = 140.0F;
constexpr float kResumePixels = 160.0F;
constexpr float kClosePixels = 90.0F;

void CloseButton(const SkinArt& art) {
  if (SkinButton(core::Tr("Fermer"), art, ImVec2(Px(kClosePixels), 0.0F))) {
    ImGui::CloseCurrentPopup();
  }
}

void DrawFileList(GuildTabState& state, const GuildView& view) {
  const float row = std::max(ImGui::GetTextLineHeight() + Px(kRowSlackPixels), Px(kRowLeastPixels));
  const float item = row - Px(kRowInsetPixels);
  // LA HAUTEUR EXACTE DU CONTENU : la calculer au jugé faisait paraître un
  // ascenseur dès la première ligne, qui décalait tout le contenu.
  const ImGuiStyle& style = ImGui::GetStyle();
  const int rows = std::min(kVisibleFiles, static_cast<int>(view.emblem_files.size()));
  const float height = static_cast<float>(rows) * item +
                       static_cast<float>(rows - 1) * style.ItemSpacing.y +
                       style.WindowPadding.y * 2.0F + style.ChildBorderSize * 2.0F;
  if (ImGui::BeginChild("fichiers", ImVec2(0.0F, height), ImGuiChildFlags_Borders)) {
    const float thumb = row - Px(kThumbInsetPixels);
    const float text_x = thumb + Px(kTextAfterThumbPixels);
    for (std::size_t index = 0; index < view.emblem_files.size(); ++index) {
      const EmblemFileView& file = view.emblem_files[index];
      ImGui::PushID(static_cast<int>(index));
      const ImVec2 at = ImGui::GetCursorScreenPos();
      // ⚠ UN « SELECTABLE » VIDE sur toute la largeur, et la vignette et le nom
      // peints par-dessus, chacun à sa place : indenter le nom par des espaces
      // laissait la vignette le recouvrir.
      if (ImGui::Selectable("##ligne", state.emblem.chosen_file == static_cast<int>(index), 0,
                            ImVec2(0.0F, item))) {
        state.emblem.chosen_file = static_cast<int>(index);
        state.emblem.error = file.usable ? std::string() : std::string(file.why);
      }
      if (!file.usable && ImGui::IsItemHovered()) {
        const std::string why(file.why);
        SkinTooltipFormat("%s", why.c_str());
      }
      ImDrawList* into = ImGui::GetWindowDrawList();
      if (file.preview.usable()) {
        const ImVec2 low(at.x + Px(kRowInsetPixels), at.y + (item - thumb) * 0.5F);
        const ImVec2 high(low.x + thumb, low.y + thumb);
        into->AddImage(file.preview.id, low, high);
        into->AddRect(low, high, kThumbBorder);
      }
      const std::string label =
          std::string(file.name) + (file.usable ? "" : std::string("   ") + core::Tr("(refusé)"));
      into->AddText(ImVec2(at.x + text_x, at.y + (item - ImGui::GetTextLineHeight()) * 0.5F),
                    ImGui::GetColorU32(file.usable ? ImGuiCol_Text : ImGuiCol_TextDisabled),
                    label.c_str());
      ImGui::PopID();
    }
  }
  ImGui::EndChild();
}

void DrawFileTab(GuildTabState& state, const GuildView& view, const SkinArt& art,
                 GuildAction& action) {
  std::array<char, 512> line = {};
  const std::string folder(view.emblem_folder);
  std::snprintf(line.data(), line.size(), core::Tr("Fichiers .bmp de %s"), folder.c_str());
  ColoredText(kGuildLabelColor, line.data());
  ColoredText(kGuildLabelColor, core::Tr("Attendu : 24 bits ou 256 couleurs, non compressé."));
  ImGui::Spacing();
  if (view.emblem_files.empty()) {
    ColoredText(kGuildRedColor, core::Tr("Aucun .bmp dans ce dossier."));
  } else {
    DrawFileList(state, view);
  }

  // CE QUE LE SERVEUR VA RÉELLEMENT RECEVOIR.
  const int chosen_index = state.emblem.chosen_file;
  const EmblemFileView* chosen =
      chosen_index >= 0 && chosen_index < static_cast<int>(view.emblem_files.size())
          ? &view.emblem_files[static_cast<std::size_t>(chosen_index)]
          : nullptr;
  if (chosen != nullptr && chosen->usable) {
    std::snprintf(line.data(), line.size(), core::Tr("%zu octets, prêt à être envoyé."),
                  chosen->bytes);
    ColoredText(kGuildLabelColor, line.data());
    if (chosen->transparency > view.transparency_limit) {
      std::snprintf(line.data(), line.size(),
                    core::Tr("Transparence ~%d %% : au-delà de %d %% le serveur refuse."),
                    chosen->transparency, view.transparency_limit);
      ColoredText(kGuildRedColor, line.data());
    }
  } else if (!state.emblem.error.empty()) {
    ColoredText(kGuildRedColor, state.emblem.error.c_str());
  } else {
    ColoredText(kGuildLabelColor, core::Tr("Choisis un fichier dans la liste."));
  }

  ImGui::Spacing();
  const bool may_send = chosen != nullptr && chosen->usable && view.guild_id > 0;
  ImGui::BeginDisabled(!may_send);
  if (SkinButton(core::Tr("Envoyer"), art, ImVec2(Px(kSendPixels), 0.0F))) {
    action.verb = GuildVerb::kUploadEmblemFile;
    action.file = chosen_index;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (SkinButton(core::Tr("Relire le dossier"), art, ImVec2(Px(kRescanPixels), 0.0F))) {
    action.verb = GuildVerb::kScanEmblemFolder;
    state.emblem.chosen_file = -1;
    state.emblem.error.clear();
  }
  ImGui::SameLine();
  // La passerelle vers l'éditeur : retoucher un emblème plutôt que le refaire.
  ImGui::BeginDisabled(chosen == nullptr || chosen->bytes == 0);
  if (SkinButton(core::Tr("Reprendre au dessin"), art, ImVec2(Px(kResumePixels), 0.0F))) {
    action.verb = GuildVerb::kLoadFileIntoCanvas;
    action.file = chosen_index;
  }
  ImGui::EndDisabled();
}

}  // namespace

void DrawGuildEmblemModal(GuildTabState& state, const GuildView& view, const SkinArt& art,
                          GuildAction& action) {
  const std::string id = SuffixedLabel(core::Tr("Changer l'emblème"), kGuildEmblemModalId);
  if (!BeginSkinnedModal(id.c_str(), art)) {
    return;
  }
  std::array<char, 160> line = {};
  std::snprintf(line.data(), line.size(),
                core::Tr("Format envoyé : %dx%d, magenta pur (255, 0, 255) = transparent."),
                kEmblemCanvasSide, kEmblemCanvasSide);
  ColoredText(kGuildLabelColor, line.data());
  // Le serveur SE TAIT quand l'expéditeur n'est pas maître : autant l'annoncer
  // avant de laisser dessiner.
  if (!view.master) {
    ColoredText(kGuildRedColor,
                core::Tr("Tu n'es pas maître de guilde : le serveur ignorera l'envoi."));
  }
  ImGui::Spacing();
  if (!BeginSkinTabBar("embleme", art)) {
    EndSkinnedModal();
    return;
  }
  // « REPRENDRE AU DESSIN » a chargé le canevas : l'onglet doit suivre, sinon
  // le clic n'a aucun effet visible.
  const ImGuiTabItemFlags paint_flags =
      state.emblem.goto_paint ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
  state.emblem.goto_paint = false;
  if (ImGui::BeginTabItem(core::Tr("Dessiner"), nullptr, paint_flags)) {
    DrawEmblemPaintTab(state, view, art, action);
    ImGui::EndTabItem();
  }
  if (!ImGui::BeginTabItem(core::Tr("Choisir un fichier"))) {
    EndSkinTabBar();
    ImGui::Separator();
    CloseButton(art);
    if (!state.emblem.report.empty()) {
      ColoredText(kGuildLabelColor, state.emblem.report.c_str());
    }
    EndSkinnedModal();
    return;
  }
  DrawFileTab(state, view, art, action);
  ImGui::EndTabItem();
  EndSkinTabBar();

  ImGui::Separator();
  CloseButton(art);
  ImGui::SameLine();
  ColoredText(kGuildLabelColor,
              core::Tr("L'envoi passe par le service web du serveur, comme la fenêtre "
                       "native.\nL'emblème se met à jour dès que le serveur a publié la "
                       "nouvelle version."));
  if (!state.emblem.report.empty()) {
    ColoredText(kGuildLabelColor, state.emblem.report.c_str());
  }
  EndSkinnedModal();
}

}  // namespace ro::ui
