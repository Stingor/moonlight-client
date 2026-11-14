// LE COURRIER, comme modèle pur.
//
// 🔴 CE BANC GARDE LES VERDICTS QUI ÉVITENT DE PERDRE DES BIENS : un titre vide
// (pièces jointes détruites par le serveur), un titre trop long, un zeny que la
// bourse ne couvre pas avec ses frais, la case du zeny, un item porté.

#include "ro/sim/mailbox.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/inventory.h"

using ro::sim::InventoryItem;
using ro::sim::MailAttachFault;
using ro::sim::MailBox;
using ro::sim::Mailbox;
using ro::sim::MailClaim;
using ro::sim::MailContent;
using ro::sim::MailDraftStanding;
using ro::sim::MailHeader;
using ro::sim::MailNewsKind;
using ro::sim::MailSendFault;

namespace {

constexpr std::uint16_t kPotionSlot = 5;
constexpr std::int64_t kRichPurse = 1000000;

InventoryItem Potions(std::int16_t amount, std::uint16_t slot = kPotionSlot) {
  InventoryItem item;
  item.client_slot = slot;
  item.item_id = 501;
  item.amount = amount;
  item.identified = true;
  return item;
}

MailHeader Header(std::int64_t mail_id, MailBox box = MailBox::kNormal) {
  MailHeader header;
  header.mail_id = mail_id;
  header.box = box;
  header.title = "Salut";
  header.sender = "Bob";
  return header;
}

Mailbox Writing() {
  Mailbox mailbox;
  mailbox.TellWriteOpened(true, "Bob");
  REQUIRE(mailbox.writing());
  return mailbox;
}

// Un ajout accusé : la case, la quantité ajoutée par ce geste.
void Attached(Mailbox& mailbox, std::uint16_t slot, std::int16_t count) {
  mailbox.NoteAttachSent(Potions(count, slot), count, 0);
  mailbox.TellAttachResult(ro::sim::kMailAttachSucceeded, Potions(count, slot), count, 0);
}

}  // namespace

TEST_CASE("Courrier : la liste remplace, trie, garde le contenu des ids présents", "[sim][mail]") {
  Mailbox mailbox;
  mailbox.NoteOpened(1000);
  CHECK(mailbox.ListLoading(1000 + ro::sim::kMailListLoadingMs));
  CHECK_FALSE(mailbox.ListLoading(1001 + ro::sim::kMailListLoadingMs));

  mailbox.TellList({Header(3), Header(9), Header(5)});
  CHECK_FALSE(mailbox.ListLoading(1000));
  REQUIRE(mailbox.headers().size() == 3);
  CHECK(mailbox.headers()[0].mail_id == 9);
  CHECK(mailbox.headers()[2].mail_id == 3);
  CHECK(mailbox.UnreadCount() == 3);
  CHECK(mailbox.NewestIdIn(MailBox::kNormal) == 9);
  CHECK(mailbox.NewestIdIn(MailBox::kAccount) == 0);

  mailbox.TellContent(MailContent{.mail_id = 5, .body = "corps"});
  mailbox.TellContent(MailContent{.mail_id = 3, .body = "autre"});
  CHECK(mailbox.UnreadCount() == 1);

  // Le 3 a disparu, le 5 reste — et reste lu même si la liste dit le contraire.
  mailbox.TellList({Header(5), Header(9)});
  CHECK(mailbox.FindContent(3) == nullptr);
  REQUIRE(mailbox.FindContent(5) != nullptr);
  CHECK(mailbox.FindHeader(5)->read);
  CHECK(mailbox.UnreadCount() == 1);
}

