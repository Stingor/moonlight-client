// Les classements et les avis d'étoile sur le fil. Voir l'en-tête.

#include "ro/net/rank_notices.h"

#include <string_view>

#include "ro/core/byte_reader.h"
#include "ro/core/text.h"
#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

// L'AVIS D'ÉTOILE n'a pas de structure côté serveur : `clif_starskill` l'écrit
// champ par champ. Le nom à 2, l'identifiant à 26, l'étoile à 30, le code à 31.
constexpr std::uint16_t kStarNoticeOpcode = 0x020E;
constexpr std::size_t kStarNoticeSize = 32;
constexpr std::size_t kStarNoticeNameOffset = 2;
constexpr std::size_t kStarNoticeNameSize = 24;

// Un nom de taille fixe, coupé à son premier zéro.
std::string FixedName(std::span<const std::uint8_t> raw) {
  std::string_view bytes(reinterpret_cast<const char*>(raw.data()), raw.size());
  if (const std::size_t end = bytes.find('\0'); end != std::string_view::npos) {
    bytes = bytes.substr(0, end);
  }
  return core::Utf8FromCp1252(bytes);
}

}  // namespace

void RequestRanking(Connection& link, RankKind kind) {
  proto::CzReqRanking request;
  request.type = static_cast<std::uint16_t>(kind);
  link.Send(proto::Encode(request));
}

std::optional<RankingList> ReadRankingList(const Frame& frame) {
  if (frame.opcode != proto::ZcAckRanking2::kOpcode) {
    return std::nullopt;
  }
  proto::ZcAckRanking2 packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return std::nullopt;
  }
  RankingList list;
  list.kind = static_cast<RankKind>(packet.type);
  list.character_ids = packet.CIDs;
  list.points = packet.points;
  list.own_points = packet.mypoints;
  return list;
}

std::optional<RankingPoints> ReadRankingPoints(const Frame& frame) {
  if (frame.opcode != proto::ZcUpdateRankingPoint::kOpcode) {
    return std::nullopt;
  }
  proto::ZcUpdateRankingPoint packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return std::nullopt;
  }
  return RankingPoints{.kind = static_cast<RankKind>(packet.type),
                       .gained = packet.points,
                       .total = packet.points_total};
}

std::optional<StarNotice> ReadStarNotice(const Frame& frame) {
  if (frame.opcode != kStarNoticeOpcode || frame.bytes.size() < kStarNoticeSize) {
    return std::nullopt;
  }
  core::ByteReader reader(frame.bytes);
  reader.Skip(kStarNoticeNameOffset);
  StarNotice notice;
  notice.name = FixedName(reader.Bytes(kStarNoticeNameSize));
  notice.subject_id = reader.U32();
  notice.star = reader.U8();
  notice.code = static_cast<StarNoticeCode>(reader.U8());
  if (!reader.ok()) {
    return std::nullopt;
  }
  return notice;
}

std::optional<CharacterNameAnswer> ReadCharacterName(const Frame& frame) {
  if (frame.opcode != proto::ZcAckReqnameBygid::kOpcode) {
    return std::nullopt;
  }
  proto::ZcAckReqnameBygid packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return std::nullopt;
  }
  return CharacterNameAnswer{.character_id = packet.CID, .name = core::Utf8FromCp1252(packet.name)};
}

}  // namespace ro::net
