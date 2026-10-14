#pragma once

#include <cstdint>
#include <string_view>

#include "ro/sim/world_fwd.h"

namespace ro::net {
class Connection;
}  // namespace ro::net

namespace moonclient {

// `/str+ 5`, `/agi+`, `/vit+`, `/int+`, `/dex+`, `/luk+` — et leurs six sœurs
// de métier, `/pow+`, `/sta+`, `/wis+`, `/spl+`, `/con+`, `/crt+`.
//
// LA LIGNE SE COUPE AUX BLANCS, et le nombre est le DEUXIÈME MOT, lu comme le
// jeu le lit : un signe, des chiffres, et rien d'autre — « abc » vaut zéro,
// « 12x » vaut douze. Le premier mot se compare sans la casse latine.
//
// 🔴 SANS DEUXIÈME MOT, CE N'EST PAS UNE COMMANDE : la ligne n'est pas reconnue
// et part comme une parole ordinaire. Le jeu ne dit ni l'usage, ni le manque de
// points, ni le refus : c'est la fiche qui change, ou ne change pas.
//
// ⚠ LE CLIENT NE BORNE RIEN. Une caractéristique de base part sur UN OCTET —
// « 300 » demande 44, « -1 » demande 255 — et une de métier sur deux. C'est le
// serveur qui ramène la quantité à ce que les points et le plafond permettent,
// et qui refuse une quantité nulle.
enum class StatRaiseKind : std::uint8_t {
  kNone,
  // `CZ_STATUS_CHANGE`, 0x00BB : la quantité tient sur un octet.
  kBase,
  // `CZ_ADVANCED_STATUS_CHANGE`, 0x0B24 : la quantité tient sur deux octets.
  kTrait,
};

struct StatRaiseCommand {
  StatRaiseKind kind = StatRaiseKind::kNone;
  // Le numéro du serveur : `SP_STR` à `SP_LUK` (13 à 18) ou `SP_POW` à
  // `SP_CRT` (219 à 224).
  std::uint16_t parameter = 0;
  // La quantité telle qu'elle part, déjà réduite à la largeur de son champ.
  std::uint16_t amount = 0;
};

[[nodiscard]] StatRaiseCommand ReadStatRaiseCommand(std::string_view line);

// Le nombre en tête d'un mot, comme `atoi` : un signe facultatif, puis des
// chiffres jusqu'au premier qui n'en est pas un. Aucun chiffre : zéro. Hors de
// l'entier signé de 32 bits : la borne la plus proche.
[[nodiscard]] std::int32_t LeadingInteger(std::string_view word);

// Envoie la montée que la ligne demande. Rend faux quand la ligne n'en est pas
// une ; hors ligne, la commande se prend et ne demande rien à personne.
[[nodiscard]] bool RunStatRaiseCommand(ro::net::Connection& link, ro::sim::World& world,
                                       std::string_view line);

}  // namespace moonclient
