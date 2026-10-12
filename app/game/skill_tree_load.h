#pragma once

#include "ro/vfs/vfs.h"

namespace moonclient {

struct GamePieces;

// L'ARBRE DES COMPÉTENCES, SES DESCRIPTIONS ET LES RÈGLES DU SERVEUR. Les fiches
// et leurs descriptions viennent de `skill_info.yml`, la grille et les onglets de
// `skill_layout.yml`, ce que chaque métier et la guilde peuvent apprendre de
// `skill_trees.yml`. Sans les fiches, la fiche de personnage n'a pas de grimoire
// à montrer ; sans la disposition, il n'a pas de grille ; sans les règles, rien
// ne s'y monte.
//
// ⚠ LES TROIS ÉCHOUENT SÉPARÉMENT : une table absente ne doit pas coûter les
// autres. Ce qui manque ne se montre pas, le reste tient.
void LoadSkillTree(ro::vfs::Vfs& vfs, GamePieces& pieces);

}  // namespace moonclient
