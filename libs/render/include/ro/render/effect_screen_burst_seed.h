#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "ro/render/effect_screen_node.h"

namespace ro::render {

// LE SEMIS DE L'ÉCLAT ET DE LA COURONNE, sous-types 3 et 4.
//
// HUIT identifiants, et trois d'entre eux posent les DEUX formes. Le 121 n'a
// pas de bâtisseur : son cas pose la couronne lui-même. Voir
// `scripts/releve_pieces_de_l_eclat_d_ecran.py`.
//
// ⚠ LE TIC DE SEMIS EST L'ÂGE DE L'EFFET, pas celui du nœud, et deux
// bâtisseurs n'en sèment pas au tic zéro : le 40 attend le trentième. Tout
// semer au premier tic ferait paraître Signum Crucis une demi-seconde trop tôt.

// UN TIRAGE DU CLIENT : un entier de zéro à `spread` exclu, décalé puis divisé.
//
// ⚠ ET UN `spread` NUL NE TIRE RIEN. C'est ce qui distingue « toujours cette
// valeur » de « un tirage qui tombe toujours pareil » : le second consommerait
// un nombre du générateur et décalerait tout ce qui suit.
struct EffectScreenBurstDraw {
  std::int32_t spread = 0;
  float floor = 0.0F;
  float divisor = 1.0F;
};

// L'ANGLE DE DÉPART ET LA ROTATION D'UN ÉCLAT, que les sept bâtisseurs posent
// tous de la même façon.
//
// ⚠ L'ACCÉLÉRATION N'EST PAS TIRÉE : elle se calcule — moins la vitesse divisée
// par la vie, le tout divisé par un et demi —, si bien que la rotation s'arrête
// aux deux tiers de la vie. La tirer inventerait un nombre que le client n'a
// pas.
inline constexpr std::int32_t kScreenWedgeSpinSpread = 60;
inline constexpr float kScreenWedgeSpinFloor = 10.0F;
inline constexpr float kScreenWedgeSpinDivisor = 10.0F;
inline constexpr float kScreenWedgeSpinDecay = 1.5F;

// COMBIEN DE TIRAGES un éclat consomme : l'angle, la vitesse, et trois des
// quatre plages du patron — celles dont l'étendue n'est pas nulle.
inline constexpr std::size_t kScreenWedgeDrawsPerNode = 5;

// UN LOT DE NŒUDS D'ÉCRAN qu'un effet pose d'un coup.
struct EffectScreenBurstPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  EffectScreenShape shape = EffectScreenShape::kWedge;
  // À QUEL ÂGE DE L'EFFET le bâtisseur sème, et combien de nœuds d'un coup.
  std::int32_t sow_tick = 0;
  std::uint16_t count = 1;

  std::int32_t life_ticks = 100;
  // MOINS UN veut dire « jamais » pour les deux.
  std::int32_t fade_out_tick = -1;
  std::int32_t second_step_tick = -1;
  // ⚠ NÉGATIF, ET IL ÉLÈVE : l'ordonnée d'écran descend.
  float screen_offset_y = 0.0F;

  float alpha = 254.0F;
  float alpha_max = 254.0F;
  float alpha_step = 0.0F;
  std::uint32_t flags = 0;

  // LES QUATRE PLAGES DE L'ÉCLAT, dans l'ordre où le client les tire.
  EffectScreenBurstDraw half_angle;
  EffectScreenBurstDraw radius;
  EffectScreenBurstDraw radius_step;
  EffectScreenBurstDraw turn_speed_after;

  // CE QUE LA COURONNE POSE EN DUR. Le pas garde le défaut du constructeur —
  // TRENTE-SIX, dix bandes qui ferment le tour — chez trois bâtisseurs sur
  // quatre. Voir `kEffectScreenDefaultStepDegrees`.
  float angle_degrees = kEffectScreenDefaultStepDegrees;
  float outer_radius = 0.0F;
  float outer_radius_step = 0.0F;
  float outer_radius_acceleration = 0.0F;
  float band_width_target = 0.0F;
};

[[nodiscard]] std::span<const EffectScreenBurstPattern> EffectScreenBurstPatternTable();

[[nodiscard]] std::span<const EffectScreenBurstPattern> EffectScreenBurstPatternsFor(
    std::uint32_t effect_id);

// LES NŒUDS qu'un patron décrit, tirages faits.
//
// Le générateur avance : deux éclats semés à la suite n'ont ni le même angle ni
// le même rayon, et repartir de la même graine rejoue la même gerbe.
[[nodiscard]] std::vector<EffectScreenNode> SowEffectScreenBurstNodes(
    const EffectScreenBurstPattern& pattern, std::uint32_t& generator);

}  // namespace ro::render
