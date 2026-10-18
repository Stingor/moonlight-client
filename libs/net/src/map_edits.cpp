// Les éditions de carte que le serveur tient. Voir l'en-tête.

#include "ro/net/map_edits.h"

#include <format>

#include <zlib.h>

#include "ro/core/text.h"
#include "ro/proto/map_packets.h"

#include "frame_apply.h"

namespace ro::net {

core::Result<std::vector<std::uint8_t>> ZipMapEdit(std::string_view text) {
  if (text.empty() || text.size() > proto::kMapEditMostTextBytes) {
    return core::Fail(
        core::ErrorCode::kInvalidArgument,
        std::format("document d'édition de {} octets, hors des plafonds", text.size()));
  }
  uLongf zipped_size = compressBound(static_cast<uLong>(text.size()));
  std::vector<std::uint8_t> zipped(zipped_size);
  if (compress2(zipped.data(), &zipped_size, reinterpret_cast<const Bytef*>(text.data()),
                static_cast<uLong>(text.size()), Z_BEST_COMPRESSION) != Z_OK) {
    return core::Fail(core::ErrorCode::kCorruptData, "le document d'édition ne se compresse pas");
  }
  zipped.resize(zipped_size);
  if (zipped.size() > proto::kMapEditMostZippedBytes) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      std::format("document d'édition de {} octets compressés, au-delà de {}",
                                  zipped.size(), proto::kMapEditMostZippedBytes));
  }
  return zipped;
}

core::Result<std::string> UnzipMapEdit(std::span<const std::uint8_t> zipped,
                                       std::uint32_t text_size) {
  if (zipped.empty() || zipped.size() > proto::kMapEditMostZippedBytes || text_size == 0 ||
      text_size > proto::kMapEditMostTextBytes) {
    return core::Fail(core::ErrorCode::kCorruptData, "document d'édition hors des plafonds");
  }
  std::string text(text_size, '\0');
  uLongf produced = text_size;
  if (uncompress(reinterpret_cast<Bytef*>(text.data()), &produced, zipped.data(),
                 static_cast<uLong>(zipped.size())) != Z_OK ||
      produced != text_size) {
    return core::Fail(
        core::ErrorCode::kCorruptData,
        std::format("le document d'édition ne donne pas les {} octets annoncés", text_size));
  }
  return text;
}

FrameOutcome MapEditInbox::Apply(const Frame& frame) {
  switch (frame.opcode) {
    case proto::ZcMoonclientMapEditRevision::kOpcode:
      return Decoded<proto::ZcMoonclientMapEditRevision>(frame, [this](const auto& packet) {
        announces_.push_back(MapEditAnnounce{.map = packet.map,
                                             .revision = packet.revision,
                                             .can_publish = packet.can_publish != 0});
        return FrameOutcome::kApplied;
      });
    case proto::ZcMoonclientMapEditResult::kOpcode:
      return Decoded<proto::ZcMoonclientMapEditResult>(frame, [this](const auto& packet) {
        if (packet.result > static_cast<std::uint8_t>(MapEditVerdictKind::kStorage)) {
          return FrameOutcome::kIgnored;
        }
        verdicts_.push_back(MapEditVerdict{.map = packet.map,
                                           .kind = static_cast<MapEditVerdictKind>(packet.result),
                                           .revision = packet.revision});
        return FrameOutcome::kApplied;
      });
    case proto::ZcMoonclientMapEditDoc::kOpcode:
      break;
    default:
      return FrameOutcome::kUnhandled;
  }
  std::optional<proto::MapEditChunk> chunk = proto::DecodeMapEditDoc(frame.bytes);
  if (!chunk.has_value()) {
    return FrameOutcome::kUndecodable;
  }
  switch (assembler_.Feed(std::move(*chunk))) {
    case proto::MapEditAssembler::Step::kPending:
      return FrameOutcome::kApplied;
    case proto::MapEditAssembler::Step::kRefused:
      refusals_.push_back(assembler_.refusal());
      return FrameOutcome::kIgnored;
    case proto::MapEditAssembler::Step::kComplete:
      break;
  }
  proto::AssembledMapEdit assembled = assembler_.Take();
  core::Result<std::string> text = UnzipMapEdit(assembled.zipped, assembled.text_size);
  if (!text) {
    refusals_.push_back(std::format("révision {} de {} : {}", assembled.revision, assembled.map,
                                    core::Describe(text.error())));
    return FrameOutcome::kIgnored;
  }
  documents_.push_back(ServedMapEdit{.map = std::move(assembled.map),
                                     .revision = assembled.revision,
                                     .author = core::Utf8FromWire(assembled.author),
                                     .text = std::move(*text)});
  return FrameOutcome::kApplied;
}

void MapEditInbox::Reset() {
  assembler_.Reset();
  announces_.clear();
  verdicts_.clear();
  documents_.clear();
  refusals_.clear();
}

core::Result<void> PublishMapEdit(Connection& link, std::string_view map,
                                  std::uint32_t base_revision, std::string_view text) {
  core::Result<std::vector<std::uint8_t>> zipped = ZipMapEdit(text);
  if (!zipped) {
    return std::unexpected(zipped.error());
  }
  const std::vector<std::vector<std::uint8_t>> frames =
      proto::EncodeMapEditPut(map, base_revision, static_cast<std::uint32_t>(text.size()), *zipped);
  if (frames.empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      std::format("document d'édition de « {} » non encodable", map));
  }
  for (const std::vector<std::uint8_t>& frame : frames) {
    link.Send(frame);
  }
  return {};
}

core::Result<void> RequestMapEdit(Connection& link, std::string_view map) {
  if (!proto::IsMapEditName(map)) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      std::format("« {} » ne nomme pas une carte", map));
  }
  proto::CzMoonclientMapEditGet request;
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonclientMapEditGet::kWireSize);
  request.map = std::string(map);
  const std::vector<std::uint8_t> bytes = proto::Encode(request);
  if (bytes.empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "demande d'édition non encodable");
  }
  link.Send(bytes);
  return {};
}

}  // namespace ro::net
