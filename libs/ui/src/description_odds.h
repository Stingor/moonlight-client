#pragma once

// La description d'un objet : l'onglet « Probabilités » et le fil de
// navigation. Interne à `ui`.

#include <cstdint>
#include <string_view>

#include "ro/ui/description_window.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// UN LIEN DE LA DESCRIPTION — une ligne de table, un membre de combo : l'icône
// (facultative) et son libellé, une seule zone sensible, aux gestes de tout
// lien. `color` nul prend le bleu des liens.
struct DescriptionLinkCell {
  std::uint32_t id = 0;
  bool monster = false;
  // Le nom seul, pour le menu ; `shown`, ce qui s'écrit.
  std::string_view name;
  std::string_view shown;
  bool with_icon = false;
  SkinImage icon;
  ImU32 color = 0;
};
void DrawDescriptionLink(DescriptionWindowState& state, const DescriptionLinkCell& cell,
                         DescriptionWindowAction& action);

// LES QUATRE ONGLETS DU SERVEUR — « Monstres », « Boss / MVP », « Script »,
// « Combos » — : leur contenu, et ce qui les grise. Un onglet ne se grise que
// sur une réponse ARRIVÉE et vide.
[[nodiscard]] bool SourcesKnownEmpty(const DescriptionDropsView& drops);
[[nodiscard]] bool ScriptsKnownEmpty(const DescriptionScriptsView& scripts);
[[nodiscard]] bool CombosKnownEmpty(const DescriptionScriptsView& scripts);
// `boss` : l'onglet « Boss / MVP », sinon « Monstres ».
void DrawSourcesTab(DescriptionWindowState& state, const DescriptionView& view, bool boss,
                    const SkinArt& art, DescriptionWindowAction& action);
void DrawScriptsTab(const DescriptionScriptsView& scripts, const SkinArt& art);
void DrawCombosTab(DescriptionWindowState& state, const DescriptionView& view, const SkinArt& art,
                   DescriptionWindowAction& action);

// L'ONGLET « DESCRIPTION » : le corps seul, ou la comparaison en deux colonnes.
void DrawDescriptionTab(DescriptionWindowState& state, const DescriptionView& view,
                        const SkinArt& art, DescriptionWindowAction& action);

// Un script rAthena dans un bloc « éditeur » : une instruction par ligne,
// coloration, et « Copier ».
void DrawScriptCode(const char* box_id, std::string_view raw, const SkinArt& art);

// Les panneaux accolés sous la fenêtre — options, cartes, familier —, posés à
// partir de `under` sur la largeur de la fenêtre. `in_front` : la fenêtre est
// au premier plan, et ils l'y suivent.
//
// `tag` distingue les deux jeux de panneaux d'une comparaison.
void DrawDescriptionPanels(DescriptionWindowState& state, const DescriptionView& view,
                           const SkinArt& art, ImVec2 under, float width, bool in_front,
                           const char* tag, DescriptionWindowAction& action);

// « ViewID : N » en lien, et le pantin qui porte la pièce au survol.
void DrawViewIdLine(DescriptionDollState& doll, const DescriptionView& view);

// LE PANTIN SOUS LA SOURIS : la demande à `app`, la molette qui le tourne, et
// la bulle. Pour ce qui est survolé à cette image ; `wheel_id` nomme la surface
// que la molette prend.
void ShowDollOver(DescriptionDollState& doll, ImGuiID wheel_id);

// Les mêmes données en fin d'aperçu, en simples listes.
void DrawPreviewInstance(const DescriptionView& view);

// Ce qui sépare une illustration de ce qui se pose à sa droite.
inline constexpr float kPictureGapPixels = 8.0F;

// Le placement des deux fenêtres de description, partagé : voir leur état.
void PlaceDescription(bool& place_at_mouse, bool& was_open, const DescriptionPlacement& placement);
void KeepDescriptionOnScreen();

// « < Retour : nom », tant qu'il y a un pas à défaire.
void DrawDescriptionBack(DescriptionWindowState& state, const DescriptionView& view,
                         const SkinArt& art);

// Le contenu de l'onglet « Probabilités ».
void DrawOddsTab(DescriptionWindowState& state, const DescriptionView& view,
                 DescriptionWindowAction& action);

}  // namespace ro::ui
