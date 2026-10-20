// CE QUE LES PIÈCES DU MONDE PEIGNENT SUR LES ACTEURS : bulles, étoiles, marques,
// glace — chacune seulement si l'installation la porte.

#include <cstdint>
#include <span>
#include <vector>

#include "ro/sim/world.h"

#include "game/game_pieces.h"

namespace moonclient {

std::vector<std::span<const ro::render::DrawnActor>> DrawnHeldChildSprites(
    GamePieces& pieces, std::uint32_t now_ms, std::span<const ro::render::DrawnActor> drawn,
    std::span<const std::uint32_t> ids, const ro::sim::World& world, bool translucent) {
  std::vector<std::span<const ro::render::DrawnActor>> lots;
  const auto keep = [&lots](std::span<const ro::render::DrawnActor> held) {
    if (!held.empty()) {
      lots.push_back(held);
    }
  };
  if (pieces.sleep_bubbles.has_value()) {
    keep(pieces.sleep_bubbles->Drawn(now_ms, drawn, ids, world, translucent));
  }
  if (pieces.silence_dots.has_value()) {
    keep(pieces.silence_dots->Drawn(now_ms, drawn, ids, world, translucent));
  }
  if (pieces.stun_stars.has_value()) {
    keep(pieces.stun_stars->Drawn(now_ms, drawn, ids, world, translucent));
  }
  if (pieces.quest_markers.has_value()) {
    keep(pieces.quest_markers->Drawn(now_ms, drawn, ids, world, translucent));
  }
  if (pieces.companion_target_marks.has_value()) {
    keep(pieces.companion_target_marks->Drawn(now_ms, pieces.companion_ai.target_marks(), drawn,
                                              ids, world, translucent));
  }
  return lots;
}

std::span<const ro::render::DrawnActor> DrawnFrozenIce(
    GamePieces& pieces, std::uint32_t now_ms, std::span<const ro::render::DrawnActor> drawn,
    std::span<const std::uint32_t> ids, const ro::sim::World& world, bool translucent) {
  if (!pieces.frozen_ice.has_value()) {
    return {};
  }
  return pieces.frozen_ice->Drawn(now_ms, drawn, ids, world, translucent);
}

std::span<const ro::render::DrawnActor> DrawnCurseMarks(
    GamePieces& pieces, std::uint32_t now_ms, float camera_yaw_degrees,
    std::span<const ro::render::DrawnActor> drawn, std::span<const std::uint32_t> ids,
    const ro::sim::World& world, bool translucent) {
  if (!pieces.curse_marks.has_value()) {
    return {};
  }
  return pieces.curse_marks->Drawn(now_ms, camera_yaw_degrees, drawn, ids, world, translucent);
}

}  // namespace moonclient
