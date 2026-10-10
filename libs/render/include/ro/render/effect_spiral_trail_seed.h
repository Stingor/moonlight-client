#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DES FEUX D'ARTIFICE, et les effets du sous-type 52.
//
// Un seul bâtisseur : TROIS images écrites en dur — le grain, puis les deux étincelles —, et tout
// le reste tiré au sort. Les quatre pistes sont vives.
//
// ⚠ LEURS COMPTEURS PARTENT TRÈS NÉGATIFS, de moins trois cents à moins trois
// cent cinquante, et c'est tout l'échelonnement de la gerbe : une piste ne bouge
// pas tant que son compteur n'est pas remonté à un, si bien que les quatre
// grains partent à trois cents tics d'écart au plus. Les semer à zéro ferait
// quatre fusées simultanées.
//
// ⚠ ET L'ANCRE EST DÉCALÉE DE QUINZE UNITÉS SUR L'ABSCISSE, au semis comme à la
// renaissance. Le client le fait deux fois, aux deux endroits, et la même valeur
// doit donc vivre aux deux — voir `AdvanceSwirlOrbitTracks`.
inline constexpr float kSpiralTrailAnchorShiftX = -15.0F;

// LE RETARD D'UNE PISTE : de trois cents à trois cent quarante-neuf tics.
inline constexpr std::int32_t kSpiralTrailDelayFloorTicks = -300;
inline constexpr std::uint32_t kSpiralTrailDelaySpread = 50;

// LA DEMI-TAILLE DU GRAIN, écrite en dur : deux unités.
inline constexpr float kSpiralTrailHalfSize = 2.0F;

// LE RAYON DU PAS DE VOL : de quatre dixièmes à huit dixièmes environ.
inline constexpr float kSpiralTrailStepFloor = 0.4F;
inline constexpr float kSpiralTrailStepUnit = 0.004F;
inline constexpr std::uint32_t kSpiralTrailStepSpread = 100;

// COMBIEN DE TEINTES le tirage couvre, et le pas du retard d'étincelle.
inline constexpr std::uint32_t kSpiralTrailTintSpread = 5;
inline constexpr std::uint32_t kSpiralTrailSparkDelaySpread = 4;
inline constexpr std::uint8_t kSpiralTrailSparkDelayStep = 20;

// UNE GERBE QU'UN EFFET POSE : ce que son bâtisseur écrit en dur.
//
// ⚠ SON SITE D'APPEL NE PASSE RIEN, à la différence de presque toute la famille :
// les trois images vivent dans le bâtisseur. Le patron les porte quand même, pour
// que la table dise ce que le nœud montre.
struct EffectSpiralTrailPattern {
  std::uint32_t effect_id = 0;
  // L'étiquette du site d'appel qui pose cette gerbe.
  std::string_view client_function;
  // LES TROIS IMAGES, dans l'ordre du client : le grain, puis les deux que la
  // marque d'étincelle désigne.
  std::span<const std::string_view> textures;

  // ⚠ LA VIE DU NŒUD N'EST PAS ICI, ET C'EST VOULU. Ce bâtisseur ne la POSE
  // pas, il COPIE celle de l'effet, et cette durée-là se demande à
  // `EffectNodeLifeFor`, qui répond pour toutes les figures d'un coup. La
  // porter aussi dans ce patron ferait deux tables pour une seule question.
  //
  // Pour cette figure la réponse est l'INFINI, et il lui en faut : son grain
  // demande quelque 455 tics avant de peindre — un retard de −300 à −350, puis
  // `kLastTickOfSpiralTrailClimb` tics de montée. Une vie bornée le tuerait
  // avant, et c'est ce qui l'a fait passer deux fois pour une figure cassée.
};

// Toutes les gerbes connues, par identifiant d'effet.
[[nodiscard]] std::span<const EffectSpiralTrailPattern> EffectSpiralTrailPatternTable();

// TOUTES les gerbes d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectSpiralTrailPattern> EffectSpiralTrailPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES D'UNE GERBE, toutes vives et toutes retardées séparément.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSpiralTrailTracks(
    glm::vec3 anchor, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectSpiralTrailNode(const EffectSpiralTrailPattern& pattern,
                                                        glm::vec3 anchor,
                                                        std::int32_t effect_life_ticks,
                                                        std::uint32_t& generator);

}  // namespace ro::render
