#include "ro/ui/character_create_window.h"

#include <algorithm>
#include <array>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/palette_editor.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "thumbnail_grid.h"
#include "ui_scale.h"

namespace ro::ui {
namespace {

// LES MESURES DES GRILLES, en points d'interface. Les mêmes que celles de
// l'éditeur de style, et pour la même raison : la case doit montrer la coupe.
constexpr float kHairCellPoints = 34.0F;
constexpr float kPalettePoints = 24.0F;
// Au-delà, la grille défile.
constexpr int kGridVisibleRows = 10;

// La largeur du champ du nom et des boutons de choix, en points.
constexpr float kFieldPoints = 220.0F;

// Le fond de l'aperçu : un aplat sombre pour détacher le pantin du corps clair
// de la fenêtre.
constexpr ImU32 kPreviewBackground = IM_COL32(0, 0, 0, 45);
constexpr float kPreviewRounding = 4.0F;

constexpr ImU32 kRefusalColor = IM_COL32(255, 96, 96, 255);
constexpr ImU32 kPendingColor = IM_COL32(216, 209, 115, 255);

// ⚠ HORS DU CATALOGUE : un identifiant ImGui n'est pas un texte, et le
// traduire changerait la fenêtre qu'un bouton ouvre selon la langue.
constexpr const char* kHairStylePopup = "##creation_coiffures";
constexpr const char* kHairPalettePopup = "##creation_teintes";

// LE PANTIN D'APERÇU, et la molette qui le fait tourner.
//
// ⚠ IL GARDE SA TAILLE NOMINALE mais se centre dans une colonne aussi haute
// que le formulaire d'à côté : sans cela, une grande fenêtre laisse un grand
// vide sous lui.
void DrawPreview(CharacterCreateState& state, const CharacterCreateView& view, float column_height,
                 CharacterCreateAction& action) {
  const float width = kCreatePreviewWidth * Scale();
  const float height = kCreatePreviewHeight * Scale();
  action.doll_width = width;
  action.doll_height = height;
  const float box_height = std::max(column_height, height);
  const ImVec2 at = ImGui::GetCursorScreenPos();
  ImGui::GetWindowDrawList()->AddRectFilled(at, ImVec2(at.x + width, at.y + box_height),
                                            kPreviewBackground, kPreviewRounding);
  if (view.doll.usable()) {
    const ImVec2 doll_at(at.x + (width - view.doll.width) * 0.5F,
                         at.y + (box_height - height) * 0.5F);
    ImGui::GetWindowDrawList()->AddImage(
        view.doll.id, doll_at, ImVec2(doll_at.x + view.doll.width, doll_at.y + view.doll.height));
  }
  ImGui::InvisibleButton("##apercu", ImVec2(width, box_height));
  if (!ImGui::IsItemHovered()) {
    return;
  }
  // ⚠ HUIT ORIENTATIONS EN BOUCLE, et le `& 7` ramène moins un à sept sans
  // passer par un modulo négatif.
  const float wheel = ImGui::GetIO().MouseWheel;
  if (wheel != 0.0F) {
    state.direction = (state.direction + (wheel > 0.0F ? 1 : -1) + 8) & 7;
  }
  SkinTooltip(core::Tr("Molette pour faire tourner le personnage"));
}

// UNE GRILLE QUI SURGIT, et ce qu'elle a choisi.
//
// ⚠ ELLE DIT CE QU'ELLE MONTRE : la tranche réellement visible repart dans la
// demande, pour que `app` ne compose que cela.
void DrawPicker(const char* popup, const char* title, CreatePicker which, int last, int columns,
                float cell_points, int& chosen, const CharacterCreateView& view,
                CreatePickerRequest& request) {
  if (!BeginSkinPopup(popup)) {
    return;
  }
  request.open = which;
  const float cell = cell_points * Scale();
  ImGui::TextUnformatted(title);
  ImGui::BeginChild(
      "##g",
      ImVec2(
          ThumbnailGridWidth(columns, cell, ThumbnailGridScrolls(last, columns, kGridVisibleRows)),
          ThumbnailGridHeight(last, columns, cell, kGridVisibleRows)),
      ImGuiChildFlags_Borders);
  ThumbnailGrid grid;
  // ⚠ LES DEUX GRILLES PARTENT DE ZÉRO, à la différence de l'éditeur de style :
  // là-bas, zéro n'est pas une palette mais son ABSENCE, et la proposer
  // offrirait une teinte que personne ne voit. Ici zéro EST la teinte native de
  // la coupe, celle que le serveur écrit en base quand on ne choisit rien.
  grid.first = 0;
  grid.last = last;
  grid.columns = columns;
  grid.cell = cell;
  grid.chosen = chosen;
  grid.thumbnails = view.thumbnails;
  grid.thumbnails_first = view.thumbnails_first;
  const ThumbnailChoice choice = DrawThumbnailGrid(grid);
  request.first = choice.seen_first;
  request.count = choice.seen_count;
  request.cell = cell;
  if (choice.picked >= 0) {
    chosen = choice.picked;
  }
  ImGui::EndChild();
  // 🔴 APRÈS `EndChild`, ET JAMAIS DEDANS : la fermeture vise le POPUP.
  if (choice.close) {
    ImGui::CloseCurrentPopup();
  }
  EndSkinPopup();
}

// Le formulaire de droite. Rend sa hauteur, pour centrer l'aperçu à l'image
// suivante.
void DrawForm(CharacterCreateState& state, const CharacterCreateView& view, const SkinArt& art,
              CreatePickerRequest& request) {
  const float field = kFieldPoints * Scale();
  ImGui::BeginGroup();

  ImGui::TextUnformatted(core::Tr("Nom"));
  ImGui::SetNextItemWidth(field);
  // ⚠ LE CLAVIER VA AU NOM À L'OUVERTURE : c'est le seul champ à remplir, et
  // le seul geste qui ne se fasse pas à la souris.
  if (ImGui::IsWindowAppearing()) {
    ImGui::SetKeyboardFocusHere();
  }
  ImGui::InputTextWithHint("##nom", core::Tr("nom du personnage"), state.name.data(),
                           state.name.size());

  ImGui::TextUnformatted(core::Tr("Sexe"));
  if (SkinRadio(core::Tr("Femme"), state.sex == 0, art)) {
    state.sex = 0;
  }
  ImGui::SameLine();
  if (SkinRadio(core::Tr("Homme"), state.sex == 1, art)) {
    state.sex = 1;
  }

  // ⚠ DES BOUTONS QUI OUVRENT UNE GRILLE, et non des curseurs : « coiffure
  // 47 » ne veut rien dire, on choisit une coupe en la VOYANT. Même montage
  // que l'éditeur de style, et pour la raison qu'il énonce.
  std::array<char, 48> hair = {};
  std::snprintf(hair.data(), hair.size(), core::Tr("Coiffure %d"), state.hair_style);
  if (SkinButton(hair.data(), art, ImVec2(field, 0.0F))) {
    ImGui::OpenPopup(kHairStylePopup);
  }
  DrawPicker(kHairStylePopup, core::Tr("Coiffure"), CreatePicker::kHairStyle, kHairStyleCount,
             kHairStyleColumns, kHairCellPoints, state.hair_style, view, request);

  std::array<char, 48> tint = {};
  std::snprintf(tint.data(), tint.size(), core::Tr("Teinte %d"), state.hair_palette);
  if (SkinButton(tint.data(), art, ImVec2(field, 0.0F))) {
    ImGui::OpenPopup(kHairPalettePopup);
  }
  DrawPicker(kHairPalettePopup, core::Tr("Couleur de cheveux"), CreatePicker::kHairPalette,
             kHairPaletteCount - 1, kHairPaletteColumns, kPalettePoints, state.hair_palette, view,
             request);

  ImGui::EndGroup();
}

}  // namespace

CharacterCreateAction DrawCharacterCreate(CharacterCreateState& state,
                                          const CharacterCreateView& view, const SkinArt& art) {
  CharacterCreateAction action;
  if (!state.open()) {
    return action;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));

