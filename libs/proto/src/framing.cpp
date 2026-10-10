#include "ro/proto/framing.h"

#include "ro/proto/packet_lengths.h"

namespace ro::proto {
namespace {

// Un entier de deux octets, poids faible en premier — l'ordre du protocole.
std::uint16_t ReadLittleEndian(std::span<const std::uint8_t> buffer, std::size_t offset) {
  return static_cast<std::uint16_t>(buffer[offset]) |
         static_cast<std::uint16_t>(static_cast<std::uint16_t>(buffer[offset + 1]) << 8);
}

}  // namespace

FrameResult NextFrame(std::span<const std::uint8_t> buffer) {
  FrameResult result;
  if (buffer.size() < kOpcodeBytes) {
    // Même l'opcode n'est pas encore là. On ne peut rien dire, pas même
    // lequel : le laisser à zéro serait affirmer le paquet 0x0000.
    return result;
  }

  result.opcode = ReadLittleEndian(buffer, 0);
  const std::optional<std::int16_t> declared = PacketLength(result.opcode);
  if (!declared.has_value()) {
    result.status = FrameStatus::kUnknownOpcode;
    return result;
  }

  if (*declared != kVariableLength) {
    const auto size = static_cast<std::size_t>(*declared);
    // Une longueur fixe inférieure à l'opcode lui-même ne décrit aucune trame
    // possible. La table vient du serveur, mais elle n'est pas une garantie :
    // un jour elle sera régénérée depuis d'autres sources.
    if (size < kOpcodeBytes) {
      result.status = FrameStatus::kCorrupt;
      return result;
    }
    if (buffer.size() < size) {
      return result;
    }
    result.status = FrameStatus::kComplete;
    result.size = size;
    return result;
  }

  // Longueur variable : elle est portée par les deux octets qui suivent
  // l'opcode, et elle compte l'en-tête.
  if (buffer.size() < kVariableHeaderBytes) {
    return result;
  }
  const std::size_t announced = ReadLittleEndian(buffer, kOpcodeBytes);
  if (announced < kVariableHeaderBytes) {
    // Une trame ne peut pas être plus courte que ce qui porte sa propre
    // longueur. L'accepter ferait avancer de moins que l'en-tête, ou de rien
    // du tout : le lecteur tournerait en boucle sur les mêmes octets.
    result.status = FrameStatus::kCorrupt;
    return result;
  }
  if (buffer.size() < announced) {
    return result;
  }
  result.status = FrameStatus::kComplete;
  result.size = announced;
  return result;
}

}  // namespace ro::proto
