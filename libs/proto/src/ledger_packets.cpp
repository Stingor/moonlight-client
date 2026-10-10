#include "ro/proto/ledger_packets.h"

#include <string_view>
#include <utility>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"

namespace ro::proto {
namespace {

using core::ByteReader;
using core::ByteWriter;
using core::ErrorCode;

// La longueur annoncée, lue après l'opcode d'un paquet à queue variable.
[[nodiscard]] bool LengthMatches(ByteReader& reader, std::span<const std::uint8_t> frame) {
  return reader.U16() == frame.size() && reader.ok();
}

// La longueur d'un paquet à queue variable suit son opcode.
constexpr std::size_t kLengthOffset = 2;

void PatchLength(ByteWriter& writer) {
  static_cast<void>(writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size())));
}

}  // namespace

core::Result<AccountIdNotice> DecodeAccountIdNotice(std::span<const std::uint8_t> frame) {
  if (frame.size() != AccountIdNotice::kWireSize) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "identifiant de compte de longueur inattendue");
  }
  ByteReader reader(frame);
  if (reader.U16() != AccountIdNotice::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature,
                      "trame d'un autre paquet que l'identifiant de compte");
  }
  return AccountIdNotice{.account_id = reader.U32()};
}

std::vector<std::uint8_t> EncodeAccountIdNotice(const AccountIdNotice& notice) {
  std::vector<std::uint8_t> frame;
  frame.reserve(AccountIdNotice::kWireSize);
  ByteWriter writer(frame);
  writer.U16(AccountIdNotice::kOpcode);
  writer.U32(notice.account_id);
  return frame;
}

core::Result<ExpNotice> DecodeExpNotice(std::span<const std::uint8_t> frame) {
  if (frame.size() != ExpNotice::kWireSize) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "gain d'expérience de longueur inattendue");
  }
  ByteReader reader(frame);
  if (reader.U16() != ExpNotice::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature, "trame d'un autre paquet que le gain d'expérience");
  }
  ExpNotice notice;
  notice.account_id = reader.U32();
  notice.amount = reader.I64();
  notice.var_id = reader.U16();
  notice.exp_type = reader.U16();
  return notice;
}

std::vector<std::uint8_t> EncodeExpNotice(const ExpNotice& notice) {
  std::vector<std::uint8_t> frame;
  frame.reserve(ExpNotice::kWireSize);
  ByteWriter writer(frame);
  writer.U16(ExpNotice::kOpcode);
  writer.U32(notice.account_id);
  writer.I64(notice.amount);
  writer.U16(notice.var_id);
  writer.U16(notice.exp_type);
  return frame;
}

core::Result<QuestList> DecodeQuestList(std::span<const std::uint8_t> frame) {
  ByteReader reader(frame);
  if (reader.U16() != QuestList::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature, "trame d'un autre paquet que la liste des quêtes");
  }
  if (!LengthMatches(reader, frame)) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "liste des quêtes de longueur inattendue");
  }
  const std::int32_t count = reader.I32();
  if (!reader.ok() || count < 0) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "liste des quêtes tronquée");
  }
  QuestList list;
  for (std::int32_t quest_rank = 0; quest_rank < count; ++quest_rank) {
    if (reader.remaining() < QuestEntry::kHeadSize) {
      return core::Fail(ErrorCode::kUnexpectedEnd, "liste des quêtes tronquée");
    }
    QuestEntry quest;
    quest.quest_id = reader.U32();
    quest.active = reader.U8() != 0;
    quest.started_unix_s = reader.U32();
    quest.expires_unix_s = reader.U32();
    const std::uint16_t hunt_count = reader.U16();
    if (reader.remaining() < static_cast<std::size_t>(hunt_count) * QuestHuntEntry::kWireSize) {
      return core::Fail(ErrorCode::kUnexpectedEnd, "chasses d'une quête tronquées");
    }
    quest.hunts.resize(hunt_count);
    for (QuestHuntEntry& hunt : quest.hunts) {
      hunt.hunt_id = reader.U32();
      hunt.mob_type = reader.U32();
      hunt.mob_id = reader.U32();
      hunt.level_min = reader.I16();
      hunt.level_max = reader.I16();
      hunt.killed = reader.I16();
      hunt.total = reader.I16();
      hunt.mob_name = std::string(reader.FixedString(QuestHuntEntry::kMobNameSize));
    }
    list.quests.push_back(std::move(quest));
  }
  // Une queue qui déborde ne se lit pas à moitié : c'est une autre disposition.
  if (!reader.ok() || reader.remaining() != 0) {
    return core::Fail(ErrorCode::kUnexpectedEnd,
                      "liste des quêtes dont la queue ne se découpe pas en quêtes entières");
  }
  return list;
}

std::vector<std::uint8_t> EncodeQuestList(const QuestList& list) {
  std::vector<std::uint8_t> frame;
  ByteWriter writer(frame);
  writer.U16(QuestList::kOpcode);
  writer.U16(0);
  writer.I32(static_cast<std::int32_t>(list.quests.size()));
  for (const QuestEntry& quest : list.quests) {
    writer.U32(quest.quest_id);
    writer.U8(quest.active ? 1 : 0);
    writer.U32(quest.started_unix_s);
    writer.U32(quest.expires_unix_s);
    writer.U16(static_cast<std::uint16_t>(quest.hunts.size()));
    for (const QuestHuntEntry& hunt : quest.hunts) {
      writer.U32(hunt.hunt_id);
      writer.U32(hunt.mob_type);
      writer.U32(hunt.mob_id);
      writer.I16(hunt.level_min);
      writer.I16(hunt.level_max);
      writer.I16(hunt.killed);
      writer.I16(hunt.total);
      writer.FixedString(hunt.mob_name, QuestHuntEntry::kMobNameSize);
    }
  }
  PatchLength(writer);
  return frame;
}

core::Result<EquipSwitchList> DecodeEquipSwitchList(std::span<const std::uint8_t> frame) {
  ByteReader reader(frame);
  if (reader.U16() != EquipSwitchList::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature,
                      "trame d'un autre paquet que le jeu d'équipement de rechange");
  }
  if (!LengthMatches(reader, frame) ||
      (frame.size() - EquipSwitchList::kHeaderSize) % EquipSwitchEntry::kWireSize != 0) {
    return core::Fail(ErrorCode::kUnexpectedEnd,
                      "jeu d'équipement de rechange de longueur inattendue");
  }
  EquipSwitchList list;
  list.entries.resize((frame.size() - EquipSwitchList::kHeaderSize) / EquipSwitchEntry::kWireSize);
  for (EquipSwitchEntry& entry : list.entries) {
    entry.client_slot = reader.U16();
    entry.location = reader.U32();
  }
  return list;
}

std::vector<std::uint8_t> EncodeEquipSwitchList(const EquipSwitchList& list) {
  std::vector<std::uint8_t> frame;
  ByteWriter writer(frame);
  writer.U16(EquipSwitchList::kOpcode);
  writer.U16(0);
  for (const EquipSwitchEntry& entry : list.entries) {
    writer.U16(entry.client_slot);
    writer.U32(entry.location);
  }
  PatchLength(writer);
  return frame;
}

}  // namespace ro::proto
