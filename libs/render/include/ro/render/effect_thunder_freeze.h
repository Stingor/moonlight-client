#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE CŒUR QUI ERRE ET SES QUATRE ÉCLATS DE GEL : le sous-type 145, l'effet 833.
//
// Le mouvement et la figure sont propres à ce sous-type : aucun autre ne les
// emploie.
//
// ⚠ RIEN NE TOURBILLONNE, AUCUN ANNEAU N'EST PROJETÉ (ADR 0017). Chaque nœud
// porte UN CŒUR — deux carrés tournés vers la caméra, l'un dans l'autre, bleu
// violacé puis bleu pâle — qui fait le tour du porteur sur un cercle de sept
// unités, quinze au-dessus de ses pieds, en tremblant d'une demi-unité sur
// chaque axe ; et autour de ce cœur, QUATRE ÉCLATS de gel debout, chacun sous
// son propre cap, qui grandissent, pâlissent et renaissent.
//
// ⚠ UNE SEULE PISTE MÈNE, LA PREMIÈRE. Le mouvement ne juge que son allumage,
// n'incrémente que SON compteur à part et ne pose que SON ancre ; les quatre
// éclats se lisent chacun dans sa piste, mais s'ancrent tous à celle de la
// première.

// LES CASES QUE LE BÂTISSEUR, LE MOUVEMENT ET LA FIGURE PARTAGENT, aux rangs du
// client. Voir `effect_track.h` : ce sont leurs lectures qui leur donnent un sens.
//
// Trois couples (angle courant, angle visé), un par axe du tremblement.
inline constexpr std::size_t kThunderFreezeSlotOfWander = 0;
inline constexpr std::size_t kThunderFreezeWanderAxes = 3;
// Un axe tient deux cases, l'angle courant puis sa cible.
inline constexpr std::size_t kThunderFreezeWanderSlotsPerAxis = 2;
// L'angle du cercle que le cœur parcourt autour du porteur.
inline constexpr std::size_t kThunderFreezeSlotOfOrbitDegrees = 6;
// La hauteur du cœur, dans le repère DESCENDANT du client.
inline constexpr std::size_t kThunderFreezeSlotOfHeight = 7;
// L'angle qui fait respirer le rayon du cœur.
inline constexpr std::size_t kThunderFreezeSlotOfPulseDegrees = 8;
// ⚠ POSÉE À ZÉRO ET LUE PAR PERSONNE : ni le mouvement ni la figure ne touchent
// cette case — leurs lectures de la première piste la sautent.
inline constexpr std::size_t kThunderFreezeSlotOfUnread = 10;
// La taille de l'éclat, recalculée à chaque tic par le mouvement pour chaque
// piste, et lue par la figure.
inline constexpr std::size_t kThunderFreezeSlotOfShardSize = 12;
// LA MARQUE QUI DIVISE LE TREMBLEMENT PAR DEUX.
inline constexpr std::size_t kThunderFreezeMarkOfHalvedWander = 0;

// L'ALPHA DU CŒUR monte d'un par tic tant que le compteur de la première piste,
// DÉJÀ incrémenté, ne dépasse pas deux cents : il plafonne donc à deux cents,
// et ne redescend jamais.
inline constexpr std::int32_t kThunderFreezeAlphaRiseTicks = 200;

// LE TREMBLEMENT : chaque angle marche d'un degré par tic vers sa cible, borné
// à [0, 359]. Au-delà de sa cible, il en tire une nouvelle EN DESSOUS de lui —
// `rand() % angle` ; en deçà, une AU-DESSUS — `359 − rand() % angle`. Un angle
// nul tire zéro sans appeler `rand`.
inline constexpr float kThunderFreezeWanderStepDegrees = 1.0F;
inline constexpr float kThunderFreezeWanderCeilingDegrees = 359.0F;
inline constexpr std::int32_t kThunderFreezeWanderRetargetTop = 359;
// La moitié qu'impose la marque.
inline constexpr float kThunderFreezeHalvedWanderFactor = 0.5F;

// LE CERCLE : sept unités, un degré de plus quand le compteur est PAIR, replié
// à trois cent soixante.
inline constexpr float kThunderFreezeOrbitRadius = 7.0F;
inline constexpr std::int32_t kThunderFreezeOrbitParityMask = 1;
inline constexpr float kThunderFreezeOrbitStepDegrees = 1.0F;

// LA RESPIRATION DU CŒUR avance de `rand() % 5 + 5` degrés par tic, repliée à trois cent soixante.
inline constexpr std::int32_t kThunderFreezePulseDraws = 5;
inline constexpr std::int32_t kThunderFreezePulseBase = 5;

