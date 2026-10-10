#include "ro/sim/quest_markers.h"

namespace ro::sim {

void QuestMarkers::Tell(std::uint32_t npc_id, QuestMarker marker) {
  // Un retrait ne garde pas la case qu'il porte : la marque n'est plus nulle
  // part, et la minimap ne doit plus la situer.
  if (!marker.shown()) {
    marker.cell_x = kQuestMarkerNoCell;
    marker.cell_y = kQuestMarkerNoCell;
  }
  markers_[npc_id] = marker;
}

const QuestMarker* QuestMarkers::ShownFor(std::uint32_t npc_id) const {
  const auto found = markers_.find(npc_id);
  if (found == markers_.end() || !found->second.shown()) {
    return nullptr;
  }
  return &found->second;
}

}  // namespace ro::sim
