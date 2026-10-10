#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE MUR TRIPLE : la figure du sous-type 75.
//
// Il y a bien TROIS quads, mais ce ne sont pas trois figures — c'est UNE
// figure peinte trois fois à UN DEGRÉ D'ÉCART, dont deux au TIERS de l'alpha.
// Le « glow » n'est pas une teinte mais ce flou-là : un bord épais obtenu en
// répétant le même mur.

// COMBIEN DE FOIS LE MUR SE RÉPÈTE, et de combien ses copies s'écartent.
inline constexpr std::size_t kTripleWallCopies = 3;
inline constexpr std::int32_t kTripleWallSpreadDegrees = 1;

// ⚠ LES DEUX COPIES DES CÔTÉS N'ONT QUE LE TIERS DE L'ALPHA. C'est ce qui fait
// le halo : un bord franc au milieu, deux ombres de part et d'autre.
inline constexpr std::int16_t kTripleWallSideAlphaDivisor = 3;

// LES DEUX HAUTEURS DE CHAQUE COPIE, en parts de la clé de la piste.
//
// ⚠ ELLES DIFFÈRENT ENTRE LE MILIEU ET LES CÔTÉS, si bien que les trois murs
// n'ont pas la même pente. Leur donner la même les superposerait exactement, et
// le halo disparaîtrait derrière le mur franc.
inline constexpr float kTripleWallMiddleInnerRise = 0.2F;
inline constexpr float kTripleWallMiddleOuterRise = 1.0F;
inline constexpr float kTripleWallSideInnerRise = 0.1F;
inline constexpr float kTripleWallSideOuterRise = 0.4F;

// LES TROIS MURS D'UNE PISTE.
//
// Chacun est un rectangle DEBOUT entre le rayon intérieur — la case zéro — et
// l'extérieur, qui vaut cette case PLUS le rayon à l'ancre. Ses deux bords bas
// sont au sol ; ses bords hauts ne sont pas à la même hauteur, ce qui penche le
// mur vers l'extérieur.
//
// Rend une liste VIDE quand la piste est éteinte ou son alpha nul — c'est la
// garde du client, mot pour mot.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeTripleWallQuads(const EffectTrack& track,
                                                                  glm::vec3 anchor);

}  // namespace ro::render
