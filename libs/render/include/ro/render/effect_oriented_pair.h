#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA PAIRE ORIENTÉE : la figure des sous-types 155 ET 170.
//
// UN SEUL dessinateur pour deux sous-types — leurs mouvements diffèrent, leur
// figure non : le 155 suit un essaim de billboards et le 170 le même essaim
// élargi, et c'est pourquoi les deux sous-types ont chacun leur forme de
// troupe.
//
// ⚠ « PAIRE » NE DIT QU'UN QUART DE CE QU'IL FAIT. Il y a bien une PAIRE de
// quads, mais il y en a DEUX paires — une par POSITION —, et l'une des marques
// n'en peint qu'un seul. Compter deux quads par piste en perdrait la moitié.
//
// ⚠ ET SES HUIT « TABLES » N'EN SONT QUE DEUX : les tables de sinus et de
// cosinus du client aux index quarante-cinq, cent trente-cinq, deux cent
// vingt-cinq et trois cent quinze. Ses quatre coins sont donc un carré TOURNÉ
// D'UN HUITIÈME DE TOUR, là où celui du sous-type 32 est droit. Deux carrés, et
// ils ne se posent pas pareil.

// DE COMBIEN LE CARRÉ EST TOURNÉ, et de combien ses coins s'écartent.
inline constexpr std::int32_t kOrientedPairFirstCornerDegrees = 45;
inline constexpr std::int32_t kOrientedPairDegreesPerCorner = 90;

// ⚠ LE QUAD PÂLE N'A QUE LE TIERS DE L'ALPHA, et il passe le mélange UN quand le
// second passe zéro. C'est lui qui fait le halo ; les peindre au même alpha
// doublerait l'opacité au lieu de l'adoucir.
inline constexpr std::int16_t kOrientedPairPaleAlphaDivisor = 3;

// LES DEUX MARQUES QUI NE PEIGNENT QU'UN QUAD, et qui RETOURNENT son image.
inline constexpr std::uint8_t kOrientedPairFlippedMark = 2;
inline constexpr std::uint8_t kOrientedPairFlippedOnceMark = 3;

// CE QU'UNE POSITION DEMANDE POUR SE DESSINER.
struct EffectOrientedPairRequest {
  // Où le carré se centre, et de quel rayon.
  glm::vec3 center = {0.0F, 0.0F, 0.0F};
  float radius = 0.0F;
  std::int16_t alpha = 0;
  // LES DEUX ANGLES DE ROTATION, pris dans les champs de STYLE des pistes zéro
  // et UN — comme le carré concentrique, et pour la même raison : le mouvement
  // les tire de la caméra.
  std::int16_t turn_degrees = 0;
  std::int16_t tilt_degrees = 0;
  std::uint8_t mark = 0;
  // ⚠ VRAI POUR LA SECONDE DES DEUX POSITIONS. La marque TROIS y est MUETTE : la
  // première la traite comme la deux, la seconde l'écarte. Traiter les deux
  // pareil ajouterait un carré que le client ne montre pas.
  bool is_second_position = false;
};

// ⚠ ET CHACUN DE SES QUADS PASSE DEVANT TOUT, à toutes les marques : voir
// `EffectQuadDepth`.

// LES UN OU DEUX QUADS D'UNE POSITION.
//
// Rend une liste VIDE quand l'alpha ou le rayon est nul, et quand la marque
// trois se présente à la seconde position.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeOrientedPairQuads(
    const EffectOrientedPairRequest& request, glm::vec3 node_origin);

// LES DEUX POSITIONS D'UNE PISTE, lues dans ses cases.
//
// ⚠ DEUX POSITIONS ET DEUX ALPHAS. Le même carré est peint une première fois
// autour de trois cases de la QUEUE, une seconde autour de l'ANCRE ; leurs
// alphas sont distincts — les cases douze et treize. Une seule position en
// perdrait la moitié, et un seul alpha les ferait s'éteindre ensemble.
[[nodiscard]] std::array<EffectOrientedPairRequest, 2> EffectOrientedPairRequestsOf(
    const EffectTrack& track, std::int16_t turn_degrees, std::int16_t tilt_degrees);

}  // namespace ro::render
