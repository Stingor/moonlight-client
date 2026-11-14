// LES PAQUETS DU SUIVI DES QUÊTES : aller-retour, longueurs refusées, champs
// que le serveur écrit mal.

#include "ro/proto/quest_packets.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace ro::proto;

namespace {

constexpr std::size_t kStartTimeOffset = 7;
constexpr std::size_t kHuntCountOffset = 15;
constexpr std::size_t kFirstHuntOffset = 17;
constexpr std::size_t kFirstMobNameOffset = kFirstHuntOffset + 18;
constexpr std::uint32_t kQuestId = 1001;
constexpr std::uint32_t kExpiresUnixS = 1'700'086'400;

[[nodiscard]] QuestAdded SampleAdded() {
  QuestAdded added{.quest_id = kQuestId, .active = true, .expires_unix_s = kExpiresUnixS};
  added.hunts.push_back(QuestAddedHunt{.hunt_id = kQuestId * 1000,
                                       .mob_type = 4,
                                       .mob_id = 1002,
                                       .level_min = 1,
                                       .level_max = 99,
                                       .killed = 3,
                                       .mob_name = "Poring"});
  added.hunts.push_back(QuestAddedHunt{.hunt_id = kQuestId * 1000 + 1, .mob_name = "Lunatic"});
  return added;
}

}  // namespace

TEST_CASE("Quête ajoutée : l'aller-retour garde tout, et le paquet fait 143 octets",
          "[proto][quest]") {
  const std::vector<std::uint8_t> bytes = EncodeQuestAdded(SampleAdded());
  CHECK(bytes.size() == QuestAdded::kWireSize);
  const auto decoded = DecodeQuestAdded(bytes);
  REQUIRE(decoded);
  CHECK(decoded->quest_id == kQuestId);
  CHECK(decoded->active);
  CHECK(decoded->expires_unix_s == kExpiresUnixS);
  REQUIRE(decoded->hunts.size() == 2);
  CHECK(decoded->hunts[0].hunt_id == kQuestId * 1000);
  CHECK(decoded->hunts[0].mob_type == 4);
  CHECK(decoded->hunts[0].mob_id == 1002);
  CHECK(decoded->hunts[0].level_max == 99);
  CHECK(decoded->hunts[0].killed == 3);
  CHECK(decoded->hunts[0].mob_name == "Poring");
  CHECK(decoded->hunts[1].mob_name == "Lunatic");
}

TEST_CASE("Quête ajoutée : l'instant de début est ignoré", "[proto][quest]") {
  std::vector<std::uint8_t> bytes = EncodeQuestAdded(SampleAdded());
  bytes[kStartTimeOffset] = 0x7F;
  bytes[kStartTimeOffset + 1] = 0xFF;
  const auto decoded = DecodeQuestAdded(bytes);
  REQUIRE(decoded);
  CHECK(decoded->expires_unix_s == kExpiresUnixS);
}

TEST_CASE("Quête ajoutée : un nom s'arrête au premier zéro", "[proto][quest]") {
  std::vector<std::uint8_t> bytes = EncodeQuestAdded(SampleAdded());
  // « Poring » suivi d'un zéro puis d'un reste de bourrage non nul.
  bytes[kFirstMobNameOffset + 7] = 'X';
  const auto decoded = DecodeQuestAdded(bytes);
  REQUIRE(decoded);
  CHECK(decoded->hunts[0].mob_name == "Poring");
}

TEST_CASE("Quête ajoutée : longueur, opcode et nombre de chasses refusés", "[proto][quest]") {
  std::vector<std::uint8_t> bytes = EncodeQuestAdded(SampleAdded());
  SECTION("trop courte") {
    bytes.pop_back();
    CHECK_FALSE(DecodeQuestAdded(bytes));
  }
  SECTION("trop longue") {
    bytes.push_back(0);
    CHECK_FALSE(DecodeQuestAdded(bytes));
  }
  SECTION("autre opcode") {
    bytes[0] ^= 1;
    CHECK_FALSE(DecodeQuestAdded(bytes));
  }
  SECTION("quatre chasses") {
    bytes[kHuntCountOffset] = 4;
    CHECK_FALSE(DecodeQuestAdded(bytes));
  }
  SECTION("trois chasses passent") {
    bytes[kHuntCountOffset] = 3;
    const auto decoded = DecodeQuestAdded(bytes);
    REQUIRE(decoded);
    CHECK(decoded->hunts.size() == 3);
  }
}

