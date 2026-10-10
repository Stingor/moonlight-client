#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/map_effect_table.h"

namespace ro::render {

// Combien de segments porte un anneau de vent : vingt et un, dont le premier ne
// se peint pas — il ne sert qu'à donner son bord au deuxième.
inline constexpr std::size_t kWindSegmentsPerRing = 21;

// UN ANNEAU DE VENT : l'effet de carte 2343.
//
// Chaque anneau est un RUBAN : un point tourne sur un cercle, et le ruban relie
// ses vingt et une positions, du bord intérieur (rayon `radius`) au bord
// extérieur (rayon `radius + thickness`, et `thickness` plus bas). L'arc
// couvert est `full_angle` ; à 360, le ruban se referme. Plusieurs anneaux
// s'étagent sur `height`, chacun parti d'un angle et d'une phase d'alpha tirés
// au hasard — c'est le halo qui entoure le cristal du navire de mu_fild01.
//
// L'alpha d'un anneau RESPIRE : il monte d'une unité par pas jusqu'à son pic,
// redescend d'une unité jusqu'à zéro, et recommence.
class MapWind {
 public:
  // L'anneau posé par un objet 2343 du RSW. La place de la table, quand elle en
  // donne une, remplace celle de l'objet.
  MapWind(glm::vec3 native_position, MapWindSetting setting, std::uint32_t& draws);

  // Un pas : le tri par distance, puis la mise à jour s'il est à portée.
  void Tick(glm::vec3 native_player);

  [[nodiscard]] bool drawn() const noexcept { return ticked_ && !hidden_; }

  // Les quads de ses rubans, dans NOTRE repère.
  void AppendQuads(const MapEffectFrame& frame, std::vector<EffectWorldQuad>& quads) const;

  [[nodiscard]] const MapWindSetting& setting() const noexcept { return setting_; }
  [[nodiscard]] glm::vec3 native_position() const noexcept { return position_; }
  [[nodiscard]] std::size_t segment_count() const noexcept { return segments_.size(); }

 private:
  struct Segment {
    std::uint16_t ring = 0;
    std::uint16_t index = 0;
    // En degrés, et il avance de `speed` par pas.
    float angle_degrees = 0.0F;
    // De combien l'anneau MONTE au-dessus du premier. Le client l'écrit en
    // négatif sur son ordonnée, qui descend.
    float lift = 0.0F;
    std::int32_t alpha = 0;
    // Zéro : l'alpha monte. Un : il descend.
    std::uint8_t breath = 0;
    // Les deux bords de ce segment et ceux du précédent, dans le repère du
    // client : le quad va de l'un à l'autre.
    glm::vec3 inner{0.0F, 0.0F, 0.0F};
    glm::vec3 outer{0.0F, 0.0F, 0.0F};
    glm::vec3 previous_inner{0.0F, 0.0F, 0.0F};
    glm::vec3 previous_outer{0.0F, 0.0F, 0.0F};
  };

  glm::vec3 position_{0.0F, 0.0F, 0.0F};
  MapWindSetting setting_;
  std::vector<Segment> segments_;
  bool hidden_ = false;
  bool ticked_ = false;
};

}  // namespace ro::render
