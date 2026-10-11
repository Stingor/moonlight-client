// L'inspecteur du staff : la demande et la réponse. Voir l'en-tête.

#include "ro/net/entity_props.h"

#include <string_view>

#include "ro/core/byte_reader.h"
#include "ro/core/text.h"
#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

static_assert(kEntityPropsReplyOpcode == proto::ZcMoonlightEntityProps::kOpcode);
static_assert(kEntityPropsHeaderSize == proto::ZcMoonlightEntityProps::kWireSize);

[[nodiscard]] EntityPropsStatus StatusOf(std::uint8_t wire) {
  switch (wire) {
    case static_cast<std::uint8_t>(EntityPropsStatus::kFound):
      return EntityPropsStatus::kFound;
    case static_cast<std::uint8_t>(EntityPropsStatus::kDenied):
      return EntityPropsStatus::kDenied;
    default:
      break;
  }
  return EntityPropsStatus::kMissing;
}

// Un texte préfixé de sa longueur sur un octet, ou rien quand la trame n'en
// porte pas autant. Le serveur écrit dans la page de codes du jeu, mais relaie
// tels quels les noms qu'un client en UTF-8 a donnés : le fil porte les deux.
[[nodiscard]] std::optional<std::string> LengthPrefixed(core::ByteReader& reader) {
  if (reader.remaining() < 1) {
    return std::nullopt;
  }
  const std::size_t length = reader.U8();
  if (reader.remaining() < length) {
    return std::nullopt;
  }
  return core::Utf8FromWire(reader.FixedString(length));
}

}  // namespace

core::Result<void> RequestEntityProps(Connection& link, std::uint32_t gid) {
  proto::CzMoonlightReqEntityProps request;
  // ⚠ LA TAILLE EST ÉCRITE, le serveur la LIT.
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightReqEntityProps::kWireSize);
  request.gid = gid;
  link.Send(proto::Encode(request));
  return {};
}

std::optional<EntityPropsReply> DecodeEntityProps(std::span<const std::uint8_t> frame) {
  proto::ZcMoonlightEntityProps head;
  if (!proto::Decode(frame, head)) {
    return std::nullopt;
  }
  EntityPropsReply reply;
  reply.gid = head.gid;
  reply.status = StatusOf(head.status);
  if (reply.status != EntityPropsStatus::kFound) {
    return reply;
  }
  core::ByteReader reader(frame);
  reader.Skip(kEntityPropsHeaderSize);
  for (std::size_t rank = 0; rank < head.count; ++rank) {
    std::optional<std::string> key = LengthPrefixed(reader);
    if (!key.has_value()) {
      break;
    }
    std::optional<std::string> value = LengthPrefixed(reader);
    reply.props.push_back(EntityProp{.key = std::move(*key), .value = value.value_or("")});
    if (!value.has_value()) {
      break;
    }
  }
  return reply;
}

std::optional<EntityPropsReply> ReadEntityProps(const Frame& frame) {
  if (frame.opcode != proto::ZcMoonlightEntityProps::kOpcode) {
    return std::nullopt;
  }
  return DecodeEntityProps(frame.bytes);
}

}  // namespace ro::net
