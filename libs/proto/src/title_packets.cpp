#include "ro/proto/title_packets.h"

#include <array>
#include <string_view>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"

#include "layout_holds.h"

namespace ro::proto {
namespace {

using core::ByteReader;
using core::ByteWriter;
using core::ErrorCode;

// ⚠ AUCUN DÉCALAGE DÉCLARÉ : le serveur enregistre ce paquet sans en lister les
// champs, et lit le titre lui-même, en deux. Le recoupement porte donc sur le
// gestionnaire et la longueur.
constexpr std::string_view kChangeTitleHandler = "clif_parse_change_title";
constexpr std::array<std::uint16_t, 0> kChangeTitleOffsets = {};

// Même cas pour la réclamation d'un succès : le numéro se lit à la main.
constexpr std::string_view kRewardHandler = "clif_parse_AchievementCheckReward";
constexpr std::array<std::uint16_t, 0> kRewardOffsets = {};

}  // namespace

core::Result<std::vector<std::uint8_t>> Encode(const AchievementRewardRequest& request) {
  if (!FixedLayoutHolds<AchievementRewardRequest>(kRewardHandler, kRewardOffsets)) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la disposition écrite pour la réclamation d'un succès ne correspond plus "
                      "à ce que le serveur déclare lire");
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(AchievementRewardRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(AchievementRewardRequest::kOpcode);
  writer.U32(request.achievement_id);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const ChangeTitleRequest& request) {
  if (!FixedLayoutHolds<ChangeTitleRequest>(kChangeTitleHandler, kChangeTitleOffsets)) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la disposition écrite pour le changement de titre ne correspond plus à "
                      "ce que le serveur déclare lire");
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(ChangeTitleRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(ChangeTitleRequest::kOpcode);
  writer.I32(request.title_id);
  return frame;
}

core::Result<ChangeTitleAck> DecodeChangeTitleAck(std::span<const std::uint8_t> frame) {
  if (frame.size() != ChangeTitleAck::kWireSize) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "accusé de titre de longueur inattendue");
  }
  ByteReader reader(frame);
  if (reader.U16() != ChangeTitleAck::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature, "trame d'un autre paquet que l'accusé de titre");
  }
  ChangeTitleAck ack;
  ack.result = reader.U8();
  ack.title_id = reader.I32();
  return ack;
}

core::Result<AchievementList> DecodeAchievementList(std::span<const std::uint8_t> frame) {
  ByteReader reader(frame);
  if (reader.U16() != AchievementList::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature, "trame d'un autre paquet que la liste des succès");
  }
  const std::uint16_t length = reader.U16();
  const std::uint32_t count = reader.U32();
  if (!reader.ok() || length != frame.size() || length < AchievementList::kHeaderSize) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "liste des succès tronquée");
  }
  // 🔴 LE COMPTE ANNONCÉ DOIT TOMBER SUR LA LONGUEUR, et c'est ce qui rend ce
  // décodeur sûr : le serveur écrit l'un et l'autre du même compte, et un
  // désaccord veut dire qu'on lit autre chose que ce qu'on croit.
  const std::size_t body = length - AchievementList::kHeaderSize;
  if (body % AchievementEntry::kWireSize != 0 || body / AchievementEntry::kWireSize != count) {
    return core::Fail(ErrorCode::kCorruptData,
                      "liste des succès : le compte annoncé ne tombe pas sur la longueur");
  }
  AchievementList list;
  list.total_points = reader.U32();
  list.rank = reader.U16();
  list.current_rank_points = reader.U32();
  list.next_rank_points = reader.U32();
  list.entries.reserve(count);
  for (std::uint32_t index = 0; index < count; ++index) {
    AchievementEntry entry;
    entry.achievement_id = reader.U32();
    entry.completed = reader.U8() != 0;
    for (std::uint32_t& objective : entry.objectives) {
      objective = reader.U32();
    }
    entry.completed_at = reader.U32();
    entry.rewarded = reader.U8() != 0;
    list.entries.push_back(entry);
  }
  if (!reader.ok()) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "liste des succès tronquée");
  }
  return list;
}

std::vector<std::uint8_t> EncodeChangeTitleAck(const ChangeTitleAck& ack) {
  std::vector<std::uint8_t> frame;
  frame.reserve(ChangeTitleAck::kWireSize);
  ByteWriter writer(frame);
  writer.U16(ChangeTitleAck::kOpcode);
  writer.U8(ack.result);
  writer.I32(ack.title_id);
  return frame;
}

std::vector<std::uint8_t> EncodeAchievementList(const AchievementList& list) {
  const std::size_t total =
      AchievementList::kHeaderSize + list.entries.size() * AchievementEntry::kWireSize;
  std::vector<std::uint8_t> frame;
  frame.reserve(total);
  ByteWriter writer(frame);
  writer.U16(AchievementList::kOpcode);
  writer.U16(static_cast<std::uint16_t>(total));
  writer.U32(static_cast<std::uint32_t>(list.entries.size()));
  writer.U32(list.total_points);
  writer.U16(list.rank);
  writer.U32(list.current_rank_points);
  writer.U32(list.next_rank_points);
  for (const AchievementEntry& entry : list.entries) {
    writer.U32(entry.achievement_id);
    writer.U8(entry.completed ? 1 : 0);
    for (const std::uint32_t objective : entry.objectives) {
      writer.U32(objective);
    }
    writer.U32(entry.completed_at);
    writer.U8(entry.rewarded ? 1 : 0);
  }
  return frame;
}

}  // namespace ro::proto
