#pragma once

#include <imgui.h>

namespace ro::ui {

// LE CADRE DE LA CHATBOX, ET CE N'EST PAS UNE FENÊTRE DU JEU.
//
// ⚠ ELLE NE PREND NI LA PALETTE NI LES PIÈCES. Une fenêtre de Ragnarok est
// claire, bordée, coiffée d'une barre de titre ; la chatbox est un rectangle
// translucide posé sur le monde, sans titre, sans bouton de fermeture. Les deux
// habillages n'ont en commun que l'ascenseur.
//
// ⚠ ET SON CORPS EST SOMBRE, DONC SES CHAMPS DE SAISIE RESTENT CLAIRS. Les
// `UIEditWnd` du chat natif sont blancs sur le cadre sombre : c'est ce qui rend
// la saisie lisible quand tout le reste est translucide. Un champ qui prendrait
// la couleur du cadre serait du texte clair sur un fond clair — donc rien.
// Autour d'un `InputText`, il faut donc pousser `kChatDarkTextColor`.
//
// ⚠ ELLE NE SE REDIMENSIONNE PAS PAR LES POIGNÉES D'IMGUI, et c'est le point
// qu'on ne devine pas. Voir `EndChatFrame`.

// Le fond. Alpha 150 sur du noir.
//
// ⚠ LE CLIENT EN MET MOINS — 102 — et c'est délibérément plus opaque ici : à
// 102, un texte blanc posé sur un sol clair de Prontera devient illisible. C'est
// le réglage retenu après l'avoir essayé en jeu.
inline constexpr ImU32 kChatBodyColor = IM_COL32(0, 0, 0, 150);
// Le filet du cadre, gris clair.
inline constexpr ImU32 kChatBorderColor = IM_COL32(197, 197, 197, 255);
// Les champs de saisie : clairs, comme ceux du chat natif.
inline constexpr ImU32 kChatFieldColor = IM_COL32(206, 206, 206, 255);
inline constexpr ImU32 kChatFieldHoverColor = IM_COL32(222, 222, 222, 255);
inline constexpr ImU32 kChatFieldActiveColor = IM_COL32(238, 238, 238, 255);
// Le texte à poser SUR un champ clair — donc presque noir, à l'inverse du texte
// du cadre. Il vaut aussi pour les infobulles et les menus, qui sont clairs.
inline constexpr ImU32 kChatDarkTextColor = IM_COL32(20, 20, 20, 255);

// L'arrondi des coins. ⚠ LE CHAT NATIF EST CARRÉ ; on adoucit.
inline constexpr float kChatRounding = 6.0F;
// La marge intérieure, et l'interligne de l'historique. Serrés : une chatbox
// montre le plus de lignes possible dans le moins de place possible.
inline constexpr float kChatPadding = 3.0F;
inline constexpr float kChatLineGap = 2.0F;
// La plus petite chatbox utilisable. En deçà, une ligne ne tient plus.
inline constexpr float kChatSmallestWidth = 400.0F;
inline constexpr float kChatSmallestHeight = 200.0F;
// La plus grande, en part d'écran. Une part plutôt qu'un nombre de pixels : un
// changement de résolution ne doit pas rendre la borne absurde.
inline constexpr float kChatLargestScreenShare = 0.8F;

// L'apparence et les libertés de la chatbox. Tout y vient du joueur — c'est
// `app` qui la remplit depuis ses réglages, `ui` ne retient rien.
struct ChatStyle {
  ImU32 body = kChatBodyColor;
  ImU32 border = kChatBorderColor;
  // ⚠ NI `padding` NI `line_gap` NE PASSENT PAR L'ÉCHELLE D'INTERFACE, et c'est
  // délibéré : ce sont des réglages du joueur, exprimés en pixels. Surtout, le
  // pas des lignes du log se calcule avec ces MÊMES valeurs brutes — les mettre
  // à l'échelle ici et pas là désaccorderait le texte de son interligne.
  float padding = kChatPadding;
  float line_gap = kChatLineGap;
  float rounding = kChatRounding;
  float smallest_width = kChatSmallestWidth;
  float smallest_height = kChatSmallestHeight;
  // LA HAUTEUR SE QUANTIFIE EN RANGÉES ENTIÈRES : `chrome_pixels + N × row_pixels`.
  // Sans quoi le bas de la fenêtre coupe une ligne en deux, comme aucun chat ne
  // le fait. Les deux mesures viennent de l'image PRÉCÉDENTE — c'est le dessin
  // qui sait la hauteur d'une ligne et la place que prend tout le reste.
  //
  // `row_pixels` à zéro = aucune quantification (première image, log vide).
  float row_pixels = 0.0F;
  float chrome_pixels = 0.0F;
  // Verrouillage de la géométrie. Séparés parce qu'ils se verrouillent parfois
  // séparément : une fenêtre figée en taille peut rester déplaçable.
  bool movable = true;
  bool resizable = true;
};

// Ouvre la chatbox. Pas de `open` : celle du client n'a pas de bouton de
// fermeture — elle se replie, ou se coupe dans les options.
[[nodiscard]] bool BeginChatFrame(const char* name, const ChatStyle& style,
                                  ImGuiWindowFlags flags = 0);

// Referme, et POSE LES QUATRE BORDS DE REDIMENSIONNEMENT.
//
// ⚠ CE NE SONT PAS CEUX D'IMGUI, et il y a deux raisons de les refaire. La
// première : ImGui ne sait pas désactiver un bord en particulier, or un bord
// collé au bord de l'écran ne peut pas s'écarter — la fenêtre grandit alors du
// côté OPPOSÉ, ce qui donne l'inverse du geste demandé. La seconde : la hauteur
// se quantifie en rangées, et un arrondi appliqué par ImGui APRÈS coup ne
// toucherait que la taille — le bord qu'on ne tire pas se mettrait à dériver.
//
// ⚠ EN FIN DE FENÊTRE, jamais au début : à survol égal, c'est le dernier élément
// soumis qui gagne. Les bords passent ainsi devant le contenu, pas derrière.
void EndChatFrame();

}  // namespace ro::ui