TEST_CASE("Courrier : récupérations en vol parallèles, bits retirés au succès", "[sim][mail]") {
  Mailbox mailbox;
  MailHeader both = Header(7);
  both.has_items = true;
  both.has_zeny = true;
  MailHeader items = Header(8);
  items.has_items = true;
  mailbox.TellList({both, items, Header(9)});
  mailbox.TellContent(MailContent{.mail_id = 7, .zeny = 500, .items = {Potions(2)}});
  CHECK(mailbox.ClaimableIn(MailBox::kNormal) == std::vector<std::int64_t>{8, 7});

  mailbox.NoteClaimSent(7, MailClaim::kItems, 0);
  mailbox.NoteClaimSent(8, MailClaim::kItems, 0);
  mailbox.NoteClaimSent(7, MailClaim::kZeny, 0);
  CHECK(mailbox.ClaimableIn(MailBox::kNormal).empty());

  mailbox.TellClaimResult(8, MailClaim::kItems, ro::sim::kMailClaimSucceeded);
  CHECK(mailbox.ClaimInFlight(7, MailClaim::kItems));
  CHECK_FALSE(mailbox.FindHeader(8)->has_items);

  mailbox.TellClaimResult(7, MailClaim::kItems, ro::sim::kMailClaimSucceeded);
  CHECK(mailbox.FindContent(7)->items.empty());
  CHECK(mailbox.FindContent(7)->zeny == 500);
  CHECK(mailbox.FindHeader(7)->has_zeny);
  mailbox.TellClaimResult(7, MailClaim::kZeny, ro::sim::kMailClaimOverWeight);
  CHECK(mailbox.FindHeader(7)->has_zeny);
  CHECK(mailbox.news().back().kind == MailNewsKind::kClaimRefused);

  mailbox.TellDeleted(9);
  CHECK(mailbox.FindHeader(9) == nullptr);
}

TEST_CASE("Courrier : un ajout cumulé sur la même case, retiré par l'accusé", "[sim][mail]") {
  Mailbox mailbox = Writing();
  Attached(mailbox, kPotionSlot, 3);
  Attached(mailbox, kPotionSlot, 2);
  CHECK(mailbox.attachment_count() == 1);
  CHECK(mailbox.AttachedAmountAt(kPotionSlot) == 5);
  mailbox.TellDetached(kPotionSlot, 4, 0);
  CHECK(mailbox.AttachedAmountAt(kPotionSlot) == 1);
  mailbox.TellDetached(kPotionSlot, 1, 0);
  CHECK(mailbox.attachment_count() == 0);
}

TEST_CASE("Courrier : un envoi réussi rend l'annulation due, levée par l'annulation",
          "[sim][mail]") {
  Mailbox mailbox = Writing();
  Attached(mailbox, kPotionSlot, 1);
  mailbox.NoteSendSent(0);
  mailbox.TellSendResult(ro::sim::kMailSendSucceeded);
  // Le brouillon est fermé, mais le serveur tient encore l'écriture.
  CHECK_FALSE(mailbox.writing());
  CHECK(mailbox.cancel_due());
  CHECK(mailbox.draft_standing() == MailDraftStanding::kCancelDue);
  CHECK(mailbox.WithheldAt(kPotionSlot) == 0);

  mailbox.NoteWriteCancelled();
  CHECK_FALSE(mailbox.cancel_due());
  CHECK(mailbox.draft_standing() == MailDraftStanding::kUsable);

  // Un changement de carte lève aussi l'annulation due : le serveur a tout remis
  // à zéro.
  Mailbox moved = Writing();
  moved.NoteSendSent(0);
  moved.TellSendResult(ro::sim::kMailSendSucceeded);
  moved.CloseWriteForMapChange();
  CHECK_FALSE(moved.cancel_due());
}

