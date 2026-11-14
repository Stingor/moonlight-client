// Les paquets d'instance : chaque décodeur relit son encodeur, refuse une
// taille fausse, et la commande de destruction porte la valeur que le serveur
// lit.

#include "ro/proto/memorial_packets.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/play_ready_packets.h"

using ro::proto::Decode;
using ro::proto::Encode;

TEST_CASE("Instance : réservée, puis ouverte, sur le fil", "[proto][instance]") {
  const ro::proto::InstanceCreatedNotice created{.name = "Old Glast Heim", .wait_position = 2};
  std::vector<std::uint8_t> bytes = Encode(created);
  REQUIRE(bytes.size() == ro::proto::InstanceCreatedNotice::kWireSize);
  ro::proto::InstanceCreatedNotice read_created;
  REQUIRE(Decode(bytes, read_created));
  CHECK(read_created.name == "Old Glast Heim");
  CHECK(read_created.wait_position == 2);

  const ro::proto::InstanceStatusNotice status{
      .name = "Airship Assault", .keep_limit_unix_s = 1'800'000'000, .idle_limit_unix_s = 0};
  bytes = Encode(status);
  REQUIRE(bytes.size() == ro::proto::InstanceStatusNotice::kWireSize);
  ro::proto::InstanceStatusNotice read_status;
  REQUIRE(Decode(bytes, read_status));
  CHECK(read_status.name == "Airship Assault");
  CHECK(read_status.keep_limit_unix_s == 1'800'000'000U);
  CHECK(read_status.idle_limit_unix_s == 0U);
}

TEST_CASE("Instance : la file, le changement, et une taille fausse", "[proto][instance]") {
  ro::proto::InstanceWaitNotice wait;
  REQUIRE(Decode(Encode(ro::proto::InstanceWaitNotice{.wait_position = 0xFFFF}), wait));
  CHECK(wait.wait_position == ro::proto::InstanceWaitNotice::kCancelled);

  ro::proto::InstanceChangeNotice change;
  REQUIRE(Decode(Encode(ro::proto::InstanceChangeNotice{.type = 3, .limit_unix_s = 7}), change));
  CHECK(change.type == 3U);
  CHECK(change.limit_unix_s == 7U);

  std::vector<std::uint8_t> truncated = Encode(ro::proto::InstanceChangeNotice{});
  truncated.pop_back();
  CHECK_FALSE(Decode(truncated, change));
  // Un autre opcode de la même taille n'est pas un changement.
  std::vector<std::uint8_t> other = Encode(ro::proto::InstanceChangeNotice{});
  other[0] = 0x01;
  CHECK_FALSE(Decode(other, change));
}

TEST_CASE("Instance : la destruction part avec la commande 3", "[proto][instance]") {
  const std::vector<std::uint8_t> expected = {0xCF, 0x02, 0x03, 0x00, 0x00, 0x00};
  CHECK(ro::proto::EncodeInstanceDestroyCommand() == expected);
}

TEST_CASE("Prêt à jouer : deux octets, tels que le serveur les lit", "[proto][instance]") {
  const auto frame = ro::proto::EncodePlayReady();
  REQUIRE(frame.has_value());
  CHECK(*frame == std::vector<std::uint8_t>{0x47, 0x04});
}
