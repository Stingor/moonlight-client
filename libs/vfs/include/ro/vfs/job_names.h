#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LE NOM AFFICHABLE D'UN MÉTIER : « Lord Knight », pas « knight_h ».
//
// 🔴 CE N'EST PAS LE NOM DE RESSOURCE. Le catalogue de sprites connaît celui des
// DOSSIERS — en coréen, et sans rapport avec ce qu'on montre. Celui-ci se lit
// dans `data\yaml_files\job_names.yml`, que `scripts/yaml_tables/job_names.py`
// tire des noms de l'installation, sur les métiers que connaît le serveur :
//
//     version: 1
//     jobs:
//       0: "Novice"
//       4008: "Lord Knight"
//
// Le commentaire qui suit chaque ligne du fichier y nomme le métier au serveur.
//
// ⚠ C'EST LE NOM DE BASE, partout le même : un nom qui changerait avec le sexe
// ferait diverger la fiche de la fenêtre de groupe.
inline constexpr std::string_view kJobNamesPath = "data\\yaml_files\\job_names.yml";

struct JobNames {
  std::unordered_map<std::uint32_t, std::string> by_id;

  // Le nom d'un métier, ou rien. ⚠ RIEN N'EST PAS UNE PANNE : un métier que la
  // table ne nomme pas s'écrit par ses niveaux seuls.
  [[nodiscard]] std::string_view Find(std::uint32_t job_id) const;

  [[nodiscard]] std::size_t size() const noexcept { return by_id.size(); }
  [[nodiscard]] bool empty() const noexcept { return by_id.empty(); }
};

// Le texte de `job_names.yml`. Refuse un texte qui n'est pas du YAML, dont
// `version` n'est pas 1, sans table `jobs`, ou dont une entrée n'associe pas un
// identifiant à un texte.
[[nodiscard]] core::Result<JobNames> ParseJobNames(std::string_view yaml_text);

// Un fichier ABSENT rend une table vide plutôt qu'une erreur : un métier sans
// nom se lit encore par ses niveaux. Un fichier illisible est une erreur.
[[nodiscard]] core::Result<JobNames> ReadJobNames(Vfs& vfs);

}  // namespace ro::vfs
