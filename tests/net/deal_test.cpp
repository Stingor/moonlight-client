// L'ÉCHANGE ENTRE JOUEURS, décodé — et ses demandes, sur le fil.
//
// 🔴 CE BANC PORTE CE QU'AUCUNE SESSION NE MONTRERAIT VITE :
//
//   · l'accusé d'un dépôt retire l'item du sac DANS LA MÊME TRAME, et le renvoi
//     d'une annulation le rend sans le doubler ;
//   · un identifiant nul dans un dépôt du partenaire est du zeny ;
//   · un verbe refuse sans rien envoyer ce que le serveur tiendrait pour de la
//     triche — un item porté, un zeny au-delà de la bourse.

#include "ro/net/deal.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/inventory.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/deal.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"

using ro::net::ApplyDealFrame;
using ro::net::FrameOutcome;
using ro::sim::Deal;
using ro::sim::Inventory;
using ro::sim::InventoryItem;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kPartnerAid = 2000002;
constexpr std::uint16_t kPotionSlot = 7;

InventoryItem Potions(std::int16_t amount) {
  InventoryItem item;
  item.client_slot = kPotionSlot;
  item.item_id = 501;
  item.amount = amount;
  item.identified = true;
  return item;
}

void Open(Deal& deal, Inventory& bag) {
  ro::proto::ZcReqExchangeItem asked;
  asked.requesterName = "Bob";
  asked.targetId = kPartnerAid;
  asked.targetLv = 99;
  REQUIRE(ApplyDealFrame(FrameOf(ro::proto::Encode(asked)), deal, bag) == FrameOutcome::kApplied);
  REQUIRE(deal.phase() == ro::sim::DealPhase::kProposed);
  CHECK(deal.partner().name == "Bob");
  CHECK(deal.partner().account_id == kPartnerAid);

  ro::proto::ZcAckExchangeItem answered;
  answered.result = ro::sim::kDealAnswerAccepted;
  answered.targetId = kPartnerAid;
  answered.targetLv = 99;
  REQUIRE(ApplyDealFrame(FrameOf(ro::proto::Encode(answered)), deal, bag) ==
          FrameOutcome::kApplied);
  REQUIRE(deal.open());
}

}  // namespace

TEST_CASE("Échange : l'accusé d'un dépôt retire du sac, l'annulation le rend sans doubler",
          "[net][deal]") {
  Deal deal;
  Inventory bag;
  bag.Put(Potions(10));
  Open(deal, bag);

  deal.NoteAddSent(*bag.Find(kPotionSlot), 4, 0);
  ro::proto::ZcAckAddExchangeItem ack;
  ack.index = kPotionSlot;
  ack.result = ro::sim::kDealAddSucceeded;
  REQUIRE(ApplyDealFrame(FrameOf(ro::proto::Encode(ack)), deal, bag) == FrameOutcome::kApplied);
  REQUIRE(bag.Find(kPotionSlot) != nullptr);
  CHECK(bag.Find(kPotionSlot)->amount == 6);
  REQUIRE(deal.my_offer().size() == 1);
  CHECK(deal.my_offer()[0].item.amount == 4);

  REQUIRE(ApplyDealFrame(FrameOf(ro::proto::Encode(ro::proto::ZcCancelExchangeItem{})), deal,
                         bag) == FrameOutcome::kApplied);
  CHECK_FALSE(deal.open());
  CHECK(deal.news().back().kind == ro::sim::DealNewsKind::kCancelled);
}

TEST_CASE("Échange : un dépôt du partenaire d'identifiant nul est du zeny", "[net][deal]") {
  Deal deal;
  Inventory bag;
  Open(deal, bag);

  ro::proto::ZcAddExchangeItem zeny;
  zeny.amount = 1'500'000;
  REQUIRE(ApplyDealFrame(FrameOf(ro::proto::Encode(zeny)), deal, bag) == FrameOutcome::kApplied);
  CHECK(deal.partner_zeny() == 1'500'000);
  CHECK(deal.partner_offer().empty());

  ro::proto::ZcAddExchangeItem card;
  card.itemId = 4001;
  card.itemType = 6;
  card.amount = 1;
  card.identified = 1;
  card.refine = 0;
  card.slot.card = {0, 0, 0, 0};
  card.option_data[0].index = 25;
  card.option_data[0].value = 3;
  REQUIRE(ApplyDealFrame(FrameOf(ro::proto::Encode(card)), deal, bag) == FrameOutcome::kApplied);
  REQUIRE(deal.partner_offer().size() == 1);
  CHECK(deal.partner_offer()[0].item.item_id == 4001);
  CHECK(deal.partner_offer()[0].item.options[0].id == 25);
  CHECK(deal.partner_offer()[0].item.identified);
}

