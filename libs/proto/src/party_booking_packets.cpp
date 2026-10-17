#include "ro/proto/party_booking_packets.h"

#include <array>
#include <string_view>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/packet_lengths.h"

#include "layout_holds.h"

namespace ro::proto {
namespace {

using core::ByteReader;
using core::ByteWriter;
using core::ErrorCode;

// ⚠ LE SERVEUR NE DÉCLARE QUE DEUX DÉCALAGES POUR LA PUBLICATION, le niveau et
// le lieu ; il lit les métiers au troisième, resté de l'inscription d'une
// version plus ancienne du même paquet, à l'octet 6.
constexpr std::string_view kRegisterHandler = "clif_parse_PartyBookingRegisterReq";
constexpr std::array<std::uint16_t, 2> kRegisterOffsets = {2, 4};
// Le niveau, le lieu, le métier, le premier rang, le compte demandé.
constexpr std::string_view kSearchHandler = "clif_parse_PartyBookingSearchReq";
constexpr std::array<std::uint16_t, 5> kSearchOffsets = {2, 4, 6, 8, 12};
constexpr std::string_view kDeleteHandler = "clif_parse_PartyBookingDeleteReq";
constexpr std::array<std::uint16_t, 0> kDeleteOffsets = {};
// Les six métiers, à la suite.
constexpr std::string_view kUpdateHandler = "clif_parse_PartyBookingUpdateReq";
constexpr std::array<std::uint16_t, 1> kUpdateOffsets = {2};

core::Result<std::vector<std::uint8_t>> RefuseLayout(std::string_view what) {
  return core::Fail(ErrorCode::kUnsupportedVersion,
                    std::string("la disposition écrite pour ") + std::string(what) +
                        " ne correspond plus à ce que le serveur déclare lire");
}

void WriteJobs(ByteWriter& writer, const PartyBookingJobs& jobs) {
  for (const std::int16_t job : jobs) {
    writer.I16(job);
  }
}

PartyBookingJobs ReadJobs(ByteReader& reader) {
  PartyBookingJobs jobs = {};
  for (std::int16_t& job : jobs) {
    job = reader.I16();
  }
  return jobs;
}

PartyBookingEntry ReadEntry(ByteReader& reader) {
  PartyBookingEntry entry;
  entry.index = reader.U32();
  entry.leader_name = std::string(reader.FixedString(kPartyBookingNameSize));
  entry.posted_unix = reader.U32();
  entry.level = reader.I16();
  entry.map_code = reader.I16();
  entry.jobs = ReadJobs(reader);
  return entry;
}

void WriteEntry(ByteWriter& writer, const PartyBookingEntry& entry) {
  writer.U32(entry.index);
  writer.FixedString(entry.leader_name, kPartyBookingNameSize);
  writer.U32(entry.posted_unix);
  writer.I16(entry.level);
  writer.I16(entry.map_code);
  WriteJobs(writer, entry.jobs);
}

// Ouvre une trame de longueur fixe : la longueur, puis l'opcode.
core::Result<ByteReader> OpenFixed(std::span<const std::uint8_t> frame, std::uint16_t opcode,
                                   std::size_t wire_size, std::string_view what) {
  if (frame.size() != wire_size) {
    return core::Fail(ErrorCode::kUnexpectedEnd, std::string(what) + " : longueur inattendue");
  }
  ByteReader reader(frame);
  if (reader.U16() != opcode) {
    return core::Fail(ErrorCode::kBadSignature, std::string(what) + " : trame d'un autre paquet");
  }
  return reader;
}

std::vector<std::uint8_t> Start(std::uint16_t opcode, std::size_t wire_size) {
  std::vector<std::uint8_t> frame;
  frame.reserve(wire_size);
  ByteWriter writer(frame);
  writer.U16(opcode);
  return frame;
}

}  // namespace

core::Result<std::vector<std::uint8_t>> Encode(const PartyBookingRegisterRequest& request) {
  if (!FixedLayoutHolds<PartyBookingRegisterRequest>(kRegisterHandler, kRegisterOffsets)) {
    return RefuseLayout("la publication d'une annonce de groupe");
  }
  std::vector<std::uint8_t> frame =
      Start(PartyBookingRegisterRequest::kOpcode, PartyBookingRegisterRequest::kWireSize);
  ByteWriter writer(frame);
  writer.I16(request.level);
  writer.I16(request.map_code);
  WriteJobs(writer, request.jobs);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const PartyBookingSearchRequest& request) {
  if (!FixedLayoutHolds<PartyBookingSearchRequest>(kSearchHandler, kSearchOffsets)) {
    return RefuseLayout("la recherche d'annonces de groupe");
  }
  std::vector<std::uint8_t> frame =
      Start(PartyBookingSearchRequest::kOpcode, PartyBookingSearchRequest::kWireSize);
  ByteWriter writer(frame);
  writer.I16(request.level);
  writer.I16(request.map_code);
  writer.I16(request.job);
  writer.U32(request.first_index);
  writer.I16(request.result_count);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const PartyBookingDeleteRequest& /*request*/) {
  if (!FixedLayoutHolds<PartyBookingDeleteRequest>(kDeleteHandler, kDeleteOffsets)) {
    return RefuseLayout("le retrait d'une annonce de groupe");
  }
  return Start(PartyBookingDeleteRequest::kOpcode, PartyBookingDeleteRequest::kWireSize);
}

core::Result<std::vector<std::uint8_t>> Encode(const PartyBookingUpdateRequest& request) {
  if (!FixedLayoutHolds<PartyBookingUpdateRequest>(kUpdateHandler, kUpdateOffsets)) {
    return RefuseLayout("la modification d'une annonce de groupe");
  }
  std::vector<std::uint8_t> frame =
      Start(PartyBookingUpdateRequest::kOpcode, PartyBookingUpdateRequest::kWireSize);
  ByteWriter writer(frame);
  WriteJobs(writer, request.jobs);
  return frame;
}

core::Result<PartyBookingRegisterAck> DecodePartyBookingRegisterAck(
    std::span<const std::uint8_t> frame) {
  core::Result<ByteReader> reader =
      OpenFixed(frame, PartyBookingRegisterAck::kOpcode, PartyBookingRegisterAck::kWireSize,
                "accusé de publication d'annonce");
  if (!reader) {
    return std::unexpected(reader.error());
  }
  return PartyBookingRegisterAck{.result = reader->U16()};
}

core::Result<PartyBookingSearchAck> DecodePartyBookingSearchAck(
    std::span<const std::uint8_t> frame) {
  ByteReader reader(frame);
  if (reader.U16() != PartyBookingSearchAck::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature,
                      "page d'annonces de groupe : trame d'un autre paquet");
  }
  const std::uint16_t length = reader.U16();
  if (!reader.ok() || length != frame.size() || length < PartyBookingSearchAck::kHeaderSize) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "page d'annonces de groupe tronquée");
  }
  const std::size_t body = length - PartyBookingSearchAck::kHeaderSize;
  if (body % PartyBookingEntry::kWireSize != 0) {
    return core::Fail(ErrorCode::kCorruptData,
                      "page d'annonces de groupe : un reste qui n'est pas une annonce entière");
  }
  PartyBookingSearchAck ack;
  ack.more_results = reader.U8() != 0;
  const std::size_t count = body / PartyBookingEntry::kWireSize;
  ack.entries.reserve(count);
  for (std::size_t rank = 0; rank < count; ++rank) {
    ack.entries.push_back(ReadEntry(reader));
  }
  if (!reader.ok()) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "page d'annonces de groupe tronquée");
  }
  return ack;
}

