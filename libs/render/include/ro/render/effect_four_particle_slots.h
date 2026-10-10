#pragma once

#include <cstddef>

#include <glm/glm.hpp>

#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// OÙ LE CLIENT RANGE LES QUATRE DÉCALAGES D'UNE PISTE À QUATRE PARTICULES.
//
// ⚠ CE N'EST PAS UNE SUITE. La première particule vit au MILIEU de la queue, la
// deuxième dans l'ANCRE, la troisième à la FIN de la queue et la quatrième à son
// DÉBUT. Deux dessinateurs qui n'ont rien d'autre en commun — la gerbe de
// quatre quads et les faces de polyèdre — suivent cet ordre, et leurs deux
// bâtisseurs aussi. Les ranger en suite les mélangerait.
//
// ⚠ ET LES ORDONNÉES SONT DANS LE REPÈRE DU CLIENT, qui DESCEND. C'est à la
// figure de les rendre au nôtre, au dernier moment.
//
// En-tête public parce que les semis en ont besoin autant que les figures ;
// il ne porte que ces deux accès et le compte.

inline constexpr std::size_t kEffectFourParticles = 4;

[[nodiscard]] glm::vec3 EffectFourParticleOffsetOf(const EffectTrack& track, std::size_t particle);
void SetEffectFourParticleOffset(EffectTrack& track, std::size_t particle, glm::vec3 offset);

}  // namespace ro::render
