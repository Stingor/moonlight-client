#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace ro::render {

// CE QUI BAT À CHAQUE IMAGE, ET NON AU TIC.
//
// Une image qui doit N tics met tout à jour N fois. Une image qui n'en doit
// AUCUN ne met à jour que deux sortes de choses, une fois chacune :
//
//   - les INSTANCES de certains effets : leur bâtisseur est rappelé, il sème ce
//     que son âge demande, puis leur âge avance d'un. Elles vivent donc moins
//     longtemps en temps réel au-delà de soixante images par seconde, et leurs
//     nœuds meurent avec elles ;
//   - les NŒUDS que leur bâtisseur marque pour cela — ceux d'Increase Agility.

// LES EFFETS DONT L'INSTANCE BAT À CHAQUE IMAGE. Triée, et la recherche en dépend.
inline constexpr std::array<std::uint32_t, 83> kEffectsWhoseInstanceBeatsEveryFrame = {
    25,  37,  49,  51,  52,  53,  147, 152, 200, 201, 202, 203, 228, 254, 312, 313, 320,
    325, 328, 362, 397, 398, 418, 436, 437, 438, 441, 454, 467, 468, 469, 470, 471, 472,
    473, 488, 489, 490, 503, 504, 506, 523, 524, 525, 526, 527, 528, 529, 530, 531, 532,
    533, 534, 535, 536, 590, 603, 629, 678, 679, 680, 755, 757, 788, 789, 790, 826, 833,
    837, 850, 861, 865, 867, 871, 873, 875, 877, 881, 911, 920, 932, 940, 1122};
// Un compte trop grand laisserait des zéros en queue, et la table ne serait plus triée.
static_assert(std::ranges::is_sorted(kEffectsWhoseInstanceBeatsEveryFrame));

[[nodiscard]] constexpr bool EffectInstanceBeatsEveryFrame(std::uint32_t effect_id) noexcept {
  return std::ranges::binary_search(kEffectsWhoseInstanceBeatsEveryFrame, effect_id);
}

// LES NŒUDS QUI BATTENT À CHAQUE IMAGE : ceux d'Increase Agility, que son
// bâtisseur marque pour cela.
inline constexpr std::uint32_t kIncreaseAgilityEffect = 37;
[[nodiscard]] constexpr bool EffectBeatsEveryFrame(std::uint32_t effect_id) noexcept {
  return effect_id == kIncreaseAgilityEffect;
}

}  // namespace ro::render
