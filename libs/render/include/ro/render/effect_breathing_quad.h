#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE CARRÉ QUI RESPIRE : quatre coins sur un cercle dont le rayon oscille.
//
// Le dessinateur du sous-type 39. Il sort par la voie A. Six identifiants
// l'emploient, tous bâtis par le même semis.
//
// ⚠ IL N'Y A AUCUN RAYON (ADR 0017) : c'est un
// carré posé à plat, quatre points à quatre-vingt-dix degrés sur un même cercle,
// tous à la même hauteur. Chercher des branches partant d'un moyeu n'en
// trouverait pas.
//
// ⚠ SON RAYON RESPIRE, et pas au rythme qu'on croirait. Il vaut quatre
// cinquièmes du rayon de l'ancre, PLUS un dixième multiplié par « le sinus de
// l'inclinaison, augmenté de un ». Il oscille donc entre quatre cinquièmes et le
// rayon entier — jamais au-delà —, et c'est l'INCLINAISON qui mène ce souffle,
// pas le compteur de tics.

// LES DEUX NOMBRES DU SOUFFLE : la part fixe du rayon, et celle qui respire.
inline constexpr float kBreathingQuadSteadyPart = 0.8F;
inline constexpr float kBreathingQuadSwingPart = 0.1F;

// L'ÉCART DES QUATRE COINS sur le cercle.
inline constexpr std::int32_t kBreathingQuadCornerDegrees = 90;

// DE COMBIEN LE RANG D'UNE PISTE ÉCARTE son angle de départ, pour les styles qui
// ne lisent pas l'azimut. Vingt-trois degrés, et ce nombre n'a pas d'autre
// origine que la mesure.
inline constexpr std::int32_t kBreathingQuadDegreesPerTrack = 23;

// LES CASES QUE CETTE FIGURE LIT, et leur sens lui appartient (ADR 0017).
inline constexpr std::size_t kSlotOfBreathingHeight = 0;
inline constexpr std::size_t kSlotOfBreathingVariant = 4;

// CE QU'UN STYLE DONNE AU CARRÉ.
struct EffectBreathingQuadStyle {
  std::array<std::uint8_t, 3> color = {255, 255, 255};
  // Dans la numérotation du seizième paramètre de la voie A : zéro veut dire
  // ADDITION, un le mélange ordinaire.
  std::int16_t blend_mode = 0;
  // Deux styles peignent le même quad DEUX fois, pour le renforcer.
  std::int32_t layers = 1;
  // ⚠ L'ANGLE DE DÉPART VIENT DE DEUX ENDROITS SELON LE STYLE : de l'azimut de
  // la piste pour cinq valeurs, du RANG de la piste pour toutes les autres.
  // Prendre toujours l'azimut superposerait les quatre pistes ; prendre toujours
  // le rang ignorerait l'azimut que le semis a tiré.
  bool starts_at_azimuth = false;
  // ⚠ DEUX STYLES PASSENT DEVANT TOUT, le bleu et le rouge sombre : voir
  // `EffectQuadDepth`.
  EffectQuadDepth depth = EffectQuadDepth::kProjected;
};

// Ce que le style d'un nœud demande. Tout style hors des douze connus rend le
// cas par défaut du client : un carré blanc qui s'ajoute.
//
// `variant` est la case que le semis remplit avec l'octet du site ; un seul
// style la consulte, et c'est pourquoi elle est ici.
[[nodiscard]] EffectBreathingQuadStyle EffectBreathingQuadStyleFor(std::int32_t style,
                                                                   float variant);

// LES QUADS D'UNE PISTE : un par couche, ou rien.
//
// `style` est le champ que le nœud porte — la figure le lit à un déplacement que
// sa boucle ne décale pas, si bien que les quatre pistes le partagent. `rank`
// est le rang de la piste dans le nœud.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeBreathingQuad(const EffectTrack& track,
                                                                std::int32_t style,
                                                                std::size_t rank,
                                                                glm::vec3 node_origin,
                                                                const EffectOwnerTint& owner = {});

}  // namespace ro::render
