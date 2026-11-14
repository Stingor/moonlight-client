// L'ÉCHANGE ENTRE JOUEURS : les cinq demandes écrites à la main, octet par octet.

#include "ro/proto/deal_packets.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::proto::DealAnswer;
using ro::proto::DealCancelRequest;
using ro::proto::DealExecuteRequest;
using ro::proto::DealLockRequest;
using ro::proto::DealRequest;
using ro::proto::Encode;

TEST_CASE("Échange : la proposition porte le compte du partenaire", "[proto][deal]") {
  const auto frame = Encode(DealRequest{.partner_aid = 0x00201234});
  REQUIRE(frame.has_value());
  CHECK(*frame == std::vector<std::uint8_t>{0xE4, 0x00, 0x34, 0x12, 0x20, 0x00});
}

TEST_CASE("Échange : une proposition sans partenaire est refusée", "[proto][deal]") {
  CHECK_FALSE(Encode(DealRequest{}).has_value());
}

TEST_CASE("Échange : la réponse ne porte que trois ou quatre", "[proto][deal]") {
  const auto accepted = Encode(DealAnswer{.accept = true});
  REQUIRE(accepted.has_value());
  CHECK(*accepted == std::vector<std::uint8_t>{0xE6, 0x00, DealAnswer::kAccept});
  const auto refused = Encode(DealAnswer{.accept = false});
  REQUIRE(refused.has_value());
  CHECK(*refused == std::vector<std::uint8_t>{0xE6, 0x00, DealAnswer::kRefuse});
}

TEST_CASE("Échange : verrou, annulation et validation ne sont que leur opcode", "[proto][deal]") {
  const auto lock = Encode(DealLockRequest{});
  REQUIRE(lock.has_value());
  CHECK(*lock == std::vector<std::uint8_t>{0xEB, 0x00});
  const auto cancel = Encode(DealCancelRequest{});
  REQUIRE(cancel.has_value());
  CHECK(*cancel == std::vector<std::uint8_t>{0xED, 0x00});
  const auto execute = Encode(DealExecuteRequest{});
  REQUIRE(execute.has_value());
  CHECK(*execute == std::vector<std::uint8_t>{0xEF, 0x00});
}
