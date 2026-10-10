#pragma once

#include <span>

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// UNE GRILLE DE CASES NUMÉROTÉES, avec sa vignette dans chacune.
//
// ⚠ PARTAGÉE PAR L'ÉDITEUR DE STYLE ET LA CRÉATION DE PERSONNAGE, et ce n'est
// pas un rangement : les deux montrent LES MÊMES quatre-vingts coupes et deux
// cent cinquante et une teintes, au même moment de la vie du joueur — l'une au
// salon de coiffure, l'autre à la naissance. Deux grilles dériveraient, et la
// première à dériver serait la garde de visibilité, qu'on oublie.
//
// ⚠ ELLE NE DESSINE AUCUN SPRITE ELLE-MÊME : `ui` ne compose pas d'acteur. Les
// vignettes arrivent toutes faites, et une case dont l'image manque reste
// vide — ce qui est la bonne réponse aussi bien pour un numéro sans sprite que
// pour une vignette pas encore composée.
//
// ⚠ EN-TÊTE PRIVÉ : rien hors de `libs/ui/src` n'appelle ceci.

struct ThumbnailGrid {
  // Les numéros montrés, bornes comprises.
  int first = 0;
  int last = 0;
  int columns = 1;
  // Le côté d'une case, en pixels d'écran.
  float cell = 0.0F;
  // Le numéro allumé, ou moins un.
  int chosen = -1;
  // Les vignettes, celle de `thumbnails_first` en tête.
  std::span<const SkinImage> thumbnails;
  int thumbnails_first = 0;
};

// CE QUE LA GRILLE REND.
//
// ⚠ `seen_first` ET `seen_count` SONT LA TRANCHE RÉELLEMENT VISIBLE, et c'est
// ce que l'appelant renvoie à `app` : sans ce relevé, ouvrir la grille des
// teintes demanderait deux cent cinquante et une vignettes dont on en regarde
// cent soixante.
struct ThumbnailChoice {
  int picked = -1;
  bool close = false;
  int seen_first = 0;
  int seen_count = 0;
};

// Le simple clic laisse la grille ouverte — c'est ce qui permet de comparer
// trois coiffures en les essayant, et c'est la raison d'être de ces grilles. Le
// DOUBLE clic referme : le joueur a tranché.
[[nodiscard]] ThumbnailChoice DrawThumbnailGrid(const ThumbnailGrid& grid);

}  // namespace ro::ui
