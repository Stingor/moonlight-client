#pragma once

#include <cstdint>

#include "ro/sim/actor.h"

namespace ro::sim {

// ── CE QUE LA MORT FAIT D'UN ACTEUR ─────────────────────────────────────────
//
// Le serveur annonce une mort par une disparition de type « mort », et ne dit
// rien de plus. Ce que l'on voit ensuite, c'est le client qui le décide, sur la
// classe affichée et le type d'objet du paquet d'apparition :
//
//   * UN PNJ NE MEURT PAS : il garde sa pose d'attente et reste debout. Est PNJ
//     ce que le paquet annonce PNJ (types d'objet 1 et 6) ou ce qui porte une
//     classe de PNJ ou de portail — sauf le PNJ au sprite de monstre (type 12)
//     et une classe à part, qui meurent comme des créatures.
//   * UNE CRÉATURE joue son agonie, puis laisse à sa place un CADAVRE que plus
//     rien ne désigne et qui s'efface de lui-même : voir
//     `ro/render/vanish_fade.h`. Sont créatures les classes de monstre, de
//     corps de PNJ, d'unité spéciale (homoncules, mercenaires, élémentaires) et
//     tout ce que le paquet annonce mercenaire.
//   * TOUT LE RESTE — un joueur, un monstre vêtu d'un corps de joueur — reste à
//     terre, et c'est le serveur qui l'enlève : il relève un joueur, et il fait
//     sortir de vue un monstre à corps de joueur trois secondes après sa mort.
enum class DeathAftermath : std::uint8_t {
  kStandsUp,
  kStaysDown,
  kLeavesCorpse,
};

// Les classes de PNJ et de portail : deux plages, bornes comprises.
inline constexpr std::uint32_t kFirstNonPlayerJob = 45;
inline constexpr std::uint32_t kLastNonPlayerJob = 999;
inline constexpr std::uint32_t kFirstExtraNonPlayerJob = 10001;
inline constexpr std::uint32_t kLastExtraNonPlayerJob = 19998;
// La seule classe de PNJ qui meurt comme une créature.
inline constexpr std::uint32_t kDyingNonPlayerJob = 565;

// Les classes dont la mort laisse un cadavre, bornes comprises : celles qui
// suivent les métiers de joueur jusqu'aux monstres, les monstres, leur plage
// étendue, et les unités spéciales.
inline constexpr std::uint32_t kFirstCorpseBodyJob = 32;
inline constexpr std::uint32_t kLastCorpseBodyJob = 999;
inline constexpr std::uint32_t kFirstMonsterJob = 1001;
inline constexpr std::uint32_t kLastMonsterJob = 3998;
inline constexpr std::uint32_t kFirstExtendedMonsterJob = 20021;
inline constexpr std::uint32_t kLastExtendedMonsterJob = 31999;
inline constexpr std::uint32_t kFirstSpecialUnitJob = 6001;
inline constexpr std::uint32_t kLastSpecialUnitJob = 6052;

// Les types d'objet du paquet d'apparition que la règle lit.
inline constexpr std::uint8_t kObjectTypeNonPlayerEvent = 1;
inline constexpr std::uint8_t kObjectTypeNonPlayer = 6;
inline constexpr std::uint8_t kObjectTypeMercenary = 9;
inline constexpr std::uint8_t kObjectTypeMonsterLookingNonPlayer = 12;

[[nodiscard]] DeathAftermath DeathAftermathOf(const Actor& actor) noexcept;

}  // namespace ro::sim
