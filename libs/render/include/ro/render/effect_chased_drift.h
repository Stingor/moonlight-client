#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_track.h"

namespace ro::render {

// LA DÉRIVE POURSUIVIE D'UN SPRITE : un pétale qui naît sur un cercle autour de
// l'effet, MONTE d'un pas fixe à chaque tic et dérive de côté au gré de deux
// angles qui poursuivent chacun une cible.
//
// ⚠ CE N'EST PAS UN LANCER. Le pétale ne part dans aucune direction tirée une
// fois : sa dérive change de sens à chaque cible atteinte, et c'est ce qui le
// fait flotter au lieu de filer. Le lancer d'un sprite ne sait pas l'exprimer.
//
// ⚠ ET SA PLACE NE SUIT PAS LE PORTEUR : elle est absolue depuis la naissance.
// Chaque pétale naît autour de l'effet au tic de son semis, puis s'en va seul.
//
// 🔴 LA MÉTÉO EN EST LA VARIANTE QUI TOMBE : les pétales de @sakura et les
// feuilles de @leaves naissent dans un CARRÉ au-dessus du porteur, descendent,
// et renaissent en haut du carré dès qu'ils passent sous ses pieds. Semés aux
// pieds et sans renaissance, ils montraient une nuée collée au personnage au
// lieu d'une pluie qui couvre l'écran.

// CE QU'UN EFFET DEMANDE À SES SPRITES, tiré dans cet ordre au semis — celui du
// client, et avant tout autre tirage du nœud :
//
//   · la montée, `(tirage(montee_etendue) + montee_plancher) × montee_unite` —
//     TROIS valeurs, et non un intervalle continu ;
//   · l'échelle du sprite, `(tirage(etendue) + plancher) / diviseur` — six
//     valeurs, entières avant la division ;
//   · l'angle du cercle de naissance, sur trois cent soixante degrés — ou,
//     pour la variante qui tombe, l'abscisse dans le carré, avant ou après les
//     angles selon le bâtisseur, et la profondeur après eux ;
//   · les deux paires d'angles, courant puis cible, sur trois cent soixante ;
//   · l'action du `.act`, `tirage(etendue)`.
struct EffectChasedDriftPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  // Le rayon du CERCLE de naissance, en unités du monde. Un cercle et non un
  // disque : tous les pétales naissent à la même distance de l'effet.
  float birth_ring_radius = 0.0F;
  std::int32_t rise_spread = 0;
  std::int32_t rise_floor = 0;
  // ⚠ NÉGATIVE POUR CE QUI TOMBE : la montée est comptée vers le haut.
  float rise_unit = 0.0F;
  // De combien le pétale dérive par tic, multiplié par le sinus de chaque
  // angle courant — le premier en abscisse, le second en profondeur.
  float drift_step = 0.0F;
  float drift_step_depth = 0.0F;
  std::int32_t scale_spread = 0;
  std::int32_t scale_floor = 0;
  float scale_divisor = 1.0F;
  std::int32_t action_spread = 0;
  // SOUS CET ALPHA, LE PÉTALE S'ÉTEINT d'un coup : le mouvement ôte son pas,
  // puis met à zéro ce qui reste en deçà. Voir `EffectNode::alpha_zero_below`.
  float alpha_zero_below = 0.0F;
  // LE CARRÉ DE NAISSANCE, son côté en unités du monde, centré sur le porteur
  // et élevé de `birth_lift` — zéro pour le cercle. Un carré veut dire aussi
  // la RENAISSANCE : passé sous les pieds du porteur, le pétale y renaît.
  float birth_square_side = 0.0F;
  float birth_lift = 0.0F;
  // L'ABSCISSE DU CARRÉ SE TIRE AVANT LES ANGLES, et non après : l'ordre du
  // bâtisseur des feuilles, pas celui des pétales.
  bool square_x_before_angles = false;
};

// CE QUE LE SEMIS D'UN PÉTALE TIRE pour le nœud lui-même, en plus de sa dérive :
// l'écart de naissance que l'appelant ajoute à la place du nœud, l'échelle et
// l'action qui REMPLACENT celles du patron.
struct EffectChasedDriftSowing {
  glm::vec3 birth_offset{0.0F, 0.0F, 0.0F};
  float sprite_scale = 1.0F;
  std::int32_t action = 0;
};

// COMBIEN DE PAIRES D'ANGLES le pétale poursuit : une par direction du sol.
inline constexpr std::size_t kChasedDriftAnglePairs = 2;

// L'ÉTAT D'UN PÉTALE. Éteint pour tout autre nœud, qui n'en paie rien.
//
// Les angles vivent dans les cases d'une piste, courant puis cible, parce que la
// poursuite qui les fait avancer est celle des pistes : la même règle, le même
// code.
struct EffectChasedDrift {
  bool active = false;
  float rise_per_tick = 0.0F;
  float drift_step = 0.0F;
  float drift_step_depth = 0.0F;
  // Le carré de renaissance, nul pour un pétale qui ne renaît pas.
  float birth_square_side = 0.0F;
  float birth_lift = 0.0F;
  EffectTrack angles;
};

// VRAI POUR UN PÉTALE QUI RENAÎT AU-DESSUS DE SON PORTEUR. ⚠ IL NE SE
// TRANSLATE PAS AVEC LUI : sa place est absolue, et seule sa renaissance relit
// celle du porteur. L'emmener à chaque pas collerait la pluie au personnage.
[[nodiscard]] inline bool EffectChasedDriftIsReborn(const EffectChasedDrift& drift) {
  return drift.active && drift.birth_square_side > 0.0F;
}

[[nodiscard]] std::span<const EffectChasedDriftPattern> EffectChasedDriftPatternTable();

// Le patron de cet effet, ou nul s'il n'en a pas.
[[nodiscard]] const EffectChasedDriftPattern* EffectChasedDriftPatternFor(std::uint32_t effect_id);

// SÈME UN PÉTALE : tire, dans l'ordre du patron, sa montée, son échelle, sa
// place sur le cercle, ses quatre angles et son action.
[[nodiscard]] EffectChasedDriftSowing SowEffectChasedDrift(const EffectChasedDriftPattern& pattern,
                                                           EffectChasedDrift& drift,
                                                           std::uint32_t& generator);

// UN TIC : les angles poursuivent leur cible, puis le pétale monte et dérive.
// Rend le DÉPLACEMENT de ce tic ; nul, sans tirage, pour un état éteint.
//
// ⚠ DANS NOTRE REPÈRE : la montée est POSITIVE en ordonnée, et la dérive en
// profondeur change de signe — le client range l'une et l'autre à l'opposé.
[[nodiscard]] glm::vec3 AdvanceEffectChasedDrift(EffectChasedDrift& drift,
                                                 std::uint32_t& generator);

// LA RENAISSANCE, après le déplacement du tic : passé sous les pieds du
// porteur, le pétale repart d'un point neuf du carré au-dessus de lui. Rend
// vrai quand il vient de renaître. Sans effet pour un pétale sans carré.
bool RebirthEffectChasedDrift(const EffectChasedDrift& drift, glm::vec3& position, glm::vec3 owner,
                              std::uint32_t& generator);

}  // namespace ro::render
