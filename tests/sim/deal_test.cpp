// L'ÉCHANGE ENTRE JOUEURS, comme modèle pur.
//
// 🔴 CE BANC GARDE LES VERDICTS QUI ÉVITENT LE BANNISSEMENT : un item porté, une
// pile offerte qui fondrait, un zeny offert au-delà de la bourse ne doivent
// jamais pouvoir partir — le serveur les tiendrait pour de la triche.

#include "ro/sim/deal.h"

#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/inventory.h"
#include "ro/sim/npc_shop_cart.h"
#include "ro/sim/pet_egg.h"
#include "ro/sim/player_status.h"

using ro::sim::Deal;
using ro::sim::DealAddFault;
using ro::sim::DealExecuteFault;
using ro::sim::DealNewsKind;
using ro::sim::DealPartner;
using ro::sim::DealPhase;
using ro::sim::DealTaken;
using ro::sim::DealZenyFault;
using ro::sim::InventoryItem;

namespace {

constexpr std::uint32_t kPartnerAid = 2000002;
constexpr std::uint16_t kPotionSlot = 5;

InventoryItem Potions(std::int16_t amount, std::uint16_t slot = kPotionSlot) {
  InventoryItem item;
  item.client_slot = slot;
  item.item_id = 501;
  item.type = 0;
  item.amount = amount;
  item.identified = true;
  return item;
}

InventoryItem Sword(std::uint16_t slot) {
  InventoryItem item;
  item.client_slot = slot;
  item.item_id = 1101;
  item.type = ro::sim::kItemTypeWeapon;
  item.amount = 1;
  item.identified = true;
  return item;
}

Deal OpenDeal() {
  Deal deal;
  deal.NoteProposalSent(DealPartner{.account_id = kPartnerAid, .name = "Bob"}, 0);
  deal.TellAnswer(ro::sim::kDealAnswerAccepted, kPartnerAid, 42);
  REQUIRE(deal.open());
  return deal;
}

}  // namespace

TEST_CASE("Échange : une proposition acceptée ouvre, un refus se dit", "[sim][deal]") {
  Deal deal;
  deal.NoteProposalSent(DealPartner{.account_id = kPartnerAid, .name = "Bob"}, 100);
  CHECK(deal.phase() == DealPhase::kProposing);
  deal.TellAnswer(ro::sim::kDealAnswerTooFar, kPartnerAid, 0);
  CHECK(deal.phase() == DealPhase::kNone);
  REQUIRE_FALSE(deal.news().empty());
  CHECK(deal.news().back().kind == DealNewsKind::kAnswered);
  CHECK(deal.news().back().code == ro::sim::kDealAnswerTooFar);

  deal.TellProposal("Carol", 2000003, 77);
  CHECK(deal.phase() == DealPhase::kProposed);
  CHECK(deal.partner().name == "Carol");
  const std::uint32_t before = deal.opening_seq();
  deal.NoteAnswerSent(true);
  deal.TellAnswer(ro::sim::kDealAnswerAccepted, 2000003, 77);
  CHECK(deal.open());
  CHECK(deal.opening_seq() == before + 1);
  CHECK(deal.partner().name == "Carol");
  CHECK(deal.partner().base_level == 77);
}

TEST_CASE("Échange : mon propre refus ne se redit pas", "[sim][deal]") {
  Deal deal;
  deal.TellProposal("Carol", 2000003, 77);
  deal.NoteAnswerSent(false);
  CHECK(deal.phase() == DealPhase::kNone);
  const std::uint32_t said = deal.last_news_seq();
  deal.TellAnswer(ro::sim::kDealAnswerCancelled, 0, 0);
  CHECK(deal.last_news_seq() == said);
}

TEST_CASE("Échange : ce que le serveur bannirait ne part pas", "[sim][deal]") {
  Deal deal = OpenDeal();
  InventoryItem worn = Sword(9);
  worn.worn_at = 0x02;
  CHECK(deal.CheckAdd(worn, 1) == DealAddFault::kEquipped);

  InventoryItem rented = Sword(10);
  rented.rental_until_unix_s = 1'800'000'000;
  CHECK(deal.CheckAdd(rented, 1) == DealAddFault::kRented);

  InventoryItem egg;
  egg.client_slot = 11;
  egg.item_id = ro::sim::kPetEggFirstId;
  egg.amount = 1;
  egg.damaged = true;
  CHECK(deal.CheckAdd(egg, 1) == DealAddFault::kHatchedEgg);

  CHECK(deal.CheckAdd(Potions(10), 11) == DealAddFault::kBadAmount);
  CHECK(deal.CheckAdd(Potions(10), 0) == DealAddFault::kBadAmount);
  CHECK(deal.CheckAdd(Potions(10), 10) == DealAddFault::kNone);
}

