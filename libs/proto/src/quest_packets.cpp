#include "ro/proto/quest_packets.h"

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"

namespace ro::proto {
namespace {

using core::ByteReader;
using core::ByteWriter;
using core::ErrorCode;

// La longueur d'un paquet à queue variable suit son opcode.
constexpr std::size_t kLengthOffset = 2;

// L'instant de début de quête, que le serveur écrit mal : le champ est sauté.
constexpr std::size_t kStartTimeSize = 4;

void PatchLength(ByteWriter& writer) {
  static_cast<void>(writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size())));
}

// Vrai si la trame fait exactement `size` octets et porte `opcode`.
[[nodiscard]] bool HasShape(std::span<const std::uint8_t> frame, std::size_t size,
                            std::uint16_t opcode) {
  if (frame.size() != size) {
    return false;
  }
  ByteReader reader(frame);
  return reader.U16() == opcode;
}

}  // namespace

core::Result<QuestAdded> DecodeQuestAdded(std::span<const std::uint8_t> frame) {
  if (!HasShape(frame, QuestAdded::kWireSize, QuestAdded::kOpcode)) {
    return core::Fail(ErrorCode::kUnexpectedEnd,
                      "quête ajoutée de longueur ou d'opcode inattendus");
  }
  ByteReader reader(frame);
  reader.Skip(sizeof(std::uint16_t));
  QuestAdded added;
  added.quest_id = reader.U32();
  added.active = reader.U8() != 0;
  reader.Skip(kStartTimeSize);
  added.expires_unix_s = reader.U32();
  const std::uint16_t hunt_count = reader.U16();
  if (hunt_count > QuestAdded::kMaxHunts) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "quête ajoutée portant trop de chasses");
  }
  added.hunts.resize(hunt_count);
  for (QuestAddedHunt& hunt : added.hunts) {
    hunt.hunt_id = reader.U32();
    hunt.mob_type = reader.U32();
    hunt.mob_id = reader.U32();
    hunt.level_min = reader.I16();
    hunt.level_max = reader.I16();
    hunt.killed = reader.I16();
    hunt.mob_name = std::string(reader.FixedString(QuestAddedHunt::kMobNameSize));
  }
  if (!reader.ok()) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "quête ajoutée tronquée");
  }
  return added;
}

std::vector<std::uint8_t> EncodeQuestAdded(const QuestAdded& added) {
  std::vector<std::uint8_t> frame;
  frame.reserve(QuestAdded::kWireSize);
  ByteWriter writer(frame);
  writer.U16(QuestAdded::kOpcode);
  writer.U32(added.quest_id);
  writer.U8(added.active ? 1 : 0);
  writer.U32(0);
  writer.U32(added.expires_unix_s);
  writer.U16(static_cast<std::uint16_t>(added.hunts.size()));
  for (const QuestAddedHunt& hunt : added.hunts) {
    writer.U32(hunt.hunt_id);
    writer.U32(hunt.mob_type);
    writer.U32(hunt.mob_id);
    writer.I16(hunt.level_min);
    writer.I16(hunt.level_max);
    writer.I16(hunt.killed);
    writer.FixedString(hunt.mob_name, QuestAddedHunt::kMobNameSize);
  }
  // Le reste du paquet fixe est du bourrage.
  if (frame.size() < QuestAdded::kWireSize) {
    frame.resize(QuestAdded::kWireSize, 0);
  }
  return frame;
}

core::Result<QuestHuntTotals> DecodeQuestHuntTotals(std::span<const std::uint8_t> frame) {
  ByteReader reader(frame);
  if (reader.U16() != QuestHuntTotals::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature, "trame d'un autre paquet que les totaux de chasse");
  }
  if (reader.U16() != frame.size() || frame.size() < QuestHuntTotals::kHeaderSize ||
      (frame.size() - QuestHuntTotals::kHeaderSize) % QuestHuntTotal::kWireSize != 0) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "totaux de chasse de longueur inattendue");
  }
  QuestHuntTotals totals;
  totals.entries.resize((frame.size() - QuestHuntTotals::kHeaderSize) / QuestHuntTotal::kWireSize);
  for (QuestHuntTotal& entry : totals.entries) {
    entry.quest_or_hunt_id = reader.U32();
    entry.mob_id = reader.U32();
    entry.total = reader.I16();
    entry.killed = reader.I16();
  }
  return totals;
}

std::vector<std::uint8_t> EncodeQuestHuntTotals(const QuestHuntTotals& totals) {
  std::vector<std::uint8_t> frame;
  ByteWriter writer(frame);
  writer.U16(QuestHuntTotals::kOpcode);
  writer.U16(0);
  for (const QuestHuntTotal& entry : totals.entries) {
    writer.U32(entry.quest_or_hunt_id);
    writer.U32(entry.mob_id);
    writer.I16(entry.total);
    writer.I16(entry.killed);
  }
  PatchLength(writer);
  return frame;
}

