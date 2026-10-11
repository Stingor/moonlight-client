#pragma once

#include "ro/vfs/vfs.h"

namespace moonclient {

struct GamePieces;

// L'ARBRE DES COMPÉTENCES, SES DESCRIPTIONS ET LES RÈGLES DU SERVEUR. Cinq
// tables du client, évaluées À LA SUITE : les trois dernières écrivent des clés
// que seules les deux premières savent résoudre. Puis `skill_trees.yml`, qui dit
// ce que chaque métier et la guilde peuvent apprendre. Sans l'arbre, la fiche de
// personnage n'a pas de grimoire à montrer ; sans les descriptions, elle le
// montre muet ; sans les règles, rien ne s'y monte.
//
// ⚠ LES TROIS ÉCHOUENT SÉPARÉMENT : une table de descriptions absente ne doit
// pas coûter l'arbre. Ce qui manque ne se montre pas, le reste tient.
void LoadSkillTree(ro::vfs::Vfs& vfs, GamePieces& pieces);

}  // namespace moonclient
