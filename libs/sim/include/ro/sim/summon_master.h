#pragma once

#include <cstdint>

#include "ro/sim/world_fwd.h"

namespace ro::sim {

// LE MAÎTRE D'UN MONSTRE INVOQUÉ : le joueur, l'homoncule ou le monstre qui l'a
// appelé — les plantes d'un Alchimiste, la Légion d'un homoncule, les sbires d'un
// MVP. Le serveur Moonlight le dit par `ZC_MOONLIGHT_UNIT_MASTER`, chaque fois
// qu'il montre le monstre, toujours APRÈS son apparition.
//
// LE MÊME PAQUET DIT À QUI APPARTIENT UN COMPAGNON — pet, homoncule, mercenaire,
// élémentaire —, à qui a annoncé `kUiCapCompanionMaster` : il se range de même.

// Note le maître que le serveur vient de dire ; zéro dit « sans maître ». Rend
// faux si aucun acteur ne porte cet identifiant : le serveur redit le maître à
// chaque apparition, si bien qu'un maître qui précéderait l'acteur n'a pas à être
// gardé — il reviendra avec lui.
bool TellSummonMaster(World& world, std::uint32_t actor_id, std::uint32_t master_id);

// L'identifiant du maître de cet acteur, ou zéro : acteur inconnu, monstre sans
// maître, ou maître que le serveur n'a pas dit. Il s'efface avec l'acteur — sortie
// de vue, changement de carte — et revient avec sa prochaine apparition.
[[nodiscard]] std::uint32_t SummonMasterOf(const World& world, std::uint32_t actor_id);

}  // namespace ro::sim