TEST_CASE("Totaux de chasse : l'aller-retour, et les longueurs refusées", "[proto][quest]") {
  QuestHuntTotals totals;
  totals.entries = {{.quest_or_hunt_id = 1'001'000, .mob_id = 1002, .total = 10, .killed = 3},
                    {.quest_or_hunt_id = 1'001'001, .mob_id = 0, .total = 5, .killed = 0}};
  std::vector<std::uint8_t> bytes = EncodeQuestHuntTotals(totals);
  CHECK(bytes.size() == QuestHuntTotals::kHeaderSize + 2 * QuestHuntTotal::kWireSize);
  const auto decoded = DecodeQuestHuntTotals(bytes);
  REQUIRE(decoded);
  REQUIRE(decoded->entries.size() == 2);
  CHECK(decoded->entries[0].mob_id == 1002);
  CHECK(decoded->entries[0].total == 10);
  CHECK(decoded->entries[1].mob_id == 0);
  CHECK(decoded->entries[1].total == 5);

  SECTION("une trame sans entrée") { CHECK(DecodeQuestHuntTotals(EncodeQuestHuntTotals({}))); }
  SECTION("une queue qui n'est pas faite d'entrées entières") {
    bytes.pop_back();
    bytes[2] = static_cast<std::uint8_t>(bytes.size());
    CHECK_FALSE(DecodeQuestHuntTotals(bytes));
  }
  SECTION("une longueur annoncée qui ment") {
    bytes.pop_back();
    CHECK_FALSE(DecodeQuestHuntTotals(bytes));
  }
}

TEST_CASE("Progression : l'aller-retour, le cas vide, et les longueurs refusées",
          "[proto][quest]") {
  QuestHuntProgress progress;
  progress.entries = {{.quest_id = kQuestId, .hunt_id = 1'001'000, .total = 10, .killed = 4},
                      {.quest_id = kQuestId, .hunt_id = 1'001'001, .total = 5, .killed = 5}};
  std::vector<std::uint8_t> bytes = EncodeQuestHuntProgress(progress);
  CHECK(bytes.size() == QuestHuntProgress::kHeaderSize + 2 * QuestHuntCount::kWireSize);
  const auto decoded = DecodeQuestHuntProgress(bytes);
  REQUIRE(decoded);
  REQUIRE(decoded->entries.size() == 2);
  CHECK(decoded->entries[1].hunt_id == 1'001'001);
  CHECK(decoded->entries[1].killed == 5);

  SECTION("n = 0") {
    const auto empty = DecodeQuestHuntProgress(EncodeQuestHuntProgress({}));
    REQUIRE(empty);
    CHECK(empty->entries.empty());
  }
  SECTION("une longueur qui n'est pas 6 + 12n") {
    bytes.pop_back();
    bytes[2] = static_cast<std::uint8_t>(bytes.size());
    CHECK_FALSE(DecodeQuestHuntProgress(bytes));
  }
  SECTION("un nombre d'entrées qui contredit la longueur") {
    bytes[4] = 3;
    CHECK_FALSE(DecodeQuestHuntProgress(bytes));
  }
  SECTION("une longueur annoncée qui ment") {
    bytes.pop_back();
    CHECK_FALSE(DecodeQuestHuntProgress(bytes));
  }
}

TEST_CASE("Retrait, état et demande d'état : aller-retour et longueurs", "[proto][quest]") {
  const std::vector<std::uint8_t> removed = EncodeQuestRemoved({.quest_id = kQuestId});
  CHECK(removed.size() == QuestRemoved::kWireSize);
  CHECK(DecodeQuestRemoved(removed)->quest_id == kQuestId);
  CHECK_FALSE(DecodeQuestRemoved(std::vector<std::uint8_t>(removed.begin(), removed.end() - 1)));

  const std::vector<std::uint8_t> notice =
      EncodeQuestActiveNotice({.quest_id = kQuestId, .active = true});
  CHECK(notice.size() == QuestActiveNotice::kWireSize);
  const auto notice_back = DecodeQuestActiveNotice(notice);
  REQUIRE(notice_back);
  CHECK(notice_back->quest_id == kQuestId);
  CHECK(notice_back->active);
  CHECK_FALSE(DecodeQuestActiveNotice(removed));

  const std::vector<std::uint8_t> request =
      EncodeQuestActiveRequest({.quest_id = kQuestId, .active = false});
  CHECK(request.size() == QuestActiveRequest::kWireSize);
  const auto request_back = DecodeQuestActiveRequest(request);
  REQUIRE(request_back);
  CHECK_FALSE(request_back->active);
  // Même longueur que l'état, autre opcode : ils ne se confondent pas.
  CHECK_FALSE(DecodeQuestActiveNotice(request));
}
