// LA RECHERCHE DE GROUPE : les quatre demandes, écrites octet par octet, et les
// six réponses et avis, relus.
//
// 🔴 LE SERVEUR LIT LES DEMANDES PAR DÉCALAGES ET ÉCRIT LES RÉPONSES CHAMP PAR
// CHAMP : l'essai fige l'ordre et la largeur de chaque champ.

#include "ro/proto/party_booking_packets.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::proto::Encode;
using ro::proto::PartyBookingEntry;
using ro::proto::PartyBookingJobs;

namespace {

constexpr std::int16_t kLevel = 50;
// Région 2, lieu 3.
constexpr std::int16_t kMapCode = 0x0203;
constexpr std::int16_t kKnight = 11;
constexpr std::int16_t kPriest = 12;
constexpr std::uint32_t kIndex = 7;
constexpr std::uint32_t kPostedUnix = 0x01020304;
constexpr PartyBookingJobs kJobs = {kKnight, kPriest, -1, -1, -1, -1};

PartyBookingEntry SampleEntry() {
  PartyBookingEntry entry;
  entry.index = kIndex;
  entry.leader_name = "Alice";
  entry.posted_unix = kPostedUnix;
  entry.level = kLevel;
  entry.map_code = kMapCode;
  entry.jobs = kJobs;
  return entry;
}

}  // namespace

TEST_CASE("Recherche de groupe : la publication porte niveau, lieu et six métiers",
          "[proto][party_booking]") {
  ro::proto::PartyBookingRegisterRequest request;
  request.level = kLevel;
  request.map_code = kMapCode;
  request.jobs = kJobs;
  const auto frame = Encode(request);
  REQUIRE(frame.has_value());
  const std::vector<std::uint8_t> expected = {
      0x02, 0x08,                                      // opcode
      50,   0x00,                                      // niveau
      0x03, 0x02,                                      // lieu : région 2, lieu 3
      11,   0x00, 12,   0x00,                          // Knight, Priest
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // cases vides : octet bas 0xFF
  };
  CHECK(*frame == expected);
}

TEST_CASE("Recherche de groupe : la recherche porte ses critères et le premier rang",
          "[proto][party_booking]") {
  ro::proto::PartyBookingSearchRequest request;
  request.level = kLevel;
  request.job = kKnight;
  request.first_index = 0x0A0B0C0D;
  request.result_count = 10;
  const auto frame = Encode(request);
  REQUIRE(frame.has_value());
  const std::vector<std::uint8_t> expected = {
      0x04, 0x08,              // opcode
      50,   0x00,              // niveau
      0x00, 0x00,              // aucun lieu
      11,   0x00,              // Knight
      0x0D, 0x0C, 0x0B, 0x0A,  // premier rang
      10,   0x00,              // compte demandé
  };
  CHECK(*frame == expected);
}

