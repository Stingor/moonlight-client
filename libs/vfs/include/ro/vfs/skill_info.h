#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LA FICHE DE CHAQUE COMPÉTENCE, ET SA DESCRIPTION.
//
// `data\yaml_files\skill_info.yml`, écrit par `scripts/yaml_tables/skill_info.py`
// à partir de la skill_db du serveur — nom interne, coût, portée — et des
// tables de l'installation — nom affiché, zone, réglage du rang, description.
//
// La table pèse un méga-octet et demi, descriptions comprises : elle se lit en
// FORME STRICTE, sans bibliothèque YAML, comme `item_info.yml`. Une ligne par
// compétence :
//
//     version: 1
//     skills: {
//       5: {name: "SM_BASH", label: "Bash", sp: [8, 15], range: [1, 1],
//           scale: [[5, 5], [0, 0]], separate_levels: true,
//           description: ["Bash - ID:5", "…"]},
//     }
//
// Les champs dans cet ordre, chacun omis quand il vaut vide ou faux ; un texte
// entre guillemets aux échappements de JSON, sur une seule ligne. Tout autre
// texte est refusé, même du YAML valide, et le message dit sa ligne.
inline constexpr std::string_view kSkillInfoPath = "data\\yaml_files\\skill_info.yml";

// La version de la forme que ce lecteur comprend.
inline constexpr std::uint32_t kSkillInfoVersion = 1;

// La zone dessinée au sol pour un rang, en cases. Zéro des deux côtés : pas
// de zone déclarée pour ce rang.
struct SkillInfoScale {
  std::uint16_t x = 0;
  std::uint16_t y = 0;

  [[nodiscard]] bool operator==(const SkillInfoScale& other) const = default;
};

struct SkillInfoRecord {
  std::uint16_t skill_id = 0;
  // Le nom INTERNE, « SM_BASH » : celui de l'icône et des barres sauvées.
  std::string name;
  // Le nom montré au joueur.
  std::string label;
  // Par rang : le rang n vaut l'entrée n-1. Vides pour qui n'en a pas.
  std::vector<std::uint16_t> sp_by_level;
  std::vector<std::uint16_t> range_by_level;
  std::vector<SkillInfoScale> scale_by_level;
  // Le rang de lancement se règle : l'effet dépend du rang lancé.
  bool separate_levels = false;
  // Ses lignes, balisage compris (`^RRGGBB`, `<NAVI>`…).
  std::vector<std::string> description;

  [[nodiscard]] bool operator==(const SkillInfoRecord& other) const = default;
};

// Les fiches, dans l'ordre du texte. Tout ou rien.
[[nodiscard]] core::Result<std::vector<SkillInfoRecord>> ParseSkillInfo(std::string_view text);
[[nodiscard]] core::Result<std::vector<SkillInfoRecord>> LoadSkillInfo(Vfs& vfs);

}  // namespace ro::vfs
