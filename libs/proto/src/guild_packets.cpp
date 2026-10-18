#include "ro/proto/guild_packets.h"

#include <array>
#include <string_view>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/proto/manual_packets.h"

#include "layout_holds.h"
#include "opcode_only.h"

namespace ro::proto {
namespace {

using core::ByteReader;
using core::ByteWriter;
using core::ErrorCode;

// Le gestionnaire du serveur qui lit chaque paquet, et les décalages que sa
// table déclare. C'est LUI qui identifie le paquet, pas son opcode.
constexpr std::string_view kMenuHandler = "clif_parse_GuildRequestInfo";
constexpr std::array<std::uint16_t, 1> kMenuOffsets = {2};
constexpr std::string_view kMenuInterfaceHandler = "clif_parse_GuildCheckMaster";
constexpr std::string_view kNoticeHandler = "clif_parse_GuildChangeNotice";
constexpr std::array<std::uint16_t, 3> kNoticeOffsets = {2, 6, 66};
constexpr std::string_view kPositionsHandler = "clif_parse_GuildChangePositionInfo";
// La longueur en deux, les entrées en quatre.
constexpr std::array<std::uint16_t, 2> kPositionsOffsets = {2, 4};
constexpr std::string_view kCreateHandler = "clif_parse_CreateGuild";
constexpr std::array<std::uint16_t, 2> kCreateOffsets = {2, 6};
constexpr std::string_view kRelationHandler = "clif_parse_GuildDelAlliance";
constexpr std::array<std::uint16_t, 2> kRelationOffsets = {2, 6};
constexpr std::string_view kAllianceHandler = "clif_parse_GuildRequestAlliance";
constexpr std::array<std::uint16_t, 3> kAllianceOffsets = {2, 6, 10};
constexpr std::string_view kAllianceAnswerHandler = "clif_parse_GuildReplyAlliance";
constexpr std::array<std::uint16_t, 2> kAllianceAnswerOffsets = {2, 6};
constexpr std::string_view kOppositionHandler = "clif_parse_GuildOpposition";
constexpr std::array<std::uint16_t, 1> kOppositionOffsets = {2};

// Ce que porte chaque entrée reçue.
constexpr std::size_t kPositionNameEntry = 28;
constexpr std::size_t kPositionNameSize = 24;
// L'en-tête d'une trame variable : l'opcode, puis sa longueur.
constexpr std::size_t kVariableHeader = 4;
// L'en-tête de l'image d'emblème : en plus, la guilde et la version.
constexpr std::size_t kEmblemImageHeader = 12;
// Au plus vingt postes : le serveur n'en envoie jamais davantage, et une trame
// qui en annoncerait plus se refuse plutôt que de se lire loin.
constexpr std::size_t kPositionsMost = 20;

core::Error LayoutDrift() {
  return core::Error{ErrorCode::kUnsupportedVersion,
                     "la disposition écrite pour ce paquet de guilde ne correspond plus à ce "
                     "que le serveur déclare lire"};
}

// Relit l'en-tête d'une trame variable et rend sa longueur annoncée, bornée à
// ce que la trame porte vraiment.
[[nodiscard]] core::Result<std::size_t> VariableLength(std::span<const std::uint8_t> frame,
                                                       std::uint16_t opcode) {
  ByteReader reader(frame);
  const std::uint16_t read_opcode = reader.U16();
  const std::uint16_t length = reader.U16();
  if (!reader.ok() || read_opcode != opcode) {
    return core::Fail(ErrorCode::kBadSignature, "trame de guilde d'un autre paquet");
  }
  if (length < kVariableHeader || length > frame.size()) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "trame de guilde tronquée");
  }
  return static_cast<std::size_t>(length);
}

}  // namespace

