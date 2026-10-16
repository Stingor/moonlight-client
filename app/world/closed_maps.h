#pragma once

#include <set>
#include <string>
#include <string_view>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace moonclient {

// LES CARTES FERMÉES : cavernes, donjons, souterrains, où le ciel ne se voit
// pas. Le cycle jour/nuit du joueur ne s'y applique pas, et la lumière de la
// carte reste telle que son RSW la déclare.
//
// Elles s'ajoutent aux intérieurs que l'installation déclare dans
// `data\indoorrswtable.txt` : ceux-là changent aussi de caméra, une caverne
// garde celle du plein air.
//
// La table est `data\yaml_files\closed_maps.yml`, tenue à la main.
inline constexpr std::string_view kClosedMapsPath = "data\\yaml_files\\closed_maps.yml";

class ClosedMaps {
 public:
  // Le texte de `closed_maps.yml`. Refuse un texte qui n'est pas du YAML, dont
  // la version n'est pas gérée, ou dont « maps » n'est pas une liste de noms.
  [[nodiscard]] static ro::core::Result<ClosedMaps> Parse(std::string_view yaml_text);

  // Vrai pour une carte de la table, nommée sans extension.
  [[nodiscard]] bool Closes(std::string_view map_name) const;

  [[nodiscard]] std::size_t size() const noexcept { return maps_.size(); }

 private:
  std::set<std::string, std::less<>> maps_;
};

// La table de l'installation. Absente ou illisible, elle est vide, et le dit au
// journal : le cycle s'applique alors partout en plein air.
[[nodiscard]] ClosedMaps LoadClosedMaps(ro::vfs::Vfs& vfs);

}  // namespace moonclient
