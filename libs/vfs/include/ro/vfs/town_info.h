#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/core/error.h"

namespace ro::vfs {

class Vfs;

// LES COMMODITÉS D'UNE VILLE : marchands, forge, auberge, Kafra, guide, salon —
// ce que la minimap montre en icônes cliquables.
//
// La table est `data\yaml_files\town_info.yml`, que
// `scripts/yaml_tables/town_info.py` tire de la table de l'installation et de
// son complément :
//
//     version: 1
//     towns:
//       prontera:
//         - {name: Kafra Employee, x: 146, y: 89, facility: kafra}
inline constexpr std::string_view kTownInfoPath = "data\\yaml_files\\town_info.yml";

// La nature d'une commodité.
enum class TownFacility : std::uint8_t {
  kToolDealer = 0,
  kWeaponDealer = 1,
  kArmorDealer = 2,
  kSmith = 3,
  kGuide = 4,
  kInn = 5,
  kKafra = 6,
  kStylist = 7,
};
inline constexpr std::uint8_t kTownFacilityCount = 8;

// Le nom de chaque nature dans la table, dans l'ordre de l'énumération.
inline constexpr auto kTownFacilityNames = std::to_array<std::string_view>(
    {"tool_dealer", "weapon_dealer", "armor_dealer", "smith", "guide", "inn", "kafra", "stylist"});
static_assert(kTownFacilityNames.size() == kTownFacilityCount);

struct TownFacilityEntry {
  std::string name;
  std::int32_t cell_x = 0;
  std::int32_t cell_y = 0;
  TownFacility facility = TownFacility::kToolDealer;
};

class TownInfo {
 public:
  // Lit la table du client. Une table absente n'est pas une faute pour
  // l'appelant : il y a simplement moins d'icônes.
  [[nodiscard]] static core::Result<TownInfo> Load(Vfs& vfs);

  // Le texte de `town_info.yml`. Refuse un texte qui n'est pas du YAML, dont
  // `version` n'est pas 1, sans table `towns`, ou dont une commodité n'a pas sa
  // case (de 0 à 1023) ou porte une nature inconnue.
  [[nodiscard]] static core::Result<TownInfo> Parse(std::string_view yaml_text);

  // Les commodités d'une carte, par son nom interne ; vide si la table n'en dit
  // rien. La casse ne compte pas.
  [[nodiscard]] std::span<const TownFacilityEntry> On(std::string_view map_name) const;

  [[nodiscard]] std::size_t map_count() const noexcept { return by_map_.size(); }

 private:
  std::unordered_map<std::string, std::vector<TownFacilityEntry>> by_map_;
};

}  // namespace ro::vfs
