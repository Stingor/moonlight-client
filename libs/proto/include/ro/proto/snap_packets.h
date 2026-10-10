#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ro::proto {

// LE BOND DE BODY RELOCATION (Snap), écrit à la main par le serveur
// (clif_snap) : l'acteur et la case où il arrive.
//
// ⚠ PAS ZC_HIGHJUMP : pour ce PACKETVER, le serveur envoie 0x08D2, que
// l'en-tête du protocole ne décrit pas.
struct SnapNotice {
  static constexpr std::uint16_t kOpcode = 0x08D2;
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t actor_id = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
};

// Faux si la trame n'a ni le bon opcode ni la bonne taille.
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, SnapNotice& packet);

// L'ENVERS DU DÉCODEUR, pour les essais : le client n'émet pas ce paquet.
[[nodiscard]] std::vector<std::uint8_t> Encode(const SnapNotice& packet);

}  // namespace ro::proto
