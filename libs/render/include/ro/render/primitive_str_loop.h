#pragma once

#include <cstdint>
#include <span>

namespace ro::render {

// LES EFFETS PRIMITIFS DONT LE `.str` REPART À SA FIN, triés.
//
// Le cas de chacun lit son fichier et, quand la lecture répond qu'elle a fini,
// remet la boucle à zéro au lieu de laisser l'instance s'éteindre : le fichier
// rejoue jusqu'à la fin de la vie de l'effet — sa durée de table —, ou jusqu'à
// ce que son porteur s'en aille. L'éclair d'un Shockwave Trap reste ainsi sur
// le piège tant que le piège est là.
[[nodiscard]] std::span<const std::uint16_t> PrimitiveStrLoopTable();

// Vrai quand le fichier de cet effet primitif repart à sa fin.
[[nodiscard]] bool PrimitiveStrLoopsForLife(std::uint32_t effect_id);

}  // namespace ro::render
