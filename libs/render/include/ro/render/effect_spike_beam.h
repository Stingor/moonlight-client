#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE FAISCEAU EN PIQUE : un ruban plat qui part de l'ancre et file droit.
//
// Le dessinateur du sous-type 164. Il sort par la voie A, jusqu'à six fois par piste vive.
//
// ⚠ SES QUATRE COINS SONT DEUX PAIRES DIAMÉTRALEMENT OPPOSÉES, à vingt degrés
// de part et d'autre de l'inclinaison : moins vingt et plus cent soixante d'un
// côté, plus vingt et plus deux cents de l'autre. C'est cela, la pique. Chercher
// quatre angles quelconques n'en trouverait pas la symétrie, et un quad dont les
// coins ne s'opposent pas se vrille.
//
// ⚠ LE COIN À PLUS DEUX CENTS EST L'OPPOSÉ DU COIN À PLUS VINGT : l'inclinaison
// plus vingt, décalée d'un demi-tour. Il se lit dans les mêmes tables de sinus
// et de cosinus que les autres.
//
// ⚠ SON RUBAN EST PLAT : l'ordonnée de chaque coin ne dépend PAS du segment. Le
// faisceau avance dans le plan du sol — d'un pas de la clé à chaque segment — et
// sa section ne bouge pas. Lui donner une ordonnée qui suive le segment en
// ferait un tube, ce qu'il n'est pas.

// COMBIEN DE SEGMENTS AU PLUS, et où le faisceau s'arrête.
//
// ⚠ LA BORNE EST SUR LA PORTÉE, PAS SUR LE COMPTE. Le client sort de sa boucle
// dès que la portée dépasse cent, si bien qu'un faisceau à grand pas n'a que
// deux ou trois segments. Compter six segments quoi qu'il arrive le ferait
// dépasser sa longueur.
inline constexpr std::size_t kSpikeBeamMaxSegments = 6;
inline constexpr float kSpikeBeamMaxReach = 100.0F;

// LES QUATRE ANGLES DE LA SECTION, en degrés depuis l'inclinaison, dans l'ordre
// où le client pousse ses coins.
inline constexpr std::array<std::int32_t, 4> kSpikeBeamCornerDegrees = {200, -20, 20, 160};

// LES CASES QUE CETTE FIGURE LIT, et leur sens lui appartient (ADR 0017) : la clé
// porte le RAYON de la section ET le pas du faisceau, la première case la hauteur
// commune aux quatre coins, la seconde l'INDICE DE L'IMAGE.
//
// ⚠ C'EST LE MOUVEMENT QUI DONNE SON SENS À LA SECONDE CASE : il la fait croître
// d'une unité par tic et la ramène à zéro dès qu'elle atteint le NOMBRE D'IMAGES
// du nœud.
inline constexpr std::size_t kSlotOfSpikeBeamHeight = 0;
inline constexpr std::size_t kSlotOfSpikeBeamImage = 1;

// LA TEINTE, la même pour tous : le client la passe en dur, et c'est un bleu
// pâle.
inline constexpr std::array<std::uint8_t, 3> kSpikeBeamTint = {0x69, 0x69, 0xFF};

// LES QUADS D'UNE PISTE, du plus proche au plus lointain, ou un lot VIDE.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeSpikeBeamQuads(const EffectTrack& track,
                                                                 glm::vec3 node_origin,
                                                                 const EffectOwnerTint& owner = {});

}  // namespace ro::render
