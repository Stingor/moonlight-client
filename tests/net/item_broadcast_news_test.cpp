// Les annonces d'objet que le serveur envoie à tous, décodées : un objet rare
// obtenu (deux formes d'une même trame), un raffinage, un grade.

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/item_news.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/item_news.h"

#include "support/net_frames.h"
#include "support/wire_bytes.h"

using ro::net::ApplyItemNewsFrame;
using ro::net::FrameOutcome;
using ro::sim::ItemNews;
using ro::test_support::FrameOf;
using ro::test_support::PutU32;

namespace {

constexpr std::uint32_t kNowMs = 7'000;
constexpr std::uint32_t kElunium = 985;
constexpr std::uint32_t kOldBlueBox = 603;
constexpr std::size_t kNameSize = 24;

void PutName(std::vector<std::uint8_t>& bytes, std::string_view name) {
  bytes.push_back(static_cast<std::uint8_t>(kNameSize));
  for (std::size_t rank = 0; rank < kNameSize; ++rank) {
    bytes.push_back(rank < name.size() ? static_cast<std::uint8_t>(name[rank]) : 0);
  }
}

// `0x07FD` : en-tête, type, objet, nom du joueur, puis la queue de la source.
[[nodiscard]] std::vector<std::uint8_t> RareObtain(std::uint8_t source, std::uint32_t item_id,
                                                   std::string_view player,
                                                   const std::vector<std::uint8_t>& tail) {
  std::vector<std::uint8_t> bytes = {0xFD, 0x07, 0, 0, source};
  PutU32(bytes, item_id);
  PutName(bytes, player);
  bytes.insert(bytes.end(), tail.begin(), tail.end());
  bytes[2] = static_cast<std::uint8_t>(bytes.size());
  bytes[3] = static_cast<std::uint8_t>(bytes.size() >> 8);
  return bytes;
}

}  // namespace

TEST_CASE("Annonces d'objet (net) : une boîte ouverte nomme la boîte", "[net][items]") {
  std::vector<std::uint8_t> tail = {4};
  PutU32(tail, kOldBlueBox);
  ItemNews news;
  CHECK(ApplyItemNewsFrame(FrameOf(RareObtain(0, kElunium, "Stingor", tail)), news, kNowMs) ==
        FrameOutcome::kApplied);
  const std::vector<ro::sim::RareItemObtain> taken = news.TakeRareObtains();
  REQUIRE(taken.size() == 1);
  CHECK(taken[0].source == 0);
  CHECK(taken[0].item_id == kElunium);
  CHECK(taken[0].player_name == "Stingor");
  CHECK(taken[0].box_item_id == kOldBlueBox);
  CHECK(taken[0].monster_name.empty());
  CHECK(taken[0].heard_ms == kNowMs);
}

TEST_CASE("Annonces d'objet (net) : un monstre se nomme, un PNJ se tait", "[net][items]") {
  std::vector<std::uint8_t> monster;
  PutName(monster, "Baphomet");
  ItemNews news;
  CHECK(ApplyItemNewsFrame(FrameOf(RareObtain(1, kElunium, "Stingor", monster)), news, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(ApplyItemNewsFrame(FrameOf(RareObtain(2, kElunium, "Stingor", {0})), news, kNowMs) ==
        FrameOutcome::kApplied);
  const std::vector<ro::sim::RareItemObtain> taken = news.TakeRareObtains();
  REQUIRE(taken.size() == 2);
  CHECK(taken[0].monster_name == "Baphomet");
  CHECK(taken[0].box_item_id == 0);
  CHECK(taken[1].source == 2);
  CHECK(taken[1].monster_name.empty());
}

TEST_CASE("Annonces d'objet (net) : une trame tronquée ne dépose rien", "[net][items]") {
  std::vector<std::uint8_t> bytes = RareObtain(0, kElunium, "Stingor", {4});
  ItemNews news;
  CHECK(ApplyItemNewsFrame(FrameOf(bytes), news, kNowMs) == FrameOutcome::kIgnored);
  CHECK(news.TakeRareObtains().empty());
}

TEST_CASE("Annonces d'objet (net) : le raffinage et le grade", "[net][items]") {
  constexpr std::int8_t kRefine = 9;
  ro::proto::ZcBroadcastItemrefiningResult refined;
  refined.name = "Stingor";
  refined.itemId = kElunium;
  refined.refine_level = kRefine;
  refined.status = 1;
  ro::proto::ZcGradeEnchantBroadcastResult graded;
  graded.name = "Stingor";
  graded.itemId = kElunium;
  graded.grade = 4;
  // ⚠ LA RÉUSSITE SE LIT À UN EXACTEMENT : deux se dit comme un échec.
  graded.status = 2;
  ItemNews news;
  CHECK(ApplyItemNewsFrame(FrameOf(Encode(refined)), news, kNowMs) == FrameOutcome::kApplied);
  CHECK(ApplyItemNewsFrame(FrameOf(Encode(graded)), news, kNowMs) == FrameOutcome::kApplied);
  const std::vector<ro::sim::RefineAnnounce> refines = news.TakeRefines();
  REQUIRE(refines.size() == 1);
  CHECK(refines[0].player_name == "Stingor");
  CHECK(refines[0].item_id == kElunium);
  CHECK(refines[0].refine_level == kRefine);
  CHECK(refines[0].success);
  const std::vector<ro::sim::GradeAnnounce> grades = news.TakeGrades();
  REQUIRE(grades.size() == 1);
  CHECK(grades[0].grade == 4);
  CHECK_FALSE(grades[0].success);
}
