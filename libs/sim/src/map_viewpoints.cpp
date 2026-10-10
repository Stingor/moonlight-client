#include "ro/sim/map_viewpoints.h"

#include <algorithm>

namespace ro::sim {
namespace {

[[nodiscard]] bool HasKey(const Viewpoint& viewpoint, std::uint32_t npc_id,
                          std::int32_t point_id) noexcept {
  return viewpoint.npc_id == npc_id && viewpoint.point_id == point_id;
}

// Vrai quand un repère éphémère a fait son temps. La soustraction non signée
// reste juste quand l'horloge de l'appelant fait le tour de ses trente-deux
// bits.
[[nodiscard]] bool HasExpired(const Viewpoint& viewpoint, std::uint32_t now_ms) noexcept {
  return !viewpoint.permanent && now_ms - viewpoint.placed_ms >= kEphemeralViewpointLifetimeMs;
}

}  // namespace

void MapViewpoints::Place(const Viewpoint& viewpoint) {
  const auto same = std::ranges::find_if(viewpoints_, [&viewpoint](const Viewpoint& held) {
    return HasKey(held, viewpoint.npc_id, viewpoint.point_id);
  });
  if (same != viewpoints_.end()) {
    *same = viewpoint;
    return;
  }
  viewpoints_.push_back(viewpoint);
}

bool MapViewpoints::Remove(std::uint32_t npc_id, std::int32_t point_id) {
  return std::erase_if(viewpoints_, [npc_id, point_id](const Viewpoint& held) {
           return HasKey(held, npc_id, point_id);
         }) > 0;
}

std::size_t MapViewpoints::PurgeExpired(std::uint32_t now_ms) {
  return std::erase_if(viewpoints_,
                       [now_ms](const Viewpoint& held) { return HasExpired(held, now_ms); });
}

const Viewpoint* MapViewpoints::Find(std::uint32_t npc_id, std::int32_t point_id) const noexcept {
  const auto found = std::ranges::find_if(viewpoints_, [npc_id, point_id](const Viewpoint& held) {
    return HasKey(held, npc_id, point_id);
  });
  return found == viewpoints_.end() ? nullptr : &*found;
}

}  // namespace ro::sim
