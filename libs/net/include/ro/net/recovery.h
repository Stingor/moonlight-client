#pragma once

#include <cstdint>

#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LA VIE ET LA MAGIE RENDUES AU JOUEUR : `ZC_RECOVERY`, 0x0A27.
//
//     0a27 <quoi>.W <montant>.L
//
// Le serveur l'envoie au seul joueur concerné, pour tout soin qu'il veut
// MONTRER : le tic de régénération des compétences passives (Increase HP
// Recovery, Increase SP Recovery, toutes les dix secondes), la magie que Soul
// Drain arrache à sa victime, la vie et la magie qu'une attaque aspire quand le
// serveur est réglé pour le dire.
//
// Le client en fait trois choses, sur le joueur et sur lui seul :
//   · il AJOUTE le montant à sa propre vie ou à sa propre magie ;
//   · il fait monter le montant au-dessus du personnage, en vert pour la vie,
//     en bleu pour la magie, avec l'allure d'un soin ;
//   · il pose l'effet de la vie ou de la magie rendue au fil du temps.
//
// C'est exactement ce que fait l'effet à valeur 0x0284 pour ces deux mêmes
// effets : la trame se traduit donc en un effet à valeur, sur le joueur, et le
// chiffre suit le même chemin.
//
// 🔴 L'AJOUT N'EST PAS UNE REDITE. Pour la magie, le serveur n'envoie RIEN
// d'autre : ni `ZC_PAR_CHANGE`, ni la feuille. Sans cet ajout, la jauge de
// magie resterait figée entre deux tics de régénération ordinaire.
//
// ⚠ Tout autre `quoi` que la vie et la magie est ignoré, comme chez le client.
// Le serveur en connaît un troisième — l'AP —, que le client ne montre pas.
[[nodiscard]] FrameOutcome ApplyRecoveryFrame(const Frame& frame, sim::World& world,
                                              std::uint32_t now_ms);

// Les effets que la trame pose : la vie, puis la magie, rendues au fil du
// temps — les mêmes numéros que l'effet à valeur fait compter.
inline constexpr std::uint32_t kHpRecoveryEffect = 331;
inline constexpr std::uint32_t kSpRecoveryEffect = 332;

}  // namespace ro::net
