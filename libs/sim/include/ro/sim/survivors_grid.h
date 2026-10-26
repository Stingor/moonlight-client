#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/sim/survivors_geometry.h"

namespace ro::sim {

// LES VOISINS D'UN POINT, SANS TOUT PARCOURIR. Des seaux carrés de
// kGridBucketCells cases de côté, reconstruits à chaque pas par dénombrement
// (compte, préfixe, remplissage) dans l'ordre des index : aucune allocation
// après le premier pas.
//
// Un point hors de l'aire tombe dans le seau du bord le plus proche : il reste
// trouvable, au prix d'un seau plus chargé. Comme Near filtre par la distance
// exacte et rend des index croissants, la place de l'aire ne change aucune
// réponse : une grille peut suivre le héros sur un sol sans bord.
inline constexpr std::int32_t kGridBucketCells = 2;

class SurvivorsGrid {
 public:
  // L'aire couverte, en cases ; une aire vide se ramène à un seul seau.
  // L'origine de l'aire est gardée.
  void Reset(std::int32_t width_cells, std::int32_t height_cells);
  // L'aire commence désormais à la case (origin_x, origin_y) ; elle vaut
  // (0, 0) par défaut. Vaut pour le prochain Rebuild, qui doit suivre.
  void Recenter(std::int32_t origin_x_cells, std::int32_t origin_y_cells) noexcept;
  void Rebuild(std::span<const FieldPoint> points);
  // Les index des points à distance <= radius_units de center, CROISSANTS, dans
  // `out` (vidé). `points` est le span passé à Rebuild.
  void Near(FieldPoint center, std::int32_t radius_units, std::span<const FieldPoint> points,
            std::vector<std::uint32_t>& out) const;

  // La place retenue pour les index : elle ne bouge plus quand le nombre de
  // points ne grandit pas.
  [[nodiscard]] std::size_t capacity() const noexcept { return indexes_.capacity(); }

 private:
  [[nodiscard]] std::size_t BucketOf(FieldPoint point) const noexcept;

  // L'origine de l'aire, en sous-unités.
  std::int64_t origin_x_units_ = 0;
  std::int64_t origin_y_units_ = 0;
  std::int32_t columns_ = 0;
  std::int32_t rows_ = 0;
  std::vector<std::uint32_t> starts_;  // columns_ * rows_ + 1
  std::vector<std::uint32_t> indexes_;
};

}  // namespace ro::sim
