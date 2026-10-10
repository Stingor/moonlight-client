#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_burst.h"

namespace ro::render {

// LE SEMIS DES ANNEAUX QUI PULSENT, sous-type 63.
//
// Sa figure est la JUPE D'ANNEAU, portée depuis le début du lot 8 : ce sous-type
// n'a coûté que son semis et son mouvement. Voir `ComposeEffectTrackSkirt` et
// `AdvanceRadialBurstTracks`.
//
// ⚠ SES « NEUF BÂTISSEURS » NE SONT QUE CINQ FONCTIONS. Le relevé donne
// plusieurs noms à une même adresse, parce que plusieurs cas du répartiteur y
// mènent. Quatre des cinq posent exactement les mêmes champs et ne diffèrent que
// par quatre nombres ; la cinquième est l'exception.
//
// ⚠ ET LA QUATRIÈME PISTE NE SE DESSINE PAS : elle sert de REGISTRE. Son style
// porte le MODE de tout le nœud, son compteur l'âge global. La croire morte
// ferait perdre le mode, et avec lui tout le mouvement.

// LE BALAYAGE DES TROIS ANNEAUX : le tour entier, donc des cercles fermés.
inline constexpr std::int16_t kPulsingRingSweepDegrees =
    static_cast<std::int16_t>(kEffectTrackFullTurnDegrees);

// ⚠ LA CLÉ EXACTE QUE LA JUPE COMPARE. La jupe teste sa clé contre des flottants
// EXACTS pour décider de s'élever, et celui-ci est l'un d'eux. L'écrire six
// poserait les trois anneaux au sol.
//
// ⚠ ET LE MOUVEMENT LA RELIT COMME UNE LONGUEUR : il la recopie dans les vingt
// et une cases de la piste, où la jupe la lit comme la largeur de son ruban. Une
// même case, deux sens, et c'est l'ADR 0017 sur un seul flottant.
inline constexpr float kPulsingRingLiftKey = 6.001F;

// LES TROIS ANNEAUX SONT DÉCALÉS, en azimut comme en inclinaison, et les six
// nombres sont les mêmes chez les cinq bâtisseurs.
inline constexpr std::array<std::int16_t, kEffectRadialBurstRingCount> kPulsingRingAzimuthDegrees =
    {0, 25, 50};
inline constexpr std::array<std::int16_t, kEffectRadialBurstRingCount> kPulsingRingTiltDegrees = {
    2, 3, 4};

// UN NŒUD D'ANNEAUX QU'UN EFFET POSE.
struct EffectPulsingRingsPattern {
  std::uint32_t effect_id = 0;
  // LA FONCTION DU CLIENT qui le pose, par son nom.
  std::string_view client_function;
  std::string_view texture;
  // LA VARIANTE DE TEINTE du nœud. C'est elle que la palette
  // de traînée indexe — voir `effect_trail_palette.h`.
  //
  // ⚠ LE RELEVÉ L'APPELLE « echelle_sprite » ET CE NOM MENT (ADR 0017) : le
  // dessinateur de ce sous-type n'y lit pas une taille mais un numéro de teinte.
  std::uint8_t trail_variant = 0;
  // LE MODE, que le bâtisseur range dans le registre et dont le mouvement tire
  // TOUT : le sens du rayon, le rythme de l'alpha, son plafond et la durée au
  // bout de laquelle une piste cesse de renaître.
  std::int16_t mode = 1;
  // Le style des trois pistes vives. Cent chez quatre bâtisseurs, trois mille
  // cent onze chez l'exception.
  std::int16_t track_style = 0;
  // ⚠ DE COMBIEN DE TICS CHAQUE ANNEAU ATTEND DE PLUS QUE LE PRÉCÉDENT. Les
  // compteurs valent zéro, moins un pas, moins deux pas : c'est cela la
  // pulsation. Les semer ensemble n'en montrerait qu'un, trois fois plus épais.
  std::int16_t delay_step_ticks = 10;
  // LE TIC DE L'EFFET auquel le bâtisseur pose son nœud, et la PÉRIODE à
  // laquelle il le repose. Zéro en période veut dire « une seule fois ».
  //
  // ⚠ QUATRE DES CINQ BÂTISSEURS SONT DES `BuildUpdate`, rappelés à chaque tic,
  // et c'est une division qui leur sert de garde. La retirer sèmerait cent nœuds
  // par seconde.
  std::int32_t first_tick = 0;
  std::int32_t rebuild_period_ticks = 0;
};

// Tous les nœuds d'anneaux connus, par identifiant d'effet.
[[nodiscard]] std::span<const EffectPulsingRingsPattern> EffectPulsingRingsPatternTable();

// TOUS les nœuds d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectPulsingRingsPattern> EffectPulsingRingsPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES : trois anneaux et un registre.
//
// Les trois naissent sans rayon et sans alpha — c'est le mouvement qui les
// ouvre — et leurs vingt et une marques valent UN.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectPulsingRingsTracks(
    const EffectPulsingRingsPattern& pattern);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectPulsingRingsNode(const EffectPulsingRingsPattern& pattern,
                                                         glm::vec3 anchor,
                                                         std::int32_t effect_life_ticks);

}  // namespace ro::render