TEST_CASE("Échange : un dépôt en route borne le suivant, l'accusé retire du sac", "[sim][deal]") {
  Deal deal = OpenDeal();
  deal.NoteAddSent(Potions(10), 6, 0);
  CHECK(deal.InFlightOf(kPotionSlot) == 6);
  CHECK(deal.MostAddable(Potions(10)) == 4);
  CHECK(deal.CheckAdd(Potions(10), 5) == DealAddFault::kBadAmount);
  CHECK(ro::sim::DealEngages(deal, kPotionSlot));

  const std::optional<DealTaken> taken = deal.TellAddResult(kPotionSlot, 0);
  REQUIRE(taken.has_value());
  CHECK(taken->client_slot == kPotionSlot);
  CHECK(taken->amount == 6);
  REQUIRE(deal.my_offer().size() == 1);
  CHECK(deal.my_offer()[0].item.amount == 6);
  CHECK(deal.InFlightOf(kPotionSlot) == 0);

  // Le sac montre désormais quatre : le reste peut partir, et se fond dans la
  // même ligne.
  deal.NoteAddSent(Potions(4), 4, 10);
  CHECK(deal.MostAddable(Potions(4)) == 0);
  CHECK(deal.CheckAdd(Potions(4), 1) == DealAddFault::kAllOffered);
  REQUIRE(deal.TellAddResult(kPotionSlot, 0).has_value());
  REQUIRE(deal.my_offer().size() == 1);
  CHECK(deal.my_offer()[0].item.amount == 10);
}

TEST_CASE("Échange : un dépôt refusé se dit et ne retire rien", "[sim][deal]") {
  Deal deal = OpenDeal();
  deal.NoteAddSent(Potions(10), 3, 0);
  CHECK_FALSE(deal.TellAddResult(kPotionSlot, ro::sim::kDealAddOverWeight).has_value());
  CHECK(deal.my_offer().empty());
  CHECK(deal.pending_adds().empty());
  REQUIRE_FALSE(deal.news().empty());
  CHECK(deal.news().back().kind == DealNewsKind::kAddRefused);
  CHECK(deal.news().back().code == ro::sim::kDealAddOverWeight);
  CHECK(deal.news().back().item.item_id == 501);
}

TEST_CASE("Échange : l'accusé du verrou, case zéro, ne vise aucun dépôt", "[sim][deal]") {
  Deal deal = OpenDeal();
  deal.NoteAddSent(Potions(10), 3, 0);
  CHECK_FALSE(deal.TellAddResult(ro::sim::kDealZenySlot, 0).has_value());
  CHECK(deal.pending_adds().size() == 1);
}

TEST_CASE("Échange : un dépôt muet se relâche, un accusé tardif retire encore", "[sim][deal]") {
  Deal deal = OpenDeal();
  deal.NoteAddSent(Potions(10), 2, 1000);
  CHECK(deal.ExpireSilentAdds(1000 + ro::sim::kDealAddSilenceMs - 1) == 0);
  CHECK(deal.ExpireSilentAdds(1000 + ro::sim::kDealAddSilenceMs) == 1);
  CHECK(deal.pending_adds().empty());
  CHECK(deal.news().back().code == ro::sim::kDealAddSilent);
  // Toujours engagée : l'accusé peut encore venir.
  CHECK(deal.Engages(kPotionSlot));
  const std::optional<DealTaken> late = deal.TellAddResult(kPotionSlot, 0);
  REQUIRE(late.has_value());
  CHECK(late->amount == 2);
}

TEST_CASE("Échange : dix items distincts, pas un de plus", "[sim][deal]") {
  Deal deal = OpenDeal();
  for (std::uint16_t slot = 2; slot < 2 + ro::sim::kDealMostItems; ++slot) {
    REQUIRE(deal.CheckAdd(Sword(slot), 1) == DealAddFault::kNone);
    deal.NoteAddSent(Sword(slot), 1, 0);
  }
  CHECK(deal.CheckAdd(Sword(40), 1) == DealAddFault::kOfferFull);
  // Une case déjà engagée n'en ajoute pas une onzième.
  CHECK(deal.CheckAdd(Potions(10, 2), 1) == DealAddFault::kNone);
}

TEST_CASE("Échange : rien ne s'ajoute après le verrou", "[sim][deal]") {
  Deal deal = OpenDeal();
  deal.NoteLockSent();
  CHECK(deal.CheckAdd(Potions(10), 1) == DealAddFault::kLocked);
  CHECK(deal.CheckZeny(10, 100) == DealZenyFault::kLocked);
}

TEST_CASE("Échange : le zeny se borne à la bourse et au plafond", "[sim][deal]") {
  Deal deal = OpenDeal();
  CHECK(deal.CheckZeny(-1, 100) == DealZenyFault::kNegative);
  CHECK(deal.CheckZeny(101, 100) == DealZenyFault::kAbovePurse);
  CHECK(deal.CheckZeny(1, ro::sim::kUnknownStat) == DealZenyFault::kAbovePurse);
  CHECK(deal.CheckZeny(ro::sim::kDealMostZeny + 1, ro::sim::kDealMostZeny + 5) ==
        DealZenyFault::kAboveMost);
  CHECK(deal.CheckZeny(100, 100) == DealZenyFault::kNone);
}

