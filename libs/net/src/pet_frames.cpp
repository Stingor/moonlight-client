// Voir l'en-tête.

#include "ro/net/pet_frames.h"

#include <string>
#include <utility>
#include <vector>

#include "ro/core/text.h"
#include "ro/proto/config_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

// Le plus grand changement d'état que le familier connaisse.
constexpr std::int8_t kLastStateChange = static_cast<std::int8_t>(sim::PetStateChange::kEgg);

[[nodiscard]] core::Result<void> Sent(Connection& link,
                                      core::Result<std::vector<std::uint8_t>> frame) {
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

[[nodiscard]] std::uint32_t Unsigned(std::int16_t value) {
  return static_cast<std::uint32_t>(static_cast<std::uint16_t>(value));
}

}  // namespace

FrameOutcome ApplyPetFrame(const Frame& frame, sim::Pet& pet) {
  switch (frame.opcode) {
    case proto::ZcPropertyPet::kOpcode:
      return Decoded<proto::ZcPropertyPet>(frame, [&pet](const auto& packet) {
        pet.ApplyProperty(sim::PetProperty{
            .name = core::Utf8FromWire(packet.szName),
            .renamed = packet.bModified != 0,
            .level = packet.nLevel,
            .hunger = packet.nFullness,
            .intimacy = packet.nRelationship,
            .accessory_item_id = Unsigned(packet.ITID),
            .pet_class = Unsigned(packet.job),
        });
        return FrameOutcome::kApplied;
      });
    case proto::ZcChangestatePet::kOpcode:
      return Decoded<proto::ZcChangestatePet>(frame, [&pet](const auto& packet) {
        if (packet.type < 0 || packet.type > kLastStateChange) {
          return FrameOutcome::kIgnored;
        }
        return pet.ApplyStateChange(static_cast<sim::PetStateChange>(packet.type),
                                    static_cast<std::uint32_t>(packet.GID), packet.data)
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kUnhandled;
      });
    case proto::PetEggList::kOpcode: {
      auto list = proto::DecodePetEggList(frame.bytes);
      if (!list) {
        return FrameOutcome::kUndecodable;
      }
      pet.OfferEggs(std::move(list->client_slots));
      return FrameOutcome::kApplied;
    }
    case proto::PetEvolutionResult::kOpcode: {
      const auto result = proto::DecodePetEvolutionResult(frame.bytes);
      if (!result) {
        return FrameOutcome::kUndecodable;
      }
      pet.AnnounceEvolution(result->result);
      return FrameOutcome::kApplied;
    }
    case proto::ConfigAck::kOpcode: {
      const auto ack = proto::DecodeConfigAck(frame.bytes);
      if (!ack) {
        return FrameOutcome::kUndecodable;
      }
      if (ack->type != static_cast<std::uint32_t>(proto::ConfigType::kPetAutofeed)) {
        return FrameOutcome::kUnhandled;
      }
      pet.AnnounceAutoFeed(ack->value != 0);
      return FrameOutcome::kApplied;
    }
    default:
      return FrameOutcome::kUnhandled;
  }
}

bool ApplyPetAccessoryLook(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcChangestatePet::kOpcode) {
    return false;
  }
  proto::ZcChangestatePet packet;
  if (!proto::Decode(frame.bytes, packet) ||
      packet.type != static_cast<std::int8_t>(sim::PetStateChange::kAccessory)) {
    return false;
  }
  const auto actor_id = static_cast<std::uint32_t>(packet.GID);
  const sim::Actor* actor = world.Find(actor_id);
  if (actor == nullptr) {
    return false;
  }
  sim::Appearance look = actor->look;
  // L'apparence d'un accessoire tient sur seize bits, comme toute coiffe.
  look.lower_headgear = static_cast<std::uint16_t>(packet.data);
  return world.SetLook(actor_id, look);
}

core::Result<void> RequestPetCommand(Connection& link, proto::PetCommand command) {
  return Sent(link, proto::Encode(proto::PetCommandRequest{.command = command}));
}

core::Result<void> RequestPetRename(Connection& link, std::string_view name) {
  return Sent(link, proto::Encode(proto::PetRenameRequest{.name = core::Cp1252FromUtf8(name)}));
}

core::Result<void> RequestPetEgg(Connection& link, std::uint16_t client_slot) {
  return Sent(link, proto::Encode(proto::PetEggChoice{.client_slot = client_slot}));
}

core::Result<void> RequestPetEvolution(Connection& link, std::uint32_t target_egg_id) {
  return Sent(link, proto::Encode(proto::PetEvolutionRequest{.target_egg_id = target_egg_id}));
}

core::Result<void> RequestPetAutoFeed(Connection& link, bool enabled) {
  return Sent(link, proto::Encode(proto::ConfigRequest{.type = proto::ConfigType::kPetAutofeed,
                                                       .value = enabled ? 1U : 0U}));
}

}  // namespace ro::net