  static WindowName name(kCreateWindowTitle, kCreateWindowId);
  // ⚠ SANS CROIX, comme le formulaire de connexion : « Annuler » est le geste,
  // et il est à côté de « Créer ». Une croix ferait deux chemins pour la même
  // sortie, dont l'un n'aurait pas de nom.
  if (!BeginSkinnedWindow(name.Get(), nullptr, art, SkinnedTitle::kBar,
                          ImGuiWindowFlags_AlwaysAutoResize)) {
    EndSkinnedWindow();
    return action;
  }

  DrawPreview(state, view, state.form_height_px, action);
  ImGui::SameLine();
  DrawForm(state, view, art, action.picker);
  state.form_height_px = ImGui::GetItemRectSize().y;

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  if (view.pending) {
    ImGui::PushStyleColor(ImGuiCol_Text, kPendingColor);
    ImGui::TextUnformatted(core::Tr("Création en cours…"));
    ImGui::PopStyleColor();
  } else if (!view.refusal.empty()) {
    // ⚠ IL S'ENROULE : le motif fourre-tout du serveur énumère quatre causes,
    // et une ligne qui traverse l'écran ferait grandir la fenêtre à sa mesure.
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kFieldPoints * Scale());
    ImGui::PushStyleColor(ImGuiCol_Text, kRefusalColor);
    ImGui::TextUnformatted(view.refusal.c_str());
    ImGui::PopStyleColor();
    ImGui::PopTextWrapPos();
  }

  // ⚠ LA GARDE EST SUR LE BOUTON, et elle ne porte que ce que l'écran SAIT :
  // un nom vide, et une demande déjà partie. Un nom trop court, une balise de
  // lien d'objet ou un mot interdit se refusent à l'envoi, avec leur message ;
  // les lettres autorisées et le plafond de personnages sont au serveur — les
  // recopier au jugé donnerait une seconde vérité, celle qui refuserait un nom
  // qu'il aurait pris.
  const bool can_create = state.name[0] != '\0' && !view.pending;
  ImGui::BeginDisabled(!can_create);
  if (SkinButton(core::Tr("Créer"), art)) {
    action.verb = CharacterCreateVerb::kSubmit;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art)) {
    action.verb = CharacterCreateVerb::kCancel;
  }
  // ⚠ ÉCHAPPEMENT FERME, comme partout dans ce client — mais pas quand une
  // grille est ouverte : elle a déjà pris la touche pour se refermer.
  if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && action.picker.open == CreatePicker::kNone) {
    action.verb = CharacterCreateVerb::kCancel;
  }

  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
