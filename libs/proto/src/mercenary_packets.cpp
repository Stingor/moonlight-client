// Voir l'en-tête.

#include "ro/proto/mercenary_packets.h"

#include <array>
#include <string_view>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"

#include "layout_holds.h"

namespace ro::proto {
namespace {

constexpr std::string_view kCommandHandler = "clif_parse_mercenary_action";
// La commande, seul champ lu.
constexpr std::array<std::uint16_t, 1> kCommandOffsets = {2};

// L'opcode lu, puis la longueur : la trame doit être celle qu'on attend.
[[nodiscard]] bool Opens(core::ByteReader& reader, std::uint16_t opcode) {
  return reader.U16() == opcode && reader.ok();
}

}  // namespace

core::Result<MercenaryInfo> DecodeMercenaryInfo(std::span<const std::uint8_t> frame) {
  core::ByteReader reader(frame);
  if (!Opens(reader, MercenaryInfo::kOpcode)) {
    return core::Fail(core::ErrorCode::kBadSignature, "trame d'un autre paquet que la fiche");
  }
  if (frame.size() != MercenaryInfo::kWireSize) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "fiche du mercenaire tronquée");
  }
  MercenaryInfo info;
  info.actor_id = reader.U32();
  info.atk = reader.U16();
  info.matk = reader.U16();
  info.hit = reader.U16();
  info.crit = reader.U16();
  info.def = reader.I16();
  info.mdef = reader.I16();
  info.flee = reader.U16();
  info.amotion_ms = reader.U16();
  info.name = std::string(reader.FixedString(MercenaryInfo::kNameSize));
  info.level = reader.U16();
  info.hp = reader.I32();
  info.max_hp = reader.I32();
  info.sp = reader.I32();
  info.max_sp = reader.I32();
  info.expire_unix_s = reader.U32();
  info.faith = reader.I16();
  info.calls = reader.U32();
  info.kills = reader.U32();
  info.range_cells = reader.U16();
  if (!reader.ok()) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "fiche du mercenaire tronquée");
  }
  return info;
}

core::Result<MercenaryStatus> DecodeMercenaryStatus(std::span<const std::uint8_t> frame) {
  core::ByteReader reader(frame);
  if (!Opens(reader, MercenaryStatus::kOpcode)) {
    return core::Fail(core::ErrorCode::kBadSignature,
                      "trame d'un autre paquet que la valeur du mercenaire");
  }
  if (frame.size() != MercenaryStatus::kWireSize) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "valeur du mercenaire tronquée");
  }
  MercenaryStatus status;
  status.variable = reader.U16();
  status.value = reader.I32();
  if (!reader.ok()) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "valeur du mercenaire tronquée");
  }
  return status;
}

core::Result<MercenarySkillList> DecodeMercenarySkillList(std::span<const std::uint8_t> frame) {
  core::ByteReader reader(frame);
  if (!Opens(reader, MercenarySkillList::kOpcode)) {
    return core::Fail(core::ErrorCode::kBadSignature,
                      "trame d'un autre paquet que les compétences du mercenaire");
  }
  const std::uint16_t length = reader.U16();
  if (!reader.ok() || length != frame.size() || length < MercenarySkillList::kHeaderSize ||
      (frame.size() - MercenarySkillList::kHeaderSize) % MercenarySkillList::kEntrySize != 0) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "compétences du mercenaire tronquées");
  }
  MercenarySkillList list;
  list.skills.resize((frame.size() - MercenarySkillList::kHeaderSize) /
                     MercenarySkillList::kEntrySize);
  for (MercenarySkillEntry& skill : list.skills) {
    skill.skill_id = reader.U16();
    skill.targeting = reader.U32();
    skill.level = reader.U16();
    skill.sp_cost = reader.U16();
    skill.range_cells = reader.U16();
    skill.name = std::string(reader.FixedString(MercenarySkillList::kNameSize));
    skill.upgradable = reader.U8() != 0;
  }
  if (!reader.ok()) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "compétences du mercenaire tronquées");
  }
  return list;
}

core::Result<std::vector<std::uint8_t>> EncodeMercenaryCommand(const MercenaryCommand& command) {
  if (!FixedLayoutHolds<MercenaryCommand>(kCommandHandler, kCommandOffsets)) {
    return core::Fail(core::ErrorCode::kUnsupportedVersion,
                      "la disposition écrite pour l'ordre au mercenaire ne correspond plus à ce "
                      "que le serveur déclare lire");
  }
  std::vector<std::uint8_t> frame;
  core::ByteWriter writer(frame);
  writer.U16(MercenaryCommand::kOpcode);
  writer.U8(command.command);
  return frame;
}

std::vector<std::uint8_t> EncodeMercenaryInfo(const MercenaryInfo& info) {
  std::vector<std::uint8_t> frame;
  core::ByteWriter writer(frame);
  writer.U16(MercenaryInfo::kOpcode);
  writer.U32(info.actor_id);
  writer.U16(info.atk);
  writer.U16(info.matk);
  writer.U16(info.hit);
  writer.U16(info.crit);
  writer.I16(info.def);
  writer.I16(info.mdef);
  writer.U16(info.flee);
  writer.U16(info.amotion_ms);
  writer.FixedString(info.name, MercenaryInfo::kNameSize);
  writer.U16(info.level);
  writer.I32(info.hp);
  writer.I32(info.max_hp);
  writer.I32(info.sp);
  writer.I32(info.max_sp);
  writer.U32(info.expire_unix_s);
  writer.I16(info.faith);
  writer.U32(info.calls);
  writer.U32(info.kills);
  writer.U16(info.range_cells);
  return frame;
}

std::vector<std::uint8_t> EncodeMercenaryStatus(const MercenaryStatus& status) {
  std::vector<std::uint8_t> frame;
  core::ByteWriter writer(frame);
  writer.U16(MercenaryStatus::kOpcode);
  writer.U16(status.variable);
  writer.I32(status.value);
  return frame;
}

std::vector<std::uint8_t> EncodeMercenarySkillList(const MercenarySkillList& list) {
  std::vector<std::uint8_t> frame;
  core::ByteWriter writer(frame);
  writer.U16(MercenarySkillList::kOpcode);
  writer.U16(static_cast<std::uint16_t>(MercenarySkillList::kHeaderSize +
                                        list.skills.size() * MercenarySkillList::kEntrySize));
  for (const MercenarySkillEntry& skill : list.skills) {
    writer.U16(skill.skill_id);
    writer.U32(skill.targeting);
    writer.U16(skill.level);
    writer.U16(skill.sp_cost);
    writer.U16(skill.range_cells);
    writer.FixedString(skill.name, MercenarySkillList::kNameSize);
    writer.U8(skill.upgradable ? 1 : 0);
  }
  return frame;
}

}  // namespace ro::proto
