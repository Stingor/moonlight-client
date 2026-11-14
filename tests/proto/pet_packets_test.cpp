// LES PAQUETS DU FAMILIER ÉCRITS À LA MAIN : la liste d'éclosion, le résultat
// d'une évolution, et les cinq demandes du client.

#include "ro/proto/pet_packets.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::proto::Encode;

TEST_CASE("PetPackets : la liste d'éclosion porte sa longueur et deux octets par case",
          "[petpackets]") {
  const std::vector<std::uint8_t> frame = {0xA6, 0x01, 8, 0, 14, 0, 18, 0};
  const auto list = ro::proto::DecodePetEggList(frame);
  REQUIRE(list.has_value());
  CHECK(list->client_slots == std::vector<std::uint16_t>{14, 18});
  CHECK(ro::proto::EncodePetEggList(*list) == frame);

  const std::vector<std::uint8_t> empty = {0xA6, 0x01, 4, 0};
  const auto none = ro::proto::DecodePetEggList(empty);
  REQUIRE(none.has_value());
  CHECK(none->client_slots.empty());

  // Une longueur annoncée qui ment, une queue impaire.
  CHECK_FALSE(
      ro::proto::DecodePetEggList(std::vector<std::uint8_t>{0xA6, 0x01, 8, 0, 14, 0}).has_value());
  CHECK_FALSE(
      ro::proto::DecodePetEggList(std::vector<std::uint8_t>{0xA6, 0x01, 5, 0, 14}).has_value());
}

TEST_CASE("PetPackets : le résultat d'une évolution est un entier de quatre octets",
          "[petpackets]") {
  const std::vector<std::uint8_t> frame = {0xFC, 0x09, 4, 0, 0, 0};
  const auto result = ro::proto::DecodePetEvolutionResult(frame);
  REQUIRE(result.has_value());
  CHECK(result->result ==
        static_cast<std::uint32_t>(ro::proto::PetEvolutionOutcome::kFailMaterial));
  CHECK(ro::proto::EncodePetEvolutionResult(*result) == frame);
  CHECK_FALSE(ro::proto::DecodePetEvolutionResult(std::vector<std::uint8_t>{0xFC, 0x09, 4, 0, 0})
                  .has_value());
}

TEST_CASE("PetPackets : un ordre tient en trois octets", "[petpackets]") {
  const auto frame =
      Encode(ro::proto::PetCommandRequest{.command = ro::proto::PetCommand::kReturnToEgg});
  REQUIRE(frame.has_value());
  CHECK(*frame == std::vector<std::uint8_t>{0xA1, 0x01, 3});
}

TEST_CASE("PetPackets : le nom part sur vingt-quatre octets, et se refuse trop long",
          "[petpackets]") {
  const auto frame = Encode(ro::proto::PetRenameRequest{.name = "Poring"});
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == ro::proto::PetRenameRequest::kWireSize);
  CHECK((*frame)[0] == 0xA5);
  CHECK((*frame)[1] == 0x01);
  CHECK((*frame)[2] == 'P');
  CHECK((*frame)[7] == 'g');
  CHECK((*frame)[8] == 0);
  CHECK((*frame)[25] == 0);

  CHECK_FALSE(Encode(ro::proto::PetRenameRequest{.name = ""}).has_value());
  CHECK(Encode(ro::proto::PetRenameRequest{.name = std::string(23, 'a')}).has_value());
  CHECK_FALSE(Encode(ro::proto::PetRenameRequest{.name = std::string(24, 'a')}).has_value());
}

TEST_CASE("PetPackets : l'œuf choisi part par sa case, telle que la liste l'a donnée",
          "[petpackets]") {
  const auto frame = Encode(ro::proto::PetEggChoice{.client_slot = 18});
  REQUIRE(frame.has_value());
  CHECK(*frame == std::vector<std::uint8_t>{0xA7, 0x01, 18, 0});
}

TEST_CASE("PetPackets : l'évolution part sous sa forme minimale de huit octets", "[petpackets]") {
  const auto frame = Encode(ro::proto::PetEvolutionRequest{.target_egg_id = 9069});
  REQUIRE(frame.has_value());
  // 9069 = 0x236D.
  CHECK(*frame == std::vector<std::uint8_t>{0xFB, 0x09, 8, 0, 0x6D, 0x23, 0, 0});
}

TEST_CASE("PetPackets : la parole du familier est un entier de quatre octets", "[petpackets]") {
  // La réplique « repas » d'un Poring au rang de faim 3, en petit-boutiste.
  const auto frame = Encode(ro::proto::PetActRequest{.data = 1002030});
  REQUIRE(frame.has_value());
  CHECK(*frame == std::vector<std::uint8_t>{0xA9, 0x01, 0x2E, 0x4A, 0x0F, 0x00});
}
