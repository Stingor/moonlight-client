#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"
#include "ro/formats/image.h"
#include "ro/render/ground_quad.h"
#include "ro/render/ground_quad_renderer.h"
#include "ro/render/navi_trail.h"
#include "ro/sim/packed_position.h"
#include "ro/vfs/vfs.h"

namespace moonclient {

// LA TRACE DU GUIDAGE, prête à peindre : les flèches de `ro/render/navi_trail.h`
// posées sur le relief, rangées en un lot par direction — une image par lot.
//
// Elle tient les huit images du jeu de flèches que le joueur a choisi, et les
// relit quand il en change. Une image absente de l'archive laisse sa direction
// sans flèche, et le reste de la trace se peint.
class NaviTrailView {
 public:
  // Les lots de cette image, vides quand le chemin ne porte aucune flèche. Le
  // résultat vit jusqu'au prochain appel.
  [[nodiscard]] std::span<const ro::render::DrawnGroundQuads> Compose(
      ro::vfs::Vfs& vfs, const ro::formats::GndMap& gnd, const ro::formats::GatMap& gat,
      std::span<const ro::sim::CellPosition> path_cells, int icon_set, std::uint32_t now_ms);

 private:
  void UseIconSet(ro::vfs::Vfs& vfs, int icon_set);

  // Le jeu dont les images sont chargées ; aucun avant le premier appel.
  std::optional<int> icon_set_;
  // La révision des surcharges du VFS à la lecture : un skin changé fait
  // relire les flèches.
  std::uint64_t overlay_revision_ = 0;
  // Le chemin de chaque image doit survivre à l'appel : le renderer indexe son
  // téléversement dessus.
  std::array<std::string, ro::render::kNaviTrailDirections> texture_paths_;
  std::array<std::optional<ro::formats::Image>, ro::render::kNaviTrailDirections> textures_;
  std::vector<glm::ivec2> cells_;
  std::array<std::vector<ro::render::GroundQuad>, ro::render::kNaviTrailDirections> quads_;
  std::vector<ro::render::DrawnGroundQuads> lots_;
};

}  // namespace moonclient
