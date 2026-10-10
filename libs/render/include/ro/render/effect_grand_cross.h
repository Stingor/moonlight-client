#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// GRAND CROSS : DEUX NŒUDS, DEUX FIGURES, DEUX MOUVEMENTS.
//
// L'effet 226 — et son jumeau 450 — compte quarante-six sites d'émission à lui
// seul. Son cas du répartiteur appelle DEUX bâtisseurs, et c'est pourquoi il
// porte deux nœuds :
//
//   le premier → sous-type 34, le RUBAN À QUATRE BRAS, quatre pistes qui
//   balaient chacune un quart de tour ;
//   le second → sous-type 37, le CADRE TOURNANT, deux pistes seulement, un
//   rectangle extrudé vers le haut.
//
// ⚠ LES DEUX MOUVEMENTS SE FONT SUR PLACE, dans le cas du répartiteur, sans
// fonction de mouvement propre. Les croire immobiles pour autant aurait donné
// deux figures figées — c'est l'erreur de crible que l'ADR 0023 a déjà payée.
//
// ⚠ ET LA SECONDE IMAGE DU CADRE VIENT DU SITE. Le bâtisseur du cadre charge
// `ring_yellow.tga` ou `ring_black.tga` selon l'octet que son appelant pousse ;
// n'en retenir qu'une serait une erreur. C'est l'ADR 0019, et ici elle change la couleur de la
// croix entière.

// CE QUE LES DEUX MOUVEMENTS PARTAGENT : une montée d'alpha, puis une descente,
// puis la mort.
//
// ⚠ LE PAS DE MONTÉE N'EST PAS LE MÊME POUR LES DEUX — dix pour le ruban, neuf
// pour le cadre —, et rien dans le code ne dit pourquoi. Les unifier ferait
// diverger les deux nœuds d'un cran d'alpha par tic pendant dix tics.
inline constexpr std::int32_t kGrandCrossRiseTicks = 10;
inline constexpr std::int16_t kGrandCrossRibbonAlphaRise = 10;
inline constexpr std::int16_t kGrandCrossFrameAlphaRise = 9;
// ⚠ LA PISTE NE MEURT PAS QUAND SON ALPHA S'ANNULE : il faut AUSSI que le tic
// ait dépassé celui-ci. Une piste dont l'alpha tombe plus tôt reste allumée et
// continue d'avancer — et c'est ce qui fait repartir le ruban.
inline constexpr std::int32_t kGrandCrossLastTick = 30;

// LE RUBAN À QUATRE BRAS, sous-type 34.
//
// Voie B — la paire de quads de traînée. Chaque piste parcourt son balayage par pas d'un neuvième
// et, à chaque pas, joint le segment précédent au courant : un ruban qui suit un
// arc de cercle et s'élève de toute la longueur que le mouvement lui donne.
inline constexpr float kGrandCrossRibbonRadiusDecay = 0.01F;
// Où le rayon REPART quand l'alpha s'annule, et sous quelle inclinaison.
inline constexpr float kGrandCrossRibbonRadiusReset = 20.0F;
inline constexpr std::int16_t kGrandCrossRibbonTiltReset = 90;
// ⚠ LE PAS EST LE NEUVIÈME DU BALAYAGE, et le client le calcule par une
// multiplication magique suivie d'un décalage : un cinquième, puis la moitié.
// Avec le balayage de quatre-vingt-dix degrés que le bâtisseur pose, cela fait
// dix degrés et neuf segments — pas vingt et un, comme la piste en porte.
inline constexpr std::int32_t kGrandCrossRibbonStepDivisor = 9;

// L'IMAGE DU RUBAN : une BANDE par segment, le long du bras.
//
// 🔴 SANS ELLE, RIEN NE SE VOIT. Un quad sans coordonnées de texture lit le
// texel du coin, partout ; celui de `ring_white.tga` est TRANSPARENT — l'image
// est un rideau opaque à sa base qui s'efface vers le haut —, et le ruban de
// Grand Cross se peignait entier, quatre bras et huit segments, sans qu'un pixel
// ne paraisse.
//
// Le segment de rang k prend la bande [k / 21, (k + 1) / 21] : vingt et une
// bandes, autant que la piste porte de cases, bien que le balayage de
// quatre-vingt-dix degrés n'en use que huit. En travers, le pied de l'arc
// (au sol) porte l'ordonnée un, le bout du bras l'ordonnée zéro.
inline constexpr float kGrandCrossRibbonTextureBands = 21.0F;
inline constexpr float kGrandCrossRibbonTextureAtArc = 1.0F;
inline constexpr float kGrandCrossRibbonTextureAtTip = 0.0F;

