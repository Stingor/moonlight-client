#pragma once

// GÉNÉRÉ par tools/packetgen/packetgen.py — ne pas modifier à la main.
//
// Les longueurs viennent du compilateur qui construit le serveur, pas d'une
// relecture de ses sources : voir le générateur pour la raison.

#include <cstdint>
#include <optional>

namespace ro::proto {

// La version de protocole dont cette table est tirée.
inline constexpr std::int32_t kPacketVersion = 20250716;

// Longueur annoncée d'un paquet dont la taille est portée par le paquet
// lui-même, sur les deux octets qui suivent l'opcode.
inline constexpr std::int16_t kVariableLength = -1;

// La longueur d'un paquet, ou rien si l'opcode est inconnu.
//
// ⚠ Un opcode inconnu n'est PAS une longueur nulle : impossible de savoir
// combien d'octets consommer, donc impossible de reprendre le flux. L'appelant
// doit fermer la session plutôt que deviner.
[[nodiscard]] std::optional<std::int16_t> PacketLength(std::uint16_t opcode);

// Le nombre d'opcodes connus.
[[nodiscard]] std::size_t KnownPacketCount();

}  // namespace ro::proto
