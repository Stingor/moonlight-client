// LES DEUX ARMES D'UN AMBIDEXTRE, chacune sous le sprite de son objet.
//
// Chaque main est cherchée comme si elle tenait son arme seule : c'est la même
// recherche, nom propre puis famille, que pour une arme unique.

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/render/sprite_catalog.h"

#include "sprite_catalog_internal.h"

namespace ro::render {

std::optional<DualWieldFiles> DualWieldSpritePaths(const SpriteCatalog& catalog,
                                                   const ActorLook& look) {
  if (look.shield == 0 || catalog_internal::WeaponClassOf(catalog, look.shield) <= 0) {
    return std::nullopt;
  }
  ActorLook left_alone = look;
  left_alone.weapon = look.shield;
  left_alone.shield = 0;
  std::vector<std::string> off_hand = WeaponSpritePaths(catalog, left_alone);
  if (off_hand.empty()) {
    return std::nullopt;
  }
  DualWieldFiles files;
  files.off_hand = std::move(off_hand);
  if (look.weapon != 0) {
    ActorLook right_alone = look;
    right_alone.shield = 0;
    files.main_hand = WeaponSpritePaths(catalog, right_alone);
  }
  return files;
}

}  // namespace ro::render
