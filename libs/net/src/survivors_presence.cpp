// La présence en arène sur le fil : codage et décodage, sans émettre. Voir
// l'en-tête pour la forme des trames.

#include "ro/net/survivors_presence.h"

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"

namespace ro::net {
namespace {

[[nodiscard]] SurvivorsPresenceState StateOf(std::uint8_t wire) {
  switch (wire) {
    case static_cast<std::uint8_t>(SurvivorsPresenceState::kPlaying):
      return SurvivorsPresenceState::kPlaying;
    case static_cast<std::uint8_t>(SurvivorsPresenceState::kPaused):
      return SurvivorsPresenceState::kPaused;
    default:
      break;
  }
  return SurvivorsPresenceState::kOut;
}

}  // namespace

std::vector<std::uint8_t> EncodeSurvivorsPresenceAsk(const SurvivorsPresenceAsk& ask) {
  std::vector<std::uint8_t> bytes;
  bytes.reserve(kSurvivorsPresenceAskSize);
  core::ByteWriter writer(bytes);
  writer.U16(kSurvivorsPresenceAskOpcode);
  writer.U16(static_cast<std::uint16_t>(kSurvivorsPresenceAskSize));
  writer.U8(static_cast<std::uint8_t>(ask.state));
  writer.U16(ask.arena_id);
  writer.U16(ask.class_id);
  writer.U8(ask.flags);
  return bytes;
}

std::optional<SurvivorsPresence> DecodeSurvivorsPresence(std::span<const std::uint8_t> frame) {
  if (frame.size() != kSurvivorsPresenceSize) return std::nullopt;
  core::ByteReader reader(frame);
  const std::uint16_t opcode = reader.U16();
  const std::uint16_t length = reader.U16();
  if (opcode != kSurvivorsPresenceOpcode || length != frame.size()) return std::nullopt;
  SurvivorsPresence presence;
  presence.account_id = reader.U32();
  presence.char_id = reader.U32();
  presence.state = StateOf(reader.U8());
  presence.arena_id = reader.U16();
  presence.class_id = reader.U16();
  presence.elapsed_s = reader.U32();
  if (!reader.ok()) return std::nullopt;
  return presence;
}

}  // namespace ro::net