TEST_CASE("Échange : les verrous, et un changement de carte qui ferme sans consommer",
          "[net][deal]") {
  Deal deal;
  Inventory bag;
  Open(deal, bag);

  ro::proto::ZcConcludeExchangeItem locked;
  locked.who = 1;
  REQUIRE(ApplyDealFrame(FrameOf(ro::proto::Encode(locked)), deal, bag) == FrameOutcome::kApplied);
  CHECK(deal.partner_locked());
  CHECK_FALSE(deal.my_locked());
  locked.who = 0;
  REQUIRE(ApplyDealFrame(FrameOf(ro::proto::Encode(locked)), deal, bag) == FrameOutcome::kApplied);
  CHECK(deal.my_locked());

  ro::proto::ZcNpcackMapmove moved;
  moved.mapName = "prontera.gat";
  CHECK(ApplyDealFrame(FrameOf(ro::proto::Encode(moved)), deal, bag) == FrameOutcome::kUnhandled);
  CHECK_FALSE(deal.open());
}

TEST_CASE("Échange : les demandes sur le fil, et ce qui ne part pas", "[net][deal]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  Deal deal;
  Inventory bag;
  bag.Put(Potions(10));

  REQUIRE(ro::net::ProposeDeal(link, deal,
                               ro::sim::DealPartner{.account_id = kPartnerAid, .name = "Bob"}, 0)
              .has_value());
  CHECK(server.Receive(6) == std::vector<std::uint8_t>{0xE4, 0x00, 0x82, 0x84, 0x1E, 0x00});
  CHECK(deal.phase() == ro::sim::DealPhase::kProposing);

  ro::proto::ZcAckExchangeItem answered;
  answered.result = ro::sim::kDealAnswerAccepted;
  answered.targetId = kPartnerAid;
  REQUIRE(ApplyDealFrame(FrameOf(ro::proto::Encode(answered)), deal, bag) ==
          FrameOutcome::kApplied);
  REQUIRE(deal.open());

  // Un item porté ne part pas : le serveur bannirait.
  InventoryItem worn = Potions(1);
  worn.client_slot = 9;
  worn.worn_at = 0x10;
  CHECK_FALSE(ro::net::OfferDealItem(link, deal, worn, 1, 0).has_value());
  // Au-delà de la bourse non plus : le serveur annulerait.
  CHECK_FALSE(ro::net::OfferDealZeny(link, deal, 1001, 1000).has_value());

  REQUIRE(ro::net::OfferDealItem(link, deal, *bag.Find(kPotionSlot), 3, 0).has_value());
  CHECK(server.Receive(8) ==
        std::vector<std::uint8_t>{0xE8, 0x00, kPotionSlot, 0x00, 0x03, 0x00, 0x00, 0x00});
  // Un dépôt en route retient le verrou.
  CHECK_FALSE(ro::net::LockDeal(link, deal).has_value());

  REQUIRE(ro::net::OfferDealZeny(link, deal, 1000, 1000).has_value());
  CHECK(server.Receive(8) ==
        std::vector<std::uint8_t>{0xE8, 0x00, 0x00, 0x00, 0xE8, 0x03, 0x00, 0x00});

  ro::proto::ZcAckAddExchangeItem ack;
  ack.index = kPotionSlot;
  REQUIRE(ApplyDealFrame(FrameOf(ro::proto::Encode(ack)), deal, bag) == FrameOutcome::kApplied);
  REQUIRE(ro::net::LockDeal(link, deal).has_value());
  CHECK(server.Receive(2) == std::vector<std::uint8_t>{0xEB, 0x00});

  // La validation attend le verrou du partenaire.
  CHECK_FALSE(ro::net::ExecuteDeal(link, deal, deal.partner_revision(), 1000).has_value());
  deal.TellLocked(false);
  deal.TellLocked(true);
  // Une bourse qui a fondu retient la validation.
  CHECK_FALSE(ro::net::ExecuteDeal(link, deal, deal.partner_revision(), 999).has_value());
  REQUIRE(ro::net::ExecuteDeal(link, deal, deal.partner_revision(), 1000).has_value());
  CHECK(server.Receive(2) == std::vector<std::uint8_t>{0xEF, 0x00});

  REQUIRE(ro::net::CancelDeal(link, deal).has_value());
  CHECK(server.Receive(2) == std::vector<std::uint8_t>{0xED, 0x00});
  CHECK_FALSE(deal.open());
}

