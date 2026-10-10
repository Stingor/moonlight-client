#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"

// CE QUE LES DEUX TRADUCTEURS DE TRAMES PARTAGENT.
//
// `world_update.cpp` traduit ce qui arrive à un ACTEUR — il entre, il marche,
// il s'assoit, il change d'apparence — et `world_skill.cpp` ce qu'une
// COMPÉTENCE montre. Les deux décodent de la même façon, et les deux ont à
// reconnaître un coup qu'on encaisse sans broncher.
//
// En-tête PRIVÉ : il vit dans `src/` et non dans `include/`, parce que rien
// hors de cette bibliothèque n'a à décoder une trame.
namespace ro::net {

// Décode et applique, ou dit pourquoi il n'a pas pu.
template <typename Packet, typename Apply>
FrameOutcome Decoded(const Frame& frame, Apply apply) {
  Packet packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }
  return apply(packet);
}

template <std::size_t N>
inline bool Listed(const std::array<std::uint8_t, N>& values, std::uint8_t value) {
  return std::ranges::find(values, value) != values.end();
}

// Les coups qu'on ENCAISSE : la cible les prend sans broncher. Ce sont deux des
// valeurs de e_damage_type (src/map/clif.hpp du serveur), et le coup ordinaire
// comme le coup de compétence peuvent l'un et l'autre les porter.
inline constexpr std::array<std::uint8_t, 2> kEnduredBlowActions = {4, 9};

}  // namespace ro::net
