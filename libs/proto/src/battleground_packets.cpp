// Les paquets du champ de bataille. Voir l'en-tête.

#include "ro/proto/battleground_packets.h"

#include <array>
#include <string_view>
#include <utility>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/proto/chat_limits.h"

#include "named_chat.h"

namespace ro::proto {
namespace {

// Le gestionnaire du serveur qui lit la parole du champ, et ses décalages :
// la longueur en 2, le texte en 4 — comme la parole publique.
constexpr std::string_view kBattleChatHandler = "clif_parse_BattleChat";
constexpr std::array<std::uint16_t, 2> kBattleChatOffsets = {2, 4};

// Le terminateur que le serveur pose après le texte de la parole, et qu'il
// compte dans la longueur.
constexpr std::uint8_t kTextTerminator = 0;

}  // namespace

bool Decode(std::span<const std::uint8_t> frame, BattlefieldChatNotice& packet) {
  if (frame.size() < BattlefieldChatNotice::kHeaderSize) {
    return false;
  }
  core::ByteReader reader(frame);
  if (reader.U16() != BattlefieldChatNotice::kOpcode) {
    return false;
  }
  // La longueur déclarée est celle de la trame entière : une autre dirait une
  // trame mal découpée, dont le texte ne serait pas le bon.
  if (reader.U16() != frame.size()) {
    return false;
  }
  BattlefieldChatNotice read;
  read.speaker_id = reader.U32();
  read.speaker_name = std::string(reader.FixedString(kBattlefieldNameBytes));
  read.message = std::string(reader.TailString());
  if (!reader.ok()) {
    return false;
  }
  packet = std::move(read);
  return true;
}

bool Decode(std::span<const std::uint8_t> frame, BattlefieldCampNotice& packet) {
  if (frame.size() != BattlefieldCampNotice::kWireSize) {
    return false;
  }
  core::ByteReader reader(frame);
  if (reader.U16() != BattlefieldCampNotice::kOpcode) {
    return false;
  }
  BattlefieldCampNotice read;
  read.actor_id = reader.U32();
  read.name = std::string(reader.FixedString(kBattlefieldNameBytes));
  read.camp = reader.U16();
  if (!reader.ok()) {
    return false;
  }
  packet = std::move(read);
  return true;
}

bool Decode(std::span<const std::uint8_t> frame, BattlefieldScoreNotice& packet) {
  if (frame.size() != BattlefieldScoreNotice::kWireSize) {
    return false;
  }
  core::ByteReader reader(frame);
  if (reader.U16() != BattlefieldScoreNotice::kOpcode) {
    return false;
  }
  BattlefieldScoreNotice read;
  read.lion = reader.U16();
  read.eagle = reader.U16();
  if (!reader.ok()) {
    return false;
  }
  packet = read;
  return true;
}

bool Decode(std::span<const std::uint8_t> frame, BattlefieldPositionNotice& packet) {
  if (frame.size() != BattlefieldPositionNotice::kWireSize) {
    return false;
  }
  core::ByteReader reader(frame);
  if (reader.U16() != BattlefieldPositionNotice::kOpcode) {
    return false;
  }
  BattlefieldPositionNotice read;
  read.account_id = reader.U32();
  read.name = std::string(reader.FixedString(kBattlefieldNameBytes));
  read.job = reader.U16();
  read.x = reader.I16();
  read.y = reader.I16();
  if (!reader.ok()) {
    return false;
  }
  packet = std::move(read);
  return true;
}

bool Decode(std::span<const std::uint8_t> frame, LobbyAdmissionAck& packet) {
  if (frame.size() != LobbyAdmissionAck::kWireSize) {
    return false;
  }
  core::ByteReader reader(frame);
  if (reader.U16() != LobbyAdmissionAck::kOpcode) {
    return false;
  }
  LobbyAdmissionAck read;
  read.result = reader.U8();
  read.field_name = std::string(reader.FixedString(kBattlefieldNameBytes));
  read.lobby_name = std::string(reader.FixedString(kBattlefieldNameBytes));
  if (!reader.ok()) {
    return false;
  }
  packet = std::move(read);
  return true;
}

std::vector<std::uint8_t> Encode(const BattlefieldChatNotice& packet) {
  const std::size_t length = BattlefieldChatNotice::kHeaderSize + packet.message.size() + 1;
  std::vector<std::uint8_t> frame;
  frame.reserve(length);
  core::ByteWriter writer(frame);
  writer.U16(BattlefieldChatNotice::kOpcode);
  writer.U16(static_cast<std::uint16_t>(length));
  writer.U32(packet.speaker_id);
  writer.FixedString(packet.speaker_name, kBattlefieldNameBytes);
  writer.Text(packet.message);
  writer.U8(kTextTerminator);
  return frame;
}

std::vector<std::uint8_t> Encode(const BattlefieldCampNotice& packet) {
  std::vector<std::uint8_t> frame;
  frame.reserve(BattlefieldCampNotice::kWireSize);
  core::ByteWriter writer(frame);
  writer.U16(BattlefieldCampNotice::kOpcode);
  writer.U32(packet.actor_id);
  writer.FixedString(packet.name, kBattlefieldNameBytes);
  writer.U16(packet.camp);
  return frame;
}

std::vector<std::uint8_t> Encode(const BattlefieldScoreNotice& packet) {
  std::vector<std::uint8_t> frame;
  frame.reserve(BattlefieldScoreNotice::kWireSize);
  core::ByteWriter writer(frame);
  writer.U16(BattlefieldScoreNotice::kOpcode);
  writer.U16(packet.lion);
  writer.U16(packet.eagle);
  return frame;
}

std::vector<std::uint8_t> Encode(const BattlefieldPositionNotice& packet) {
  std::vector<std::uint8_t> frame;
  frame.reserve(BattlefieldPositionNotice::kWireSize);
  core::ByteWriter writer(frame);
  writer.U16(BattlefieldPositionNotice::kOpcode);
  writer.U32(packet.account_id);
  writer.FixedString(packet.name, kBattlefieldNameBytes);
  writer.U16(packet.job);
  writer.I16(packet.x);
  writer.I16(packet.y);
  return frame;
}

std::vector<std::uint8_t> Encode(const LobbyAdmissionAck& packet) {
  std::vector<std::uint8_t> frame;
  frame.reserve(LobbyAdmissionAck::kWireSize);
  core::ByteWriter writer(frame);
  writer.U16(LobbyAdmissionAck::kOpcode);
  writer.U8(packet.result);
  writer.FixedString(packet.field_name, kBattlefieldNameBytes);
  writer.FixedString(packet.lobby_name, kBattlefieldNameBytes);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const BattlefieldChatRequest& request) {
  // Le serveur compare le nom en tête au personnage : vide, la ligne serait
  // rejetée sans un mot.
  if (request.speaker.empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "la parole du champ de bataille exige le nom de qui parle");
  }
  return EncodeNamedChat(BattlefieldChatRequest::kOpcode, kBattleChatHandler, kBattleChatOffsets,
                         request.speaker, request.text, kChatNamedBodyMax);
}

}  // namespace ro::proto
