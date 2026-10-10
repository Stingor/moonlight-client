#pragma once

#include <array>
#include <cstdint>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pattern_table.h"

// CE QU'UN NŒUD D'ANNEAU TIRE ET PARCOURT APRÈS SA NAISSANCE : sa teinte, l'axe
// le long duquel son pas le pousse, et le point où il meurt en arrivant.
//
// À part du semis parce que ces trois règles se lisent sans troupe ni monde :
// elles s'éprouvent seules, sur des nombres.

namespace ro::render {

struct PooledEffectNode;

// LE BIT QUI TUE UN NŒUD DES SOUS-TYPES QUINZE ET SEIZE à son arrivée : passé à
// trois unités de la cible de l'effet, mesurées au sol, il disparaît dans le tic.
//
// ⚠ UN DRAPEAU NE VEUT DIRE QUELQUE CHOSE QUE POUR SON MOUVEMENT (ADR 0017) :
// ailleurs, ce bit en lance un autre. Voir `EffectNodeArrivedAtTarget`.
inline constexpr std::uint32_t kEffectNodeDiesAtTarget = 0x400;

// L'ANGLE DE BASE D'UN EFFET ET L'ANGLE DU CORPS DE SON PORTEUR se répondent
// autour de cette valeur : l'un vaut elle moins l'autre. Voir
// `EffectOwnerFacingDegrees`.
inline constexpr float kEffectBodyAngleMirrorDegrees = 180.0F;

// LA DISTANCE AU SOL sous laquelle un nœud porteur de ce bit est arrivé.
inline constexpr float kEffectNodeArrivalDistance = 3.0F;

// UN TIRAGE ENTIER DU CLIENT, pris au générateur. Voir `EffectClientIntegerDraw` :
// un modulo nul ne tire rien et rend le biais.
[[nodiscard]] std::int32_t DrawEffectClientInteger(const EffectClientIntegerDraw& draw,
                                                   std::uint32_t& generator);

// TIRE LA TEINTE D'UN ANNEAU selon son patron, sur la couleur déjà posée. Voir
// `EffectRingPattern::color_high` : un patron qui ne tire rien ne touche pas au
// générateur.
void DrawEffectRingTint(const EffectRingPattern& ring, std::array<float, 3>& color,
                        std::uint32_t& generator);

// 🔴 L'AXE LE LONG DUQUEL LE PAS D'UN SOUS-TYPE QUINZE OU SEIZE POUSSE LE NŒUD,
// dans notre repère, bâti des trois angles du nœud.
//
// ⚠ CE N'EST PAS LA TROISIÈME COLONNE DE L'ORIENTATION, C'EST SON OPPOSÉE. La
// figure se conjugue par le changement de repère, le déplacement se réfléchit :
// la profondeur locale d'une figure et la direction de son pas sont opposées
// chez nous alors qu'elles coïncident chez le client. Pour un cap nul et une
// inclinaison de moins quatre-vingt-dix, l'axe descend.
[[nodiscard]] glm::vec3 EffectNodeDepthAxis(float yaw_degrees, float pitch_degrees,
                                            float roll_degrees);

// CE QUE LE PAS A FAIT PARCOURIR À UN NŒUD QUI AVANCE LE LONG DE SA PROFONDEUR,
// à ajouter à sa place. Nul pour tout autre nœud. Voir
// `EffectRingPattern::advances_along_depth_axis`.
[[nodiscard]] glm::vec3 EffectNodeDepthAdvance(const PooledEffectNode& pooled);

// L'ANGLE DU CORPS QUE LE BÂTISSEUR D'UN NŒUD QUI VOLE VERS LA CIBLE LUI DONNE,
// en degrés du client dans [0, 360) : sa face vers la cible, au sol, puis le
// symétrique de cette face — voir `kEffectBodyAngleMirrorDegrees`.
// `offset_to_target` est l'écart du nœud à la cible, dans notre repère ; nul,
// la face reste à zéro et le corps vaut le demi-tour.
[[nodiscard]] float EffectNodeBodyTowardTarget(glm::vec3 offset_to_target);

// VRAI QUAND UN NŒUD PORTEUR DU BIT D'ARRIVÉE est à trois unités ou moins de la
// cible de son effet, au sol. Faux sans cible, sans le bit, ou pour une figure
// dont le mouvement ne lit pas ce bit.
[[nodiscard]] bool EffectNodeArrivedAtTarget(const PooledEffectNode& pooled);

}  // namespace ro::render
