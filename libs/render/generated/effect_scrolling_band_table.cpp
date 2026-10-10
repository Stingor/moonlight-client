// LES CASCADES DU CLIENT : ce que chaque site d'appel passe a son batisseur.
//
// Site d'appel par site d'appel, chez ses 2 batisseurs.
//
// 13 cascades, toutes du sous-type 79 et toutes dessinees par la bande
// qui defile. Le second octet n'a pas le meme sens chez les deux batisseurs :
// voir effect_scrolling_band_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_scrolling_bands.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_scrolling_band_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectScrollingBandPattern, 13> kScrollingBands = {{
    {349,
     "Effect_InitWaterfall",
     {{"data\\texture\\effect\\waterfall11.tga", "data\\texture\\effect\\waterfall12.tga",
       "data\\texture\\effect\\waterfall13.tga"}},
     EffectScrollingBandBuilder::kInitWaterfall,
     0,
     0},
    {350,
     "Effect_InitWaterfall",
     {{"data\\texture\\effect\\waterfall11.tga", "data\\texture\\effect\\waterfall12.tga",
       "data\\texture\\effect\\waterfall13.tga"}},
     EffectScrollingBandBuilder::kInitWaterfall,
     1,
     0},
    {351,
     "Effect_InitWaterfall",
     {{"data\\texture\\effect\\waterfall11.tga", "data\\texture\\effect\\waterfall12.tga",
       "data\\texture\\effect\\waterfall13.tga"}},
     EffectScrollingBandBuilder::kInitWaterfall,
     0,
     1},
    {352,
     "Effect_InitWaterfall",
     {{"data\\texture\\effect\\waterfall11.tga", "data\\texture\\effect\\waterfall12.tga",
       "data\\texture\\effect\\waterfall13.tga"}},
     EffectScrollingBandBuilder::kInitWaterfall,
     1,
     1},
    {353,
     "Effect_InitWaterfall",
     {{"data\\texture\\effect\\waterfall31.tga", "data\\texture\\effect\\waterfall32.tga",
       "data\\texture\\effect\\waterfall33.tga"}},
     EffectScrollingBandBuilder::kInitWaterfall,
     0,
     0},
    {354,
     "Effect_InitWaterfall",
     {{"data\\texture\\effect\\waterfall31.tga", "data\\texture\\effect\\waterfall32.tga",
       "data\\texture\\effect\\waterfall33.tga"}},
     EffectScrollingBandBuilder::kInitWaterfall,
     1,
     0},
    {355,
     "Effect_InitWaterfall",
     {{"data\\texture\\effect\\waterfall31.tga", "data\\texture\\effect\\waterfall32.tga",
       "data\\texture\\effect\\waterfall33.tga"}},
     EffectScrollingBandBuilder::kInitWaterfall,
     0,
     1},
    {356,
     "Effect_InitWaterfall",
     {{"data\\texture\\effect\\waterfall31.tga", "data\\texture\\effect\\waterfall32.tga",
       "data\\texture\\effect\\waterfall33.tga"}},
     EffectScrollingBandBuilder::kInitWaterfall,
     1,
     1},
    {557,
     "Effect_Build_Waterfall",
     {{"data\\texture\\effect\\waterfall11.tga", "data\\texture\\effect\\waterfall12.tga",
       "data\\texture\\effect\\waterfall13.tga"}},
     EffectScrollingBandBuilder::kBuildWaterfall,
     0,
     0},
    {558,
     "Effect_Build_Waterfall",
     {{"data\\texture\\effect\\waterfall11.tga", "data\\texture\\effect\\waterfall12.tga",
       "data\\texture\\effect\\waterfall13.tga"}},
     EffectScrollingBandBuilder::kBuildWaterfall,
     1,
     0},
    {559,
     "Effect_Build_Waterfall",
     {{"data\\texture\\effect\\waterfall11.tga", "data\\texture\\effect\\waterfall12.tga",
       "data\\texture\\effect\\waterfall13.tga"}},
     EffectScrollingBandBuilder::kBuildWaterfall,
     0,
     1},
    {560,
     "Effect_Build_Waterfall",
     {{"data\\texture\\effect\\waterfall11.tga", "data\\texture\\effect\\waterfall12.tga",
       "data\\texture\\effect\\waterfall13.tga"}},
     EffectScrollingBandBuilder::kBuildWaterfall,
     1,
     1},
    {711,
     "Effect_Build_Waterfall",
     {{"data\\texture\\effect\\alpha_down.tga", "data\\texture\\effect\\alpha_down.tga",
       "data\\texture\\effect\\alpha_down.tga"}},
     EffectScrollingBandBuilder::kBuildWaterfall,
     0,
     10},
}};

}  // namespace

std::span<const EffectScrollingBandPattern> EffectScrollingBandPatternTable() {
  return kScrollingBands;
}

}  // namespace ro::render