core::Result<QuestHuntProgress> DecodeQuestHuntProgress(std::span<const std::uint8_t> frame) {
  ByteReader reader(frame);
  if (reader.U16() != QuestHuntProgress::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature,
                      "trame d'un autre paquet que la progression des chasses");
  }
  if (reader.U16() != frame.size() || frame.size() < QuestHuntProgress::kHeaderSize ||
      (frame.size() - QuestHuntProgress::kHeaderSize) % QuestHuntCount::kWireSize != 0) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "progression des chasses de longueur inattendue");
  }
  const std::size_t entry_count =
      (frame.size() - QuestHuntProgress::kHeaderSize) / QuestHuntCount::kWireSize;
  if (reader.U16() != entry_count) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "progression des chasses dont le nombre ment");
  }
  QuestHuntProgress progress;
  progress.entries.resize(entry_count);
  for (QuestHuntCount& entry : progress.entries) {
    entry.quest_id = reader.U32();
    entry.hunt_id = reader.U32();
    entry.total = reader.I16();
    entry.killed = reader.I16();
  }
  return progress;
}

std::vector<std::uint8_t> EncodeQuestHuntProgress(const QuestHuntProgress& progress) {
  std::vector<std::uint8_t> frame;
  ByteWriter writer(frame);
  writer.U16(QuestHuntProgress::kOpcode);
  writer.U16(0);
  writer.U16(static_cast<std::uint16_t>(progress.entries.size()));
  for (const QuestHuntCount& entry : progress.entries) {
    writer.U32(entry.quest_id);
    writer.U32(entry.hunt_id);
    writer.I16(entry.total);
    writer.I16(entry.killed);
  }
  PatchLength(writer);
  return frame;
}

core::Result<QuestRemoved> DecodeQuestRemoved(std::span<const std::uint8_t> frame) {
  if (!HasShape(frame, QuestRemoved::kWireSize, QuestRemoved::kOpcode)) {
    return core::Fail(ErrorCode::kUnexpectedEnd,
                      "retrait de quête de longueur ou d'opcode inattendus");
  }
  ByteReader reader(frame);
  reader.Skip(sizeof(std::uint16_t));
  return QuestRemoved{.quest_id = reader.U32()};
}

std::vector<std::uint8_t> EncodeQuestRemoved(const QuestRemoved& removed) {
  std::vector<std::uint8_t> frame;
  frame.reserve(QuestRemoved::kWireSize);
  ByteWriter writer(frame);
  writer.U16(QuestRemoved::kOpcode);
  writer.U32(removed.quest_id);
  return frame;
}

core::Result<QuestActiveNotice> DecodeQuestActiveNotice(std::span<const std::uint8_t> frame) {
  if (!HasShape(frame, QuestActiveNotice::kWireSize, QuestActiveNotice::kOpcode)) {
    return core::Fail(ErrorCode::kUnexpectedEnd,
                      "état de quête de longueur ou d'opcode inattendus");
  }
  ByteReader reader(frame);
  reader.Skip(sizeof(std::uint16_t));
  QuestActiveNotice notice;
  notice.quest_id = reader.U32();
  notice.active = reader.U8() != 0;
  return notice;
}

std::vector<std::uint8_t> EncodeQuestActiveNotice(const QuestActiveNotice& notice) {
  std::vector<std::uint8_t> frame;
  frame.reserve(QuestActiveNotice::kWireSize);
  ByteWriter writer(frame);
  writer.U16(QuestActiveNotice::kOpcode);
  writer.U32(notice.quest_id);
  writer.U8(notice.active ? 1 : 0);
  return frame;
}

core::Result<QuestActiveRequest> DecodeQuestActiveRequest(std::span<const std::uint8_t> frame) {
  if (!HasShape(frame, QuestActiveRequest::kWireSize, QuestActiveRequest::kOpcode)) {
    return core::Fail(ErrorCode::kUnexpectedEnd,
                      "demande d'état de quête de longueur ou d'opcode inattendus");
  }
  ByteReader reader(frame);
  reader.Skip(sizeof(std::uint16_t));
  QuestActiveRequest request;
  request.quest_id = reader.U32();
  request.active = reader.U8() != 0;
  return request;
}

std::vector<std::uint8_t> EncodeQuestActiveRequest(const QuestActiveRequest& request) {
  std::vector<std::uint8_t> frame;
  frame.reserve(QuestActiveRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(QuestActiveRequest::kOpcode);
  writer.U32(request.quest_id);
  writer.U8(request.active ? 1 : 0);
  return frame;
}

}  // namespace ro::proto
