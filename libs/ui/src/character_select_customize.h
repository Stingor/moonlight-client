#pragma once

#include <string_view>

#include <imgui.h>

#include "ro/ui/character_select_window.h"

// LE MODE « PERSONNALISER » DE LA TABLÉE : ce qui ne sert qu'à composer la
// scène, séparé de ce qui la montre.
//
// En-tête PRIVÉ : personne hors de cette bibliothèque n'a à dessiner une
// poignée d'ancre. La scène appelle ces pièces aux endroits où elle posait ses
// constantes.
namespace ro::ui {

// BASCULE LA PERSONNALISATION et rend le verbe qui le dit à `app`.
//
// ⚠ EN ENTRANT, la sélection de la liste des mises en page repart de zéro :
// l'index d'un passage précédent ne désigne plus forcément la même entrée,
// une suppression a pu décaler la liste.
[[nodiscard]] CharacterSelectVerb ToggleCustomizing(CharacterSelectState& state);

// LA POSITION ÉCRAN DU BLOC `name`, enregistrée à sa place d'usine (en
// fractions d'écran) à son premier appel.
//
// Pendant la personnalisation, elle devient une pastille qu'on glisse.
// ⚠ SUR LA LISTE DE LA SCÈNE, et non au premier plan : celui-ci passerait
// par-dessus le panneau, qui capte pourtant les clics à cet endroit — on
// verrait des poignées qu'on ne peut pas saisir.
[[nodiscard]] ImVec2 SceneAnchor(CharacterSelectState& state, std::string_view name,
                                 float default_x, float default_y, ImVec2 display);

// LES GESTES SUR UN SIÈGE, juste après son bouton invisible : glisser pour le
// placer, molette pour TOURNER le convive, Ctrl+molette pour sa taille, clic
// droit pour sa pose. Et son cadre, numéroté comme sur le décor.
//
// ⚠ LA ROTATION A LA MOLETTE NUE : c'est le geste le plus fréquent une fois le
// décor changé. Le zoom, qu'on règle une fois, prend le modificateur.
void EditSeat(CharacterSelectState& state, int seat, const SeatFrame& frame, ImDrawList* list,
              ImVec2 display, bool hovered);

// LE MENU DE POSE d'un siège, ouvert par le clic droit.
//
// ⚠ À LA RACINE DE LA SCÈNE, hors de la pile d'identifiants du siège : ouvert
// depuis celle-ci, il ne se retrouverait pas au niveau où on le dessine.
// `padding` est celui de l'habillage : la scène pose le sien à zéro.
void DrawSeatPoseMenu(CharacterSelectState& state, const CharacterSelectView& view, ImVec2 padding);

// LES REPÈRES DE COMPOSITION — règle des tiers et axe central — et le rappel
// des gestes, PAR-DESSUS les sièges.
void DrawLayoutGuides(ImDrawList* list, ImVec2 origin, ImVec2 display);

// LE PANNEAU « PERSONNALISER L'ÉCRAN ».
//
// ⚠ UNE FENÊTRE À PART ENTIÈRE, dessinée APRÈS la scène : celle-ci est en
// `NoBringToFrontOnFocus`, donc le panneau passe au-dessus et reste
// déplaçable, ce qu'un enfant de la scène n'aurait pas permis. Et il se
// replie : il couvre une partie de la scène qu'on est justement en train de
// composer.
[[nodiscard]] CharacterSelectAction DrawLayoutPanel(CharacterSelectState& state,
                                                    const CharacterSelectView& view,
                                                    const SkinArt& art);

}  // namespace ro::ui
