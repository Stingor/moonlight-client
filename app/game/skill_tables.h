#pragma once

#include <optional>
#include <span>
#include <vector>

#include "ro/core/error.h"
#include "ro/sim/skill_tree.h"
#include "ro/vfs/skill_info.h"
#include "ro/vfs/skill_layout.h"
#include "ro/vfs/skill_words.h"
#include "ro/vfs/vfs.h"

namespace moonclient {

// Les fiches lues, dans la forme que l'arbre attend. Les descriptions n'y
// passent pas : elles vont à `ro::vfs::SkillWords`.
[[nodiscard]] std::vector<ro::sim::SkillNode> ToSkillNodes(
    std::span<const ro::vfs::SkillInfoRecord> records);

// La disposition lue, dans la forme que l'arbre attend.
[[nodiscard]] ro::sim::SkillTreeLayout ToSkillTreeLayout(const ro::vfs::SkillLayout& layout);

// L'arbre et les descriptions, montés de `skill_info.yml` et `skill_layout.yml`.
struct SkillTables {
  ro::sim::SkillTree tree;
  ro::vfs::SkillWords words;
  // Pourquoi la disposition manque, quand elle manque : l'arbre se monte quand
  // même, sans grille — les noms, les icônes et les descriptions tiennent.
  std::optional<ro::core::Error> layout_error;
};

// ⚠ SANS `skill_info.yml`, RIEN : ni fiche ni description. Les règles du
// serveur (`LoadSkillRules`) se lisent à part et se remettent à l'arbre.
[[nodiscard]] ro::core::Result<SkillTables> LoadSkillTables(ro::vfs::Vfs& vfs);

}  // namespace moonclient
