#pragma once

#include <vector>

#include "ro/sim/minigame_trophies.h"

namespace ro::sim {

class SurvivorsRun;

// ── LES TROPHÉES DU MODE SURVIVORS ───────────────────────────────────────────
//
// Des trophées de mini-jeu comme les autres (`MinigameTrophy`, rangs 8 et
// suivants) : tenus par le client, dits au serveur par `CZ 0x0F4B`, sans
// récompense, parce que rien ne prouve la partie.
//
// ⚠ ILS SE JUGENT SUR L'ÉTAT DE LA PARTIE, quelle que soit son issue : une
// partie abandonnée, ou figée par une liaison perdue, garde ce qu'elle a
// atteint (cinq minutes tenues, mille monstres, le niveau 40, une évolution,
// une carte). Les trois trophées de victoire exigent que Baphomet soit tombé.
//
// - « Tenir bon » : 5:00 de partie.
// - « Tombeur de Baphomet » : la partie finie en victoire.
// - « Métamorphose » : un skill évolué.
// - « Marée repoussée » : 1 000 monstres vaincus dans la partie.
// - « Mage aguerri » : le niveau 40.
// - « Première carte » : une carte tombée d'un monstre et ramassée au sol ;
//   une carte prise dans une offre ou un coffre n'en est pas une.
// - « Pyromane » : la victoire, chaque skill possédé à la fin étant de
//   l'élément Fire.
// - « Esprit pur » : la victoire sans aucune carte possédée (une carte
//   ramassée au sol ne se porte pas).

// LES TROPHÉES QUE CETTE PARTIE GAGNE. Rend ceux gagnés à l'instant, dans
// l'ordre du catalogue ; vide quand rien ne change.
[[nodiscard]] std::vector<MinigameTrophy> RecordSurvivors(MinigameRecord& record,
                                                          const SurvivorsRun& run);

}  // namespace ro::sim
