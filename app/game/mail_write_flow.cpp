#include "game/mail_write_flow.h"

#include "ro/sim/mailbox.h"

namespace moonclient {

void MailWriteFlow::NoteAsked(std::uint32_t now_ms) noexcept {
  asking_ = true;
  asked_ms_ = now_ms;
}

MailWriteOrders MailWriteFlow::TellGranted() noexcept {
  asking_ = false;
  MailWriteOrders orders;
  if (discard_next_grant_) {
    discard_next_grant_ = false;
    orders.emit_cancel = true;
    return orders;
  }
  if (reopening_) {
    reopening_ = false;
    orders.keep_typing = true;
    orders.redo_attachments = redo_attachments_;
    orders.outcome_unknown = outcome_unknown_;
    redo_attachments_ = false;
    outcome_unknown_ = false;
    return orders;
  }
  // Une écriture déjà montrée ne se remet pas à zéro sous les doigts du joueur.
  if (shown_) {
    return orders;
  }
  shown_ = true;
  orders.open_fresh = true;
  return orders;
}

MailWriteOrders MailWriteFlow::TellRefused() noexcept {
  asking_ = false;
  MailWriteOrders orders;
  if (discard_next_grant_) {
    discard_next_grant_ = false;
    return orders;
  }
  if (reopening_) {
    reopening_ = false;
    redo_attachments_ = false;
    outcome_unknown_ = false;
    shown_ = false;
    orders.close_window = true;
    orders.reopen_failed = true;
  }
  return orders;
}

MailWriteOrders MailWriteFlow::TellSendResult(std::uint8_t code, bool carried_attachments,
                                              std::uint32_t now_ms) noexcept {
  MailWriteOrders orders;
  if (!shown_ || reopening_) {
    return orders;
  }
  orders.emit_cancel = true;
  if (code == ro::sim::kMailSendSucceeded) {
    shown_ = false;
    orders.close_window = true;
    return orders;
  }
  reopening_ = true;
  redo_attachments_ = carried_attachments;
  outcome_unknown_ = code == ro::sim::kMailSilent;
  asking_ = true;
  asked_ms_ = now_ms;
  orders.emit_reopen = true;
  return orders;
}

MailWriteOrders MailWriteFlow::NoteClosedByPlayer() noexcept {
  MailWriteOrders orders;
  if (!shown_) {
    return orders;
  }
  shown_ = false;
  if (reopening_) {
    // L'accord de la réouverture peut encore venir : il ne rouvrira rien.
    reopening_ = false;
    redo_attachments_ = false;
    outcome_unknown_ = false;
    discard_next_grant_ = true;
  }
  orders.emit_cancel = true;
  orders.close_window = true;
  return orders;
}

MailWriteOrders MailWriteFlow::NoteServerDropped() noexcept {
  MailWriteOrders orders;
  if (!shown_ || reopening_) {
    return orders;
  }
  shown_ = false;
  orders.close_window = true;
  return orders;
}

MailWriteOrders MailWriteFlow::Tick(std::uint32_t now_ms) noexcept {
  MailWriteOrders orders;
  if (!asking_ || now_ms - asked_ms_ < kMailWriteAskSilenceMs) {
    return orders;
  }
  asking_ = false;
  // Un accord tardif n'aurait plus personne pour l'attendre.
  discard_next_grant_ = true;
  if (reopening_) {
    reopening_ = false;
    redo_attachments_ = false;
    outcome_unknown_ = false;
    shown_ = false;
    orders.close_window = true;
    orders.reopen_failed = true;
  }
  return orders;
}

}  // namespace moonclient
