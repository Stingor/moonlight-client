#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_four_particle_slots.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// L'AURA DU PERSONNAGE : quatre étincelles qui tournent, montent, s'éteignent
// et RENAISSENT plus haut, autour de celui qui les porte.
//
// Porte les dessinateurs des sous-types 99 et 35 et le mouvement qu'ils
// partagent. SIX identifiants — 203, 254, 418, 455, 485 et 486.
//
// ⚠ LES DEUX SOUS-TYPES SONT UNE SEULE FIGURE. Le 35 est la boucle interne du
// 99 sortie de son enveloppe : même carré, mêmes quatre particules, même règle
// de retournement. Il n'en diffère que par le nombre de COUCHES — une au lieu
// de trois — et par l'endroit d'où sa teinte vient.
//
// ⚠ RIEN NE TRAÎNE (ADR 0017) : aucune position passée n'est gardée. Et le
// quatre de la figure n'est pas un compte de quads mais le RAYON du cercle que
// les particules parcourent.

// COMBIEN DE PARTICULES une piste porte, et où elles vivent.
//
// ⚠ C'EST LE RANGEMENT DE LA RONDE ET DES LUCIOLES, à l'identique — voir
// `effect_four_particle_slots.h`. C'est la troisième famille à s'en servir, et
// c'est tout ce que les trois ont en commun.
inline constexpr std::size_t kJobAuraParticles = kEffectFourParticles;

// SUR QUEL CERCLE une particule tourne. La figure recalcule l'abscisse et la
// profondeur à chaque image ; seule la hauteur vient du mouvement.
inline constexpr float kJobAuraOrbitRadius = 4.0F;

// LES QUATRE VALEURS D'UNE PARTICULE, à partir de quatre fois son rang.
inline constexpr std::size_t kJobAuraValuesPerParticle = 4;
inline constexpr std::size_t kJobAuraValueOfPhase = 0;
inline constexpr std::size_t kJobAuraValueOfAlpha = 1;
inline constexpr std::size_t kJobAuraValueOfFrame = 2;
inline constexpr std::size_t kJobAuraValueOfAngle = 3;

// OÙ LE MOUVEMENT RANGE LA HAUTEUR DE L'AURA — la case qui suit les seize des
// quatre particules, et que la figure RETRANCHE de chaque coin.
inline constexpr std::size_t kJobAuraValueOfHeight = 20;

// LE CARRÉ, encore le même : la HUITIÈME figure du lot bâtie sur les index 45,
// 135, 225 et 315.
inline constexpr std::int32_t kJobAuraFirstCornerDegrees = 45;
inline constexpr std::int32_t kJobAuraDegreesPerCorner = 90;

// COMBIEN D'IMAGES la famille défile — `effect\super1.bmp` à `super5.bmp`.
inline constexpr std::int32_t kJobAuraFrameCount = 5;

// LA RÈGLE DE RETOURNEMENT, partagée par les deux dessinateurs.
//
// L'écart entre l'angle de la particule et le tour de la caméra décide si
// l'image se peint telle quelle ou en MIROIR — coins B, A, D, C au lieu de
// A, B, C, D. Et les images ZÉRO et DEUX inversent la règle.
//
// ⚠ CE N'EST PAS UNE GARDE DE VISIBILITÉ. Le quad se peint dans les deux cas ;
// seul l'ordre de ses coins change, donc le sens de son image. Le lire comme
// un test de visibilité en ferait disparaître la moitié.
inline constexpr std::int32_t kJobAuraFlipAbove = 70;
inline constexpr std::int32_t kJobAuraFlipBelow = -80;
inline constexpr std::int32_t kJobAuraFlipHalfRange = 270;
inline constexpr std::int32_t kJobAuraFrameThatInvertsFlipFirst = 0;
inline constexpr std::int32_t kJobAuraFrameThatInvertsFlipSecond = 2;

// VRAI QUAND L'IMAGE SE PEINT EN MIROIR, pour un angle et une image donnés.
[[nodiscard]] bool EffectJobAuraFlips(float particle_degrees, std::int16_t camera_turn_degrees,
                                      std::int32_t frame);

// ---------------------------------------------------------------------------
// LE SOUS-TYPE 99 — trois couches
// ---------------------------------------------------------------------------

// LES TROIS ÉCHELLES, de la plus serrée à la plus large.
inline constexpr std::size_t kJobAuraLayers = 3;
inline constexpr std::array<float, kJobAuraLayers> kJobAuraLayerScales = {1.0F, 1.2F, 2.0F};

// UNE PALETTE : la teinte de chacune des trois couches.
using EffectJobAuraPalette = std::array<std::array<std::uint8_t, 3>, kJobAuraLayers>;

// ⚠ LA PLUS LARGE EST LA PLUS SOMBRE, et c'est ce qui fait le halo : trois
// carrés concentriques peints en ADDITION, dont le plus grand n'ajoute presque
// rien. Les peindre à teinte égale ferait un bloc au lieu d'une lueur.
inline constexpr EffectJobAuraPalette kJobAuraBluePalette = {
    {{100, 100, 255}, {0, 0, 120}, {0, 0, 120}}};
inline constexpr EffectJobAuraPalette kJobAuraPalePalette = {
    {{90, 90, 80}, {60, 60, 50}, {30, 30, 20}}};
inline constexpr EffectJobAuraPalette kJobAuraRedPalette = {
    {{255, 100, 100}, {120, 0, 0}, {120, 0, 0}}};

// LA MARQUE QUI CHOISIT LA PALETTE — la case UN, que le site d'appel du
// bâtisseur pousse.
inline constexpr std::size_t kJobAuraMarkOfPalette = 1;
inline constexpr std::uint8_t kJobAuraBlueMark = 0;
inline constexpr std::uint8_t kJobAuraPaleMark = 1;

