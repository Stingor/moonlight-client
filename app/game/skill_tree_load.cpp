#include "game/skill_tree_load.h"

#include <utility>

#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/sim/skill_tree.h"
#include "ro/vfs/lua_chain.h"
#include "ro/vfs/skill_words.h"

#include "game/game_pieces.h"
#include "game/skill_rules.h"

namespace moonclient {

void LoadSkillTree(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  pieces.skill_icons.emplace(vfs);
  auto globals = ro::vfs::ReadLuaChain(vfs, ro::sim::kSkillLuaFiles);
  if (!globals) {
    // ⚠ CET ÉCHEC-CI COÛTE AUSSI LES DESCRIPTIONS : la fonction retourne avant
    // de les lire.
    ro::core::LogWarning(
        "arbre des compétences : tables illisibles ({}) — le grimoire et les "
        "descriptions resteront vides",
        ro::core::Describe(globals.error()));
    return;
  }
  auto tree = ro::sim::SkillTree::FromGlobals(*globals);
  if (!tree) {
    ro::core::LogWarning(
        "arbre des compétences : construction impossible ({}) — le grimoire "
        "restera vide",
        ro::core::Describe(tree.error()));
    return;
  }
  ro::core::LogInfo("arbre des compétences : {} fiche(s)", tree->node_count());

  auto rules = LoadSkillRules(vfs);
  if (rules) {
    ro::core::LogInfo("règles des compétences : {} métier(s), {} de guilde{}", rules->by_job.size(),
                      rules->guild.size(), rules->skill_free ? ", prérequis non vérifiés" : "");
    tree->UseServerRules(std::move(*rules));
  } else {
    ro::core::LogWarning(
        "règles des compétences : {} — la grille se montrera entière, et rien ne s'y montera",
        ro::core::Describe(rules.error()));
  }
  pieces.skill_tree = std::move(*tree);

  auto words = ro::vfs::SkillWords::FromGlobals(*globals);
  if (!words) {
    ro::core::LogWarning("descriptions de compétences : {} — le grimoire s'affichera muet",
                         ro::core::Describe(words.error()));
    return;
  }
  ro::core::LogInfo("descriptions de compétences : {} fiche(s)", words->skill_count());
  pieces.skill_words = std::move(*words);
}

}  // namespace moonclient
