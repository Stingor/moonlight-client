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
// une carte). Les trophées de victoire exigent que le boss final soit tombé.
//
// - « Tenir bon » : 5:00 de partie.
// - « Tombeur de Baphomet » : la victoire dans une arène dont Baphomet est le
//   boss final, quelle que soit la classe.
// - « Métamorphose » : un skill évolué.
// - « Marée repoussée » : 1 000 monstres vaincus dans la partie.
// - « Mage aguerri » : le niveau 40, avec le Magicien.
// - « Première carte » : une carte tombée d'un monstre et ramassée au sol ;
//   une carte prise dans une offre ou un coffre n'en est pas une.
// - « Pyromane » : la victoire avec le Magicien, chaque skill possédé à la fin
//   étant de l'élément Fire. Le Chevalier en est exclu : il part de Magnum Break,
//   qui suffirait.
// - « Esprit pur » : la victoire sans aucune carte possédée (une carte
//   ramassée au sol ne se porte pas).
// - « Blizzard », « Foudre de Jupiter », « Seigneur du tonnerre », « Ciel en
//   feu », « Union » : Storm Gust, Jupitel Thunder, Lord of Vermilion,
//   Heaven's Drive, Napalm Vulcan possédé à la fin de la partie.
// - « Grand sorcier » : chacun des skills évolués du Magicien obtenu au moins
//   une fois, au fil des parties (`MinigameRecord::survivors_evolved`).
// - « Hiver éternel » : 1 000 monstres gelés dans la partie.
// - « À l'aveugle » : la victoire dans une partie lancée sans minimap.
// - « Souffle ardent », « Déluge », « Rempart de glace » : Fire Ball, Water
//   Ball, Ice Wall possédé à son plus haut niveau à la fin de la partie.
// - « Roche pourpre », « Givre » : Crimson Rock, Jack Frost possédé à la fin.
// - « Brise-crâne », « Lance en spirale », « Brise-genoux », « Lame d'aura »,
//   « Esprit affûté », « Berserker », « Terre embrasée » : Head Crush, Spiral
//   Pierce, Joint Beat, Aura Blade, Concentration, Frenzy, Ignition Break possédé
//   à la fin de la partie.
// - « Grand chevalier » : chacun des skills évolués du Chevalier obtenu au moins
//   une fois, au fil des parties.

// CE QUE LA PARTIE NE SAIT PAS D'ELLE-MÊME, et qu'`app` dit : la minimap est
// un affichage, que la simulation ignore.
struct SurvivorsTrophyFacts {
  bool without_minimap = false;
};

// LES TROPHÉES QUE CETTE PARTIE GAGNE, et les évolués qu'elle ajoute au
// record. Rend les trophées gagnés à l'instant, dans l'ordre du catalogue ;
// vide quand rien ne change.
[[nodiscard]] std::vector<MinigameTrophy> RecordSurvivors(MinigameRecord& record,
                                                          const SurvivorsRun& run,
                                                          const SurvivorsTrophyFacts& facts = {});

}  // namespace ro::sim