TEST_CASE("Échange : le dernier rempart retient ce que le serveur tiendrait pour de la triche",
          "[net][deal]") {
  using ro::net::DealFrameRefusal;
  using ro::net::DealRefusesFrame;
  Deal deal;
  Inventory bag;
  bag.Put(Potions(10));

  ro::proto::CzReqWearEquip wear;
  wear.index = kPotionSlot;
  const std::vector<std::uint8_t> wear_frame = ro::proto::Encode(wear);
  const std::vector<std::uint8_t> skill_frame = {0x38, 0x04, 0x05, 0x00, 0x1C,
                                                 0x00, 0x01, 0x00, 0x00, 0x00};
  ro::proto::CzPcPurchaseItemlistFrommc2 purchase;
  const std::vector<std::uint8_t> purchase_frame = ro::proto::Encode(purchase);

  // Sans échange, tout part.
  CHECK(DealRefusesFrame(deal, wear_frame) == DealFrameRefusal::kNone);
  CHECK(DealRefusesFrame(deal, skill_frame) == DealFrameRefusal::kNone);

  Open(deal, bag);
  // Échange ouvert, rien en jeu : compétences et achats partent, aucun geste
  // d'item — le client d'origine les refuse tous.
  CHECK(DealRefusesFrame(deal, wear_frame) == DealFrameRefusal::kItemGesture);
  CHECK(DealRefusesFrame(deal, skill_frame) == DealFrameRefusal::kNone);
  CHECK(DealRefusesFrame(deal, purchase_frame) == DealFrameRefusal::kNone);

  deal.NoteAddSent(*bag.Find(kPotionSlot), 2, 0);
  CHECK(DealRefusesFrame(deal, skill_frame) == DealFrameRefusal::kSkillWithStake);
  CHECK(DealRefusesFrame(deal, purchase_frame) == DealFrameRefusal::kNone);

  deal.NoteZenySent(10);
  CHECK(DealRefusesFrame(deal, purchase_frame) == DealFrameRefusal::kZenyWithStake);
}

TEST_CASE("Échange : une proposition reçue s'annule par un refus", "[net][deal]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  Deal deal;
  deal.TellProposal("Bob", kPartnerAid, 99);
  REQUIRE(ro::net::CancelDeal(link, deal).has_value());
  CHECK(server.Receive(3) == std::vector<std::uint8_t>{0xE6, 0x00, 0x04});
  CHECK(deal.phase() == ro::sim::DealPhase::kNone);
}

// ── LES DÉPÔTS RENDUS ───────────────────────────────────────────────────────
//
// 🔴 LE SERVEUR REND UN DÉPÔT PAR LE PAQUET D'UN GAIN, AVANT d'annoncer
// l'annulation. Ce banc rejoue l'ordre de la boucle de jeu — le monde range,
// puis l'échange lit — et vérifie ce que le bandeau et la conversation liront.