TEST_CASE("Recherche de groupe : retrait et modification", "[proto][party_booking]") {
  const auto removal = Encode(ro::proto::PartyBookingDeleteRequest{});
  REQUIRE(removal.has_value());
  CHECK(*removal == std::vector<std::uint8_t>{0x06, 0x08});

  ro::proto::PartyBookingUpdateRequest update;
  update.jobs = kJobs;
  const auto frame = Encode(update);
  REQUIRE(frame.has_value());
  const std::vector<std::uint8_t> expected = {0x08, 0x08, 11,   0x00, 12,   0x00, 0xFF,
                                              0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  CHECK(*frame == expected);
}

TEST_CASE("Recherche de groupe : une page se relit annonce par annonce", "[proto][party_booking]") {
  ro::proto::PartyBookingSearchAck ack;
  ack.more_results = true;
  ack.entries = {SampleEntry(), SampleEntry()};
  ack.entries[1].index = kIndex + 1;
  const std::vector<std::uint8_t> bytes = ro::proto::EncodePartyBookingSearchAck(ack);
  REQUIRE(bytes.size() == ro::proto::PartyBookingSearchAck::kHeaderSize +
                              2 * ro::proto::PartyBookingEntry::kWireSize);
  const auto read = ro::proto::DecodePartyBookingSearchAck(bytes);
  REQUIRE(read.has_value());
  CHECK(read->more_results);
  REQUIRE(read->entries.size() == 2);
  CHECK(read->entries[0].index == kIndex);
  CHECK(read->entries[0].leader_name == "Alice");
  CHECK(read->entries[0].posted_unix == kPostedUnix);
  CHECK(read->entries[0].level == kLevel);
  CHECK(read->entries[0].map_code == kMapCode);
  CHECK(read->entries[0].jobs == kJobs);
  CHECK(read->entries[1].index == kIndex + 1);

  // Une page vide n'a que son en-tête.
  const auto empty =
      ro::proto::DecodePartyBookingSearchAck(ro::proto::EncodePartyBookingSearchAck({}));
  REQUIRE(empty.has_value());
  CHECK(empty->entries.empty());
  CHECK_FALSE(empty->more_results);
}

TEST_CASE("Recherche de groupe : une page qui ne tombe pas juste est refusée",
          "[proto][party_booking]") {
  ro::proto::PartyBookingSearchAck ack;
  ack.entries = {SampleEntry()};
  std::vector<std::uint8_t> bytes = ro::proto::EncodePartyBookingSearchAck(ack);
  // Un octet de trop, annoncé : le reste n'est pas une annonce entière.
  bytes.push_back(0);
  bytes[2] = static_cast<std::uint8_t>(bytes.size());
  CHECK_FALSE(ro::proto::DecodePartyBookingSearchAck(bytes).has_value());
  // Une longueur annoncée qui n'est pas celle de la trame.
  bytes.pop_back();
  CHECK_FALSE(ro::proto::DecodePartyBookingSearchAck(bytes).has_value());
}

TEST_CASE("Recherche de groupe : accusés et avis se relisent", "[proto][party_booking]") {
  const auto registered = ro::proto::DecodePartyBookingRegisterAck(
      ro::proto::EncodePartyBookingRegisterAck({.result = 2}));
  REQUIRE(registered.has_value());
  CHECK(registered->result == 2);

  const auto deleted =
      ro::proto::DecodePartyBookingDeleteAck(ro::proto::EncodePartyBookingDeleteAck({.result = 3}));
  REQUIRE(deleted.has_value());
  CHECK(deleted->result == 3);

  const std::vector<std::uint8_t> inserted_bytes =
      ro::proto::EncodePartyBookingInsertNotice({.entry = SampleEntry()});
  CHECK(inserted_bytes.size() == ro::proto::PartyBookingInsertNotice::kWireSize);
  const auto inserted = ro::proto::DecodePartyBookingInsertNotice(inserted_bytes);
  REQUIRE(inserted.has_value());
  CHECK(inserted->entry.leader_name == "Alice");
  CHECK(inserted->entry.jobs == kJobs);

  const auto updated = ro::proto::DecodePartyBookingUpdateNotice(
      ro::proto::EncodePartyBookingUpdateNotice({.index = kIndex, .jobs = kJobs}));
  REQUIRE(updated.has_value());
  CHECK(updated->index == kIndex);
  CHECK(updated->jobs == kJobs);

  const auto removed = ro::proto::DecodePartyBookingDeleteNotice(
      ro::proto::EncodePartyBookingDeleteNotice({.index = kIndex}));
  REQUIRE(removed.has_value());
  CHECK(removed->index == kIndex);

  // Une trame d'un autre paquet, ou tronquée, est refusée.
  std::vector<std::uint8_t> wrong = ro::proto::EncodePartyBookingDeleteNotice({.index = kIndex});
  wrong[0] = 0x0A;
  CHECK_FALSE(ro::proto::DecodePartyBookingDeleteNotice(wrong).has_value());
  wrong.pop_back();
  CHECK_FALSE(ro::proto::DecodePartyBookingRegisterAck(wrong).has_value());
}
