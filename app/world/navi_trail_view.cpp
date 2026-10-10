#include "world/navi_trail_view.h"

#include <cstddef>
#include <utility>

#include "ro/core/error.h"
#include "ro/core/log.h"

namespace moonclient {

void NaviTrailView::UseIconSet(ro::vfs::Vfs& vfs, int icon_set) {
  if (icon_set_ == icon_set && overlay_revision_ == vfs.overlay_revision()) {
    return;
  }
  icon_set_ = icon_set;
  overlay_revision_ = vfs.overlay_revision();
  for (std::size_t direction = 0; direction < ro::render::kNaviTrailDirections; ++direction) {
    texture_paths_[direction] =
        ro::render::NaviTrailTexturePath(icon_set, static_cast<std::uint8_t>(direction));
    textures_[direction].reset();
    auto bytes = vfs.Read(texture_paths_[direction]);
    if (!bytes) {
      ro::core::LogWarning("trace du guidage : {} absent, cette direction restera sans flèche",
                           texture_paths_[direction]);
      continue;
    }
    auto image = ro::formats::DecodeImage(texture_paths_[direction], *bytes);
    if (!image) {
      ro::core::LogWarning("trace du guidage : {} illisible ({})", texture_paths_[direction],
                           ro::core::Describe(image.error()));
      continue;
    }
    textures_[direction] = std::move(*image);
  }
}

std::span<const ro::render::DrawnGroundQuads> NaviTrailView::Compose(
    ro::vfs::Vfs& vfs, const ro::formats::GndMap& gnd, const ro::formats::GatMap& gat,
    std::span<const ro::sim::CellPosition> path_cells, int icon_set, std::uint32_t now_ms) {
  lots_.clear();
  for (std::vector<ro::render::GroundQuad>& quads : quads_) {
    quads.clear();
  }
  if (path_cells.size() < 2) {
    return lots_;
  }
  UseIconSet(vfs, icon_set);

  // ⚠ LE CHEMIN ENTIER, et non sa seule portée : les flèches se comptent depuis
  // la cible, et le tronquer ici déplacerait leur parité.
  cells_.clear();
  cells_.reserve(path_cells.size());
  for (const ro::sim::CellPosition& cell : path_cells) {
    cells_.emplace_back(cell.x, cell.y);
  }
  for (const ro::render::NaviTrailMark& mark : ro::render::ComposeNaviTrail(cells_, now_ms)) {
    if (!textures_[mark.direction].has_value()) {
      continue;
    }
    if (std::optional<ro::render::GroundQuad> quad =
            ro::render::ComposeNaviTrailQuad(gnd, gat, mark);
        quad.has_value()) {
      quads_[mark.direction].push_back(*quad);
    }
  }
  // Les lots se bâtissent APRÈS les quads : un span pris pendant le remplissage
  // désignerait un vecteur que l'ajout suivant a pu réallouer.
  for (std::size_t direction = 0; direction < ro::render::kNaviTrailDirections; ++direction) {
    if (quads_[direction].empty()) {
      continue;
    }
    lots_.push_back(ro::render::DrawnGroundQuads{.quads = quads_[direction],
                                                 .texture_path = &texture_paths_[direction],
                                                 .texture = &*textures_[direction]});
  }
  return lots_;
}

}  // namespace moonclient
