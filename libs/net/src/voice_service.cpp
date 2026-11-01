#include "ro/net/voice_service.h"

#include <utility>

#include "ro/core/byte_reader.h"
#include "ro/proto/map_packets.h"

namespace ro::net {

std::optional<VoiceService> DecodeVoiceService(std::span<const std::uint8_t> frame) {
  if (frame.size() != kVoiceServiceSize) return std::nullopt;
  // La table des longueurs le tient pour variable (sa structure porte `len`) :
  // `len` doit dire la taille exacte avant que la structure engendrée ne le lise.
  core::ByteReader header(frame);
  const std::uint16_t opcode = header.U16();
  const std::uint16_t length = header.U16();
  if (!header.ok() || opcode != kVoiceServiceOpcode || length != frame.size()) {
    return std::nullopt;
  }
  proto::ZcMoonlightVoiceService packet;
  if (!proto::Decode(frame, packet)) return std::nullopt;
  if (packet.host.empty() || packet.port == 0) return std::nullopt;
  return VoiceService{.host = std::move(packet.host), .port = packet.port};
}

}  // namespace ro::net