namespace {

using ro::sim::DealNewsKind;
using ro::sim::ObtainedVerdict;
using ro::sim::World;

constexpr std::uint32_t kReturnedMs = 5000;
constexpr std::int16_t kDeposited = 4;
constexpr std::uint32_t kPotionId = 501;
constexpr std::uint32_t kOtherItemId = 502;
constexpr std::uint16_t kOtherSlot = 12;

// Le dépôt de quatre potions, accusé : le sac en garde six.
void DepositPotions(Deal& deal, Inventory& bag) {
  deal.NoteAddSent(*bag.Find(kPotionSlot), kDeposited, 0);
  ro::proto::ZcAckAddExchangeItem ack;
  ack.index = kPotionSlot;
  ack.result = ro::sim::kDealAddSucceeded;
  REQUIRE(ApplyDealFrame(FrameOf(ro::proto::Encode(ack)), deal, bag) == FrameOutcome::kApplied);
}

// Un gain, ou un dépôt rendu : le même paquet.
void Receive(World& world, Deal& deal, std::uint16_t slot, std::uint32_t item_id,
             std::int16_t count, std::uint32_t now_ms) {
  ro::proto::ZcItemPickupAck picked;
  picked.Index = slot;
  picked.count = static_cast<std::uint16_t>(count);
  picked.nameid = item_id;
  picked.IsIdentified = 1;
  const ro::net::Frame frame = FrameOf(ro::proto::Encode(picked));
  REQUIRE(ro::net::ApplyInventoryFrame(frame, world, now_ms) == FrameOutcome::kApplied);
  // LU SANS ÊTRE PRIS : le gain appartient au monde.
  CHECK(ApplyDealFrame(frame, deal, world.inventory()) == FrameOutcome::kUnhandled);
}

void Cancelled(Deal& deal, Inventory& bag) {
  REQUIRE(ApplyDealFrame(FrameOf(ro::proto::Encode(ro::proto::ZcCancelExchangeItem{})), deal,
                         bag) == FrameOutcome::kApplied);
}

std::size_t CountNews(const Deal& deal, DealNewsKind kind) {
  return static_cast<std::size_t>(std::ranges::count_if(
      deal.news(), [kind](const ro::sim::DealNews& news) { return news.kind == kind; }));
}

ObtainedVerdict LastVerdict(const Inventory& bag, std::uint32_t now_ms) {
  REQUIRE_FALSE(bag.obtained().empty());
  return ro::sim::VerdictOf(bag.obtained().back(), now_ms);
}

}  // namespace

TEST_CASE("Échange : le partenaire annule, mon dépôt revient sans s'annoncer comme un gain",
          "[net][deal]") {
  World world;
  Deal deal;
  Inventory& bag = world.inventory();
  bag.Put(Potions(10));
  Open(deal, bag);
  DepositPotions(deal, bag);

  Receive(world, deal, kPotionSlot, kPotionId, kDeposited, kReturnedMs);
  // Avant l'annulation, ce n'est qu'un soupçon : l'annonce attend.
  CHECK(LastVerdict(bag, kReturnedMs) == ObtainedVerdict::kUndecided);
  CHECK(ro::sim::AwaitsTradeVerdict(bag, 0, kReturnedMs));

  Cancelled(deal, bag);
  CHECK(LastVerdict(bag, kReturnedMs) == ObtainedVerdict::kTradeReturn);
  CHECK_FALSE(ro::sim::AwaitsTradeVerdict(bag, 0, kReturnedMs));
  REQUIRE(bag.Find(kPotionSlot) != nullptr);
  CHECK(bag.Find(kPotionSlot)->amount == 10);
  // Une seule ligne pour le retour, après celle de l'annulation.
  CHECK(CountNews(deal, DealNewsKind::kStakeReturned) == 1);
  CHECK(deal.news().back().kind == DealNewsKind::kStakeReturned);
  CHECK(CountNews(deal, DealNewsKind::kCancelled) == 1);
}

TEST_CASE("Échange : j'annule, et le dépôt revient après la fermeture du modèle", "[net][deal]") {
  World world;
  Deal deal;
  Inventory& bag = world.inventory();
  bag.Put(Potions(10));
  Open(deal, bag);
  DepositPotions(deal, bag);

  deal.NoteCancelSent();
  REQUIRE_FALSE(deal.open());
  Receive(world, deal, kPotionSlot, kPotionId, kDeposited, kReturnedMs);
  CHECK(LastVerdict(bag, kReturnedMs) == ObtainedVerdict::kUndecided);

  Cancelled(deal, bag);
  CHECK(LastVerdict(bag, kReturnedMs) == ObtainedVerdict::kTradeReturn);
  CHECK(CountNews(deal, DealNewsKind::kStakeReturned) == 1);
}

