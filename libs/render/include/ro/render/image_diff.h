#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "ro/formats/image.h"

namespace ro::render {

// Comparaison de deux images, chiffres à l'appui.
//
// POURQUOI CELA EXISTE
//
// Un défaut de rendu se diagnostique mal à l'œil. Deux captures prises sous des
// angles voisins donnent l'impression d'un écart là où il n'y en a pas, et
// cachent celui qui compte. Plusieurs corrections ont été prises puis reprises
// sur cette base, chacune plausible et aucune mesurée.
//
// Ici, rien ne se discute : combien de pixels diffèrent, de combien, et OÙ.
// « La face arrière est trop claire » devient un écart moyen sur une région
// nommée, qu'une correction fait baisser ou non.
//
// Le calcul est une transformation de données PURE : il se teste sans fenêtre
// ni carte graphique, donc aussi sous Linux avec les sanitizers.

// Écart par canal au-delà duquel un pixel compte comme différent.
//
// Deux rendus du même contenu ne sont jamais identiques au bit près : le
// filtrage de texture et l'ordre des additions flottantes font varier le
// dernier chiffre. Compter ces pixels-là noierait le signal.
inline constexpr int kNoticeableChannelGap = 8;

// Côté du damier de régions. Une moyenne sur l'image entière dit qu'il y a un
// écart, pas où le chercher ; neuf régions répondent à la question suivante
// sans que l'appelant ait à la poser.
inline constexpr std::size_t kRegionsPerSide = 3;

struct ImageComparison {
  std::size_t compared = 0;
  std::size_t differing = 0;
  double mean_gap = 0.0;
  int worst_gap = 0;
  // Écart moyen par région, rangées d'abord : la première est le HAUT de
  // l'image, comme à l'écran.
  std::array<double, kRegionsPerSide * kRegionsPerSide> region_mean{};
};

// Compare deux images de même taille.
//
// `difference`, s'il est fourni, reçoit une image en niveaux de gris où l'écart
// est amplifié pour se voir. Elle sert à SITUER, jamais à mesurer : les
// chiffres sont dans le résultat, et l'amplification y sature.
//
// Rend nullopt si les tailles diffèrent. Rééchantillonner pour les rapprocher
// fabriquerait des pixels, et l'écart mesuré serait en partie le nôtre.
[[nodiscard]] std::optional<ImageComparison> CompareImages(const formats::Image& left,
                                                           const formats::Image& right,
                                                           formats::Image* difference = nullptr);

// De combien l'écart est amplifié dans l'image de différence.
inline constexpr int kDifferenceGain = 8;

}  // namespace ro::render
