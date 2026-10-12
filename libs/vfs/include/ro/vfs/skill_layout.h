#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LA DISPOSITION DE LA FENÊTRE DES COMPÉTENCES : quelle compétence occupe quelle
// case de la grille de chaque métier, de quel métier chaque métier descend, et
// le nom des onglets.
//
// `data\yaml_files\skill_layout.yml`, tiré des tables de l'installation par
// `scripts/yaml_tables/skill_info.py` :
//
//     version: 1
//     grids:
//       1: {1: 2, 3: 5, 8: 3}
//     inherit:
//       7: 1
//     origins:
//       4060: 4054
//     tab_names:
//       0: ["1st", "2nd", "3rd", "4th"]
//
// 🔴 CE N'EST PAS CE QUI S'APPREND : `skill_trees.yml` (le serveur) le dit. Une
// case qu'il ne donne pas au métier se tait.
inline constexpr std::string_view kSkillLayoutPath = "data\\yaml_files\\skill_layout.yml";

// Une case : son rang, à partir de zéro sur une grille de sept colonnes, et la
// compétence qu'elle montre.
struct SkillLayoutSlot {
  std::uint16_t position = 0;
  std::uint16_t skill_id = 0;

  [[nodiscard]] bool operator==(const SkillLayoutSlot& other) const = default;
};

struct SkillLayoutGrid {
  std::uint32_t job = 0;
  // Par rang de case croissant.
  std::vector<SkillLayoutSlot> slots;
};

// `job` descend de `parent`.
struct SkillLayoutLink {
  std::uint32_t job = 0;
  std::uint32_t parent = 0;
};

struct SkillLayoutTabNames {
  std::uint32_t job = 0;
  std::vector<std::string> names;
};

struct SkillLayout {
  // Par métier croissant, comme les trois listes suivantes.
  std::vector<SkillLayoutGrid> grids;
  // La chaîne des onglets : de quel métier chacun descend.
  std::vector<SkillLayoutLink> inherit;
  // De quel métier ORDINAIRE descend un métier transcendant, dont il prend la
  // grille quand il n'en a pas.
  std::vector<SkillLayoutLink> origins;
  std::vector<SkillLayoutTabNames> tab_names;
};

// Tout ou rien : un texte mal formé est refusé entier.
[[nodiscard]] core::Result<SkillLayout> ParseSkillLayout(std::string_view yaml_text);
[[nodiscard]] core::Result<SkillLayout> LoadSkillLayout(Vfs& vfs);

}  // namespace ro::vfs
