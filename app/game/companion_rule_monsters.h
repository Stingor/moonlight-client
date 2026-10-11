#pragma once

// LES MONSTRES QUE LA LISTE « MONSTRE DE LA CIBLE » D'UNE RÈGLE PROPOSE, au-delà
// de ceux que le client a vus : tous ceux de la table du serveur, et ce qui aide
// à en choisir un.

#include <cstdint>
#include <string>
#include <vector>

#include "ro/vfs/monster_list.h"

#include "game/companion_ai_fields.h"

namespace moonclient {

// LES MONSTRES DE LA TABLE, PAR NOM, à la casse près, puis par identifiant :
// deux monstres du même nom restent dans un ordre stable.
[[nodiscard]] std::vector<std::uint16_t> MonstersByName(const ro::vfs::MonsterList& list);

// CE QUI AIDE À CHOISIR un monstre de la table, traduit : « niv. 81 · MVP ·
// Demon · Dark 3 · Large ». Les mots de race, d'élément et de taille viennent
// de `names` ; nuls, ils sont tus.
[[nodiscard]] std::string MonsterListDetail(const ro::vfs::MonsterListEntry& entry,
                                            const RuleNames& names);

}  // namespace moonclient
