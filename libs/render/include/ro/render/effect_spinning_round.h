#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_four_particle_slots.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA RONDE : quatre carrés qui tournent autour du nœud, à hauteur fixe, en
// tremblant chacun dans son quadrant.
//
// La figure et le mouvement du sous-type 27. TROIS identifiants l'emploient.
//
// ⚠ C'EST LA SIXIÈME FIGURE DU LOT BÂTIE SUR LE CARRÉ À QUARANTE-CINQ DEGRÉS,
// après la paire orientée, la taillade, la rafale, le grain et les trois plans.
// Le client lit ses tables aux index 45, 135, 225 et 315.
//
// ⚠ ET LA HAUTEUR DE SES QUATRE PARTICULES N'EST JAMAIS ÉCRITE. Le bâtisseur la
// pose une fois — deux fois l'octet du site, au-dessus du nœud — et aucun
// mouvement n'y touche. La garde « au-dessus du nœud » du dessinateur est donc
// toujours vraie, et son alpha ne varie pas.

// COMBIEN DE PARTICULES une ronde porte, et où elles se rangent.
//
// ⚠ LA DISPOSITION EST PARTAGÉE avec le sous-type 130, et ce n'est pas une
// coïncidence : c'est la même disposition du client, dans les deux
// dessinateurs et leurs deux bâtisseurs. Voir
// `effect_four_particle_slots.h`.
inline constexpr std::size_t kSpinningRoundParticles = kEffectFourParticles;

// À QUELLE DISTANCE DU NŒUD elles tournent, et de combien de degrés par tic.
//
// ⚠ L'ANGLE DESCEND : le client retranche un degré et rattrape sous zéro en
// ajoutant un tour. La ronde tourne donc dans le sens des aiguilles.
inline constexpr float kSpinningRoundOrbitRadius = 20.0F;
inline constexpr float kSpinningRoundSpinStep = 1.0F;

// DE COMBIEN CHAQUE PARTICULE TREMBLE, et sur combien de paires d'angles le
// tremblement se calcule.
//
// ⚠ LES QUATRE PARTICULES PARTAGENT LEURS DEUX ANGLES et n'en diffèrent que par
// le SIGNE qu'elles leur donnent — un quadrant chacune. Leur en donner quatre
// paires les ferait trembler indépendamment, ce qui se voit.
inline constexpr float kSpinningRoundJitter = 2.5F;
inline constexpr std::size_t kSpinningRoundAnglePairs = 2;

// LES DEUX PAS DE LA POURSUITE D'ANGLES, pour CETTE famille.
//
// ⚠ UN ET UN, ET NON DEUX ET TROIS. C'est le même code du client que la rafale,
// le grain et les trois plans, avec d'autres constantes : lui prêter celles de
// ses sœurs ferait trembler la ronde deux à trois fois trop vite.
inline constexpr float kSpinningRoundAngleFall = 1.0F;
inline constexpr float kSpinningRoundAngleRise = 1.0F;

// LES CASES QUE CETTE FIGURE LIT, et leur sens lui appartient (ADR 0017).
//
// Les quatre courants de tremblement vivent dans les cases zéro et deux — ce
// sont les COURANTS de deux paires, jamais leurs cibles —, et les quatre angles
// de ronde dans les cases quatre à sept.
inline constexpr std::size_t kSpinningRoundSlotOfFirstSpin = 4;

// LA MARQUE QUI CHOISIT LA TEINTE, et la valeur qui la rend verte.
inline constexpr std::size_t kSpinningRoundMarkOfColor = 1;
inline constexpr std::uint8_t kSpinningRoundGreenMark = 2;

// LES DEUX TEINTES, en dur dans le dessinateur.
inline constexpr std::array<std::uint8_t, 3> kSpinningRoundWhite = {255, 255, 255};
inline constexpr std::array<std::uint8_t, 3> kSpinningRoundGreen = {155, 255, 155};

// LES DEUX ALPHAS DE PLEIN PIED, et la rampe qui les remplace en hauteur.
inline constexpr std::int16_t kSpinningRoundGreenAlpha = 150;
inline constexpr std::int16_t kSpinningRoundWhiteAlpha = 130;
inline constexpr float kSpinningRoundFadeBelow = -1.0F;
inline constexpr float kSpinningRoundFadePerUnit = 4.0F;

// OÙ UNE PARTICULE SE TROUVE, dans le repère du client — son ordonnée DESCEND.
[[nodiscard]] glm::vec3 EffectSpinningRoundOffsetOf(const EffectTrack& track, std::size_t particle);
void SetEffectSpinningRoundOffset(EffectTrack& track, std::size_t particle, glm::vec3 offset);

// LA TEINTE et L'ALPHA d'une ronde, selon sa marque et la hauteur du coin.
[[nodiscard]] std::array<std::uint8_t, 3> EffectSpinningRoundColor(std::uint8_t mark);
[[nodiscard]] std::int16_t EffectSpinningRoundAlpha(std::uint8_t mark, float height);

// LES QUATRE CARRÉS d'une piste.
//
// `turn_degrees` et `tilt_degrees` sont les deux angles de caméra, que le
// mouvement range dans les styles des pistes zéro et un — comme la rafale.
//
// Rend un lot VIDE quand la piste est éteinte. Une particule sous le nœud n'est
// pas dessinée.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeSpinningRoundQuads(
    const EffectTrack& track, std::int16_t turn_degrees, std::int16_t tilt_degrees,
    glm::vec3 anchor, const EffectOwnerTint& owner = {});

// LE MOUVEMENT DU SOUS-TYPE 27 : la ronde tourne, et tremble.
//
// ⚠ IL SUIT SON PORTEUR, et il TIRE à chaque tic — ses deux paires d'angles
// renouvellent leur cible dès qu'elles l'atteignent.
void AdvanceSpinningRoundTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                std::int16_t camera_turn_degrees, std::int16_t camera_tilt_degrees,
                                std::uint32_t& generator);

}  // namespace ro::render
