// LES GERBES : les noeuds du sous-type 146.
//
// 3 identifiants et 4 lignes : le dernier appelle son batisseur DEUX
// FOIS de suite, sur deux images differentes.
//
// ENGENDRE par scripts/gen_client_effect_anchored_skirts.py - ne pas
// modifier a la main.

#include <array>
#include <string_view>

#include "ro/render/effect_anchored_skirt_seed.h"

namespace ro::render {
namespace {

constexpr std::array<std::string_view, 13> kBubbleFrames = {{
    "data\\texture\\effect\\W_bubble02.tga",
    "data\\texture\\effect\\W_bubble04.tga",
    "data\\texture\\effect\\W_bubble06.tga",
    "data\\texture\\effect\\W_bubble08.tga",
    "data\\texture\\effect\\W_bubble10.tga",
    "data\\texture\\effect\\W_bubble12.tga",
    "data\\texture\\effect\\W_bubble14.tga",
    "data\\texture\\effect\\W_bubble16.tga",
    "data\\texture\\effect\\W_bubble18.tga",
    "data\\texture\\effect\\W_bubble20.tga",
    "data\\texture\\effect\\W_bubble22.tga",
    "data\\texture\\effect\\W_bubble24.tga",
    "data\\texture\\effect\\W_bubble26.tga",
}};

constexpr std::array<std::string_view, 1> kWaveOne = {{
    "data\\texture\\effect\\new_sonicwave\\new_sonicwave\\son_wave_01.tga",
}};

constexpr std::array<std::string_view, 1> kWaveZero = {{
    "data\\texture\\effect\\new_sonicwave\\new_sonicwave\\son_wave_00.tga",
}};

constexpr std::array<EffectAnchoredSkirtPattern, 4> kAnchoredSkirts = {{
    {832,
     "Effect_InitWaterSprayDirected",
     kBubbleFrames,
     30.0F,
     3.0F,
     0.0F,
     {0, 0, 0}},  // treize bulles, sans variante
    {943,
     "Effect_InitWaterSprayDirected",
     kBubbleFrames,
     30.0F,
     3.0F,
     7.0F,
     {0, 0, 0}},  // treize bulles, variante sept
    {1678,
     "Effect_InitWaterSprayBurst3",
     kWaveOne,
     20.0F,
     9.0F,
     0.0F,
     {0, 40, 20}},  // onde 01, le plus large, aux tics 0, 20 et 40
    {1678,
     "Effect_InitWaterSprayBurst3",
     kWaveZero,
     30.0F,
     6.0F,
     0.0F,
     {0, 40, 20}},  // onde 00, le plus long, aux tics 0, 20 et 40
}};

}  // namespace

std::span<const EffectAnchoredSkirtPattern> EffectAnchoredSkirtPatternTable() {
  return kAnchoredSkirts;
}

}  // namespace ro::render
