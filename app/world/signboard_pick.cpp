#include "world/signboard_pick.h"

namespace moonclient {

bool SignboardOpensStore(ro::sim::SignboardKind kind) noexcept {
  return kind == ro::sim::SignboardKind::kVendingShop ||
         kind == ro::sim::SignboardKind::kBuyingStore;
}

bool StoreVisitMayLeave(const StoreVisitGate& gate) noexcept {
  if (gate.owner_id == 0 || gate.owner_id == gate.player_id || gate.player_dead || gate.visiting) {
    return false;
  }
  return !gate.last_sent_ms.has_value() || gate.now_ms - *gate.last_sent_ms > kStoreVisitSpacingMs;
}

std::optional<SignboardHit> TopSignboardAt(std::span<const SignboardHit> boards,
                                           glm::vec2 point_px) {
  for (auto board = boards.rbegin(); board != boards.rend(); ++board) {
    // Le bord droit et le bord bas n'appartiennent pas à l'enseigne : deux
    // enseignes jointives ne se disputent aucune colonne.
    if (point_px.x < board->rect.min.x || point_px.y < board->rect.min.y ||
        point_px.x >= board->rect.max.x || point_px.y >= board->rect.max.y) {
      continue;
    }
    return *board;
  }
  return std::nullopt;
}

std::optional<SignboardHit> StoreSignboardAt(std::span<const SignboardHit> boards,
                                             glm::vec2 point_px) {
  std::optional<SignboardHit> top = TopSignboardAt(boards, point_px);
  if (top.has_value() && !SignboardOpensStore(top->kind)) {
    return std::nullopt;
  }
  return top;
}

}  // namespace moonclient