TEST_CASE("Échange : les piles du partenaire se fondent, l'équipement jamais", "[sim][deal]") {
  Deal deal = OpenDeal();
  deal.TellPartnerItem(Potions(20, 0));
  deal.TellPartnerItem(Potions(29, 0));
  deal.TellPartnerItem(Sword(0));
  deal.TellPartnerItem(Sword(0));
  REQUIRE(deal.partner_offer().size() == 3);
  CHECK(deal.partner_offer()[0].item.amount == 49);

  InventoryItem refined = Potions(1, 0);
  refined.refine_level = 1;
  deal.TellPartnerItem(refined);
  CHECK(deal.partner_offer().size() == 4);
}

TEST_CASE("Échange : une offre changée après mon verrou se signale et retient la validation",
          "[sim][deal]") {
  Deal deal = OpenDeal();
  deal.TellPartnerZeny(1'000'000);
  deal.NoteLockSent();
  deal.TellLocked(false);
  const std::uint32_t seen = deal.partner_revision();
  CHECK_FALSE(deal.partner_changed_after_my_lock());

  deal.TellPartnerZeny(100'000);
  CHECK(deal.partner_changed_after_my_lock());
  CHECK(deal.partner_zeny_changed_after_my_lock());
  CHECK(deal.news().back().kind == DealNewsKind::kPartnerChangedAfterMyLock);

  deal.TellPartnerItem(Potions(1, 0));
  REQUIRE(deal.partner_offer().size() == 1);
  CHECK(deal.partner_offer()[0].changed_after_my_lock);

  deal.TellLocked(true);
  CHECK(deal.CheckExecute(seen, 0) == DealExecuteFault::kPartnerChanged);
  CHECK(deal.CheckExecute(deal.partner_revision(), 0) == DealExecuteFault::kNone);
}

TEST_CASE("Échange : la validation attend les deux verrous et une bourse intacte", "[sim][deal]") {
  Deal deal = OpenDeal();
  CHECK(deal.CheckExecute(0, 0) == DealExecuteFault::kNotBothLocked);
  deal.NoteZenySent(500);
  deal.TellLocked(false);
  deal.TellLocked(true);
  CHECK(deal.CheckExecute(deal.partner_revision(), 499) == DealExecuteFault::kZenyMelted);
  CHECK(deal.CheckExecute(deal.partner_revision(), 500) == DealExecuteFault::kNone);
  deal.NoteExecuteSent();
  CHECK(deal.CheckExecute(deal.partner_revision(), 500) == DealExecuteFault::kAlreadySent);
}

TEST_CASE("Échange : annulé après ma validation, c'est le serveur qui a refusé", "[sim][deal]") {
  Deal deal = OpenDeal();
  deal.TellLocked(false);
  deal.TellLocked(true);
  deal.NoteExecuteSent();
  deal.TellCancelled();
  CHECK(deal.phase() == DealPhase::kNone);
  CHECK(deal.news().back().kind == DealNewsKind::kRefusedAtExecution);
}

TEST_CASE("Échange : l'exécution ferme et se dit", "[sim][deal]") {
  Deal deal = OpenDeal();
  deal.TellExecuted(0);
  CHECK(deal.phase() == DealPhase::kNone);
  CHECK(deal.news().back().kind == DealNewsKind::kExecuted);
}

TEST_CASE("Échange : les gestes retenus pendant un échange", "[sim][deal]") {
  Deal deal;
  CHECK_FALSE(ro::sim::DealForbidsSkills(deal));
  CHECK_FALSE(ro::sim::DealForbidsZenySpending(deal));

  deal = OpenDeal();
  CHECK_FALSE(ro::sim::DealForbidsSkills(deal));
  deal.NoteAddSent(Potions(10), 1, 0);
  CHECK(ro::sim::DealForbidsSkills(deal));
  CHECK(ro::sim::DealEngages(deal, kPotionSlot));
  CHECK_FALSE(ro::sim::DealEngages(deal, kPotionSlot + 1));
  CHECK_FALSE(ro::sim::DealForbidsZenySpending(deal));
  deal.NoteZenySent(1);
  CHECK(ro::sim::DealForbidsZenySpending(deal));

  deal.NoteCancelSent();
  CHECK_FALSE(ro::sim::DealForbidsSkills(deal));
  CHECK_FALSE(ro::sim::DealEngages(deal, kPotionSlot));
}

TEST_CASE("Échange : la portée se mesure sur chaque axe", "[sim][deal]") {
  CHECK(ro::sim::DealReachable(10, 10, 12, 12));
  CHECK(ro::sim::DealReachable(10, 10, 8, 11));
  CHECK_FALSE(ro::sim::DealReachable(10, 10, 13, 10));
  CHECK_FALSE(ro::sim::DealReachable(10, 10, 10, 7));
}
