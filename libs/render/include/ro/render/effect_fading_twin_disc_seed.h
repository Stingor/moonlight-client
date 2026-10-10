#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LES DEUX DISQUES QUI S'ÉTEIGNENT : le semis et le mouvement du sous-type 161.
//
// Le bâtisseur, et le mouvement que la table des sous-types lui donne (cas 161
// du répartiteur). Un seul identifiant l'emploie : l'effet 831, qui appelle ce
// bâtisseur APRÈS celui du ruban typé.
//
// ⚠ SA FIGURE EST DÉJÀ PORTÉE : c'est celle des sous-types 26, 28, 39, 67, 88
// et 161, c'est-à-dire `ComposeBreathingQuad`.
//
// ⚠ ET SON BÂTISSEUR EST CELUI DU SOUS-TYPE 28 À TROIS ÉCRITURES PRÈS : il ne
// connaît pas la valeur 99999998 de l'âge, pose le mot de rendu à cinq au lieu
// de treize, et remet à zéro les compteurs des deux pistes. Ce qui le sépare
// vraiment est son MOUVEMENT : les deux disques respirent comme ceux du 28, mais
// comptent leurs tics et S'ÉTEIGNENT pendant les vingt-cinq derniers.

// COMBIEN DE PISTES VIVENT : les deux premières sont allumées, les deux
// dernières éteintes.
inline constexpr std::size_t kFadingTwinDiscLiveTracks = 2;

// L'INCLINAISON DE LA SECONDE PISTE ; la première part de zéro. Les deux souffles sont en
// opposition.
inline constexpr std::int16_t kFadingTwinDiscSecondTilt = 180;

// L'ALPHA DE NAISSANCE des deux pistes.
inline constexpr std::int16_t kFadingTwinDiscAlphaAtBirth = 200;

// LA HAUTEUR DES DEUX DISQUES, première case de la piste. Le repère du client descend : moins un
// est AU-DESSUS du nœud.
inline constexpr float kFadingTwinDiscHeight = -1.0F;

// LE MOT DE RENDU (0 | 1 | 5 = 5) : le seau lit
// le mélange de chaque primitive — l'ADDITION que demande le style — et passe
// l'épreuve de profondeur. Voir `effect_render_word.h`.
inline constexpr std::uint32_t kFadingTwinDiscRenderWord = 5;

// UN COUPLE DE DISQUES qu'un effet pose : ce que son site passe au bâtisseur.
struct EffectFadingTwinDiscPattern {
  std::uint32_t effect_id = 0;
  // LA FONCTION DU CLIENT qui pose ce couple.
  std::string_view client_function;
  std::string_view texture;
  std::uint8_t builder_argument = 0;
  // LE PLANCHER QU'UN BÂTISSEUR VOISIN POSE À LA VIE DE L'INSTANCE, avant que
  // celui-ci ne la copie dans son nœud. Zéro
  // quand aucun ne le fait. Voir la table engendrée.
  std::int32_t instance_life_floor_ticks = 0;
};

// Tous les couples connus, par identifiant d'effet.
[[nodiscard]] std::span<const EffectFadingTwinDiscPattern> EffectFadingTwinDiscPatternTable();

// TOUS les couples d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectFadingTwinDiscPattern> EffectFadingTwinDiscPatternsFor(
    std::uint32_t effect_id);

// CE QUE L'OCTET DU SITE POSE.
struct EffectFadingTwinDiscSeed {
  // Le champ de style du NŒUD, que la figure relit comme un flottant : quinze, dix-sept, dix-huit,
  // ou quatre.
  std::int32_t node_style = 0;
  // Le rayon d'ancre des DEUX pistes, le même pour l'une et l'autre.
  float radius_at_anchor = 0.0F;
};

[[nodiscard]] EffectFadingTwinDiscSeed EffectFadingTwinDiscSeedFor(std::uint8_t builder_argument);

// LES QUATRE PISTES d'un nœud — DEUX vives. Aucun tirage : tout est en dur.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectFadingTwinDiscTracks(
    const EffectFadingTwinDiscPattern& pattern);

// LA VIE DU NŒUD : celle de l'effet, relevée au plancher que pose le voisin.
[[nodiscard]] std::int32_t FadingTwinDiscLifeTicks(const EffectFadingTwinDiscPattern& pattern,
                                                   std::int32_t effect_life_ticks);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectFadingTwinDiscNode(
    const EffectFadingTwinDiscPattern& pattern, glm::vec3 anchor, std::int32_t effect_life_ticks);

// ---------------------------------------------------------------------------
// LE MOUVEMENT
// ---------------------------------------------------------------------------

// DE COMBIEN L'INCLINAISON TOURNE par tic, sur les deux pistes. C'est elle qui mène le souffle du
// rayon.
inline constexpr std::int16_t kFadingTwinDiscTiltPerTick = 3;

// COMBIEN DE TICS AVANT LA FIN L'EXTINCTION COMMENCE, et de combien l'alpha
// baisse par tic. Vingt-cinq fois
// huit font deux cents : l'alpha de naissance touche zéro au dernier tic.
inline constexpr std::int32_t kFadingTwinDiscFadeTicks = 25;
inline constexpr std::int16_t kFadingTwinDiscAlphaStepPerTick = 8;

// LE MOUVEMENT DU SOUS-TYPE 161, pour un nœud qui vit `life_ticks` tics.
//
// Pour les pistes zéro et un, dans cet ordre, et SANS tester qu'elles vivent :
//   - le compteur de tics gagne un ;
//   - l'inclinaison gagne trois, et perd un tour à trois cent soixante ;
//   - passé `life_ticks − 25`, l'alpha perd huit, plancher à zéro.
//
// ⚠ LE SEUIL SE LIT DANS LE NŒUD À CHAQUE TIC, pas au semis : c'est la vie
// que la troupe a donnée au nœud — plancher et échéance compris — qui compte.
void AdvanceFadingTwinDiscTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                 std::int32_t life_ticks);

}  // namespace ro::render