core::Result<PartyBookingDeleteAck> DecodePartyBookingDeleteAck(
    std::span<const std::uint8_t> frame) {
  core::Result<ByteReader> reader =
      OpenFixed(frame, PartyBookingDeleteAck::kOpcode, PartyBookingDeleteAck::kWireSize,
                "accusé de retrait d'annonce");
  if (!reader) {
    return std::unexpected(reader.error());
  }
  return PartyBookingDeleteAck{.result = reader->U16()};
}

core::Result<PartyBookingInsertNotice> DecodePartyBookingInsertNotice(
    std::span<const std::uint8_t> frame) {
  core::Result<ByteReader> reader =
      OpenFixed(frame, PartyBookingInsertNotice::kOpcode, PartyBookingInsertNotice::kWireSize,
                "avis d'annonce publiée");
  if (!reader) {
    return std::unexpected(reader.error());
  }
  return PartyBookingInsertNotice{.entry = ReadEntry(*reader)};
}

core::Result<PartyBookingUpdateNotice> DecodePartyBookingUpdateNotice(
    std::span<const std::uint8_t> frame) {
  core::Result<ByteReader> reader =
      OpenFixed(frame, PartyBookingUpdateNotice::kOpcode, PartyBookingUpdateNotice::kWireSize,
                "avis d'annonce modifiée");
  if (!reader) {
    return std::unexpected(reader.error());
  }
  PartyBookingUpdateNotice notice;
  notice.index = reader->U32();
  notice.jobs = ReadJobs(*reader);
  return notice;
}

