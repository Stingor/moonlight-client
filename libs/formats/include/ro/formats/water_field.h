#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "ro/core/error.h"

namespace ro::formats {

// Niveau d'eau applicable à chaque cellule GAT d'une carte.
//
// Trois formes, parce que les données en ont trois :
//   · aucune eau ;
//   · un niveau unique — le RSW jusqu'en 2.5 le porte dans son en-tête ;
//   · une grille de zones — à partir du RSW 2.6 l'eau a déménagé dans le GND,
//     qui la découpe en `split_width × split_height` régions.
//
// Les zones ne sont pas une hypothèse : sur les cartes du serveur, la plupart
// n'ont qu'une zone, mais jor_dun02 et jor_que en ont 3×3 avec deux niveaux
// distincts (2.0 et 20.0). Un niveau unique y donnerait la mauvaise grille.
class WaterField {
 public:
  [[nodiscard]] static WaterField None() noexcept { return {}; }
  [[nodiscard]] static WaterField Uniform(float level);

  // `levels` compte `split_width × split_height` entrées, rangée par rangée.
  // `gnd_width` et `gnd_height` sont les dimensions de la grille de CUBES.
  [[nodiscard]] static core::Result<WaterField> Zoned(std::vector<float> levels,
                                                      std::int32_t split_width,
                                                      std::int32_t split_height,
                                                      std::int32_t gnd_width,
                                                      std::int32_t gnd_height);

  [[nodiscard]] bool has_water() const noexcept { return !levels_.empty(); }
  [[nodiscard]] std::size_t zone_count() const noexcept { return levels_.size(); }

  // `x` et `y` sont des coordonnées de cellule GAT, pas de cube GND.
  [[nodiscard]] std::optional<float> LevelAt(std::int32_t x, std::int32_t y) const;

 private:
  WaterField() = default;

  std::vector<float> levels_;
  std::int32_t split_width_ = 1;
  std::int32_t split_height_ = 1;
  std::int32_t cubes_per_zone_x_ = 1;
  std::int32_t cubes_per_zone_y_ = 1;
};

}  // namespace ro::formats
