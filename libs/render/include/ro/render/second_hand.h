#pragma once

#include <cstdint>

#include "ro/render/floating_number.h"

namespace ro::render {

// ── LA SECONDE MAIN D'UN COUP ORDINAIRE ─────────────────────────────────────
//
// Le serveur annonce à part les dégâts de la main gauche d'un coup ordinaire.
// Le client en fait un MESSAGE DE DÉGÂTS de plus — un nombre, un bruit, une
// ligne de combat —, mais seulement quand le métier de l'attaquant est l'un de
// ceux qui peuvent tenir deux armes à ses yeux : l'assassin (12), tout métier
// au-delà de 4000 — les transcendants, les troisièmes, et les monstres dont le
// numéro dépasse 20000 —, et deux plages de corps de PNJ.
//
// ⚠ ET LA SECONDE MAIN CHANGE LE PAS DE LA PREMIÈRE : les frappes suivantes
// d'un coup multiple partent toutes ensemble, un demi-pas après la première, et
// la seconde main un pas et trois quarts après elle.

inline constexpr std::uint32_t kAssassinJob = 12;
inline constexpr std::uint32_t kLastPlainJob = 4000;
inline constexpr std::uint32_t kFirstNpcBodyJob = 45;
inline constexpr std::uint32_t kFirstMonsterJob = 1000;
inline constexpr std::uint32_t kFirstExtraNpcBodyJob = 10001;
inline constexpr std::uint32_t kLastExtraNpcBodyJob = 20000;

// Vrai si un attaquant de ce métier fait paraître la seconde main.
[[nodiscard]] constexpr bool JobShowsSecondHand(std::uint32_t attacker_job) noexcept {
  return attacker_job == kAssassinJob || attacker_job > kLastPlainJob ||
         (attacker_job >= kFirstNpcBodyJob && attacker_job < kFirstMonsterJob) ||
         (attacker_job >= kFirstExtraNpcBodyJob && attacker_job <= kLastExtraNpcBodyJob);
}

// Les sortes que le traitement du coup ordinaire rejoue : e_damage_type.
[[nodiscard]] constexpr bool PlainBlowReplays(std::uint8_t damage_type) noexcept {
  constexpr std::uint8_t kPlain = 0;
  constexpr std::uint8_t kEndured = 4;
  constexpr std::uint8_t kLuckyDodge = 11;
  return damage_type == kPlain || damage_type == kEndured || damage_type == kMultipleBlows ||
         damage_type == kMultipleBlowsEndured || damage_type == kCriticalBlow ||
         damage_type == kLuckyDodge || damage_type == kCriticalMultipleBlows;
}

// Vrai si ce coup fait paraître la seconde main : un coup ORDINAIRE — une
// compétence n'en a pas —, d'une sorte que ce traitement rejoue, avec des
// dégâts de seconde main, porté par un métier qui les montre.
[[nodiscard]] constexpr bool ShowsSecondHand(std::uint16_t skill_id, std::uint8_t damage_type,
                                             std::int32_t second_damage,
                                             std::uint32_t attacker_job) noexcept {
  return skill_id == 0 && second_damage != 0 && PlainBlowReplays(damage_type) &&
         JobShowsSecondHand(attacker_job);
}

// Le pas du client entre deux frappes, en millisecondes entières.
inline constexpr auto kHitStepMs = static_cast<std::uint32_t>(kMultiHitIntervalMs);

// L'instant de la frappe de rang `index` — zéro pour la première —, depuis la
// première, quand la seconde main paraît : toutes les suivantes ensemble, un
// demi-pas plus tard. `step_ms` est le pas.
[[nodiscard]] constexpr std::uint32_t TwoHandedHitOffsetMs(std::uint16_t index,
                                                           std::uint32_t step_ms) noexcept {
  return index == 0 ? 0U : step_ms / 2U;
}

// L'instant de la seconde main, depuis la première frappe : un pas, un quart de
// pas et un demi-pas, en divisions entières.
[[nodiscard]] constexpr std::uint32_t SecondHandOffsetMs(std::uint32_t step_ms) noexcept {
  return step_ms + step_ms / 4U + step_ms / 2U;
}

}  // namespace ro::render