TEST_CASE("Courrier : un refus d'envoi oublie les pièces jointes et attend la réouverture",
          "[sim][mail]") {
  Mailbox mailbox = Writing();
  Attached(mailbox, kPotionSlot, 3);
  Attached(mailbox, kPotionSlot + 1, 1);
  mailbox.NoteSendSent(0);
  mailbox.TellSendResult(ro::sim::kMailSendNoRecipient);
  CHECK(mailbox.writing());
  CHECK_FALSE(mailbox.send_in_flight());
  CHECK(mailbox.reopen_due());
  // Le fil ne dit pas où sont les pièces jointes : le sac montré ne retranche
  // plus rien, et ni ajout ni envoi ne partent.
  CHECK(mailbox.attachment_count() == 0);
  CHECK(mailbox.WithheldAt(kPotionSlot) == 0);
  CHECK(mailbox.CheckSend("Bob", "Salut", "", 0, kRichPurse) == MailSendFault::kReopenDue);
  CHECK(mailbox.CheckAttach(Potions(5), 1) == MailAttachFault::kReopenDue);
  CHECK_FALSE(mailbox.CanDetach(kPotionSlot, 1));
  // Les retraits que le serveur annonce ensuite ne visent plus rien.
  mailbox.TellDetached(kPotionSlot, 3, 0);
  CHECK(mailbox.attachment_count() == 0);

  // Annulation puis réouverture accordée : le brouillon resert.
  mailbox.NoteWriteCancelled();
  CHECK(mailbox.reopen_due());
  mailbox.TellWriteOpened(true, "Bob");
  CHECK_FALSE(mailbox.reopen_due());
  CHECK(mailbox.CheckSend("Bob", "Salut", "", 0, kRichPurse) == MailSendFault::kNone);
}

TEST_CASE("Courrier : un envoi sans réponse interdit de renvoyer avant la réouverture",
          "[sim][mail]") {
  Mailbox mailbox = Writing();
  Attached(mailbox, kPotionSlot, 2);
  mailbox.NoteSendSent(0);
  mailbox.ExpireSilent(ro::sim::kMailSendSilenceMs - 1);
  CHECK(mailbox.send_in_flight());
  mailbox.ExpireSilent(ro::sim::kMailSendSilenceMs);
  CHECK_FALSE(mailbox.send_in_flight());
  CHECK(mailbox.draft_standing() == MailDraftStanding::kOutcomeUnknown);
  REQUIRE_FALSE(mailbox.news().empty());
  CHECK(mailbox.news().back().kind == MailNewsKind::kSendResult);
  CHECK(mailbox.news().back().code == ro::sim::kMailSilent);
  // 🔴 Le premier envoi a pu partir : un second ne part pas tel quel.
  CHECK(mailbox.CheckSend("Bob", "Salut", "", 0, kRichPurse) == MailSendFault::kReopenDue);
  CHECK(mailbox.CheckAttach(Potions(5), 1) == MailAttachFault::kReopenDue);
  CHECK(mailbox.WithheldAt(kPotionSlot) == 0);

  // La réponse tardive se dit à part, sans toucher au brouillon.
  mailbox.TellSendResult(ro::sim::kMailSendSucceeded);
  CHECK(mailbox.news().back().kind == MailNewsKind::kSendResultLate);
  CHECK(mailbox.writing());
  CHECK_FALSE(mailbox.cancel_due());
  // Une seconde réponse sans envoi attendu ne dit plus rien.
  const std::uint32_t seen = mailbox.last_news_seq();
  mailbox.TellSendResult(ro::sim::kMailSendFailed);
  CHECK(mailbox.last_news_seq() == seen);

  mailbox.TellWriteOpened(true, "Bob");
  CHECK(mailbox.CheckSend("Bob", "Salut", "", 0, kRichPurse) == MailSendFault::kNone);
}

