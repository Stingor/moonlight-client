#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ro::sim {

// COMBIEN DE PROTÉGÉS une liste de DEVOTION nomme au plus.
inline constexpr std::size_t kDevotionSlots = 5;

// LES PROTÉGÉS QU'UN ACTEUR ANNONCE : ceux que DEVOTION relie à un croisé, ou le
// maître que son mercenaire protège.
//
// 🔴 LE SERVEUR REDIT LA LISTE ENTIÈRE, jamais un seul lien : à chaque pose, à
// chaque fin, et chaque fois que le croisé ou l'un des siens entre en vue. Le
// client défait alors les traits de l'acteur et en retend un par protégé, si
// bien qu'une liste redite à l'identique fait REPOUSSER ses traits. Garder la
// dernière ne suffit donc pas à dire qu'il faut les retendre : le monde
// NUMÉROTE les annonces.
struct DevotionList {
  // Zéro pour une case vide. Une case vide peut précéder une pleine : le
  // serveur libère la case de celui qui s'en va sans tasser les autres.
  std::array<std::uint32_t, kDevotionSlots> devotee_ids{};
  // LE NUMÉRO DE LA DERNIÈRE ANNONCE, zéro pour aucune.
  //
  // ⚠ UNIQUE DANS LE MONDE, ET NON PROPRE À L'ACTEUR : un croisé qui sort de la
  // vue et y revient est un acteur NEUF, et le serveur lui renvoie sa liste.
  // Un compte propre à l'acteur repartirait de zéro et pourrait retomber sur
  // celui que l'on avait déjà tendu pour l'ancien, si les deux arrivent entre
  // deux images : la liste neuve passerait pour déjà vue.
  std::uint32_t told_serial = 0;
};

}  // namespace ro::sim
