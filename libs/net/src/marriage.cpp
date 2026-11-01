// Le mariage : la cérémonie, le divorce et le nom du conjoint. Voir l'en-tête.

#include "ro/net/marriage.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <string>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/text.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/chat_line.h"

namespace ro::net {
namespace {

// L'effet de la cérémonie : le jet de pétales et `wedding.wav`.
constexpr std::uint32_t kWeddingEffect = 347;

// Le jaune du divorce, en `0xRRGGBB`.
constexpr std::uint32_t kDivorceColor = 0xFFFF00;

// Assez pour deux noms de personnage et leur phrase.
constexpr std::size_t kLineBytes = 160;

FrameOutcome ApplyCongratulation(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  proto::ZcCongratulation packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }
  const auto actor_id = static_cast<std::uint32_t>(packet.GID);
  // Un acteur nul ne porte rien ; un acteur hors de vue non plus.
  if (actor_id == 0 || !world.RecordEffect(actor_id, kWeddingEffect, now_ms)) {
    ro::core::LogDebug("mariage : acteur {} hors de la scène, la cérémonie ne paraîtra pas",
                       actor_id);
    return FrameOutcome::kIgnored;
  }
  return FrameOutcome::kApplied;
}

FrameOutcome ApplyDivorce(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  proto::ZcDivorce packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }
  const std::string former_spouse = core::Utf8FromWire(packet.name);
  std::array<char, kLineBytes> line{};
  std::snprintf(line.data(), line.size(), core::Tr("%s et %s ont divorcé."),
                world.player_name().c_str(), former_spouse.c_str());
  world.RecordChat(sim::ChatKind::kServer, sim::ChatType::kPublic, 0, line.data(), now_ms,
                   kDivorceColor);
  return FrameOutcome::kApplied;
}

}  // namespace

FrameOutcome ApplyMarriageFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  switch (frame.opcode) {
    case proto::ZcCongratulation::kOpcode:
      return ApplyCongratulation(frame, world, now_ms);
    case proto::ZcDivorce::kOpcode:
      return ApplyDivorce(frame, world, now_ms);
    case proto::ZcCouplename::kOpcode:
      return FrameOutcome::kIgnored;
    default:
      return FrameOutcome::kUnhandled;
  }
}

}  // namespace ro::net