[[nodiscard]] const EffectJobAuraPalette& EffectJobAuraPaletteOf(std::uint8_t mark);

// LES DOUZE CARRÉS d'une piste — quatre particules, trois couches chacune.
//
// `turn_degrees` et `tilt_degrees` sont les deux angles de caméra, que le
// mouvement range dans les styles des pistes zéro et un.
//
// ⚠ L'ALPHA EST PLAFONNÉ PAR LES TICS de la piste. L'aura monte donc en fondu
// à sa naissance au lieu de paraître d'un coup — c'est ce qui évite qu'elle
// éclate au visage du joueur quand il entre en jeu.
//
// ⚠ ET IL N'Y A AUCUNE GARDE D'ALPHA : une particule à alpha nul rend ses
// quads comme les autres. En addition ils ne peignent rien, et leur en
// épargner le calcul changerait le compte des quads.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeLayeredAuraQuads(
    const EffectTrack& track, std::int16_t turn_degrees, std::int16_t tilt_degrees,
    glm::vec3 anchor, const EffectOwnerTint& owner = {});

// ---------------------------------------------------------------------------
// LE SOUS-TYPE 35 — une seule couche
// ---------------------------------------------------------------------------

// CE QUE LE CAS DE RÉPARTITION POUSSE AU DESSINATEUR, en dur : l'échelle du
// carré et sa teinte. C'est la couche zéro du 99 à la marque bleue.
inline constexpr float kPlainAuraScale = 1.0F;
inline constexpr std::array<std::uint8_t, 3> kPlainAuraColor = {100, 100, 255};

// ⚠ ET LA MARQUE UN LE REND ROUGE, DÉFINITIVEMENT. Le dessinateur écrase alors
// ses trois octets de teinte — et il écrase ses PARAMÈTRES, si bien que toutes
// les pistes SUIVANTES de la même passe sont rouges elles aussi, quelle que
// soit leur marque. C'est pourquoi le nœud entier se compose d'un coup.
inline constexpr std::uint8_t kPlainAuraRedMark = 1;
inline constexpr std::array<std::uint8_t, 3> kPlainAuraRed = {255, 20, 20};

// LES QUATRE CARRÉS d'une piste, à la teinte qu'on lui donne.
[[nodiscard]] std::vector<EffectWorldQuad> ComposePlainAuraQuads(
    const EffectTrack& track, std::int16_t turn_degrees, std::int16_t tilt_degrees,
    glm::vec3 anchor, std::array<std::uint8_t, 3> color, const EffectOwnerTint& owner = {});

// LES QUADS DE TOUT LE NŒUD, la teinte qui déborde comprise.
[[nodiscard]] std::vector<EffectWorldQuad> ComposePlainAuraNodeQuads(
    const std::array<EffectTrack, kEffectTracksPerNode>& tracks, std::int16_t turn_degrees,
    std::int16_t tilt_degrees, glm::vec3 anchor, const EffectOwnerTint& owner = {});

// ---------------------------------------------------------------------------
// LE MOUVEMENT — un CYCLE DE VIE, et c'est le premier du lot
// ---------------------------------------------------------------------------

// JUSQU'À QUELLE PHASE une particule monte, et de combien. Passé ce seuil elle
// redescend, et plus vite qu'elle n'est montée.
//
// ⚠ LA PHASE N'EST PAS L'ÂGE. Elle ne sépare que la montée de la descente et ne
// retombe qu'à la renaissance : une particule monte neuf tics puis descend une
// vingtaine, et non l'inverse.
inline constexpr float kJobAuraRisePhases = 10.0F;
inline constexpr float kJobAuraAlphaRise = 25.0F;
inline constexpr float kJobAuraAlphaFall = 12.0F;

// SOUS QUEL ALPHA une particule renaît. C'est le même nombre que le pas de
// descente, et ce n'est pas une coïncidence : le client compare à ce qu'il
// vient de retrancher.
inline constexpr float kJobAuraRebirthBelow = 12.0F;

// OÙ UNE PARTICULE RENAÎT : de trois à dix au-DESSUS du nœud.
//
// ⚠ LE REPÈRE DU CLIENT DESCEND, le nôtre monte. Le client range moins trois
// moins un tirage ; nous rangeons le même nombre, et c'est la FIGURE qui le
// rend à notre sens, au dernier moment.
inline constexpr std::int32_t kJobAuraRebirthHeightSpread = 8;
inline constexpr std::int32_t kJobAuraRebirthHeightFloor = -3;

// COMBIEN DE TIRAGES une renaissance consomme : l'angle, l'image, la hauteur.
inline constexpr std::size_t kJobAuraDrawsPerRebirth = 3;

// LE MOUVEMENT DES SOUS-TYPES 99 ET 35.
//
// `aura_height` est la hauteur à laquelle le sprite du porteur veut son aura,
// que le client va lire dans l'acteur. `render` ne connaît pas les acteurs :
// l'appelant la donne, et zéro pose l'aura aux pieds — arbitraire, jamais
// inventé.
//
// ⚠ LE COMPTEUR N'AVANCE QUE SUR UNE PISTE VIVE, au contraire des lucioles :
// le client teste avant d'incrémenter. Et c'est ce compteur qui plafonne
// l'alpha du sous-type 99 — le déplacer hors du test ferait naître l'aura à
// plein.
void AdvanceJobAuraTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                          std::int16_t camera_turn_degrees, std::int16_t camera_tilt_degrees,
                          float aura_height, std::uint32_t& generator);

}  // namespace ro::render
