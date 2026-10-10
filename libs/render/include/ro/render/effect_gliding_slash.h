#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA TAILLADE QUI GLISSE : la figure des sous-types 86 ET 92.
//
// ⚠ RIEN N'Y EST COURBE (ADR 0017) : c'est un CARRÉ PLAT, posé à
// l'horizontale, et l'animation n'est pas dans la figure mais dans un INDEX
// D'IMAGE que seule une marque sur trois emploie.
//
// Les quatre identifiants qui l'emploient sont les taillades de Soul Breaker.

// DE COMBIEN LE CARRÉ EST TOURNÉ par rapport à l'azimut, et de combien ses
// coins s'écartent.
//
// ⚠ QUARANTE-CINQ DEGRÉS, comme la paire orientée et non comme le carré
// concentrique : le carré se présente par la POINTE dans le sens de la
// glissade. Le poser droit le montrerait par le côté.
inline constexpr std::int32_t kGlidingSlashFirstCornerDegrees = 45;
inline constexpr std::int32_t kGlidingSlashDegreesPerCorner = 90;

// LES TROIS TEINTES, choisies par la SECONDE marque de la piste.
//
// ⚠ C'EST LA MARQUE UN, PAS LA ZÉRO. Les deux bâtisseurs mettent la zéro à
// zéro et rangent l'octet du site dans la SUIVANTE. Lire la première rendrait
// les quatre taillades de la même couleur.
inline constexpr std::array<std::uint8_t, 3> kGlidingSlashPale = {205, 205, 255};
inline constexpr std::array<std::uint8_t, 3> kGlidingSlashGreen = {0, 255, 0};
inline constexpr std::array<std::uint8_t, 3> kGlidingSlashPlain = {200, 200, 255};

// LA MARQUE QUI ANIME SON IMAGE, et le rythme du défilement.
//
// ⚠ UNE SEULE DES TROIS DÉFILE, et sur QUATRE images à raison d'une tous les
// SIX tics. Les deux autres n'en ont qu'une, et leur bâtisseur n'en charge
// qu'une : leur donner un index qui tourne les laisserait sans image cinq
// fois sur six.
inline constexpr std::uint8_t kGlidingSlashAnimatedMark = 1;
inline constexpr std::uint8_t kGlidingSlashGreenMark = 2;
inline constexpr std::int32_t kGlidingSlashTicksPerFrame = 6;
inline constexpr std::size_t kGlidingSlashFrameCount = 4;

// LA TEINTE et L'INDEX D'IMAGE d'une piste.
[[nodiscard]] std::array<std::uint8_t, 3> EffectGlidingSlashColor(std::uint8_t mark);
[[nodiscard]] std::size_t EffectGlidingSlashFrame(std::uint8_t mark, std::int32_t ticks);

// LE CARRÉ PLAT D'UNE PISTE, ou rien.
//
// ⚠ SES COINS SONT ABSOLUS, et son ANCRE porte les trois coordonnées — y
// compris la hauteur, que le bâtisseur tire au sort. Le mouvement fait glisser
// cette ancre ; y ajouter l'origine du nœud doublerait la position.
//
// Rend `nullopt` quand la piste est éteinte ou son alpha nul.
[[nodiscard]] std::optional<EffectWorldQuad> ComposeGlidingSlashQuad(const EffectTrack& track);

}  // namespace ro::render
