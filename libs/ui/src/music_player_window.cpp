#include "ro/ui/music_player_window.h"

#include <algorithm>
#include <string>

#include <imgui.h>

#include "ro/core/ascii.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_bounds.h"
#include "ro/ui/window_name.h"

#include "ui_text.h"

namespace ro::ui {
namespace {

// La taille à la première ouverture : de quoi lire une douzaine de morceaux et
// les noms de quelques cartes.
constexpr float kFirstWidth = 420.0F;
constexpr float kFirstHeight = 380.0F;

// La colonne du numéro : quatre chiffres, de quoi couvrir le dossier du jeu.
constexpr const char* kWidestLabel = "0000";

// Le surlignage du morceau qui joue : la teinte du survol, à demi effacée pour
// que la sélection reste lisible par-dessus.
constexpr float kPlayingRowAlpha = 0.45F;

[[nodiscard]] std::string_view LabelAt(const MusicPlayerView& view, int row) {
  if (row < 0 || static_cast<std::size_t>(row) >= view.rows.size()) {
    return {};
  }
  return view.rows[static_cast<std::size_t>(row)].label;
}

// LA LIGNE D'ÉTAT : ce qui joue, et qui en a décidé.
void DrawNowPlaying(const MusicPlayerView& view) {
  FaintText(core::Tr("En cours :"));
  ImGui::SameLine();
  const std::string_view playing = LabelAt(view, view.playing);
  if (playing.empty()) {
    ImGui::TextUnformatted(core::Tr("aucun morceau"));
  } else {
    Unformatted(playing);
  }
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  if (view.chosen) {
    ImGui::TextUnformatted(core::Tr("— choisi au lecteur"));
  } else {
    ImGui::TextUnformatted(core::Tr("— musique de la carte"));
    ImGui::SameLine();
    Unformatted(view.map_name);
  }
  ImGui::PopStyleColor();
  // ⚠ LA COUPURE SE DIT : sans elle, « Jouer » aurait l'air de ne rien faire.
  if (!view.audible) {
    ImGui::TextWrapped("%s", core::Tr("La musique est coupée dans les options : le morceau "
                                      "choisi jouera à son retour."));
  }
}

// UNE LIGNE DE LA LISTE. Vrai au double clic : le morceau est à jouer.
[[nodiscard]] bool DrawRow(MusicPlayerState& state, const MusicPlayerView& view, int index,
                           bool appearing) {
  const MusicPlayerRow& row = view.rows[static_cast<std::size_t>(index)];
  ImGui::TableNextRow();
  if (index == view.playing) {
    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,
                           ImGui::GetColorU32(ImGuiCol_HeaderHovered, kPlayingRowAlpha));
    // ⚠ À L'OUVERTURE, LA LISTE DESCEND JUSQU'AU MORCEAU QUI JOUE : c'est par
    // lui qu'on commence, et il peut être le deux-centième.
    if (appearing) {
      ImGui::SetScrollHereY();
    }
  }
  ImGui::TableNextColumn();
  ImGui::PushID(index);
  const std::string label(row.label);
  bool play = false;
  if (ImGui::Selectable(
          label.c_str(), state.selected == index,
          ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
    state.selected = index;
    play = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
  }
  // Les cartes d'un morceau partagé débordent la colonne : la liste entière
  // en infobulle.
  if (ImGui::IsItemHovered() && !row.maps.empty()) {
    SkinTooltip(std::string(row.maps).c_str());
  }
  ImGui::PopID();
  ImGui::TableNextColumn();
  if (index == view.map_row) {
    ImGui::TextUnformatted(core::Tr("(carte actuelle)"));
    ImGui::SameLine();
  }
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  if (row.maps.empty()) {
    ImGui::TextUnformatted(core::Tr("aucune carte"));
  } else {
    Unformatted(row.maps);
  }
  ImGui::PopStyleColor();
  return play;
}

}  // namespace

bool MusicRowMatches(const MusicPlayerRow& row, std::string_view filter) {
  return ro::core::AsciiContainsFolded(row.label, filter) ||
         ro::core::AsciiContainsFolded(row.maps, filter);
}

MusicPlayerAsk DrawMusicPlayerWindow(MusicPlayerState& state, const MusicPlayerView& view,
                                     const SkinArt& art) {
  MusicPlayerAsk asked;
  if (!state.open) {
    return asked;
  }
  static WindowName name(kMusicPlayerTitle, kMusicPlayerId);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidth, kFirstHeight), ImGuiCond_FirstUseEver);
  ConstrainNextWindowToScreen(name.Get());
  if (!BeginSkinnedWindow(name.Get(), &state.open, art)) {
    EndSkinnedWindow();
    return asked;
  }
  const bool appearing = ImGui::IsWindowAppearing();

  DrawNowPlaying(view);
  ImGui::SetNextItemWidth(-1.0F);
  ImGui::InputTextWithHint("##musique_recherche", core::Tr("Rechercher : numéro ou carte"),
                           state.filter.data(), state.filter.size());

  const float footer = SkinButtonHeight(art) + ImGui::GetStyle().ItemSpacing.y * 2.0F;
  const ImVec2 table(0.0F,
                     std::max(ImGui::GetFrameHeight(), ImGui::GetContentRegionAvail().y - footer));
  const std::string_view filter(state.filter.data());
  if (ImGui::BeginTable(
          "##musique_morceaux", 2,
          ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp,
          table)) {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(
        core::Tr("Morceau"), ImGuiTableColumnFlags_WidthFixed,
        ImGui::CalcTextSize(kWidestLabel).x + ImGui::GetStyle().FramePadding.x * 2.0F);
    ImGui::TableSetupColumn(core::Tr("Cartes"), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    for (std::size_t index = 0; index < view.rows.size(); ++index) {
      if (!MusicRowMatches(view.rows[index], filter)) {
        continue;
      }
      if (DrawRow(state, view, static_cast<int>(index), appearing)) {
        asked.play = static_cast<int>(index);
      }
    }
    ImGui::EndTable();
  }

  const bool has_selection =
      state.selected >= 0 && static_cast<std::size_t>(state.selected) < view.rows.size();
  ImGui::BeginDisabled(!has_selection);
  const float play_width = SkinButtonWidth(core::Tr("Jouer"), art);
  if (SkinButton(core::Tr("Jouer"), art, ImVec2(play_width, 0.0F))) {
    asked.play = state.selected;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!view.chosen);
  const float follow_width = SkinButtonWidth(core::Tr("Suivre la carte"), art);
  if (SkinButton(core::Tr("Suivre la carte"), art, ImVec2(follow_width, 0.0F))) {
    asked.follow_map = true;
  }
  SkinTooltip(
      core::Tr("Rend la main à la carte : sa musique reprend, et la suit de nouveau "
               "d'une carte à l'autre."));
  ImGui::EndDisabled();

  EndSkinnedWindow();
  return asked;
}

}  // namespace ro::ui
