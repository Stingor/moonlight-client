#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA COLONNE QUI TOURNE : la figure du sous-type 137.
//
// Ce n'est ni un arc ni un faisceau (ADR 0017), et ce n'est même pas une
// figure couchée. C'est la JUPE D'ANNEAU — la même que `Ring_0` — dont
// l'inclinaison vaut quatre-vingt-dix degrés : ses segments partent donc à la
// VERTICALE. Le premier identifiant qui l'emploie s'appelle EF_FIREPILLARON2.
//
// ⚠ ELLE SORT PAR DEUX VOIES, et c'est le seul dessinateur du lot dans ce cas.
// Le même balayage est peint DEUX FOIS : une passe par le quad projeté teinté,
// une seconde par la paire de quads de traînée, à rayon multiplié par un et demi
// et décalée de quarante-cinq degrés. N'en porter qu'une donnerait la moitié de
// la figure, et rien ne le dirait.

// COMBIEN DE POINTS la colonne compte, et combien de quads par passe.
//
// ⚠ CINQ, PAS VINGT ET UN. Le pas angulaire vaut le balayage divisé par QUATRE,
// et l'index des longueurs est BORNÉ à quatre : les cases au-delà de la
// cinquième ne sont jamais lues, bien que la piste en porte vingt et une.
inline constexpr std::size_t kSpiralColumnPoints = 5;
inline constexpr std::size_t kSpiralColumnQuadsPerPass = kSpiralColumnPoints - 1;

// DE COMBIEN LA SECONDE PASSE S'ÉCARTE de la première.
inline constexpr float kSpiralColumnOuterScale = 1.5F;
inline constexpr std::int32_t kSpiralColumnOuterOffsetDegrees = 45;

// LA TEINTE DE LA PASSE INTÉRIEURE, écrite en dur dans le dessinateur.
//
// ⚠ CE DESSINATEUR NE PEUT PEINDRE SA PASSE INTÉRIEURE QUE ROUGE, quelle que
// soit l'image du nœud : deux cent cinquante-cinq de rouge, DIX de vert, DIX de
// bleu. La passe extérieure, elle, prend sa teinte dans la palette de traînée.
inline constexpr std::uint8_t kSpiralColumnInnerRed = 255;
inline constexpr std::uint8_t kSpiralColumnInnerGreen = 10;
inline constexpr std::uint8_t kSpiralColumnInnerBlue = 10;

// ⚠ ET L'ALPHA DE LA PASSE INTÉRIEURE EST LE CINQUIÈME de celui de la piste. Le
// client divise avant de le passer à la primitive ; lui donner l'alpha entier
// peindrait ce ruban cinq fois trop fort.
inline constexpr std::int16_t kSpiralColumnInnerAlphaDivisor = 5;

// LES DEUX PASSES D'UNE PISTE, l'intérieure d'abord.
//
// `variant` est le numéro que le nœud porte pour sa teinte — voir la palette de
// traînée. Il ne sert qu'à la passe EXTÉRIEURE.
//
// ⚠ LE POINT S'ANCRE DEUX FOIS. Le cercle s'ajoute à l'ancre de la piste PUIS
// aux trois dernières cases de sa queue — et ces trois cases-là portent le
// vecteur vers la CIBLE quand le bâtisseur l'a posé. C'est ce qui fait pencher
// la colonne vers ce qu'elle vise ; n'en porter qu'un la poserait droite.
//
// Rend une liste VIDE quand la piste est éteinte, quand son balayage est
// négatif, quand son alpha est nul, ou quand AUCUN de ses cinq segments n'a
// encore de hauteur — ce qui est le cas pendant les cent premiers tics.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeSpiralColumnQuads(const EffectTrack& track,
                                                                    std::uint8_t variant,
                                                                    glm::vec3 anchor);

}  // namespace ro::render