core::Result<std::vector<std::uint8_t>> Encode(const GuildMenuRequest& request) {
  if (!FixedLayoutHolds<GuildMenuRequest>(kMenuHandler, kMenuOffsets)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(GuildMenuRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(GuildMenuRequest::kOpcode);
  writer.I32(request.part);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const GuildMenuInterfaceRequest& /*request*/) {
  if (!FixedLayoutHolds<GuildMenuInterfaceRequest>(kMenuInterfaceHandler,
                                                   std::array<std::uint16_t, 0>{})) {
    return std::unexpected(LayoutDrift());
  }
  return OpcodeOnlyFrame<GuildMenuInterfaceRequest>();
}

core::Result<std::vector<std::uint8_t>> Encode(const GuildNoticeRequest& request) {
  if (!FixedLayoutHolds<GuildNoticeRequest>(kNoticeHandler, kNoticeOffsets)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(GuildNoticeRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(GuildNoticeRequest::kOpcode);
  writer.U32(request.guild_id);
  writer.FixedString(request.subject, GuildNoticeRequest::kSubjectSize);
  writer.FixedString(request.notice, GuildNoticeRequest::kNoticeSize);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const GuildPositionsRequest& request) {
  if (!MatchesServerLayout(GuildPositionsRequest::kOpcode, kPositionsHandler, kPositionsOffsets)) {
    return std::unexpected(LayoutDrift());
  }
  if (request.entries.empty() || request.entries.size() > kPositionsMost) {
    return core::Fail(ErrorCode::kInvalidArgument, "entre un et vingt postes par envoi");
  }
  std::vector<std::uint8_t> frame;
  const std::size_t total =
      GuildPositionsRequest::kHeaderSize + request.entries.size() * GuildPositionEntry::kWireSize;
  frame.reserve(total);
  ByteWriter writer(frame);
  writer.U16(GuildPositionsRequest::kOpcode);
  writer.U16(static_cast<std::uint16_t>(total));
  for (const GuildPositionEntry& entry : request.entries) {
    writer.I32(entry.position);
    writer.I32(entry.rights);
    writer.I32(entry.ranking);
    writer.I32(entry.pay_rate);
    writer.FixedString(entry.name, GuildPositionEntry::kNameSize);
  }
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const GuildCreateRequest& request) {
  if (!FixedLayoutHolds<GuildCreateRequest>(kCreateHandler, kCreateOffsets)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(GuildCreateRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(GuildCreateRequest::kOpcode);
  writer.U32(request.character_id);
  writer.FixedString(request.name, GuildCreateRequest::kNameSize);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const GuildRelationDeleteRequest& request) {
  if (!FixedLayoutHolds<GuildRelationDeleteRequest>(kRelationHandler, kRelationOffsets)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(GuildRelationDeleteRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(GuildRelationDeleteRequest::kOpcode);
  writer.U32(request.guild_id);
  writer.U32(request.relation);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const GuildAllianceRequest& request) {
  if (!FixedLayoutHolds<GuildAllianceRequest>(kAllianceHandler, kAllianceOffsets)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(GuildAllianceRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(GuildAllianceRequest::kOpcode);
  writer.U32(request.target_account_id);
  writer.U32(request.own_account_id);
  writer.U32(request.own_char_id);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const GuildAllianceAnswer& request) {
  if (!FixedLayoutHolds<GuildAllianceAnswer>(kAllianceAnswerHandler, kAllianceAnswerOffsets)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(GuildAllianceAnswer::kWireSize);
  ByteWriter writer(frame);
  writer.U16(GuildAllianceAnswer::kOpcode);
  writer.U32(request.inviter_account_id);
  writer.U32(request.answer);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const GuildOppositionRequest& request) {
  if (!FixedLayoutHolds<GuildOppositionRequest>(kOppositionHandler, kOppositionOffsets)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(GuildOppositionRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(GuildOppositionRequest::kOpcode);
  writer.U32(request.target_account_id);
  return frame;
}

core::Result<std::vector<GuildPositionName>> DecodeGuildPositionNames(
    std::span<const std::uint8_t> frame) {
  const auto length = VariableLength(frame, kGuildPositionNamesOpcode);
  if (!length) {
    return std::unexpected(length.error());
  }
  std::vector<GuildPositionName> names;
  ByteReader reader(frame.first(*length));
  reader.Skip(kVariableHeader);
  while (reader.remaining() >= kPositionNameEntry && names.size() < kPositionsMost) {
    GuildPositionName entry;
    entry.position = reader.I32();
    entry.name = std::string(reader.FixedString(kPositionNameSize));
    names.push_back(std::move(entry));
  }
  return names;
}

core::Result<std::vector<GuildPositionEntry>> DecodeGuildPositionsChanged(
    std::span<const std::uint8_t> frame) {
  const auto length = VariableLength(frame, kGuildPositionsChangedOpcode);
  if (!length) {
    return std::unexpected(length.error());
  }
  std::vector<GuildPositionEntry> entries;
  ByteReader reader(frame.first(*length));
  reader.Skip(kVariableHeader);
  while (reader.remaining() >= GuildPositionEntry::kWireSize && entries.size() < kPositionsMost) {
    GuildPositionEntry entry;
    entry.position = reader.I32();
    entry.rights = reader.I32();
    entry.ranking = reader.I32();
    entry.pay_rate = reader.I32();
    entry.name = std::string(reader.FixedString(GuildPositionEntry::kNameSize));
    entries.push_back(std::move(entry));
  }
  return entries;
}

core::Result<GuildEmblemImage> DecodeGuildEmblemImage(std::span<const std::uint8_t> frame) {
  const auto length = VariableLength(frame, kGuildEmblemImageOpcode);
  if (!length) {
    return std::unexpected(length.error());
  }
  if (*length < kEmblemImageHeader) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "image d'emblème sans en-tête");
  }
  ByteReader reader(frame.first(*length));
  reader.Skip(kVariableHeader);
  GuildEmblemImage image;
  image.guild_id = reader.I32();
  image.version = reader.I32();
  const std::span<const std::uint8_t> rest = reader.Bytes(*length - kEmblemImageHeader);
  image.compressed.assign(rest.begin(), rest.end());
  return image;
}

}  // namespace ro::proto
