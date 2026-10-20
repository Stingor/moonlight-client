#pragma once

#include <cstdint>

namespace ro::sim {

// L'ÉLÉMENT QUE PORTE L'ATTAQUE NORMALE D'UN JOUEUR, tel que le serveur le dit
// (`ZC 0x0F4A`) : celui de l'arme, de la flèche, d'un endow ou d'Enchant Arms.
//
// Les dix premières valeurs suivent l'ordre du serveur, qui les écrit telles
// quelles sur le fil. `kNone` : le serveur n'a rien dit, ou il retient
// l'élément — sur une carte d'affrontement et pendant un duel, il ne le dit
// qu'au joueur lui-même.
//
// 🔴 LE PAQUET EST LA SEULE SOURCE : rien ne se déduit de l'arme qu'on voit,
// ni d'un état qu'on reçoit. Un élément deviné donnerait au combat ce que le
// serveur refuse de dire.
enum class Element : std::uint8_t {
  kNeutral = 0,
  kWater = 1,
  kEarth = 2,
  kFire = 3,
  kWind = 4,
  kPoison = 5,
  kHoly = 6,
  kShadow = 7,
  kGhost = 8,
  kUndead = 9,
  kNone = 0xFF,
};

// L'élément qu'un octet du fil désigne. Une valeur hors de la liste vaut
// `kNone` : mieux vaut ne rien montrer qu'une couleur fausse.
[[nodiscard]] constexpr Element ElementOfWire(std::uint8_t wire) noexcept {
  return wire <= static_cast<std::uint8_t>(Element::kUndead) ? static_cast<Element>(wire)
                                                             : Element::kNone;
}

}  // namespace ro::sim
