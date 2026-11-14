// LE COURRIER VU DE L'ÉCRAN : le sac montré pendant l'écriture, et la suite
// que chaque réponse du serveur impose à la fenêtre d'écriture.
//
// 🔴 CE BANC TIENT DEUX RÈGLES QUE LE SERVEUR NE DIT PAS. Après un envoi réussi,
// il laisse l'écriture ouverte — le joueur reste figé — tant que l'annulation
// ne part pas. Après un échec, l'état des pièces jointes est ambigu : la
// session se refait, et la saisie du joueur doit y survivre.

#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/mailbox.h"

#include "game/mail_box_move.h"
#include "game/mail_write_flow.h"
#include "items/mail_withheld.h"

using moonclient::MailWriteFlow;
using moonclient::MailWriteOrders;
using moonclient::WithholdMailedCells;

namespace {

ro::ui::InventoryCellView Cell(std::uint16_t client_slot, int amount) {
  ro::ui::InventoryCellView cell;
  cell.client_slot = client_slot;
  cell.amount = amount;
  return cell;
}

// Une écriture accordée et montrée.
MailWriteFlow OpenFlow() {
  MailWriteFlow flow;
  flow.NoteAsked(0);
  const MailWriteOrders orders = flow.TellGranted();
  REQUIRE(orders.open_fresh);
  REQUIRE(flow.shown());
  return flow;
}

}  // namespace

TEST_CASE("Courrier : le sac montré retranche ce qui est joint ou en route", "[app][courrier]") {
  std::vector<ro::ui::InventoryCellView> cells = {Cell(2, 10), Cell(3, 1), Cell(4, 5)};
  WithholdMailedCells(cells, [](std::uint16_t slot) -> std::int32_t {
    switch (slot) {
      case 2:
        return 4;
      case 3:
        return 1;
      default:
        return 0;
    }
  });
  // La pile entamée reste, amputée ; celle dont tout est retenu disparaît ; la
  // case libre ne bouge pas.
  REQUIRE(cells.size() == 2);
  CHECK(cells[0].client_slot == 2);
  CHECK(cells[0].amount == 6);
  CHECK(cells[1].client_slot == 4);
  CHECK(cells[1].amount == 5);
}

TEST_CASE("Courrier : sans écriture, le sac montré est le sac", "[app][courrier]") {
  std::vector<ro::ui::InventoryCellView> cells = {Cell(2, 10), Cell(3, 1)};
  WithholdMailedCells(cells, {});
  REQUIRE(cells.size() == 2);
  CHECK(cells[0].amount == 10);
  CHECK(cells[1].amount == 1);
}

TEST_CASE("Courrier : un envoi réussi ferme la fenêtre ET annule l'écriture", "[app][courrier]") {
  MailWriteFlow flow = OpenFlow();
  const MailWriteOrders orders = flow.TellSendResult(ro::sim::kMailSendSucceeded, true, 0);
  CHECK(orders.close_window);
  CHECK(orders.emit_cancel);
  CHECK_FALSE(orders.emit_reopen);
  CHECK_FALSE(flow.shown());
}

TEST_CASE("Courrier : un échec d'envoi refait la session sans effacer la saisie",
          "[app][courrier]") {
  MailWriteFlow flow = OpenFlow();
  const MailWriteOrders failed = flow.TellSendResult(ro::sim::kMailSendNoRecipient, true, 0);
  CHECK(failed.emit_cancel);
  CHECK(failed.emit_reopen);
  CHECK_FALSE(failed.close_window);
  CHECK(flow.shown());
  CHECK(flow.reopening());

  // L'accord de la réouverture GARDE la saisie : rien ne la remet à zéro, et
  // les pièces jointes sont à refaire.
  const MailWriteOrders granted = flow.TellGranted();
  CHECK(granted.keep_typing);
  CHECK(granted.redo_attachments);
  CHECK_FALSE(granted.open_fresh);
  CHECK_FALSE(flow.reopening());
  CHECK(flow.shown());
}

TEST_CASE("Courrier : le silence sur l'envoi se traite comme un échec", "[app][courrier]") {
  MailWriteFlow flow = OpenFlow();
  const MailWriteOrders orders = flow.TellSendResult(ro::sim::kMailSilent, false, 0);
  CHECK(orders.emit_cancel);
  CHECK(orders.emit_reopen);
  // 🔴 Le courrier a pu partir : la réouverture remet le zeny à zéro et le dit.
  const MailWriteOrders granted = flow.TellGranted();
  CHECK(granted.keep_typing);
  CHECK(granted.outcome_unknown);
  CHECK_FALSE(granted.redo_attachments);
}

