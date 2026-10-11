#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// CE QUE CHAQUE MÉTIER PEUT APPRENDRE, et l'arbre de la guilde, tels que le
// serveur les charge.
//
// `data\yaml_files\skill_trees.yml`, exporté du skill_tree, du guild_skill_tree,
// de la skill_db et de la job_db du serveur par `scripts/yaml_tables/
// skill_trees.py`. L'arbre d'un métier y est déjà complet, héritage compris :
//
//     version: 1
//     skill_free: true
//     jobs:
//       1:
//         - {id: 3, name: SM_TWOHAND, max: 10, needs: [[2, 1]]}
//         - {id: 142, name: NV_FIRSTAID, max: 1, by_points: false}
//     guild:
//       - {id: 10001, name: GD_KAFRACONTRACT, max: 1, needs: [[10000, 1]]}
//
// `needs`, `base_level`, `job_level` et `by_points` manquent quand ils valent
// leur défaut : aucun prérequis, aucun niveau requis, apprenable par points.
//
// 🔴 C'EST LA DONNÉE DE JEU, pas la disposition : les cases et les onglets de la
// fenêtre restent ceux des tables de l'installation.
inline constexpr std::string_view kSkillTreeRulesPath = "data\\yaml_files\\skill_trees.yml";

struct ServerSkillRule {
  std::uint16_t skill_id = 0;
  std::uint16_t max_level = 0;
  // [compétence, rang requis], dans l'ordre du serveur.
  std::vector<std::pair<std::uint16_t, std::uint16_t>> needs;
  std::uint32_t base_level = 0;
  std::uint32_t job_level = 0;
  // Faux quand le serveur ne l'accorde pas contre des points à qui ne l'a pas
  // déjà : compétence de quête, de noces ou d'esprit.
  bool by_points = true;
};

struct ServerJobSkills {
  std::uint32_t job = 0;
  std::vector<ServerSkillRule> skills;
};

class SkillTreeRules {
 public:
  [[nodiscard]] static core::Result<SkillTreeRules> Load(Vfs& vfs);

  // La MÊME lecture, à partir du texte : tout ou rien, un texte mal formé est
  // refusé entier.
  [[nodiscard]] static core::Result<SkillTreeRules> Parse(std::string_view yaml_text);

  // Le serveur ne vérifie ni prérequis ni niveau requis (`player_skillfree`).
  [[nodiscard]] bool skill_free() const noexcept { return skill_free_; }
  // Les métiers, par identifiant croissant.
  [[nodiscard]] std::span<const ServerJobSkills> jobs() const noexcept { return jobs_; }
  [[nodiscard]] std::span<const ServerSkillRule> guild() const noexcept { return guild_; }

 private:
  bool skill_free_ = false;
  std::vector<ServerJobSkills> jobs_;
  std::vector<ServerSkillRule> guild_;
};

}  // namespace ro::vfs