// LE CADRE TOURNANT, sous-type 37.
//
// Voie A — le quad projeté teinté. Quatre coins d'un rectangle tourné autour de l'axe
// vertical, un cran au-dessus du sol, et les mêmes quatre coins plus haut d'une
// hauteur que le mouvement fait osciller : quatre panneaux de côté, et DEUX
// panneaux de diagonale à la moitié de l'alpha.
inline constexpr float kGrandCrossFrameCornerGrowth = 0.005F;
// ⚠ L'ORDONNÉE DU CADRE EST MOINS UN dans le repère du client, qui descend, en
// dur dans la figure. Ce n'est pas un champ du nœud et ce n'est pas zéro : la
// poser à zéro ferait raser le sol au cadre, que le client laisse un cran
// au-dessus. Le dessinateur la ramène à notre repère.
inline constexpr float kGrandCrossFrameBaseHeight = -1.0F;
inline constexpr std::size_t kGrandCrossFrameSideQuads = 4;
inline constexpr std::size_t kGrandCrossFrameDiagonalQuads = 2;
inline constexpr std::size_t kGrandCrossFrameQuadsPerTrack =
    kGrandCrossFrameSideQuads + kGrandCrossFrameDiagonalQuads;
// LE MODE DE MÉLANGE de chaque panneau, tel que le quad projeté le lit : UN pour
// les quatre côtés — le mélange ordinaire —, ZÉRO pour les deux panneaux du
// sol — l'addition. Voir `EffectParticleBlend`.
inline constexpr std::int16_t kGrandCrossFrameSideBlendMode = 1;
inline constexpr std::int16_t kGrandCrossFrameFloorBlendMode = 0;

// LE MOUVEMENT DU RUBAN, cas 34 du répartiteur.
//
// Le rayon décroît d'un centième par tic ; l'alpha monte de dix pendant dix
// tics puis redescend ; à l'extinction, le rayon REPART de vingt et
// l'inclinaison de quatre-vingt-dix ; et les vingt et une cases de la piste
// prennent toutes la même valeur — le sinus du tic, en degrés, multiplié par la
// clé. C'est cette valeur qui est la LONGUEUR du bras.
void AdvanceGrandCrossRibbonTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// LE MOUVEMENT DU CADRE, cas 37 du répartiteur.
//
// Les deux demi-extensions croissent de cinq millièmes par tic — le cadre
// s'élargit —, l'alpha monte de neuf pendant dix tics puis redescend, et la
// hauteur d'extrusion suit le sinus du tic.
void AdvanceGrandCrossFrameTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// LES QUADS DU RUBAN, pour UNE piste.
//
// ⚠ LE RANG DE LA PISTE ENTRE DANS LA FIGURE, et pas seulement par son azimut :
// le client choisit l'angle du bras par une chaîne de comparaisons sur le rang —
// 450, 270, 90 puis 630 degrés moins l'angle courant. Quatre pistes identiques
// à l'azimut près ne donneraient pas la même croix.
//
// ⚠ ET LE NUMÉRO DE VARIANTE DU NŒUD choisit la teinte et le mélange dans la
// palette du ruban : un rose ajouté pour Grand Cross, un gris mélangé pour le
// second site du même bâtisseur.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeGrandCrossRibbonQuads(const EffectTrack& track,
                                                                        std::size_t rank,
                                                                        std::uint8_t variant,
                                                                        glm::vec3 anchor);

// LES SIX QUADS DU CADRE, pour UNE piste.
//
// Chacun sort par le quad projeté (`ComposeEffectParticleQuad`), blanc, l'image
// ENTIÈRE et retournée — un en ordonnée aux deux premiers coins, zéro aux deux
// derniers : le pied d'un côté porte le bas opaque de `ring_yellow.tga`, son
// rebord le haut effacé.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeGrandCrossFrameQuads(const EffectTrack& track,
                                                                       glm::vec3 anchor);

}  // namespace ro::render
