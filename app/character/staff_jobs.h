#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/ui/character_staff.h"
#include "ro/vfs/job_names.h"

namespace moonclient {

// LA LISTE DES CLASSES, COMPOSÉE UNE FOIS.
//
// 🔴 UNE FOIS, ET C'EST MESURÉ : le menu déroulant redemanderait cent quarante-
// cinq noms par image tant qu'il est ouvert, et chacun traverse une table lue du
// Lua du client. Le nom d'une classe ne change pas.
//
// ⚠ IL TIENT SES PROPRES TAMPONS parce que les vues pointent dedans : une vue
// posée sur un `std::string` que la croissance du vecteur déplacerait pendrait.
// Les deux se remplissent donc entièrement AVANT qu'on y pointe.
class StaffJobs {
 public:
  // `names` peut être nul : une installation sans la table des métiers montre
  // alors les noms de repli, qui sont les termes du jeu.
  std::span<const ro::ui::StaffJobView> Rebuild(const ro::vfs::JobNames* names);

 private:
  std::vector<std::string> labels_;
  std::vector<ro::ui::StaffJobView> rows_;
};

}  // namespace moonclient
