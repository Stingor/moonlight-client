// Voir l'en-tête.

#include "ro/proto/homunculus_packets.h"

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

constexpr std::string_view kCommandHandler = "clif_parse_HomMenu";
// Le type, puis la commande.
constexpr std::array<std::uint16_t, 2> kCommandOffsets = {2, 4};
constexpr std::string_view kRenameHandler = "clif_parse_ChangeHomunculusName";
constexpr std::array<std::uint16_t, 1> kRenameOffsets = {2};
// Le type d'un ordre du menu : le serveur ne le lit pas, le client l'envoie nul.
constexpr std::uint16_t kCommandType = 0;

[[nodiscard]] core::Error Drift() {
  return core::Error{ErrorCode::kUnsupportedVersion,
                     "la disposition écrite pour un paquet d'homoncule ne correspond plus à ce "
                     "que le serveur déclare lire"};
}

}  // namespace

core::Result<HomunculusProperties> DecodeHomunculusProperties(std::span<const std::uint8_t> frame) {
  if (frame.size() != HomunculusProperties::kWireSize) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "fiche d'homoncule de longueur inattendue");
  }
  ByteReader reader(frame);
  if (reader.U16() != HomunculusProperties::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature, "trame d'un autre paquet que la fiche d'homoncule");
  }
  HomunculusProperties properties;
  properties.name = std::string(reader.FixedString(HomunculusProperties::kNameBytes));
  properties.flags = reader.U8();
  properties.level = reader.I16();
  properties.hunger = reader.I16();
  properties.intimacy = reader.I16();
  properties.atk = reader.I16();
  properties.matk = reader.I16();
  properties.hit = reader.I16();
  properties.crit = reader.I16();
  properties.def = reader.I16();
  properties.mdef = reader.I16();
  properties.flee = reader.I16();
  properties.amotion_ms = reader.I16();
  properties.hp = reader.U32();
  properties.max_hp = reader.U32();
  properties.sp = reader.U32();
  properties.max_sp = reader.U32();
  properties.exp = reader.I64();
  properties.exp_next = reader.I64();
  properties.skill_points = reader.I16();
  properties.attack_range_cells = reader.I16();
  if (!reader.ok()) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "fiche d'homoncule tronquée");
  }
  return properties;
}

std::vector<std::uint8_t> EncodeHomunculusProperties(const HomunculusProperties& properties) {
  std::vector<std::uint8_t> frame;
  frame.reserve(HomunculusProperties::kWireSize);
  ByteWriter writer(frame);
  writer.U16(HomunculusProperties::kOpcode);
  writer.FixedString(properties.name, HomunculusProperties::kNameBytes);
  writer.U8(properties.flags);
  writer.I16(properties.level);
  writer.I16(properties.hunger);
  writer.I16(properties.intimacy);
  writer.I16(properties.atk);
  writer.I16(properties.matk);
  writer.I16(properties.hit);
  writer.I16(properties.crit);
  writer.I16(properties.def);
  writer.I16(properties.mdef);
  writer.I16(properties.flee);
  writer.I16(properties.amotion_ms);
  writer.U32(properties.hp);
  writer.U32(properties.max_hp);
  writer.U32(properties.sp);
  writer.U32(properties.max_sp);
  writer.I64(properties.exp);
  writer.I64(properties.exp_next);
  writer.I16(properties.skill_points);
  writer.I16(properties.attack_range_cells);
  return frame;
}

core::Result<HomunculusParameter> DecodeHomunculusParameter(std::span<const std::uint8_t> frame) {
  if (frame.size() != HomunculusParameter::kWireSize) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "champ d'homoncule de longueur inattendue");
  }
  ByteReader reader(frame);
  if (reader.U16() != HomunculusParameter::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature, "trame d'un autre paquet qu'un champ d'homoncule");
  }
  HomunculusParameter parameter;
  parameter.type = reader.U16();
  parameter.value = reader.U64();
  return parameter;
}

std::vector<std::uint8_t> EncodeHomunculusParameter(const HomunculusParameter& parameter) {
  std::vector<std::uint8_t> frame;
  frame.reserve(HomunculusParameter::kWireSize);
  ByteWriter writer(frame);
  writer.U16(HomunculusParameter::kOpcode);
  writer.U16(parameter.type);
  writer.U64(parameter.value);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const HomunculusCommandRequest& request) {
  if (!FixedLayoutHolds<HomunculusCommandRequest>(kCommandHandler, kCommandOffsets)) {
    return std::unexpected(Drift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(HomunculusCommandRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(HomunculusCommandRequest::kOpcode);
  writer.U16(kCommandType);
  writer.U8(static_cast<std::uint8_t>(request.command));
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const HomunculusRenameRequest& request) {
  if (!FixedLayoutHolds<HomunculusRenameRequest>(kRenameHandler, kRenameOffsets)) {
    return std::unexpected(Drift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(HomunculusRenameRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(HomunculusRenameRequest::kOpcode);
  writer.FixedString(request.name, HomunculusRenameRequest::kNameBytes);
  return frame;
}

}  // namespace ro::proto
