#include "game/mail_box_move.h"

namespace moonclient {

MailBoxMove MailBoxMoveFor(bool box_open, bool toggle_asked,
                           std::optional<ro::sim::MailWindowOrder> order) noexcept {
  bool wanted_open = box_open;
  if (order.has_value()) {
    wanted_open = *order == ro::sim::MailWindowOrder::kOpen;
  } else if (toggle_asked) {
    wanted_open = !box_open;
  }
  if (wanted_open == box_open) {
    return MailBoxMove::kNone;
  }
  return wanted_open ? MailBoxMove::kOpen : MailBoxMove::kClose;
}

}  // namespace moonclient
