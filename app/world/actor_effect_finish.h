#pragma once

#include <cstddef>
#include <cstdint>

#include "ro/sim/actor.h"
#include "ro/sim/world.h"

// QUAND UN EFFET JOUÉ QUITTE LA LISTE DE SON ACTEUR DE LUI-MÊME.
//
// Le monde garde les effets qu'un acteur porte, mais ne sait pas ce qu'un
// numéro montre ni combien de temps : ce sont les tables du client qui le
// disent, et `sim` ne les connaît pas. C'est donc ici que l'on tranche, et le
// monde n'en reçoit que le retrait.
namespace moonclient {

// L'EFFET EST-IL FINI à cet instant, au point de ne plus rien devoir à la liste ?
//
// Fini veut dire que son INSTANCE est morte — la durée que le client lui donne
// est relevée, bornée, et son âge l'a atteinte — ET que rien ne relit plus sa
// place dans la liste. Ne finissent donc jamais d'eux-mêmes, et attendent un
// retrait du serveur ou d'un autre effet :
//
//   - un effet en boucle : durée infinie, inconnue, ou instance qui rembobine
//     son âge (`ro::render::InstanceAliveAt`) ;
//   - un effet d'office (`PlayedEffect::lasting`) ;
//   - un effet qui porte sa propre échéance (`ends_ms`) : le monde l'ôte à
//     cette échéance-là, voir `World::ExpirePlayerStatuses` ;
//   - un effet posé par un état, ou né de sa classe : c'est l'état ou la classe
//     qui règle sa vie ;
//   - un effet qui porte une VALEUR : le rang d'un anneau PVP, le nombre de
//     sphères se relisent dans la liste, et l'en ôter les ferait reposer ;
//   - un effet qui MARQUE SON PORTEUR : une pose, un angle, une teinte qui
//     remonte vers le blanc survivent à l'instance, et se relisent à l'âge de
//     l'effet dans la liste.
[[nodiscard]] bool EffectFinishedAt(const ro::sim::PlayedEffect& effect, std::uint32_t now_ms);

// ÔTE DE LA LISTE DE CHAQUE ACTEUR LES EFFETS FINIS à cet instant, sans les
// inscrire au journal des retraits : rien ne joue plus pour eux, il n'y a rien
// à couper. Rend le nombre d'effets ôtés.
std::size_t ForgetFinishedEffects(ro::sim::World& world, std::uint32_t now_ms);

}  // namespace moonclient
