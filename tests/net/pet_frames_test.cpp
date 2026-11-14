// LES TRAMES DU FAMILIER : une trame appliquée change l'état attendu, une
// demande part avec la disposition que le serveur lit.

#include "ro/net/pet_frames.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/config_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/pet_packets.h"
#include "ro/sim/pet.h"
#include "ro/sim/world.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"

using ro::net::ApplyPetAccessoryLook;
using ro::net::ApplyPetFrame;
using ro::net::FrameOutcome;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;

namespace {

constexpr std::int32_t kPetActor = 110000123;
constexpr std::int16_t kPoring = 1002;
constexpr std::int16_t kBackpack = 10013;

std::vector<std::uint8_t> StateChange(ro::sim::PetStateChange change, std::int32_t actor,
                                      std::int32_t value) {
  ro::proto::ZcChangestatePet packet;
  packet.type = static_cast<std::int8_t>(change);
  packet.GID = actor;
  packet.data = value;
  return ro::proto::Encode(packet);
}

}  // namespace

TEST_CASE("PetFrames : la naissance puis la fiche décrivent le familier", "[petframes]") {
  ro::sim::Pet pet;
  CHECK(ApplyPetFrame(FrameOf(StateChange(ro::sim::PetStateChange::kBirth, kPetActor, 0)), pet) ==
        FrameOutcome::kApplied);
  ro::proto::ZcPropertyPet property;
  property.szName = "Popo";
  property.bModified = 1;
  property.nLevel = 3;
  property.nFullness = 60;
  property.nRelationship = 420;
  property.ITID = kBackpack;
  property.job = kPoring;
  CHECK(ApplyPetFrame(FrameOf(ro::proto::Encode(property)), pet) == FrameOutcome::kApplied);
  CHECK(pet.known());
  CHECK(pet.actor_id() == static_cast<std::uint32_t>(kPetActor));
  CHECK(pet.name() == "Popo");
  CHECK_FALSE(pet.renameable());
  CHECK(pet.level() == 3);
  CHECK(pet.hunger() == 60);
  CHECK(pet.intimacy() == 420);
  CHECK(pet.accessory_item_id() == static_cast<std::uint32_t>(kBackpack));
  CHECK(pet.pet_class() == static_cast<std::uint32_t>(kPoring));

  CHECK(ApplyPetFrame(FrameOf(StateChange(ro::sim::PetStateChange::kIntimacy, kPetActor, 95)),
                      pet) == FrameOutcome::kApplied);
  CHECK(pet.intimacy() == 95);
  CHECK(pet.TakeLowIntimacyWarning());
  // L'œuf vidé : le familier n'existe plus.
  CHECK(ApplyPetFrame(FrameOf(StateChange(ro::sim::PetStateChange::kEgg, kPetActor, 0)), pet) ==
        FrameOutcome::kApplied);
  CHECK_FALSE(pet.known());
}

TEST_CASE("PetFrames : la liste d'éclosion, le résultat et l'écho de l'auto-alimentation",
          "[petframes]") {
  ro::sim::Pet pet;
  CHECK(ApplyPetFrame(FrameOf(ro::proto::EncodePetEggList(ro::proto::PetEggList{{14, 18}})), pet) ==
        FrameOutcome::kApplied);
  CHECK(pet.eggs_offered());
  CHECK(pet.TakeEggOfferOpened());
  CHECK(pet.offered_egg_slots() == std::vector<std::uint16_t>{14, 18});

  CHECK(ApplyPetFrame(FrameOf(ro::proto::EncodePetEvolutionResult(
                          ro::proto::PetEvolutionResult{.result = 4})),
                      pet) == FrameOutcome::kApplied);
  CHECK(pet.TakeEvolutionResult() == std::optional<std::uint32_t>(4));

  const auto echo = [](ro::proto::ConfigType type, std::uint32_t value) {
    return FrameOf(ro::proto::EncodeConfigAck(
        ro::proto::ConfigAck{.type = static_cast<std::uint32_t>(type), .value = value}));
  };
  CHECK(ApplyPetFrame(echo(ro::proto::ConfigType::kPetAutofeed, 1), pet) == FrameOutcome::kApplied);
  CHECK(pet.auto_feed());
  CHECK(pet.TakeAutoFeedEcho() == std::optional<bool>(true));
  // L'écho d'un autre réglage ne le regarde pas.
  CHECK(ApplyPetFrame(echo(ro::proto::ConfigType::kHomunculusAutofeed, 0), pet) ==
        FrameOutcome::kUnhandled);
  CHECK(pet.auto_feed());
}

