#pragma once

// GÉNÉRÉ par tools/packetgen/packetgen.py — ne pas modifier à la main.
//
// Ce que la table du serveur déclare des paquets qu'il PARSE : le nom de la
// fonction qui les lit, et les décalages auxquels elle lit chaque champ.

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace ro::proto {

// La version de protocole dont ces déclarations sont tirées.
inline constexpr std::int32_t kParsedPacketVersion = 20250716;

struct ParsedPacket {
  // Le nom de la fonction du serveur qui lit ce paquet.
  //
  // C'est LUI qui identifie un paquet, pas son opcode : le serveur réattribue
  // les opcodes d'une version à l'autre, et 0x0072 ne porte plus l'entrée en jeu
  // mais l'usage d'une compétence.
  std::string_view handler;
  // Les décalages auxquels le serveur lit les champs, dans l'ordre où sa
  // fonction les lit. Vides pour un paquet qu'il reçoit sans en lire le contenu.
  std::span<const std::uint16_t> field_offsets;
};

// Ce que le serveur déclare de ce paquet, ou rien s'il ne le parse pas.
//
// Un paquet que le serveur n'écoute pas n'a pas de disposition déclarée : soit
// il va dans l'autre sens, soit cette version ne l'accepte pas.
[[nodiscard]] std::optional<ParsedPacket> ParsedPacketFor(std::uint16_t opcode);

// Le nombre de paquets que le serveur déclare lire.
[[nodiscard]] std::size_t ParsedPacketCount();

}  // namespace ro::proto
