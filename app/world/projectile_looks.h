#pragma once

namespace moonclient {

// CE QUE LA PAGE « Graphics » DIT DE L'ALLURE DES PROJECTILES, lu à chaque
// image par ceux qui les tirent.
struct ProjectileLooks {
  // LA LUEUR ÉLÉMENTAIRE des flèches en vol : voir `ElementGlowHueOf`.
  bool element_glow = true;
  // LE TIR DE BAGUETTE AMÉLIORÉ : le trait d'une baguette devient une boule
  // d'énergie. Voir `WandOrbRgbOf`.
  bool wand_orb = true;
};

}  // namespace moonclient