TEST_CASE("PetFrames : les demandes partent avec la disposition du serveur", "[petframes]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  REQUIRE(ro::net::RequestPetCommand(link, ro::proto::PetCommand::kFeed).has_value());
  CHECK(server.Receive(ro::proto::PetCommandRequest::kWireSize) ==
        std::vector<std::uint8_t>{0xA1, 0x01, 1});

  REQUIRE(ro::net::RequestPetEgg(link, 14).has_value());
  CHECK(server.Receive(ro::proto::PetEggChoice::kWireSize) ==
        std::vector<std::uint8_t>{0xA7, 0x01, 14, 0});

  REQUIRE(ro::net::RequestPetAutoFeed(link, true).has_value());
  CHECK(server.Receive(ro::proto::ConfigRequest::kWireSize) ==
        std::vector<std::uint8_t>{0xD8, 0x02, 2, 0, 0, 0, 1, 0, 0, 0});

  // 302 = 0x012E : le baiser, en émoticône.
  REQUIRE(ro::net::RequestPetAct(link, 302).has_value());
  CHECK(server.Receive(ro::proto::PetActRequest::kWireSize) ==
        std::vector<std::uint8_t>{0xA9, 0x01, 0x2E, 0x01, 0, 0});

  // LA BORNE SE MESURE SUR LA FORME ENVOYÉE : vingt-trois « é » font
  // quarante-six octets en UTF-8, mais vingt-trois sur le fil.
  std::string accented;
  for (int letter = 0; letter < 23; ++letter) {
    accented += "\xC3\xA9";
  }
  REQUIRE(ro::net::RequestPetRename(link, accented).has_value());
  const std::vector<std::uint8_t> renamed = server.Receive(ro::proto::PetRenameRequest::kWireSize);
  REQUIRE(renamed.size() == ro::proto::PetRenameRequest::kWireSize);
  CHECK(renamed[2] == 0xE9);
  CHECK(renamed[24] == 0xE9);
  CHECK(renamed[25] == 0);

  // Un nom trop long ne part pas.
  CHECK_FALSE(ro::net::RequestPetRename(link, std::string(24, 'a')).has_value());
}

TEST_CASE("PetFrames : le nom arrive transcodé, l'accessoire suit son changement d'état",
          "[petframes]") {
  ro::sim::Pet pet;
  CHECK(ApplyPetFrame(FrameOf(StateChange(ro::sim::PetStateChange::kBirth, kPetActor, 0)), pet) ==
        FrameOutcome::kApplied);
  ro::proto::ZcPropertyPet property;
  // « Pépé » dans la page de codes du fil.
  property.szName = "P\xE9p\xE9";
  property.job = kPoring;
  CHECK(ApplyPetFrame(FrameOf(ro::proto::Encode(property)), pet) == FrameOutcome::kApplied);
  CHECK(pet.name() == "P\xC3\xA9p\xC3\xA9");
  CHECK_FALSE(pet.wears_accessory());

  CHECK(ApplyPetFrame(
            FrameOf(StateChange(ro::sim::PetStateChange::kAccessory, kPetActor, kBackpack)), pet) ==
        FrameOutcome::kApplied);
  CHECK(pet.wears_accessory());
  CHECK(pet.TakeSheetWanted());
  CHECK(ApplyPetFrame(FrameOf(StateChange(ro::sim::PetStateChange::kAccessory, kPetActor, 0)),
                      pet) == FrameOutcome::kApplied);
  CHECK_FALSE(pet.wears_accessory());
}

