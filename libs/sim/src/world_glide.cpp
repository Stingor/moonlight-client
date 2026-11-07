// LE GLISSEMENT : un déplacement que le client impose à sa propre vitesse, celle
// du bond de Body Relocation. Voir `World::StartGliding`.

#include <cmath>
#include <utility>

#include "ro/sim/movement.h"
#include "ro/sim/world.h"

namespace ro::sim {

bool World::StartGliding(std::uint32_t id, CellPosition to, std::uint16_t ms_per_cell,
                         std::uint32_t now_ms) {
  const Actor* actor = Find(id);
  if (actor == nullptr) {
    return false;
  }
  // LE DÉPART EST L'ENDROIT OÙ L'ACTEUR SE MONTRE, au milieu d'un pas s'il le
  // faut : partir de sa case annoncée le ferait sauter en arrière.
  const FractionalCell here = InterpolatedCell(*actor, now_ms);
  const auto cell_x = static_cast<std::uint16_t>(std::lround(here.x));
  const auto cell_y = static_cast<std::uint16_t>(std::lround(here.y));
  CellMovement path;
  path.from = CellPosition{cell_x, cell_y, actor->position.direction};
  path.to = to;
  path.sub_x = static_cast<std::uint8_t>(
      std::lround((here.x - static_cast<float>(cell_x)) * kSubCellsPerCell + kSubCellCenter));
  path.sub_y = static_cast<std::uint8_t>(
      std::lround((here.y - static_cast<float>(cell_y)) * kSubCellsPerCell + kSubCellCenter));
  if (!StartMoving(id, path, now_ms)) {
    return false;
  }
  actors_.at(id).glide_ms_per_cell = ms_per_cell;
  return true;
}

void World::SpawnWalking(Actor actor, std::uint32_t now_ms) {
  const std::uint32_t id = actor.id;
  const auto known = actors_.find(id);
  if (known == actors_.end()) {
    Spawn(std::move(actor));
    return;
  }
  const ShownWalk shown = ShownWalkAt(known->second, walk_map_, now_ms);
  Spawn(std::move(actor));
  ContinueShownWalk(actors_.at(id), walk_map_, shown, now_ms);
}

bool World::RecordPlayedEffect(std::uint32_t id, const PlayedEffect& played) {
  if (!RecordEffect(id, played.id, played.since_ms, played.value, played.posed_by_state)) {
    return false;
  }
  for (PlayedEffect& kept : actors_.at(id).effects) {
    if (kept.id == played.id) {
      kept.told_color = played.told_color;
      kept.ends_ms = played.ends_ms;
    }
  }
  return true;
}

}  // namespace ro::sim
