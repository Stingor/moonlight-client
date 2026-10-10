#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LES CARRÉS QUI MONTENT, sous-type 49 : la marque au sol de Loki's Veil.
//
// Deux carrés teintés — la figure de `ComposeTintedRotatedQuad`, marque deux —
// qui s'allument en dix tics, puis s'éteignent en MONTANT, et repartent de la
// hauteur de leur porteur dès qu'ils sont éteints. L'unité vit des minutes :
// c'est ce cycle, recommencé sans fin et décalé d'une piste à l'autre, que l'on
// voit.
//
// ⚠ LE PREMIER CYCLE PART DE LA HAUTEUR ZÉRO DU MONDE. Le bâtisseur pose
// l'abscisse et la profondeur de l'ancre, jamais sa hauteur : seule la reprise
// la pose. Partir du porteur dès le semis serait plus joli et faux.
struct EffectRisingQuadPattern {
  std::uint32_t effect_id = 0;
  std::string_view texture;
  // LE MODE que le site passe au bâtisseur. Zéro : DEUX pistes, un rayon de
  // cinq à dix, et le carré garde son cap. Un : UNE piste, un rayon de trois à
  // moins de cinq, le cap tourne, et la reprise part cinq unités plus haut.
  std::uint8_t mode = 0;
};

[[nodiscard]] std::span<const EffectRisingQuadPattern> EffectRisingQuadPatternTable();

// Les patrons d'un identifiant d'effet, vides quand il n'en a pas.
[[nodiscard]] std::span<const EffectRisingQuadPattern> EffectRisingQuadPatternsFor(
    std::uint32_t effect_id);

// Le mode qui ne sème qu'une piste et fait tourner le cap.
inline constexpr std::uint8_t kRisingQuadTurningMode = 1;
// Chaque piste naît sous ce cap, en degrés.
inline constexpr std::int16_t kRisingQuadStartDegrees = 45;
// LA DERNIÈRE AVANCE QUI ALLUME, et de combien. Les tics de départ sont tirés
// sous onze, si bien que les pistes ne s'allument pas ensemble.
inline constexpr std::int32_t kRisingQuadLastTickOfRise = 10;
inline constexpr std::int16_t kRisingQuadAlphaRise = 25;
inline constexpr std::int16_t kRisingQuadAlphaFall = 5;
// De combien la piste monte à chaque avance où elle s'éteint, en unités du
// monde.
inline constexpr float kRisingQuadClimbPerTick = 0.2F;
// De combien la reprise du mode tournant part au-dessus du porteur.
inline constexpr float kRisingQuadTurningLift = 5.0F;
// Le pas de l'angle propre — qui fait respirer le rayon — et celui du cap du
// mode tournant, en degrés par avance.
inline constexpr float kRisingQuadSpinPerTick = 5.0F;
inline constexpr std::int16_t kRisingQuadTurnPerTick = 5;

// LES PISTES SEMÉES. `anchor` est la place du porteur dans notre repère ; la
// piste n'en prend que l'abscisse et la profondeur.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRisingQuadTracks(
    const EffectRisingQuadPattern& pattern, glm::vec3 anchor, std::uint32_t& generator);

// UNE AVANCE. `owner_height` est la hauteur du porteur dans notre repère, d'où
// la piste éteinte repart ; les deux angles de la caméra se rangent dans les
// pistes zéro et un, où la figure les lit.
void AdvanceRisingQuadTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                             float owner_height, std::int16_t camera_turn_degrees,
                             std::int16_t camera_tilt_degrees);

[[nodiscard]] PooledEffectNode SowEffectRisingQuadNode(const EffectRisingQuadPattern& pattern,
                                                       glm::vec3 anchor,
                                                       std::int32_t effect_life_ticks,
                                                       std::uint32_t& generator);

}  // namespace ro::render
