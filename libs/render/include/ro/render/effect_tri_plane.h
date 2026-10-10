#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LES TROIS PLANS : trois carrés tournés vers la caméra, chacun posé à son
// propre décalage et peint de son propre alpha.
//
// La figure, et les deux mouvements des sous-types 111 et 115. Quatre
// identifiants : EF_SOULLIGHT et ses deux suites, EF_CARTTER.
//
// ⚠ C'EST ENCORE LE CARRÉ À QUARANTE-CINQ DEGRÉS, cinquième figure du lot
// bâtie dessus après la paire orientée, la taillade, la rafale et le grain. Le
// client lit ses tables aux index 45, 135, 225 et 315.
//
// ⚠ ET L'ALPHA EST PAR PLAN, non par piste : il vit dans les valeurs douze,
// treize et quatorze, et le client le tronque en entier. Prendre celui de la
// piste peindrait les trois ensemble.

// COMBIEN DE PLANS une piste porte.
inline constexpr std::size_t kTriPlanePlanes = 3;

// DE COMBIEN LE CARRÉ EST TOURNÉ, et de combien ses coins s'écartent.
inline constexpr std::int32_t kTriPlaneFirstCornerDegrees = 45;
inline constexpr std::int32_t kTriPlaneDegreesPerCorner = 90;

// LES CASES QUE CETTE FAMILLE LIT, et leur sens lui appartient (ADR 0017).
//
// Les douze premières valeurs sont SIX PAIRES d'angles — deux par plan ; les
// trois suivantes les alphas, et la seizième la teinte.
inline constexpr std::size_t kTriPlaneAnglePairs = 6;
inline constexpr std::size_t kTriPlaneSlotOfFirstAlpha = 12;
inline constexpr std::size_t kTriPlaneSlotOfColor = 15;

// OÙ CHAQUE PLAN RANGE SON DÉCALAGE. ⚠ LE DEUXIÈME EST DANS L'ANCRE et non
// dans la queue, comme la deuxième particule de la rafale.
inline constexpr std::size_t kTriPlaneSlotOfFirstOffset = 3;
inline constexpr std::size_t kTriPlaneSlotOfThirdOffset = 6;
inline constexpr std::size_t kTriPlaneOffsetInTheAnchor = 1;

// LES CINQ TEINTES QUE LE DESSINATEUR NOMME.
inline constexpr float kTriPlaneBlueColor = 0.0F;
inline constexpr float kTriPlanePalePinkColor = 1.0F;
inline constexpr float kTriPlanePinkColor = 2.0F;
inline constexpr float kTriPlaneDeepPinkColor = 3.0F;
inline constexpr float kTriPlanePerPlaneColor = 4.0F;

inline constexpr std::array<std::uint8_t, 3> kTriPlaneBlue = {80, 80, 255};
inline constexpr std::array<std::uint8_t, 3> kTriPlanePalePink = {255, 200, 200};
inline constexpr std::array<std::uint8_t, 3> kTriPlanePink = {255, 160, 160};
inline constexpr std::array<std::uint8_t, 3> kTriPlaneDeepPink = {255, 120, 120};
// ⚠ LA CINQUIÈME TEINTE CHANGE AVEC LE PLAN, et elle est seule du lot dans ce
// cas : trois bleus voisins, un par plan. Leur donner la même teinte
// aplatirait la profondeur que le client cherche.
inline constexpr std::array<std::array<std::uint8_t, 3>, kTriPlanePlanes> kTriPlanePerPlane = {
    {{40, 40, 255}, {10, 10, 255}, {70, 70, 255}}};

// LE DÉCALAGE d'un plan, lu dans la piste.
[[nodiscard]] glm::vec3 EffectTriPlaneOffsetOf(const EffectTrack& track, std::size_t plane);
void SetEffectTriPlaneOffset(EffectTrack& track, std::size_t plane, glm::vec3 offset);

// CETTE TEINTE-LÀ DESSINE-T-ELLE ?
//
// ⚠ LE CLIENT COMPARE À CINQ VALEURS EXACTES et n'a pas de cas par défaut :
// toute autre — y compris entre deux — ne dessine RIEN.
[[nodiscard]] bool EffectTriPlaneDraws(float color);

// LA TEINTE d'un plan, selon la seizième valeur de sa piste.
[[nodiscard]] std::array<std::uint8_t, 3> EffectTriPlaneColor(float color, std::size_t plane);

// LES TROIS PLANS D'UNE PISTE, ou moins : chacun se tait quand son premier coin
// passe SOUS le nœud, et tous quand la teinte dépasse les cinq que le client
// nomme.
//
// ⚠ LA GARDE PORTE SUR LE PREMIER COIN, pas sur le centre : un carré dont le
// centre est au ras du nœud se montre déjà si son premier coin est au-dessus.
//
// ⚠ ET AU-DELÀ DE LA CINQUIÈME TEINTE LE CLIENT NE DESSINE RIEN : son
// aiguillage n'a pas de cas par défaut. Retomber sur une teinte inventerait
// une figure.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeTriPlaneQuads(const EffectTrack& track,
                                                                std::int16_t turn_degrees,
                                                                std::int16_t tilt_degrees,
                                                                glm::vec3 node_origin,
                                                                const EffectOwnerTint& owner = {});

// LE MOUVEMENT DU SOUS-TYPE 111 : les plans dérivent en poursuivant des angles.
//
// ⚠ SIX PAIRES D'ANGLES, et c'est LA MÊME POURSUITE que la rafale du 29 et le
// grain du 96 — voir `ChaseEffectTrackAngles`.
//
// ⚠ ET UN SEUL PLAN BOUGE VRAIMENT. Le premier monte d'un pas par tic et
// dérive dès qu'il a quitté le nœud ; le deuxième DESCEND, si bien que sa
// garde « au-dessus du nœud » n'est jamais vraie et qu'il ne dérive jamais ; le
// troisième n'a pas de hauteur écrite du tout : le client n'écrit la hauteur du
// troisième nulle part.
void AdvanceDriftingTriPlaneTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                   std::int16_t camera_turn_degrees,
                                   std::int16_t camera_tilt_degrees, std::uint32_t& generator);

// LE MOUVEMENT DU SOUS-TYPE 115 : les trois plans s'envolent en ligne droite.
//
// Chaque plan a un CAP et une PENTE fixes, et file dessus à la vitesse de la
// clé. Passé le vingt-cinquième tic la clé perd un dixième par tic et les trois
// alphas tombent de six.
void AdvanceFlyingTriPlaneTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                 std::int16_t camera_turn_degrees,
                                 std::int16_t camera_tilt_degrees);

}  // namespace ro::render
