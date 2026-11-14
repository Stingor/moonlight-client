#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/packed_position.h"

namespace ro::test_support {

// Une unité au repos, telle que le serveur l'annonce.
//
// Elle vit ICI parce que TROIS fichiers d'essais en ont besoin : il faut
// quelqu'un dans le monde avant de lui poser quoi que ce soit dessus. Trois
// copies d'un même bâtisseur finiraient par se séparer, et l'essai qui tombe
// accuserait alors le code.
[[nodiscard]] inline proto::PacketIdleUnit IdleUnit(std::uint32_t id, sim::CellPosition where) {
  proto::PacketIdleUnit unit;
  unit.objecttype = 0;  // un joueur
  unit.AID = id;
  unit.speed = 150;
  unit.job = 4054;
  unit.name = "Alice";
  unit.PosDir = sim::PackPosition(where);
  return unit;
}

// Une trame telle que le lecteur la rend : l'opcode, puis les octets entiers,
// en-tête compris.
//
// Les essais de traduction encodent un paquet et le repassent à ApplyFrame ;
// l'opcode se relit donc de la trame elle-même plutôt que de se redire.
[[nodiscard]] inline net::Frame FrameOf(std::vector<std::uint8_t> bytes) {
  const auto opcode = static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
  return net::Frame{opcode, std::move(bytes)};
}

}  // namespace ro::test_support
