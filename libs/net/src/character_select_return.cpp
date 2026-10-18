#include "ro/net/character_select_return.h"

#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"

#include "send_encoded.h"

namespace ro::net {
namespace {

// `ZC_ACK_REQ_DISCONNECT` : un opcode, puis un résultat sur deux octets
// (`clif_disconnect_ack`). Le générateur ne le porte pas : il se lit à la main.
constexpr std::uint16_t kDisconnectAckOpcode = 0x018B;
constexpr std::size_t kDisconnectAckSize = 4;
constexpr std::size_t kDisconnectAckResultOffset = 2;
// Le type de `ZC_RESTART_ACK` qui accorde le retour.
constexpr std::uint8_t kRestartGranted = 1;

}  // namespace

core::Result<void> RequestCharacterSelect(Connection& link) {
  proto::RestartRequest request;
  request.what = proto::RestartRequest::kCharacterSelect;
  return SendEncoded(link, proto::Encode(request));
}

CharacterSelectAnswer CharacterSelectAnswerOf(const Frame& frame) {
  if (frame.opcode == proto::ZcRestartAck::kOpcode) {
    proto::ZcRestartAck packet;
    if (!proto::Decode(frame.bytes, packet)) {
      return CharacterSelectAnswer::kNone;
    }
    // ⚠ ZÉRO EST UN REFUS : le serveur de personnages ne répondait pas.
    return packet.type == kRestartGranted ? CharacterSelectAnswer::kGranted
                                          : CharacterSelectAnswer::kRefused;
  }
  if (frame.opcode == kDisconnectAckOpcode && frame.bytes.size() >= kDisconnectAckSize) {
    const auto result =
        static_cast<std::uint16_t>(frame.bytes[kDisconnectAckResultOffset] |
                                   (frame.bytes[kDisconnectAckResultOffset + 1] << 8));
    return result != 0 ? CharacterSelectAnswer::kRefused : CharacterSelectAnswer::kNone;
  }
  return CharacterSelectAnswer::kNone;
}

}  // namespace ro::net
