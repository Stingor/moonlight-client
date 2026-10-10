#include "world/grey_world_view.h"

#include <cmath>

#include "ro/core/log.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

namespace moonclient {

const ro::formats::Image* GreyWorldGrid::TextureFor(ro::vfs::Vfs& vfs, std::string_view path) {
  if (const auto found = textures_.find(path); found != textures_.end()) {
    return found->second.has_value() ? &*found->second : nullptr;
  }
  auto& slot = textures_[std::string(path)];
  // 🔴 LE CARREAU PLEIN SE BÂTIT, IL NE SE LIT PAS : aucun fichier du jeu n'est
  // un carré blanc opaque, et aller le chercher rendrait un motif éteint sans
  // que rien ne s'en plaigne.
  if (ro::render::GreyWorldIsPainted(path)) {
    slot = ro::render::ComposeGreyWorldTile();
    return &*slot;
  }
  auto bytes = vfs.Read(std::string(path));
  if (!bytes) {
    ro::core::LogWarning("GreyWorld : {} absent — ce motif ne se peindra pas", path);
    return nullptr;
  }
  auto image = ro::formats::DecodeImage(std::string(path), *bytes);
  if (!image) {
    ro::core::LogWarning("GreyWorld : {} illisible — ce motif ne se peindra pas", path);
    return nullptr;
  }
  slot = std::move(*image);
  return &*slot;
}

std::span<const ro::render::DrawnGroundQuads> GreyWorldGrid::Cells(
    ro::vfs::Vfs& vfs, const ro::render::GreyWorldLook& look, const MapStage& stage,
    const ro::sim::World& world, ro::sim::WalkPlanner& planner, std::uint32_t now_ms) {
  lots_.clear();
  if (!ro::render::GreyWorldShowsGrid(look) || !stage.gat.has_value()) {
    return lots_;
  }
  // AUTOUR DU JOUEUR, à sa place du moment : un carré posé ailleurs ne dirait
  // rien du terrain sur lequel il se bat.
  const ro::sim::Actor* player = world.player();
  if (player == nullptr) {
    return lots_;
  }
  const ro::sim::FractionalCell cell =
      ro::sim::InterpolatedCell(*player, planner.PathFor(*player), now_ms);
  ro::render::ComposeGreyWorldCells(stage.scene.gnd, *stage.gat,
                                    static_cast<std::int32_t>(std::lround(cell.x)),
                                    static_cast<std::int32_t>(std::lround(cell.y)), look, quads_);
  if (quads_.empty()) {
    return lots_;
  }
  texture_path_ = std::string(ro::render::GreyWorldTexturePath(look.pattern));
  const ro::formats::Image* texture = TextureFor(vfs, texture_path_);
  if (texture == nullptr) {
    return lots_;
  }
  lots_.push_back(ro::render::DrawnGroundQuads{quads_, &texture_path_, texture});
  return lots_;
}

bool StaffGroundWins(const ro::ui::FlatGroundLook& staff_ground, bool staff_tools_allowed) {
  return staff_ground.on && staff_tools_allowed;
}

std::optional<std::array<float, 3>> FlatGroundOf(const ro::ui::FlatGroundLook& staff_ground,
                                                 bool staff_tools_allowed,
                                                 const ro::render::GreyWorldLook& grey_world) {
  if (StaffGroundWins(staff_ground, staff_tools_allowed)) {
    return ro::ui::FlatGroundColorOf(staff_ground);
  }
  if (ro::render::GreyWorldPaintsGround(grey_world)) {
    return ro::render::GreyWorldGroundColor(grey_world);
  }
  return std::nullopt;
}

}  // namespace moonclient
