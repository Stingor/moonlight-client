#include "game/skill_tree_load.h"

#include <utility>

#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/sim/skill_tree.h"

#include "game/game_pieces.h"
#include "game/skill_rules.h"
#include "game/skill_tables.h"

namespace moonclient {

void LoadSkillTree(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  pieces.skill_icons.emplace(vfs);
  auto tables = LoadSkillTables(vfs);
  if (!tables) {
    // ⚠ CET ÉCHEC-CI COÛTE AUSSI LES DESCRIPTIONS : elles vivent dans la même
    // table que les fiches.
    ro::core::LogWarning(
        "arbre des compétences : fiches illisibles ({}) — le grimoire et les "
        "descriptions resteront vides",
        ro::core::Describe(tables.error()));
    return;
  }
  if (tables->layout_error) {
    ro::core::LogWarning("disposition des compétences : {} — le grimoire n'aura pas de grille",
                         ro::core::Describe(*tables->layout_error));
  }
  ro::sim::SkillTree& tree = tables->tree;
  ro::core::LogInfo("arbre des compétences : {} fiche(s)", tree.node_count());

  auto rules = LoadSkillRules(vfs);
  if (rules) {
    ro::core::LogInfo("règles des compétences : {} métier(s), {} de guilde{}", rules->by_job.size(),
                      rules->guild.size(), rules->skill_free ? ", prérequis non vérifiés" : "");
    tree.UseServerRules(std::move(*rules));
  } else {
    ro::core::LogWarning(
        "règles des compétences : {} — la grille se montrera entière, et rien ne s'y montera",
        ro::core::Describe(rules.error()));
  }
  pieces.skill_tree = std::move(tree);
  ro::core::LogInfo("descriptions de compétences : {} fiche(s)", tables->words.skill_count());
  pieces.skill_words = std::move(tables->words);
}

}  // namespace moonclient
