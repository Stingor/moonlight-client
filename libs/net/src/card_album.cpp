// L'album de cartes sur le fil. Voir l'en-tête pour la forme des deux trames.

#include "ro/net/card_album.h"

#include <algorithm>
#include <utility>

#include "ro/core/byte_reader.h"
#include "ro/proto/map_packets.h"

namespace ro::net {

std::optional<CardAlbumReply> DecodeCardAlbum(std::span<const std::uint8_t> frame) {
  proto::ZcMoonlightCardAlbum packet;
  if (!proto::Decode(frame, packet)) {
    return std::nullopt;
  }
  CardAlbumReply reply;
  reply.result = static_cast<sim::CardAlbumResult>(packet.result);

  const std::size_t tail = frame.size() - kCardAlbumFixedSize;
  // La taille d'une entrée telle que CE serveur l'envoie. Un serveur d'avant
  // l'octet de nature tient en onze octets, et le catalogue se lit quand même.
  std::size_t stride = kCardAlbumEntrySize;
  if (packet.count > 0 && tail / packet.count >= kCardAlbumEntrySize) {
    stride = tail / packet.count;
  }
  reply.dropper_known = stride >= kCardAlbumEntryWithDropperSize;

  // 🔴 LE COMPTE EST CRU, MAIS PAS SUR PAROLE : une trame tronquée en
  // annoncerait plus qu'elle n'en porte.
  const std::size_t count = std::min<std::size_t>(packet.count, tail / stride);
  reply.cards.reserve(count);
  core::ByteReader reader(frame);
  for (std::size_t rank = 0; rank < count; ++rank) {
    if (!reader.Seek(kCardAlbumFixedSize + rank * stride)) {
      break;
    }
    sim::CardAlbumCard card;
    card.card_id = reader.U32();
    card.reserve = reader.U16();
    card.unlocked = (reader.U8() & kCardAlbumUnlockedFlag) != 0;
    card.equip_mask = reader.U32();
    if (reply.dropper_known) {
      card.dropper_rank = reader.U8();
    }
    if (!reader.ok()) {
      break;
    }
    reply.cards.push_back(card);
  }
  return reply;
}

FrameOutcome ApplyCardAlbumFrame(const Frame& frame, sim::CardAlbum& album) {
  if (frame.opcode != proto::ZcMoonlightCardAlbum::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  std::optional<CardAlbumReply> reply = DecodeCardAlbum(frame.bytes);
  if (!reply.has_value()) {
    return FrameOutcome::kUndecodable;
  }
  album.Receive(reply->result, std::move(reply->cards), reply->dropper_known);
  return FrameOutcome::kApplied;
}

core::Result<void> RequestCardAlbum(Connection& link, CardAlbumCommand command, std::uint32_t arg,
                                    std::uint16_t amount) {
  proto::CzMoonlightCardAlbumCmd request;
  request.cmd = static_cast<std::uint8_t>(command);
  request.arg = arg;
  request.amount = amount;
  const std::vector<std::uint8_t> bytes = proto::Encode(request);
  if (bytes.empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "commande d'album non encodable");
  }
  link.Send(bytes);
  return {};
}

}  // namespace ro::net
