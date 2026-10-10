#pragma once

#include "ro/render/gl_context.h"

namespace ro::render {

// LA TAILLE D'UNE FENÊTRE QUI TIENT SUR L'ÉCRAN.
//
// `wanted` est la taille demandée — celle d'usine, ou celle que le joueur a
// choisie —, `room` la place utile de l'écran, barre des tâches et bords de la
// fenêtre déjà ôtés.
//
// Une taille qui tient revient TELLE QUELLE : c'est le choix du joueur. Une
// taille qui déborde se réduit, aux mêmes proportions, jusqu'à une part de la
// place — et non toute la place, parce que la barre de titre doit rester
// atteignable quand les bords ne sont pas encore connus. Elle ne descend pas
// sous un plancher, sauf sur un écran plus petit que lui.
//
// ⚠ UNE DEMANDE OU UNE PLACE VIDE REVIENT TELLE QUELLE : sans place connue, il
// n'y a rien à borner, et une taille vide se remplace en amont par celle
// d'usine.
[[nodiscard]] Resolution FitWindowSize(Resolution wanted, Resolution room) noexcept;

// La part de la place qu'occupe une fenêtre réduite.
inline constexpr float kFittedRoomShare = 0.9F;

// La plus petite fenêtre qu'on rende : en dessous, l'interface ne tient plus.
inline constexpr Resolution kSmallestWindow{.width = 800, .height = 450};

}  // namespace ro::render
