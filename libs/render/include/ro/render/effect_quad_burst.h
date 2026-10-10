#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA RAFALE DE QUADS TEINTÉS : la figure des sous-types 29 et 80.
//
// Le plus gros dessinateur du lot, dont la structure tient pourtant en une
// phrase : chaque piste porte QUATRE particules, chacune est le MÊME carré
// tourné vers la caméra, translaté par son propre décalage, et sa teinte comme
// son alpha viennent de la marque du nœud.
//
// ⚠ QUATRE PARTICULES PAR PISTE, ET QUATRE PISTES : seize carrés par nœud, et
// jusqu'à trente-deux quads puisque deux marques peignent le leur DEUX FOIS.
// N'en compter qu'un par piste donnerait le quart de la matière.

// COMBIEN DE PARTICULES une piste porte.
inline constexpr std::size_t kQuadBurstParticles = 4;

// DE COMBIEN LE CARRÉ EST TOURNÉ, et de combien ses coins s'écartent.
//
// ⚠ QUARANTE-CINQ DEGRÉS, encore : le client lit ses tables de sinus et de
// cosinus aux index 45, 135, 225 et 315. C'est le même carré que celui de la
// paire orientée et de la taillade — trois figures, une seule géométrie de
// départ.
inline constexpr std::int32_t kQuadBurstFirstCornerDegrees = 45;
inline constexpr std::int32_t kQuadBurstDegreesPerCorner = 90;

// LES MARQUES QUE LE CLIENT COMPARE, une à une. Elles ne se suivent pas et ne
// veulent rien dire ensemble : chacune est un effet du jeu.
inline constexpr std::uint8_t kQuadBurstMarkWhite = 0;
inline constexpr std::uint8_t kQuadBurstMarkBlue = 1;
inline constexpr std::uint8_t kQuadBurstMarkYellow = 3;
inline constexpr std::uint8_t kQuadBurstMarkPlainWhite = 10;
inline constexpr std::uint8_t kQuadBurstMarkGrey = 11;
inline constexpr std::uint8_t kQuadBurstMarkFading = 15;
inline constexpr std::uint8_t kQuadBurstMarkRising = 14;
inline constexpr std::uint8_t kQuadBurstMarkScattering = 16;

// LES SOUS-MARQUES de la marque quinze, qui choisissent sa teinte.
inline constexpr std::uint8_t kQuadBurstShadeOrange = 7;
inline constexpr std::uint8_t kQuadBurstShadeYellow = 8;

// LES TEINTES, une par cas mesuré.
inline constexpr std::array<std::uint8_t, 3> kQuadBurstWhite = {255, 255, 255};
inline constexpr std::array<std::uint8_t, 3> kQuadBurstBlue = {80, 80, 255};
inline constexpr std::array<std::uint8_t, 3> kQuadBurstYellow = {255, 255, 55};
inline constexpr std::array<std::uint8_t, 3> kQuadBurstGrey = {155, 155, 155};
inline constexpr std::array<std::uint8_t, 3> kQuadBurstGreen = {70, 120, 100};
inline constexpr std::array<std::uint8_t, 3> kQuadBurstOrange = {250, 70, 100};
inline constexpr std::array<std::uint8_t, 3> kQuadBurstLemon = {250, 250, 10};

// OÙ VIT LA POSITION D'UNE PARTICULE.
//
// ⚠ LES QUATRE DÉCALAGES NE SONT PAS RANGÉS À LA SUITE : le premier occupe les
// cases de queue trois à cinq, le DEUXIÈME l'ANCRE, le troisième les cases six
// à huit, et le quatrième les cases ZÉRO à deux. Les lire dans l'ordre des
// cases mélangerait les quatre particules — et leurs mouvements, qui diffèrent
// par leurs signes.
struct EffectQuadBurstOffset {
  float sideways = 0.0F;
  // ⚠ LA HAUTEUR EST DANS LE REPÈRE DU CLIENT, qui DESCEND : c'est elle que
  // l'alpha compare à des seuils négatifs, et la figure ne la retourne qu'au
  // moment de poser ses coins.
  float depth_below = 0.0F;
  float forward = 0.0F;
};

[[nodiscard]] EffectQuadBurstOffset EffectQuadBurstOffsetOf(const EffectTrack& track,
                                                            std::size_t particle);
void SetEffectQuadBurstOffset(EffectTrack& track, std::size_t particle,
                              const EffectQuadBurstOffset& offset);

// L'ALPHA D'UNE PARTICULE, d'après sa hauteur et la marque du nœud.
//
// ⚠ QUATRE RAMPES, ET LA PREMIÈRE PARTICULE N'A PAS LES MÊMES SEUILS QUE LES
// TROIS AUTRES pour la marque ONZE : moins quarante chez elle, moins vingt
// chez elles. C'est une asymétrie du client, pas une élégance ; la lisser
// ferait apparaître la première particule vingt unités trop tôt.
[[nodiscard]] std::int16_t EffectQuadBurstAlpha(std::uint8_t mark, float depth_below,
                                                std::uint8_t fade_mark, bool first_particle);

// LA TEINTE d'une particule, et COMBIEN DE FOIS elle se peint.
//
// ⚠ DEUX MARQUES PEIGNENT LEUR QUAD DEUX FOIS, au même endroit et au même
// alpha : c'est ainsi que le client double leur éclat. N'en peindre qu'un les
// rendrait deux fois plus pâles que les autres.
[[nodiscard]] std::array<std::uint8_t, 3> EffectQuadBurstColor(std::uint8_t mark,
                                                               std::uint8_t fade_mark);
[[nodiscard]] std::size_t EffectQuadBurstPasses(std::uint8_t mark);

// LES QUADS D'UNE PISTE : de zéro à trente-deux.
//
// `turn_degrees` et `tilt_degrees` sont les deux angles de caméra, que le
// mouvement range dans les champs de STYLE des pistes zéro et un — comme le
// carré concentrique, et pour la même raison.
//
// ⚠ UNE PARTICULE NE SE MONTRE QUE SOUS LE NŒUD, hauteur négative dans le
// repère du client, DONC AU-DESSUS dans le nôtre. La seule exception est la
// marque quinze, qui se peint quelle que soit sa hauteur.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeQuadBurstQuads(const EffectTrack& track,
                                                                 std::int16_t turn_degrees,
                                                                 std::int16_t tilt_degrees,
                                                                 glm::vec3 anchor);

}  // namespace ro::render