TEST_CASE("Courrier : un seul ajout en route, et l'échec lui est attribué", "[sim][mail]") {
  Mailbox mailbox = Writing();
  mailbox.NoteAttachSent(Potions(3), 3, 0);
  CHECK(mailbox.CheckAttach(Potions(4, kPotionSlot + 1), 1) == MailAttachFault::kAttachInFlight);
  CHECK(mailbox.WithheldAt(kPotionSlot) == 3);
  mailbox.TellAttachResult(ro::sim::kMailAttachOverWeight, InventoryItem(), 0, 0);
  REQUIRE_FALSE(mailbox.news().empty());
  CHECK(mailbox.news().back().kind == MailNewsKind::kAttachRefused);
  CHECK(mailbox.news().back().item.client_slot == kPotionSlot);
  CHECK(mailbox.CheckAttach(Potions(4, kPotionSlot + 1), 1) == MailAttachFault::kNone);

  // Le silence relâche aussi l'ajout, et libère le suivant.
  mailbox.NoteAttachSent(Potions(4, kPotionSlot + 1), 1, 0);
  mailbox.ExpireSilent(ro::sim::kMailAttachSilenceMs);
  CHECK(mailbox.pending_attaches().empty());
  CHECK(mailbox.CheckAttach(Potions(4, kPotionSlot + 1), 1) == MailAttachFault::kNone);
}

TEST_CASE("Courrier : aucun retrait pendant un envoi, ni hors d'une pièce jointe connue",
          "[sim][mail]") {
  Mailbox mailbox = Writing();
  Attached(mailbox, kPotionSlot, 2);
  CHECK(mailbox.CanDetach(kPotionSlot, 2));
  CHECK_FALSE(mailbox.CanDetach(kPotionSlot, 3));
  CHECK_FALSE(mailbox.CanDetach(kPotionSlot + 1, 1));
  mailbox.NoteSendSent(0);
  CHECK_FALSE(mailbox.CanDetach(kPotionSlot, 1));

  // Un retrait d'une case inconnue ne touche pas au poids joint.
  Mailbox weighed = Writing();
  weighed.NoteAttachSent(Potions(2), 2, 0);
  weighed.TellAttachResult(ro::sim::kMailAttachSucceeded, Potions(2), 2, 40);
  weighed.TellDetached(kPotionSlot + 3, 1, 0);
  CHECK(weighed.attached_weight() == 40);
  // Hors écriture, un retrait ne vise rien.
  Mailbox closed;
  closed.TellDetached(kPotionSlot, 1, 0);
  CHECK(closed.attached_weight() == 0);
}

TEST_CASE("Courrier : la vérification d'un nom vaut pour le nom interrogé", "[sim][mail]") {
  Mailbox mailbox = Writing();
  // Hors vérification en attente, une réponse est ignorée.
  mailbox.TellRecipient(77, 4, 99, "Bob");
  CHECK(mailbox.recipient().state == ro::sim::MailRecipientState::kUnchecked);

  mailbox.NoteRecipientCheckSent(0, "Bob");
  CHECK(mailbox.recipient().queried_wire == "Bob");
  mailbox.TellRecipient(77, 4, 99, "Bob");
  CHECK(mailbox.recipient().state == ro::sim::MailRecipientState::kFound);
  CHECK(mailbox.recipient().queried_wire == "Bob");
  CHECK(mailbox.recipient().char_id == 77);
  // Une seconde réponse, tardive, ne réécrit rien.
  mailbox.TellRecipient(0, 0, 0, "");
  CHECK(mailbox.recipient().state == ro::sim::MailRecipientState::kFound);
}

TEST_CASE("Courrier : les silences se relâchent", "[sim][mail]") {
  Mailbox mailbox = Writing();
  mailbox.NoteAttachSent(Potions(3), 3, 0);
  mailbox.NoteSendSent(0);
  mailbox.NoteRecipientCheckSent(0, "Bob");
  mailbox.ExpireSilent(ro::sim::kMailSendSilenceMs);
  CHECK(mailbox.pending_attaches().empty());
  CHECK_FALSE(mailbox.send_in_flight());
  CHECK(mailbox.recipient().state == ro::sim::MailRecipientState::kSilent);
}