TEST_CASE("Courrier : un refus net ne remet pas le zeny à zéro", "[app][courrier]") {
  MailWriteFlow flow = OpenFlow();
  static_cast<void>(flow.TellSendResult(ro::sim::kMailSendFailed, false, 0));
  CHECK_FALSE(flow.TellGranted().outcome_unknown);
}

TEST_CASE("Courrier : une réouverture refusée ferme la fenêtre et le dit", "[app][courrier]") {
  MailWriteFlow flow = OpenFlow();
  static_cast<void>(flow.TellSendResult(ro::sim::kMailSendFailed, false, 0));
  const MailWriteOrders refused = flow.TellRefused();
  CHECK(refused.close_window);
  CHECK(refused.reopen_failed);
  CHECK_FALSE(flow.shown());
}

TEST_CASE("Courrier : une réouverture sans réponse se relâche", "[app][courrier]") {
  MailWriteFlow flow = OpenFlow();
  static_cast<void>(flow.TellSendResult(ro::sim::kMailSendFailed, false, 0));
  CHECK_FALSE(flow.Tick(moonclient::kMailWriteAskSilenceMs - 1).close_window);
  const MailWriteOrders silent = flow.Tick(moonclient::kMailWriteAskSilenceMs);
  CHECK(silent.close_window);
  CHECK(silent.reopen_failed);
  // L'accord tardif n'ouvre plus rien : il s'annule.
  const MailWriteOrders late = flow.TellGranted();
  CHECK(late.emit_cancel);
  CHECK_FALSE(late.open_fresh);
  CHECK_FALSE(flow.shown());
}

TEST_CASE("Courrier : fermer pendant la réouverture annule l'accord à venir", "[app][courrier]") {
  MailWriteFlow flow = OpenFlow();
  static_cast<void>(flow.TellSendResult(ro::sim::kMailSendFailed, false, 0));
  const MailWriteOrders closed = flow.NoteClosedByPlayer();
  CHECK(closed.emit_cancel);
  CHECK(closed.close_window);
  const MailWriteOrders late = flow.TellGranted();
  CHECK(late.emit_cancel);
  CHECK_FALSE(late.open_fresh);
  CHECK_FALSE(late.keep_typing);
}

TEST_CASE("Courrier : le changement de carte ferme sans rien émettre", "[app][courrier]") {
  MailWriteFlow flow = OpenFlow();
  const MailWriteOrders dropped = flow.NoteServerDropped();
  CHECK(dropped.close_window);
  CHECK_FALSE(dropped.emit_cancel);
  CHECK_FALSE(flow.shown());
}

TEST_CASE("Courrier : un résultat d'envoi sans fenêtre ne commande rien", "[app][courrier]") {
  MailWriteFlow flow;
  const MailWriteOrders orders = flow.TellSendResult(ro::sim::kMailSendFailed, true, 0);
  CHECK_FALSE(orders.emit_cancel);
  CHECK_FALSE(orders.emit_reopen);
}

TEST_CASE("Courrier : l'ordre du serveur ouvre la boîte comme l'icône du menu", "[app][courrier]") {
  using moonclient::MailBoxMove;
  using moonclient::MailBoxMoveFor;
  using ro::sim::MailWindowOrder;

  // L'icône bascule.
  CHECK(MailBoxMoveFor(false, false, std::nullopt) == MailBoxMove::kNone);
  CHECK(MailBoxMoveFor(false, true, std::nullopt) == MailBoxMove::kOpen);
  CHECK(MailBoxMoveFor(true, true, std::nullopt) == MailBoxMove::kClose);

  // L'ordre dit l'état voulu : ouvrir une boîte ouverte ne redemande rien.
  CHECK(MailBoxMoveFor(false, false, MailWindowOrder::kOpen) == MailBoxMove::kOpen);
  CHECK(MailBoxMoveFor(true, false, MailWindowOrder::kOpen) == MailBoxMove::kNone);
  CHECK(MailBoxMoveFor(true, false, MailWindowOrder::kClose) == MailBoxMove::kClose);
  CHECK(MailBoxMoveFor(false, false, MailWindowOrder::kClose) == MailBoxMove::kNone);

  // Il l'emporte sur la bascule de la même image.
  CHECK(MailBoxMoveFor(true, true, MailWindowOrder::kOpen) == MailBoxMove::kNone);
  CHECK(MailBoxMoveFor(false, true, MailWindowOrder::kClose) == MailBoxMove::kNone);
}
