#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DES ANNEAUX EN CŒUR, sous-type 110.
//
// UN bâtisseur, DEUX anneaux par nœud, et un nœud par point d'un dessin. `EF_HEARTCASTING` (343)
// appelle le sien VINGT fois, et les vingt points forment un cœur.
//
// ⚠ LA FIGURE ET LE MOUVEMENT ÉTAIENT DÉJÀ PORTÉS, chacun de son côté : le
// ruban de traînée vient du sous-type 36, et le mouvement des groupes radiaux du
// soixante — que le cent dix partage avec lui. Ce qui manquait était la FORME qui les réunit.
//
// ⚠ ET L'ORDONNÉE DE L'ANCRE N'EST PAS UNE HAUTEUR. Le bâtisseur y pose
// 1234,5 — la SENTINELLE du ruban : à cette valeur exacte, la figure cesse de
// lire l'ancre comme un point et la TOURNE du cap de la caméra. C'est ainsi que
// le cœur reste face au joueur sans qu'aucun champ ne le dise. La lire comme une
// hauteur poserait le dessin mille deux cents unités au-dessus du sol.

// LES DEUX ANNEAUX d'un nœud : le petit vif, et le grand plus lent.
struct EffectHeartRingSeed {
  float lift_key = 0.0F;
  float radius_at_anchor = 0.0F;
  std::int16_t tilt_degrees = 0;
};

[[nodiscard]] std::span<const EffectHeartRingSeed> EffectHeartRingSeeds();

// CE QUE LE MOUVEMENT RADIAL ATTEND DANS LA PREMIÈRE MARQUE pour tourner le
// nœud vers la caméra. Le bâtisseur la pose sur les vingt et une.
inline constexpr std::uint8_t kHeartRingCameraMark = 33;
// LE BALAYAGE : le tour entier, pour les deux anneaux.
inline constexpr std::int16_t kHeartRingSweepDegrees = 360;
// DE COMBIEN LA VIE DE L'INSTANCE EST RACCOURCIE pour donner au style sa durée.
//
// ⚠ LE STYLE EST UNE DURÉE, et le mouvement l'emploie comme telle : passé ce
// compte, l'anneau s'efface. Le bâtisseur le tire de la vie de l'effet moins
// vingt-cinq, ce qui fait retomber les anneaux juste avant sa fin.
inline constexpr std::int16_t kHeartRingStyleBeforeEnd = 25;
// LE NUMÉRO DE VARIANTE du ruban, que le bâtisseur pose en dur : dix, un rose
// ajouté. La primitive de ruban y prend sa teinte et son mélange ; sans lui le
// cœur serait blanc.
inline constexpr std::uint8_t kHeartRingTrailVariant = 10;

// UN POINT DU DESSIN : un nœud, à son décalage.
struct EffectHeartRingPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // ⚠ CE N'EST PAS UNE POSITION DU MONDE mais un décalage que la figure TOURNE
  // du cap de la caméra. Voir la sentinelle.
  float offset_x = 0.0F;
  float offset_z = 0.0F;
};

[[nodiscard]] std::span<const EffectHeartRingPattern> EffectHeartRingPatternTable();

[[nodiscard]] std::span<const EffectHeartRingPattern> EffectHeartRingPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud : deux vives, deux éteintes.
//
// `azimuth_seed` fait tourner les deux azimuts, que le client tire au sort : deux
// cœurs joués à la suite ne présentent pas leurs anneaux sous le même angle.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectHeartRingTracks(
    const EffectHeartRingPattern& pattern, std::int32_t effect_life_ticks,
    std::uint32_t& azimuth_seed);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectHeartRingNode(const EffectHeartRingPattern& pattern,
                                                      glm::vec3 anchor,
                                                      std::int32_t effect_life_ticks,
                                                      std::uint32_t& azimuth_seed);

}  // namespace ro::render
