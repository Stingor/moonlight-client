// Voir l'en-tête.

#include "ro/proto/pet_packets.h"

#include <array>
#include <string_view>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/packet_lengths.h"

namespace ro::proto {
namespace {

using core::ByteReader;
using core::ByteWriter;
using core::ErrorCode;

// Ce que chaque demande déclare lire, selon le serveur.
constexpr std::string_view kCommandHandler = "clif_parse_PetMenu";
constexpr std::array<std::uint16_t, 1> kCommandOffsets = {2};
constexpr std::string_view kRenameHandler = "clif_parse_ChangePetName";
constexpr std::array<std::uint16_t, 1> kRenameOffsets = {2};
constexpr std::string_view kEggChoiceHandler = "clif_parse_SelectEgg";
constexpr std::array<std::uint16_t, 1> kEggChoiceOffsets = {2};
// La longueur, puis l'œuf cible.
constexpr std::string_view kEvolutionHandler = "clif_parse_pet_evolution";
constexpr std::array<std::uint16_t, 2> kEvolutionOffsets = {2, 4};

// La disposition écrite ici est-elle celle que le serveur déclare, longueur
// comprise ? Un paquet variable se déclare `kVariableLength`.
[[nodiscard]] bool LayoutHolds(std::uint16_t opcode, std::string_view handler,
                               std::span<const std::uint16_t> offsets, std::int16_t length) {
  const auto declared = PacketLength(opcode);
  return MatchesServerLayout(opcode, handler, offsets) && declared.has_value() &&
         *declared == length;
}

[[nodiscard]] std::unexpected<core::Error> LayoutChanged(std::string_view what) {
  return core::Fail(ErrorCode::kUnsupportedVersion,
                    std::string("la disposition écrite pour ") + std::string(what) +
                        " ne correspond plus à ce que le serveur déclare lire");
}

}  // namespace

core::Result<PetEggList> DecodePetEggList(std::span<const std::uint8_t> frame) {
  ByteReader reader(frame);
  if (reader.U16() != PetEggList::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature, "trame d'un autre paquet que la liste d'éclosion");
  }
  const std::uint16_t length = reader.U16();
  if (!reader.ok() || length != frame.size() || length < PetEggList::kHeaderSize ||
      (frame.size() - PetEggList::kHeaderSize) % PetEggList::kEntrySize != 0) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "liste d'éclosion tronquée");
  }
  PetEggList list;
  list.client_slots.resize((frame.size() - PetEggList::kHeaderSize) / PetEggList::kEntrySize);
  for (std::uint16_t& slot : list.client_slots) {
    slot = reader.U16();
  }
  if (!reader.ok()) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "liste d'éclosion tronquée");
  }
  return list;
}

std::vector<std::uint8_t> EncodePetEggList(const PetEggList& list) {
  std::vector<std::uint8_t> frame;
  ByteWriter writer(frame);
  writer.U16(PetEggList::kOpcode);
  writer.U16(static_cast<std::uint16_t>(PetEggList::kHeaderSize +
                                        list.client_slots.size() * PetEggList::kEntrySize));
  for (const std::uint16_t slot : list.client_slots) {
    writer.U16(slot);
  }
  return frame;
}

core::Result<PetEvolutionResult> DecodePetEvolutionResult(std::span<const std::uint8_t> frame) {
  if (frame.size() != PetEvolutionResult::kWireSize) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "résultat d'évolution de longueur inattendue");
  }
  ByteReader reader(frame);
  if (reader.U16() != PetEvolutionResult::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature,
                      "trame d'un autre paquet que le résultat d'évolution");
  }
  PetEvolutionResult result;
  result.result = reader.U32();
  return result;
}

std::vector<std::uint8_t> EncodePetEvolutionResult(const PetEvolutionResult& result) {
  std::vector<std::uint8_t> frame;
  ByteWriter writer(frame);
  writer.U16(PetEvolutionResult::kOpcode);
  writer.U32(result.result);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const PetCommandRequest& request) {
  if (!LayoutHolds(PetCommandRequest::kOpcode, kCommandHandler, kCommandOffsets,
                   static_cast<std::int16_t>(PetCommandRequest::kWireSize))) {
    return LayoutChanged("un ordre au familier");
  }
  std::vector<std::uint8_t> frame;
  ByteWriter writer(frame);
  writer.U16(PetCommandRequest::kOpcode);
  writer.U8(static_cast<std::uint8_t>(request.command));
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const PetRenameRequest& request) {
  if (!LayoutHolds(PetRenameRequest::kOpcode, kRenameHandler, kRenameOffsets,
                   static_cast<std::int16_t>(PetRenameRequest::kWireSize))) {
    return LayoutChanged("le renommage du familier");
  }
  // Le serveur garde un octet de moins que le champ : un nom qui le remplit
  // perdrait sa fin, peut-être au milieu d'un caractère.
  if (request.name.empty() || request.name.size() >= PetRenameRequest::kNameSize) {
    return core::Fail(ErrorCode::kInvalidArgument, "nom de familier vide ou trop long");
  }
  std::vector<std::uint8_t> frame;
  ByteWriter writer(frame);
  writer.U16(PetRenameRequest::kOpcode);
  writer.FixedString(request.name, PetRenameRequest::kNameSize);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const PetEggChoice& choice) {
  if (!LayoutHolds(PetEggChoice::kOpcode, kEggChoiceHandler, kEggChoiceOffsets,
                   static_cast<std::int16_t>(PetEggChoice::kWireSize))) {
    return LayoutChanged("le choix d'un œuf");
  }
  std::vector<std::uint8_t> frame;
  ByteWriter writer(frame);
  writer.U16(PetEggChoice::kOpcode);
  writer.U16(choice.client_slot);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const PetEvolutionRequest& request) {
  if (!LayoutHolds(PetEvolutionRequest::kOpcode, kEvolutionHandler, kEvolutionOffsets,
                   kVariableLength)) {
    return LayoutChanged("l'évolution du familier");
  }
  std::vector<std::uint8_t> frame;
  ByteWriter writer(frame);
  writer.U16(PetEvolutionRequest::kOpcode);
  writer.U16(static_cast<std::uint16_t>(PetEvolutionRequest::kWireSize));
  writer.U32(request.target_egg_id);
  return frame;
}

}  // namespace ro::proto
