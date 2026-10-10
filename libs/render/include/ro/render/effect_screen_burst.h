#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_screen_node.h"
#include "ro/render/effect_screen_quad.h"

namespace ro::render {

// LES DEUX FIGURES D'ÉCLAT DE LA VOIE D'ÉCRAN : la couronne et l'éclat.
//
// La couronne est le sous-type 4 ; l'éclat est le cas 3 du dessinateur, qui
// peint lui-même sans sous-fonction. HUIT identifiants les emploient — 16, 40,
// 82, 98, 99, 121, 128 et 215 —, et trois d'entre eux posent les DEUX à la
// fois.
//
// ⚠ ELLES SORTENT EN PIXELS (ADR 0022) : le client remplit lui-même des sommets
// déjà transformés et les soumet sans rien projeter. Voir `effect_screen_quad.h`
// pour ce que cela change.
//
// ⚠ ET LES DEUX LISENT LE MÊME CHAMP POUR DEUX CHOSES. `angle_degrees` est le
// PAS du balayage de la couronne et la DEMI-OUVERTURE du triangle de l'éclat.
// C'est l'ADR 0017 dans sa forme la plus nue : rien dans le champ ne le dit.

// ---------------------------------------------------------------------------
// LA COURONNE — sous-type 4
// ---------------------------------------------------------------------------

// DE COMBIEN L'ABSCISSE DE TEXTURE AVANCE par bande, et où elle reboucle.
//
// ⚠ L'IMAGE SE RÉPÈTE DONC TOUS LES QUATRE SEGMENTS, quel que soit le pas
// angulaire. Ce n'est pas une fraction du tour : c'est un quart par bande.
inline constexpr float kScreenRingTextureStep = 0.25F;

// LE TOUR COMPLET, dont le balayage s'arrête un pas avant.
inline constexpr float kScreenRingFullTurnDegrees = 360.0F;

// ⚠ EN DEÇÀ DE CE PAS, AUCUNE BANDE. Le client boucle tant que l'angle cumulé
// n'a pas dépassé le tour moins le pas ; un pas nul ou négatif ne l'avancerait
// jamais et le ferait tourner sans fin. Le défaut du constructeur vaut
// trente-cinq, si bien que le cas ne se présente pas — mais un patron qui
// poserait zéro figerait le client, pas nous.
inline constexpr float kScreenRingSmallestStep = 0.0001F;

// LES BANDES D'UNE COURONNE, en pixels.
//
// `owner_screen_position` est la position d'écran du porteur, déjà projetée ;
// elle est ignorée quand le nœud se pose dans l'absolu.
//
// ⚠ LA COURONNE NE SE REFERME PAS. Au pas de trente-cinq degrés, dix bandes
// couvrent trois cent cinquante degrés et il reste un trou de dix. Le client ne
// rattrape pas le reste, et le rattraper ferait une bande plus étroite que les
// autres — visible, puisqu'elle porterait l'image entière.
[[nodiscard]] std::vector<EffectScreenQuad> ComposeScreenRingQuads(
    const EffectScreenNode& node, glm::vec2 owner_screen_position = {0.0F, 0.0F});

// ---------------------------------------------------------------------------
// L'ÉCLAT — sous-type 3
// ---------------------------------------------------------------------------

// LES TROIS NOMBRES DE TEXTURE DU TRIANGLE : la pointe au milieu du haut, les
// deux coins de base aux deux bouts du bas.
inline constexpr float kScreenWedgeTipU = 0.5F;
inline constexpr float kScreenWedgeTipV = 0.0F;
inline constexpr float kScreenWedgeBaseV = 1.0F;

// L'ÉCLAT D'UNE PISTE, en pixels.
//
// ⚠ C'EST UN TRIANGLE, rendu comme un quad dont le dernier coin REPREND la
// pointe. Le client soumet trois sommets ; notre quad en porte quatre, et le
// quatrième referme sur la pointe — le second triangle est alors d'aire nulle.
// C'est ce que fait déjà le coin de cône de la voie A.
//
// ⚠ ET SON ANGLE ZÉRO POINTE VERS LE HAUT. Le client prend le SINUS en abscisse
// et le COSINUS en ordonnée, qu'il RETRANCHE — l'inverse de la composition
// habituelle. Recopier celle-ci ferait pointer l'éclat vers la droite et
// tourner dans l'autre sens.
[[nodiscard]] EffectScreenQuad ComposeScreenWedgeQuad(const EffectScreenNode& node,
                                                      glm::vec2 owner_screen_position = {0.0F,
                                                                                         0.0F});

// OÙ LE CENTRE D'UNE DE CES DEUX FIGURES SE TROUVE, en pixels.
//
// Le décalage du nœud, plus la position d'écran de son porteur quand il est
// relatif. Les deux figures s'en servent, et le mouvement le recalcule à chaque
// tic : ces nœuds SUIVENT celui qui les a lancés.
[[nodiscard]] glm::vec2 EffectScreenBurstCenter(const EffectScreenNode& node,
                                                glm::vec2 owner_screen_position);

}  // namespace ro::render