TEST_CASE("PetFrames : l'accessoire de tout pet visible se pose dans sa coiffe basse",
          "[petframes]") {
  // LE VOISIN AUSSI : le serveur envoie ce changement à la zone, et le dessin
  // lit la coiffe basse de chaque pet.
  constexpr std::int32_t kNeighbourPet = 110000456;
  constexpr auto kNeighbourId = static_cast<std::uint32_t>(kNeighbourPet);
  ro::sim::World world;
  ro::sim::Actor actor;
  actor.id = kNeighbourId;
  actor.familiar = ro::sim::Familiar::kPet;
  world.Spawn(actor);

  CHECK(ApplyPetAccessoryLook(
      FrameOf(StateChange(ro::sim::PetStateChange::kAccessory, kNeighbourPet, kBackpack)), world));
  REQUIRE(world.Find(kNeighbourId) != nullptr);
  CHECK(world.Find(kNeighbourId)->look.lower_headgear == kBackpack);
  CHECK(ApplyPetAccessoryLook(
      FrameOf(StateChange(ro::sim::PetStateChange::kAccessory, kNeighbourPet, 0)), world));
  CHECK(world.Find(kNeighbourId)->look.lower_headgear == 0);

  // Un autre changement d'état, ou un acteur que le monde ignore, ne touche rien.
  CHECK_FALSE(ApplyPetAccessoryLook(
      FrameOf(StateChange(ro::sim::PetStateChange::kHunger, kNeighbourPet, kBackpack)), world));
  CHECK(world.Find(kNeighbourId)->look.lower_headgear == 0);
  CHECK_FALSE(ApplyPetAccessoryLook(
      FrameOf(StateChange(ro::sim::PetStateChange::kAccessory, kPetActor, kBackpack)), world));
}

TEST_CASE("PetFrames : l'accusé d'un repas dit s'il a été pris, et quelle nourriture",
          "[petframes]") {
  constexpr std::uint32_t kPetFood = 537;
  ro::sim::Pet pet;

  // Sur le fil : l'opcode, un octet de résultat, puis le numéro de l'objet.
  const std::vector<std::uint8_t> fed =
      ro::proto::Encode(ro::proto::ZcFeedPet{.result = 1, .itemId = kPetFood});
  CHECK(fed == std::vector<std::uint8_t>{0xA3, 0x01, 1, 0x19, 0x02, 0, 0});
  CHECK(ApplyPetFrame(FrameOf(fed), pet) == FrameOutcome::kApplied);
  CHECK(pet.TakeFeeding() ==
        std::optional<ro::sim::PetFeeding>({.fed = true, .food_item_id = kPetFood}));
  // Dit une fois.
  CHECK_FALSE(pet.TakeFeeding().has_value());

  CHECK(ApplyPetFrame(
            FrameOf(ro::proto::Encode(ro::proto::ZcFeedPet{.result = 0, .itemId = kPetFood})),
            pet) == FrameOutcome::kApplied);
  CHECK(pet.TakeFeeding() ==
        std::optional<ro::sim::PetFeeding>({.fed = false, .food_item_id = kPetFood}));
}

TEST_CASE("PetFrames : la parole d'un familier se lit, de quelque familier que ce soit",
          "[petframes]") {
  constexpr std::uint32_t kNeighbourPet = 110000999;
  const auto heard = ro::net::ReadPetAct(
      FrameOf(ro::proto::Encode(ro::proto::ZcPetAct{.GID = kNeighbourPet, .data = 1002030})));
  REQUIRE(heard.has_value());
  CHECK(heard->actor_id == kNeighbourPet);
  CHECK(heard->code == 1002030);
  CHECK_FALSE(
      ro::net::ReadPetAct(FrameOf(StateChange(ro::sim::PetStateChange::kBirth, kPetActor, 0)))
          .has_value());
}

TEST_CASE("PetFrames : le spectacle se lit, et n'est pas un autre changement d'état",
          "[petframes]") {
  const auto shown = ro::net::ReadPetPerformance(
      FrameOf(StateChange(ro::sim::PetStateChange::kPerformance, kPetActor, 3)));
  REQUIRE(shown.has_value());
  CHECK(shown->actor_id == static_cast<std::uint32_t>(kPetActor));
  CHECK(shown->performance == 3);
  CHECK_FALSE(ro::net::ReadPetPerformance(
                  FrameOf(StateChange(ro::sim::PetStateChange::kHunger, kPetActor, 3)))
                  .has_value());
}
