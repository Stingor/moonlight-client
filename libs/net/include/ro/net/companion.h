#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/companion.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LE CHARIOT, LA MONTURE, LE FAUCON — `ZC 0x0F16` et `CZ 0x0F15`, tous deux
// réservés aux clients ayant donné leur poignée de main (ADR 0040).
//
// 🔴 LE PROTOCOLE D'ORIGINE N'A AUCUN PAQUET POUR LES APPELER. Un chevalier ne
// peut pas monter sur son peco depuis son client : il lui faut un PNJ ou une
// commande. Le chasseur et son faucon de même. Seul le chariot se bascule, par
// la compétence `MC_PUSHCART`. Ces deux paquets-ci ouvrent les trois.
//
// 🔴 ET L'ÉTAT NE SE DEVINE PAS NON PLUS. Le client d'origine le lit dans le
// bitmask d'option de l'acteur — et il se trompe : sous `NEW_CARTS`, le chariot
// n'y figure PAS. Il devrait de plus connaître les identifiants de compétence
// pour savoir lesquelles sont apprises, ce qui les ferait écrire en dur. Le
// serveur envoie donc les niveaux, les états ET les identifiants.
//
// ⚠ ON NE DEMANDE JAMAIS CET ÉTAT : il est POUSSÉ, au login vérifié puis à
// chaque changement — y compris quand un script de Kafra pose le chariot, les
// scripts passant par les mêmes poseurs (`pc_setcart`, `pc_setriding`,
// `pc_setfalcon`).
[[nodiscard]] FrameOutcome ApplyCompanionFrame(const Frame& frame, sim::World& world);

// CE QU'ON PEUT DEMANDER D'UN COMPAGNON.
//
// ⚠ `kDecorate` N'APPARTIENT QU'AU CHARIOT, et le serveur l'ignore pour les
// deux autres. C'est la seule des trois qui porte un ARGUMENT.
enum class CompanionWish : std::uint8_t { kDismiss, kSummon, kDecorate };

// « Appelle-le », « renvoie-le », « change sa décoration ».
//
// 🔴 RIEN N'EST SUPPOSÉ D'AVANCE, ET LE SERVEUR REVALIDE TOUT : `pc_setcart`
// reprend `MC_PUSHCART`, `pc_setriding` reprend `KN_RIDING`, la décoration
// reprend `MC_CHANGECART` ET le palier de niveau de base. Un client qui
// mentirait n'obtiendrait rien de plus qu'un client honnête.
//
// `decoration` ne sert qu'à `kDecorate` — et, pour un chariot qu'on appelle, à
// choisir l'apparence de départ ; zéro laisse le serveur prendre la première.
[[nodiscard]] core::Result<void> RequestCompanion(Connection& link, sim::CompanionKind kind,
                                                  CompanionWish wish, std::uint8_t decoration = 0);

}  // namespace ro::net
