#pragma once

#include <cstdint>

namespace ro::render {

// VRAI QUAND LE GESTIONNAIRE D'EFFETS DU CLIENT TIENT UNE CLASSE POUR CE
// NUMÉRO. C'est alors la classe qui vit, et sa propre règle qui décide de sa
// fin. Faux pour un effet PRIMITIF : l'acteur sur lequel il naît l'inscrit dans
// sa liste, et le retire de ce monde en disparaissant — l'effet meurt avec lui.
//
// Tout numéro au-delà du dernier identifiant d'effet est inscrit : ce sont les
// clés calculées des chapeaux et d'une liste que le client dresse au démarrage.
[[nodiscard]] bool ClientRegistersEffectClass(std::uint32_t effect_id) noexcept;

}  // namespace ro::render
