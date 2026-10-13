// Les points de manière : la demande et ses deux réponses. Voir l'en-tête.

#include "ro/net/manner_points.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

#include "ro/core/byte_reader.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/text.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/staff_packets.h"
#include "ro/sim/chat_line.h"

namespace ro::net {
namespace {

// L'opcode et la taille fixe du verdict : aucune structure générée ne le décrit.
constexpr std::uint16_t kMannerVerdictOpcode = 0x014A;
constexpr std::size_t kMannerVerdictWireSize = 6;

// Les verdicts que le client écrit, tels que le serveur les numérote.
enum class MannerVerdict : std::uint32_t {
  kAligned = 0,
  kBlockedByStaff = 3,
  kBlockedByAntiSpam = 4,
};

// LES COULEURS, en `0xRRGGBB` : le cyan de ce qu'un geste du staff a fait, le
// jaune d'un silence subi.
constexpr std::uint32_t kAlignedColor = 0x00FFFF;
constexpr std::uint32_t kBlockedColor = 0xFFFF00;

// Assez pour un nom de personnage et sa phrase.
constexpr std::size_t kLineBytes = 160;

void Say(sim::World& world, const char* text, std::uint32_t color, std::uint32_t now_ms) {
  world.RecordChat(sim::ChatKind::kServer, sim::ChatType::kPublic, 0, text, now_ms, color);
}

FrameOutcome ApplyVerdict(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  if (frame.bytes.size() != kMannerVerdictWireSize) {
    return FrameOutcome::kUndecodable;
  }
  core::ByteReader reader(frame.bytes);
  static_cast<void>(reader.U16());
  const std::uint32_t verdict = reader.U32();
  switch (static_cast<MannerVerdict>(verdict)) {
    case MannerVerdict::kAligned:
      Say(world, core::Tr("Le point de manière a été appliqué."), kAlignedColor, now_ms);
      return FrameOutcome::kApplied;
    case MannerVerdict::kBlockedByStaff:
      Say(world, core::Tr("Le staff vous a interdit le chat et les skills pour mauvaise conduite."),
          kBlockedColor, now_ms);
      return FrameOutcome::kApplied;
    case MannerVerdict::kBlockedByAntiSpam:
      Say(world, core::Tr("L'anti-spam vous a interdit le chat et les skills pour un moment."),
          kBlockedColor, now_ms);
      return FrameOutcome::kApplied;
  }
  core::LogDebug("point de manière : verdict {} sans phrase", verdict);
  return FrameOutcome::kIgnored;
}

FrameOutcome ApplyGiven(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  proto::ZcNotifyMannerPointGiven packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }
  if (packet.type != static_cast<std::uint8_t>(MannerPointWay::kGood)) {
    return FrameOutcome::kIgnored;
  }
  const std::string author = core::Utf8FromWire(packet.name);
  std::array<char, kLineBytes> line{};
  std::snprintf(line.data(), line.size(), core::Tr("Bon point de manière reçu de %s."),
                author.c_str());
  Say(world, line.data(), kAlignedColor, now_ms);
  return FrameOutcome::kApplied;
}

}  // namespace

core::Result<void> RequestMannerPoint(Connection& link, std::uint32_t account_id,
                                      MannerPointWay way, int minutes) {
  proto::MannerPointRequest request;
  request.account_id = account_id;
  request.kind = static_cast<std::uint8_t>(way);
  request.minutes = static_cast<std::uint16_t>(std::clamp(minutes, 0, kMannerPointsMostMinutes));
  auto frame = proto::Encode(request);
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

FrameOutcome ApplyMannerPointFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  switch (frame.opcode) {
    case kMannerVerdictOpcode:
      return ApplyVerdict(frame, world, now_ms);
    case proto::ZcNotifyMannerPointGiven::kOpcode:
      return ApplyGiven(frame, world, now_ms);
    default:
      return FrameOutcome::kUnhandled;
  }
}

}  // namespace ro::net
