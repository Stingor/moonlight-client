// SERTIR PAR GLISSER-DÉPOSER : le jugement d'un dépôt, et ce qu'il fait partir.
//
// 🔴 CE BANC PORTE CE QU'UNE SESSION NE MONTRERAIT PAS SANS LE CHERCHER :
//
//   · chaque refus que le serveur TAIT se dit ici, et rien ne part ;
//   · l'emplacement de la carte ne se devine pas : sans la réponse du serveur,
//     le dépôt attend, puis part quand même passé le délai ;
//   · un dépôt accepté devient le verbe du sertissage rapide, mot pour mot.

#include "items/card_drop.h"

#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/card_composition.h"
#include "ro/sim/inventory.h"
#include "ro/ui/card_drop.h"
#include "ro/ui/inventory_window.h"

using moonclient::CardDropFacts;
using moonclient::CardDropFlow;
using moonclient::CardDropStep;
using moonclient::CardDropVerdict;
using moonclient::JudgeCardDrop;
using ro::sim::CompatibleCards;
using ro::sim::Inventory;
using ro::sim::InventoryItem;
using ro::ui::CardDropAim;
using ro::ui::CardDropLook;
using ro::ui::CardDropSeen;

namespace {

constexpr std::uint16_t kCardSlot = 29;
constexpr std::uint16_t kSwordSlot = 33;
constexpr std::uint16_t kPotionSlot = 35;
constexpr std::uint32_t kCardId = 4035;
constexpr std::uint32_t kOtherCardId = 4001;
constexpr std::uint32_t kSwordId = 1101;
constexpr std::uint32_t kSocketlessSwordId = 1102;
constexpr std::uint32_t kPotionId = 501;
constexpr std::uint8_t kCardType = 6;
constexpr std::uint8_t kWeaponType = 5;
constexpr std::uint8_t kHealingType = 0;
constexpr std::uint32_t kRightHand = 0x2;
// L'épée a trois emplacements, selon la table du client.
constexpr int kSwordSockets = 3;
// Une marque de forgeron : une première case basse.
constexpr std::uint32_t kForgeMark = 0x00FF;
constexpr std::uint32_t kStartMs = 10'000;

const CardDropAim kOnSword{.card_slot = kCardSlot, .equip_slot = kSwordSlot};

struct Bench {
  Inventory bag;
  CompatibleCards compatible;
  bool dealing = false;

  Bench() {
    bag.Put(InventoryItem{
        .client_slot = kCardSlot, .item_id = kCardId, .type = kCardType, .amount = 2});
    bag.Put(InventoryItem{.client_slot = kSwordSlot,
                          .item_id = kSwordId,
                          .type = kWeaponType,
                          .amount = 1,
                          .wearable_at = kRightHand,
                          .identified = true});
    bag.Put(InventoryItem{.client_slot = kPotionSlot,
                          .item_id = kPotionId,
                          .type = kHealingType,
                          .amount = 5,
                          .identified = true});
  }

  // Retouche l'épée : le sac ne prête ses objets qu'en lecture.
  template <typename Edit>
  void ChangeSword(Edit edit) {
    InventoryItem sword = *bag.Find(kSwordSlot);
    edit(sword);
    bag.Put(sword);
  }

  // Le serveur a répondu pour l'épée : ces cartes-là y vont.
  void Answer(std::vector<std::uint16_t> card_slots) {
    compatible.equip_slot = kSwordSlot;
    compatible.answered = true;
    compatible.card_slots = std::move(card_slots);
  }

  [[nodiscard]] CardDropFacts Facts() const {
    return CardDropFacts{.bag = &bag,
                         .compatible = &compatible,
                         .dealing = dealing,
                         .card_slots_of = [](std::uint32_t item_id) {
                           return item_id == kSwordId ? kSwordSockets : 0;
                         }};
  }

  [[nodiscard]] CardDropVerdict Judge() const { return JudgeCardDrop(kOnSword, Facts()); }
};

[[nodiscard]] CardDropSeen Dropped(const CardDropAim& aim) {
  return CardDropSeen{.hovering = aim, .dropped = aim};
}

[[nodiscard]] CardDropSeen Hovering(const CardDropAim& aim) {
  return CardDropSeen{.hovering = aim};
}

}  // namespace

