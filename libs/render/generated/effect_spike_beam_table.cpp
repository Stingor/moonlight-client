// LES FAISCEAUX EN PIQUE : ce que chaque site d'appel passe a son batisseur.
//
// Une ligne par site d'appel du batisseur.
//
// 1 faisceaux pour 1 effets, tous du sous-type 164. Le batisseur ne recoit
// qu'une image : le semis est le meme pour tous, et ce qui fait la figure
// vient du TIC de naissance. Voir effect_spike_beam_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_spike_beams.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_spike_beam_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectSpikeBeamPattern, 1> kSpikeBeams = {{
    {897, "Effect_InitAngularBurst", "data\\texture\\effect\\shockwave_c.bmp"},
}};

}  // namespace

std::span<const EffectSpikeBeamPattern> EffectSpikeBeamPatternTable() { return kSpikeBeams; }

}  // namespace ro::render
