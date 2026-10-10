// LES QUATRE ANNEAUX : les noeuds du sous-type 41.
//
// 5 identifiants, DEUX batisseurs - et le second ne ressemble au
// premier que de loin : ses quatre pistes partent DEJA ouvertes, son
// inclinaison DECROIT d'une piste a l'autre, et son style fait tomber
// l'alpha.
//
// ENGENDRE par scripts/gen_client_effect_ring_quartets.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_ring_quartet_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectRingQuartetPattern, 5> kRingQuartets = {{
    {231, "Effect_Build_Rings4FromParam29", "data\\texture\\effect\\ring_blue.tga", 0, 30, 50, 2.0F,
     89, 0, 0, 4.0F},
    // octet 0 : alpha 50, rayon 2, variante 4
    {247, "Effect_Build_Rings4FromParam29", "data\\texture\\effect\\ring_blue.tga", 0, 30, 70,
     11.0F, 89, 0, 0, 4.0F},
    // octet 1 : alpha 70, rayon 11, variante 4
    {248, "Effect_Build_Rings4FromParam29b", "data\\texture\\effect\\ring_yellow.tga", 200, 0, 40,
     6.0F, 83, -1, 1, 5.0F},
    // second batisseur : quatre pistes ouvertes d'emblee, style UN
    {259, "Effect_Build_Rings4FromParam29", "data\\texture\\effect\\magic_green.tga", 0, 30, 50,
     2.0F, 89, 0, 0, 7.0F},
    // octet 2 : alpha 50, rayon 2, variante 7
    {260, "Effect_Build_Rings4FromParam29", "data\\texture\\effect\\ring_red.tga", 0, 30, 50, 2.0F,
     89, 0, 0, 6.0F},
    // octet 3 : alpha 50, rayon 2, variante 6
}};

}  // namespace

std::span<const EffectRingQuartetPattern> EffectRingQuartetPatternTable() { return kRingQuartets; }

}  // namespace ro::render