TEST_CASE("Courrier : une lecture sans réponse se relâche et se redemande", "[sim][mail]") {
  Mailbox mailbox;
  mailbox.TellList({Header(4), Header(6)});
  mailbox.NoteReadSent(4, 0);
  CHECK(mailbox.ReadInFlight(4));
  mailbox.ExpireSilent(ro::sim::kMailReadSilenceMs - 1);
  CHECK(mailbox.ReadInFlight(4));
  mailbox.ExpireSilent(ro::sim::kMailReadSilenceMs);
  CHECK_FALSE(mailbox.ReadInFlight(4));
  CHECK(mailbox.ReadUnanswered(4));

  mailbox.NoteReadSent(4, 0);
  CHECK_FALSE(mailbox.ReadUnanswered(4));
  mailbox.TellContent(MailContent{.mail_id = 4, .body = "corps"});
  CHECK_FALSE(mailbox.ReadInFlight(4));
  CHECK_FALSE(mailbox.ReadUnanswered(4));

  // Un échec de « retour » non demandé clôt la lecture du même courrier.
  mailbox.NoteReadSent(6, 0);
  mailbox.TellReturnResult(6, 1);
  CHECK(mailbox.ReadUnanswered(6));
  CHECK(mailbox.news().empty());
}

TEST_CASE("Courrier : plusieurs retours attendent chacun leur réponse", "[sim][mail]") {
  Mailbox mailbox;
  mailbox.TellList({Header(11), Header(12), Header(13)});
  mailbox.NoteReturnSent(11);
  mailbox.NoteReturnSent(12);
  CHECK(mailbox.ReturnInFlight(11));
  CHECK(mailbox.ReturnInFlight(12));
  mailbox.TellReturnResult(12, 1);
  REQUIRE_FALSE(mailbox.news().empty());
  CHECK(mailbox.news().back().kind == MailNewsKind::kReturnRefused);
  CHECK(mailbox.news().back().mail_id == 12);
  mailbox.TellReturnResult(11, ro::sim::kMailReturnSucceeded);
  CHECK(mailbox.news().back().kind == MailNewsKind::kReturned);
  CHECK(mailbox.FindHeader(11) == nullptr);
  CHECK_FALSE(mailbox.ReturnInFlight(11));
  CHECK_FALSE(mailbox.ReturnInFlight(12));
}

TEST_CASE("Courrier : verdicts d'ajout", "[sim][mail]") {
  Mailbox closed;
  CHECK(closed.CheckAttach(Potions(5), 1) == MailAttachFault::kNotWriting);

  Mailbox mailbox = Writing();
  CHECK(mailbox.CheckAttach(Potions(5, 0), 1) == MailAttachFault::kBadSlot);
  InventoryItem worn = Potions(1);
  worn.worn_at = 2;
  CHECK(mailbox.CheckAttach(worn, 1) == MailAttachFault::kEquipped);
  InventoryItem rented = Potions(1);
  rented.rental_until_unix_s = 1;
  CHECK(mailbox.CheckAttach(rented, 1) == MailAttachFault::kRented);
  CHECK(mailbox.CheckAttach(Potions(5), 6) == MailAttachFault::kBadAmount);
  CHECK(mailbox.CheckAttach(Potions(5), 0) == MailAttachFault::kBadAmount);

  Attached(mailbox, kPotionSlot, 4);
  CHECK(mailbox.CheckAttach(Potions(5), 2) == MailAttachFault::kBadAmount);
  CHECK(mailbox.CheckAttach(Potions(5), 1) == MailAttachFault::kNone);
  Attached(mailbox, kPotionSlot, 1);
  CHECK(mailbox.CheckAttach(Potions(5), 1) == MailAttachFault::kAllAttached);

  for (std::uint16_t slot = kPotionSlot + 1; slot < kPotionSlot + 5; ++slot) {
    Attached(mailbox, slot, 1);
  }
  CHECK(mailbox.CheckAttach(Potions(5, kPotionSlot + 9), 1) == MailAttachFault::kAttachmentsFull);
}