// L'ÉCLAT : sa taille de base croît de deux centièmes par tic. Passé six, son
// alpha perd trois par tic ; à zéro ou moins, l'éclat RENAÎT — taille trois,
// cap tiré sur le tour, alpha deux cents.
inline constexpr float kThunderFreezeShardGrowth = 1.02F;
inline constexpr float kThunderFreezeShardFadingSize = 6.0F;
inline constexpr std::int16_t kThunderFreezeShardAlphaFall = 3;
inline constexpr float kThunderFreezeShardRebirthSize = 3.0F;
inline constexpr std::int32_t kThunderFreezeShardHeadingDraws = 360;
inline constexpr std::int16_t kThunderFreezeShardRebirthAlpha = 200;
// Et la taille peinte respire autour de neuf dixièmes de la base, d'un dixième,
// au rythme de l'angle propre de l'éclat : `base·sin·0,1 + base·0,9`, jamais
// négative.
inline constexpr float kThunderFreezeShardSizeSteady = 0.9F;
inline constexpr float kThunderFreezeShardSizeSwing = 0.1F;
// Cet angle propre tourne d'un degré par tic, replié à trois cent soixante — ⚠
// SEULEMENT une fois le compteur de la piste positif.
inline constexpr std::int16_t kThunderFreezeShardSpinPerTick = 1;
// L'ÉCLAT EST UN CARRÉ DEBOUT, de demi-côté la moitié de sa taille, posé en
// travers de son cap : ses deux bords verticaux sont au cap et au cap opposé,
// le haut et le bas à une demi-taille de l'ancre du cœur.
inline constexpr float kThunderFreezeShardHalf = 0.5F;

// LES DEUX IMAGES DU NŒUD, au dix-septième paramètre du traceur : le cœur prend
// la première, les éclats la seconde.
inline constexpr std::size_t kThunderFreezeCoreTexture = 0;
inline constexpr std::size_t kThunderFreezeShardTexture = 1;

// LE CŒUR : un carré de rayon `r + r·sin(respiration)·0,05`, puis un second de
// rayon `r·0,5 + r·sin(respiration)·0,05` — ⚠ PAS LA MOITIÉ DU PREMIER : seul
// le terme fixe est divisé.
inline constexpr float kThunderFreezeCoreBreath = 0.050000001F;
inline constexpr float kThunderFreezeInnerCoreShare = 0.5F;
// Leurs teintes, poussées en dur.
inline constexpr std::array<std::uint8_t, 3> kThunderFreezeOuterCoreColor = {0x44, 0x2A, 0xFF};
inline constexpr std::array<std::uint8_t, 3> kThunderFreezeInnerCoreColor = {0x9B, 0x9B, 0xFF};
// LA TEINTE D'UN ÉCLAT dépend de son RANG et de lui seul : un gris posé sur le
// rouge et le vert, le bleu plein.
inline constexpr std::array<std::uint8_t, kEffectTracksPerNode> kThunderFreezeShardGreys = {105, 35,
                                                                                            75, 5};
inline constexpr std::uint8_t kThunderFreezeShardBlue = 0xFF;

// Avance les quatre pistes d'un tic, et range les deux angles de caméra.
//
// ⚠ LES DEUX ANGLES SONT RANGÉS AVANT LA GARDE D'ALLUMAGE, tels que la troupe
// les donne : ce sont DÉJÀ les compléments `360 − lacet` et `450 − tangage` que
// le client calcule.
//
// ⚠ L'ANCRE DE LA PREMIÈRE PISTE EST UN ÉCART AU NŒUD, dans NOTRE repère : le
// client la compose dans le sien et la figure l'ajoute à la place du nœud
// (dix-huitième paramètre du traceur à zéro). Elle se convertit ici, une fois,
// par `(x, −y, −z)`.
void AdvanceThunderFreezeTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                std::int16_t billboard_turn_degrees,
                                std::int16_t billboard_tilt_degrees, std::uint32_t& generator);

// LES QUADS D'UN NŒUD : les deux carrés du cœur, puis un éclat par piste dont
// la taille est positive. Rien quand la première piste est éteinte ou que son
// alpha est nul — les éclats compris, qui vivent sous la même garde.
//
// `node_origin` est la place du nœud, que le traceur ajoute à tous les coins
// (dix-huitième paramètre à zéro).
[[nodiscard]] std::vector<EffectWorldQuad> ComposeThunderFreezeQuads(
    const std::array<EffectTrack, kEffectTracksPerNode>& tracks, glm::vec3 node_origin);

}  // namespace ro::render