TEST_CASE("un dépôt que le serveur accepterait est accepté", "[card_drop]") {
  Bench bench;
  bench.Answer({kCardSlot});
  CHECK(bench.Judge() == CardDropVerdict::kAccepted);
  CHECK(moonclient::CardDropRefusalText(CardDropVerdict::kAccepted) == nullptr);
}

TEST_CASE("sans réponse du serveur, le dépôt attend", "[card_drop]") {
  Bench bench;
  CHECK(bench.Judge() == CardDropVerdict::kPending);
  // ⚠ UNE RÉPONSE POUR UNE AUTRE PIÈCE NE DIT RIEN DE CELLE-CI.
  bench.compatible = CompatibleCards{.equip_slot = kPotionSlot, .answered = true};
  CHECK(bench.Judge() == CardDropVerdict::kPending);
}

TEST_CASE("chaque refus muet du serveur a sa raison", "[card_drop]") {
  Bench bench;
  bench.Answer({kCardSlot});

  SECTION("échange ouvert") {
    bench.dealing = true;
    CHECK(bench.Judge() == CardDropVerdict::kDealing);
  }
  SECTION("pièce portée") {
    bench.ChangeSword([](InventoryItem& sword) { sword.worn_at = kRightHand; });
    CHECK(bench.Judge() == CardDropVerdict::kWorn);
  }
  SECTION("pièce non identifiée") {
    bench.ChangeSword([](InventoryItem& sword) { sword.identified = false; });
    CHECK(bench.Judge() == CardDropVerdict::kUnidentified);
  }
  SECTION("pièce forgée") {
    bench.ChangeSword([](InventoryItem& sword) { sword.cards[0] = kForgeMark; });
    CHECK(bench.Judge() == CardDropVerdict::kForged);
  }
  SECTION("pièce sans emplacement") {
    // Une épée que la table dit sans emplacement.
    bench.ChangeSword([](InventoryItem& sword) { sword.item_id = kSocketlessSwordId; });
    CHECK(bench.Judge() == CardDropVerdict::kNoSocket);
  }
  SECTION("pièce pleine") {
    bench.ChangeSword(
        [](InventoryItem& sword) { sword.cards = {kOtherCardId, kOtherCardId, kOtherCardId, 0}; });
    CHECK(bench.Judge() == CardDropVerdict::kFull);
  }
  SECTION("carte d'un autre emplacement") {
    bench.Answer({});
    CHECK(bench.Judge() == CardDropVerdict::kIncompatible);
  }
  CHECK(moonclient::CardDropRefusalText(bench.Judge()) != nullptr);
}

TEST_CASE("un objet disparu, ou qui n'est ni carte ni pièce, ne dit rien", "[card_drop]") {
  Bench bench;
  CHECK(JudgeCardDrop(CardDropAim{.card_slot = kPotionSlot, .equip_slot = kSwordSlot},
                      bench.Facts()) == CardDropVerdict::kVanished);
  CHECK(JudgeCardDrop(CardDropAim{.card_slot = kCardSlot, .equip_slot = kPotionSlot},
                      bench.Facts()) == CardDropVerdict::kVanished);
  REQUIRE(bench.bag.Take(kCardSlot, 2));
  CHECK(bench.Judge() == CardDropVerdict::kVanished);
  CHECK(moonclient::CardDropRefusalText(CardDropVerdict::kVanished) == nullptr);
}

TEST_CASE("un dépôt accepté devient la demande du sertissage rapide", "[card_drop]") {
  const ro::ui::InventoryWindowAction asked = moonclient::CardDropAsQuickInsert(kOnSword);
  CHECK(asked.verb == ro::ui::ItemVerb::kQuickInsert);
  CHECK(asked.slot == kSwordSlot);
  CHECK(asked.insert_card_slot == kCardSlot);

  Bench bench;
  bench.Answer({kCardSlot});
  CardDropFlow flow;
  const CardDropStep step = flow.Step(Dropped(kOnSword), CardDropSeen{}, bench.Facts(), kStartMs);
  CHECK(step.order.card_slot == kCardSlot);
  CHECK(step.order.equip_slot == kSwordSlot);
  CHECK(step.refusal == nullptr);
}

