#include "character_select_bars.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <span>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"

#include "character_select_customize.h"

namespace ro::ui {
namespace {

// La marge laissée de chaque côté de la barre avant qu'elle ne s'empile, en
// écarts : un bouton collé au bord se clique mal.
constexpr float kBarMarginRatio = 4.0F;

// La largeur des flèches de page, et de combien leur libellé descend pour
// tomber au milieu du bouton.
constexpr float kPageArrowWidth = 46.0F;
constexpr float kPageLabelDrop = 6.0F;
constexpr ImU32 kPageColor = IM_COL32(240, 232, 208, 255);

// UN BOUTON DE LA BARRE : son libellé, s'il répond, et ce qu'il demande.
struct BarButton {
  const char* label = nullptr;
  bool enabled = true;
  CharacterSelectVerb verb = CharacterSelectVerb::kNothing;
};

[[nodiscard]] BarShape ShapeBar(std::span<const char* const> labels, const SkinArt& art) {
  BarShape shape;
  shape.width = SkinWidestButton(labels, art);
  const auto count = static_cast<int>(labels.size());
  const float one_row = static_cast<float>(count) * (shape.width + kBarGapPixels) - kBarGapPixels;
  const bool two_rows =
      one_row > ImGui::GetMainViewport()->Size.x - kBarGapPixels * kBarMarginRatio;
  shape.rows = two_rows ? 2 : 1;
  shape.per_row = two_rows ? (count + 1) / 2 : count;
  return shape;
}

// LES BOUTONS DU MOMENT.
//
// ⚠ LA GARDE EST SUR LE BOUTON, contrairement au formulaire : là-bas, ce qui
// manque est dans un champ que le joueur regarde ; ici, « aucun personnage
// désigné » se voit sur la scène entière, et un bouton vif qui ne ferait
// rien serait la seule chose qui mente.
[[nodiscard]] std::vector<BarButton> ButtonsFor(const CharacterSelectState& state,
                                                const CharacterSelectView& view,
                                                const CharacterSlotView* designated) {
  std::vector<BarButton> buttons;
  if (state.moving()) {
    buttons.push_back({core::Tr("Valider le déplacement"),
                       state.moving_target >= 0 && !view.amend_pending,
                       CharacterSelectVerb::kMove});
    buttons.push_back({core::Tr("Annuler le déplacement"), true, CharacterSelectVerb::kDropAmend});
    return buttons;
  }
  buttons.push_back({core::Tr("Entrer en jeu"), designated != nullptr && !designated->doomed(),
                     CharacterSelectVerb::kEnter});
  // ⚠ UN BOUTON VISIBLE, et non un raccourci seul : c'est une fonctionnalité
  // du joueur, pas un outil d'auteur. F10 reste en doublon pour qui l'a appris.
  buttons.push_back({state.customizing ? core::Tr("Terminer") : core::Tr("Personnaliser"), true,
                     CharacterSelectVerb::kBeginCustomizing});
  if (designated == nullptr) {
    return buttons;
  }
  if (designated->doomed()) {
    buttons.push_back(
        {core::Tr("Annuler la suppression"), true, CharacterSelectVerb::kCancelDeletion});
    // ⚠ GRISÉ TANT QUE L'HEURE N'EST PAS VENUE, et le compte à rebours est
    // sous le personnage : le serveur refuserait, et son refus se lit
    // « adresse de courriel incorrecte » sur un courriel juste.
    buttons.push_back({core::Tr("Supprimer"), designated->delete_in_seconds == 0,
                       CharacterSelectVerb::kAskDeletion});
    return buttons;
  }
  // ⚠ LES RETOUCHES N'APPARAISSENT QUE SI LE SERVEUR EN ACCORDE : un bouton
  // toujours présent inviterait à un geste refusé neuf fois sur dix.
  const bool amend_free = !view.amend_pending && !state.customizing && !state.rename.open();
  if (designated->moves_left > 0) {
    buttons.push_back({core::Tr("Déplacer"), amend_free, CharacterSelectVerb::kBeginMove});
  }
  if (designated->renamable) {
    buttons.push_back({core::Tr("Renommer"), amend_free, CharacterSelectVerb::kBeginRename});
  }
  buttons.push_back(
      {core::Tr("Programmer la suppression"), true, CharacterSelectVerb::kReserveDeletion});
  return buttons;
}

// CE QU'UN CLIC SUR CE BOUTON DEMANDE, état basculé compris.
[[nodiscard]] CharacterSelectAction Pressed(CharacterSelectState& state, const BarButton& button,
                                            const CharacterSlotView* designated) {
  CharacterSelectAction action;
  action.verb = button.verb;
  action.slot = state.selected;
  action.character_id = designated != nullptr ? designated->character_id : 0;
  switch (button.verb) {
    case CharacterSelectVerb::kBeginCustomizing:
      action.verb = ToggleCustomizing(state);
      break;
    case CharacterSelectVerb::kBeginMove:
      state.moving_slot = state.selected;
      state.moving_target = -1;
      break;
    case CharacterSelectVerb::kBeginRename:
      state.rename = CharacterRenameDraft{};
      if (designated != nullptr) {
        state.rename.character_id = designated->character_id;
        state.rename.former_name = designated->name;
      }
      break;
    case CharacterSelectVerb::kMove:
      action.slot = state.moving_slot;
      action.target_slot = state.moving_target;
      break;
    case CharacterSelectVerb::kDropAmend:
      state.moving_slot = -1;
      state.moving_target = -1;
      break;
    default:
      break;
  }
  return action;
}

}  // namespace

float BarRowHeight(const SkinArt& art) { return SkinButtonHeight(art) + kBarGapPixels; }

CharacterSelectAction DrawActionBar(CharacterSelectState& state, const CharacterSelectView& view,
                                    const CharacterSlotView* designated, const SkinArt& art,
                                    ImVec2 middle_top) {
  CharacterSelectAction action;
  const std::vector<BarButton> buttons = ButtonsFor(state, view, designated);

  std::vector<const char*> labels;
  labels.reserve(buttons.size());
  for (const BarButton& button : buttons) {
    labels.push_back(button.label);
  }
  const BarShape shape = ShapeBar(labels, art);
  const float row_height = BarRowHeight(art);
  const auto count = static_cast<int>(buttons.size());

  int placed = 0;
  int row = 0;
  while (placed < count) {
    const int here = std::min(shape.per_row, count - placed);
    const float row_width =
        static_cast<float>(here) * (shape.width + kBarGapPixels) - kBarGapPixels;
    const float row_top = middle_top.y - static_cast<float>(shape.rows - 1 - row) * row_height;
    ImGui::SetCursorScreenPos(ImVec2(middle_top.x - row_width * 0.5F, row_top));
    for (int rank = 0; rank < here; ++rank) {
      const BarButton& button = buttons[static_cast<std::size_t>(placed + rank)];
      if (rank > 0) {
        ImGui::SameLine(0.0F, kBarGapPixels);
      }
      ImGui::BeginDisabled(!button.enabled);
      if (SkinButton(button.label, art, ImVec2(shape.width, 0.0F))) {
        action = Pressed(state, button, designated);
      }
      ImGui::EndDisabled();
    }
    placed += here;
    ++row;
  }

  // ⚠ ÉCHAP RENONCE AU DÉPLACEMENT, sauf sous la fenêtre de renommage, qui
  // prend la touche pour elle.
  if (state.moving() && !state.rename.open() && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    action = Pressed(state, BarButton{nullptr, true, CharacterSelectVerb::kDropAmend}, designated);
  }

  // ⚠ ENTRÉE VALIDE, comme au formulaire — et pas quand la scène attend une
  // saisie : la création et la confirmation de suppression en demandent une.
  // Ni pendant la personnalisation ou un déplacement : on n'y joue pas.
  if (designated != nullptr && !designated->doomed() && !state.customizing && !state.moving() &&
      !ImGui::GetIO().WantTextInput &&
      (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
       ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
    action.verb = CharacterSelectVerb::kEnter;
    action.slot = state.selected;
    action.character_id = designated->character_id;
  }
  return action;
}

BarShape WidestActionBar(const SkinArt& art, int* count) {
  const std::array<const char*, 10> every = {core::Tr("Entrer en jeu"),
                                             core::Tr("Personnaliser"),
                                             core::Tr("Terminer"),
                                             core::Tr("Déplacer"),
                                             core::Tr("Renommer"),
                                             core::Tr("Programmer la suppression"),
                                             core::Tr("Annuler la suppression"),
                                             core::Tr("Supprimer"),
                                             core::Tr("Valider le déplacement"),
                                             core::Tr("Annuler le déplacement")};
  // Entrer, personnaliser, déplacer, renommer, programmer : cinq à la fois, au
  // plus.
  constexpr int kWidestCount = 5;
  *count = kWidestCount;
  BarShape shape = ShapeBar(every, art);
  const float one_row =
      static_cast<float>(kWidestCount) * (shape.width + kBarGapPixels) - kBarGapPixels;
  shape.rows = one_row > ImGui::GetMainViewport()->Size.x - kBarGapPixels * kBarMarginRatio ? 2 : 1;
  return shape;
}

CharacterSelectVerb DrawExitBar(const SkinArt& art, ImVec2 top_left, float width) {
  CharacterSelectVerb verb = CharacterSelectVerb::kNothing;
  ImGui::SetCursorScreenPos(top_left);
  if (SkinButton(core::Tr("Revenir au login"), art, ImVec2(width, 0.0F))) {
    verb = CharacterSelectVerb::kBack;
  }
  ImGui::SameLine(0.0F, kBarGapPixels);
  if (SkinButton(core::Tr("Quitter le jeu"), art, ImVec2(width, 0.0F))) {
    verb = CharacterSelectVerb::kQuit;
  }
  return verb;
}

void DrawPageBar(CharacterSelectState& state, int pages, const SkinArt& art, ImDrawList* list,
                 ImVec2 middle_top) {
  std::array<char, 40> label = {};
  std::snprintf(label.data(), label.size(), core::Tr("Page %d / %d"), state.page + 1, pages);
  const ImVec2 label_size = ImGui::CalcTextSize(label.data());
  const float row_width = kPageArrowWidth * 2.0F + kBarGapPixels * 2.0F + label_size.x;
  const float left = middle_top.x - row_width * 0.5F;

  // ⚠ L'ÉTAT SE LIT AVANT LE CLIC : le clic change la page, donc relire la
  // condition pour `EndDisabled` la dépareillerait de son `BeginDisabled`.
  const bool at_first = state.page <= 0;
  ImGui::SetCursorScreenPos(ImVec2(left, middle_top.y));
  ImGui::BeginDisabled(at_first);
  if (SkinButton("<", art, ImVec2(kPageArrowWidth, 0.0F))) {
    --state.page;
  }
  ImGui::EndDisabled();

  const bool at_last = state.page >= pages - 1;
  ImGui::SetCursorScreenPos(
      ImVec2(left + kPageArrowWidth + kBarGapPixels * 2.0F + label_size.x, middle_top.y));
  ImGui::BeginDisabled(at_last);
  if (SkinButton(">", art, ImVec2(kPageArrowWidth, 0.0F))) {
    ++state.page;
  }
  ImGui::EndDisabled();

  list->AddText(ImVec2(left + kPageArrowWidth + kBarGapPixels, middle_top.y + kPageLabelDrop),
                kPageColor, label.data());
}

}  // namespace ro::ui
