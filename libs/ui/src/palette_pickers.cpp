#include "palette_pickers.h"

#include <algorithm>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_text.h"
#include "ro/ui/skin_widgets.h"

#include "thumbnail_grid.h"
#include "ui_scale.h"

namespace ro::ui {
namespace {

// ── LES GRILLES DE VIGNETTES ────────────────────────────────────────────────
//
// LES MESURES SONT EN POINTS D'INTERFACE : huit colonnes de
// trente-quatre pour les coiffures, seize de vingt-quatre pour leurs couleurs,
// huit de quarante-six pour les teintes de vêtement. Elles ne sont pas
// arbitraires — la case doit montrer la coupe, et la ligne doit tenir dans une
// fenêtre qui reste à côté de l'éditeur.
constexpr int kHairColumns = 8;
constexpr float kHairCellPoints = 34.0F;
constexpr int kHairColorColumns = 16;
constexpr float kHairColorCellPoints = 24.0F;
constexpr int kBodyColumns = 8;
constexpr int kBodyRows = 6;
constexpr float kBodyCellPoints = 46.0F;
// Au-delà, la grille défile. Dix lignes tiennent sur l'écran le plus bas que
// ce client accepte.
constexpr int kGridVisibleRows = 10;

// LA GRILLE EST PARTAGÉE avec la création de personnage — voir
// `thumbnail_grid.h`. Ce qui reste ici est le raccord : la tranche visible
// qu'elle relève repart dans l'état que `app` lira.
[[nodiscard]] ThumbnailChoice DrawPickerGrid(int first, int last, int columns, float cell,
                                             int chosen, const PaletteEditorView& view,
                                             PalettePickerState& picker) {
  ThumbnailGrid grid;
  grid.first = first;
  grid.last = last;
  grid.columns = columns;
  grid.cell = cell;
  grid.chosen = chosen;
  grid.thumbnails = view.thumbnails;
  grid.thumbnails_first = view.thumbnails_first;
  const ThumbnailChoice choice = DrawThumbnailGrid(grid);
  picker.first = choice.seen_first;
  picker.count = choice.seen_count;
  picker.cell = cell;
  return choice;
}

// Les identifiants des trois fenêtres surgissantes. ⚠ HORS DU CATALOGUE : un
// identifiant ImGui n'est pas un texte, et le traduire changerait la fenêtre
// qu'un bouton ouvre selon la langue.
constexpr const char* kHairStylePopup = "##coiffures";
constexpr const char* kHairColorPopup = "##couleurs_cheveux";
constexpr const char* kBodyPalettePopup = "##palettes_corps";

void DrawHairStylePicker(PaletteEditorState& state, const PaletteEditorView& view) {
  if (!BeginSkinPopup(kHairStylePopup)) {
    return;
  }
  state.picker.open = PaletteEditorPicker::kHairStyle;
  const int last = view.bounds.hair_style;
  const float cell = kHairCellPoints * Scale();
  ImGui::TextUnformatted(core::Tr("Coiffure"));
  // ⛔ PLUS D'AVERTISSEMENT SUR LES CASES VIDES, retiré à la demande du joueur.
  // Il expliquait pourquoi certains numéros ne montrent rien — mais il occupait
  // deux lignes en tête de CHAQUE ouverture pour un cas qui se comprend d'un
  // coup d'œil, et la grille est l'endroit où l'on veut voir des coiffures, pas
  // lire.
  ImGui::BeginChild(
      "##g",
      ImVec2(ThumbnailGridWidth(kHairColumns, cell,
                                ThumbnailGridScrolls(last, kHairColumns, kGridVisibleRows)),
             ThumbnailGridHeight(last, kHairColumns, cell, kGridVisibleRows)),
      ImGuiChildFlags_Borders);
  // La case allumée porte la coupe CHOISIE, ou celle qu'on porte tant que rien
  // n'est choisi : une grille qui n'allumerait rien laisserait croire que le
  // personnage n'a pas de coiffure.
  const int chosen = state.recipe.hair_style > 0 ? state.recipe.hair_style : view.worn_hair_style;
  const ThumbnailChoice choice =
      DrawPickerGrid(view.bounds.first, last, kHairColumns, cell, chosen, view, state.picker);
  if (choice.picked > 0) {
    state.recipe.hair_style = static_cast<std::int16_t>(choice.picked);
  }
  ImGui::EndChild();
  // 🔴 APRÈS `EndChild`, ET JAMAIS DEDANS : la fermeture vise le POPUP, et la
  // demander depuis un enfant reviendrait à parier sur la pile de fenêtres
  // d'ImGui plutôt que sur ce qu'on veut dire.
  if (choice.close) {
    ImGui::CloseCurrentPopup();
  }
  EndSkinPopup();
}

void DrawHairColorPicker(PaletteEditorState& state, const PaletteEditorView& view) {
  if (!BeginSkinPopup(kHairColorPopup)) {
    return;
  }
  state.picker.open = PaletteEditorPicker::kHairColor;
  const int last = view.bounds.hair_palette;
  const float cell = kHairColorCellPoints * Scale();
  ImGui::TextUnformatted(core::Tr("Couleur de cheveux"));
  // ⚠ CHAQUE CASE PORTE SA PROPRE COUPE dans la teinte du numéro, et c'est ce
  // que fait l'écran de création : une pastille unie ne dit rien du DÉGRADÉ
  // qu'une palette applique.
  ImGui::BeginChild(
      "##g",
      ImVec2(ThumbnailGridWidth(kHairColorColumns, cell,
                                ThumbnailGridScrolls(last, kHairColorColumns, kGridVisibleRows)),
             ThumbnailGridHeight(last, kHairColorColumns, cell, kGridVisibleRows)),
      ImGuiChildFlags_Borders);
  const ThumbnailChoice choice = DrawPickerGrid(view.bounds.first, last, kHairColorColumns, cell,
                                                state.recipe.hair_palette, view, state.picker);
  if (choice.picked > 0) {
    state.recipe.hair_palette = static_cast<std::int16_t>(choice.picked);
  }
  ImGui::EndChild();
  if (choice.close) {
    ImGui::CloseCurrentPopup();
  }
  EndSkinPopup();
}

void DrawBodyPalettePicker(PaletteEditorState& state, const PaletteEditorView& view,
                           const SkinArt& art) {
  if (!BeginSkinPopup(kBodyPalettePopup)) {
    return;
  }
  state.picker.open = PaletteEditorPicker::kBodyPalette;
  // 🔴 LE VRAI CORPS PEINT, ET NON UNE PASTILLE DE COULEUR MOYENNE. La moyenne
  // trompe : la palette 484 rend un beige qui ne laisse rien présager du vert
  // qu'elle contient. Une palette
  // de vêtement n'est pas une teinte, c'est une DISTRIBUTION — et seul le
  // sprite la montre.
  //
  // ⚠ LE PRIX EST UNE TEXTURE PAR PALETTE, d'où la PAGINATION : cinq cent
  // cinquante-trois vignettes vivantes videraient le cache, qui se mettrait à
  // évincer en pleine image des vignettes déjà dessinées.
  constexpr int kPerPage = kBodyColumns * kBodyRows;
  const int last = view.bounds.body_palette;
  const int pages = (last - view.bounds.first + kPerPage) / kPerPage;
  state.picker.page = std::clamp(state.picker.page, 0, std::max(0, pages - 1));
  const float cell = kBodyCellPoints * Scale();

  ImGui::TextUnformatted(core::Tr("Teinte de base"));
  // « < » et « > » sont dans la plage que couvre la police de l'interface —
  // une flèche d'icône n'y serait qu'un rectangle vide.
  if (SkinSmallButton("<", art) && state.picker.page > 0) {
    --state.picker.page;
  }
  ImGui::SameLine();
  char page[64] = {};
  std::snprintf(page, sizeof(page), core::Tr("Page %d/%d"), state.picker.page + 1, pages);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(page);
  ImGui::SameLine();
  if (SkinSmallButton(">", art) && state.picker.page + 1 < pages) {
    ++state.picker.page;
  }

  const int first = view.bounds.first + state.picker.page * kPerPage;
  const int upto = std::min(first + kPerPage - 1, last);
  // Paginée : jamais d'ascenseur, une page tient exactement dans la grille.
  ImGui::BeginChild("##g",
                    ImVec2(ThumbnailGridWidth(kBodyColumns, cell, /*with_scrollbar=*/false),
                           ThumbnailGridHeight(kPerPage, kBodyColumns, cell, kBodyRows)),
                    ImGuiChildFlags_Borders);
  const ThumbnailChoice choice = DrawPickerGrid(first, upto, kBodyColumns, cell,
                                                state.recipe.body_palette, view, state.picker);
  if (choice.picked > 0) {
    // 🔴 `app` RECHARGERA TOUT : la teinte de base change la fusion, donc le
    // DÉCOUPAGE des rampes. Les réglages en place se ré-appliquent par index,
    // comme lors d'un changement de corps.
    state.recipe.body_palette = static_cast<std::int16_t>(choice.picked);
  }
  ImGui::EndChild();
  if (choice.close) {
    ImGui::CloseCurrentPopup();
  }
  EndSkinPopup();
}

// LES TROIS SÉLECTEURS, EN BOUTONS. Le libellé porte le choix courant.

}  // namespace

void DrawChoiceButtons(PaletteEditorState& state, const PaletteEditorView& view,
                       const SkinArt& art) {
  char label[96] = {};
  const int hair = state.recipe.hair_style > 0 ? state.recipe.hair_style : view.worn_hair_style;
  std::snprintf(label, sizeof(label), "%s (%d)", core::Tr("Coiffure"), hair);
  if (SkinButton(label, art)) {
    ImGui::OpenPopup(kHairStylePopup);
  }
  DrawHairStylePicker(state, view);

  if (state.recipe.hair_palette > 0) {
    std::snprintf(label, sizeof(label), "%s (%d)", core::Tr("Couleur de cheveux"),
                  state.recipe.hair_palette);
  } else {
    std::snprintf(label, sizeof(label), "%s (%s)", core::Tr("Couleur de cheveux"),
                  core::Tr("d'origine"));
  }
  if (SkinButton(label, art)) {
    ImGui::OpenPopup(kHairColorPopup);
  }
  DrawHairColorPicker(state, view);

  // 🔴 LE LIBELLÉ PORTE LES NUMÉROS DU SERVEUR. Le joueur et l'administrateur
  // doivent parler de la même palette : « teinte 42 » ici doit être
  // `body_42.pal` là-bas. Et « d'origine » n'est pas la palette zéro — celle-ci
  // n'existe pas pour le jeu, une couleur de vêtement nulle signifiant « aucune
  // palette externe » ; la sentinelle reste moins un.
  if (state.recipe.body_palette > 0) {
    std::snprintf(label, sizeof(label), "%s (%d)", core::Tr("Teinte de base"),
                  state.recipe.body_palette);
  } else {
    std::snprintf(label, sizeof(label), "%s (%s)", core::Tr("Teinte de base"),
                  core::Tr("d'origine"));
  }
  if (SkinButton(label, art)) {
    ImGui::OpenPopup(kBodyPalettePopup);
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Les couleurs officielles du styliste servent de point de départ ; tes "
                 "réglages s'appliquent par-dessus. Changer de teinte de base peut "
                 "redécouper les pièces."));
  }
  DrawBodyPalettePicker(state, view, art);
}

bool ThumbnailGridScrolls(int total, int columns, int visible_rows) {
  return (total + columns - 1) / columns > visible_rows;
}

float ThumbnailGridWidth(int columns, float cell, bool with_scrollbar) {
  const ImGuiStyle& style = ImGui::GetStyle();
  return static_cast<float>(columns) * cell +
         static_cast<float>(columns - 1) * style.ItemSpacing.x + 2.0F * style.WindowPadding.x +
         (with_scrollbar ? style.ScrollbarSize : 0.0F);
}

float ThumbnailGridHeight(int total, int columns, float cell, int visible_rows) {
  const ImGuiStyle& style = ImGui::GetStyle();
  const int rows = std::min((total + columns - 1) / columns, visible_rows);
  return static_cast<float>(rows) * cell + static_cast<float>(rows - 1) * style.ItemSpacing.y +
         2.0F * style.WindowPadding.y;
}

}  // namespace ro::ui