core::Result<PartyBookingDeleteNotice> DecodePartyBookingDeleteNotice(
    std::span<const std::uint8_t> frame) {
  core::Result<ByteReader> reader =
      OpenFixed(frame, PartyBookingDeleteNotice::kOpcode, PartyBookingDeleteNotice::kWireSize,
                "avis d'annonce retirée");
  if (!reader) {
    return std::unexpected(reader.error());
  }
  return PartyBookingDeleteNotice{.index = reader->U32()};
}

std::vector<std::uint8_t> EncodePartyBookingRegisterAck(const PartyBookingRegisterAck& ack) {
  std::vector<std::uint8_t> frame =
      Start(PartyBookingRegisterAck::kOpcode, PartyBookingRegisterAck::kWireSize);
  ByteWriter writer(frame);
  writer.U16(ack.result);
  return frame;
}

std::vector<std::uint8_t> EncodePartyBookingSearchAck(const PartyBookingSearchAck& ack) {
  const std::size_t total =
      PartyBookingSearchAck::kHeaderSize + ack.entries.size() * PartyBookingEntry::kWireSize;
  std::vector<std::uint8_t> frame = Start(PartyBookingSearchAck::kOpcode, total);
  ByteWriter writer(frame);
  writer.U16(static_cast<std::uint16_t>(total));
  writer.U8(ack.more_results ? 1 : 0);
  for (const PartyBookingEntry& entry : ack.entries) {
    WriteEntry(writer, entry);
  }
  return frame;
}

std::vector<std::uint8_t> EncodePartyBookingDeleteAck(const PartyBookingDeleteAck& ack) {
  std::vector<std::uint8_t> frame =
      Start(PartyBookingDeleteAck::kOpcode, PartyBookingDeleteAck::kWireSize);
  ByteWriter writer(frame);
  writer.U16(ack.result);
  return frame;
}

std::vector<std::uint8_t> EncodePartyBookingInsertNotice(const PartyBookingInsertNotice& notice) {
  std::vector<std::uint8_t> frame =
      Start(PartyBookingInsertNotice::kOpcode, PartyBookingInsertNotice::kWireSize);
  ByteWriter writer(frame);
  WriteEntry(writer, notice.entry);
  return frame;
}

std::vector<std::uint8_t> EncodePartyBookingUpdateNotice(const PartyBookingUpdateNotice& notice) {
  std::vector<std::uint8_t> frame =
      Start(PartyBookingUpdateNotice::kOpcode, PartyBookingUpdateNotice::kWireSize);
  ByteWriter writer(frame);
  writer.U32(notice.index);
  WriteJobs(writer, notice.jobs);
  return frame;
}

std::vector<std::uint8_t> EncodePartyBookingDeleteNotice(const PartyBookingDeleteNotice& notice) {
  std::vector<std::uint8_t> frame =
      Start(PartyBookingDeleteNotice::kOpcode, PartyBookingDeleteNotice::kWireSize);
  ByteWriter writer(frame);
  writer.U32(notice.index);
  return frame;
}

}  // namespace ro::proto