TEST_CASE("un dépôt refusé n'envoie rien et dit pourquoi", "[card_drop]") {
  Bench bench;
  bench.Answer({kCardSlot});
  bench.ChangeSword([](InventoryItem& sword) { sword.worn_at = kRightHand; });
  CardDropFlow flow;
  // ⚠ LE LÂCHER SUR LA FICHE DU PERSONNAGE se juge comme celui du sac.
  const CardDropStep step = flow.Step(CardDropSeen{}, Dropped(kOnSword), bench.Facts(), kStartMs);
  CHECK_FALSE(step.order.aimed());
  CHECK(step.refusal == moonclient::CardDropRefusalText(CardDropVerdict::kWorn));
  CHECK(step.ask_compatible == 0);
}

TEST_CASE("un dépôt en attente se tranche à la réponse", "[card_drop]") {
  Bench bench;
  CardDropFlow flow;
  CardDropStep step = flow.Step(Dropped(kOnSword), CardDropSeen{}, bench.Facts(), kStartMs);
  CHECK_FALSE(step.order.aimed());
  CHECK(step.ask_compatible == kSwordSlot);
  CHECK(flow.waiting().aimed());

  SECTION("acceptée") {
    bench.Answer({kCardSlot});
    step = flow.Step(CardDropSeen{}, CardDropSeen{}, bench.Facts(), kStartMs + 1);
    CHECK(step.order.equip_slot == kSwordSlot);
  }
  SECTION("refusée") {
    bench.Answer({});
    step = flow.Step(CardDropSeen{}, CardDropSeen{}, bench.Facts(), kStartMs + 1);
    CHECK_FALSE(step.order.aimed());
    CHECK(step.refusal == moonclient::CardDropRefusalText(CardDropVerdict::kIncompatible));
  }
  SECTION("muette : la demande part au délai") {
    step = flow.Step(CardDropSeen{}, CardDropSeen{}, bench.Facts(),
                     kStartMs + moonclient::kCardDropWaitMs - 1);
    CHECK_FALSE(step.order.aimed());
    step = flow.Step(CardDropSeen{}, CardDropSeen{}, bench.Facts(),
                     kStartMs + moonclient::kCardDropWaitMs);
    CHECK(step.order.equip_slot == kSwordSlot);
  }
  CHECK_FALSE(flow.waiting().aimed());
}

TEST_CASE("le survol montre le verdict et pose la question", "[card_drop]") {
  Bench bench;
  CardDropFlow flow;
  CardDropStep step = flow.Step(Hovering(kOnSword), CardDropSeen{}, bench.Facts(), kStartMs);
  CHECK(step.hint.Describes(kOnSword));
  CHECK(step.hint.look == CardDropLook::kUnknown);
  CHECK(step.ask_compatible == kSwordSlot);
  CHECK_FALSE(step.order.aimed());

  bench.Answer({kCardSlot});
  step = flow.Step(Hovering(kOnSword), CardDropSeen{}, bench.Facts(), kStartMs + 1);
  CHECK(step.hint.look == CardDropLook::kAccepted);
  CHECK(step.hint.refusal == nullptr);
  CHECK(step.ask_compatible == 0);

  bench.ChangeSword(
      [](InventoryItem& sword) { sword.cards = {kOtherCardId, kOtherCardId, kOtherCardId, 0}; });
  step = flow.Step(Hovering(kOnSword), CardDropSeen{}, bench.Facts(), kStartMs + 2);
  CHECK(step.hint.look == CardDropLook::kRefused);
  CHECK(step.hint.refusal == moonclient::CardDropRefusalText(CardDropVerdict::kFull));

  // Plus rien sous le curseur : plus de verdict.
  step = flow.Step(CardDropSeen{}, CardDropSeen{}, bench.Facts(), kStartMs + 3);
  CHECK_FALSE(step.hint.Describes(kOnSword));
}
