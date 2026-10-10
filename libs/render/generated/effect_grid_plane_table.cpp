// LES NAPPES AU SOL : ce que chaque site d'appel passe a son batisseur.
//
// 1 nappes pour 1 effets, toutes du sous-type 72. L'IMAGE VIENT DU
// BATISSEUR et non du site — l'exception de l'ADR 0019 : il en nomme deux
// en dur et choisit par l'octet. Ce que l'octet decide d'autre vit dans le
// semis, voir effect_grid_plane_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_grid_planes.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_grid_plane_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectGridPlanePattern, 1> kGridPlanes = {{
    {335, "Effect_Build_PoisonWhiteEmitter48", "data\\texture\\effect\\poison_f.bmp", 0},
}};

}  // namespace

std::span<const EffectGridPlanePattern> EffectGridPlanePatternTable() { return kGridPlanes; }

}  // namespace ro::render
