#pragma once

#include "ro/core/error.h"
#include "ro/sim/skill_tree.h"
#include "ro/vfs/skill_tree_rules.h"
#include "ro/vfs/vfs.h"

namespace moonclient {

// Les règles lues dans `skill_trees.yml`, dans la forme que l'arbre attend.
[[nodiscard]] ro::sim::SkillRules ToSkillRules(const ro::vfs::SkillTreeRules& read);

// Lit `skill_trees.yml` et le convertit : ce qu'on remet à
// `ro::sim::SkillTree::UseServerRules`.
[[nodiscard]] ro::core::Result<ro::sim::SkillRules> LoadSkillRules(ro::vfs::Vfs& vfs);

}  // namespace moonclient