TEST_CASE("Courrier : verdicts d'envoi", "[sim][mail]") {
  Mailbox mailbox = Writing();
  CHECK(mailbox.CheckSend("Bob", "Salut", "", 0, kRichPurse) == MailSendFault::kNone);
  CHECK(mailbox.CheckSend("", "Salut", "", 0, kRichPurse) == MailSendFault::kRecipientEmpty);
  CHECK(mailbox.CheckSend(std::string(24, 'a'), "Salut", "", 0, kRichPurse) ==
        MailSendFault::kRecipientTooLong);
  CHECK(mailbox.CheckSend("Bob", "", "", 0, kRichPurse) == MailSendFault::kTitleEmpty);
  CHECK(mailbox.CheckSend("Bob", std::string(39, 't'), "", 0, kRichPurse) == MailSendFault::kNone);
  CHECK(mailbox.CheckSend("Bob", std::string(40, 't'), "", 0, kRichPurse) ==
        MailSendFault::kTitleTooLong);
  CHECK(mailbox.CheckSend("Bob", "Salut", std::string(500, 'b'), 0, kRichPurse) ==
        MailSendFault::kBodyTooLong);
  CHECK(mailbox.CheckSend("Bob", std::string("\0a", 2), "", 0, kRichPurse) ==
        MailSendFault::kEmbeddedNul);
  CHECK(mailbox.CheckSend("Bob", "Salut", "", -1, kRichPurse) == MailSendFault::kZenyNegative);
  CHECK(mailbox.CheckSend("Bob", "Salut", "", ro::sim::kMailMostZeny + 1, kRichPurse) ==
        MailSendFault::kZenyAboveMost);
  CHECK(mailbox.CheckSend("Bob", "Salut", "", 0, std::nullopt) == MailSendFault::kPurseUnknown);

  // Deux pièces et 10 000 zeny : 5 000 + 200 de frais.
  Attached(mailbox, kPotionSlot, 1);
  Attached(mailbox, kPotionSlot + 1, 1);
  CHECK(ro::sim::MailFeeFor(10000, mailbox.attachment_count()) == 5200);
  CHECK(ro::sim::MailSendTotal(10000, mailbox.attachment_count()) == 15200);
  CHECK(mailbox.CheckSend("Bob", "Salut", "", 10000, 15200) == MailSendFault::kNone);
  CHECK(mailbox.CheckSend("Bob", "Salut", "", 10000, 15199) == MailSendFault::kPurseShort);

  mailbox.NoteAttachSent(Potions(1, kPotionSlot + 2), 1, 0);
  CHECK(mailbox.CheckSend("Bob", "Salut", "", 0, kRichPurse) == MailSendFault::kAttachInFlight);
}

TEST_CASE("Courrier : le changement de carte ferme l'écriture", "[sim][mail]") {
  Mailbox mailbox = Writing();
  Attached(mailbox, kPotionSlot, 2);
  mailbox.CloseWriteForMapChange();
  CHECK_FALSE(mailbox.writing());
  CHECK(mailbox.AttachedAmountAt(kPotionSlot) == 0);
}

TEST_CASE("Courrier : secondes restantes avant expiration", "[sim][mail]") {
  MailHeader header = Header(1);
  header.expires_at_unix_s = 1000;
  CHECK(ro::sim::MailSecondsLeft(header, 400) == 600);
  CHECK(ro::sim::MailSecondsLeft(header, 2000) == 0);
}

TEST_CASE("Courrier : l'ordre du serveur sur la boîte se prend une fois, le plus récent",
          "[sim][mail]") {
  Mailbox mailbox;
  CHECK_FALSE(mailbox.TakeWindowOrder().has_value());

  mailbox.TellWindowOrder(ro::sim::MailWindowOrder::kOpen);
  mailbox.TellWindowOrder(ro::sim::MailWindowOrder::kClose);
  CHECK(mailbox.TakeWindowOrder() == ro::sim::MailWindowOrder::kClose);
  CHECK_FALSE(mailbox.TakeWindowOrder().has_value());
  // L'ordre ne touche pas la boîte : c'est la boucle qui l'exécute.
  CHECK_FALSE(mailbox.open());
}
