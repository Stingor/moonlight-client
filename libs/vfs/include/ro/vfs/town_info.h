#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/lua_table.h"

namespace ro::vfs {

class Vfs;

// LES COMMODITÉS D'UNE VILLE : marchands, forge, auberge, Kafra, guide, salon —
// ce que la minimap montre en icônes cliquables.
//
// La table vit dans l'installation et non dans l'archive, en Lua EN CLAIR :
//
//   mapNPCInfoTable = {
//     prontera = { { name = [=[Kafra Employee]=], X = 146, Y = 89, TYPE = 6 }, … },
//   }
//
// Un complément, `_C`, déclare `mapNPCInfoTable_C` sous la même forme ; chacune
// de ses cartes REMPLACE la liste entière de la table principale — c'est ce que
// fait la fonction de fusion que le complément appelle, et qu'on ne lit pas.
inline constexpr std::string_view kTownInfoPath = "SystemEN\\Towninfo.lub";
inline constexpr std::string_view kTownInfoCustomPath = "SystemEN\\Towninfo_C.lub";
inline constexpr std::string_view kTownInfoTable = "mapNPCInfoTable";
inline constexpr std::string_view kTownInfoCustomTable = "mapNPCInfoTable_C";

// La nature d'une commodité, le champ `TYPE`.
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

struct TownFacilityEntry {
  std::string name;
  std::int32_t cell_x = 0;
  std::int32_t cell_y = 0;
  TownFacility facility = TownFacility::kToolDealer;
};

class TownInfo {
 public:
  // Lit la table de l'installation, puis son complément s'il existe. Une table
  // absente n'est pas une faute : il y a simplement moins d'icônes.
  [[nodiscard]] static core::Result<TownInfo> Load(Vfs& vfs);

  // La même lecture depuis des sources en clair, pour les essais. `custom` peut
  // être vide.
  [[nodiscard]] static core::Result<TownInfo> ParseSources(std::string_view main,
                                                           std::string_view custom);

  // Les commodités d'une carte, par son nom interne ; vide si la table n'en dit
  // rien. La casse ne compte pas.
  [[nodiscard]] std::span<const TownFacilityEntry> On(std::string_view map_name) const;

  [[nodiscard]] std::size_t map_count() const noexcept { return by_map_.size(); }

 private:
  void Merge(const formats::LuaTable& globals, std::string_view table_name);

  std::unordered_map<std::string, std::vector<TownFacilityEntry>> by_map_;
};

}  // namespace ro::vfs
