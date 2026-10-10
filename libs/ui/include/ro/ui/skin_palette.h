#pragma once

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA PALETTE DU SKIN, posée sur une fenêtre : les couleurs de son habillage
// (`SkinLook`, que porte `SkinArt`) et les mesures du jeu.

// 🔴 LA HAUTEUR D'UN CADRE EST CELLE DE LA BARRE DE TITRE DU JEU, et c'est la
// mesure qui donne à l'interface sa DENSITÉ.
//
// Un cadre d'ImGui — champ de saisie, jauge, liste déroulante — vaut la police
// plus deux fois son remplissage vertical. Laissé au défaut d'ImGui, ce
// remplissage vaut trois pixels et le cadre en fait vingt et un ; on le calcule
// ici pour que le cadre vaille exactement la hauteur de l'art de barre de
// titre — dix-sept. Quatre pixels par cadre, sur chaque ligne de chaque fenêtre :
// c'est ce qui sépare une interface qui ressemble au jeu d'une qui y ressemble
// presque.
//
// ⚠ ELLE VIENT DE L'ART, jamais d'un nombre écrit ici : un habillage de joueur
// peut changer cette hauteur, et la densité doit le suivre.
[[nodiscard]] float SkinFramePaddingY(const SkinArt& art);

// Pose la palette et les mesures. Rendent ce qu'il faudra retirer — jamais un
// nombre écrit à la main, qui dériverait à la première couleur ajoutée.
struct SkinStylePushed {
  int colors = 0;
  int vars = 0;
};
[[nodiscard]] SkinStylePushed PushSkinStyle(const SkinArt& art);
void PopSkinStyle(SkinStylePushed pushed);

// LE PAPIER : le corps des descriptions et des bulles, et ce qu'on y écrit — son
// texte, et le texte grisé et les liens de l'habillage rendus lisibles sur lui.
//
// ⚠ IL POSE LE FOND D'UNE FENÊTRE ET CELUI D'UNE BULLE À LA FOIS : une
// description est une fenêtre, son aperçu une bulle, et le même papier les
// habille. Se retire par `PopStyleColor(kPaperStyleColors)`.
inline constexpr int kPaperStyleColors = 5;
void PushPaperStyle(const SkinLook& look);

}  // namespace ro::ui
