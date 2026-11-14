// Les nouvelles d'objets, décodées : le ramassage d'un membre du groupe et une
// pièce abîmée, déposés tels quels pour l'application.

#include "ro/net/item_news.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/map_packets.h"
#include "ro/sim/item_news.h"

#include "support/net_frames.h"

using ro::net::ApplyItemNewsFrame;
using ro::net::FrameOutcome;
using ro::sim::ItemNews;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kNowMs = 42'000;
constexpr std::uint32_t kMember = 2'000'101;
constexpr std::uint32_t kKnife = 1201;
constexpr std::uint32_t kPoringCard = 4001;
constexpr std::uint16_t kRightHand = 0x0002;

}  // namespace

TEST_CASE("Nouvelles d'objets (net) : le ramassage d'un membre garde tout l'objet",
          "[net][items]") {
  ro::proto::ZcItemPickupParty packet;
  packet.AID = kMember;
  packet.itemId = kKnife;
  packet.identified = 1;
  packet.refine = 7;
  packet.grade = 2;
  packet.location = kRightHand;
  packet.slot.card = {kPoringCard, 0, 0, 0};
  ItemNews news;
  CHECK(ApplyItemNewsFrame(FrameOf(ro::proto::Encode(packet)), news, kNowMs) ==
        FrameOutcome::kApplied);

  const std::vector<ro::sim::PartyPickup> pickups = news.TakePartyPickups();
  REQUIRE(pickups.size() == 1);
  CHECK(pickups[0].account_id == kMember);
  CHECK(pickups[0].heard_ms == kNowMs);
  CHECK(pickups[0].item.item_id == kKnife);
  CHECK(pickups[0].item.identified);
  CHECK(pickups[0].item.refine_level == 7);
  CHECK(pickups[0].item.grade == 2);
  CHECK(pickups[0].item.wearable_at == kRightHand);
  CHECK(pickups[0].item.cards[0] == kPoringCard);
  // Prise une fois, la nouvelle est partie.
  CHECK(news.TakePartyPickups().empty());
}

TEST_CASE("Nouvelles d'objets (net) : une pièce abîmée garde qui et où", "[net][items]") {
  ro::proto::ZcEquipitemDamaged packet;
  packet.equipLocation = kRightHand;
  packet.GID = kMember;
  ItemNews news;
  CHECK(ApplyItemNewsFrame(FrameOf(ro::proto::Encode(packet)), news, kNowMs) ==
        FrameOutcome::kApplied);
  const std::vector<ro::sim::EquipDamage> damages = news.TakeEquipDamages();
  REQUIRE(damages.size() == 1);
  CHECK(damages[0].account_id == kMember);
  CHECK(damages[0].location == kRightHand);
  CHECK(damages[0].heard_ms == kNowMs);
}

TEST_CASE("Nouvelles d'objets (sim) : la file ne garde que les plus récentes", "[sim][items]") {
  ItemNews news;
  for (std::uint32_t rank = 0; rank < ro::sim::kItemNewsKept + 3; ++rank) {
    news.NoteEquipDamage(ro::sim::EquipDamage{.account_id = rank, .location = kRightHand});
  }
  const std::vector<ro::sim::EquipDamage> damages = news.TakeEquipDamages();
  REQUIRE(damages.size() == ro::sim::kItemNewsKept);
  CHECK(damages.front().account_id == 3);
}
