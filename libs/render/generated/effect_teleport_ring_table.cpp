// LES ANNEAUX DE TELEPORTATION : les noeuds du sous-type 57.
//
// 1 identifiant(s), UN SEUL batisseur.
// Le site ne choisit que l'image et le parametre d'echelle ; les quatre
// pistes suivent le batisseur.
//
// ENGENDRE par scripts/gen_client_effect_teleport_rings.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_teleport_rings_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectTeleportRingPattern, 1> kTeleportRings = {{
    {304, "Effect_Build_TeleportRingsScaled", "data\\texture\\effect\\Magic_Violet.tga",
     0},  // EF_TELEPORTATION2
}};

}  // namespace

std::span<const EffectTeleportRingPattern> EffectTeleportRingPatternTable() {
  return kTeleportRings;
}

}  // namespace ro::render