TEST_CASE("Échange : un vrai gain pendant l'annulation reste un gain", "[net][deal]") {
  World world;
  Deal deal;
  Inventory& bag = world.inventory();
  bag.Put(Potions(10));
  Open(deal, bag);
  DepositPotions(deal, bag);

  // Un autre item : il ne ressemble à aucun dépôt, il s'annonce aussitôt.
  Receive(world, deal, kOtherSlot, kOtherItemId, 1, kReturnedMs);
  CHECK(LastVerdict(bag, kReturnedMs) == ObtainedVerdict::kGain);
  const std::uint64_t other_seq = bag.last_obtained_seq();

  Receive(world, deal, kPotionSlot, kPotionId, kDeposited, kReturnedMs);
  // Le même dépôt une seconde fois : un dépôt ne revient qu'une fois.
  Receive(world, deal, kPotionSlot, kPotionId, kDeposited, kReturnedMs);
  CHECK(LastVerdict(bag, kReturnedMs) == ObtainedVerdict::kGain);

  Cancelled(deal, bag);
  std::size_t returned = 0;
  std::size_t gained = 0;
  for (const ro::sim::ObtainedItem& entry : bag.obtained()) {
    const ObtainedVerdict verdict = ro::sim::VerdictOf(entry, kReturnedMs);
    if (verdict == ObtainedVerdict::kTradeReturn) {
      ++returned;
    }
    if (verdict == ObtainedVerdict::kGain) {
      ++gained;
    }
    if (entry.seq == other_seq) {
      CHECK(verdict == ObtainedVerdict::kGain);
    }
  }
  CHECK(returned == 1);
  CHECK(gained == 2);
}

TEST_CASE("Échange : sans annulation, le soupçon redevient un gain au bout du délai",
          "[net][deal]") {
  World world;
  Deal deal;
  Inventory& bag = world.inventory();
  bag.Put(Potions(10));
  Open(deal, bag);
  DepositPotions(deal, bag);

  Receive(world, deal, kPotionSlot, kPotionId, kDeposited, kReturnedMs);
  const std::uint32_t just_before = kReturnedMs + ro::sim::kTradeReturnVerdictMs - 1;
  const std::uint32_t expired = kReturnedMs + ro::sim::kTradeReturnVerdictMs;
  CHECK(LastVerdict(bag, just_before) == ObtainedVerdict::kUndecided);
  CHECK(LastVerdict(bag, expired) == ObtainedVerdict::kGain);
  CHECK_FALSE(ro::sim::AwaitsTradeVerdict(bag, 0, expired));
}

TEST_CASE("Échange : conclu, ce qui ressemblait à un dépôt est un gain", "[net][deal]") {
  World world;
  Deal deal;
  Inventory& bag = world.inventory();
  bag.Put(Potions(10));
  Open(deal, bag);
  DepositPotions(deal, bag);

  Receive(world, deal, kPotionSlot, kPotionId, kDeposited, kReturnedMs);
  REQUIRE(ApplyDealFrame(FrameOf(ro::proto::Encode(ro::proto::ZcExecExchangeItem{})), deal, bag) ==
          FrameOutcome::kApplied);
  CHECK(LastVerdict(bag, kReturnedMs) == ObtainedVerdict::kGain);
  CHECK(deal.news().back().kind == DealNewsKind::kExecuted);
  CHECK(CountNews(deal, DealNewsKind::kStakeReturned) == 0);
}

TEST_CASE("Échange : le zeny offert se dit rendu ; rien en jeu, rien à dire", "[net][deal]") {
  Deal deal;
  Inventory bag;
  Open(deal, bag);
  Cancelled(deal, bag);
  CHECK(CountNews(deal, DealNewsKind::kStakeReturned) == 0);

  Open(deal, bag);
  deal.NoteZenySent(1000);
  Cancelled(deal, bag);
  CHECK(CountNews(deal, DealNewsKind::kStakeReturned) == 1);
}
